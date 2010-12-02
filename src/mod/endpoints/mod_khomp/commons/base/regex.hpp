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

#include <sys/types.h>
#include <limits.h>
#include <stdarg.h>
#include <string.h>
#include <regex.h>

#include <iostream>
#include <string>
#include <stdexcept>
#include <map>

#include <refcounter.hpp>
#include <noncopyable.hpp>

#ifndef _REGEX_HPP_
#define _REGEX_HPP_

struct Regex
{
    enum
    {
        E_EXTENDED         = REG_EXTENDED,
        E_IGNORE_CASE      = REG_ICASE,
        E_NO_SUB_MATCH     = REG_NOSUB,
        E_EXPLICIT_NEWLINE = REG_NEWLINE,
    };

    enum
    {
        M_NO_BEGIN_OF_LINE    = REG_NOTBOL,
        M_NO_END_OF_LINE      = REG_NOTEOL,
    };

    enum
    {
        /* mark replace for full match (submatch "0"). */
        REP_BASE             = 0u,
        /* mark global string for replace. */
        REP_ALL              = UINT_MAX,
    };

    typedef std::pair < unsigned int, std::string >    ReplacePair;
    typedef std::map  < unsigned int, std::string >     ReplaceMap;

    struct Expression : public NonCopyable
    {
        Expression(const char * expression, unsigned int flags = 0)
        :  _expression(expression), _alloced(false),
           _subcounter(0), _errorstate(INT_MAX), _flags(flags)
        {
            initialize();
        }

        Expression(std::string & expression, unsigned int flags = 0)
        :  _expression(strdup(expression.c_str())), _alloced(true),
           _subcounter(0), _errorstate(INT_MAX), _flags(flags)
        {
            initialize();
        }

        ~Expression()
        {
            if (_errorstate != INT_MAX)
                regfree(&_comp_regex);

            if (_alloced)
            {
                delete _expression;
                _expression = 0;
            }
        }

        bool               valid(void) const { return (_errorstate == 0); }

        unsigned int    subcount(void) const { return  _subcounter; }
        const regex_t *     repr(void) const { return &_comp_regex; }

        std::string error(void) const
        {
            switch (_errorstate)
            {
                case 0:       return "";
                case INT_MAX: return "uninitialized";
                default:      return regerror_as_string();
            }
        }

     private:
        std::string regerror_as_string(void) const;

     private:
        void initialize(void);

     protected:
        const char   * _expression;
        const bool     _alloced;

        unsigned int   _subcounter;

        int            _errorstate;
        regex_t        _comp_regex;

        const unsigned int _flags;
    };

    struct Match: COUNTER_SUPER(Match)
    {
        Match(const char * basestring, const Expression & expression, unsigned int flags = 0)
        : _basestring(basestring), _expression(expression), _subcounter(0), _submatches(0),
           _have_match(false), _flags(flags)
        {
            initialize();
        }

        Match(const std::string & basestring, const Expression & expression, unsigned int flags = 0)
        : _basestring(basestring), _expression(expression), _subcounter(0), _submatches(0),
          _have_match(false), _flags(flags)
        {
            initialize();
        }

        Match(const Match & o)
        : COUNTER_REFER(o, Match),
          _basestring(o._basestring), _expression(o._expression),
          _subcounter(o._subcounter), _submatches(o._submatches),
          _have_match(o._have_match), _flags(o._flags)
        {
        }

        void unreference()
        {
            delete[] _submatches;
            delete[] _subcaching;

            _submatches = 0;
            _subcaching = 0;
        }

        bool matched(void)
        {
            return _have_match;
        }

        bool matched(unsigned int number)
        {
            if (_have_match && number < _subcounter)
                return (_submatches[number].rm_so != -1);

            return false;
        }

        const std::string & submatch(int number)
        {
            if (!matched(number))
                return _subcaching[_subcounter - 1 /* invalid, always empty! */ ];

            if (_subcaching[number].empty())
            {
                _subcaching[number].assign(_basestring, _submatches[number].rm_so,
                    _submatches[number].rm_eo - _submatches[number].rm_so);
            }

            return _subcaching[number];
        }

        const std::string & operator[](int number)
        {
            return submatch(number);
        }

        /**
        * \brief replaces strings matched by parentesis
        * \param each item of the vector is a parentesis replaced
        * \return string replaced
        * \note The overload method match only one string in parentesis.
        * \author Eduardo Nunes Pereira
        *
        * If fails the empty string is returned.
        */
        std::string replace(ReplaceMap &);
        std::string replace(std::string, unsigned int index = REP_BASE);

        // NOTE: there is already a way to get subcount defined on EXPRESSION class!

     private:
        void initialize(void);

     protected:
        const std::string    _basestring;
        const Expression   & _expression;

        unsigned int   _subcounter;
        regmatch_t   * _submatches;
        std::string  * _subcaching;
        bool           _have_match;

        const unsigned int  _flags;
    };
};

#endif /* _REGEX_HPP_ */

