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
 * Ivr.cs -- 
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
    public partial class Switch
    {
        [DllImport("freeswitch")]
        extern public static
            Status switch_ivr_play_file(
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(CoreSessionMarshaler))]
                CoreSession session,
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(FileHandleMarshaler))]
                FileHandle fh,
                [MarshalAs(UnmanagedType.LPStr)]
                string file,
                [MarshalAs(UnmanagedType.LPStr)]
                string timer_name,
                InputCallbackFunction input_callback,
                IntPtr buf,
                uint buflen);

        [DllImport("freeswitch")]
        public extern static
            Status switch_ivr_multi_threaded_bridge(
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(CoreSessionMarshaler))]
                CoreSession session,
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(CoreSessionMarshaler))]
                CoreSession peer_session,
                uint timelimit,
                IntPtr dtmf_callback,
                IntPtr session_data,
                IntPtr peer_session_data);

        [DllImport("freeswitch")]
        public extern static
            Status switch_ivr_record_file(
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(CoreSessionMarshaler))]
                CoreSession session,
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(FileHandleMarshaler))]
                FileHandle fh,
                IntPtr file,
                InputCallbackFunction input_callback,
                IntPtr buf,
                uint buflen);
    }
}
