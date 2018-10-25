#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QSerialPortInfo>


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
    minY = NAN;
    maxY = NAN;
}

void Series::append(qreal x, qreal y)
{
    if (isnan(minY) || y < minY)
        minY = y;
    if (isnan(maxY) || y > maxY)
        maxY = y;
    axis.setRange(minY - 1, maxY + 1);
    if (series.count() > maxCount)
        series.removePoints(0, 1);
    series.append(x, y);
    series.show();

}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    minX(NAN), maxX(NAN),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    stopTimer.setSingleShot(true);
    connect(&stopTimer, SIGNAL(timeout()), this, SLOT(stop()));

    chart = new QChart();
    chart->setTitle("Time view");
    chart->createDefaultAxes();
    axisX = new QValueAxis();
    chart->addAxis(axisX, Qt::AlignBottom);

    series.emplace(std::piecewise_construct, std::forward_as_tuple("f"), std::forward_as_tuple(chart, axisX, "Force", "g"));
    series.emplace(std::piecewise_construct, std::forward_as_tuple("v"), std::forward_as_tuple(chart, axisX, "Voltage", "V"));
    series.emplace(std::piecewise_construct, std::forward_as_tuple("i"), std::forward_as_tuple(chart, axisX, "Current", "A"));
    series.emplace(std::piecewise_construct, std::forward_as_tuple("out"), std::forward_as_tuple(chart, axisX, "Command", "μs"));

    axisX->setRange(0, 2000);

    ui->graphicsView->setChart(chart);
    ui->graphicsView->setRenderHint(QPainter::Antialiasing);

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

void MainWindow::readData()
{
    const QByteArray data = port.readAll();
    ui->statusBar->showMessage(data);
    for (const auto &line : QString(data).split("\r")) {
        auto items = line.split(" ");

        if (items[0] == "pbl") {
            qreal ts = 0;
            for (int i = 1; i < items.size(); ++i) {
                const auto item = items[i].split(":");
                const auto &key = item[0];
                if (key == "ts") {
                    ts = item[1].toInt() / 1000.0;
                    if (isnan(minX) || ts < minX)
                        minX = ts;
                    if (isnan(maxX) || ts > maxX)
                        maxX = ts;
                    axisX->setRange(minX - 5, maxX + 5);
                    continue;
                }
                if (key == "out") {
                    auto stamp = QDateTime::currentMSecsSinceEpoch();
                    if (staticValue && !staticNext && staticValue == uint(item[1].toInt())) {
                        staticNext = stamp + staticDelay;
                        stopTimer.stop();
                    }
                    if (staticNext && staticNext < stamp) {
                        staticNext = 0;
                        staticValue += staticStep;
                        if (staticValue > staticMax) {
                            stop();

                        } else {
                            setOut(staticValue);
                            stopTimer.setInterval(1000);
                        }
                    }
                }
                auto it = series.find(key.toStdString());
                if (it != series.end()) {
                    it->second.append(ts, item[1].toDouble());
                }
            }
        }
    }
    //ui->
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
}

void MainWindow::on_buttonStop_clicked()
{
    stop();
}

void MainWindow::clear()
{
    minX = NAN;
    maxX = NAN;
    for (auto &i : series) i.second.clear();
    ui->graphicsView->repaint();
}
