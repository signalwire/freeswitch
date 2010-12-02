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
  "GNU Lesser General Public License 2.1" license (the "LGPL" License), in which
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

/* This struct uses static polymorphism, and derived classes should implement *
 * the "unreference()" method, which should release all resources when called */

#ifndef _REFCOUNTER_HPP_
#define _REFCOUNTER_HPP_

#define COUNTER_CLASS(...)    ReferenceCounter< __VA_ARGS__ >
#define COUNTER_SUPER(...)    public COUNTER_CLASS( __VA_ARGS__ )
#define COUNTER_REFER(o, ...) COUNTER_CLASS( __VA_ARGS__ )(static_cast< const COUNTER_CLASS( __VA_ARGS__ ) & >(o))

// DEPRECATED DECLARATIONS ///
#define NEW_REFCOUNTER(...)    public ReferenceCounter< __VA_ARGS__ >
#define INC_REFCOUNTER(o, ...) ReferenceCounter< __VA_ARGS__ >(static_cast< const ReferenceCounter < __VA_ARGS__ > & >(o))

#include <stdlib.h>

#include <noncopyable.hpp>
#include <atomic.hpp>

#ifdef DEBUG
# include <iostream>
#endif

struct ReferenceData: public NonCopyable
{
    ReferenceData()
    : _data_count(1)
    {};

    inline unsigned int increment(void)
    {
        if (!_data_count)
            abort();

        Atomic::doAdd(&_data_count);

        return _data_count;
    }

    inline unsigned int decrement(void)
    {
        if (!_data_count)
            abort();

        Atomic::doSub(&_data_count);
        return _data_count;
    }

    volatile unsigned int _data_count;
};

template < typename T >
struct ReferenceCounter
{
    typedef T Type;

    ReferenceCounter(bool create_counter = true)
    : _reference_count(0)
    {
        reference_restart(create_counter);

#ifdef DEBUG
        std::cerr <<  ((void*)this) << ": ReferenceCounter() [ref_count="
            << (_reference_count ? (*_reference_count) : -1) << "]" << std::endl;
#endif
    };

    ReferenceCounter(const ReferenceCounter & o)
    : _reference_count(0)
    {
        reference_reflect(o);

#ifdef DEBUG
        std::cerr << ((void*)this) << ": ReferenceCounter(" << ((void*)(&o)) << ") [ref_count="
            << (_reference_count ? (*_reference_count) : -1) << "]" << std::endl;
#endif
    };

    virtual ~ReferenceCounter()
    {
#ifdef DEBUG
        std::cerr << ((void*)this) << ": ~ReferenceCounter() [ref_count="
            << (_reference_count ? (*_reference_count) : -1) << "]" << std::endl;
#endif
        reference_disconnect(_reference_count);
    }

    ReferenceCounter & operator=(const ReferenceCounter & o)
    {
        reference_reflect(o);

#ifdef DEBUG
        std::cerr << ((void*)this) << ": ReferenceCounter::operator=(" << ((void*)(&o)) << ") [ref_count="
            << (_reference_count ? (*_reference_count) : -1) << "]" << std::endl;
#endif

        return *this;
    };

 protected:
    inline void reference_restart(bool create_counter = false)
    {
        ReferenceData * oldref = _reference_count;

        _reference_count = (create_counter ? new ReferenceData() : 0);

        if (oldref) reference_disconnect(oldref);
    }

    inline void reference_reflect(const ReferenceCounter & other)
    {
        ReferenceData * newref = other._reference_count;
        ReferenceData * oldref = _reference_count;

        /* NOTE: increment before, avoid our reference being zero, even *
         *       for a short period of time.                            */

        if (newref) newref->increment();

        _reference_count = newref;

        if (oldref) reference_disconnect(oldref);
    };

    inline void reference_disconnect(ReferenceData *& counter)
    {
        if (counter)
        {
            unsigned int result = counter->decrement();

            if (!result)
            {
                static_cast< Type * >(this)->unreference();
                delete counter;
            }

            counter = 0;
        }
    };

  private:
    ReferenceData * _reference_count;
};

template < typename T >
struct ReferenceContainer: COUNTER_SUPER(ReferenceContainer< T >)
{
    /* type */
    typedef T Type;

    /* shorthand */
    typedef COUNTER_CLASS(ReferenceContainer< Type >) Counter;

    // TODO: make this a generic exception someday
    struct NotFound {};

    ReferenceContainer()
    : Counter(false),
      _reference_value(0)
    {};

    ReferenceContainer(Type * value)
    : _reference_value(value)
    {};

    ReferenceContainer(const ReferenceContainer & value)
    : Counter(false),
      _reference_value(0)
    {
        operator()(value);
    };

    virtual ~ReferenceContainer()
    {};

    ReferenceContainer operator=(const ReferenceContainer & value)
    {
        operator()(value);
        return *this;
    };

    /**/

    void unreference()
    {
        if (_reference_value)
        {
            delete _reference_value;
            _reference_value = 0;
        }
    }

    // simulates a copy constructor
    void operator()(const ReferenceContainer & value)
    {
        Counter::reference_reflect(value);

        _reference_value = const_cast<Type *>(value._reference_value);
    };

    // shortcut for operator below
    void operator=(const Type * value)
    {
        operator()(value);
    };

    // accept value (pointer)!
    void operator()(const Type * value)
    {
         Counter::reference_restart((value != 0));

        _reference_value = const_cast<Type *>(value);
    };

    // return value (pointer)!
    Type * operator()(void) const
    {
        return _reference_value;
    };

  protected:
    Type * _reference_value;

  protected:
};

#endif /* _REFCOUNTER_HPP_ */
