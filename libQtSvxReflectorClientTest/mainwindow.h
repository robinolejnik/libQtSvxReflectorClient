#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDebug>
#include <QAudioOutputSelectorControl>
#include <QSettings>
#include <QSerialPortInfo>
#include <QSerialPort>
#include <QTimer>
#include <qtsvxreflectorclient.h>
#include "windows.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void saveSettings();
    void restoreSettings();
    void on_pushButton_ptt_released();
    void on_pushButton_ptt_pressed();

    void on_pushButton_connect_clicked();

    void pollSerialPortInputs();
    void onReflectorConnectionStateChanged(QtSvxReflectorClient::connectionState state, QtSvxReflectorClient::connectionStateReason reason);
    void onInitialUserList(QStringList list);
    void onUserJoined(QString callsign);
    void onUserLeft(QString callsign);
    void onReceiveStart(QString callsign);
    void onReceiveStop(QString callsign);

private:
    Ui::MainWindow *ui;
    QtSvxReflectorClient *ref;
    QSerialPort *serialPort;
    QTimer *serialPortInputPollTimer;
    bool isTransmitting;
    bool isTransmittingSerial;
    INPUT keyip;

};

#endif // MAINWINDOW_H
