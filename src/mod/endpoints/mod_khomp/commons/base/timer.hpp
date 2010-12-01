/*
    KHOMP generic endpoint/channel library.
    Copyright (C) 2007-2009 Khomp Ind. & Com.

  The contents of this file are subject to the Mozilla Public License Version 1.1
  (the "License"); you may not use this file except in compliance with the
  License. You may obtain a copy of the License at http://www.mozilla.org/MPL/

  Software distributed under the License is distributed on an "AS IS" basis,
  WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for
  the specific language governing rights and limitations under the License.

  Alternatively, the contents of this file may be used under the terms of the
  "GNU Lesser General Public License 2.1" license (the “LGPL" License), in which
  case the provisions of "LGPL License" are applicable instead of those above.

  If you wish to allow use of your version of this file only under the terms of
  the LGPL License and not to allow others to use your version of this file under
  the MPL, indicate your decision by deleting the provisions above and replace them
  with the notice and other provisions required by the LGPL License. If you do not
  delete the provisions above, a recipient may use your version of this file under
  either the MPL or the LGPL License.

  The LGPL header follows below:

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#if defined(_WINDOWS) || defined(_Windows) || defined(_WIN32) || defined(WIN32)
#include <windows.h>
#else
#include <sched.h>
#endif

#include <set>

#include <simple_lock.hpp>
#include <saved_condition.hpp>
#include <thread.hpp>
#include <refcounter.hpp>

#ifndef _TIMER_HPP_
#define _TIMER_HPP_

struct TimerTraits
{
    typedef bool          (* CallbackFuncType)(const void * volatile);
    typedef const void *     CallbackDataType;

    TimerTraits();

    virtual ~TimerTraits() {};

 protected:

    /* pre-declaration, used below */
    struct ControlCompare;

    struct Control
    {
        Control(unsigned int age, unsigned int msecs, const void * func = 0, const void * data = 0, unsigned int value = 0)
        : _age(age), _msecs(msecs), _func(func), _data(data), _value(value) {}

        unsigned int  _age;

        unsigned int  _msecs;

        const void * _func;
        const void * _data;

        unsigned int  _value;
    };

    struct ControlCompare
    {
        bool operator()(const Control & c1, const Control & c2) const
        {
            return (c1._age < c2._age ? true : c1._msecs < c2._msecs);
        }
    };

    typedef std::multiset < Control, ControlCompare > ControlSet;

 public:
    struct Index
    {
        Index(): era(0), msec(0), valid(false) {};

        Index(unsigned int _era, unsigned int _msec, unsigned int _delta, const void * _func, const void * _data, unsigned int _value)
        : era(_era), msec(_msec), delta(_delta), func(_func), data(_data), value(_value), valid(true) {};

        unsigned int   era;
        unsigned int  msec;

        unsigned int  delta;

        const void *  func;
        const void *  data;

        unsigned int value;

        bool         valid;

        void reset(void) { era = 0; msec = 0; valid = false; };
    };

    /* timer add/remove functions */
    Index traits_add (unsigned int msecs, const void * func, const void * data = 0, unsigned int value = 0);

    bool traits_restart (Index & idx, bool force);

    bool traits_del (Index & idx);
    bool traits_del (const void * func, const void * data = 0, unsigned int value = 0);

    void traits_setup(Index * idx, unsigned int msecs, const void * func, void * data = 0, unsigned int value = 0);

    /* timer start/stop functions */
    bool start(void);
    bool stop(void);

  protected:
    Index traits_add_unlocked (unsigned int msecs, const void * func, const void * data, unsigned int value);

    bool traits_del_unlocked (Index & idx);

  protected:
    void execute(ControlSet::iterator, const Control &);

    void loop(void);

    struct loop_thread
    {
        loop_thread(TimerTraits *timer) : _timer(timer) {};

        int operator()(void);

      protected:
        TimerTraits * _timer;
    };

    unsigned int tick();

    /* variables */

    SavedCondition   _condition;

    SimpleLock       _mutex;
    Thread         * _thread;
    bool             _purify;

    ControlSet       _timer_set;

    unsigned int     _last_tick;
    unsigned int     _age_count;

    SavedCondition   _finalized;
    bool             _shutdown;
};

template < typename F, typename D >
struct TimerTemplate: COUNTER_SUPER(TimerTemplate< F, D >)
{
    typedef TimerTraits::Index   Index;
    typedef TimerTraits::Control Control;

    TimerTemplate()
    : _timer(new TimerTraits())
    {};

    TimerTemplate(const TimerTemplate< F, D > & o)
    : COUNTER_REFER(o, TimerTemplate< F, D >),
      _timer(o._timer)
    {};

    void unreference(void)
    {
        if (_timer)
            delete _timer;
    };

    bool start() { return _timer->start(); }
    bool stop()  { return _timer->stop();  }

    inline void setup(Index * idx, unsigned int msecs, F * func, D data = 0, unsigned int value = 0)
    {
        _timer->traits_setup(idx, msecs, (const void *)func, (void *)(data), value);
    }

    inline Index add(unsigned int msecs, F * func, D data = 0, unsigned int value = 0)
    {
        return _timer->traits_add(msecs, (const void *)func, (void *)(data), value);
    }

    inline bool restart(Index & idx, bool force = false)
    {
        return _timer->traits_restart(idx, force);
    }

    inline bool del(Index & idx)
    {
        return _timer->traits_del(idx);
    }

    inline bool del(F * func, D data, unsigned int value = 0)
    {
        return _timer->traits_del((const void *)func, (void *)(data), value);
    }

    inline bool del(unsigned int value)
    {
        return _timer->traits_del((const void *)0, (void *)0, value);
    }

  protected:
    TimerTraits * _timer;
};

#endif /* _TIMER_HPP_ */
