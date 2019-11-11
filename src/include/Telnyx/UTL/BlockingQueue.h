// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.


#ifndef TELNYX_BLOCKINGQUEUE_H_INCLUDED
#define TELNYX_BLOCKINGQUEUE_H_INCLUDED

#include <queue>
#include <boost/noncopyable.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include "Telnyx/UTL/Thread.h"
#include <unistd.h>

namespace Telnyx {

template <class T>
class BlockingQueue : boost::noncopyable
{
public:
  typedef boost::function<bool(BlockingQueue<T>&, const T&)> QueueObserver;
  
  BlockingQueue(bool usePipe = false, std::size_t maxSize = SEM_VALUE_MAX - 1) :
    _sem(0, SEM_VALUE_MAX),
    _usePipe(usePipe),
    _maxSize(maxSize)
  {
    if (_usePipe)
    {
      assert(pipe(_pipe) == 0);
    }
  }
  
  ~BlockingQueue()
  {
    if (_usePipe)
    {
      close(_pipe[0]);
      close(_pipe[1]);
    }
  }

  bool enqueue(T data)
  {
    _cs.lock();
    
    if (_maxSize && _queue.size() >= _maxSize)
    {
      _cs.unlock();
      return false;
    }
    
    if (_enqueueObserver && !_enqueueObserver(*this, data))
    {
      _cs.unlock();
      return false;
    }
    _queue.push_back(data);
    if (_usePipe)
    {
      std::size_t w = 0;
      w = write(_pipe[1], " ", 1);
      (void)w;
    }
    _cs.unlock();
    _sem.set();
    return true;
  }

  void dequeue(T& data)
  {
    _sem.wait();
    _cs.lock();
    if (_usePipe)
    {
      std::size_t r = 0;
      char buf[1];
      r = read(_pipe[0], buf, 1);
      (void)r;
    }
    if (!_queue.empty())
    {
      data = _queue.front();
      _queue.pop_front();
      if (_dequeueObserver)
      {
        _dequeueObserver(*this, data);
      }
    }
    _cs.unlock();
  }

  bool try_dequeue(T& data, long milliseconds)
  {
    if (!_sem.tryWait(milliseconds))
      return false;

    _cs.lock();
    if (_usePipe)
    {
      std::size_t r = 0;
      char buf[1];
      r = read(_pipe[0], buf, 1);
      (void)r;
    }
    if (!_queue.empty())
    {
      data = _queue.front();
      _queue.pop_front();
      if (_dequeueObserver)
      {
        _dequeueObserver(*this, data);
      }
    }
    else
    {
      _cs.unlock();
      return false;
    }
    _cs.unlock();

    return true;
  }
  
  std::size_t size() const
  {
    std::size_t ret = 0;
    _cs.lock();
    ret = _queue.size();
    _cs.unlock();
    return ret;
  }
  
  int getFd() const
  {
    return _usePipe ? _pipe[0] : 0;
  }
  
  void setEnqueueObserver(const QueueObserver& observer)
  {
    _enqueueObserver = observer;
  }
  
  void setDequeueObserver(const QueueObserver& observer)
  {
    _dequeueObserver = observer;
  }
  
  void copy(std::deque<T>& content)
  {
    _cs.lock();
    std::copy(_queue.begin(), _queue.end(),  std::back_inserter(content));
    _cs.unlock();
  }
  
  void clear()
  {
    while(size() != 0)
    {
      T data;
      dequeue(data);
    }
  }
  
  void setMaxSize(std::size_t maxSize)
  {
    _cs.lock();
    _maxSize = maxSize;
    _cs.unlock();
  }
  
  std::size_t getMaxSize() const
  {
    return _maxSize;
  }
private:
  Telnyx::semaphore _sem;
  mutable Telnyx::mutex_critic_sec _cs;
  std::deque<T> _queue;
  int _pipe[2];
  bool _usePipe;
  QueueObserver _enqueueObserver;
  QueueObserver _dequeueObserver;
  std::size_t _maxSize;
};

} // OSS

#endif //TELNYX_BLOCKINGQUEUE_H_INCLUDED


