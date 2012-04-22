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

#include "format.hpp"
//#include <iostream>

void FormatTraits::initialize(const char * format_string)
{
    std::string txt;

    const char * ptr = format_string;

    while (*ptr != '\0')
    {
        if (*ptr != '%')
        {
            txt += *ptr;
            ++ptr;
            continue;
        }

        const char * ptr2 = ptr+1;

        if (*ptr2 == '%')
        {
            txt += *ptr;
            ptr += 2;
            continue;
        }

        if (!txt.empty())
            push_argument(txt, T_LITERAL);

        std::string arg(1, *ptr);

        ++ptr;

        bool finished = false;

        short long_count = 0;
        short short_count = 0;

        while(*ptr != '\0' && !finished)
        {
            switch (*ptr)
            {
                case ' ':
                    // uncomplete format with ' ', make it a literal.
                    arg += *ptr;
                    push_argument(arg, T_LITERAL);
                    finished = true;
                    break;

                case '%':
                    // uncomplete format with '%', make it a literal and start a new format.
                    push_argument(arg, T_LITERAL);
                    arg += *ptr;
                    break;

                case 'h':
                    short_count = std::min<short>(short_count+1, 2);
                    long_count = 0;
                    arg += *ptr;
                    break;

                case 'l':
                    long_count = std::min<short>(long_count+1, 2);
                    short_count = 0;
                    arg += *ptr;
                    break;

                case 'd':
                case 'i':
                    arg += *ptr;
                    switch (long_count - short_count)
                    {
                        case  2:
                            push_argument(arg, T_SIGNED_LONG_LONG);
                            break;
                        case  1:
                            push_argument(arg, T_SIGNED_LONG);
                            break;
                        case  0:
                            push_argument(arg, T_SIGNED_INT);
                            break;
                        case -1:
                            push_argument(arg, T_SIGNED_SHORT);
                            break;
                        case -2:
                            push_argument(arg, T_SIGNED_SHORT_SHORT);
                            break;
                        default:
                            break;
                    }
                    finished = true;
                    break;

                case 'o':
                case 'u':
                case 'x':
                case 'X':
                    arg += *ptr;
                    switch (long_count - short_count)
                    {
                        case  2:
                            push_argument(arg, T_UNSIGNED_LONG_LONG);
                            break;
                        case  1:
                            push_argument(arg, T_UNSIGNED_LONG);
                            break;
                        case  0:
                            push_argument(arg, T_UNSIGNED_INT);
                            break;
                        case -1:
                            push_argument(arg, T_UNSIGNED_SHORT);
                            break;
                        case -2:
                            push_argument(arg, T_UNSIGNED_SHORT_SHORT);
                            break;
                        default:
                            break;
                    }
                    finished = true;
                    break;

                case 'e':
                case 'E':
                case 'f':
                case 'F':
                case 'g':
                case 'G':
                case 'a':
                case 'A':
                    arg += *ptr;
                    push_argument(arg, T_FLOAT);
                    finished = true;
                    break;

                case 'c':
                    arg += *ptr;
                    push_argument(arg, T_CHAR);
                    finished = true;
                    break;

                case 's':
                    arg += *ptr;
                    push_argument(arg, T_STRING);
                    finished = true;
                    break;

                case 'p':
                    arg += *ptr;
                    push_argument(arg, T_POINTER);
                    finished = true;
                    break;

                case 'C':
                case 'S':
                case 'm':
                case 'n': // unsupported for now.
                    arg += *ptr;
                    push_argument(arg, T_ANYTHING);
                    finished = true;
                    break;

                default:
                    arg += *ptr;
                    break;
            }

            ++ptr;
        }

        if (!arg.empty())
            push_argument(arg, T_LITERAL);
    }

    if (!txt.empty())
        push_argument(txt, T_LITERAL);
}

void FormatTraits::push_argument(std::string & data, FormatTraits::Type type)
{
//    std::cerr << "pushing type (" << type << ") with format (" << data << ")" << std::endl;

    _args.push(Argument(data, type));
    data.clear();
}

void FormatTraits::pop_argument(void)
{
    _args.pop();
}

const FormatTraits::Argument * FormatTraits::next_argument(void)
{
//    std::cerr << "size: " << _args.size() << std::endl;

    while (true)
    {
//        std::cerr << "loop size: " << _args.size() << std::endl;

        if (_args.empty())
            return NULL; // throw NoArgumentLeft();

        const Argument & top = _args.front();

        if (top.type() == T_LITERAL)
        {
//            std::cerr << "top type == LITERAL, looping..." << std::endl;
            _result += top.fmts();
            pop_argument();
        }
        else
        {
//            std::cerr << "top type: " << top.type() << std::endl;
            return &top;
        }
    }
}

/******************************************************************/

#if 0
Format::Format(const char  * format_string, bool raise_exception)
: _format(format_string), _valid(true), _raise(raise_exception)
{
    FormatTraits::initialize(format_string);
}

Format::Format(std::string   format_string, bool raise_exception)
: _format(format_string), _valid(true), _raise(raise_exception)
{
    FormatTraits::initialize(format_string.c_str());
}

/*
Format::Format(std::string & format_string, bool raise_exception)
: _format(NULL), _valid(true), _raise(raise_exception)
{
    initialize(format_string.c_str());
}
*/

void Format::mark_invalid(std::string & msg)
{
    if (_valid)
    {
        _valid = false;

        _result  = "** INVALID FORMAT: ";
        _result += msg;
        _result += " **";
    }
}

void Format::raise(void) const
{
    if (!_valid)
    {
        // call specialized class
        FormatException::raise(_result);
    }
}

bool Format::valid(void) const
{
//    raise();
    return _valid;
}

std::string Format::str()
{
    if (!valid())
        return _result;

//    try
//    {
    if (next_argument() != NULL)
    {
        std::string msg;

        msg += "too few arguments passed for format '";
        msg += _format;
        msg += "' (";
        msg += _format;
        msg += ")";

        mark_invalid(msg);

        return _result;
    }
//    catch (NoArgumentLeft e)
//    {
//        return _result;
//    }
}

#endif
