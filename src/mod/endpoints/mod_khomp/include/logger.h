/*******************************************************************************

    KHOMP generic endpoint/channel library.
    Copyright (C) 2007-2010 Khomp Ind. & Com.

  The contents of this file are subject to the Mozilla Public License 
  Version 1.1 (the "License"); you may not use this file except in compliance 
  with the License. You may obtain a copy of the License at 
  http://www.mozilla.org/MPL/ 

  Software distributed under the License is distributed on an "AS IS" basis,
  WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for
  the specific language governing rights and limitations under the License.

  Alternatively, the contents of this file may be used under the terms of the
  "GNU Lesser General Public License 2.1" license (the “LGPL" License), in which
  case the provisions of "LGPL License" are applicable instead of those above.

  If you wish to allow use of your version of this file only under the terms of
  the LGPL License and not to allow others to use your version of this file 
  under the MPL, indicate your decision by deleting the provisions above and 
  replace them with the notice and other provisions required by the LGPL 
  License. If you do not delete the provisions above, a recipient may use your 
  version of this file under either the MPL or the LGPL License.

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
    along with this library; if not, write to the Free Software Foundation, 
    Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*******************************************************************************/

#ifndef _Logger_H_
#define _Logger_H_

#include <switch.h>
#include <string>
#include <simple_lock.hpp>
#include <logger.hpp>
#include <configurator/configfile.hpp>
#include <klog-config.hpp>
#include "defs.h"
#include "format.hpp"

namespace K
{
    struct LogConfig
    {
        static bool set(Logfile &, const char *, const char *, bool);
        static bool commit(Logfile &);
    };

    struct SwitchConsoleLog {}; 

    struct SwitchPrinter: public Logger::DefaultPrinter
    {
        typedef Logger::DefaultPrinter Super;
        typedef Tagged::Union < std::ostream *, Tagged::Union < int, Tagged::Union < SwitchConsoleLog > > > BaseType;

        SwitchPrinter(std::string & msg): Super(msg) {}; 

        using Super::operator();

        bool operator()(const SwitchConsoleLog & ignored)
        {   
            switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN,SWITCH_LOG_CONSOLE,"%s",_msg.c_str());
            return true;
        };  
    };

    /* Log manager declaration. */
    typedef Logger::Manager <class_type, output_type, SwitchPrinter, SimpleLock>  LogManager;

    /* Forward declaration */
    struct LogInternalManager; 

    struct Logger
    {
        /* Logger instance. */
        static LogManager Logg;

        /* Util Logger instance. */
        static LogInternalManager Logg2;

        static bool start();
        static void stop();
        static bool rotate();
        static void processLogConsole(switch_stream_handle_t *s, const std::string options, bool invert, bool unique);
        static void processLogDisk(switch_stream_handle_t *s, const std::string options, bool invert, bool unique);

    private:
        static std::string     base_path;
        static std::ofstream   generic_file;
    };

    /* Internal logging facility declaration */
    struct LogInternalManager
    {
        bool operator()(class_type classe, switch_stream_handle_t *stream, const char *args)
        {
            switch (classe)
            {
                case C_CLI:
                    stream->write_function(stream,"%s\n",args);
                    return true;
                default:
                    return K::Logger::Logg(classe, args);
            }
        }
        
        bool operator()(class_type classe, switch_stream_handle_t *stream, Format &fmt)
        {
            return K::Logger::Logg2(classe,stream,(char*) STR(fmt));
        }

        bool operator()(class_type classe, switch_stream_handle_t *stream, const char *fmt, const char *args)
        {
            switch (classe)
            {
                case C_CLI:
                    stream->write_function(stream,fmt,args); 
                    return true;
                default:
                    return K::Logger::Logg(classe, FMT(fmt) % args);
            }
        }
    };
};

#endif /* _Logger_H_ */
