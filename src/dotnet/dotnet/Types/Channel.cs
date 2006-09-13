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
using FreeSwitch.NET.Marshaling.Types;

namespace FreeSwitch.NET.Types
{
    /*
     * 	char *name;
	switch_buffer_t *dtmf_buffer;
	switch_mutex_t *dtmf_mutex;
	switch_mutex_t *flag_mutex;
	switch_mutex_t *profile_mutex;
	switch_core_session_t *session;
	switch_channel_state_t state;
	uint32_t flags;
	switch_caller_profile_t *caller_profile;
	switch_caller_profile_t *originator_caller_profile;
	switch_caller_profile_t *originatee_caller_profile;
	switch_caller_extension_t *caller_extension;
	const switch_state_handler_table_t *state_handlers[SWITCH_MAX_STATE_HANDLERS];
	int state_handler_index;
	switch_hash_t *variables;
	switch_channel_timetable_t *times;
	void *private_info;
	switch_call_cause_t hangup_cause;
	int freq;
	int bits;
	int channels;
	int ms;
	int kbps;
     */
    public class Channel
    {
        internal HandleRef marshaledObject;

        public string Name
        {
            get
            {
                ChannelMarshal channel = (ChannelMarshal) marshaledObject.Wrapper;

                return Marshal.PtrToStringAnsi(channel.name);
            }
        }

        public Buffer DtmfBuffer
        {
            get
            {
                Console.WriteLine("Buffer");
                return new Buffer();
                //throw new NotImplementedException();
            }
        }

        public ChannelState State
        {
            set
            {
                Switch.switch_channel_set_state(this, value);
            }
            get
            {
                return Switch.switch_channel_get_state(this);
            }
        }

        public bool IsReady
        {
            get
            {
                uint isReady = Switch.switch_channel_ready(this);

                if (isReady != 0)
                    return true;
                else
                    return false;
            }
        }

        public ChannelFlag Flags
        {
            get
            {
                Console.WriteLine("ChannelFlag");
                return new ChannelFlag();
                //throw new NotImplementedException();
            }
        }

        public CallerProfile CallerProfile
        {
            get
            {
                return Switch.switch_channel_get_caller_profile(this);
            }
        }

        public CallerProfile OriginatorCallerProfile
        {
            get
            {
                return Switch.switch_channel_get_originator_caller_profile(this);
            }
        }

        public CallerProfile OriginateeCallerProfile
        {
            get
            {
                return Switch.switch_channel_get_originatee_caller_profile(this);
            }
        }

        public CallerExtension CallerExtension
        {
            get
            {
                return Switch.switch_channel_get_caller_extension(this);
            }
        }

        public ChannelTimetable Times
        {
            get
            {
                Console.WriteLine("Channel Timetable");
                return new ChannelTimetable();
              //  throw new NotImplementedException();
            }
        }

        public int Freq
        {
            get
            {
                ChannelMarshal channel = (ChannelMarshal) marshaledObject.Wrapper;

                return channel.freq;
            }
        }

        public int Bits
        {
            get
            {
                ChannelMarshal channel = (ChannelMarshal) marshaledObject.Wrapper;

                return channel.bits;
            }
        }

        public int Channels
        {
            get
            {
                ChannelMarshal channel = (ChannelMarshal) marshaledObject.Wrapper;

                return channel.channels;
            }
        }

        public int Ms
        {
            get
            {
                ChannelMarshal channel = (ChannelMarshal) marshaledObject.Wrapper;

                return channel.ms;
            }
        }

        public int Kbps
        {
            get {
                ChannelMarshal channel = (ChannelMarshal) marshaledObject.Wrapper;

                return channel.kbps;
            }
        }

        public void PerformHangup()
        {
            Switch.switch_channel_perform_hangup(this, "file", "func", 100, CallCause.OutgoingCallBarred);
        }

        public Status Answer()
        {
            return Switch.switch_channel_answer(this);
        }
    }
}
