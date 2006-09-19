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
        public extern static
            Status switch_ivr_sleep(
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(CoreSessionMarshaler))]
                CoreSession session,
                UInt32 ms);

        [DllImport("freeswitch")]
        public extern static
            Status switch_ivr_park(
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(CoreSessionMarshaler))]
                CoreSession session);

        [DllImport("freeswitch")]
        public extern static
            Status switch_ivr_collect_digits_callback(
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(CoreSessionMarshaler))]
                CoreSession session,
                InputCallbackFunction dtmfCallback,
                IntPtr buf,
                uint buflen);

        [DllImport("freeswitch")]
        public extern static
            Status switch_ivr_collect_digits_count(
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(CoreSessionMarshaler))]
                CoreSession session,
                [MarshalAs(UnmanagedType.LPStr)]
                string buffer,
                uint buflen,
                uint maxdigits,
                [MarshalAs(UnmanagedType.LPStr)]
                string terminators,
                [MarshalAs(UnmanagedType.LPStr)]
                string terminator,
                uint timeout);

        [DllImport("freeswitch")]
        public extern static
            Status switch_ivr_record_session(
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(CoreSessionMarshaler))]
                CoreSession session,
                [MarshalAs(UnmanagedType.LPStr)]
                string file,
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(FileHandleMarshaler))]
                FileHandle fh);

        [DllImport("freeswitch")]
        public extern static
            Status switch_ivr_stop_record_session(
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(CoreSessionMarshaler))]
                CoreSession session,
                [MarshalAs(UnmanagedType.LPStr)]
                string file);

        [DllImport("freeswitch")]
        extern public static
            Status switch_ivr_play_file(
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(CoreSessionMarshaler))]
                CoreSession session,
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(FileHandleMarshaler))]
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
            Status switch_ivr_record_file(
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(CoreSessionMarshaler))]
                CoreSession session,
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(FileHandleMarshaler))]
                FileHandle fh,
                IntPtr file,
                InputCallbackFunction input_callback,
                IntPtr buf,
                uint buflen);

        [DllImport("freeswitch")]
        public extern static
            Status switch_ivr_speak_text_handle(
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(CoreSessionMarshaler))]
                CoreSession session,
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(SpeechHandleMarshaler))]
                SpeechHandle sh,
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(CodecMarshaler))]
                Codec codec,
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(TimerMarshaler))]
                Timer timer,
                InputCallbackFunction dtmfCallback,
                [MarshalAs(UnmanagedType.LPStr)]
                string text,
                IntPtr buf,
                uint buflen);

        [DllImport("freeswitch")]
        public extern static
            Status switch_ivr_speak_text(
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(CoreSessionMarshaler))]
                CoreSession session,
                [MarshalAs(UnmanagedType.LPStr)]
                string tts_name,
                [MarshalAs(UnmanagedType.LPStr)]
                string voice_name,
                [MarshalAs(UnmanagedType.LPStr)]
                string timer_name,
                uint rate,
                InputCallbackFunction dtmfCallback,
                [MarshalAs(UnmanagedType.LPStr)]
                string text,
                IntPtr buf,
                uint buflen);

        [DllImport("freeswitch")]
        public extern static
            Status switch_ivr_originate(
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(CoreSessionMarshaler))]
                CoreSession session,
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(CoreSessionMarshaler))]
                ref CoreSession bleg,
                IntPtr cause,
                IntPtr bridgeto,
                UInt32 timelimit_sec,
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(StateHandlerTableMarshaler))]
                StateHandlerTable table,
                [MarshalAs(UnmanagedType.LPStr)]
                string cid_name_override,
                [MarshalAs(UnmanagedType.LPStr)]
                string cid_num_override,
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(CallerProfileMarshaler))]
                CallerProfile caller_profile_override);

        [DllImport("freeswitch")]
        public extern static
            Status switch_ivr_multi_threaded_bridge(
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(CoreSessionMarshaler))]
                CoreSession session,
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(CoreSessionMarshaler))]
                CoreSession peer_session,
                InputCallbackFunction dtmfCallback,
                IntPtr session_data,
                IntPtr peer_session_data);

        [DllImport("freeswitch")]
        public extern static
            Status switch_ivr_session_transfer(
                [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef=typeof(CoreSessionMarshaler))]
                CoreSession session,
                [MarshalAs(UnmanagedType.LPStr)]
                string extension,
                [MarshalAs(UnmanagedType.LPStr)]
                string dialplan,
                [MarshalAs(UnmanagedType.LPStr)]
                string context);
    }
}
