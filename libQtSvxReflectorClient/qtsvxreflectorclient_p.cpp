#include "qtsvxreflectorclient_p.h"
#include <QTimer>
#include <QDateTime>
#include <QHostAddress>
#include <QNetworkDatagram>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QAudioInput>
#include <QAudioOutput>
#include <QAudioDecoder>
#include <QAudioDeviceInfo>
#include <QAudioFormat>
#include <QIODevice>
#include <sstream>
#include <opus.h>

#include "ReflectorMsg.h"

QtSvxReflectorClientPrivate::QtSvxReflectorClientPrivate(QtSvxReflectorClient *q, float framesize_ms) : q_ptr(q) {
    //m_flush_timeout_timer(3000, Timer::TYPE_ONESHOT, false);
    automaticReconnect = false;
    inputDisabled = true;
    outputDisabled = true;
    transmitting = false;
    receiving = false;
    audioRate = 48000;

    m_selected_tg = 0;
    //m_monitor_tgs.insert(MonitorTgEntry(1));

    tcpSocket = nullptr;
    udpSocket = nullptr;
    m_next_udp_rx_seq = 0;
    m_next_udp_tx_seq = 0;
    m_udp_heartbeat_rx_cnt = 0;
    m_udp_heartbeat_tx_cnt = 0;
    m_tcp_heartbeat_rx_cnt = 0;
    m_tcp_heartbeat_tx_cnt = 0;
    m_con_state = STATE_DISCONNECTED;

    reconnectTimer = new QTimer(this);
    heartbeatTimer = new QTimer(this);
    flushTimeoutTimer = new QTimer(this);
    m_report_tg_timer = new QTimer(this);
    m_tg_select_timer = new QTimer(this);
    connect(reconnectTimer, SIGNAL(timeout()), this, SLOT(reconnectReflector()));
    connect(heartbeatTimer, SIGNAL(timeout()), this, SLOT(handleTimerTick()));
    connect(flushTimeoutTimer, SIGNAL(timeout()), this, SLOT(flushTimeout()));
    connect(m_report_tg_timer, SIGNAL(timeout()), this, SLOT(processTgSelectionEvent()));
    connect(m_tg_select_timer, SIGNAL(timeout()), this, SLOT(tgSelectTimerExpired()));
    heartbeatTimer->setInterval(1000);
    reconnectTimer->setInterval(1000);
    reconnectTimer->setSingleShot(true);
    reconnectTimer->stop();

    int error;
    opusdec = opus_decoder_create(audioRate, 1, &error);
    if(error != OPUS_OK) {
      qWarning() << "*** ERROR: Could not initialize Opus decoder";
    }

    opusenc = opus_encoder_create(audioRate, 1, OPUS_APPLICATION_AUDIO, &error);
    if(error != OPUS_OK) {
      qWarning() << "*** ERROR: Could not initialize Opus encoder";
    }
    //opus_encoder_ctl(opusenc, OPUS_SET_BITRATE(20000));
    //opus_encoder_ctl(opusenc, OPUS_SET_COMPLEXITY(9));
    //opus_encoder_ctl(opusenc, OPUS_SET_VBR(1));

    inputFrameSize = static_cast<quint64>(framesize_ms * audioRate / 1000);
    inputSampleBuffer = new float[inputFrameSize];
}

QtSvxReflectorClientPrivate::~QtSvxReflectorClientPrivate() {
    delete inputSampleBuffer;
    opus_encoder_destroy(opusenc);
    opus_decoder_destroy(opusdec);
}

bool QtSvxReflectorClientPrivate::isConnected() {
    return m_con_state==STATE_CONNECTED;
}

void QtSvxReflectorClientPrivate::startTransmit() {
    if(!inputDisabled && !transmitting) {
        transmitting = true;
        audioInputDevice->readAll();
        connect(audioInputDevice, SIGNAL(readyRead()), this, SLOT(encodeTransmitSamples()));
    }
}

void QtSvxReflectorClientPrivate::stopTransmit() {
    if(!inputDisabled && transmitting) {
        transmitting = false;
        disconnect(audioInputDevice, SIGNAL(readyRead()), this, SLOT(encodeTransmitSamples()));
        flushEncodedAudio();
    }
}

void QtSvxReflectorClientPrivate::connectReflector() {
    if(tcpSocket==nullptr) {
        tcpSocket = new QTcpSocket(this);
        connect(tcpSocket, SIGNAL(connected()), this, SLOT(onConnected()));
        connect(tcpSocket, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
        connect(tcpSocket, SIGNAL(readyRead()), this, SLOT(onFrameReceived()));
        connect(tcpSocket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(onm_con_stateChanged(QAbstractSocket::SocketState)));
        tcpSocket->connectToHost(reflectorHost, reflectorPort);
        reconnectTimer->start();
        qDebug() << "connectReflector::reconnectTimer::isActive" << reconnectTimer->isActive();
    }
}

void QtSvxReflectorClientPrivate::disconnectReflector(QtSvxReflectorClient::connectionStateReason reason) {
    Q_Q(QtSvxReflectorClient);
    if(tcpSocket!=nullptr) {
        if(tcpSocket->state()==QTcpSocket::ConnectedState) {
            tcpSocket->disconnectFromHost();
        }
        delete tcpSocket;
        tcpSocket = nullptr;
        m_con_state = STATE_DISCONNECTED;
        reconnectTimer->stop();
    }
    emit q->connectionStatusChanged(QtSvxReflectorClient::DISCONNECTED, reason);
}

void QtSvxReflectorClientPrivate::reconnectReflector(void) {
    disconnectReflector(QtSvxReflectorClient::UNREACHABLE);
    if(automaticReconnect) {
        connectReflector();
    }
}

void QtSvxReflectorClientPrivate::connectTimeout() {

}

void QtSvxReflectorClientPrivate::onConnected() {
    Q_Q(QtSvxReflectorClient);
    sendMsg(MsgProtoVer());
    m_udp_heartbeat_tx_cnt = UDP_HEARTBEAT_TX_CNT_RESET;
    m_udp_heartbeat_rx_cnt = UDP_HEARTBEAT_RX_CNT_RESET;
    m_tcp_heartbeat_tx_cnt = TCP_HEARTBEAT_TX_CNT_RESET;
    m_tcp_heartbeat_rx_cnt = TCP_HEARTBEAT_RX_CNT_RESET;
    m_next_udp_tx_seq = 0;
    m_next_udp_rx_seq = 0;
    reconnectTimer->stop();
    if(automaticReconnect) {
        reconnectTimer->start();
    }
    heartbeatTimer->start();
    talkerTimeoutActive = false;
    m_con_state = STATE_EXPECT_AUTH_CHALLENGE;
    //m_con->setMaxFrameSize(ReflectorMsg::MAX_PREAUTH_FRAME_SIZE);

    QAudioFormat format;
    format.setSampleRate(audioRate);
    format.setChannelCount(1);
    format.setSampleSize(32);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setSampleType(QAudioFormat::Float);

    if(!inputDisabled) {
        if(!inputDevice.isFormatSupported(format)) {
            qWarning() << "Raw audio format not supported by backend, cannot record audio.";
            return;
        }
        audioInput = new QAudioInput(inputDevice, format, this);
        audioInputDevice = audioInput->start();
    }
    if(!outputDisabled) {
        if(!outputDevice.isFormatSupported(format)) {
            qWarning() << "Raw audio format not supported by backend, cannot play audio.";
            return;
        }
        audioOutput = new QAudioOutput(outputDevice, format, this);
        audioOutputDevice = audioOutput->start();
    }
    emit q->connectionStatusChanged(QtSvxReflectorClient::CONNECTED, QtSvxReflectorClient::OK);
}

void QtSvxReflectorClientPrivate::onDisconnected() {
    Q_Q(QtSvxReflectorClient);
    if(!inputDisabled) {
        audioInput->stop();
    }
    if(!outputDisabled) {
        audioOutput->stop();
    }
    if(automaticReconnect) {
        reconnectTimer->start();
    }
    if(udpSocket!=nullptr) {
        delete udpSocket;
        udpSocket = nullptr;
    }
    m_next_udp_tx_seq = 0;
    m_next_udp_rx_seq = 0;
    heartbeatTimer->stop();
    if(flushTimeoutTimer->isActive()) {
        flushTimeoutTimer->stop();
        allEncodedSamplesFlushed();
    }
    if(talkerTimeoutActive) {
        //m_dec->flushEncodedSamples();
        talkerTimeoutActive = false;
    }
    m_con_state = STATE_DISCONNECTED;
    transmitting = false;
    receiving = false;
    // TODO
    m_monitor_tgs.clear();
    emit q->connectionStatusChanged(QtSvxReflectorClient::DISCONNECTED, QtSvxReflectorClient::OK);
}

void QtSvxReflectorClientPrivate::onm_con_stateChanged(QAbstractSocket::SocketState state) {
    Q_UNUSED(state)
    //qDebug() << "onTcpStateChanged" << state;
}

void QtSvxReflectorClientPrivate::onFrameReceived() {
    int len = static_cast<int>(tcpSocket->bytesAvailable());
    int remainingBytes = len;
    QByteArray data = tcpSocket->readAll();

    while(remainingBytes>=4) {
        int frameSize = 0;
        frameSize += static_cast<unsigned int>(data.at(0) << 24);
        frameSize += static_cast<unsigned int>(data.at(1) << 16);
        frameSize += static_cast<unsigned int>(data.at(2) << 8);
        frameSize += static_cast<unsigned int>(data.at(3) << 0);
        data.remove(0, 4);
        if(frameSize>data.size() || frameSize>len) {
            return;
        }
        std::stringstream ss;
        ss.write(data.data(), static_cast<std::streamsize>(frameSize));
        data.remove(0, frameSize);
        remainingBytes -= frameSize+4;

        ReflectorMsg header;
        if(!header.unpack(ss)) {
          qWarning() << "*** ERROR: Unpacking failed for TCP message header";
          disconnectReflector(QtSvxReflectorClient::OTHERERROR);
          return;
        }

        if((header.type() > 100) && (m_con_state != STATE_CONNECTED)) {
          qWarning() << "*** ERROR: Unexpected protocol message received";
          disconnectReflector(QtSvxReflectorClient::OTHERERROR);
          return;
        }

        m_tcp_heartbeat_rx_cnt = TCP_HEARTBEAT_RX_CNT_RESET;
        switch (header.type()) {
        case MsgHeartbeat::TYPE:
          break;
        case MsgError::TYPE:
          handleMsgError(ss);
          break;
        case MsgProtoVerDowngrade::TYPE:
          handleMsgProtoVerDowngrade(ss);
          break;
        case MsgAuthChallenge::TYPE:
          handleMsgAuthChallenge(ss);
          break;
        case MsgAuthOk::TYPE:
          handleMsgAuthOk();
          break;
        case MsgServerInfo::TYPE:
          handleMsgServerInfo(ss);
          break;
        case MsgNodeList::TYPE:
          handleMsgNodeList(ss);
          break;
        case MsgNodeJoined::TYPE:
          handleMsgNodeJoined(ss);
          break;
        case MsgNodeLeft::TYPE:
          handleMsgNodeLeft(ss);
          break;
        case MsgTalkerStart::TYPE:
          handleMsgTalkerStart(ss);
          break;
        case MsgTalkerStop::TYPE:
          handleMsgTalkerStop(ss);
          break;
        case MsgRequestQsy::TYPE:
          handleMsgRequestQsy(ss);
          break;
        default:
            qDebug() << "onFrameReceived default";
            // Better just ignoring unknown messages for easier addition of protocol
            // messages while being backwards compatible

            //qDebug() << "*** WARNING[" << ""
            //     << "]: Unknown protocol message received: msg_type="
            //     << header.type();
            break;
        }
    }
}

void QtSvxReflectorClientPrivate::sendMsg(const ReflectorMsg &msg) {
    if(tcpSocket==nullptr || tcpSocket->state()!=QTcpSocket::ConnectedState) {
        return;
    }
    m_tcp_heartbeat_tx_cnt = TCP_HEARTBEAT_TX_CNT_RESET;

    std::ostringstream ss;
    ReflectorMsg header(msg.type());
    if(!header.pack(ss) || !msg.pack(ss)) {
      qWarning() << "*** ERROR: Failed to pack reflector TCP message";
      disconnectReflector(QtSvxReflectorClient::OTHERERROR);
      return;
    }

    quint64 datalen = ss.str().size();
    QByteArray data(ss.str().data(), static_cast<int>(datalen));

    data.push_front(static_cast<char>((datalen >> 0) & 0xff));
    data.push_front(static_cast<char>((datalen >> 8) & 0xff));
    data.push_front(static_cast<char>((datalen >> 16) & 0xff));
    data.push_front(static_cast<char>((datalen >> 24) & 0xff));

    if(tcpSocket->write(data) == -1) {
      disconnectReflector(QtSvxReflectorClient::OTHERERROR);
    }
}

void QtSvxReflectorClientPrivate::handleMsgNodeList(std::istream& is) {
    qDebug() << "handleMsgNodeList";
    MsgNodeList msg;
    if(!msg.unpack(is)) {
        qWarning() << "*** ERROR: Could not unpack MsgNodeList";
        disconnectReflector(QtSvxReflectorClient::OTHERERROR);
        return;
    }
    qDebug() << "Connected nodes: ";
    const std::vector<std::string>& nodes = msg.nodes();
    if(!nodes.empty()) {
        std::vector<std::string>::const_iterator it = nodes.begin();
        qDebug() << &*it++;
        for (; it != nodes.end(); ++it) {
            qDebug() << ", " << &*it;
        }
    }
}

void QtSvxReflectorClientPrivate::handleMsgError(std::istream& is) {
    qDebug() << "handleMsgError";
    MsgError msg;
    if(!msg.unpack(is)) {
        qWarning() << "*** ERROR: Could not unpack MsgAuthError";
        disconnectReflector(QtSvxReflectorClient::OTHERERROR);
        return;
    }
    qDebug() << "Error message received from server: " << &msg.message();
    disconnectReflector(QtSvxReflectorClient::OTHERERROR);
}

void QtSvxReflectorClientPrivate::handleMsgAuthChallenge(std::istream& is) {
    if(m_con_state != STATE_EXPECT_AUTH_CHALLENGE) {
        qWarning() << "*** ERROR: Unexpected MsgAuthChallenge";
        disconnectReflector(QtSvxReflectorClient::OTHERERROR);
        return;
    }

    MsgAuthChallenge msg;
    if(!msg.unpack(is)) {
        qWarning() << "*** ERROR: Could not unpack MsgAuthChallenge";
        disconnectReflector(QtSvxReflectorClient::OTHERERROR);
        return;
    }
    const uint8_t *challenge = msg.challenge();
    if(challenge == nullptr) {
        qWarning() << "*** ERROR: Illegal challenge received";
        disconnectReflector(QtSvxReflectorClient::OTHERERROR);
        return;
    }
    qDebug() << "HandleMsgAuthChallenge" << passwd << challenge;
    sendMsg(MsgAuthResponse(callsign.toStdString(), passwd.toStdString(), challenge));
    m_con_state = STATE_EXPECT_AUTH_OK;
}

void QtSvxReflectorClientPrivate::handleMsgAuthOk(void) {
    if(m_con_state != STATE_EXPECT_AUTH_OK) {
        qWarning() << "*** ERROR: Unexpected MsgAuthOk";
        disconnectReflector(QtSvxReflectorClient::OTHERERROR);
        return;
    }
    m_con_state = STATE_EXPECT_SERVER_INFO;
    //m_con->setMaxFrameSize(ReflectorMsg::MAX_POSTAUTH_FRAME_SIZE);
}

void QtSvxReflectorClientPrivate::handleMsgNodeJoined(std::istream& is) {
    qDebug() << "handleMsgNodeJoined";
    Q_Q(QtSvxReflectorClient);
    MsgNodeJoined msg;
    if(!msg.unpack(is)) {
        qWarning() << "*** ERROR: Could not unpack MsgNodeJoined";
        disconnectReflector(QtSvxReflectorClient::OTHERERROR);
        return;
    }
    emit q->userJoined(QString::fromStdString(msg.callsign()));
    qDebug() << "Node joined: " << QString::fromStdString(msg.callsign());
}

void QtSvxReflectorClientPrivate::handleMsgNodeLeft(std::istream& is) {
    qDebug() << "handleMsgNodeLeft";
    Q_Q(QtSvxReflectorClient);
    MsgNodeLeft msg;
    if(!msg.unpack(is)) {
        qWarning() << "*** ERROR: Could not unpack MsgNodeLeft";
        disconnectReflector(QtSvxReflectorClient::OTHERERROR);
        return;
    }
    emit q->userLeft(QString::fromStdString(msg.callsign()));
    qDebug() << "Node left: " << QString::fromStdString(msg.callsign());
}

void QtSvxReflectorClientPrivate::handleMsgTalkerStart(std::istream& is) {
    Q_Q(QtSvxReflectorClient);
    MsgTalkerStart msg;
    if(!msg.unpack(is)) {
        qWarning() << "*** ERROR: Could not unpack MsgTalkerStart";
        disconnectReflector(QtSvxReflectorClient::OTHERERROR);
        return;
    }

    qDebug() << "Talker start on TG #" << msg.tg() << ": " << QString::fromStdString(msg.callsign());

    // Select the incoming TG if idle
    //if (m_tg_select_timeout_cnt == 0) {
    //    selectTg(msg.tg(), !m_mute_first_tx_rem);
    //}
    //else {
    /*    uint32_t selected_tg_prio = 0;
        MonitorTgsSet::const_iterator selected_tg_it = m_monitor_tgs.find(MonitorTgEntry(m_selected_tg));
        if (selected_tg_it != m_monitor_tgs.end()) {
            selected_tg_prio = selected_tg_it->prio;
        }
        MonitorTgsSet::const_iterator talker_tg_it = m_monitor_tgs.find(MonitorTgEntry(msg.tg()));
        if ((talker_tg_it != m_monitor_tgs.end()) && (talker_tg_it->prio > selected_tg_prio) && !m_tg_local_activity) {
            qDebug() << "Activity on prioritized TG #" << msg.tg() << ". Switching!";
            selectTg(msg.tg(), !m_mute_first_tx_rem);
        }
    */
    //}

    // TODO process and show tg

    receiving = true;
    emit q->receiveStart(QString::fromStdString(msg.callsign()));
}

void QtSvxReflectorClientPrivate::handleMsgTalkerStop(std::istream& is) {
    Q_Q(QtSvxReflectorClient);
    MsgTalkerStop msg;
    if(!msg.unpack(is)) {
        qWarning() << "*** ERROR: Could not unpack MsgTalkerStop";
        disconnectReflector(QtSvxReflectorClient::OTHERERROR);
        return;
    }
    qDebug() << ": Talker stop on TG #" << msg.tg() << ": " << QString::fromStdString(msg.callsign());
    receiving = false;
    emit q->receiveStop(QString::fromStdString(msg.callsign()));
}

void QtSvxReflectorClientPrivate::handleMsgServerInfo(std::istream& is) {
    qDebug() << "handleMsgServerInfo";
    Q_Q(QtSvxReflectorClient);
    if(m_con_state != STATE_EXPECT_SERVER_INFO) {
        qWarning() << "*** ERROR: Unexpected MsgServerInfo";
        disconnectReflector(QtSvxReflectorClient::OTHERERROR);
        return;
    }
    MsgServerInfo msg;
    if(!msg.unpack(is)) {
        qWarning() << "*** ERROR: Could not unpack MsgServerInfo";
        disconnectReflector(QtSvxReflectorClient::OTHERERROR);
        return;
    }
    m_client_id = static_cast<uint16_t>(msg.clientId());

    const std::vector<std::string>& nodes = msg.nodes();
    QStringList list;
    if(!nodes.empty()) {
        std::vector<std::string>::const_iterator it = nodes.begin();
        for (; it != nodes.end(); ++it) {
            QString name = QString::fromStdString(*it);
            list << name;
        }
    }
    emit q->connectedUsers(list);

    std::string selected_codec;
    for (std::vector<std::string>::const_iterator it = msg.codecs().begin(); it != msg.codecs().end();++it) {
        if(*it=="OPUS") {
            selected_codec = *it;
            //qDebug() << "Using audio codec" << QString::fromStdString(selected_codec);
            break;
        }
    }
    if(selected_codec.empty()) {
        qWarning() << "No supported codec :-(";
    }

    delete udpSocket;
    udpSocket = new QUdpSocket(this);

    udpSocket->bind(reflectorPort);
    connect(udpSocket, SIGNAL(readyRead()), this, SLOT(udpDatagramReceived()));

    m_con_state = STATE_CONNECTED;
    //#################### new below

    /* TODO
    std::ostringstream node_info_os;
    Json::StreamWriterBuilder builder;
    builder["commentStyle"] = "None";
    builder["indentation"] = ""; //The JSON document is written on a single line
    Json::StreamWriter* writer = builder.newStreamWriter();
    writer->write(m_node_info, &node_info_os);
    delete writer;
    MsgNodeInfo node_info_msg(node_info_os.str());
    sendMsg(node_info_msg);
    */

    if (m_selected_tg > 0) {
        qDebug() << ": Selecting TG #" << m_selected_tg;
        sendMsg(MsgSelectTG(m_selected_tg));
        m_monitor_tgs.insert(MonitorTgEntry(m_selected_tg));
    }

    if (!m_monitor_tgs.empty()) {
        sendMsg(MsgTgMonitor(std::set<uint32_t>(m_monitor_tgs.begin(), m_monitor_tgs.end())));
    }

    sendUdpMsg(MsgUdpHeartbeat());
}

void QtSvxReflectorClientPrivate::handleMsgProtoVerDowngrade(std::istream& is) {
    MsgProtoVerDowngrade msg;
    if (!msg.unpack(is)) {
        qWarning() << "*** ERROR: Could not unpack MsgProtoVerDowngrade" << endl;
        disconnectReflector(QtSvxReflectorClient::OTHERERROR);
        return;
    }
    qDebug() << "Server too old and we cannot downgrade to protocol version "
             << msg.majorVer() << "." << msg.minorVer() << " from "
             << MsgProtoVer::MAJOR << "." << MsgProtoVer::MINOR;
    disconnectReflector(QtSvxReflectorClient::SERVERTOOOLD);
}

void QtSvxReflectorClientPrivate::handleMsgRequestQsy(std::istream& is) {
    MsgRequestQsy msg;
    if (!msg.unpack(is)) {
        qWarning() << "*** ERROR: Could not unpack MsgRequestQsy";
        disconnectReflector(QtSvxReflectorClient::OTHERERROR);
        return;
    }
    qDebug() << "Server QSY request for TG #" << msg.tg();

    /*
    if (m_tg_local_activity) {
        selectTg(msg.tg(), true);
    }
    else {
        m_last_qsy = msg.tg();
        qDebug() << "Server QSY request ignored. No local activity.";
        //TODOstd::ostringstream os;
        //os << "tg_qsy_ignored " << msg.tg();
        //processEvent(os.str());
    }
    */
}

void QtSvxReflectorClientPrivate::processTgSelectionEvent(void) {
    //if (!m_logic_con_out->isIdle() || !m_logic_con_in->isIdle() || m_tg_selection_event.empty()) {
        return;
    //}
    //TODOprocessEvent(m_tg_selection_event);
    //m_tg_selection_event.clear();
}

void QtSvxReflectorClientPrivate::tgSelectTimerExpired(void) {
    qDebug() << "### ReflectorLogic::tgSelectTimerExpired: m_tg_select_timeout_cnt=" << m_tg_select_timeout_cnt;
    if (m_tg_select_timeout_cnt > 0) {
        //TODOif (m_logic_con_out->isIdle() && m_logic_con_in->isIdle() && (--m_tg_select_timeout_cnt == 0)) {
            //selectTg(0, false);
        //}
    }
}

void QtSvxReflectorClientPrivate::checkTmpMonitorTimeout(void) {
    bool changed = false;
    MonitorTgsSet::iterator it = m_monitor_tgs.begin();
    while (it != m_monitor_tgs.end()) {
        MonitorTgsSet::iterator next=it;
        ++next;
        const MonitorTgEntry& mte = *it;
        if (mte.timeout > 0) {
            // NOTE: mte.timeout is mutable
            if (--mte.timeout <= 0) {
                qDebug() << "Temporary monitor timeout for TG #" << mte.tg;
                changed = true;
                m_monitor_tgs.erase(it);
                //TODOstd::ostringstream os;
                //os << "tmp_monitor_remove " << mte.tg;
                //processEvent(os.str());
            }
        }
        it = next;
    }
    if (changed) {
        sendMsg(MsgTgMonitor(std::set<uint32_t>(m_monitor_tgs.begin(), m_monitor_tgs.end())));
    }
}

void QtSvxReflectorClientPrivate::selectTg(uint32_t tg, bool unmute) {
    Q_UNUSED(unmute)
    qDebug() << ": Selecting TG #" << tg;

    //TODOm_report_tg_timer.reset();
    //TODOm_report_tg_timer.setEnable(true);

    if (tg != m_selected_tg) {
        sendMsg(MsgSelectTG(tg));
        if (m_selected_tg != 0) {
            m_previous_tg = m_selected_tg;
        }
        m_selected_tg = tg;
        m_tg_local_activity = false;
    }
    m_tg_select_timeout_cnt = (tg > 0) ? m_tg_select_timeout : 0;
}

void QtSvxReflectorClientPrivate::onLogicConInStreamStateChanged(bool is_active, bool is_idle) {
    qDebug() << "### ReflectorLogic::onLogicConInStreamStateChanged: is_active="<< is_active << "  is_idle=" << is_idle;
    if (is_idle) {
        //if ((m_logic_con_in_valve != 0) && (m_selected_tg > 0)) {
        //    m_logic_con_in_valve->setOpen(true);
        //}
    }
    else {
        if (m_tg_select_timeout_cnt == 0) {// No TG currently selected
            if (m_default_tg > 0) {
                selectTg(m_default_tg, !m_mute_first_tx_loc);
            }
        }
        m_tg_local_activity = true;
        m_tg_select_timeout_cnt = m_tg_select_timeout;
    }

    //if (!m_tg_selection_event.empty()) {
        //processTgSelectionEvent();
        //m_report_tg_timer.reset();
        //m_report_tg_timer.setEnable(true);
    //}
}

void QtSvxReflectorClientPrivate::onLogicConOutStreamStateChanged(bool is_active, bool is_idle) {
    qDebug() << "### ReflectorLogic::onLogicConOutStreamStateChanged: is_active=" << is_active << "  is_idle=" << is_idle;
    if (!is_idle && (m_tg_select_timeout_cnt > 0)) {
        m_tg_select_timeout_cnt = m_tg_select_timeout;
    }

    //if (!m_tg_selection_event.empty()) {
    //    //processTgSelectionEvent();
    //    m_report_tg_timer.reset();
    //    m_report_tg_timer.setEnable(true);
    //}
}

void QtSvxReflectorClientPrivate::handleTimerTick(void) {
    if(talkerTimeoutActive) {
        if(m_last_talker_timestamp.secsTo(QDateTime::currentDateTime())>3) {
            qDebug() << "Last talker audio timeout";
            //m_dec->flushEncodedSamples();
            talkerTimeoutActive = false;
        }
    }

  if(--m_udp_heartbeat_tx_cnt == 0) {
    sendUdpMsg(MsgUdpHeartbeat());
  }

  if(--m_tcp_heartbeat_tx_cnt == 0) {
    sendMsg(MsgHeartbeat());
  }

  if(--m_udp_heartbeat_rx_cnt == 0) {
    qDebug() << "UDP Heartbeat timeout";
    disconnectReflector(QtSvxReflectorClient::OTHERERROR);
  }

  if(--m_tcp_heartbeat_rx_cnt == 0) {
    qDebug() << "TCP Heartbeat timeout";
    disconnectReflector(QtSvxReflectorClient::OTHERERROR);
  }
}

void QtSvxReflectorClientPrivate::flushTimeout(void) {
    flushTimeoutTimer->stop();
    allEncodedSamplesFlushed();
}

void QtSvxReflectorClientPrivate::udpDatagramReceived() {
    QNetworkDatagram dg = udpSocket->receiveDatagram();
    QByteArray data = dg.data();

    if((tcpSocket == nullptr) || tcpSocket->state()!=QTcpSocket::ConnectedState || (m_con_state != STATE_CONNECTED)) {
        return;
    }
/*
    if(dg.senderAddress().toString() != m_con->remoteHost()) {
        qWarning() << "*** WARNING: UDP packet received from wrong source address " << addr << ". Should be " << m_con->remoteHost() << ".";
        return;
    }
    if(dg.senderPort() != m_con->remotePort()) {
        qWarning() << "*** WARNING: UDP packet received with wrong source port number " << port << ". Should be " << m_con->remotePort() << ".";
        return;
    }
*/
    std::stringstream ss;
    ss.write(reinterpret_cast<const char *>(data.data()), data.size());

    ReflectorUdpMsg header;
    if(!header.unpack(ss)) {
        qWarning() << "*** WARNING: Unpacking failed for UDP message header";
        return;
    }

    if(header.clientId() != m_client_id) {
        qWarning() << "*** WARNING: UDP packet received with wrong client id " << header.clientId() << ". Should be " << m_client_id << ".";
        return;
    }

    // Check sequence number
    uint16_t udp_rx_seq_diff = header.sequenceNum() - m_next_udp_rx_seq;
    if(udp_rx_seq_diff > 0x7fff) { // Frame out of sequence (ignore)
        qWarning() << "Dropping out of sequence UDP frame with seq=" << header.sequenceNum();
        return;
    }
    else if(udp_rx_seq_diff > 0) { // Frame lost
        qWarning() << "UDP frame(s) lost. Expected seq=" << m_next_udp_rx_seq << " but received " << header.sequenceNum() << ". Resetting next expected sequence number to " << (header.sequenceNum() + 1);
    }
    m_next_udp_rx_seq = header.sequenceNum() + 1;
    m_udp_heartbeat_rx_cnt = UDP_HEARTBEAT_RX_CNT_RESET;

    switch (header.type()) {
    case MsgUdpHeartbeat::TYPE: {
        break;
    }

    case MsgUdpAudio::TYPE: {
        if(!outputDisabled) {
            MsgUdpAudio msg;
            if(!msg.unpack(ss)) {
                qWarning() << "*** WARNING: Could not unpack MsgUdpAudio";
                return;
            }
            if(!msg.audioData().empty()) {
                m_last_talker_timestamp = QDateTime::currentDateTime();
                talkerTimeoutActive = true;
                //QByteArray data(reinterpret_cast<char*>(msg.audioData().data()), static_cast<int>(msg.audioData().size()));
                //qDebug() << data.toHex();
                decodeReceivedSamples(&msg.audioData().front(), static_cast<int>(msg.audioData().size()));
            }
        }
        break;
    }

    case MsgUdpFlushSamples::TYPE:
        qDebug() << "msgUdpFlushSamples";
        //m_dec->flushEncodedSamples();
        talkerTimeoutActive = false;
        break;

    case MsgUdpAllSamplesFlushed::TYPE:
        //qDebug() << "MsgUdpAllSamplesFlushed";
        allEncodedSamplesFlushed();
      break;

    default:
      // Better ignoring unknown protocol messages for easier addition of new
      // messages while still being backwards compatible

      //qWarning() << "*** WARNING[" << ""
      //     << "]: Unknown UDP protocol message received: msg_type="
      //     << header.type();
      break;
  }
}

void QtSvxReflectorClientPrivate::sendUdpMsg(const ReflectorUdpMsg& msg) {
    if((tcpSocket == nullptr) || tcpSocket->state()!=QTcpSocket::ConnectedState || (m_con_state != STATE_CONNECTED)) {
        return;
    }

    m_udp_heartbeat_tx_cnt = UDP_HEARTBEAT_TX_CNT_RESET;

    if(udpSocket == nullptr) {
        return;
    }

    ReflectorUdpMsg header(msg.type(), m_client_id, m_next_udp_tx_seq++);
    std::ostringstream ss;
    if(!header.pack(ss) || !msg.pack(ss)) {
        qWarning() << "*** ERROR: Failed to pack reflector TCP message";
        return;
    }
    udpSocket->writeDatagram(ss.str().data(), static_cast<qint64>(ss.str().size()), QHostAddress(reflectorHost), reflectorPort);
}

void QtSvxReflectorClientPrivate::flushEncodedAudio(void) {
    if((tcpSocket == nullptr) || tcpSocket->state()!=QTcpSocket::ConnectedState) {
        flushTimeout();
        return;
    }
    sendUdpMsg(MsgUdpFlushSamples());
    flushTimeoutTimer->start();
}

void QtSvxReflectorClientPrivate::allEncodedSamplesFlushed(void) {
    sendUdpMsg(MsgUdpAllSamplesFlushed());
}

void QtSvxReflectorClientPrivate::decodeReceivedSamples(void *buf, int size) {
    unsigned char *packet = reinterpret_cast<unsigned char *>(buf);

    int frame_cnt = opus_packet_get_nb_frames(packet, size);
    if (frame_cnt == 0) {
        return;
    }
    else if (frame_cnt < 0) {
        qWarning() << "*** ERROR: Opus decoder error: " << opus_strerror(dec_frame_size);
        return;
    }
    dec_frame_size = opus_packet_get_samples_per_frame(packet, audioRate);
    if (dec_frame_size == 0) {
        return;
    }
    else if (dec_frame_size < 0) {
        qWarning() << "*** ERROR: Opus decoder error: " << opus_strerror(dec_frame_size);
        return;
    }
    int channels = opus_packet_get_nb_channels(packet);
    if (channels <= 0) {
        qWarning() << "*** ERROR: Opus decoder error: " << opus_strerror(channels);
        return;
    }
    else if (channels != 1) {
        qWarning() << "*** ERROR: Multi channel Opus packet received but only one channel can be handled";
        return;
    }
    float samples[5760];
    dec_frame_size = opus_decode_float(opusdec, packet, size, samples, 5760, 0);
    if (dec_frame_size > 0) {
        const char *data = reinterpret_cast<const char*>(samples);
        audioOutputDevice->write(data, dec_frame_size*4);
    }
    else if (dec_frame_size < 0) {
        qWarning() << "**** ERROR: Opus decoder error: " << opus_strerror(dec_frame_size);
    }
}

void QtSvxReflectorClientPrivate::encodeTransmitSamples() {
    char *audioData = new char[static_cast<quint64>(audioInput->bufferSize())];
    qint64 count = audioInputDevice->read(audioData, audioInput->bufferSize());
    const float *audioFloatData = reinterpret_cast<const float*>(audioData);
    count /= 4;
    inputBufferLen = 0;
    for(qint64 i=0;i<count;i++) {
        inputSampleBuffer[inputBufferLen++] = audioFloatData[i];
        if(inputBufferLen==inputFrameSize) {
            inputBufferLen = 0;
            unsigned char output_buf[4000];
            opus_int32 nbytes = opus_encode_float(opusenc, inputSampleBuffer, static_cast<int>(inputFrameSize), output_buf, sizeof(output_buf));
            if(nbytes > 0) {
                if((tcpSocket == nullptr) || tcpSocket->state()!=QTcpSocket::ConnectedState) {
                    return;
                }
                if(flushTimeoutTimer->isActive()) {
                    flushTimeoutTimer->stop();
                }
                sendUdpMsg(MsgUdpAudio(output_buf, nbytes));
            }
            else if(nbytes < 0) {
                qWarning() << "**** ERROR: Opus encoder error: " << opus_strerror(static_cast<int>(inputFrameSize));
            }
        }
    }
    delete[] audioData;
}
