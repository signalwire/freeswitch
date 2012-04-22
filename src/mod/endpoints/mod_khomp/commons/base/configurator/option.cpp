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

#include <configurator/option.hpp>

bool Option::equals(const std::string & value) const
{
    switch (_restriction.numeral())
    {
        case Restriction::N_UNIQUE:
        {
            Restriction::Value my_value;

            if (!_restriction.get(Restriction::F_USER, my_value))
                return false;

            return (my_value == value);
        }
        case Restriction::N_MULTIPLE:
        {
            Restriction::Vector my_values;

            if (!_restriction.get(Restriction::F_USER, my_values))
                return false;

            for (Restriction::Vector::iterator i = my_values.begin(); i != my_values.end(); i++)
            {
                if ((*i) == value)
                    return true;
            }

            return false;
        }
    }

    return false;
}

bool Option::load(const std::string & value)
{
    bool ret = _restriction.set( (const Restriction::Format)Restriction::F_FILE, value);

    if (ret) _modified = false;

    return ret;
}

bool Option::change(const std::string & value)
{
    bool ret = _restriction.set(Restriction::F_FILE, value);

    if (ret) _modified = true;

    return ret;
}

bool Option::store(std::string & value) const
{
    switch  (_restriction.numeral())
    {
        case Restriction::N_UNIQUE:
            return _restriction.get(Restriction::F_FILE, value);

        case Restriction::N_MULTIPLE:
        {
            Restriction::Vector values;

            if (!_restriction.get(Restriction::F_FILE, values))
                return false;

            Strings::Merger strs;

            for (Restriction::Vector::iterator i = values.begin(); i != values.end(); i++)
                strs.add(*i);

            value = strs.merge(",");

            return true;
        }

        default:
            return false;
    }
}

/*
Option::Flags Option::set(const char * value)
{
    std::string str_value(value);
    return set(str_value);
}
*/

Option::Flags Option::set(const Restriction::Value & value)
{
    Restriction::Value  last_value, curr_value;
    Flags               flags;

    bool ret1 = _restriction.get(Restriction::F_USER, last_value);

    if (!_restriction.set(Restriction::F_USER, value))
        return flags;

    flags[F_ADJUSTED] = true;

    bool ret2 = _restriction.get(Restriction::F_USER, curr_value);

    if (!ret1 || (ret2 && (last_value != curr_value)))
    {
        flags[F_MODIFIED] = true;
        _modified = true;
    }

    return flags;
}

Option::Flags Option::set(const Restriction::Vector & values)
{
    Restriction::Vector  last_values, curr_values;
    Flags                flags;

    bool ret1 = _restriction.get(Restriction::F_USER, last_values);

    if (!_restriction.set(Restriction::F_USER, values))
        return flags;

    flags[F_ADJUSTED] = true;

    bool ret2 = _restriction.get(Restriction::F_USER, curr_values);

    if (!ret1 || (ret2 && (last_values != curr_values)))
    {
        flags[F_MODIFIED] = true;
        _modified = true;
    }

    return flags;
}

bool Option::get(Restriction::Value & value) const
{
    return _restriction.get(Restriction::F_USER, value);
}

bool Option::get(Restriction::Vector & values) const
{
    return _restriction.get(Restriction::F_USER, values);
}
