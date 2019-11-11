// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.


#ifndef TELNYX_THREAD_H_INCLUDED
#define TELNYX_THREAD_H_INCLUDED

#include <boost/thread.hpp>
#include <boost/noncopyable.hpp>
#include <boost/function.hpp>
#include <boost/any.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>

#include "Telnyx/Telnyx.h"


namespace Telnyx {

//
// Mutex type definitions
//
typedef boost::recursive_mutex mutex;
typedef boost::lock_guard<mutex> mutex_lock;
typedef boost::mutex mutex_critic_sec;
typedef boost::lock_guard<mutex_critic_sec> mutex_critic_sec_lock;
typedef boost::shared_mutex mutex_read_write;
typedef boost::shared_lock<boost::shared_mutex> mutex_read_lock;
typedef boost::lock_guard<boost::shared_mutex> mutex_write_lock;
//
// Interprocess mutex
//
typedef boost::interprocess::named_mutex mutex_interprocess_critic_sec;
typedef boost::lock_guard<mutex_interprocess_critic_sec> mutex_interprocess_critic_sec_lock;

//
// Semaphore class since boost doesnt have semaphores
//
class TELNYX_API semaphore : boost::noncopyable
	/// A Semaphore is a synchronization object with the following
	/// characteristics:
	/// A semaphore has a value that is constrained to be a non-negative
	/// integer and two atomic operations. The allowable operations are V
	/// (here called set()) and P (here called wait()). A V (set()) operation
	/// increases the value of the semaphore by one.
	/// A P (wait()) operation decreases the value of the semaphore by one,
	/// provided that can be done without violating the constraint that the
	/// value be non-negative. A P (wait()) operation that is initiated when
	/// the value of the semaphore is 0 suspends the calling thread.
	/// The calling thread may continue when the value becomes positive again.
{
public:
  semaphore();
	semaphore(int n);
	semaphore(int n, int max);
		/// Creates the semaphore. The current value
		/// of the semaphore is given in n. The
		/// maximum value of the semaphore is given
		/// in max.
		/// If only n is given, it must be greater than
		/// zero.
		/// If both n and max are given, max must be
		/// greater than zero, n must be greater than
		/// or equal to zero and less than or equal
		/// to max.
    /// If n and max is not specified, value of n is 0 and max is 1

	~semaphore();
		/// Destroys the semaphore.

	void set();
		/// Increments the semaphore's value by one and
		/// thus signals the semaphore. Another thread
		/// waiting for the semaphore will be able
		/// to continue.

	void wait();
		/// Waits for the semaphore to become signalled.
		/// To become signalled, a semaphore's value must
		/// be greater than zero.
		/// Decrements the semaphore's value by one.

	bool wait(long milliseconds);
		/// Waits for the semaphore to become signalled.
		/// To become signalled, a semaphore's value must
		/// be greater than zero.
		/// Throws a TimeoutException if the semaphore
		/// does not become signalled within the specified
		/// time interval.
		/// Decrements the semaphore's value by one
		/// if successful.

	bool tryWait(long milliseconds);
		/// Waits for the semaphore to become signalled.
		/// To become signalled, a semaphore's value must
		/// be greater than zero.
		/// Returns true if the semaphore
		/// became signalled within the specified
		/// time interval, false otherwise.
		/// Decrements the semaphore's value by one
		/// if successful.
private:
  TELNYX_HANDLE _sem;
};


#define POOL_THREAD_STACK_SIZE 0

class TELNYX_API thread_pool : boost::noncopyable
{
public:
  typedef boost::any argument_place_holder;
  
  thread_pool(int minCapacity = 2,
		int maxCapacity = 1024,
		int idleTime = 60,
		int stackSize = POOL_THREAD_STACK_SIZE);
    /// Creates a new thread pool

  ~thread_pool();
    /// Destroys the threadpool.  
    /// This will wait for all worker to terminate

  void join();
		/// Waits for all threads to complete.

  int schedule(boost::function<void()> task);
    /// Schedule a task for execution.  Returns the number of
    /// currently used thread if successful or -1 if unsuccessful.


  int schedule_with_arg(boost::function<void(argument_place_holder)> task, argument_place_holder arg);
    /// Schedule a task with a placeholder argument.  Returns the number of
    /// currently used thread if successful or -1 if unsuccessful.
  
  int schedule_with_arg(boost::function<void(void*)> task, void* arg);
    /// Schedule a task with a void* argument.  Returns the number of
    /// currently used thread if successful or -1 if unsuccessful.

  static void static_join();
    /// Waits for all threads in the deafult thread pool to complete.

  static int static_schedule(boost::function<void()> task);
    /// Schedule a task using the default thread pool.  Returns the number of
    /// currently used thread if successful or 0 if unsuccessful.

  static int static_schedule_with_arg(boost::function<void(argument_place_holder)> task, argument_place_holder arg);
    /// Schedule a task with a placeholder argument using the default thread pool.  Returns the number of
    /// currently used thread if successful or 0 if unsuccessful.
private:
  void* _threadPool;
};


class Thread : public boost::noncopyable
{
public:
  typedef boost::function<void()> Task;
  Thread();
  Thread(const Task& task);
  virtual ~Thread();
  void run();
  void stop();
  void setTask(const Task& task);
  bool isTerminated();
  void waitForTermination();
protected:
  virtual void main();
  virtual void onTerminate();
  Task _task;
  boost::thread* _pThread;
  bool _terminateFlag;
  mutex_critic_sec _terminateFlagMutex;
  mutex_critic_sec _threadMutex;
};



void TELNYX_API thread_sleep( unsigned long milliseconds );
  /// Pause thread execution for certain time expressed in milliseconds


//
// Inlines
//


} // OSS


#endif //TELNYX_THREAD_H_INCLUDED

