#include "qtsvxreflectorclient.h"
#include "qtsvxreflectorclient_p.h"
#include <QAudioInput>
#include <QAudioOutput>

QtSvxReflectorClient::QtSvxReflectorClient(float framesize_ms, QObject *parent) : QObject(parent), d_ptr(new QtSvxReflectorClientPrivate(this, framesize_ms)) {

}

QtSvxReflectorClient::~QtSvxReflectorClient() {
    Q_D(QtSvxReflectorClient);
    d->disconnectReflector(QtSvxReflectorClient::OK);
}

void QtSvxReflectorClient::connectReflector(QString host, quint16 port, QString callsign, QString password, int selectedTalkgroup) {
    Q_D(QtSvxReflectorClient);
    d->reflectorHost = host;
    d->reflectorPort = port;
    d->callsign = callsign;
    d->passwd = password;
    d->m_selected_tg = selectedTalkgroup;
    d->connectReflector();
}

void QtSvxReflectorClient::disconnectReflector(void) {
    Q_D(QtSvxReflectorClient);
    d->disconnectReflector(QtSvxReflectorClient::OK);
}

void QtSvxReflectorClient::setAutoReconnectEnabled(bool autoReconnect) {
    Q_D(QtSvxReflectorClient);
    d->automaticReconnect = autoReconnect;
}

void QtSvxReflectorClient::setAudioInputDevice(QAudioDeviceInfo audioInputDevice) {
    Q_D(QtSvxReflectorClient);
    if(!d->transmitting) {
        d->inputDisabled = false;
        d->inputDevice = audioInputDevice;
    }
}

void QtSvxReflectorClient::setAudioOutputDevice(QAudioDeviceInfo audioOutputDevice) {
    Q_D(QtSvxReflectorClient);
    if(!d->transmitting) {
        d->outputDisabled = false;
        d->outputDevice = audioOutputDevice;
    }
}

void QtSvxReflectorClient::setAudioInputDisabled(bool audioInputDisabled) {
    Q_D(QtSvxReflectorClient);
    if(!d->transmitting) {
        d->inputDisabled = audioInputDisabled;
    }
}

void QtSvxReflectorClient::setAudioOutputDisabled(bool audioOutputDisabled) {
    Q_D(QtSvxReflectorClient);
    if(!d->transmitting) {
        d->inputDisabled = audioOutputDisabled;
    }
}

void QtSvxReflectorClient::startTransmit() {
    Q_D(QtSvxReflectorClient);
    if(d->isConnected()) {
        d->startTransmit();
    }
}

void QtSvxReflectorClient::stopTransmit() {
    Q_D(QtSvxReflectorClient);
    if(d->isConnected()) {
        d->stopTransmit();
    }
}

bool QtSvxReflectorClient::isReflectorConnected() {
    Q_D(QtSvxReflectorClient);
    return d->isConnected();
}

bool QtSvxReflectorClient::isAutoReconnectEnabled() {
    Q_D(QtSvxReflectorClient);
    return d->automaticReconnect;
}

bool QtSvxReflectorClient::isAudioInputDisabled() {
    Q_D(QtSvxReflectorClient);
    return d->inputDisabled;
}

bool QtSvxReflectorClient::isAudioOutputDisabled() {
    Q_D(QtSvxReflectorClient);
    return d->outputDisabled;
}

bool QtSvxReflectorClient::isTransmitting() {
    Q_D(QtSvxReflectorClient);
    return d->transmitting;
}

bool QtSvxReflectorClient::isReceiving() {
    Q_D(QtSvxReflectorClient);
    return d->receiving;
}
