#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <map>
#include <string>
#include <QMainWindow>
#include <QtCharts/QtCharts>
QT_CHARTS_USE_NAMESPACE
#include <QSerialPort>

namespace Ui {
class MainWindow;
}

class Series
{
private:
    QLineSeries series;
    QValueAxis axis;
    qreal minY, maxY;
    int maxCount;
public:
    void append(qreal x, qreal y);
    Series(QChart *chart, QAbstractAxis *axisX, const QString &name, const QString &units, int maxCount = 500);
    void clear();
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void stop();
    void on_pushButton_clicked();
    void readData();
    void on_pushButton_2_clicked();

    void on_pushButton_5_clicked();

    void on_pushButton_6_clicked();

    void on_buttonStop_clicked();

private:
    QTimer stopTimer;
    uint staticValue, staticMin, staticMax, staticStep;
    qint64 staticDelay, staticNext, staticSample;
    qreal minX, maxX;
    QSerialPort port;
    QChart *chart;
    std::map<std::string, Series> series;
    std::map<std::string, Series> staticSeries;
    QAbstractAxis *axisX;
    Ui::MainWindow *ui;
    void clear();
    void setOut(uint out);
};

#endif // MAINWINDOW_H
