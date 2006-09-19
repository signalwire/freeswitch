/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2006, James Martelletti <james@nerdc0re.com>
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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * James Martelletti <james@nerdc0re.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * James Martelletti <james@nerdc0re.com>
 *
 *
 * Log.cs -- 
 *
 */
using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using FreeSwitch.Types;
using FreeSwitch.Marshaling;
using FreeSwitch.Marshaling.Types;

namespace FreeSwitch
{
    /*
    src/switch_log.c:SWITCH_DECLARE(const char *) switch_log_level2str(switch_log_level_t level)
    src/switch_log.c:SWITCH_DECLARE(switch_log_level_t) switch_log_str2level(const char *str)
    src/switch_log.c:SWITCH_DECLARE(switch_status_t) switch_log_bind_logger(switch_log_function_t function, switch_log_level_t level)
    src/switch_log.c:SWITCH_DECLARE(void) switch_log_printf(switch_text_channel_t channel, char *file, const char *func, int line, switch_log_level_t level, char *fmt, ...)
    src/switch_log.c:SWITCH_DECLARE(switch_status_t) switch_log_init(switch_memory_pool_t *pool)
    src/switch_log.c:SWITCH_DECLARE(switch_status_t) switch_log_shutdown(void)
     */
    public partial class Switch
    {
        [DllImport("freeswitch")]
        public extern static
            void switch_log_printf(
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(ChannelMarshaler))]
                Channel channel,
                string file,
                string func,
                int line,
                LogLevel level,
                string format);
    }
}
