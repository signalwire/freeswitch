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

#include <strings.hpp>

void Strings::Merger::add(std::string s)
{
    _list.push_back(s);
};

std::string Strings::Merger::merge(const std::string & sep)
{
    list_type::iterator i = _list.begin();

    std::string res;

    if (i != _list.end())
    {
        res += (*i);
        ++i;
    };

    while (i != _list.end())
    {
        res += sep;
        res += (*i);
        ++i;
    }

    return res;
};

std::string Strings::Merger::merge(const char *sep)
{
    std::string ssep(sep);
    return merge(ssep);
}

unsigned int Strings::tokenize(const std::string & str, Strings::vector_type & tokens,
    const std::string & delims, long int max_tokens, bool keep_empty)
{
    std::string::size_type base = 0;

    std::string::size_type init = str.find_first_not_of(delims, 0);
    std::string::size_type fini = str.find_first_of(delims, init);

    long int cur_token = 1;

    while (std::string::npos != init)
    {
        if (keep_empty && base < init)
        {
            std::string::size_type cur_empty = init - base;

            while (cur_empty && cur_token < max_tokens)
            {
                tokens.push_back("");

                ++cur_token;
                --cur_empty;
            }
        }

        if (std::string::npos != fini && cur_token < max_tokens)
        {
            base = fini + 1;

            tokens.push_back(str.substr(init, fini - init));
            ++cur_token;
        }
        else
        {
            base = str.size(); // find_first_of(delims, init);

            tokens.push_back(str.substr(init, str.size() - init));
           	break;
        }

   	    init = str.find_first_not_of(delims, fini);
       	fini = str.find_first_of(delims, init);
    }

    if (keep_empty && base != str.size())
    {
        std::string::size_type cur_empty = str.size() - base + 1;

   	    while (cur_empty && cur_token < max_tokens)
       	{
            tokens.push_back("");

   	        ++cur_token;
       		--cur_empty;
        }

        if (cur_empty)
        {
            std::string::size_type pos = base + cur_empty - 1;
            tokens.push_back(str.substr(pos, str.size() - pos));
            ++cur_token;
        }
    }

    return (cur_token - 1);
}

long Strings::tolong(const std::string & str, int base)
{
    return tolong(str.c_str(), base);
}

unsigned long Strings::toulong(const std::string & str, int base)
{
    return toulong(str.c_str(), base);
}

unsigned long long Strings::toulonglong(const std::string & str, int base)
{
    return toulonglong(str.c_str(), base);
}

double Strings::todouble(const std::string & str)
{
    return todouble(str.c_str());
}

long Strings::tolong(const char * str, int base)
{
    char *str_end = 0;

    unsigned long value = strtol(str, &str_end, base);

    if (str_end && *str_end == 0)
        return value;

    throw invalid_value(str);
}

bool Strings::toboolean(std::string str)
{
    std::string tmp(str);

    Strings::lower(tmp);

    if ((tmp == "true")  || (tmp == "yes")) return true;
    if ((tmp == "false") || (tmp == "no"))  return false;

    throw invalid_value(str);
}

unsigned long Strings::toulong(const char * str, int base)
{
    char *str_end = 0;

    unsigned long value = strtoul(str, &str_end, base);

    if (str_end && *str_end == 0)
        return value;

    throw invalid_value(str);
}

unsigned long long Strings::toulonglong(const char * str, int base)
{
#if defined(_WINDOWS) || defined(_Windows) || defined(_WIN32) || defined(WIN32)
    throw not_implemented();
#else
    char *str_end = 0;

    unsigned long long value = strtoull(str, &str_end, base);

    if (str_end && *str_end == 0)
        return value;

    throw invalid_value(str);
#endif
}

double Strings::todouble(const char * str)
{
    char *str_end = 0;

    double value = strtod(str, &str_end);

    if (str_end && *str_end == 0)
        return value;

    throw invalid_value(str);
}

std::string Strings::fromboolean(bool value)
{
    if (value) return "true";
    else       return "false";
}

std::string Strings::lower(std::string str)
{
    std::string res;

    for (std::string::iterator i = str.begin(); i != str.end(); i++)
        res += tolower((*i));

    return res;
}

std::string Strings::hexadecimal(std::string value)
{
    std::string result;

    for (std::string::iterator i = value.begin(); i != value.end(); i++)
    {
        if (i != value.begin())
            result += " ";

        result += STG(FMT("%2x") % (unsigned int)(*i));
    }

    return result;
}

std::string Strings::trim(const std::string& str, const std::string& trim_chars)
{
    std::string result(str);

    result.erase( result.find_last_not_of( trim_chars ) + 1 );
    result.erase( 0, result.find_first_not_of( trim_chars ) );

    return result;
}
