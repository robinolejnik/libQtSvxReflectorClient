#ifndef QTSVXREFLECTORCLIENT_H
#define QTSVXREFLECTORCLIENT_H

#include <QAudioInput>
#include <QAudioOutput>
#include "libqtsvxreflectorclient_global.h"

class QtSvxReflectorClientPrivate;

class LIBQTSVXREFLECTORCLIENTSHARED_EXPORT QtSvxReflectorClient : public QObject {
    Q_OBJECT
    Q_DECLARE_PRIVATE(QtSvxReflectorClient)

public:
    QtSvxReflectorClient(float framesize = 20, QObject *parent = nullptr);
    ~QtSvxReflectorClient();

    typedef enum {
        DISCONNECTED, CONNECTED, DISCONNECTING, CONNECTING
    } connectionState;
    Q_ENUM(connectionState)
    typedef enum {
        OK, ORDERED, NOTFOUND, UNREACHABLE, WRONGCREDENTIALS, ALREADYCONNECTED, SERVERTOOOLD, OTHERERROR
    } connectionStateReason;
    Q_ENUM(connectionStateReason)

signals:
    void connectionStatusChanged(connectionState state, connectionStateReason reason);
    void reflectorConnected();
    void reflectorDisconnected();

    void connectedUsers(QStringList list);
    void userJoined(QString callsign);
    void userLeft(QString callsign);
    void receiveStart(QString callsign);
    void receiveStop(QString callsign);

public slots:
    void connectReflector(QString host, quint16 port, QString callsign, QString password, int selectedTalkgroup);
    void disconnectReflector();
    void setAutoReconnectEnabled(bool autoReconnectEnabled);
    void setAudioInputDevice(QAudioDeviceInfo inputDevice);
    void setAudioOutputDevice(QAudioDeviceInfo outputDevice);
    void setAudioInputDisabled(bool audioInputDisabled);
    void setAudioOutputDisabled(bool audioOutputDisabled);
    void startTransmit();
    void stopTransmit();
    bool isReflectorConnected();
    bool isAutoReconnectEnabled();
    bool isAudioInputDisabled();
    bool isAudioOutputDisabled();
    bool isTransmitting();
    bool isReceiving();



protected:
    QtSvxReflectorClientPrivate *d_ptr;
};

#endif // QTSVXREFLECTORCLIENT_H
