#include <sstream>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <iterator>
#include <functional>
#include <AsyncTcpClient.h>
#include <AsyncUdpSocket.h>
//#include <AsyncAudioPassthrough.h>

#include "ReflectorLogic.h"
#include "ReflectorMsg.h"
using namespace std;
using namespace Async;

ReflectorLogic::~ReflectorLogic(void)
{
  delete m_logic_con_in;
  m_logic_con_in = nullptr;
  delete m_enc;
  m_enc = nullptr;
  delete m_dec;
  m_dec = nullptr;
}

bool ReflectorLogic::initialize(void)
{
    // Create logic connection incoming audio passthrough
  m_logic_con_in = new Async::AudioPassthrough;

    // Create dummy audio codec used before setting the real encoder
  if (!setAudioCodec("DUMMY")) { return false; }
  AudioSource *prev_src = m_dec;

    // Create jitter FIFO if jitter buffer delay > 0
  unsigned jitter_buffer_delay = 0;
  jitter_buffer_delay = 0;
  if (jitter_buffer_delay > 0)
  {
 /*   AudioFifo *fifo = new Async::AudioFifo(
        2 * jitter_buffer_delay * INTERNAL_SAMPLE_RATE / 1000);
        //new Async::AudioJitterFifo(100 * INTERNAL_SAMPLE_RATE / 1000);
    fifo->setPrebufSamples(jitter_buffer_delay * INTERNAL_SAMPLE_RATE / 1000);
    prev_src->registerSink(fifo, true);
    prev_src = fifo;*/
  }
  else
  {
    AudioPassthrough *passthrough = new AudioPassthrough;
    prev_src->registerSink(passthrough, true);
    prev_src = passthrough;
  }
  m_logic_con_out = prev_src;

  connect();

  return true;
}

bool ReflectorLogic::setAudioCodec(const std::string& codec_name)
{
  delete m_enc;
  m_enc = Async::AudioEncoder::create(codec_name);
  if (m_enc == 0)
  {
    cerr << "*** ERROR[" << ""
         << "]: Failed to initialize " << codec_name
         << " audio encoder" << endl;
    m_enc = Async::AudioEncoder::create("DUMMY");
    assert(m_enc != 0);
    return false;
  }
  m_enc->writeEncodedSamples.connect(
      mem_fun(*this, &ReflectorLogic::sendEncodedAudio));
  m_enc->flushEncodedSamples.connect(
      mem_fun(*this, &ReflectorLogic::flushEncodedAudio));
  m_logic_con_in->registerSink(m_enc, false);

  AudioSink *sink = nullptr;
  if (m_dec != nullptr)
  {
    sink = m_dec->sink();
    m_dec->unregisterSink();
    delete m_dec;
  }
  m_dec = Async::AudioDecoder::create(codec_name);
  if (m_dec == 0)
  {
    cerr << "*** ERROR[" << ""
         << "]: Failed to initialize " << codec_name
         << " audio decoder" << endl;
    m_dec = Async::AudioDecoder::create("DUMMY");
    assert(m_dec != 0);
    return false;
  }
  m_dec->allEncodedSamplesFlushed.connect(
      mem_fun(*this, &ReflectorLogic::allEncodedSamplesFlushed));
  if (sink != 0)
  {
    m_dec->registerSink(sink, true);
  }

  return true;
}

bool ReflectorLogic::codecIsAvailable(const std::string &codec_name)
{
  return AudioEncoder::isAvailable(codec_name) &&
         AudioDecoder::isAvailable(codec_name);
}
