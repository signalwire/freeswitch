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

#include <config_options.hpp>

void Config::Restriction::checkRange(const std::string & name, const SIntType value, const Range < SIntType > & range)
{
    if (value < range.minimum)
        throw Failure(STG(FMT("value '%d' out-of-range for option '%s' (too low)") % value % name));

    if (value > range.maximum)
        throw Failure(STG(FMT("value '%d' out-of-range for option '%s' (too high)") % value % name));

    if (((value - range.minimum) % range.step) != 0)
        throw Failure(STG(FMT("value '%d' out-of-range for option '%s' (outside allowed step)") % value % name));
}

void Config::Restriction::checkRange(const std::string & name, const UIntType value, const Range < UIntType > & range)
{
    if (value < range.minimum)
        throw Failure(STG(FMT("value '%d' out-of-range for option '%s' (too low)") % value % name));

    if (value > range.maximum)
        throw Failure(STG(FMT("value '%d' out-of-range for option '%s' (too high)") % value % name));

    if (((value - range.minimum) % range.step) != 0)
        throw Failure(STG(FMT("value '%d' out-of-range for option '%s' (outside allowed step)") % value % name));
}

void Config::Restriction::checkStringSet(const std::string & name, const StringType & value, const StringSet & allowed)
{
    if (allowed.empty())
        return;

    if (allowed.find(value) != allowed.end())
        return;

    std::string strlist;

    for (StringSet::const_iterator i = allowed.begin(); i != allowed.end(); i++)
    {
        strlist += " '";
        strlist += (*i);
        strlist += "'";
    }

    throw Failure(STG(FMT("value '%s' not allowed for option '%s' (allowed values:%s)")
        % value % name % strlist));
}

Config::Option::Option(std::string name, Config::Option::StringMemberType value, const StringType defvalue, StringSet & allowed, bool listme)
: _myname(name), _option(InnerStringType(name, value, defvalue, allowed)), _listme(listme), _values(NULL)
{};

Config::Option::Option(std::string name, Config::Option::StringMemberType value, const StringType defvalue, bool listme)
: _myname(name), _option(InnerStringType(name, value, defvalue)), _listme(listme), _values(NULL)
{};

Config::Option::Option(std::string name, Config::Option::BooleanMemberType value, const BooleanType defvalue, bool listme)
: _myname(name), _option(InnerBooleanType(name, value, defvalue)), _listme(listme), _values(NULL)
{};

Config::Option::Option(std::string name, Config::Option::SIntMemberType value, const SIntType defvalue,
                       SIntType min, SIntType max, SIntType step, bool listme)
: _myname(name), _option(InnerSIntType(name, value, defvalue, min, max, step)), _listme(listme), _values(NULL)
{};

Config::Option::Option(std::string name, Config::Option::UIntMemberType value, const UIntType defvalue,
                       UIntType min, UIntType max, UIntType step, bool listme)
: _myname(name), _option(InnerUIntType(name, value, defvalue, min, max, step)), _listme(listme), _values(NULL)
{};

Config::Option::Option(const Config::Option & o)
: _myname(o._myname), _option(o._option), _listme(o._listme), _values(o._values)
{};

Config::Option::Option(std::string name, Config::Option::FunctionMemberType value, const StringType defvalue, StringSet & allowed, bool listme)
: _myname(name), _option(InnerFunctionType(name, value, defvalue, allowed)), _listme(listme), _values(NULL)
{};

Config::Option::Option(std::string name, Config::Option::FunctionMemberType value, const StringType defvalue, bool listme)
: _myname(name), _option(InnerFunctionType(name, value, defvalue)), _listme(listme), _values(NULL)
{};

Config::Option::~Option(void)
{
    if (_values)
    {
        for (unsigned int i = 0; _values[i] != NULL; i++)
            delete _values[i];

        delete[] _values;
        _values = NULL;
    }
};

const char ** Config::Option::values(void)
{
    if (_values != NULL)
        return _values;

    /**/ if (_option.check<InnerBooleanType>())
    {
        _values = new const char*[3];

        _values[0] = strdup("yes");
        _values[1] = strdup("no");
        _values[2] = NULL;

    }
    else if (_option.check<InnerSIntType>())
    {
        const InnerSIntType & tmp = _option.get<InnerSIntType>();

        unsigned int count = ((tmp._range.maximum - tmp._range.minimum) / tmp._range.step) + 1;
        unsigned int index = 0;

        _values = new const char*[count + 1];

        for (SIntType i = tmp._range.minimum; i <= tmp._range.maximum; i += tmp._range.step, ++index)
            _values[index] = strdup(STG(FMT("%d") % i).c_str());

        _values[index] = NULL;
    }
    else if (_option.check<InnerUIntType>())
    {
        const InnerUIntType & tmp = _option.get<InnerUIntType>();

        unsigned int count = ((tmp._range.maximum - tmp._range.minimum) / tmp._range.step) + 1;
        unsigned int index = 0;

        _values = new const char*[count + 1];

        for (UIntType i = tmp._range.minimum; i <= tmp._range.maximum; i += tmp._range.step, ++index)
            _values[index] = strdup(STG(FMT("%d") % i).c_str());

        _values[index] = NULL;
    }
    else if (_option.check<InnerStringType>())
    {
        const InnerStringType & tmp = _option.get<InnerStringType>();

        _values = new const char*[ tmp._allowed.size() + 1 ];

        unsigned int index = 0;

        for (StringSet::iterator i = tmp._allowed.begin(); i != tmp._allowed.end(); ++i, ++index)
            _values[index] = strdup((*i).c_str());

        _values[index] = NULL;
    }
    else if (_option.check<InnerFunctionType>())
    {
        const InnerFunctionType & tmp = _option.get<InnerFunctionType>();

        _values = new const char*[ tmp._allowed.size() + 1 ];

        unsigned int index = 0;

        for (StringSet::iterator i = tmp._allowed.begin(); i != tmp._allowed.end(); ++i, ++index)
            _values[index] = strdup((*i).c_str());

        _values[index] = NULL;
    }
    else
    {
        throw Failure(STG(FMT("values() not implemented for type used in option '%s'") % _myname));
    }

    return _values;
};

/*********************************/

Config::Options::Options(void)
: _values(NULL)
{};

Config::Options::~Options()
{
    if (_values)
    {
        for (unsigned int i = 0; _values[i] != NULL; i++)
            free((void*)(_values[i]));

        delete[] _values;
        _values = NULL;
    }
};

bool Config::Options::add(Config::Option option)
{
    std::pair<OptionMap::iterator, bool> ret = _map.insert(OptionPair(option.name(), option));

    return ret.second;
}

bool Config::Options::synonym(std::string equiv_opt, std::string main_opt)
{
    std::pair<SynOptionMap::iterator, bool> ret = _syn_map.insert(SynOptionPair(equiv_opt, main_opt));

    return ret.second;
}

Config::StringSet Config::Options::options(void)
{
    StringSet res;

    for (OptionMap::iterator i = _map.begin(); i != _map.end(); i++)
        res.insert(i->first);

    return res;
}

const char ** Config::Options::values(const char * name)
{
    OptionMap::iterator iter = find_option(name);

    if (iter == _map.end())
        throw Failure(STG(FMT("unknown option '%s'") % name));

    return iter->second.values();
}

const char ** Config::Options::values(void)
{
    if (_values != NULL)
        return _values;

    unsigned int count = 0;

    for (OptionMap::iterator i = _map.begin(); i != _map.end(); ++i)
        if (i->second.listme())
            ++count;

    _values = new const char*[ count + 1 ];

    unsigned int index = 0;

    for (OptionMap::iterator i = _map.begin(); i != _map.end(); ++i)
    {
        if (i->second.listme())
        {
            _values[index] = strdup(i->first.c_str());
            ++index;
        }
    }

    _values[index] = NULL;

    return _values;
}

Config::Options::OptionMap::iterator Config::Options::find_option(std::string name)
{
    SynOptionMap::iterator syn_iter = _syn_map.find(name);

    if (syn_iter != _syn_map.end())
        name = syn_iter->second;

    OptionMap::iterator iter = _map.find(name);

    return iter;
}
