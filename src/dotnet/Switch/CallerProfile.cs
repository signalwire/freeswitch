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
 * CallerProfile.cs -- 
 *
 */
using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using FreeSwitch.NET.Types;
using FreeSwitch.NET.Marshaling;
using FreeSwitch.NET.Marshaling.Types;

namespace FreeSwitch.NET
{
    public partial class Switch
    {
        /*
         * TODO: Figure out how to stop mono from trying to free the returned string.
         */
        [DllImport("freeswitch")]
        //[return: MarshalAs(UnmanagedType.LPStr)]
        public extern static
            IntPtr switch_caller_get_field_by_name(
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(CallerProfileMarshaler))]
                CallerProfile caller_profile,
                [MarshalAs(UnmanagedType.LPStr)]
                string name);
        /*
        [DllImport("freeswitch")]
        [return: MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(CallerProfileMarshaler))]
        public extern static
            CallerProfile switch_caller_profile_clone(
                [MarshalAs
            switch_core_session_t *session, switch_caller_profile_t *tocopy)
        */

        [DllImport("freeswitch")]
        [return: MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(CallerProfileMarshaler))]
        public extern static
            CallerProfile switch_caller_profile_new(
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(MemoryPoolMarshaler))]
                MemoryPool pool,
                [MarshalAs(UnmanagedType.LPStr)]
                string username,
                [MarshalAs(UnmanagedType.LPStr)]
                string dialplan,
                [MarshalAs(UnmanagedType.LPStr)]
                string callerIdName,
                [MarshalAs(UnmanagedType.LPStr)]
                string callerIdNumber,
                [MarshalAs(UnmanagedType.LPStr)]
                string networkAddress,
                [MarshalAs(UnmanagedType.LPStr)]
                string ani,
                [MarshalAs(UnmanagedType.LPStr)]
                string ani2,
                [MarshalAs(UnmanagedType.LPStr)]
                string rdnis,
                [MarshalAs(UnmanagedType.LPStr)]
                string source,
                [MarshalAs(UnmanagedType.LPStr)]
                string context,
                [MarshalAs(UnmanagedType.LPStr)]
                string destinationNumber);  
    }
}
