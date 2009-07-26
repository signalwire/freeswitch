/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application - mod_managed
 * Copyright (C) 2008, Michael Giagnocavo <mgg@giagnocavo.net>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application - mod_managed
 *
 * The Initial Developer of the Original Code is
 * Michael Giagnocavo <mgg@giagnocavo.net>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Michael Giagnocavo <mgg@giagnocavo.net>
 * 
 * Log.cs -- Log wrappers
 *
 */
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace FreeSWITCH
{
    public static class Log
    {
        public static void Write(LogLevel level, string message)
        {
            Native.freeswitch.console_log(level.ToLogString(), message);
        }
        public static void Write(LogLevel level, string format, params object[] args)
        {
            Native.freeswitch.console_log(level.ToLogString(), string.Format(format, args));
        }
        public static void WriteLine(LogLevel level, string message)
        {
            Native.freeswitch.console_log(level.ToLogString(), message + Environment.NewLine);
        }
        public static void WriteLine(LogLevel level, string format, params object[] args)
        {
            Native.freeswitch.console_log(level.ToLogString(), string.Format(format, args) + Environment.NewLine);
        }

        static string ToLogString(this LogLevel level)
        {
            switch (level) {
                case LogLevel.Alert: return "ALERT";
                case LogLevel.Critical: return "CRIT";
                case LogLevel.Debug: return "DEBUG";
                case LogLevel.Error: return "ERR";
                case LogLevel.Info: return "INFO";
                case LogLevel.Notice: return "NOTICE";
                case LogLevel.Warning: return "WARNING";
                default: 
                    System.Diagnostics.Debug.Fail("Invalid LogLevel: " + level.ToString() + " (" + (int)level+ ").");
                    return "INFO";
            }
        }
    }

    /*switch_log.c:
    tatic const char *LEVELS[] = {
	"CONSOLE",
	"ALERT",
	"CRIT",
	"ERR",
	"WARNING",
	"NOTICE",
	"INFO",
	"DEBUG",
	NULL
    };*/
    public enum LogLevel
    {
        Debug,
        Info,
        Error,
        Critical,
        Alert,
        Warning,
        Notice,
    }
}
