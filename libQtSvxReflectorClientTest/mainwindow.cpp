#include "mainwindow.h"
#include "ui_MainWindow.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);
    this->setFixedSize(600, 335);
    ref = new QtSvxReflectorClient(20, this);

    //ref->setAutoReconnectEnabled(true);

    connect(ref, &QtSvxReflectorClient::connectionStatusChanged, this, &MainWindow::onReflectorConnectionStateChanged);
    connect(ref, &QtSvxReflectorClient::userJoined, this, &MainWindow::onUserJoined);
    connect(ref, &QtSvxReflectorClient::userLeft, this, &MainWindow::onUserLeft);
    connect(ref, &QtSvxReflectorClient::receiveStart, this, &MainWindow::onReceiveStart);
    connect(ref, &QtSvxReflectorClient::receiveStop, this, &MainWindow::onReceiveStop);
    connect(ref, &QtSvxReflectorClient::connectedUsers, this, &MainWindow::onInitialUserList);
    restoreSettings();

    serialPort = new QSerialPort(this);
    serialPortInputPollTimer = new QTimer(this);
    connect(serialPortInputPollTimer, &QTimer::timeout, this, &MainWindow::pollSerialPortInputs);
    //connect(serialPortInputPollTimer, SIGNAL(timeout()), this, SLOT(pollSerialPortInputs()));
    isTransmitting = false;
    isTransmittingSerial = false;

    /*
    if(!ui->comboBox_serialPort->currentData().toString().isEmpty()) {
        serialPort->setPortName(ui->comboBox_serialPort->currentData().toString());
        if(!serialPort->open(QIODevice::ReadWrite)) {
            qWarning() << "Cannot open serial port";
        }
        else {
            serialPortInputPollTimer->start(50);
        }
    }
    */

    keyip.type = INPUT_KEYBOARD;
    keyip.ki.wScan = 0;
    keyip.ki.time = 0;
    keyip.ki.dwExtraInfo = 0;
    keyip.ki.wVk = VkKeyScan('a');
    keyip.ki.dwFlags = 0;
}

MainWindow::~MainWindow() {
    saveSettings();
    delete ref;
    delete ui;
}

void MainWindow::saveSettings() {
    QSettings settings("config.ini", QSettings::IniFormat);
    settings.setValue("host", ui->lineEdit_host->text());
    settings.setValue("port", ui->lineEdit_port->text());
    settings.setValue("callsign", ui->lineEdit_callsign->text());
    settings.setValue("password", ui->lineEdit_password->text());
    settings.setValue("selectedTalkgroup", ui->spinBox_selectedTalkgroup->value());
    settings.setValue("audioInputDevice", ui->comboBox_audioInput->currentData());
    settings.setValue("audioOutputDevice", ui->comboBox_audioOutput->currentData());
    settings.setValue("serialPort", ui->comboBox_serialPort->currentData());
}

void MainWindow::restoreSettings() {
    QSettings settings("config.ini", QSettings::IniFormat);
    ui->lineEdit_host->setText(settings.value("host").toString());
    ui->lineEdit_port->setText(settings.value("port").toString());
    ui->lineEdit_callsign->setText(settings.value("callsign").toString());
    ui->lineEdit_password->setText(settings.value("password").toString());
    ui->spinBox_selectedTalkgroup->setValue(settings.value("selectedTalkgroup").toInt());

    QString currentText;
    // ### input device
    foreach(QAudioDeviceInfo device, QAudioDeviceInfo::availableDevices(QAudio::AudioInput)) {
        QString deviceName = device.deviceName();
        if(deviceName==settings.value("audioInputDevice").toString()) {
            deviceName = "*" + deviceName;
            currentText = deviceName;
        }
        ui->comboBox_audioInput->addItem(deviceName, device.deviceName());
    }
    if(settings.value("audioInputDevice")=="disabled") {
        currentText = tr("*Disabled");
        ui->comboBox_audioInput->addItem(currentText, "disabled");
    }
    else {
        ui->comboBox_audioInput->addItem(tr("Disabled"), "disabled");
    }

    if(!currentText.isEmpty()) {
        ui->comboBox_audioInput->setCurrentText(currentText);
    }
    // ###
    currentText.clear();

    // ### output device
    foreach(QAudioDeviceInfo device, QAudioDeviceInfo::availableDevices(QAudio::AudioOutput)) {
        QString deviceName = device.deviceName();
        if(deviceName==settings.value("audioOutputDevice").toString()) {
            deviceName = "*" + deviceName;
            currentText = deviceName;
        }
        ui->comboBox_audioOutput->addItem(deviceName, device.deviceName());
    }
    if(settings.value("audioInputDevice")=="disabled") {
        currentText = tr("*Disabled");
        ui->comboBox_audioOutput->addItem(currentText, "disabled");
    }
    else {
        ui->comboBox_audioOutput->addItem(tr("Disabled"), "disabled");
    }
    if(!currentText.isEmpty()) {
        ui->comboBox_audioOutput->setCurrentText(currentText);
    }
    // ###

    currentText.clear();
    foreach(QSerialPortInfo info, QSerialPortInfo::availablePorts()) {
        QString portName = info.portName();
        if(portName==settings.value("serialPort").toString()) {
            portName = "*" + portName;
            currentText = portName;
        }
        ui->comboBox_serialPort->addItem(portName, info.portName());
    }
    QString textDisabled;
    if(settings.value("serialPort").toString().isEmpty()) {
        textDisabled = tr("*disabled");
    }
    else {
        textDisabled = tr("disabled");
    }
    ui->comboBox_serialPort->addItem(textDisabled, "");
    if(!currentText.isEmpty()) {
        ui->comboBox_serialPort->setCurrentText(currentText);
    }
    else {
        ui->comboBox_serialPort->setCurrentText(textDisabled);
    }
}

void MainWindow::on_pushButton_ptt_released() {
    if(isTransmitting) {
        if(!isTransmittingSerial) {
            ref->stopTransmit();
            qDebug() << "stop transmitting (manual)";
        }
        isTransmitting = false;
    }
}

void MainWindow::on_pushButton_ptt_pressed() {
    if(!isTransmitting) {
        if(!isTransmittingSerial) {
            ref->startTransmit();
            qDebug() << "start transmitting (manual)";
        }
        isTransmitting = true;
    }
}

void MainWindow::on_pushButton_connect_clicked() {
    if(ui->pushButton_connect->text()==tr("Connect")) {
        ui->comboBox_audioInput->setEnabled(false);
        ui->comboBox_audioOutput->setEnabled(false);
        ui->comboBox_serialPort->setEnabled(false);
        ui->lineEdit_host->setEnabled(false);
        ui->lineEdit_port->setEnabled(false);
        ui->lineEdit_callsign->setEnabled(false);
        ui->lineEdit_password->setEnabled(false);
        ui->spinBox_selectedTalkgroup->setEnabled(false);
        ui->pushButton_connect->setText(tr("Cancel"));

        if(ui->comboBox_audioInput->currentData().toString()!="disabled") {
            foreach(QAudioDeviceInfo device, QAudioDeviceInfo::availableDevices(QAudio::AudioInput)) {
                QString deviceName = device.deviceName();
                if(device.deviceName()==ui->comboBox_audioInput->currentData().toString()) {
                    ref->setAudioInputDevice(device);
                    break;
                }
            }
        }
        else {
            ref->setAudioInputDisabled(true);
        }
        if(ui->comboBox_audioOutput->currentData().toString()!="disabled") {
            foreach(QAudioDeviceInfo device, QAudioDeviceInfo::availableDevices(QAudio::AudioOutput)) {
                if(device.deviceName()==ui->comboBox_audioOutput->currentData().toString()) {
                    ref->setAudioOutputDevice(device);
                    break;
                }
            }
        }
        else {
            ref->setAudioOutputDisabled(true);
        }

        ref->connectReflector(ui->lineEdit_host->text(), static_cast<quint16>(ui->lineEdit_port->text().toUInt()), ui->lineEdit_callsign->text(), ui->lineEdit_password->text(), ui->spinBox_selectedTalkgroup->value());
    }
    else if(ui->pushButton_connect->text()==tr("Disconnect")) {
        ui->pushButton_connect->setText(tr("Disconnecting..."));
        ref->disconnectReflector();
    }
    else if(ui->pushButton_connect->text()==tr("Cancel")) {
        ui->pushButton_connect->setText(tr("Connect"));
        ref->disconnectReflector();
    }
}

void MainWindow::onInitialUserList(QStringList list) {
    ui->listWidget_userList->addItems(list);
}

void MainWindow::pollSerialPortInputs() {
    QSerialPort::PinoutSignals signal = serialPort->pinoutSignals();
    if(!isTransmittingSerial && (signal & QSerialPort::ClearToSendSignal || signal & QSerialPort::DataSetReadySignal || signal & QSerialPort::RingIndicatorSignal || signal & QSerialPort::DataCarrierDetectSignal)) {
        if(!isTransmitting) {
            ref->startTransmit();
            qDebug() << "start transmitting (serial)";
        }
        isTransmittingSerial = true;
    }
    if(isTransmittingSerial && !(signal & QSerialPort::ClearToSendSignal || signal & QSerialPort::DataSetReadySignal || signal & QSerialPort::RingIndicatorSignal || signal & QSerialPort::DataCarrierDetectSignal)) {
        if(!isTransmitting) {
            ref->stopTransmit();
            qDebug() << "stop transmitting (serial)";
        }
        isTransmittingSerial = false;
    }
}

void MainWindow::onReflectorConnectionStateChanged(QtSvxReflectorClient::connectionState state, QtSvxReflectorClient::connectionStateReason reason) {
    qDebug() << state << reason;
    if(state==QtSvxReflectorClient::CONNECTED) {
        ui->pushButton_connect->setText(tr("Disconnect"));
        ui->pushButton_ptt->setEnabled(true);
        ui->listWidget_userList->setEnabled(true);

        if(!ui->comboBox_serialPort->currentData().toString().isEmpty()) {
            serialPort->setPortName(ui->comboBox_serialPort->currentData().toString());
            if(!serialPort->open(QIODevice::ReadWrite)) {
                qWarning() << "Cannot open serial port";
            }
            else {
                serialPortInputPollTimer->start(50);
            }
        }
    }
    else if(state==QtSvxReflectorClient::DISCONNECTED) {
        ui->pushButton_connect->setText(tr("Connect"));
        ui->pushButton_ptt->setEnabled(false);
        ui->listWidget_userList->clear();
        ui->listWidget_userList->setEnabled(false);
        ui->comboBox_audioInput->setEnabled(true);
        ui->comboBox_audioOutput->setEnabled(true);
        ui->comboBox_serialPort->setEnabled(true);
        ui->lineEdit_host->setEnabled(true);
        ui->lineEdit_port->setEnabled(true);
        ui->lineEdit_callsign->setEnabled(true);
        ui->lineEdit_password->setEnabled(true);
        ui->spinBox_selectedTalkgroup->setEnabled(true);

        if(serialPort->isOpen()) {
            serialPortInputPollTimer->stop();
            serialPort->close();
        }
    }
    else if(state==QtSvxReflectorClient::CONNECTING) {

    }
    else if(state==QtSvxReflectorClient::DISCONNECTING) {

    }
}

void MainWindow::onUserJoined(QString callsign) {
    ui->listWidget_userList->addItem(callsign);
}

void MainWindow::onUserLeft(QString callsign) {
    QList<QListWidgetItem *> items = ui->listWidget_userList->findItems(callsign, Qt::MatchExactly);
    foreach(QListWidgetItem *i, items) {
        ui->listWidget_userList->takeItem(ui->listWidget_userList->row(i));
        delete i;
    }
}

void MainWindow::onReceiveStart(QString callsign) {
    ui->label_talkingUser->setText(callsign);
    if(serialPort->isOpen()) {
        serialPort->setDataTerminalReady(true);
    }
    //keyip.ki.dwFlags = 0;
    //SendInput(1, &keyip, sizeof(INPUT));
}

void MainWindow::onReceiveStop(QString callsign) {
    if(ui->label_talkingUser->text()==callsign) {
        ui->label_talkingUser->clear();
    }
    if(serialPort->isOpen()) {
        serialPort->setDataTerminalReady(false);
    }
    //keyip.ki.dwFlags = KEYEVENTF_KEYUP;
    //SendInput(1, &keyip, sizeof(INPUT));
}
