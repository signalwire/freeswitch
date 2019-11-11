// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.

#ifndef TIMEDQUEUE_H
#define	TIMEDQUEUE_H

#include <map>
#include <string>
#include <boost/any.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/thread.hpp>

namespace Telnyx {



class TimedQueue
{
public:
  typedef boost::recursive_mutex mutex;
  typedef boost::lock_guard<mutex> mutex_lock;
  typedef boost::function<void(const std::string&, const boost::any&)>  TimedQueueHandler;
  struct TimedQueueRecord
  {
    TimedQueueRecord() :
      deadline(0)
    {
    }

    ~TimedQueueRecord()
    {
      if (deadline)
      {
        deadline->cancel();
        delete deadline;
      }
    }
    std::string id;
    boost::any data;
    boost::asio::deadline_timer* deadline;
    TimedQueueHandler deadlineFunc;
  };

  TimedQueue() :
     _ioService()
  {
    housekeeper = new boost::asio::deadline_timer(_ioService, boost::posix_time::milliseconds(3600 * 1000));
    housekeeper->expires_from_now(boost::posix_time::milliseconds(3600 * 1000));
    housekeeper->async_wait(boost::bind(&TimedQueue::onHousekeeping, this, boost::asio::placeholders::error));

    _thread = new boost::thread(boost::bind(&boost::asio::io_service::run, &_ioService));
  }

  ~TimedQueue()
  {
    _ioService.stop();
    _thread->join();
    delete _thread;
    delete housekeeper;
  }

  void enqueue(const std::string& id, const boost::any& data, const TimedQueueHandler& deadlineFunc, int expires)
  {
    mutex_lock lock(_mutex);
    _map.erase(id);
    TimedQueueRecord record;
    _map[id] = record;
    _map[id].id = id;
    _map[id].data = data;
    _map[id].deadlineFunc = deadlineFunc;
    boost::asio::deadline_timer* timer = new boost::asio::deadline_timer(_ioService, boost::posix_time::milliseconds(expires * 1000));
    _map[id].deadline = timer;
    timer->expires_from_now(boost::posix_time::milliseconds(expires * 1000));
    timer->async_wait(boost::bind(&TimedQueue::onRecordExpire, this, boost::asio::placeholders::error, id));
  }

  void enqueue(const std::string& id, const boost::any& data)
  {
    mutex_lock lock(_mutex);
    _map.erase(id);
    TimedQueueRecord record;
    record.id = id;
    record.data = data;
    _map[id] = record;
  }
  
  void erase(const std::string& id)
  {
    mutex_lock lock(_mutex);
    _map.erase(id);
  }

  bool dequeue(const std::string& id, boost::any& data)
  {
    mutex_lock lock(_mutex);
    if (_map.find(id) == _map.end())
      return false;
    data = _map[id].data;
    _map.erase(id);
    return true;
  }

  bool get(const std::string& id, boost::any& data)
  {
    mutex_lock lock(_mutex);
    if (_map.find(id) == _map.end())
      return false;
    data = _map[id].data;
    return true;
  }
  
protected:
  void onRecordExpire(const boost::system::error_code& e, const std::string& id)
  {
    if (!e)
    {
    
      _mutex.lock();
      if (_map.find(id) == _map.end())
      {
        _mutex.unlock();
        return;
      }

      TimedQueueHandler deadlineFunc = _map[id].deadlineFunc;
      boost::any data = _map[id].data;
      _map.erase(id);
      _mutex.unlock();

      //
      // mutex is no longer locked.  This callback may reinsert the item to the queue without deadlock
      //
      deadlineFunc(id, data);
    }
  }

  void onHousekeeping(const boost::system::error_code&)
  {
    housekeeper->expires_from_now(boost::posix_time::milliseconds(3600 * 1000));
    housekeeper->async_wait(boost::bind(&TimedQueue::onHousekeeping, this, boost::asio::placeholders::error));
  }
  boost::asio::io_service _ioService;
  boost::thread* _thread;
  boost::asio::deadline_timer* housekeeper;
  boost::recursive_mutex _mutex;
  std::map<std::string, TimedQueueRecord> _map;
};

}

#endif	/* TIMEDQUEUE_H */

