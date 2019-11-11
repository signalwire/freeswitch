// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.



#include "Telnyx/ZMQ/ZMQSocket.h"
#include "Telnyx/UTL/Logger.h"



namespace Telnyx {
namespace ZMQ {
  
zmq::context_t* ZMQSocket::_inproc_context = new zmq::context_t(1);
  
static void zeromq_free (void *data, void *hint)
{
  free (data);
}
//  Convert string to 0MQ string and send to socket
static bool zeromq_send (zmq::socket_t & socket, const std::string & data)
{
  char * buff = (char*)malloc(data.size());
  memcpy(buff, data.data(), data.size());
  zmq::message_t message((void*)buff, data.size(), zeromq_free, 0);
  bool rc = socket.send(message);
  return (rc);
}

//  Sends string as 0MQ string, as multipart non-terminal
static bool zeromq_sendmore (zmq::socket_t & socket, const std::string & data)
{
  char * buff = (char*)malloc(data.size());
  memcpy(buff, data.data(), data.size());
  zmq::message_t message((void*)buff, data.size(), zeromq_free, 0);
  bool rc = socket.send(message, ZMQ_SNDMORE);
  return (rc);
}

static void zeromq_receive (zmq::socket_t& socket, std::string& value)
{
  zmq::message_t message;
  socket.recv(&message);
  if (!message.size())
    return;
  value = std::string(static_cast<char*>(message.data()), message.size());
} 

static zmq::socket_t* zeromq_create_socket(zmq::context_t* context, int type)
{
  zmq::socket_t* socket = new zmq::socket_t(*context, type);
  int linger = 0;
  socket->setsockopt (ZMQ_LINGER, &linger, sizeof (linger));
  return socket;
}

static bool zeromq_poll_read(zmq::socket_t* sock, int timeoutms)
{
  
  zmq::pollitem_t items[] = { { *sock, 0, ZMQ_POLLIN, 0 } };

#if ZMQ_VERSION_MAJOR < 4
  int timeoutnano = timeoutms * 1000; // convert to nanoseconds
  int rc = zmq::poll (&items[0], 1, timeoutnano);
#else
  int rc = zmq::poll (&items[0], 1, timeoutms);
#endif

  
  if (rc < 0)
  {
    return false;
  }
  
  
  switch (rc)
  {
    case ETERM:
      //TELNYX_LOG_ERROR("zeromq_poll_read - At least one of the members of the items array refers to a socket whose associated ØMQ context was terminated.");
      return false;
    case EFAULT:
      //TELNYX_LOG_ERROR("zeromq_poll_read - The provided items was not valid (NULL).");
      return false;
    case EINTR:
      //TELNYX_LOG_ERROR("zeromq_poll_read - The operation was interrupted by delivery of a signal before any events were available.");
      return false;
  }
  
  return items[0].revents & ZMQ_POLLIN;
}
  
ZMQSocket::ZMQSocket(SocketType type, zmq::context_t* pContext) :
  _type(type),
  _context(0),
  _socket(0),
  _canReconnect(false),
  _isInproc(false),
  _isExternalContext(false)
{
  if (pContext)
  {
    _isExternalContext = true;
    _context = pContext;
  }
  else  
  {
    _context = new zmq::context_t(1);
  }
}
  
ZMQSocket::~ZMQSocket()
{
  close();
  if (!_isExternalContext)
  {
    delete _context;
  }
}

bool ZMQSocket::bind(const std::string& bindAddress)
{
  Telnyx::mutex_critic_sec_lock lock(_mutex);
  
  if ( _socket || !_context)
  {
    TELNYX_LOG_ERROR("Socket is already set while calling bind");
    return false;
  }
  
  //
  // inproc transport is not pollable
  //
  _isInproc = Telnyx::string_starts_with(bindAddress, "inproc");
  
  switch(_type)
  {
  case REP:
    if (!_isExternalContext && _isInproc)
    {
      _socket = zeromq_create_socket(_inproc_context, ZMQ_REP);
    }
    else
    {
      _socket = zeromq_create_socket(_context, ZMQ_REP);
    }
    break;
  case PULL:
    if (!_isExternalContext && _isInproc)
    {
      _socket = zeromq_create_socket(_inproc_context, ZMQ_PULL);
    }
    else
    {
      _socket = zeromq_create_socket(_context, ZMQ_PULL);
    }
    break;
  case PUB:
    if (!_isExternalContext && _isInproc)
    {
      _socket = zeromq_create_socket(_inproc_context, ZMQ_PUB);
    }
    else
    {
      _socket = zeromq_create_socket(_context, ZMQ_PUB);
    }
    break;
  default:
    TELNYX_LOG_ERROR("Calling bind on incompatible socket");
    return false;
  }
  
  if (!_socket)
  {
    return false;
  }
  
  int linger = 0;
  _socket->setsockopt(ZMQ_LINGER, &linger, sizeof (linger));

  try
  {
    _socket->bind(bindAddress.c_str());
  }
  catch(zmq::error_t& error_)
  {
    TELNYX_LOG_ERROR("ZMQSocket::bind() - ZMQ Exception:  " << error_.what());
    internal_close();
    return false;
  }
  catch(const std::exception& e)
  {
    TELNYX_LOG_ERROR("ZMQSocket::bind() - ZMQ Exception: " << e.what());
    internal_close();
    return false;
  }
  catch(...)
  {
    TELNYX_LOG_ERROR("ZMQSocket::bind() - Exception: ZMQ Unknown Exception");
    internal_close();
    return false;
  }

  _bindAddress = bindAddress;
  
  return true;
}

bool ZMQSocket::connect(const std::string& peerAddress)
{
  Telnyx::mutex_critic_sec_lock lock(_mutex);
  return internal_connect(peerAddress);
}

bool ZMQSocket::internal_connect(const std::string& peerAddress)
{
  if (_socket || peerAddress.empty())
  {
    return false;
  }
  
  //
  // inproc transport is not pollable
  //
  _isInproc = Telnyx::string_starts_with(peerAddress, "inproc");
  
  switch(_type)
  {
  case REQ:
    if (!_isExternalContext && _isInproc)
    {
      _socket = zeromq_create_socket(_inproc_context, ZMQ_REQ);
    }
    else
    {
      _socket = zeromq_create_socket(_context, ZMQ_REQ);
    }
    break;
  case PUSH:
    if (!_isExternalContext && _isInproc)
    {
      _socket = zeromq_create_socket(_inproc_context, ZMQ_PUSH);
    }
    else
    {
      _socket = zeromq_create_socket(_context, ZMQ_PUSH);
    }
    break;
  case SUB:
    if (!_isExternalContext && _isInproc)
    {
      _socket = zeromq_create_socket(_inproc_context, ZMQ_SUB);
    }
    else
    {
      _socket = zeromq_create_socket(_context, ZMQ_SUB);
    }
    break;
  default:
    TELNYX_LOG_ERROR("Calling connect on incompatible socket");
    return false;
  }
  
  if (!_socket)
  {
    return false;
  }
  
  try
  {
    _socket->connect(peerAddress.c_str());
  }
  catch(zmq::error_t& error_)
  {
    TELNYX_LOG_ERROR("ZMQSocket::internal_connect() - ZMQ Exception:  " << error_.what());
    internal_close();
    return false;
  }
  catch(const std::exception& e)
  {
    TELNYX_LOG_ERROR("ZMQSocket::internal_connect() - ZMQ Exception: " << e.what());
    internal_close();
    return false;
  }
  catch(...)
  {
    TELNYX_LOG_ERROR("ZMQSocket::internal_connect() - Exception: ZMQ Unknown Exception");
    internal_close();
    return false;
  }
  _peerAddress = peerAddress;
  return true;
}

bool ZMQSocket::subscribe(const std::string& event)
{
  if (_type != SUB || !_socket)
  {
    return false;
  }
  _socket->setsockopt(ZMQ_SUBSCRIBE, event.c_str(), event.length());
  return true;
}

bool ZMQSocket::publish(const std::string& event)
{
  if (_type != PUB)
  {
    return false;
  }
  return sendRequest(event);
}

void ZMQSocket::close()
{
  Telnyx::mutex_critic_sec_lock lock(_mutex);
  _canReconnect = false;
  internal_close();
}

void ZMQSocket::internal_close()
{
  delete _socket;
  _socket = 0;
  
  if (!_canReconnect)
  {
    _peerAddress = "";
  }
}

bool ZMQSocket::sendAndReceive(const std::string& cmd, const std::string& data, std::string& response, unsigned int timeoutms)
{
  if (_type == PUSH)
  {
    return false;
  }
  
  Telnyx::mutex_critic_sec_lock lock(_mutex);
  
  if (!internal_send_request(cmd, data))
  {
    return false;
  }
  
  return internal_receive_reply(response, timeoutms);
}

bool ZMQSocket::sendAndReceive(const std::string& data, std::string& response, unsigned int timeoutms)
{
  if (_type == PUSH)
  {
    return false;
  }
  
  Telnyx::mutex_critic_sec_lock lock(_mutex);
  
  if (!internal_send_request("", data))
  {
    return false;
  }
  
  return internal_receive_reply(response, timeoutms);
}

bool ZMQSocket::sendRequest(const std::string& data)
{
  Telnyx::mutex_critic_sec_lock lock(_mutex);
  return internal_send_request("", data);
}

bool ZMQSocket::sendRequest(const std::string& cmd, const std::string& data)
{
  Telnyx::mutex_critic_sec_lock lock(_mutex);
  return internal_send_request(cmd, data);
}

bool ZMQSocket::internal_send_request(const std::string& cmd, const std::string& data)
{  
  //
  // reconnect the socket 
  //
  if (!_socket && _peerAddress.empty())
  {
    return false;
  }
  else if (!_socket && _canReconnect && !internal_connect(_peerAddress))
  {
    return false;
  }
  
  if (!cmd.empty() && !zeromq_sendmore(*_socket, cmd))
  {
    TELNYX_LOG_ERROR("ZMQSocket::send() - Exception: zeromq_sendmore(cmd) failed");
    _canReconnect = true;
    internal_close();    
    return false;
  }
  
  if (!zeromq_send(*_socket, data))
  {
    TELNYX_LOG_ERROR("ZMQSocket::send() - Exception: zeromq_send(data) failed");
    _canReconnect = true;
    internal_close();
    return false;
  }
  return true;
}

bool ZMQSocket::sendReply(const std::string& data)
{
  Telnyx::mutex_critic_sec_lock lock(_mutex);
  return internal_send_reply(data);
}


bool ZMQSocket::internal_send_reply(const std::string& data)
{  
  if (!_socket || !zeromq_send(*_socket, data))
  {
    TELNYX_LOG_ERROR("ZMQSocket::send() - Exception: zeromq_send(data) failed");
    return false;
  }
  return true;
}

bool ZMQSocket::receiveReply(std::string& data, unsigned int timeoutms)
{  
  Telnyx::mutex_critic_sec_lock lock(_mutex);
  return internal_receive_reply(data, timeoutms);
}

 
bool ZMQSocket::internal_receive_reply(std::string& response, unsigned int timeoutms)
{
  if (!_socket)
  {
    return false;
  }

  if (timeoutms && !_isInproc && !zeromq_poll_read(_socket, timeoutms))
  {
    //TELNYX_LOG_ERROR("ZMQSocket::internal_receive() - Exception: zeromq_poll_read() failed");
    _canReconnect = true;
    internal_close();
    return false;
  }
  
  try
  {
    zeromq_receive(*_socket, response);  
  }
  catch(std::exception& e)
  {
    _canReconnect = true;
    internal_close();
    return false;
  }
  return true;
}

bool ZMQSocket::receiveRequest(std::string& cmd, std::string& data, unsigned int timeoutms)
{
  if (_type == PUSH)
  {
    return false;
  }
  
  Telnyx::mutex_critic_sec_lock lock(_mutex);
  return internal_receive_request(cmd, data, timeoutms);
}

bool ZMQSocket::internal_receive_request(std::string& cmd, std::string& data, unsigned int timeoutms)
{
  if (!_socket)
  {
    return false;
  }
  
  if (timeoutms && !zeromq_poll_read(_socket, timeoutms))
  {
    // TELNYX_LOG_ERROR("ZMQSocket::internal_receive() - Exception: zeromq_poll_read() failed");
    return false;
  }
  
  zeromq_receive(*_socket, cmd); 
  zeromq_receive(*_socket, data);  
  return true;
}

bool ZMQSocket::receiveRequest(std::string& data, unsigned int timeoutms)
{
  if (_type == PUSH)
  {
    return false;
  }
  
  Telnyx::mutex_critic_sec_lock lock(_mutex);
  return internal_receive_request(data, timeoutms);
}

bool ZMQSocket::internal_receive_request(std::string& data, unsigned int timeoutms)
{
  if (!_socket)
  {
    return false;
  }
  
  if (timeoutms && !zeromq_poll_read(_socket, timeoutms))
  {
    return false;
  }
  
  zeromq_receive(*_socket, data);  
  return true;
}

int ZMQSocket::poll(ZMQSocket::PollItems& pollItems, long timeoutms)
{
  int timeout = timeoutms;
#if ZMQ_VERSION_MAJOR < 4
  if (timeoutms > 0)
  {
    timeout = timeoutms * 1000; // convert to nanoseconds
  }
#endif  
  return zmq::poll(pollItems.data(), pollItems.size(), timeout);
}

int ZMQSocket::getFd() const
{
  if (!_socket)
  {
    return 0;
  }
  int fd = 0;
  size_t fd_len = sizeof(fd);
  _socket->getsockopt(ZMQ_FD, &fd, &fd_len);
  return fd;
}

void ZMQSocket::initPollItem(zmq_pollitem_t& item)
{
  if (_socket)
  {
    item.socket = _socket->get();
    item.events = ZMQ_POLLIN;
    item.revents = 0;
    item.fd = 0;
  }
}

bool ZMQSocket::pollRead(long timeoutms)
{
  if (!_socket)
  {
    return false;
  }
  return zeromq_poll_read(_socket, timeoutms);
}


} } // Telnyx::ZMQ
