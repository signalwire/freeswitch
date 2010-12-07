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

#include <timer.hpp>

TimerTraits::TimerTraits()
: _thread((Thread*)0), _purify(false), _last_tick(0), _age_count(0), _shutdown(false)
{};

bool TimerTraits::start (void)
{
    _shutdown = false;

    _condition.reset();
    _finalized.reset();

    if (!_thread)
    {
        _thread = new Thread(TimerTraits::loop_thread(this));
        _purify = true;
    }

#if defined(_WINDOWS) || defined(_Windows) || defined(_WIN32) || defined(WIN32)
    // set priority ...
#else
    pthread_attr_t attrs;
    sched_param    param;

    if (pthread_attr_init(&attrs) < 0)
        return false;

    if (pthread_attr_setschedpolicy(&attrs, SCHED_RR) < 0)
        return false;

    if (pthread_attr_getschedparam(&attrs, &param) < 0)
        return false;

    param.sched_priority = sched_get_priority_max(SCHED_RR);

    if (pthread_attr_setschedparam(&attrs, &param) < 0)
        return false;

    // set priority...

//    if (pthread_create(&_thread, &attrs, TimerTraits::loop_thread, NULL) < 0)
//        return false;

    _thread->start();
#endif

    return true;
}

bool TimerTraits::stop (void)
{
    _shutdown = true;

    _condition.signal();

    _finalized.wait(10000); /* 10 seconds max wait */

    if (_thread && _purify)
    {
        delete _thread;

        _thread = (Thread *)0;
        _purify = false;
    }

    return true;
}

//-----------------------------------------

int TimerTraits::loop_thread::operator()(void)
{
    try
    {
        _timer->loop();
    }
    catch( ... )
    {
        /* something wrong happened! */
    }

    _timer->_finalized.signal();

    return 0;
}

void TimerTraits::execute(ControlSet::iterator init, const TimerTraits::Control & ctrl)
{
    volatile CallbackFuncType func = (volatile CallbackFuncType) ctrl._func;
    volatile CallbackDataType data = (volatile CallbackDataType) ctrl._data;

    _timer_set.erase(init);

    _mutex.unlock();

    func(data);
}

void TimerTraits::loop (void)
{
    while (true)
    {
        if (_shutdown) break;

        _mutex.lock();

        ControlSet::iterator init = _timer_set.begin();

        if (init == _timer_set.end())
        {
            _mutex.unlock();
            _condition.wait();
        }
        else
        {
            const Control & ctrl = *init;

            unsigned int ts_now    = TimerTraits::tick();

            if (_age_count == ctrl._age)
            {
                if (ts_now < ctrl._msecs)
                {
                    /* age is right, but it is not time to expire yet... */
                    volatile unsigned int wait_time = ctrl._msecs - ts_now;
                    _mutex.unlock();
                    _condition.wait(wait_time); /* expire - now */
                }
                else
                {
                    /* age is right, and we should expire! */
                    execute(init, ctrl); /* called locked, return unlocked */
                }
            }
            else if (_age_count < ctrl._age)
            {
                /* age is not there yet (need some time to overlap)... */
                volatile unsigned int wait_time = (UINT_MAX - ts_now) + ctrl._msecs;
                _mutex.unlock();
                _condition.wait(wait_time); /* MAX - now + expire */
            }
            else
            {
                /* age has passed, we should have expired before! */
                execute(init, ctrl); /* called locked, return unlocked */
            }
        }
    }

    _finalized.signal();
}

unsigned int TimerTraits::tick()
{
#if defined(_WINDOWS) || defined(_Windows) || defined(_WIN32) || defined(WIN32)
    unsigned int tick =  GetTickCount();
#else
    struct timespec ticks;

    // error condition, make the user notice this..
    if (clock_gettime(CLOCK_MONOTONIC, &ticks) < 0)
        return 0;

    unsigned int tick = ( ticks.tv_sec * 1000 ) + ( ticks.tv_nsec / 1000000 );
#endif

    if (_last_tick > tick)
        ++_age_count;

    _last_tick = tick;

    return tick;
}

//-----------------------------------------

TimerTraits::Index TimerTraits::traits_add_unlocked (unsigned int msecs, const void * func, const void * data, unsigned int value)
{
    unsigned int ms_tick = TimerTraits::tick();

    unsigned int ms_left =  UINT_MAX - ms_tick;
    unsigned int ms_real =  msecs;

    unsigned int age_num = _age_count;

    if (ms_left < msecs)
    {
        ms_real -= ms_left;
        ++age_num;
    }
    else
    {
        ms_real += ms_tick;
    }

    ControlSet::iterator it = _timer_set.insert(Control(age_num,ms_real,func,data,value));

    if (_timer_set.size() == 1 || _timer_set.begin() == it)
    {
        _condition.signal();
    };

    return Index(age_num, ms_real, msecs, func, data, value);
}

TimerTraits::Index TimerTraits::traits_add (unsigned int msecs, const void * func, const void * data, unsigned int value)
{
    _mutex.lock();

    Index idx = traits_add_unlocked(msecs, func, data, value);

    _mutex.unlock();

    return idx;
}

bool TimerTraits::traits_restart (TimerTraits::Index & idx, bool force)
{
    bool ret = false;

    _mutex.lock();

    if (idx.valid)
    {
        if (traits_del_unlocked(idx) || force)
        {
            idx = traits_add_unlocked(idx.delta, idx.func, idx.data, idx.value);
            ret = true;
        }
    }

    _mutex.unlock();

    return ret;
}

void TimerTraits::traits_setup(TimerTraits::Index * idx, unsigned int msecs, const void * func, void * data, unsigned int value)
{
    _mutex.lock();

    if (idx->valid)
    {
        (void)traits_del_unlocked(*idx);
    }

    *idx = traits_add_unlocked(msecs, func, data, value);

    _mutex.unlock();
}

bool TimerTraits::traits_del_unlocked (TimerTraits::Index & idx)
{
    bool ret = false;

    if (idx.valid)
    {
        ControlSet::iterator i = _timer_set.lower_bound(Control(idx.era, idx.msec));
        ControlSet::iterator j = _timer_set.upper_bound(Control(idx.era, idx.msec));

        for (; i != j; i++)
        {
            const Control & ctrl = (*i);

            if ((idx.value && !(ctrl._value & idx.value)))
                continue;

            if (((idx.func && ctrl._func == idx.func) || !idx.func) && ((idx.data && ctrl._data == idx.data) || !idx.data))
            {
                if (_timer_set.begin() == i)
                    _condition.signal();

                _timer_set.erase(i);

                ret = true;
                break;
            }
        }

        idx.valid = false;
    }

    return ret;
}

bool TimerTraits::traits_del (TimerTraits::Index & idx)
{
    _mutex.lock();

    bool ret = traits_del_unlocked(idx);

    _mutex.unlock();

    return ret;
}

bool TimerTraits::traits_del (const void * func, const void * data, unsigned int value)
{
    bool ret = false;

    _mutex.lock();

    for (ControlSet::iterator i = _timer_set.begin(); i != _timer_set.end(); i++)
    {
        const Control & ctrl = (*i);

        if ((value && !(ctrl._value & value)))
            continue;

        if (((func && ctrl._func == func) || !func) && ((data && ctrl._data == data) || !data))
        {
            if (_timer_set.begin() == i)
                _condition.signal();

            _timer_set.erase(i);

            ret = true;
            break;
        }
    }

    _mutex.unlock();

    return ret;
}
