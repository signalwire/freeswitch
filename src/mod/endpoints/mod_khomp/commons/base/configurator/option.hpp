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

#include <iostream>

#include <string>
#include <list>
#include <vector>

#include <format.hpp>
#include <strings.hpp>

#include <configurator/restriction.hpp>

#ifndef _CONFIG_OPTION_HPP_
#define _CONFIG_OPTION_HPP_

struct Option
{
    enum FlagTypes
    {
        F_MODIFIED = 0x0, /* if option was modified */
        F_ADJUSTED = 0x1, /* if option was correctly formated */
    };

    struct Flags: public std::vector<bool>
    {
        Flags(): std::vector<bool>(2) {};
    };

    typedef Restriction::Value   Value;
    typedef Restriction::Vector Vector;

    /* exception */
    struct InvalidDefaultValue: public std::runtime_error
    {
        InvalidDefaultValue(const std::string & name, const std::string & value)
        : std::runtime_error(STG(FMT("invalid default value '%s' for option '%s'") % value % name)),
           _name(name), _value(value)
        {};

        ~InvalidDefaultValue() throw ()
        {};

        const std::string &  name() const { return _name;  };
        const std::string & value() const { return _value; };

      protected:
        const std::string _name;
        const std::string _value;
    };

    Option(const std::string & name, const std::string & desc, const std::string & defvalue, const Restriction & restriction)
    : _name(name), _description(desc), _restriction(restriction), _modified(true)
    {
//        std::string value(defvalue);

        if (!(set(defvalue)[F_ADJUSTED]))
            throw InvalidDefaultValue(name, defvalue);
    }

    const std::string &        name() const { return _name;        };
    const std::string & description() const { return _description; };

    const Restriction & restriction() const { return _restriction; };
    bool                   modified() const { return _modified;    };

 public:
    bool    load(const std::string &);
    bool  change(const std::string &);
    bool   store(std::string &) const;

//    Flags   set(const char  *);
    Flags   set(const Value  &);
    Flags   set(const Vector &);

    bool    get(Value  &) const;
    bool    get(Vector &) const;

    bool    equals(const std::string &) const;

 protected:
    const std::string   _name;
    const std::string   _description;

    Restriction         _restriction;
    bool                _modified;
};

#endif /* _CONFIG_OPTION_HPP_ */
