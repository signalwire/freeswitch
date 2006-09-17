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
 * CodecImplementationMarshal.cs -- 
 *
 */
using System;
using FreeSwitch.Types;

namespace FreeSwitch.Marshaling.Types
{
    internal class CodecImplementationMarshal
    {
        internal UInt32 samples_per_seconds;
        internal int    bits_per_second;
        internal int    microseconds_per_frame;
        internal UInt32 samples_per_frame;
        internal UInt32 bytes_per_frame;
        internal UInt32 encoded_bytes_per_frame;
        internal Byte   number_of_channels;
        internal int    pref_frames_per_packet;
        internal int    max_frames_per_packet;
        //internal CodecInitMarshal init;
        //internal CodecEncodeMarshal encode;
        //internal CodecDecode decode;
        //internal CodecDestroy destroy;
        internal IntPtr next;
    }
}
