/////////////////////////////////////////////////////////////////////////////
// Name:        linux/timer.cpp
// Purpose:
// Author:      Joachim Buermann
// Id:          $Id: timer.cpp,v 1.1.1.1 2004/11/24 10:30:11 jb Exp $
// Copyright:   (c) 2001 Joachim Buermann
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////


#include "ctb-0.16/timer.h"
#include <unistd.h>

namespace ctb {

// a dummy function, see below
    static void timer_exit(void* arg)
    {
    };

    static void* timer_fnc(void* arg)
    {
	   // the timer thread should be canceled every time
	   // (asyncronously)
	   pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);
	   // this is tricky, but absolutly necessarily to avoid segfaults
	   // if the destructor finished a running thread
	   pthread_cleanup_push(timer_exit,NULL);
	   timer_control *tc = (timer_control*)arg;
	   // linux allows a real sleep, means the timer thread will
	   // be sleeping (no reduce of the system performance) 
	   usleep(tc->usecs);
	   // time is over, system reawake the thread.
	   // if there is an exit function, calling it
	   if(tc->exitfnc) tc->exitfnc(NULL);
	   // set the exit flag
	   if(tc->exitflag) *tc->exitflag = 1;
	   // deallocate the system resources (thread)
	   pthread_cleanup_pop(1);
	   return NULL;
    };

    // the constructor initiate the internal control struct
    Timer::Timer( unsigned int msecs, int* exitflag, void*( *exitfnc )(void*) )
    {
	   control.usecs = msecs * 1000;
	   control.exitflag = exitflag;
	   control.exitfnc = exitfnc;
	   stopped = 1;
    };

    // if a timer instance leave it's valid range, it automaticaly will
    // be finished
    Timer::~Timer()
    {
	   if(!stopped) {
		  // only a running thread may be canceled
		  stop();
	   }
    };

    // starts the timer thread
    int Timer::start()
    {
	   stopped = 0;
	   if(pthread_create(&tid,	// result parameter, covers the id
					 // of the new threads
					 NULL,	// thread attribute object, NULL means
					 // the defaults
					 timer_fnc,	// start function of the thread
					 &control	// start function parameter, must refer
					 // as void*
			) == -1) {
		  return -1;		// there was something going wrong
	   }
	   pthread_detach(tid);	// thread status must be "detach". In the other
	   // case, the destructor call of a running
	   // thread throws a segfault
	   return 0;
    };

    // stop the timer thread
    int Timer::stop()
    {
	   if(control.exitflag && (*control.exitflag == 0)) {
		  pthread_cancel(tid);
	   }
	   stopped = 1;
	   return 0;
    };

    void sleepms(unsigned int ms)
    {
	   usleep(ms * 1000);
    };

} // namespace ctb
