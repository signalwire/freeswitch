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

#ifndef _FORMAT_H_
#define _FORMAT_H_

#include <cstring>
#include <cstdlib>
#include <typeinfo>
#include <stdexcept>
#include <string>
#include <queue>
#include <iostream>
#include <stdio.h>

#ifdef WIN32 // WINDOWS
# include <KHostSystem.h>
#endif

struct InvalidFormat
{
    InvalidFormat(std::string _msg) : msg(_msg) {}
    const std::string msg;
};

template < bool E >
struct FormatException
{
    void raise(const std::string & msg) const
    {
        /* DO NOTHING */
    };
};

template < >
struct FormatException < true >
{
    void raise(const std::string & msg) const
    {
        throw InvalidFormat(msg);
    };
};

struct FormatTraits
{
    enum Type
    {
        T_ANYTHING = 1,

        T_SIGNED_SHORT,
        T_SIGNED_SHORT_SHORT,
        T_SIGNED_INT,
        T_SIGNED_LONG,
        T_SIGNED_LONG_LONG,

        T_UNSIGNED_SHORT,
        T_UNSIGNED_SHORT_SHORT,
        T_UNSIGNED_INT,
        T_UNSIGNED_LONG,
        T_UNSIGNED_LONG_LONG,

        T_FLOAT,
        T_CHAR,

        T_POINTER,
        T_STRING,

        T_LITERAL
    };

    struct Argument
    {
        Argument(std::string fmts, Type type)
        : _fmts(fmts), _type(type) {};

        const Type          type(void) const { return _type; }
        const std::string & fmts(void) const { return _fmts; }

     protected:
        const std::string _fmts;
        const Type        _type;
    };

    typedef std::queue < Argument > ArgumentQueue;

    //////////////////////////////////

    template < typename V >
    bool number_verify_signed_short( V value ) const
    {
        return
           ((typeid(V) == typeid(short int) ||
             typeid(V) == typeid(short) ||
             typeid(V) == typeid(const short int) ||
             typeid(V) == typeid(const short) ||
             typeid(V) == typeid(volatile short int) ||
             typeid(V) == typeid(volatile short)) &&
             sizeof(V) == sizeof(short));
    }

    template < typename V >
    bool number_verify_unsigned_short( V value ) const
    {
        return
           ((typeid(V) == typeid(unsigned short int) ||
             typeid(V) == typeid(unsigned short) ||
             typeid(V) == typeid(const unsigned short int) ||
             typeid(V) == typeid(const unsigned short) ||
             typeid(V) == typeid(volatile unsigned short int) ||
             typeid(V) == typeid(volatile unsigned short)) &&
             sizeof(V) == sizeof(unsigned short));
    }

    template < typename V >
    bool number_verify_signed_long( V value ) const
    {
        return
           ((typeid(V) == typeid(long int) ||
             typeid(V) == typeid(long) ||
             typeid(V) == typeid(const long int) ||
             typeid(V) == typeid(const long) ||
             typeid(V) == typeid(volatile long int) ||
             typeid(V) == typeid(volatile long)) &&
             sizeof(V) == sizeof(long));
    }

    template < typename V >
    bool number_verify_unsigned_long( V value ) const
    {
        return
           ((typeid(V) == typeid(unsigned long int) ||
             typeid(V) == typeid(unsigned long) ||
             typeid(V) == typeid(const unsigned long int) ||
             typeid(V) == typeid(const unsigned long) ||
             typeid(V) == typeid(volatile unsigned long int) ||
             typeid(V) == typeid(volatile unsigned long)) &&
             sizeof(V) == sizeof(long long));
    }

    template < typename V >
    bool number_verify_signed_long_long( V value ) const
    {
        return
           ((typeid(V) == typeid(long long int) ||
             typeid(V) == typeid(long long) ||
             typeid(V) == typeid(const long long int) ||
             typeid(V) == typeid(const long long) ||
             typeid(V) == typeid(volatile long long) ||
             typeid(V) == typeid(volatile long long int)) &&
             sizeof(V) == sizeof(long long));
    }

    template < typename V >
    bool number_verify_unsigned_long_long( V value ) const
    {
        return
           ((typeid(V) == typeid(unsigned long long int) ||
             typeid(V) == typeid(unsigned long long) ||
             typeid(V) == typeid(const unsigned long long int) ||
             typeid(V) == typeid(const unsigned long long) ||
             typeid(V) == typeid(volatile unsigned long long) ||
             typeid(V) == typeid(volatile unsigned long long int)) &&
             sizeof(V) == sizeof(unsigned long long));
    }

    template < typename V >
    bool number_verify_signed_int( V value ) const
    {
        return
            (sizeof(V) <= sizeof(int) ||
             typeid(V) == typeid(int) ||
             typeid(V) == typeid(const int) ||
             typeid(V) == typeid(volatile int));
    }

    template < typename V >
    bool number_verify_unsigned_int( V value ) const
    {
        return
            (sizeof(V) <= sizeof(unsigned int) ||
             typeid(V) == typeid(unsigned int) ||
             typeid(V) == typeid(const unsigned int) ||
             typeid(V) == typeid(volatile unsigned int));
    }

    template < typename V >
    bool generic_verify( V value, const Type type ) const
    {
        switch (type)
        {
            /* EXCEPTION: consider any number an valid input. */
            case T_SIGNED_INT:
            case T_UNSIGNED_INT:
                return
                    (number_verify_signed_int(value)     ||
                     number_verify_unsigned_int(value)   ||
                     number_verify_signed_long(value)    ||
                     number_verify_unsigned_long(value)  ||
                     number_verify_signed_short(value)   ||
                     number_verify_unsigned_short(value));

            case T_SIGNED_SHORT_SHORT:
                return (typeid(V) == typeid(char) || typeid(V) == typeid(const char));

            case T_SIGNED_SHORT:
                return number_verify_signed_short(value);

            case T_SIGNED_LONG:
                return number_verify_signed_long(value);

            case T_SIGNED_LONG_LONG:
                return number_verify_signed_long_long(value);

            case T_UNSIGNED_SHORT_SHORT:
                return (typeid(V) == typeid(unsigned char) || typeid(V) == typeid(unsigned char));

            case T_UNSIGNED_SHORT:
                return number_verify_unsigned_short(value);

            case T_UNSIGNED_LONG:
                return number_verify_unsigned_long(value);

            case T_UNSIGNED_LONG_LONG:
                return number_verify_unsigned_long_long(value);

            case T_FLOAT:
                return (typeid(V) == typeid(float)) || (typeid(V) == typeid(double) ||
                    typeid(V) == typeid(const float)) || (typeid(V) == typeid(const double));

            case T_CHAR:
                return (typeid(V) == typeid(char)) || (typeid(V) == typeid(unsigned char) ||
                    typeid(V) == typeid(const char)) || (typeid(V) == typeid(const unsigned char));

            case T_POINTER:
            case T_STRING:
                return false;

            case T_ANYTHING:
                return true;

            case T_LITERAL:
                return false;
        }

        return false;
    };

    const Argument * next_argument(void);

    void push_argument(std::string & data, const Type type);
    void pop_argument(void);

    void initialize(const char *);

  protected:
    ArgumentQueue _args;
    std::string   _result;

};

template < bool E = false >
struct FormatBase: protected FormatTraits, protected FormatException < E >
{
    static const unsigned int strings_base_length = 64;
    static const unsigned int generic_base_length = 64;

    explicit FormatBase(const char  * format_string)
    : _format(format_string), _valid(true)
    {
        FormatTraits::initialize(format_string);
    };

    explicit FormatBase(std::string   format_string)
    : _format(format_string), _valid(true)
    {
        FormatTraits::initialize(format_string.c_str());
    };

    bool valid(void) const
    {
        return _valid;
    }

    const std::string str()
    {
        if (valid() && (next_argument() != NULL))
        {
            std::string msg;

            // TODO: why format appears two times?
            msg += "too few arguments passed for format '";
            msg += _format;
            msg += "' (";
            msg += _format;
            msg += ")";

            mark_invalid(msg);
        }

        raise();
        return _result;
    };

    ////////////////////////////////////////////////////////////

    template < typename V >
    FormatBase & operator%( V value )
    {
        if (!valid())
            return *this;

        const Argument * top = next_argument();

        if (top == NULL)
        {
            std::string msg;

            msg += "too many arguments passed for format '";
            msg += _format;
            msg += "'";

            mark_invalid(msg);
        }
        else
        {
            char temp[generic_base_length];

            if (!FormatTraits::generic_verify(value, top->type()))
            {
                std::string msg;

                msg += "type mismatch: got type '";
                msg += typeid(value).name();
                msg += "' in format '";
                msg += top->fmts();
                msg += "' (";
                msg += _format;
                msg += ")";

                mark_invalid(msg);
                return *this;
            }

            snprintf(temp, sizeof(temp), top->fmts().c_str(), value);
            _result += temp;

            pop_argument();
        }

        raise();
        return *this;
    }

    template < typename V >
    FormatBase & operator%( V * value )
    {
        if (!valid())
            return *this;

        const Argument * top = next_argument();

        if (top == NULL)
        {
            std::string msg;

            msg += "too many arguments passed for format '";
            msg += _format;
            msg += "'";

            mark_invalid(msg);
        }
        else
        {
            switch (top->type())
            {
                case T_POINTER:
                {
                    char temp[generic_base_length];
                    snprintf(temp, sizeof(temp), top->fmts().c_str(), value);
                    _result += temp;
                    break;
                }

                case T_STRING:
                {
                    if ((typeid(const char)          == typeid(V)) ||
                        (typeid(char)                == typeid(V)) ||
                        (typeid(const unsigned char) == typeid(V)) ||
                        (typeid(unsigned char)       == typeid(V)) ||
                        (typeid(const void)          == typeid(V)) ||
                        (typeid(void)                == typeid(V)))
                    {
                        int len = strlen((const char*)value)+strings_base_length+1;

                        char * temp = new char[len];

                        snprintf(temp, len, top->fmts().c_str(), value);
                        _result += temp;

                        delete[] temp;
                    }
                    else
                    {
                        std::string msg;

                        msg += "type mismatch: got type '";
                        msg += typeid(value).name();
                        msg += "' in string format (";
                        msg += _format;
                        msg += ")";

                        mark_invalid(msg);
                    }
                    break;
                }

                default:
                {
                    std::string msg;

                    msg += "type mismatch: got pointer/string type in format '";
                    msg += top->fmts();
                    msg += "' (";
                    msg += _format;
                    msg += ")";

                    mark_invalid(msg);
                    break;
                }
            }

            pop_argument();
        }

        raise();
        return *this;
    }

    FormatBase & operator%( const std::string value )
    {
        if (!valid())
            return *this;

        const Argument * top = next_argument();

        if (top == NULL)
        {
            std::string msg;

            msg += "too many arguments passed for format '";
            msg += _format;
            msg += "'";

            mark_invalid(msg);
        }
        else
        {
            if (top->type() == T_STRING)
            {
                int len = value.length()+strings_base_length+1;

                char * temp = new char[len];

                snprintf(temp, len, top->fmts().c_str(), value.c_str());
                _result += temp;

                delete[] temp;
            }
            else
            {
                std::string msg;

                msg += "type mismatch: got string type in format '";
                msg += top->fmts();
                msg += "' (";
                msg += _format;
                msg += ")";

                mark_invalid(msg);
            }

            pop_argument();
        }

        raise();
        return *this;
    }

  protected:
    void mark_invalid(std::string & msg)
    {
        if (_valid)
        {
            _valid = false;

            _result  = "** INVALID FORMAT: ";
            _result += msg;
            _result += " **";
        }
    }

    void raise(void) const
    {
        if (!_valid)
        {
            // call specialized class
            FormatException< E >::raise(_result);
        }
    }

 private:
    const std::string _format;
    bool              _valid;
};

/* useful typedef for general usage (not generating exceptions) */
typedef FormatBase<> Format;

/* macros used for shortening lines and making the code clearer */
#define STG(x) (x).str()
#define FMT(x) Format(x)

#endif /* _FORMAT_H_ */
