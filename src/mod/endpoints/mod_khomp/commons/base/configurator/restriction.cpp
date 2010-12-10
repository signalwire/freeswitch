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

#include <math.h>
#include <string.h>

#include <iostream>
#include <strings.hpp>

#include <configurator/restriction.hpp>

/* internal helper! */
bool Restriction::equalNumber(const double a, const double b)
{
    char tmp1[64];
    char tmp2[64];

    snprintf(tmp1, sizeof(tmp1), "%.3f", a);
    snprintf(tmp2, sizeof(tmp2), "%.3f", b);

    if (strncmp(tmp1, tmp2, sizeof(tmp1)))
        return false;

    return true;
}

/* process value to our internal representation */

bool Restriction::process(Restriction::Format fmt,
        const Restriction::Value & value, Restriction::Value & final) const
{
    switch (_bounds)
    {
        case B_RANGE:
        {
            if (_kind != K_NUMBER)
                return false;

            std::string tmpvalue;

            Restriction::Value::const_iterator itr = value.begin();
            Restriction::Value::const_iterator end = value.end();

            tmpvalue.reserve(value.size());

            // f*cking dot/comma notation!
            for (; itr != end; ++itr)
                tmpvalue += ((*itr) != ',' ? (*itr) : '.');

            try
            {
                double newvalue = Strings::todouble(tmpvalue);

                if (newvalue < _init && newvalue > _fini)
                    return false;

                double res = (newvalue - _init) / _step;

                if (!Restriction::equalNumber(res, rint(res)))
                    return false;

                final = value;
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        case B_LIST:
            for (List::const_iterator i = _list.begin(); i != _list.end(); i++)
            {
                const Value & tmp = (*i);

                if (tmp == value)
                {
                    final = value;
                    return true;
                }
            }
            return false;

        case B_MAPS:
            switch (fmt)
            {
                case F_USER:
                {
                    Map::const_iterator i = _map_from_usr.find(value);

                    if (i == _map_from_usr.end())
                        return false;

                    const Value & tmp = (*i).second;

                    final = tmp;
                    return true;
                }

                case F_FILE:
                {
                    Map::const_iterator i = _map_from_cfg.find(value);

                    if (i == _map_from_cfg.end())
                        return false;

                    final = value;
                    return true;
                }

                default:
                    break;
            }
            return false;

        case B_FREE:
            final = value;
            return true;

        default:
            break;
    }

    return false;
}

/* unprocess the value (outputs the external representation) */

bool Restriction::unprocess(Restriction::Format fmt,
        const Restriction::Value & value, Restriction::Value & final) const
{
    switch (_bounds)
    {
        case B_MAPS:

            switch (fmt)
            {
                case F_USER:
                {
                    Map::const_iterator i = _map_from_cfg.find(value);

                    if (i == _map_from_cfg.end())
                        return false;

                    final = (*i).second;
                    return true;
                }
                default:
                    break;
            }

        default:
            final = value;
            return true;
    }
}

/***************************** *****************************/

bool Restriction::get(Restriction::Format fmt, Restriction::Value & value) const
{
    if (_numeral != N_UNIQUE)
        return false;

    if (!unprocess(fmt, _value._unique, value))
        return false;

    return true;
}

bool Restriction::get(Restriction::Format fmt, Restriction::Vector & values) const
{
    if (_numeral != N_MULTIPLE)
        return false;

    const List & my_values = _value._multiple;

    for (List::const_iterator i = my_values.begin(); i != my_values.end(); i++)
    {
        const Value & value = (*i);

        Value   final;

        if (!unprocess(fmt, value, final))
            return false;

        values.push_back(final);
    };

    return true;
}

/***************************** *****************************/

bool Restriction::set(Restriction::Format fmt, const Restriction::Value & value)
{
    switch (_numeral)
    {
        case N_UNIQUE:
        {
            Value final;

            if (!constThis().process(fmt, value, final))
                return false;

            _value._unique = final;
            return true;
        }

        case N_MULTIPLE:
        {
            if (value == "@" || value == "#" || value == "")
            {
                _value._multiple.clear();
                return true;
            }

            Strings::vector_type values;
            Strings::tokenize(value, values, ",");

            return set(fmt, values);
        }

        default:
            return false;
    }
}

bool Restriction::set(Restriction::Format fmt, const Restriction::Vector & values)
{
    if (_numeral != N_MULTIPLE)
        return false;

    if (values.empty())
    {
        _value._multiple.clear();
    }
    else
    {
        /* list needed to store temporary values */
        List finals;

        for (Vector::const_iterator i = values.begin(); i != values.end(); i++)
        {
            const Value & value = (*i);

            Value   final;

            if (!constThis().process(fmt, value, final))
                return false;

            finals.push_back(final);
        }

        List & lst = _value._multiple;

        /* need to clear values set before */
        lst.clear();

        for (List::iterator i = finals.begin(); i != finals.end(); i++)
        {
            Value value = (*i);
            lst.push_back(value);
        }
    };

    return true;
}

/***************************** *****************************/

void Restriction::allowed(Restriction::Vector & vals) const
{
    switch (_bounds)
    {
        case B_FREE:
            return;

        case B_LIST:
            for (List::const_iterator i = _list.begin(); i != _list.end(); i++)
                vals.push_back(*i);
            break;

        case B_MAPS:
            for (Map::const_iterator i = _map_from_usr.begin(); i != _map_from_usr.end(); i++)
                vals.push_back(i->first);
            break;

        case B_RANGE:
        {
            if (_kind != K_NUMBER)
                return;

            // is there any fraction?
            bool has_fraction =
                (!Restriction::equalNumber(_init, rint(_init))) ||
                (!Restriction::equalNumber(_fini, rint(_fini))) ||
                (!Restriction::equalNumber(_step, rint(_step)));

            const char * format = (has_fraction ? "%.2f" : "%02.0f");

            for (double i = _init; i <= _fini; i += _step)
            {
                char tmp[32];
                snprintf(tmp, sizeof(tmp), format, i);
                vals.push_back(std::string(tmp));
            }
            break;
        }

        default:
            break;
    }
}

void Restriction::init_class()
{
    _value._unique.clear();
    _value._multiple.clear();
}
