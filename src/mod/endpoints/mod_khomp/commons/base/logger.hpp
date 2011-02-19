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

#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#include <map>
#include <string>
#include <iostream>

#include <tagged_union.hpp>
#include <format.hpp>
#include <refcounter.hpp>
#include <flagger.hpp>

#if defined(COMMONS_LIBRARY_USING_ASTERISK) || defined(COMMONS_LIBRARY_USING_FREESWITCH)
extern "C"
{
    #include <time.h>
}
#elif defined(COMMONS_LIBRARY_USING_CALLWEAVER)
extern "C"
{
    #include <callweaver/localtime.h>
}
#endif
/*

********************************************************************************
***************************** 'Logger' user manual *****************************
********************************************************************************

* Description:

This class does the management of log messages for applications. It works with
the following axioms:

<*> There are several class of messages.
<*> There are some outputs, which may be files, sockets, or a console device.
<*> There are options for classes, for outputs and for the association of both.

The last rule also shows the order in which options are processed: first the
'classes' options are processed, then 'output' options are processed, and then
the options for the tuple '(class, output)' are processed.

The options are mapped like this:

  <class-of-message>                  -> options [prefix, flags]
  <output-sink>                       -> options [prefix]
( <class-of-message>, <output-sink> ) -> options [prefix, flags]

 - "prefix" means a fixed string prefix before the real message.
 - "flags" means auxiliary flags (DATETIME, THREADID) which are
   used to add information based on OS or process context info.

* Example of use:

typedef enum
{
    C_DBG_FUNC,
    C_DBG_LOCK,
    C_WARNING,
    C_ERROR,
    C_CLI,
}
AstClassId;

typedef enum
{
    F_CONSOLE,
    F_GENERIC,
    F_TRACE,
}
AstOutputId;

// used to indicate the console log //
struct AstConsoleLog {};

struct AstPrinter: public Logger::DefaultPrinter
{
    typedef Logger::DefaultPrinter Super;

    using namespace Tagged;

    using Super::operator()(int);

    // just 2 type of descriptors //
    typedef Union < int, Union < AstConsoleLog > > Container;

    ast_printer(std::string & msg): Super(msg) {};

    bool operator()(const AstConsoleLog & log)
    {
        ast_console_puts(_msg.c_str());
        return true;
    };

#if 0
    bool operator()(int log)
    {
        return Super::operator()(log);
    };
#endif
};

bool start_log()
{
    typedef Logger::Manager<AstClassId, AstOutputId, AstPrinter, SimpleLock> LogManager;

    LogManager logger;

    // shortcut definition //
    typedef LogManager::Option LogOption;

    FILE * log_file = fopen( "output.log", "a");

    if (!log_file)
        return false;

    logger.add( F_CONSOLE, AstConsoleLog(), "chan_khomp: ");
    logger.add( F_GENERIC, log_file);

    logger.classe( C_WARNING )
        & LogOption(F_CONSOLE, "WARNING: ", LogOption::Flags(LogOption::Flag(LogOption::DATETIME)))
        & LogOption(F_GENERIC, "W: ", LogOption::Flags(LogOption::Flag(LogOption::DATETIME)))

    logger.classe( C_DBG_LOCK ).enabled(false);

    logger.classe( C_DBG_LOCK )
        & LogOption(F_GENERIC, "L: ", LogOption::Flags
            (LogOption::flag_type(LogOption::ENABLED) &
             LogOption::flag_type(LogOption::DATETIME))

    logger(C_WARNING, "eu sou uma mensagem de warning");

    logger.classe(C_WARNING).set(F_GENERIC, LogOption::ENABLED, true);
    logger.classe(C_WARNING).set(F_CONSOLE, LogOption::ENABLED, false);

    logger.classe(C_CLI).prefix("<K>");

    return true;
}

void message_the_user(int fd)
{
    logger(C_CLI, fd, "eu sou uma mensagem de cli!");
    logger(C_WARNING, "eu sou um warning");
}

********************************************************************************
********************************************************************************

Now, the code..!

*/

#ifndef _LOGGER_HPP_
#define _LOGGER_HPP_

#include <tagged_union.hpp>

struct Logger
{
    /*** a base struct for printing messages in many ways ***/

    struct DefaultPrinter
    {
        typedef Tagged::Union < int, Tagged::Union < FILE *, Tagged::Union < std::ostream * > > >  BaseType;

        typedef bool ReturnType;

        DefaultPrinter(std::string & msg): _msg(msg) {};

        bool operator()(std::ostream * out)
        {
            (*out) << _msg;
            out->flush();

            return out->good();
        }

        bool operator()(FILE * out)
        {
            if (fputs(_msg.c_str(), out) < 0)
                return false;

            if (fflush(out) < 0)
                return false;

            return true;
        }

        bool operator()(int out)
        {
#ifndef KWIN32
            return (write(out, _msg.c_str(), _msg.size()) == (int)_msg.size());
#else
            // no need for file descriptors on windows
            return false;
#endif
        }

        std::string & msg() { return _msg; }

     protected:
        std::string & _msg;
    };

    /*** manage the printing of messages ***/

    template <class ClassId, class OutputId, class Printer, class LockType>
    struct Manager
    {
        typedef  typename Printer::BaseType       BaseType;

     protected:
         /* holds a stream, and an optimal message prefix */
        struct OutputOptions
        {
            OutputOptions(BaseType & stream, std::string & prefix)
            : _stream(stream), _prefix(prefix) {};

            BaseType     _stream;
            std::string  _prefix;
            LockType     _lock;
        };

        typedef  std::map < OutputId, OutputOptions > OutputMap;

     public:

        /* print in a specific 'message class' */
        struct ClassType
        {
            ClassType(void)
            : _enabled(true)
            {};

//            ClassType(ClassType & o)
//            : _stream_map(o._stream_map), _prefix(o.prefix),
//              _lock(o._lock),_enabled(o._enabled)
//            {};

            /* initializes the options of the (class, stream) pair */
            struct Option
            {
                typedef enum { ENABLED, DATETIME, THREADID, DATETIMEMS } EnumType;

                typedef Flagger< EnumType >             Flags;
                typedef typename Flags::InitFlags   InitFlags;

                Option(OutputId output, const char * prefix,
                       Flags flags = InitFlags(ENABLED))
                : _output(output), _prefix(prefix), _flags(flags) {};

                Option(OutputId output, std::string prefix,
                       Flags flags = InitFlags(ENABLED))
                : _output(output), _prefix(prefix), _flags(flags) {};

                Option(OutputId output,
                    Flags flags = InitFlags(ENABLED))
                : _output(output), _flags(flags) {};

                OutputId     _output;
                std::string  _prefix;
                Flags        _flags;
            };

         protected:

             /* holds a prefix and a activation status */
            struct OptionContainer
            {
                OptionContainer(std::string prefix, typename Option::Flags flags)
                : _prefix(prefix), _flags(flags) {};

                std::string            _prefix;
                typename Option::Flags _flags;
            };

            typedef std::multimap < OutputId, OptionContainer > OptionMap;

            /* utility function for printing */
            bool print(std::string & msg, BaseType & stream, LockType & lock)
            {
                lock.lock();

                Printer p(msg);
                bool ret = stream.visit(p);

                lock.unlock();

                return ret;
            };

/*
            bool print(std::string & msg, BaseType & stream, LockType & lock)
            {
                lock.lock();

                Printer p(msg);
                bool ret = stream.visit(p);

                lock.unlock();

                return ret;
            };
*/

         public:
            ClassType & operator&(const Option & value)
            {
                add(value._output, value._prefix, value._flags);
                return *this;
            }

            void add(OutputId output_id, std::string prefix,
                typename Option::Flags      flags)
            {
                typedef std::pair < OutputId, OptionContainer > pair_type;
                _stream_map.insert(pair_type(output_id, OptionContainer(prefix, flags)));
            }

            /* get and set methods for active mode */
            void set(OutputId id, typename Option::EnumType flag, bool value = true)
            {
                typename OptionMap::iterator iter = _stream_map.find(id);

                if (iter == _stream_map.end())
                    return;

                (*iter).second._flags.set(flag, value);
            }

            bool get(OutputId idx, typename Option::EnumType flag)
            {
                typename OptionMap::iterator iter = _stream_map.find(idx);

                if (iter == _stream_map.end())
                    return false;

                return (*iter).second._flags.is_set(flag);
            }

            /* get/adjust the enable/disable value for the class */
            void  enabled(bool enabled) { _enabled = enabled; };
            bool  enabled()             { return _enabled;    };

            /* get/adjust the classe prefix */
            void          prefix(const char  * prefix) { _prefix = prefix; }
            void          prefix(std::string & prefix) { _prefix = prefix; }
            std::string & prefix()                     { return _prefix;   }

            /* printing function (operator, actually) */
            bool operator()(OutputMap & out_map, std::string & msg)
            {
                if (!_enabled)
                    return true;

                typedef typename OptionMap::iterator Iter;

                bool ret = true;

                for (Iter iter = _stream_map.begin(); iter != _stream_map.end(); iter++)
                {
                    OptionContainer & opt = (*iter).second;

                    if (!opt._flags[Option::ENABLED])
                        continue;

                    typename OutputMap::iterator out_iter = out_map.find((*iter).first);

                    /* this stream have been added already? if not, skip! */
                    if (out_iter == out_map.end())
                        continue;

                    /* final message */
                    std::string out_msg;

                    if (opt._flags[Option::DATETIME])
                    {
#if defined(COMMONS_LIBRARY_USING_ASTERISK) || defined(COMMONS_LIBRARY_USING_CALLWEAVER) || defined(COMMONS_LIBRARY_USING_FREESWITCH)
                        time_t      tv;
                        struct tm   lt;

                        time (&tv);
                        localtime_r (&tv, &lt);

                        out_msg += STG(FMT("[%02d-%02d-%02d %02d:%02d:%02d] ")
                            % (lt.tm_year % 100) % (lt.tm_mon + 1) % lt.tm_mday % lt.tm_hour
                            % lt.tm_min % lt.tm_sec);
#endif
                    }

                    if (opt._flags[Option::DATETIMEMS])
                    {
#if defined(COMMONS_LIBRARY_USING_ASTERISK) || defined(COMMONS_LIBRARY_USING_CALLWEAVER) || defined(COMMONS_LIBRARY_USING_FREESWITCH)
                        time_t      tv;
                        struct tm   lt;

                        time (&tv);
                        localtime_r (&tv, &lt);

                        out_msg += STG(FMT("[%02d-%02d-%02d %02d:%02d:%02d:%04d] ")
                            % (lt.tm_year % 100) % (lt.tm_mon + 1) % lt.tm_mday % lt.tm_hour % lt.tm_min
                            % lt.tm_sec % (tv * 1000));
#endif
                    }

                    OutputOptions & out_opt = (*out_iter).second;

                    if (opt._flags[Option::THREADID])
                    {
#if defined (COMMONS_LIBRARY_USING_ASTERISK) || defined(COMMONS_LIBRARY_USING_CALLWEAVER) || defined(COMMONS_LIBRARY_USING_FREESWITCH)
                        out_msg += STG(FMT("%08x ") % ((unsigned long)pthread_self()));
#endif
                    }

                    out_msg += _prefix;
                    out_msg += out_opt._prefix;
                    out_msg += opt._prefix;
                    out_msg += msg;
                    out_msg += "\n";

                    ret |= print(out_msg, out_opt._stream, out_opt._lock);
                }

                return ret;
            }

            bool operator()(BaseType & stream, std::string & msg)
            {
                std::string final_msg;

                final_msg += _prefix;
                final_msg += msg;
                final_msg += "\n";

                return print(final_msg, stream, _lock);
            }

         protected:
            OptionMap    _stream_map;
            std::string  _prefix;
            LockType     _lock;
            bool         _enabled;
        };

        /* util declaration */
        typedef typename ClassType::Option  Option;

        /* class_id_type -> ClassType mapper */
        typedef std::map < ClassId, ClassType >  ClassMap;

        /* local option pair */
        typedef std::pair < OutputId, OutputOptions >  OutputOptionPair;

        void add(OutputId output, BaseType stream, const char * prefix = "")
        {
            std::string str_prefix(prefix);

            _output_map.insert(OutputOptionPair(output, OutputOptions(stream, str_prefix)));
        }

        void add(OutputId output, BaseType stream, std::string prefix)
        {
            _output_map.insert(OutputOptionPair(output, OutputOptions(stream, prefix)));
        }

        ClassType & classe(ClassId classeid)
        {
            return _classe_map[classeid];
        }

        bool operator()(ClassId classeid, const char * msg)
        {
            std::string str_msg(msg);
            return _classe_map[classeid](_output_map, str_msg);
        }

        bool operator()(ClassId classeid, std::string & msg)
        {
            return _classe_map[classeid](_output_map, msg);
        }

        bool operator()(ClassId classeid, Format fmt)
        {
            std::string str_fmt = STG(fmt);
            return _classe_map[classeid](_output_map, str_fmt);
        }

        bool operator()(ClassId classeid, BaseType stream, const char * msg)
        {
            std::string str_msg(msg);
            return _classe_map[classeid](stream, str_msg);
        }

        bool operator()(ClassId classeid, BaseType stream, std::string & msg)
        {
            return _classe_map[classeid](stream, msg);
        }

        bool operator()(ClassId classeid, BaseType stream, Format fmt)
        {
            std::string str_fmt = STG(fmt);
            return _classe_map[classeid](stream, str_fmt);
        }

     protected:
        ClassMap   _classe_map;
        OutputMap  _output_map;
    };

 private:
    Logger();
};

#endif /* _LOGGER_HPP_ */
