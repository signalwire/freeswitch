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
using System.Runtime.InteropServices;

namespace FreeSwitch.Types
{
    public class LoadableModule
    {
        internal IntPtr address;
        
        private  string           name;
        private  Buffer           dtmfBuffer;
        internal IntPtr           dtmf_mutex;
        internal IntPtr           session;
        private  ChannelState     state;
        private  ChannelFlag      flags;
        private  CallerProfile    callerProfile;
        private  CallerProfile    originatorCallerProfile;
        private  CallerProfile    originateeCallerProfile;
        private  CallerExtension  callerExtension;
        internal byte[]           state_handlers;
        internal int              state_handler_index;
        internal IntPtr           variables;
        private  ChannelTimetable times;
        internal IntPtr           privateInfo;
        private  int              freq;
        private  int              bits;
        private  int              channels;
        private  int              ms;
        private  int              kbps;

        public string Name
        {
            set { name = value; }
            get { return name; }
        }

        public Buffer DtmfBuffer
        {
            set { dtmfBuffer = value; }
            get { return dtmfBuffer; }
        }

        public ChannelState State
        {
            set { state = value; }
            get { return state; }
        }

        public ChannelFlag Flags
        {
            set { flags = value; }
            get { return flags; }
        }

        public CallerProfile CallerProfile
        {
            set { callerProfile = value; }
            get { return callerProfile; }
        }

        public CallerProfile OriginatorCallerProfile
        {
            set { originatorCallerProfile = value; }
            get { return originatorCallerProfile; }
        }

        public CallerProfile OriginateeCallerProfile
        {
            set { originateeCallerProfile = value; }
            get { return originateeCallerProfile; }
        }

        public CallerExtension CallerExtension
        {
            set { callerExtension = value; }
            get { return callerExtension; }
        }

        public ChannelTimetable Times
        {
            set { times = value; }
            get { return times; }
        }

        public int Freq
        {
            set { freq = value; }
            get { return freq; }
        }

        public int Bits
        {
            set { bits = value; }
            get { return bits; }
        }

        public int Channels
        {
            set { channels = value; }
            get { return channels; }
        }

        public int Ms
        {
            set { ms = value; }
            get { return ms; }
        }

        public int Kbps
        {
            set { kbps = value; }
            get { return kbps; }
        }
    }
}
