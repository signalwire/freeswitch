/////////////////////////////////////////////////////////////////////////////
// Name:        win32/timer.cpp
// Purpose:
// Author:      Joachim Buermann
// Id:          $Id: timer.cpp,v 1.2 2004/11/30 12:39:17 jb Exp $
// Copyright:   (c) 2001 Joachim Buermann
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>

#ifdef WIN32
#include <windows.h>
# ifndef DWORD_PTR 
#  define DWORD_PTR DWORD*
# endif
#endif

#include "ctb-0.16/timer.h"

namespace ctb {

    static void WINAPI timer_fnc(UINT uTimerID, 
						   UINT uMsg, 
						   DWORD_PTR dwUser, 
						   DWORD_PTR dw1, 
						   DWORD_PTR dw2)
    {
	   timer_control *tc = (timer_control*)dwUser;

	   if(tc->exitfnc) tc->exitfnc(NULL);
	   if(tc->exitflag) *tc->exitflag = 1;
	   tc->stop = 0;
    };

    Timer::Timer(unsigned int msecs,int* exitflag,void*(*exitfnc)(void*))
    {
	   control.msecs = msecs;
	   if(!control.msecs) control.msecs = 1;
	   control.exitflag = exitflag;
	   control.exitfnc = exitfnc;
	   control.stop = 0;
    };

    Timer::~Timer()
    {
	   stop();				  // stop the thread
    };

    int Timer::start()
    {
	   stop();
	   control.stop = timeSetEvent(control.msecs, 
							 (control.msecs > 10) ? 5 : 1, 
							 (LPTIMECALLBACK) timer_fnc, 
							 (DWORD) &control, 
							 TIME_ONESHOT | TIME_CALLBACK_FUNCTION);
	   return 0;
    };

    int Timer::stop()
    {
	   if (control.stop)
		  timeKillEvent(control.stop);
	   control.stop = 0;
	   return 0;
    };

    void kill_all_timer()
    {
    };

    void sleepms(unsigned int ms)
    {
	   // set the granularity of Sleep() for the application, that
	   // calls it so Sleep(1) will truly sleep for just a millisecond,
	   // rather than the default 10!
	   // See: http://www.geisswerks.com/ryan/FAQS/timing.html
	   timeBeginPeriod(1);
	   SleepEx(ms,false);
	   timeEndPeriod(1);
    };

} // namespace ctb
