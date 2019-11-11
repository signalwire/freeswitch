// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.

#ifndef TELNYX_IPCQUEUE_H
#define	TELNYX_IPCQUEUE_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string>
#include <fstream>
#include <cassert>
#include <cstring>
#include <iostream>

#include <boost/thread.hpp>
#include <boost/noncopyable.hpp>

#include "Telnyx/JSON/reader.h"
#include "Telnyx/JSON/writer.h"
#include "Telnyx/JSON/elements.h"

namespace Telnyx {

struct IPCMessage
{
  long datatype;
  char data[8000];
  size_t dataSize;
  size_t size(){return sizeof(struct IPCMessage) - sizeof(long);}
};

template <typename Message>
class IPCQueueReader
{
public:
  IPCQueueReader(const std::string& queueName) :
    _queueName(queueName),
    _open(false),
    _qid(-1)
  {
    std::ofstream dummy(queueName.c_str());

    _key = ftok(_queueName.c_str(), 'B');
    if (_key != -1)
    {
      _qid = msgget(_key, 0666 | IPC_CREAT);
      if (_qid != -1)
      {
        msgctl(_qid, IPC_RMID, 0);
        _qid = msgget(_key, 0666 | IPC_CREAT);
        if (_qid != -1)
          _open = true;
      }
    }
  }

  IPCQueueReader(key_t key) :
    _key(key),
    _open(false),
    _qid(-1)
  {
    if (_key != -1)
    {
      _qid = msgget(_key, 0666 | IPC_CREAT);
      if (_qid != -1)
      {
        _open = true;
      }
    }
  }

  ~IPCQueueReader()
  {
    //
    // close the queue
    //
    close();
  }

  void close()
  {
    if (_open)
      msgctl(_qid, IPC_RMID, 0);
    _open = false;
  }
  
  bool read(Message& message, bool blocking = true)
  {
    if (!_open)
      return false;

    int flag = blocking ? 0 : IPC_NOWAIT;

    if (msgrcv(_qid, &message, message.size(), 0, flag) == -1)
      return false;
    return true;
  }

  const std::string& getQueueName() const { return _queueName; };
  const key_t& getKey() const { return _key; };
  const int& getId() const { return _qid; };
  bool isOpen() const { return _open; }
protected:
  std::string _queueName;
  key_t _key;
  bool _open;
  int _qid;
};

template <typename Message>
class IPCQueueWriter
{
public:
  IPCQueueWriter(const std::string& queueName) :
    _queueName(queueName),
    _open(false),
    _qid(-1)
  {
    std::ofstream dummy(queueName.c_str());
    _key = ftok(_queueName.c_str(), 'B');
    if (_key != -1)
    {
      _qid = msgget(_key, 0666);
      if (_qid != -1)
      {
        _open = true;
      }
    }
  }

  IPCQueueWriter(key_t key) :
    _key(key),
    _open(false),
    _qid(-1)
  {
    if (_key != -1)
    {
      _qid = msgget(_key, 0666);
      if (_qid != -1)
      {
        _open = true;
      }
    }
  }

  ~IPCQueueWriter()
  {
    close();
  }

  void close()
  {
    if (_open)
      msgctl(_qid, IPC_RMID, 0);
    _open = false;
  }
  
  bool write(Message& message, bool blocking = true)
  {
    if (!_open)
    {
      std::ofstream dummy(_queueName.c_str());
      _key = ftok(_queueName.c_str(), 'B');
      if (_key != -1)
      {
        _qid = msgget(_key, 0666);
        if (_qid != -1)
        {
          _open = true;
        }
        else
        {
          return false;
        }
      }
      else
      {
        return false;
      }
    }

    int flag = blocking ? 0 : IPC_NOWAIT;
    if (msgsnd(_qid, &message, message.size(), flag) == -1)
      return false;
    return true;
  }

  const std::string& getQueueName() const { return _queueName; };
  const key_t& getKey() const { return _key; };
  const int& getId() const { return _qid; };
  bool isOpen() const { return _open; }
protected:
  std::string _queueName;
  key_t _key;
  bool _open;
  int _qid;
};

typedef IPCQueueReader<IPCMessage> IPCQueueStringReader;
typedef IPCQueueWriter<IPCMessage> IPCQueueStringWriter;

class IPCQueue
{
public:
  enum Type
  {
    READER,
    WRITER,
    READWRITE
  };

  IPCQueue(const std::string& fileName, Type type) :
    _type(type),
    _key(-1),
    _fileName(fileName),
    _qid(-1),
    _pReader(0),
    _pWriter(0)
  {
    if (_type == READER)
    {
      _pReader = new IPCQueueStringReader(fileName);
      _open = _pReader->isOpen();
      _key = _pReader->getKey();
    }
    else if (_type == WRITER)
    {
      _pWriter = new IPCQueueStringWriter(fileName);
      _open = _pWriter->isOpen();
      _key = _pWriter->getKey();
    }
    else if (_type == READWRITE)
    {
      _pReader = new IPCQueueStringReader(fileName);
      _pWriter = new IPCQueueStringWriter(fileName);
      _open = _pReader->isOpen() && _pWriter->isOpen();
      if (_open)
        assert(_pReader->getId() == _pWriter->getId());
      _key = _pReader->getKey();
    }
  }

  IPCQueue(key_t key, Type type) :
    _type(type),
    _key(key),
    _qid(-1),
    _pReader(0),
    _pWriter(0)
  {
    if (_type == READER)
    {
      _pReader = new IPCQueueStringReader(_key);
      _open = _pReader->isOpen();
    }
    else if (_type == WRITER)
    {
      _pWriter = new IPCQueueStringWriter(_key);
      _open = _pWriter->isOpen();
    }
    else if (_type == READWRITE)
    {
      _pReader = new IPCQueueStringReader(_key);
      _pWriter = new IPCQueueStringWriter(_key);
      _open = _pReader->isOpen() && _pWriter->isOpen();
      if (_open)
        assert(_pReader->getId() == _pWriter->getId());
      _key = _pReader->getKey();
    }
  }

  ~IPCQueue()
  {
    close();
    delete _pReader;
    delete _pWriter;
  }
  
  void close()
  {
    if (_pReader)
      _pReader->close();
    
    if (_pWriter)
      _pWriter->close();
      
  }

  bool write(const std::string& buff, bool blocking  = true)
  {
    assert((_type == WRITER || _type == READWRITE)  && _pWriter);
    IPCMessage msg;

    if (buff.size() > msg.size())
      return false;
    msg.dataSize = buff.size();
    msg.datatype = 1;
    ::memccpy(msg.data, buff.data(), '\0', buff.size());
    return _pWriter->write(msg, blocking);
  }

  bool read(std::string& buff, bool blocking = true)
  {
    assert((_type == READER || _type == READWRITE) && _pReader);
    IPCMessage msg;
    if  (_pReader->read(msg, blocking) && msg.dataSize > 0)
    {
      buff = std::string(msg.data, msg.dataSize);
      return true;
    }
    return false;
  }

  void clear()
  {
    if ((_type == READER || _type == READWRITE) && _pReader)
    {
      int qid = _pReader->getId();
      if (qid != -1)
      {
        IPCMessage message;
        while (msgrcv(_qid, &message, message.size(), 0, IPC_NOWAIT) != -1);
      }
    }
  }

  Type getType() const { return _type; }
  key_t getKey() const { return _key; }
  const std::string& getFileName() const { return _fileName; }
  bool isOpen() const { return _open; };
protected:
  Type _type;
  key_t _key;
  bool _open;
  std::string _fileName;
  int _qid;
  IPCQueueStringReader* _pReader;
  IPCQueueStringWriter* _pWriter;
};

class IPCJsonQueue : public IPCQueue
{
public:
  IPCJsonQueue(const std::string fileName, Type type) :
    IPCQueue(fileName, type)
  {
  }

  IPCJsonQueue(key_t key, Type type) :
    IPCQueue(key, type)
  {
  }

  bool read(json::Object& params, bool blocking = true)
  {
    std::string buff;
    if (IPCQueue::read(buff, blocking))
    {
       try
      {
        std::stringstream strm;
        strm << buff;
        json::Reader::Read(params, strm);
        return true;
      }
      catch(std::exception& error)
      {
        return false;
      }
    }
    return false;
  }

  bool write(const json::Object& params, bool blocking  = true)
  {
    try
    {
      std::ostringstream strm;
      json::Writer::Write(params, strm);
      std::string buff = strm.str();
      return IPCQueue::write(buff, blocking);
    }
    catch(std::exception& error)
    {
      return false;
    }
  }

  //
  // JSON helpers
  //
  static bool getParameter(const json::Object& params, const std::string& name, std::string& value) 
  {
    json::Object::const_iterator iter = params.Find(name);
    if (iter != params.End())
    {
      value = static_cast<const std::string&>(static_cast<const json::String&>(iter->element));
      return true;
    }
    return false;
  }

  static bool getParameter(const json::Object& params, const std::string& name, double& value) 
  {
    json::Object::const_iterator iter = params.Find(name);
    if (iter != params.End())
    {
      value = static_cast<const double&>(static_cast<const json::Number&>(iter->element));
      return true;
    }
    return false;
  }

  static bool getParameter(const json::Object& params, const std::string& name, bool& value) 
  {
    json::Object::const_iterator iter = params.Find(name);
    if (iter != params.End())
    {
      value = static_cast<const bool&>(static_cast<const json::Boolean&>(iter->element));
      return true;
    }
    return false;
  }

  static bool getParameter(const json::Object& params, const std::string& name, json::Object& value) 
  {
    json::Object::const_iterator iter = params.Find(name);
    if (iter != params.End())
    {
      value = static_cast<const json::Object&>(iter->element);
      return true;
    }
    return false;
  }

  static bool getParameter(const json::Object& params, const std::string& name, json::Array& value) 
  {
    json::Object::const_iterator iter = params.Find(name);
    if (iter != params.End())
    {
      value = static_cast<const json::Array&>(iter->element);
      return true;
    }
    return false;
  }

  static void setParameter(json::Object& params, const std::string& name, const std::string& value)
  {
    params[name] = json::String(value);
  }

  static void setParameter(json::Object& params, const std::string& name, const double& value)
  {
    params[name] = json::Number(value);
  }

  static void setParameter(json::Object& params, const std::string& name, const bool& value)
  {
    params[name] = json::Boolean(value);
  }

  static void setParameter(json::Object& params, const std::string& name, const json::Object& value)
  {
    params[name] = value;
  }

  static void setParameter(json::Object& params, const std::string& name, const json::Array& value)
  {
    params[name] = value;
  }

};


class IPCJsonBidirectionalQueue : boost::noncopyable
{
public:
  IPCJsonBidirectionalQueue(const std::string& readQueuePath,
                     const std::string& writeQueuePath) :
    _reader(readQueuePath, IPCQueue::READER),
    _writer(writeQueuePath, IPCQueue::WRITER)
  {
    _pThread = new boost::thread(boost::bind(&IPCJsonBidirectionalQueue::receiveIPCMessage, this));
  }

  virtual ~IPCJsonBidirectionalQueue()
  {
    _reader.close();
    _writer.close();
    _pThread->join();
    delete _pThread;
  }

  
  virtual void onReceivedIPCMessage(const json::Object& params) = 0;
  
  bool sendIPCMessage(const json::Object& params)
  {
    return _writer.write(params, false);
  }

protected:
  
  void receiveIPCMessage()
  {
    while(_reader.isOpen())
    {
      json::Object params;
      if (_reader.read(params, true))
        onReceivedIPCMessage(params);
    }
  }

private:
  IPCJsonQueue _reader;
  IPCJsonQueue _writer;
  boost::thread* _pThread;
};


} // OSS



#endif	/* TELNYX_IPCQUEUE_H */

