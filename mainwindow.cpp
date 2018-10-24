#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QSerialPortInfo>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    minX(NAN), maxX(NAN), minForce(NAN), maxForce(NAN), minVoltage(NAN), maxVoltage(NAN),
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

    forceSeries = new QLineSeries();
    chart->addSeries(forceSeries);
    axisForce = new QValueAxis();
    chart->addAxis(axisForce, Qt::AlignLeft);
    axisForce->setLinePenColor(forceSeries->pen().color());
    axisForce->setTitleText("g");
    forceSeries->attachAxis(axisX);
    forceSeries->attachAxis(axisForce);

    voltageSeries = new QLineSeries();
    chart->addSeries(voltageSeries);
    axisVoltage = new QValueAxis();
    chart->addAxis(axisVoltage, Qt::AlignLeft);
    axisVoltage->setLinePenColor(voltageSeries->pen().color());
    axisVoltage->setTitleText("V");
    voltageSeries->attachAxis(axisX);
    voltageSeries->attachAxis(axisVoltage);

    //chart->axisY()->setRange(-100, 500);
    //chart->setAxisX(axisX, voltageSeries);
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

void MainWindow::append(qreal x, qreal force, qreal voltage)
{
    if (isnan(minX) || x < minX)
        minX = x;
    if (isnan(maxX) || x > maxX)
        maxX = x;
    axisX->setRange(minX - 5, maxX + 5);
    if (isnan(minForce) || force < minForce)
        minForce = force;
    if (isnan(maxForce) || force > maxForce)
        maxForce = force;
    axisForce->setRange(minForce - 5, maxForce + 5);
    if (isnan(minVoltage) || voltage < minVoltage)
        minVoltage = voltage;
    if (isnan(maxVoltage) || voltage > maxVoltage)
        maxVoltage = voltage;
    axisVoltage->setRange(minVoltage - 5, maxVoltage + 5);
    forceSeries->append(x, force);
    voltageSeries->append(x, voltage);
}

void MainWindow::readData()
{
    const QByteArray data = port.readAll();
    ui->statusBar->showMessage(data);
    auto s = QString(data).split(" ");

    if (s[0] == "pbl") {
        auto out = s[2].split(":");
        auto stamp = QDateTime::currentMSecsSinceEpoch();
        if (staticValue && !staticNext && staticValue == uint(out[1].toInt())) {
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

        auto force = s[3].split(":");
        auto ts = s[1].split(":");
        int t = ts[1].toInt();
        auto voltage = s[4].split(":")[1].toDouble();
        //static int mint = 0;
        //if (mint == 0) mint = t;
        double xx = t / 1000.0;
        double yy = force[1].toDouble();
        append(xx, yy, voltage);
    }
    forceSeries->show();
    voltageSeries->show();
    //ui->graphicsView->repaint();
    //m_console->putData(data);
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
    forceSeries->clear();
    voltageSeries->clear();
    minX = NAN;
    maxX = NAN;
    minForce = NAN;
    maxForce = NAN;
    minVoltage = NAN;
    maxVoltage = NAN;
    ui->graphicsView->repaint();
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
