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
 * Channel.cs -- 
 *
 */
using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using FreeSwitch.Types;
using FreeSwitch.Marshaling;

namespace FreeSwitch
{
    public partial class Switch
    {
        [DllImport("freeswitch")]
        public extern static
            ChannelState switch_channel_set_state(
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(ChannelMarshaler))]
                Channel channel,
                ChannelState channelState);

        [DllImport("freeswitch")]
        public extern static
            ChannelState switch_channel_get_state(
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(ChannelMarshaler))]
                Channel channel);

        [DllImport("freeswitch")]
        public extern static
            Status switch_channel_answer(
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(ChannelMarshaler))]
                Channel channel);

        [DllImport("freeswitch")]
        public extern static
            ChannelState switch_channel_perform_hangup(
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(ChannelMarshaler))]
                Channel channel,
                [MarshalAs(UnmanagedType.LPStr)]
                string file,
                [MarshalAs(UnmanagedType.LPStr)]
                string func,
                int line,
                CallCause hangup_cause);

        [DllImport("freeswitch")]
        [return: MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(CallerExtensionMarshaler))]
        public extern static
            CallerExtension switch_channel_get_caller_extension(
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(ChannelMarshaler))]
                Channel channel);

        [DllImport("freeswitch")]
        [return: MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(CallerProfileMarshaler))]
        public extern static
            CallerProfile switch_channel_get_caller_profile(
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ChannelMarshaler))]
                Channel channel);

        [DllImport("freeswitch")]
        [return: MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(CallerProfileMarshaler))]
        public extern static
            CallerProfile switch_channel_get_originator_caller_profile(
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(ChannelMarshaler))]
                Channel channel);

        [DllImport("freeswitch")]
        [return: MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(CallerProfileMarshaler))]
        public extern static
            CallerProfile switch_channel_get_originatee_caller_profile(
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ChannelMarshaler))]
                Channel channel);

        [DllImport("freeswitch")]
        public extern static
            ChannelState switch_channel_hangup(
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(ChannelMarshaler))]
                Channel channel,
                CallCause hangup_cause);

        [DllImport("freeswitch")]
        public extern static
            uint switch_channel_ready(
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(ChannelMarshaler))]
                Channel channel);
    }
}
