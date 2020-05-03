
#ifndef REFLECTOR_LOGIC_INCLUDED
#define REFLECTOR_LOGIC_INCLUDED

#define INTERNAL_SAMPLE_RATE 48000

#include <sys/time.h>
#include <string>
#include <AsyncAudioDecoder.h>
#include <AsyncAudioEncoder.h>
#include <AsyncTcpClient.h>
#include <AsyncFramedTcpConnection.h>
#include <AsyncTimer.h>
#include <AsyncAudioFifo.h>
#include <AsyncAudioPassthrough.h>

//#include "LogicBase.h"

namespace Async
{
  class UdpSocket;
};

class ReflectorMsg;
class ReflectorUdpMsg;

class ReflectorLogic
{
  public:
    ~ReflectorLogic(void);

    virtual bool initialize(void);

    /**
     * @brief 	Get the audio pipe sink used for writing audio into this logic
     * @return	Returns an audio pipe sink object
     */
    virtual Async::AudioSink *logicConIn(void) { return m_logic_con_in; }

    /**
     * @brief 	Get the audio pipe source used for reading audio from this logic
     * @return	Returns an audio pipe source object
     */
    virtual Async::AudioSource *logicConOut(void) { return m_logic_con_out; }

  private:
    typedef Async::TcpClient<Async::FramedTcpConnection> FramedTcpClient;

    unsigned                  m_msg_type;
    Async::AudioPassthrough*  m_logic_con_in;
    Async::AudioSource*       m_logic_con_out;
    Async::AudioDecoder*      m_dec;
    Async::AudioEncoder*      m_enc;

    ReflectorLogic(const ReflectorLogic&);
    ReflectorLogic& operator=(const ReflectorLogic&);
    bool setAudioCodec(const std::string& codec_name);
    bool codecIsAvailable(const std::string &codec_name);

};

#endif /* REFLECTOR_LOGIC_INCLUDED */
