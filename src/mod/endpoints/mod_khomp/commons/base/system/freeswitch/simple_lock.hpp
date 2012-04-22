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

#ifndef _SIMPLE_LOCK_HPP_
#define _SIMPLE_LOCK_HPP_

#include <simple_lock.hpp>

extern "C"
{
    #include <switch.h>
}

template < typename Implementor >
struct SimpleLockBasic: public SimpleLockCommon < Implementor >
{
    typedef SimpleLockCommon < Implementor >   Super;
    typedef typename Super::Result            Result;

    typedef switch_mutex_t BaseMutexType;

    SimpleLockBasic(switch_memory_pool_t *pool = NULL)
    : _pool(pool), _can_delete_pool( (_pool == NULL) )

    {
        if(!_pool)
            switch_core_new_memory_pool(&_pool);

        //switch_mutex_init(&_mutex, SWITCH_MUTEX_DEFAULT, _pool);
        switch_mutex_init(&_mutex, SWITCH_MUTEX_NESTED, _pool);
    }

    virtual ~SimpleLockBasic()
    {
        /* do nothing */
    };

    void unreference_data()
    {
        switch_mutex_destroy(_mutex);

        if (_can_delete_pool)
            switch_core_destroy_memory_pool(&_pool);
    }

    Result trylock()
    {
        switch (switch_mutex_trylock(_mutex))
        {
            case SWITCH_STATUS_SUCCESS:
                return Super::SUCCESS;
            case SWITCH_STATUS_FALSE:
            case SWITCH_STATUS_TERM:
            case SWITCH_STATUS_NOTIMPL:
            case SWITCH_STATUS_MEMERR:
            case SWITCH_STATUS_GENERR:
            case SWITCH_STATUS_SOCKERR:
            case SWITCH_STATUS_NOTFOUND:
            case SWITCH_STATUS_UNLOAD:
            case SWITCH_STATUS_NOUNLOAD:
            case SWITCH_STATUS_NOT_INITALIZED:
                return Super::FAILURE;
            //case SWITCH_STATUS_INUSE:
            default:
                return Super::ISINUSE;
        }
    }

    void  unlock()
    {
        switch_mutex_unlock(_mutex);
    }

    BaseMutexType * mutex() { return _mutex; };

 protected:
    BaseMutexType *_mutex;
    switch_memory_pool_t *_pool;
    bool _can_delete_pool;
};

struct SimpleLock: public SimpleLockBasic < SimpleLock >
{
    typedef SimpleLockBasic < SimpleLock >   Super;
    typedef Super::Result                   Result;

    SimpleLock(switch_memory_pool_t *pool = NULL)
    : Super(pool) {};

    Result lock()
    {
        switch (switch_mutex_lock(_mutex))
        {
            case SWITCH_STATUS_SUCCESS:
                return Super::SUCCESS;
            case SWITCH_STATUS_FALSE:
            case SWITCH_STATUS_TERM:
            case SWITCH_STATUS_NOTIMPL:
            case SWITCH_STATUS_MEMERR:
            case SWITCH_STATUS_GENERR:
            case SWITCH_STATUS_SOCKERR:
            case SWITCH_STATUS_NOTFOUND:
            case SWITCH_STATUS_UNLOAD:
            case SWITCH_STATUS_NOUNLOAD:
            case SWITCH_STATUS_NOT_INITALIZED:
                return Super::FAILURE;
            //case SWITCH_STATUS_INUSE:
            default:
                return Super::ISINUSE;
        }
    }
};

template < unsigned int Retries = 10, unsigned int Interval = 50 >
struct SimpleNonBlockLock: public SimpleLockBasic < SimpleNonBlockLock < Retries, Interval > >
{
    typedef SimpleLockBasic < SimpleNonBlockLock < Retries, Interval > >   Super;
    typedef typename Super::Result                                        Result;

    SimpleNonBlockLock(switch_memory_pool_t *pool = NULL)
    : Super(pool) {};

    inline Result lock()
    {
        for (unsigned int i = 0; i < Retries; i++)
        {
            Result ret = Super::trylock();

            if (ret != Super::ISINUSE)
                return ret;

            usleep(Interval * 1000);
        }

        return Super::ISINUSE;
    }
};

#endif /* _SIMPLE_LOCK_HPP_ */
