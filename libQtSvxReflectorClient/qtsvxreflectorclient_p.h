#ifndef QTSVXREFLECTORCLIENT_P_H
#define QTSVXREFLECTORCLIENT_P_H

#include "qtsvxreflectorclient.h"
#include <QTimer>
#include <QDateTime>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QAudioInput>
#include <QAudioOutput>
#include <opus.h>
#include "ReflectorMsg.h"

class QtSvxReflectorClientPrivate : QObject {
    Q_OBJECT
    Q_DISABLE_COPY(QtSvxReflectorClientPrivate)
    Q_DECLARE_PUBLIC(QtSvxReflectorClient)
    QtSvxReflectorClient *q_ptr;

public:
    QtSvxReflectorClientPrivate(QtSvxReflectorClient *a, float framesize_ms);
    ~QtSvxReflectorClientPrivate();

    typedef enum {
    STATE_DISCONNECTED, STATE_EXPECT_AUTH_CHALLENGE, STATE_EXPECT_AUTH_OK, STATE_EXPECT_SERVER_INFO, STATE_CONNECTED
    } ConState;
    Q_ENUM(ConState)

    void connectReflector();
    void disconnectReflector(QtSvxReflectorClient::connectionStateReason reason);

    QString callsign;
    QString passwd;
    QString reflectorHost;
    quint16 reflectorPort;

    QAudioDeviceInfo inputDevice;
    QAudioDeviceInfo outputDevice;
    bool inputDisabled;
    bool outputDisabled;
    bool automaticReconnect;
    bool transmitting;
    bool receiving;
    bool isConnected();

private:
    struct MonitorTgEntry {
        uint32_t    tg;
        uint8_t     prio;
        mutable int timeout;
        MonitorTgEntry(uint32_t tg=0) : tg(tg), prio(0), timeout(0) {}
        bool operator<(const MonitorTgEntry& mte) const { return tg < mte.tg; }
        bool operator==(const MonitorTgEntry& mte) const { return tg == mte.tg; }
        operator uint32_t(void) const { return tg; }
    };
    typedef std::set<MonitorTgEntry> MonitorTgsSet;

    static const unsigned UDP_HEARTBEAT_TX_CNT_RESET  = 15;
    static const unsigned UDP_HEARTBEAT_RX_CNT_RESET  = 60;
    static const unsigned TCP_HEARTBEAT_TX_CNT_RESET  = 10;
    static const unsigned TCP_HEARTBEAT_RX_CNT_RESET  = 15;
    static const unsigned DEFAULT_TG_SELECT_TIMEOUT   = 30;
    static const int      DEFAULT_TMP_MONITOR_TIMEOUT = 3600;

    QTcpSocket *tcpSocket;
    QUdpSocket *udpSocket;
    uint16_t m_next_udp_tx_seq;
    uint16_t m_next_udp_rx_seq;
    uint16_t m_client_id;
    ConState m_con_state;
    unsigned                          m_tg_select_timeout_cnt; // TODO
    unsigned                          m_tg_select_timeout; // TODO
    bool                              m_mute_first_tx_loc;
    bool                              m_mute_first_tx_rem;
    bool                              m_tg_local_activity;
    uint32_t m_last_qsy;
    uint32_t m_previous_tg;
    uint32_t m_default_tg;
    uint32_t m_selected_tg;
    unsigned m_udp_heartbeat_tx_cnt;
    unsigned m_udp_heartbeat_rx_cnt;
    unsigned m_tcp_heartbeat_tx_cnt;
    unsigned m_tcp_heartbeat_rx_cnt;
    QTimer *reconnectTimer;
    QTimer *heartbeatTimer;
    QTimer *flushTimeoutTimer;
    QTimer *connectTimeoutTimer;
    QTimer *m_tg_select_timer;
    QTimer *m_report_tg_timer;
    QDateTime m_last_talker_timestamp;
    bool talkerTimeoutActive;
    MonitorTgsSet                     m_monitor_tgs;
    OpusEncoder *opusenc;
    OpusDecoder *opusdec;
    int dec_frame_size;

    QAudioInput *audioInput;
    QAudioOutput *audioOutput;
    QIODevice *audioOutputDevice;
    QIODevice *audioInputDevice;

    quint64 inputFrameSize;
    quint64 inputBufferLen;
    float *inputSampleBuffer;

    int audioRate;

private slots:

    void connectTimeout();

    void onm_con_stateChanged(QAbstractSocket::SocketState state);






    void onConnected(void);
    void onDisconnected();
    //void onDisconnected(Async::TcpConnection *con, Async::TcpConnection::DisconnectReason reason);
    void onFrameReceived();
    //void onFrameReceived(Async::FramedTcpConnection *con, std::vector<uint8_t>& data);
    void handleMsgError(std::istream& is);
    void handleMsgProtoVerDowngrade(std::istream& is);
    void handleMsgAuthChallenge(std::istream& is);
    void handleMsgNodeList(std::istream& is);
    void handleMsgNodeJoined(std::istream& is);
    void handleMsgNodeLeft(std::istream& is);
    void handleMsgTalkerStart(std::istream& is);
    void handleMsgTalkerStop(std::istream& is);
    void handleMsgRequestQsy(std::istream& is);
    void handleMsgAuthOk(void);
    void handleMsgServerInfo(std::istream& is);
    void sendMsg(const ReflectorMsg& msg);
    //void sendEncodedAudio(const void *buf, int count);
    void flushEncodedAudio(void);
    void udpDatagramReceived();
    //void udpDatagramReceived(const Async::IpAddress& addr, uint16_t port, void *buf, int count);
    void sendUdpMsg(const ReflectorUdpMsg& msg);
    void reconnectReflector();
    bool isConnected(void) const;
    //################bool isLoggedIn(void) const { return m_con_state == STATE_CONNECTED; }
    void allEncodedSamplesFlushed(void);
    void flushTimeout(void);
    //void flushTimeout(Async::Timer *t=0);
    void handleTimerTick(void);
    //void handleTimerTick(Async::Timer *t);
    void tgSelectTimerExpired(void);
    void onLogicConInStreamStateChanged(bool is_active, bool is_idle);
    void onLogicConOutStreamStateChanged(bool is_active, bool is_idle);
    void selectTg(uint32_t tg, bool unmute);
    void processTgSelectionEvent(void);
    void checkTmpMonitorTimeout(void);





    void decodeReceivedSamples(void *buf, int size);
    void encodeTransmitSamples();

public slots:
    void startTransmit();
    void stopTransmit();
};

#endif // QTSVXREFLECTORCLIENT_P_H
