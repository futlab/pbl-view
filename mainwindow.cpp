#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QSerialPortInfo>
#include <cmath>

using std::isnan;
using std::nan;

Series::Series(QChart *chart, QAbstractAxis *axisX, const QString &name, const QString &units, int maxCount) :
    maxCount(maxCount)
{
    series.setName(name);
    chart->addSeries(&series);
    chart->addAxis(&axis, Qt::AlignLeft);
    axis.setLinePenColor(series.pen().color());
    axis.setTitleText(units);
    series.attachAxis(axisX);
    series.attachAxis(&axis);
}

void Series::clear()
{
    series.clear();
    minY = nan("");
    maxY = nan("");
}

void Series::append(qreal x, qreal y)
{
    if (isnan(minY) || y < minY)
        minY = y;
    if (isnan(maxY) || y > maxY)
        maxY = y;
    axis.setRange(minY - 0.1, maxY + 0.1);
    if (series.count() > maxCount)
        series.removePoints(0, 1);
    series.append(x, y);
    series.show();
}

StaticSeries::StaticSeries(QChart *chart, QAbstractAxis *axisX, const QString &name, const QString &units) :
    Series(chart, axisX, name, units), lastX(0), count(0), sum(0)
{}

void StaticSeries::clear()
{
    Series::clear();
    sum = 0;
    count = 0;
    lastX = 0;
}


void StaticSeries::sample(uint x, qreal y)
{
    if (x != lastX) {
        lastX = x;
        count = 1;
        sum = y;
        series.append(x, y);
    } else {
        series.removePoints(series.count() - 1, 1);
        sum += y;
        count++;
        y = sum / count;
        series.append(x, y);
    }
    if (isnan(minY) || y < minY)
        minY = y;
    if (isnan(maxY) || y > maxY)
        maxY = y;
    axis.setRange(minY - 0.1, maxY + 0.1);
    series.show();
}


void MainWindow::createChart()
{
    chart = new QChart();
    chart->setTitle("Time view");
    chart->createDefaultAxes();
    axisX = new QValueAxis();
    chart->addAxis(axisX, Qt::AlignBottom);

    series.emplace(std::piecewise_construct, std::forward_as_tuple("f"), std::forward_as_tuple(chart, axisX, "Force", "g"));
    series.emplace(std::piecewise_construct, std::forward_as_tuple("v"), std::forward_as_tuple(chart, axisX, "Voltage", "V"));
    series.emplace(std::piecewise_construct, std::forward_as_tuple("i"), std::forward_as_tuple(chart, axisX, "Current", "A"));
    series.emplace(std::piecewise_construct, std::forward_as_tuple("out"), std::forward_as_tuple(chart, axisX, "Command", "μs"));
    series.emplace(std::piecewise_construct, std::forward_as_tuple("r"), std::forward_as_tuple(chart, axisX, "Rate", "Hz"));

    axisX->setRange(0, 2000);

    ui->graphicsView->setChart(chart);
    ui->graphicsView->setRenderHint(QPainter::Antialiasing);
}

void MainWindow::createStaticChart()
{
    staticChart = new QChart();
    staticChart->setTitle("Static view");
    staticChart->createDefaultAxes();
    axisOut = new QValueAxis();
    staticChart->addAxis(axisOut, Qt::AlignBottom);

    staticSeries.emplace(std::piecewise_construct, std::forward_as_tuple("f"), std::forward_as_tuple(staticChart, axisOut, "Force", "g"));
    staticSeries.emplace(std::piecewise_construct, std::forward_as_tuple("v"), std::forward_as_tuple(staticChart, axisOut, "Voltage", "V"));
    staticSeries.emplace(std::piecewise_construct, std::forward_as_tuple("i"), std::forward_as_tuple(staticChart, axisOut, "Current", "A"));
    //staticSeries.emplace(std::piecewise_construct, std::forward_as_tuple("out"), std::forward_as_tuple(chart, axisX, "Command", "μs"));
    staticSeries.emplace(std::piecewise_construct, std::forward_as_tuple("r"), std::forward_as_tuple(staticChart, axisOut, "Rate", "Hz"));

    axisOut->setRange(1200, 2000);
    axisOut->setTitleText("μs");

    ui->staticView->setChart(staticChart);
    ui->staticView->setRenderHint(QPainter::Antialiasing);
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    minX(nan("")), maxX(nan("")),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    stopTimer.setSingleShot(true);
    connect(&stopTimer, SIGNAL(timeout()), this, SLOT(stop()));

    createChart();
    createStaticChart();

    connect(&port, &QSerialPort::readyRead, this, &MainWindow::readData);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setOut(uint out)
{
    port.write(QString("o%1\r").arg(out).toLatin1());
}


void MainWindow::stop()
{
    staticValue = 0;
    port.write(QString("o%1\r").arg(800).toLatin1());
    stopTimer.stop();
    staticNext = 0;
}

inline qreal MainWindow::onStamp(const QString &stamp)
{
    qreal ts = stamp.toInt() / 1000.0;
    if (isnan(minX) || ts < minX)
        minX = ts;
    if (isnan(maxX) || ts > maxX)
        maxX = ts;
    axisX->setRange(minX - 5, maxX + 5);
    return ts;
}

inline uint MainWindow::onOut(const QString &value, qint64 localTime)
{
    const uint out = uint(value.toInt());
    if (staticValue && !staticNext && staticValue == out) { // Received value equals to specified => waiting of transients
        staticNext = localTime + staticDelay;
        staticSample = localTime + qint64(staticDelay * 0.7);
        stopTimer.stop();
    }
    if (staticNext && staticNext < localTime) { // Static sampling completed => next output value
        staticNext = 0;
        staticSample = 0;
        staticValue += staticStep;
        if (staticValue > staticMax)
            stop();
        else {
            setOut(staticValue);
            stopTimer.setInterval(1000);
        }
    }
    return out;
}

void MainWindow::readData()
{
    const QByteArray data = port.readAll();
    ui->statusBar->showMessage(data);
    for (const auto &line : QString(data).split("\r")) {
        auto items = line.split(" ");

        if (items[0] == "pbl") {
            qreal standTime = 0;
            qint64 localTime = QDateTime::currentMSecsSinceEpoch();
            uint out = 0;
            for (int i = 1; i < items.size(); ++i) {
                const auto item = items[i].split(":");
                const auto &key = item[0];
                if (key == "ts") {
                    standTime = onStamp(item[1]);
                    continue;
                }
                if (key == "out")
                    out = onOut(item[1], localTime);

                auto stdKey = key.toStdString();
                auto it = series.find(stdKey);
                if (it != series.end()) {
                    if (item.length() < 2) continue;
                    double value = item[1].toDouble();
                    it->second.append(standTime, value);
                    if (staticSample && staticSample < localTime) {
                        auto s = staticSeries.find(stdKey);
                        if (s != staticSeries.end())
                            s->second.sample(out, value);
                    }
                }
            }
        }
    }
}

void MainWindow::on_pushButton_clicked()
{
    if (port.isOpen())
        port.close();
    const auto infos = QSerialPortInfo::availablePorts();
    QString message = "";
    for (auto &info : infos) {
        if (info.vendorIdentifier() != 1155) continue;
        port.setPort(info);
        port.setBaudRate(115200);
        port.setDataBits(QSerialPort::Data8);
        port.setParity(QSerialPort::NoParity);
        port.setFlowControl(QSerialPort::NoFlowControl);
        port.setStopBits(QSerialPort::OneStop);
        if (!port.open(QIODevice::ReadWrite)) {
            message = "Unable to open port " + info.portName();
            continue;
        }
        port.write("?\re\r");
        message = "Connected to " + info.portName();
        break;
    }
    if (message == "")
        message = "STM port not found";
    ui->statusBar->showMessage(message);
}

void MainWindow::on_pushButton_2_clicked()
{
    if (port.isOpen())
        port.close();
    //ui->pushButton_2->setDisabled(true);
}

void MainWindow::on_pushButton_5_clicked()
{
    clear();
}

void MainWindow::on_pushButton_6_clicked()
{
    if (!port.isOpen()) {
        ui->statusBar->showMessage("Not connected!");
        return;
    }
    staticMin = static_cast<uint>(ui->sbStaticFrom->value());
    staticMax = static_cast<uint>(ui->sbStaticTo->value());
    staticStep = static_cast<uint>(ui->sbStaticStep->value());
    staticDelay = static_cast<qint64>(ui->sbStaticDelay->value() * 1000);
    staticValue = staticMin;
    port.write(QString("e\r").toLatin1());
    setOut(staticValue);
    stopTimer.setInterval(1000);
    staticNext = 0;
    staticSample = 0;
}

void MainWindow::on_buttonStop_clicked()
{
    stop();
}

void MainWindow::clear()
{
    minX = nan("");
    maxX = nan("");
    for (auto &i : series) i.second.clear();
    ui->graphicsView->repaint();
    for (auto &i : staticSeries) i.second.clear();
    ui->staticView->repaint();
}

void MainWindow::on_screenshotButton_clicked()
{
    auto fileName = QFileDialog::getSaveFileName(this, "Save screenshot as", QString(), "Images (*.png *.jpg)");
    if (fileName == "") return;
    auto chart = ui->chartsTabWidget->currentWidget();
    QPixmap p(chart->size());
    chart->render(&p);
    p.save(fileName);
}
