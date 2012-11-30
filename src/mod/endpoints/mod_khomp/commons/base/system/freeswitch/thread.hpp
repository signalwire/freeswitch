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

#ifndef _THREAD_HPP_
#define _THREAD_HPP_

#include <thread.hpp>

extern "C"
{
    #include <switch.h>
}

struct Thread : ThreadCommon
{
    typedef switch_thread_t BaseThreadType;

    template<typename T, typename R, typename A>
    struct ThreadData : ThreadDataCommon
    {
        ThreadData(T data, A arg) : _data(data), _arg(arg) {}

        int run()
        {
            return _data(_arg);
        }

        T _data;
        A _arg;
    };

    template<typename T, typename R>
    struct ThreadData < T, R, void > : ThreadDataCommon
    {
        ThreadData(T data) : _data(data) {}

        int run()
        {
            return _data();
        }

        T _data;
    };

    template<typename T, typename A>
    struct ThreadData < T, void, A > : ThreadDataCommon
    {
        ThreadData(T data, A arg) : _data(data), _arg(arg) {}

        int run()
        {
            _data(_arg);
            return 0;
        }

        T _data;
        A _arg;
    };


    template<typename T>
    struct ThreadData < T, void, void > : ThreadDataCommon
    {
        ThreadData(T data) : _data(data) {}

        int run()
        {
            _data();
            return 0;
        }

        T _data;
    };

    template<typename T>
    Thread(T obj, switch_memory_pool_t *pool=NULL) :
            _thread_info(new ThreadData<T, typename DecomposeFunction<T>::Return, void>(obj)),
            _pool(pool),
            _can_delete_pool(false)
    {
        if(!_pool)
        {
            switch_core_new_memory_pool(&_pool);
            _can_delete_pool = true;
        }

        _thread_info->_thread = this;
        _thread_info->_self = NULL;
        _thread_info->_attribute = NULL;

        if(switch_threadattr_create(
                (switch_threadattr_t **)&_thread_info->_attribute, _pool) != 0)
        {
            _thread_info->_attribute = NULL;
            return;
        }

        switch_threadattr_stacksize_set(
                (switch_threadattr_t *)_thread_info->_attribute,
                SWITCH_THREAD_STACKSIZE);

        if(!priority())
        {
            _thread_info->_attribute = NULL;
        }

    }

    template<typename T, typename A>
    Thread(T obj, A arg, switch_memory_pool_t *pool=NULL) :
            _thread_info(new ThreadData<T, typename DecomposeFunction<T>::Return, A>(obj, arg)),
            _pool(pool),
            _can_delete_pool(false)
    {
        if(!_pool)
        {
            switch_core_new_memory_pool(&_pool);
            _can_delete_pool = true;
        }

        _thread_info->_thread = this;
        _thread_info->_self = NULL;
        _thread_info->_attribute = NULL;

        if(switch_threadattr_create(
                (switch_threadattr_t **)&_thread_info->_attribute, _pool) != 0)
        {
            _thread_info->_attribute = NULL;
            return;
        }

        switch_threadattr_stacksize_set(
                (switch_threadattr_t *)_thread_info->_attribute,
                SWITCH_THREAD_STACKSIZE);

        if(!priority())
        {
            _thread_info->_attribute = NULL;
        }

    }

    ~Thread()
    {
        if(_thread_info)
            delete _thread_info;

        if (_can_delete_pool)
            switch_core_destroy_memory_pool(&_pool);
    }

    void detach(bool d = true)
    {
        if(!_thread_info->_attribute)
            return;

        /* Non-zero if detached threads should be created. */
        switch_threadattr_detach_set(
                (switch_threadattr_t *)_thread_info->_attribute, d ? 1 : 0);
    }

    bool start()
    {
        if(!_pool || !_thread_info->_attribute)
            return false;

        switch_thread_create((switch_thread_t**)&_thread_info->_self,
                (switch_threadattr_t *)_thread_info->_attribute,
                run,
                _thread_info,
                _pool);

        if(!_thread_info->_self)
            return false;

        return true;
    }

    int join()
    {
        /*
         * block until the desired thread stops executing.
         * @param retval The return value from the dead thread.
         * @param thd The thread to join
         *
         * SWITCH_DECLARE(switch_status_t) switch_thread_join(switch_status_t *retval, switch_thread_t *thd);
        */

        if(!_thread_info->_self)
            return -2;

        int retval = 0;

        if(switch_thread_join((switch_status_t*)&retval,
                (switch_thread_t *)_thread_info->_self) != 0)
            return -1;

        return retval;
    }

    BaseThreadType * self()
    {
        //switch_thread_self();
        //apr_os_thread_current();
        return (BaseThreadType *)_thread_info->_self;
    }

private:
    void exit(int status)
    {
        /**
         * stop the current thread
         * @param thd The thread to stop
         * @param retval The return value to pass back to any thread that cares
         */
        //SWITCH_DECLARE(switch_status_t) switch_thread_exit(switch_thread_t *thd, switch_status_t retval);
        switch_thread_exit((switch_thread_t *)_thread_info->_self, (switch_status_t)status);

    }

#ifndef WIN32
    struct apr_threadattr_t {
        apr_pool_t *pool;
        pthread_attr_t attr;
    };
#endif

    bool priority()
    {
#ifndef WIN32
        struct sched_param param;

        struct apr_threadattr_t *myattr = (struct apr_threadattr_t *)_thread_info->_attribute;

        if (pthread_attr_setschedpolicy(
                (pthread_attr_t *)&myattr->attr, SCHED_RR) < 0)
            return false;

        if (pthread_attr_getschedparam(
                (pthread_attr_t *)&myattr->attr, &param) < 0)
            return false;

        param.sched_priority = sched_get_priority_max(SCHED_RR);

        if (pthread_attr_setschedparam(
                (pthread_attr_t *)&myattr->attr, &param) < 0)
            return false;

#endif
        return true;

/*
        //BUG in Freeswitch  

THANKS FOR NOT REPORTING IT SO WE MUST LIVE WITH A PROBLEM YOU KNOW ABOUT .....

        if(switch_threadattr_priority_increase(
                (switch_threadattr_t *)_thread_info->_attribute) != 0)
            return false;

        return true;
*/
    }


protected:
    ThreadDataCommon * _thread_info;
    switch_memory_pool_t *_pool;
    bool _can_delete_pool;

protected:

    static void *SWITCH_THREAD_FUNC run(BaseThreadType *thread, void * obj)
    {
        //switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
        //        "Starting new Thread\n");

        ThreadDataCommon * data = (ThreadDataCommon *)obj;
        int retval = data->run();

        //switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
        //        "Stopping new Thread = %d\n", retval);

        ((Thread *)(data->_thread))->exit(retval);

        return NULL;
    }

};


#endif /* _THREAD_HPP_ */
