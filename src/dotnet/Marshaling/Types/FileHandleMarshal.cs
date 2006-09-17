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
 * FileHandleMarshal.cs -- 
 *
 */
using System;
using System.Runtime.InteropServices;

namespace FreeSwitch.Marshaling.Types
{
    [StructLayout(LayoutKind.Sequential)]
    internal class FileHandleMarshal
    {
        internal IntPtr file_interface;
        internal UInt32 flags;
        internal IntPtr fd;
        internal uint   samples;
        internal UInt32 samplerate;
        internal Byte   channels;
        internal uint   format;
        internal uint   sections;
        internal int    seekable;
        internal uint   sample_count;
        internal int    speed;
        internal IntPtr memory_pool;
        internal IntPtr private_info;
        internal Int64  pos;
        internal IntPtr audio_buffer;
    }

}
