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

#include <limits.h>

#include <list>
#include <vector>
#include <string>

#include <types.hpp>

#include <format.hpp>

/* Generic string funcions */

#ifndef _STRINGS_HPP_
#define _STRINGS_HPP_

struct Strings
{
    typedef std::list<std::string>      list_type;
    typedef std::vector<std::string>  vector_type;

    struct Merger
    {
        void          add(std::string);

        std::string merge(const std::string &);
        std::string merge(const char *);

        bool empty() { return _list.empty(); };

        const list_type & list() { return _list; };

     protected:
        list_type   _list;
    };

 public:
    struct invalid_value
    {
        invalid_value(const char        * value): _value(value) {};
        invalid_value(const std::string & value): _value(value) {};

        std::string & value() { return _value; }

     protected:
         std::string _value;
    };

    struct not_implemented {};

    static unsigned int tokenize(const std::string &, vector_type &, const std::string & delims = ",;:",
                                 long int max_toxens = LONG_MAX, bool keep_empty = true);

    static bool        toboolean(std::string);
    static std::string fromboolean(bool);

    static long               tolong(const std::string &, int base = 10);
    static unsigned long      toulong(const std::string &, int base = 10);
    static unsigned long long toulonglong(const std::string &, int base = 10);
    static double             todouble(const std::string &);

    static long               tolong(const char *, int base = 10);
    static unsigned long      toulong(const char *, int base = 10);
    static unsigned long long toulonglong(const char *, int base = 10);
    static double             todouble(const char *);

    static std::string lower(std::string);
    static std::string hexadecimal(std::string);

    static std::string trim(const std::string&, const std::string& trim_chars = " \f\n\r\t\v");
};

#endif // _STRINGS_HPP_ //
