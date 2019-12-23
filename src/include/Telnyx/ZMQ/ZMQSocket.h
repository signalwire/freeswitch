// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.

#ifndef TELNYX_ZMQSOCKET_H_INCLUDED
#define TELNYX_ZMQSOCKET_H_INCLUDED


#include "Telnyx/Telnyx.h"
#include "Telnyx/UTL/CoreUtils.h"
#include "Telnyx/UTL/Thread.h"
#include "Telnyx/ZMQ/zmq.hpp"

namespace Telnyx {
namespace ZMQ {
  
  
class ZMQSocket : boost::noncopyable
{
public:
  enum SocketType
  {
    REQ,
    REP,
    PUSH,
    PULL,
    PUB,
    SUB
  };
  
  typedef zmq::pollitem_t PollItem;
  typedef std::vector<PollItem> PollItems;
  
  ZMQSocket(SocketType type, zmq::context_t* pContext = 0);
  
  ~ZMQSocket();
  
  bool connect(const std::string& peerAddress);
  
  bool bind(const std::string& bindAddress);
  
  bool subscribe(const std::string& event);
  
  bool publish(const std::string& event);
  
  bool sendAndReceive(const std::string& cmd, const std::string& data, std::string& response, unsigned int timeoutms);
  bool sendAndReceive(const std::string& cmd, const std::string& data, std::string& response);
  
  bool sendAndReceive(const std::string& data, std::string& response, unsigned int timeoutms);
  bool sendAndReceive(const std::string& data, std::string& response);

  bool sendRequest(const std::string& cmd, const std::string& data);
  bool sendRequest(const std::string& data);
  
  bool sendReply(const std::string& data);
  
  bool receiveReply(std::string& reply, unsigned int timeoutms);
  bool receiveReply(std::string& reply);
  
  bool receiveRequest(std::string& cmd, std::string& data, unsigned int timeoutms);
  bool receiveRequest(std::string& cmd, std::string& data);
  
  bool receiveRequest(std::string& data);
  bool receiveRequest(std::string& data, unsigned int timeoutms);
  
  void close();
  
  zmq::socket_t* socket();
  
  static int poll(ZMQSocket::PollItems& pollItems, long timeoutMiliseconds);
  
  void initPollItem(zmq_pollitem_t& item);
  
  bool pollRead(long timeoutms = 0);
  
  int getFd() const;
  
  const std::string& getBindAddress() const;
  
  const std::string& getConnectAddress() const;
protected:
  void internal_close();
  bool internal_connect(const std::string& peerAddress);
  bool internal_send_reply(const std::string& data);
  bool internal_send_request(const std::string& cmd, const std::string& data);
  bool internal_receive_reply(std::string& reply, unsigned int timeoutms);
  bool internal_receive_request(std::string& cmd, std::string& data, unsigned int timeoutms);
  bool internal_receive_request(std::string& data, unsigned int timeoutms);
  SocketType _type;
  zmq::context_t* _context;
  zmq::socket_t* _socket;
  std::string _peerAddress;
  std::string _bindAddress;
  Telnyx::mutex_critic_sec _mutex;
  bool _canReconnect;
  bool _isInproc;
  bool _isExternalContext;
  static zmq::context_t* _inproc_context;
};

//
// Inlines
//

inline bool ZMQSocket::sendAndReceive(const std::string& cmd, const std::string& data, std::string& response)
{
  return sendAndReceive(cmd, data, response, 0);
}

inline bool ZMQSocket::sendAndReceive(const std::string& data, std::string& response)
{
  return sendAndReceive(data, response, 0);
}

inline bool ZMQSocket::receiveRequest(std::string& cmd, std::string& data)
{
  return receiveRequest(cmd, data, 0);
}

inline bool ZMQSocket::receiveRequest(std::string& data)
{
  return receiveRequest(data, 0);
}

inline bool ZMQSocket::receiveReply(std::string& reply)
{
  return receiveReply(reply, 0);
}

inline zmq::socket_t* ZMQSocket::socket()
{
  return _socket;
}

inline const std::string& ZMQSocket::getBindAddress() const
{
  return _bindAddress;
}
  
inline const std::string& ZMQSocket::getConnectAddress() const
{
  return _peerAddress;
}

} } // Telnyx::ZMQ


#endif // TELNYX_ZMQSOCKET_H_INCLUDED

