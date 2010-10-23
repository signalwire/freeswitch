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

#include <refcounter.hpp>

#include <typeinfo>

#ifndef _VARIANT_H_
#define _VARIANT_H_

/* this is internal, should not be used by the user */
struct NoArgumentDefined {};

template < typename UserReturnType, typename UserArgumentType = NoArgumentDefined >
struct VariantBaseType
{
    typedef UserReturnType       ReturnType;
    typedef UserArgumentType   ArgumentType;

    virtual ~VariantBaseType() {};

    virtual int which() = 0;

    virtual ReturnType visit(void)           { return ReturnType(); };
    virtual ReturnType visit(ArgumentType)   { return ReturnType(); };
};

template < typename BaseType = VariantBaseType < void > >
struct Variant: NEW_REFCOUNTER(Variant < BaseType >)
{
    typedef typename BaseType::ReturnType        ReturnType;
    typedef typename BaseType::ArgumentType    ArgumentType;

    struct InvalidType {};

    Variant(BaseType * value, bool is_owner = false)
    : _value(value), _is_owner(is_owner) {};

    Variant(const Variant & v)
    : INC_REFCOUNTER(v, Variant < BaseType >),
      _value(v._value), _is_owner(v._is_owner) {};

    virtual ~Variant() {};

    void unreference()
    {
        if (_is_owner && _value)
        {
            delete _value;
            _value = 0;
        }
    };

    template < typename ValueType >
    ValueType & get(void)
    {
        try
        {
            ValueType & ret = dynamic_cast < ValueType & > (*_value);
            return ret;
        }
        catch (std::bad_cast & e)
        {
            throw InvalidType();
        }
    };

    int which()
    {
        return _value->which();
    }

    ReturnType visit(void)
    {
        return _value->visit();
    }

    ReturnType visit(ArgumentType arg)
    {
        return _value->visit(arg);
    }

 protected:
    BaseType * _value;
    bool       _is_owner;
};

#endif /* _VARIANT_H_ */

