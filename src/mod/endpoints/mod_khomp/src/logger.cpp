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

#include "logger.h"
#include "globals.h"

namespace K
{
    LogManager Logger::Logg;
    LogInternalManager Logger::Logg2;

    std::string     Logger::base_path;
    std::ofstream   Logger::generic_file;

    bool LogConfig::set(Logfile & file, const char * section, const char * option, bool value)
    {
        Section * sec = file.root().section_find(section);

        if (!sec)
            return false;

        Option * opt = sec->option_find(option);

        if (!opt)
            return false;

        opt->set( value ? "Ativado" : "Desativado" );

        return true;
    }

    bool LogConfig::commit(Logfile & file)
    {
        return file.provide();
    }
        
    bool Logger::start()
    {
        /* we love shortcuts! */
        typedef LogManager::Option LogOpt; 

        typedef LogOpt::Flags     Flags;
        typedef LogOpt::InitFlags FL;  

        /* configures default log levels */
        Logger::Logg.classe(C_ERROR)
            & LogOpt(O_CONSOLE, "ERROR: ", Flags(FL(LogOpt::DATETIME) & FL(LogOpt::ENABLED)))
            & LogOpt(O_GENERIC, "E: ", Flags(FL(LogOpt::DATETIME) & FL(LogOpt::THREADID) & FL(LogOpt::ENABLED)));

        Logger::Logg.classe(C_WARNING)
            & LogOpt(O_CONSOLE, "WARNING: ", Flags(FL(LogOpt::DATETIME) & FL(LogOpt::ENABLED)))
            & LogOpt(O_GENERIC, "W: ", Flags(FL(LogOpt::DATETIME) & FL(LogOpt::THREADID) & FL(LogOpt::ENABLED)));

        Logger::Logg.classe(C_MESSAGE)
            & LogOpt(O_CONSOLE, Flags(FL(LogOpt::ENABLED)))
            & LogOpt(O_GENERIC, "M: ", Flags(FL(LogOpt::DATETIME) & FL(LogOpt::THREADID) & FL(LogOpt::ENABLED)));

        /* k3l messages */
        Logger::Logg.classe(C_COMMAND)
            & LogOpt(O_CONSOLE, Flags(FL(LogOpt::DATETIME)))
            & LogOpt(O_GENERIC, "c: ", Flags(FL(LogOpt::DATETIME) & FL(LogOpt::THREADID)));

        Logger::Logg.classe(C_EVENT)
            & LogOpt(O_CONSOLE, Flags(FL(LogOpt::DATETIME)))
            & LogOpt(O_GENERIC, "e: ", Flags(FL(LogOpt::DATETIME) & FL(LogOpt::THREADID)));

        Logger::Logg.classe(C_AUDIO_EV)
            & LogOpt(O_CONSOLE, Flags(FL(LogOpt::DATETIME)))
            & LogOpt(O_GENERIC, "a: ", Flags(FL(LogOpt::DATETIME) & FL(LogOpt::THREADID)));

        Logger::Logg.classe(C_MODEM_EV)
            & LogOpt(O_CONSOLE, Flags(FL(LogOpt::DATETIME)))
            & LogOpt(O_GENERIC, "m: ", Flags(FL(LogOpt::DATETIME) & FL(LogOpt::THREADID) & FL(LogOpt::ENABLED)));

        Logger::Logg.classe(C_LINK_STT)
            & LogOpt(O_CONSOLE, Flags(FL(LogOpt::DATETIME) & FL(LogOpt::ENABLED)))
            & LogOpt(O_GENERIC, "s: ", Flags(FL(LogOpt::DATETIME) & FL(LogOpt::THREADID) & FL(LogOpt::ENABLED)));

        Logger::Logg.classe(C_CAS_MSGS)
            & LogOpt(O_CONSOLE, Flags(FL(LogOpt::DATETIME) & FL(LogOpt::ENABLED)))
            & LogOpt(O_GENERIC, "p: ", Flags(FL(LogOpt::DATETIME) & FL(LogOpt::THREADID) & FL(LogOpt::ENABLED)));

        /* channel debug */
        Logger::Logg.classe(C_DBG_FUNC)
            & LogOpt(O_GENERIC, "F: ", Flags(FL(LogOpt::DATETIME) & FL(LogOpt::THREADID) & FL(LogOpt::ENABLED)));

        Logger::Logg.classe(C_DBG_LOCK)
            & LogOpt(O_GENERIC, "L: ", Flags(FL(LogOpt::DATETIME) & FL(LogOpt::THREADID) & FL(LogOpt::ENABLED)));

        Logger::Logg.classe(C_DBG_THRD)
            & LogOpt(O_GENERIC, "T: ", Flags(FL(LogOpt::DATETIME) & FL(LogOpt::THREADID) & FL(LogOpt::ENABLED)));

        Logger::Logg.classe(C_DBG_STRM)
            & LogOpt(O_GENERIC, "S: ", Flags(FL(LogOpt::DATETIME) & FL(LogOpt::THREADID) & FL(LogOpt::ENABLED)));

        Logger::Logg.classe(C_DBG_CONF)
            & LogOpt(O_GENERIC, "C: ", Flags(FL(LogOpt::DATETIME) & FL(LogOpt::THREADID) & FL(LogOpt::ENABLED)));

        Logger::Logg.classe(C_DBG_FUNC).enabled(false);
        Logger::Logg.classe(C_DBG_LOCK).enabled(false);
        Logger::Logg.classe(C_DBG_THRD).enabled(false);
        Logger::Logg.classe(C_DBG_STRM).enabled(false);

        /* we debug config file loading, by default */
        Logger::Logg.classe(C_DBG_CONF).enabled(true);

        /* adds a prefix to the cli messages */
        Logger::Logg.classe(C_CLI).prefix("<K> ");

        /* inserts default console log before opening files */
        Logger::Logg.add(O_CONSOLE,SwitchConsoleLog(), "mod_khomp: ");

        time_t      tv;
        struct tm   lt;

        /* get local time! */
        time (&tv);
        localtime_r (&tv, &lt);

        base_path = STG(FMT("/var/log/khomp%d.%d/mod_khomp-%04d%02d%02d_%02d%02d%02d/")
                % k3lApiMajorVersion % k3lApiMinorVersion % (lt.tm_year + 1900) % (lt.tm_mon + 1)
                % lt.tm_mday % lt.tm_hour % lt.tm_min % lt.tm_sec );

        // NOTE: ALWAYS unlink, as we may have a dangling symlink lying around...
        std::string link_path = STG(FMT("/var/log/khomp%d.%d/current") % k3lApiMajorVersion % k3lApiMinorVersion );
        unlink(link_path.c_str());

        if (mkdir(base_path.c_str(), 493 /*755*/) < 0 && errno != EEXIST)
        {
            Logger::Logg(C_ERROR, D("unable to create log directory '%s': %s!") % base_path % strerror(errno));
            return false;
        }

        if (symlink(base_path.c_str(), link_path.c_str()))
        {
            Logger::Logg(C_ERROR, D("unable to create symlink to latest log directory '%s': %s!") % base_path % strerror(errno));
        }

        std::string gen_tmp = base_path + std::string("generic.log");
        generic_file.open(gen_tmp.c_str());

        if (!generic_file.good())
        {
            Logger::Logg(C_ERROR, D("could not open file '%s': %s") % gen_tmp % strerror(errno));
            return false;
        }

        /* inserts other file descriptors (TODO: delete this when stopping logs) */
        Logger::Logg.add(O_GENERIC, &generic_file);
        return true;
    }

    void Logger::stop()
    {
        if(generic_file.is_open())
        {
            generic_file.close();
        }
    }

    bool Logger::rotate()
    {
        std::string new_gen;

        for (unsigned int i = 0;; i++)
        {   
            std::string tmp = base_path + STG(FMT("generic.%d.log") % i); 

            if (access(tmp.c_str(), R_OK|W_OK) != 0 && errno == ENOENT)
            {   
                new_gen = tmp;
                break;
            }   
        }   

        std::string old_gen = base_path + "generic.log";

        if (rename(old_gen.c_str(), new_gen.c_str()) != 0)
        {   
            Logger::Logg(C_ERROR, FMT("unable to move generic log file: %s.") % strerror(errno));
            return false;
        }   

        Globals::logs_being_rotated = true;

        generic_file.close();
        generic_file.open(old_gen.c_str());

        Globals::logs_being_rotated = false;

        return true;
    }

    void Logger::processLogConsole(switch_stream_handle_t *s, const std::string options, bool invert, bool unique)
    {
        class_type classe = ( !s ? C_MESSAGE : C_CLI );

        Strings::vector_type tokens;
        Strings::tokenize(options, tokens, ",");

        bool flag_errors   = false;
        bool flag_warnings = false;
        bool flag_messages = false;
        bool flag_events   = false;
        bool flag_commands = false;
        bool flag_audio    = false;
        bool flag_modem    = false;
        bool flag_link     = false;
        bool flag_cas      = false;

        Strings::Merger strs;

        for (Strings::vector_type::iterator i = tokens.begin(); i != tokens.end(); i++) 
        {    
            std::string tok = Strings::trim(*i);

            /**/ if ((tok) == "errors")     flag_errors   = true;
            else if ((tok) == "warnings")   flag_warnings = true;
            else if ((tok) == "messages")   flag_messages = true;
            else if ((tok) == "events")     flag_events   = true;
            else if ((tok) == "commands")   flag_commands = true;
            else if ((tok) == "audio")      flag_audio    = true;
            else if ((tok) == "modem")      flag_modem    = true;
            else if ((tok) == "link")       flag_link     = true;
            else if ((tok) == "cas")        flag_cas      = true;
            else if ((tok) == "standard")
            {    
                flag_errors   = true;
                flag_warnings = true;
                flag_messages = true;
                flag_link     = true;
            }    
            else if ((tok) == "all")
            {    
                flag_errors   = true;
                flag_warnings = true;
                flag_messages = true;
                flag_events   = true;
                flag_commands = true;
                flag_audio    = true;
                flag_modem    = true;
                flag_link     = true;
                flag_cas      = true;
            }    
            else 
            {    
                K::Logger::Logg2(classe, s, FMT("WARNING: The following console message option is not valid and will be ignored: %s.") % (tok) );
                continue;
            }    

            strs.add(tok);
        }

        if ((unique && !flag_errors)    || flag_errors)    K::Logger::Logg.classe(C_ERROR)   .set(O_CONSOLE, LogManager::Option::ENABLED, (!invert && flag_errors));
        if ((unique && !flag_warnings)  || flag_warnings)  K::Logger::Logg.classe(C_WARNING) .set(O_CONSOLE, LogManager::Option::ENABLED, (!invert && flag_warnings));
        if ((unique && !flag_messages)  || flag_messages)  K::Logger::Logg.classe(C_MESSAGE) .set(O_CONSOLE, LogManager::Option::ENABLED, (!invert && flag_messages));
        if ((unique && !flag_events)    || flag_events)    K::Logger::Logg.classe(C_EVENT)   .set(O_CONSOLE, LogManager::Option::ENABLED, (!invert && flag_events));
        if ((unique && !flag_commands)  || flag_commands)  K::Logger::Logg.classe(C_COMMAND) .set(O_CONSOLE, LogManager::Option::ENABLED, (!invert && flag_commands));
        if ((unique && !flag_audio)     || flag_audio)     K::Logger::Logg.classe(C_AUDIO_EV).set(O_CONSOLE, LogManager::Option::ENABLED, (!invert && flag_audio));
        if ((unique && !flag_modem)     || flag_modem)     K::Logger::Logg.classe(C_MODEM_EV).set(O_CONSOLE, LogManager::Option::ENABLED, (!invert && flag_modem));
        if ((unique && !flag_link)      || flag_link)      K::Logger::Logg.classe(C_LINK_STT).set(O_CONSOLE, LogManager::Option::ENABLED, (!invert && flag_link));
        if ((unique && !flag_cas)       || flag_cas)       K::Logger::Logg.classe(C_CAS_MSGS).set(O_CONSOLE, LogManager::Option::ENABLED, (!invert && flag_cas));

        if (!strs.empty())
        {
            K::Logger::Logg2(classe, s, FMT("NOTICE: %s %sthe following console messages: %s.")
                    % (invert ? "Disabling" : "Enabling")
                    % (unique ? "just " : "") % strs.merge(", "));
        }
        else
        {
            K::Logger::Logg2(classe, s, "WARNING: No valid console messages have been specified, doing nothing.");
        }
    }

    void Logger::processLogDisk(switch_stream_handle_t *s, const std::string options, bool invert, bool unique)
    {
        class_type classe = ( !s ? C_MESSAGE : C_CLI );

        Strings::vector_type tokens;
        Strings::tokenize(options, tokens, ",");

        bool flag_errors    = false;
        bool flag_warnings  = false;
        bool flag_messages  = false;
        bool flag_events    = false;
        bool flag_commands  = false;
        bool flag_audio     = false;
        bool flag_modem     = false;
        bool flag_link      = false;
        bool flag_cas       = false;
        bool flag_functions = false;
        bool flag_threads   = false;
        bool flag_locks     = false;
        bool flag_streams   = false;

        Strings::Merger strs;
        
        for (Strings::vector_type::iterator i = tokens.begin(); i != tokens.end(); i++) 
        {    
            std::string tok = Strings::trim(*i);

            /**/ if ((tok) == "errors")     flag_errors    = true;
            else if ((tok) == "warnings")   flag_warnings  = true;
            else if ((tok) == "messages")   flag_messages  = true;
            else if ((tok) == "events")     flag_events    = true;
            else if ((tok) == "commands")   flag_commands  = true;
            else if ((tok) == "audio")      flag_audio     = true;
            else if ((tok) == "modem")      flag_modem     = true;
            else if ((tok) == "link")       flag_link      = true;
            else if ((tok) == "cas")        flag_cas       = true;
            else if ((tok) == "functions")  flag_functions = true;
            else if ((tok) == "threads")    flag_threads   = true;
            else if ((tok) == "locks")      flag_locks     = true;
            else if ((tok) == "streams")    flag_streams   = true;
            else if ((tok) == "standard")
            {    
                flag_errors   = true;
                flag_warnings = true;
                flag_messages = true;
                flag_link     = true;
            }    
            else if ((tok) == "debugging")
            {    
                flag_errors    = true;
                flag_warnings  = true;
                flag_messages  = true;
                flag_commands  = true;
                flag_events    = true;
                flag_audio     = true;
                flag_modem     = true;
                flag_link      = true;
                flag_cas       = true;
                flag_functions = true;
            }    
            else if ((tok) == "all")
            {    
                flag_errors    = true;
                flag_warnings  = true;
                flag_messages  = true;
                flag_events    = true;
                flag_commands  = true;
                flag_audio     = true;
                flag_modem     = true;
                flag_link      = true;
                flag_cas       = true;
                flag_functions = true;
                flag_threads   = true;
                flag_locks     = true;
                flag_streams   = true;
            }
            else
            {
                /* do not show! */
                continue;
            }

            strs.add(tok);
        }

        if ((unique && !flag_errors)    || flag_errors)    K::Logger::Logg.classe(C_ERROR)   .set(O_GENERIC, LogManager::Option::ENABLED, (!invert && flag_errors));
        if ((unique && !flag_warnings)  || flag_warnings)  K::Logger::Logg.classe(C_WARNING) .set(O_GENERIC, LogManager::Option::ENABLED, (!invert && flag_warnings));
        if ((unique && !flag_messages)  || flag_messages)  K::Logger::Logg.classe(C_MESSAGE) .set(O_GENERIC, LogManager::Option::ENABLED, (!invert && flag_messages));
        if ((unique && !flag_events)    || flag_events)    K::Logger::Logg.classe(C_EVENT)   .set(O_GENERIC, LogManager::Option::ENABLED, (!invert && flag_events));
        if ((unique && !flag_commands)  || flag_commands)  K::Logger::Logg.classe(C_COMMAND) .set(O_GENERIC, LogManager::Option::ENABLED, (!invert && flag_commands));
        if ((unique && !flag_audio)     || flag_audio)     K::Logger::Logg.classe(C_AUDIO_EV).set(O_GENERIC, LogManager::Option::ENABLED, (!invert && flag_audio));
        if ((unique && !flag_modem)     || flag_modem)     K::Logger::Logg.classe(C_MODEM_EV).set(O_GENERIC, LogManager::Option::ENABLED, (!invert && flag_modem));
        if ((unique && !flag_link)      || flag_link)      K::Logger::Logg.classe(C_LINK_STT).set(O_GENERIC, LogManager::Option::ENABLED, (!invert && flag_link));
        if ((unique && !flag_cas)       || flag_cas)       K::Logger::Logg.classe(C_CAS_MSGS).set(O_GENERIC, LogManager::Option::ENABLED, (!invert && flag_cas));
        if ((unique && !flag_functions) || flag_functions) K::Logger::Logg.classe(C_DBG_FUNC).enabled(!invert && flag_functions);
        if ((unique && !flag_threads)   || flag_threads)   K::Logger::Logg.classe(C_DBG_THRD).enabled(!invert && flag_threads);
        if ((unique && !flag_locks)     || flag_locks)     K::Logger::Logg.classe(C_DBG_LOCK).enabled(!invert && flag_locks);
        if ((unique && !flag_streams)   || flag_streams)   K::Logger::Logg.classe(C_DBG_STRM).enabled(!invert && flag_streams);

        if (!strs.empty())
        {
            K::Logger::Logg2(classe, s, FMT("NOTICE: %s %sthe logging of the following messages: %s.")
                    % (invert ? "Disabling" : "Enabling")
                    % (unique ? "just " : "") % strs.merge(", "));

            if ((flag_streams || flag_locks) && !invert)
            {
                K::Logger::Logg2(classe, s, "WARNING: You have enabled *INTENSIVE* debug messages for the Khomp channel!");
                K::Logger::Logg2(classe, s, "WARNING: Don't *EVER* use these options on production systems, unless you *REALLY* know what you are doing!");
            }
            else if ((flag_functions || flag_threads) && !invert)
            {
                K::Logger::Logg2(classe, s, "WARNING: You have enabled some debug messages for the Khomp channel.");
                K::Logger::Logg2(classe, s, "WARNING: Do not use these options on production systems unless you really know what you are doing!");
            }
        }
        else
        {
            K::Logger::Logg2(classe, s, "WARNING: No valid log messages have been specified, doing nothing.");
        }
    }
}
