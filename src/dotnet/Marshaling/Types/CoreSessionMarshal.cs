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
 * CoreSessionMarshal.cs -- 
 *
 */
using System;
using System.Text;
using System.Runtime.InteropServices;

namespace FreeSwitch.Marshaling.Types
{
    [StructLayout(LayoutKind.Sequential, Pack=1)]
    internal class CoreSessionMarshal
    {
        internal UInt32              id;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst=80)]
        internal byte[]              name; 
        internal int                 thread_running;
        internal IntPtr              pool;
        internal IntPtr              channel;
        internal IntPtr              thread;
        internal IntPtr              endpoint_interface;
        internal IOEventHooksMarshal event_hooks;
        internal IntPtr              read_codec;
        internal IntPtr              write_codec;
        internal IntPtr              raw_write_buffer;
        internal FrameMarshal        raw_write_frame;
        internal FrameMarshal        enc_write_frame;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst=2048)]
        internal byte[]              raw_write_buf;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst=2048)]
        internal byte[]              enc_write_buf;
        internal IntPtr              raw_read_buffer;
        internal FrameMarshal        raw_read_frame;
        internal FrameMarshal        enc_read_frame;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst=2048)]
        internal byte[]              raw_read_buf;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst=2048)]
        internal byte[]              enc_read_buf;
        internal IntPtr              read_resampler;
        internal IntPtr              write_resampler;
        internal IntPtr              mutex;
        internal IntPtr              cond;
        internal IntPtr              rwlock;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst=128)]
        internal IntPtr[]            streams;
        internal int                 stream_count;
        /* 36 + 1 char string, but need to grab 40 bytes */
        [MarshalAs(UnmanagedType.ByValArray, SizeConst=40)]
        internal byte[]              uuid_str;
        internal IntPtr              private_info;
        internal IntPtr              event_queue;
    }
}
