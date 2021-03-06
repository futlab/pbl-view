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
protected:
    QLineSeries series;
    QValueAxis axis;
    qreal minY, maxY;
    int maxCount;
public:
    void append(qreal x, qreal y);
    Series(QChart *chart, QAbstractAxis *axisX, const QString &name, const QString &units, int maxCount = 500);
    void clear();
};

class StaticSeries : public Series
{
private:
    uint lastX, count;
    qreal sum;
public:
    void sample(uint x, qreal y);
    StaticSeries(QChart *chart, QAbstractAxis *axisX, const QString &name, const QString &units);
    void clear();
    QString lastValueToString() const;
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
    void on_screenshotButton_clicked();
    void on_staticDumpEdit_textChanged(const QString &);
private:
    QFile staticDumpFile;
    QTimer stopTimer;
    uint staticValue, staticMin, staticMax, staticStep;
    qint64 staticDelay, staticNext, staticSample;
    qreal minX, maxX;
    QSerialPort port;
    QChart *chart, *staticChart;
    std::map<std::string, Series> series;
    std::map<std::string, StaticSeries> staticSeries;
    QAbstractAxis *axisX, *axisOut;
    Ui::MainWindow *ui;
    void clear();
    void setOut(uint out);
    qreal onStamp(const QString &stamp);
    uint onOut(const QString &value, qint64 localTime);
    void createChart();
    void createStaticChart();
    void dumpStatic(uint out);
};

#endif // MAINWINDOW_H
