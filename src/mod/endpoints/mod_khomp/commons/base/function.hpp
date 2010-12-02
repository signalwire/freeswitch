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

#include <refcounter.hpp>

#ifndef _FUNCTION_HPP_
#define _FUNCTION_HPP_

namespace Function
{
    struct EmptyFunction     {};
    struct NonMemberFunction {};

    /**/

    template < typename FunctionTraits >
    struct StorageBase: COUNTER_SUPER(StorageBase < FunctionTraits >)
    {
        typedef typename FunctionTraits::BaseType BaseType;

        typedef typename FunctionTraits::FunType  FunType;
        typedef typename FunctionTraits::ObjType  ObjType;

        template < typename Functor >
        StorageBase(const Functor f)
        : _object(reinterpret_cast< ObjType >(new Functor(f))),
          _function(reinterpret_cast< FunType >(&Functor::operator())),
          _malloced(true)
        {};

        template < typename Functor >
        StorageBase(Functor & f, bool malloced)
        : _object(reinterpret_cast< ObjType >((malloced ? new Functor(f) : &f))),
          _function(reinterpret_cast< FunType >(&Functor::operator())),
          _malloced(malloced)
        {};

        StorageBase(FunType const * member)
        : _object(reinterpret_cast< ObjType >(0)),
          _function(reinterpret_cast< FunType >(member)),
          _malloced(false)
        {};

        StorageBase()
        : _object(reinterpret_cast< ObjType >(0)),
          _function(reinterpret_cast< FunType >(0)),
          _malloced(false)
        {};

        StorageBase(const StorageBase & o)
        : COUNTER_REFER(o, StorageBase < FunctionTraits >),
           _object(o._object), _function(o._function), _malloced(o._malloced)
        {};

        virtual ~StorageBase() {};

        void unreference()
        {
            // TODO: will this work if we delete a different type? //
            if (_malloced)
                delete _object;
        };

        template < typename Functor >
        void operator=(Functor f)
        {
            _object   = reinterpret_cast< ObjType >(new Functor(f)),
            _function = reinterpret_cast< FunType >(&Functor::operator());
            _malloced = true;
        }

     protected:
        ObjType _object;
        FunType _function;
        bool    _malloced;
    };

    /**/

    template < typename R >
    struct VTable0
    {
        R operator()(void) { return R(); };
    };

    template < >
    struct VTable0< void >
    {
        void operator()(void) { return; };
    };

    template < typename R >
    struct Function0Traits
    {
        typedef VTable0<R> BaseType;

        typedef R (BaseType::* FunType)(void);
        typedef BaseType *     ObjType;
    };

    /**/

    template < typename R, typename A0 >
    struct VTable1
    {
        R operator()(A0 a0) { return R(); };
    };

    template < typename A0 >
    struct VTable1< void, A0 >
    {
        void operator()(A0 a0) { return; };
    };

    template < typename R, typename A0 >
    struct Function1Traits
    {
        typedef VTable1<R, A0> BaseType;

        typedef R (BaseType::* FunType)(A0);
        typedef BaseType *     ObjType;
    };

    /**/

    template < typename R, typename A0, typename A1 >
    struct VTable2
    {
        R operator()(A0 a0, A1) { return R(); };
    };

    template < typename A0, typename A1 >
    struct VTable2< void, A0, A1 >
    {
        void operator()(A0 a0, A1 a1) { return; };
    };

    template < typename R, typename A0, typename A1 >
    struct Function2Traits
    {
        typedef VTable2<R, A0, A1> BaseType;

        typedef R (BaseType::* FunType)(A0, A1);
        typedef BaseType *     ObjType;
    };

    /**/

    template < typename R, typename A0, typename A1, typename A2 >
    struct VTable3
    {
        R operator()(A0 a0, A1 a1, A2 a2) { return R(); };
    };

    template < typename A0, typename A1, typename A2 >
    struct VTable3< void, A0, A1, A2 >
    {
        void operator()(A0 a0, A1 a1, A2 a2) { return; };
    };

    template < typename R, typename A0, typename A1, typename A2 >
    struct Function3Traits
    {
        typedef VTable3<R, A0, A1, A2> BaseType;

        typedef R (BaseType::* FunType)(A0, A1, A2);
        typedef BaseType *     ObjType;
    };

    /**/

    template < typename R, typename A0, typename A1, typename A2, typename A3 >
    struct VTable4
    {
        R operator()(A0 a0, A1 a1, A2 a2, A3 a3) { return R(); };
    };

    template < typename A0, typename A1, typename A2, typename A3 >
    struct VTable4< void, A0, A1, A2, A3 >
    {
        void operator()(A0 a0, A1 a1, A2 a2, A3 a3) { return; };
    };

    template < typename R, typename A0, typename A1, typename A2, typename A3 >
    struct Function4Traits
    {
        typedef VTable4<R, A0, A1, A2, A3> BaseType;

        typedef R (BaseType::* FunType)(A0, A1, A2, A3);
        typedef BaseType *     ObjType;
    };

    /**/

    template < typename R, typename A0 >
    struct Function0 : public StorageBase < Function0Traits < R > >
    {
        typedef StorageBase < Function0Traits < R > >  Storage;

        template < typename Functor >
        Function0(const Functor f)
        : Storage(f) {};

        template < typename Functor >
        Function0(Functor & f, bool m)
        : Storage(f, m) {};

        Function0(const typename Function0Traits < R >::FunType * m)
        : Storage(m) {};

        Function0() {};

        R operator()(void)
        {
            if (reinterpret_cast<void *>(Storage::_object) == 0)
                throw EmptyFunction();

            return ((Storage::_object)->*(Storage::_function))();
        }

        template < typename Object >
        R operator()(Object * object)
        {
            if (reinterpret_cast<void *>(Storage::_function) == 0)
                throw EmptyFunction();

            if (reinterpret_cast<void *>(Storage::_object) != 0)
                throw NonMemberFunction();

            return (reinterpret_cast< typename Function0Traits < R >::ObjType *>(object)->*(Storage::_function))();
        }
    };

    template < typename R, typename A0 >
    struct Function1 : public StorageBase < Function1Traits < R, A0 > >
    {
        typedef StorageBase < Function1Traits < R, A0 > >  Storage;

        template < typename Functor >
        Function1(const Functor f)
        : Storage(f) {};

        template < typename Functor >
        Function1(Functor & f, bool m)
        : Storage(f, m) {};

        Function1(const typename Function1Traits < R, A0 >::FunType * m)
        : Storage(m) {};

        Function1() {};

        R operator()(A0 a0)
        {
            if (reinterpret_cast<void *>(Storage::_object) == 0)
                throw EmptyFunction();

            return ((Storage::_object)->*(Storage::_function))(a0);
        }

        template < typename Object >
        R operator()(Object * object, A0 a0)
        {
            if (reinterpret_cast<void *>(Storage::_function) == 0)
                throw EmptyFunction();

            if (reinterpret_cast<void *>(Storage::_object) != 0)
                throw NonMemberFunction();

            return (reinterpret_cast< typename Function1Traits < R, A0 >::ObjType *>(object)->*(Storage::_function))(a0);
        }
    };

    template < typename R, typename A0, typename A1 >
    struct Function2 : public StorageBase < Function2Traits < R, A0, A1 > >
    {
        typedef StorageBase < Function2Traits < R, A0, A1 > >  Storage;

        template < typename Functor >
        Function2(const Functor f)
        : Storage(f) {};

        template < typename Functor >
        Function2(Functor & f, bool m)
        : Storage(f, m) {};

        Function2(const typename Function2Traits < R, A0, A1 >::FunType * m)
        : Storage(m) {};

        Function2() {};

        R operator()(A0 a0, A1 a1)
        {
            if (reinterpret_cast<void *>(Storage::_object) == 0)
                throw EmptyFunction();

            return ((Storage::_object)->*(Storage::_function))(a0, a1);
        }

        template < typename Object >
        R operator()(Object * object, A0 a0, A1 a1)
        {
            if (reinterpret_cast<void *>(Storage::_function) == 0)
                throw EmptyFunction();

            if (reinterpret_cast<void *>(Storage::_object) != 0)
                throw NonMemberFunction();

            return (reinterpret_cast< typename Function2Traits < R, A0, A1 >::ObjType *>(object)->*(Storage::_function))(a0, a1);
        }
    };

    template < typename R, typename A0, typename A1, typename A2 >
    struct Function3 : public StorageBase < Function3Traits < R, A0, A1, A2 > >
    {
        typedef StorageBase < Function3Traits < R, A0, A1, A2 > >  Storage;

        template < typename Functor >
        Function3(const Functor f)
        : Storage(f) {};

        template < typename Functor >
        Function3(Functor & f, bool m)
        : Storage(f, m) {};

        Function3(const typename Function3Traits < R, A0, A1, A2 >::FunType * m)
        : Storage(m) {};

        Function3() {};

        R operator()(A0 a0, A1 a1, A2 a2)
        {
            if (reinterpret_cast<const void *>(Storage::_object) == 0)
                throw EmptyFunction();

            return ((Storage::_object)->*(Storage::_function))(a0, a1, a2);
        }

        template < typename Object >
        R operator()(Object * object, A0 a0, A1 a1, A2 a2)
        {
            if (reinterpret_cast<void *>(Storage::_function) == 0)
                throw EmptyFunction();

            if (reinterpret_cast<void *>(Storage::_object) != 0)
                throw NonMemberFunction();

            return (reinterpret_cast< typename Function3Traits < R, A0, A1, A2 >::ObjType *>(object)->*(Storage::_function))(a0, a1, a2);
        }
    };

    template < typename R, typename A0, typename A1, typename A2, typename A3 >
    struct Function4 : public StorageBase < Function4Traits < R, A0, A1, A2, A3 > >
    {
        typedef StorageBase < Function4Traits < R, A0, A1, A2, A3 > >  Storage;

        template < typename Functor >
        Function4(const Functor f)
        : Storage(f) {};

        template < typename Functor >
        Function4(Functor & f, bool m)
        : Storage(f, m) {};

        Function4(const typename Function4Traits < R, A0, A1, A2, A3 >::FunType * m)
        : Storage(m) {};

        Function4() {};

        R operator()(A0 a0, A1 a1, A2 a2, A3 a3)
        {
            if (reinterpret_cast<void *>(Storage::_object) == 0)
                throw EmptyFunction();

            return ((Storage::_object)->*(Storage::_function))(a0, a1, a2, a3);
        }

        template < typename Object >
        R operator()(Object * object, A0 a0, A1 a1, A2 a2, A3 a3)
        {
            if (reinterpret_cast<void *>(Storage::_function) == 0)
                throw EmptyFunction();

            if (reinterpret_cast<void *>(Storage::_object) != 0)
                throw NonMemberFunction();

            return (reinterpret_cast< typename Function4Traits < R, A0, A1, A2, A3 >::ObjType *>(object)->*(Storage::_function))(a0, a1, a2, a3);
        }
    };
};

#endif /* _FUNCTION_HPP_ */
