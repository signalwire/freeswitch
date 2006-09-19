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
 * CoreSession.cs -- 
 *
 */
using System;
using System.Collections;
using System.Runtime.InteropServices;
using System.Text;
using FreeSwitch.Marshaling.Types;

namespace FreeSwitch.Types
{
    /*
     * 	uint32_t id;
	char name[80];
	int thread_running;
	switch_memory_pool_t *pool;
	switch_channel_t *channel;
	switch_thread_t *thread;
	const switch_endpoint_interface_t *endpoint_interface;
	switch_io_event_hooks_t event_hooks;
	switch_codec_t *read_codec;
	switch_codec_t *write_codec;

	switch_buffer_t *raw_write_buffer;
	switch_frame_t raw_write_frame;
	switch_frame_t enc_write_frame;
	uint8_t raw_write_buf[SWITCH_RECCOMMENDED_BUFFER_SIZE];
	uint8_t enc_write_buf[SWITCH_RECCOMMENDED_BUFFER_SIZE];

	switch_buffer_t *raw_read_buffer;
	switch_frame_t raw_read_frame;
	switch_frame_t enc_read_frame;
	uint8_t raw_read_buf[SWITCH_RECCOMMENDED_BUFFER_SIZE];
	uint8_t enc_read_buf[SWITCH_RECCOMMENDED_BUFFER_SIZE];


	switch_audio_resampler_t *read_resampler;
	switch_audio_resampler_t *write_resampler;

	switch_mutex_t *mutex;
	switch_thread_cond_t *cond;

	switch_thread_rwlock_t *rwlock;

	void *streams[SWITCH_MAX_STREAMS];
	int stream_count;

	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	void *private_info;
	switch_queue_t *event_queue;
     */
    public class CoreSession
    {
        public HandleRef marshaledObject;

        /*
         * Properties
         */
        public UInt32 Id
        {
            get
            {
                CoreSessionMarshal coreSessionMarshal = (CoreSessionMarshal)marshaledObject.Wrapper;

                return coreSessionMarshal.id;
            }
        }

        public string Name
        {
            get
            {
                CoreSessionMarshal coreSessionMarshal = (CoreSessionMarshal) marshaledObject.Wrapper;

                return Encoding.ASCII.GetString(coreSessionMarshal.name);
            }
        }

        public bool IsThreadRunning
        {
            get
            {
                CoreSessionMarshal coreSessionMarshal = (CoreSessionMarshal)marshaledObject.Wrapper;
                 
                if (coreSessionMarshal.thread_running <= 0)
                    return false;
                else
                    return true;
            }
        }

        public MemoryPool Pool
        {
            get
            {
                return Switch.switch_core_session_get_pool(this);
            }
        }


        public Channel Channel
        {
            get
            {
                return Switch.switch_core_session_get_channel(this);
            }
        }

        public string Uuid
        {
            get
            {
                CoreSessionMarshal coreSessionMarshal = (CoreSessionMarshal)marshaledObject.Wrapper;

                return Encoding.ASCII.GetString(coreSessionMarshal.uuid_str);
            }
        }


        public CoreSession OutgoingChannel(string endpointName, CallerProfile callerProfile)
        {
            CoreSession newSession = new CoreSession();
            MemoryPool  pool       = new MemoryPool();

            Status status = Switch.switch_core_session_outgoing_channel(this, endpointName, callerProfile, ref newSession, pool);

            if (status != Status.Success)
                throw new Exception("Unsuccessful");


            return newSession;
        }

        public CoreSession()
        {
            //CoreSessionMarshal coreSessionMarshal = new CoreSessionMarshal();
            //IntPtr coreSessionPtr = Marshal.AllocHGlobal(Marshal.SizeOf(coreSessionMarshal));

            //marshaledObject = new HandleRef(coreSessionMarshal, coreSessionPtr);
        }

        public Status DtmfCallbackFunctionWrapper(CoreSession session, string dtmf, IntPtr buf, uint buflen)
        {
            return Status.Success;
        }
    }
}