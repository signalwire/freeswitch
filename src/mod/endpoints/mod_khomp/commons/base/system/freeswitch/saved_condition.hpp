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

#ifndef _SAVED_CONDITION_
#define _SAVED_CONDITION_

#include <saved_condition.hpp>

extern "C"
{
    #include <switch.h>
}

struct SavedCondition : public SavedConditionCommon// : public RefCounter < SavedCondition >
{
    typedef switch_thread_cond_t  BaseConditionType;
    typedef switch_mutex_t        BaseMutexType;

     SavedCondition(switch_memory_pool_t *pool=NULL):
        _pool(pool),
        _can_delete_pool(false)
     {
        if(!_pool)
        {
            switch_core_new_memory_pool(&_pool);
            _can_delete_pool = true;
        }

        switch_thread_cond_create(&_condition, _pool);
        switch_mutex_init(&_mutex, SWITCH_MUTEX_DEFAULT, _pool);
     }

     //SavedCondition(const SavedCondition &);
    ~SavedCondition()
    {
        switch_thread_cond_destroy(_condition);
        switch_mutex_destroy(_mutex);

        if(_can_delete_pool)
            switch_core_destroy_memory_pool(&_pool);
    }

    void signal(void)
    {
        switch_mutex_lock(_mutex);

        _signaled = true;
        switch_thread_cond_signal(_condition);

        switch_mutex_unlock(_mutex);
    }

    void broadcast(void)
    {
        switch_mutex_lock(_mutex);

        _signaled = true;
        switch_thread_cond_broadcast(_condition);

        switch_mutex_unlock(_mutex);
    }

    void wait(void)
    {
        switch_mutex_lock(_mutex);

        if (!_signaled)
            switch_thread_cond_wait(_condition, _mutex);

        _signaled = false;

        switch_mutex_unlock(_mutex);
    }

    bool wait(unsigned int);

    void reset(void)
    {
        switch_mutex_lock(_mutex);

        _signaled = false;

        switch_mutex_unlock(_mutex);
    }

    BaseMutexType     * mutex()     { return _mutex;     };
    BaseConditionType * condition() { return _condition; };

 protected:

    BaseConditionType    *_condition;
    BaseMutexType        *_mutex;
    switch_memory_pool_t *_pool;
    bool                 _can_delete_pool;
};

#endif /* _SAVED_CONDITION_ */

