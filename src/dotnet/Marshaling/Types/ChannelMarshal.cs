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
 * ChannelMarshal.cs -- 
 *
 */
using System;
using System.Runtime.InteropServices;
using FreeSwitch.Types;

namespace FreeSwitch.Marshaling.Types
{
    [StructLayout(LayoutKind.Sequential)]
    internal class ChannelMarshal
    {
        internal IntPtr       name;
        internal IntPtr       dtmf_buffer;
        internal IntPtr       dtmf_mutex;
        internal IntPtr       flag_mutex;
        internal IntPtr       profile_mutex;
        internal IntPtr       session;
        internal ChannelState state;
        internal UInt32       flags;
        internal IntPtr       caller_profile;
        internal IntPtr       originator_caller_profile;
        internal IntPtr       originatee_caller_profile;
        internal IntPtr       caller_extension;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst=120)]
        internal byte[]       state_handlers;
        internal int          state_handler_index;
        internal IntPtr       variables;
        internal IntPtr       times;
        internal IntPtr       private_info;
        internal IntPtr       hangup_cause;
        internal int          freq;
        internal int          bits;
        internal int          channels;
        internal int          ms;
        internal int          kbps;
    }
}
