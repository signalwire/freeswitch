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

#ifndef _TAGGED_UNION_HPP_
#define _TAGGED_UNION_HPP_

#include <unistd.h>

#include <typeinfo>
#include <stdexcept>
#include <iostream>

#include <format.hpp>

namespace Tagged
{
    struct EmptyUnion
    {
        friend class Value;

        EmptyUnion()
        : _adjusted(false) {};

        // copy constructor
        EmptyUnion(const EmptyUnion & o)
        : _adjusted(o._adjusted) {};

        // copy assignment operator
        EmptyUnion & operator=(const EmptyUnion & o)
        {
            _adjusted = o._adjusted;
            return *this;
        };

        ~EmptyUnion() { _adjusted = false; };

        bool operator==(const EmptyUnion & o) const
        {
            return true;
        };

      public:
        void clear(void) { _adjusted = false; };
      protected:
        void setup(void) { _adjusted = true;  };

      protected:
        bool value_set(void) const   { return false; };
        bool value_get(void) const   { return false; };

        bool value_check(void) const { return false; };

        template < typename S >
        bool value_visit(S & visitor, typename S::ReturnType & ret) const
        {
            return false;
        };

        template < typename S >
        bool value_visit_void(S & visitor) const
        {
            return false;
        };

        bool adjusted() const { return _adjusted;  };

      private:
        bool _adjusted;
    };

    template < typename V, typename E = EmptyUnion >
    struct Union: public E
    {
        friend class Value;

        // default constructor
        Union()
        : _value(0) {};

        // constructor with initializer
        template < typename U >
        Union(U value)
        : _value(0)
        {
            set(value);
        };

        // copy constructor
        Union(const Union & o)
        : E(static_cast<const E&>(o)),
          _value( (o._value ? new V(*(o._value)) : 0) )
        {};

        // copy assignment operator
        Union & operator=(const Union & o)
        {
            if (_value)
            {
                delete _value;
                _value = 0;
            }

            if (o._value)
            {
                _value = new V(*(o._value));
            }

            E::operator=(static_cast<const E&>(o));

            return *this;
        };

        // destructor
        ~Union()
        {
            if (_value)
            {
                delete _value;
                _value = 0;
            }
        };

        // equal sign operator
        template < typename U >
        void operator=(U value)
        {
            set(value);
        }

        template < typename U >
        bool check(void) const
        {
            return value_check(static_cast< const U * const>(0));
        };

        template < typename U >
        U & get(void) const
        {
            U * res = 0;

            if (!E::adjusted())
                throw std::runtime_error("tagged union empty!");

            if (!value_get(&res) || !res)
                throw std::runtime_error(STG(FMT("type mismatch when asked for '%s'") % typeid(U).name()));

            return *res;
        };

        template < typename U >
        void set(U val)
        {
            if (E::adjusted())
                clear();

            if (!value_set(val))
                throw std::runtime_error("unable to set value of invalid type");
        };

        template < typename S >
        typename S::ReturnType visit(S visitor) const
        {
            typename S::ReturnType ret;

            if (!value_visit(visitor, ret))
                throw std::runtime_error("unable to visit empty value");

            return ret;
        };

        template < typename S >
        void visit_void(S visitor) const
        {
            if (!value_visit_void(visitor))
                throw std::runtime_error("unable to visit empty value");
        };

        void clear()
        {
            if (_value)
            {
                delete _value;
                _value = 0;
            }

            E::clear();
        };

        // compare (equal) operator
        bool operator==(const Union & o) const
        {
            bool are_equal = false;

            if (!_value && !(o._value))
                are_equal = true;

            if (_value && o._value)
                are_equal = (*_value == *(o._value));

            if (are_equal)
                return E::operator==(static_cast<const E&>(o));

            return false;
        };

        // compare types
        bool sameType(const Union & o) const
        {
            if ((!(_value) && !(o._value)) || (_value && o._value))
                return E::operator==(static_cast<const E&>(o));

            return false;
        };

      protected:
        using E::value_set;
        using E::value_get;

        using E::value_check;
        using E::value_visit;
        using E::value_visit_void;

        bool value_set(V val)
        {
            _value = new V(val);
            E::setup();

            return true;
        };

        bool value_get(V ** val) const
        {
            if (!_value)
                return false;

            *val = _value;
            return true;
        }

        bool value_check(const V * const junk) const
        {
            (void)junk;
            return (_value != 0);
        };

        template < typename S >
        bool value_visit(S & visitor, typename S::ReturnType & ret) const
        {
            if (_value)
            {
                ret = visitor(*const_cast<V*>(_value));
                return true;
            };

            return E::value_visit(visitor, ret);
        };

        template < typename S >
        bool value_visit_void(S & visitor) const
        {
            if (_value)
            {
                visitor(*const_cast<V*>(_value));
                return true;
            };

            return E::value_visit_void(visitor);
        };

      private:
        V * _value;
    };
};

#endif /* _TAGGED_UNION_HPP_ */
