/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 *
 * switch_frame.h -- Media Frame Structure
 *
 */
/*! \file switch_frame.h
  \brief Media Frame Structure
*/

#ifndef SWITCH_FRAME_H
#define SWITCH_FRAME_H

#include <switch.h>

SWITCH_BEGIN_EXTERN_C
/*! \brief An abstraction of a data frame */
	struct switch_frame {
	/*! a pointer to the codec information */
	switch_codec_t *codec;
	/*! the originating source of the frame */
	const char *source;
	/*! the raw packet */
	void *packet;
	/*! the size of the raw packet when applicable */
	uint32_t packetlen;
	/*! the extra frame data */
	void *extra_data;
	/*! the frame data */
	void *data;
	/*! the size of the buffer that is in use */
	uint32_t datalen;
	/*! the entire size of the buffer */
	uint32_t buflen;
	/*! the number of audio samples present (audio only) */
	uint32_t samples;
	/*! the rate of the frame */
	uint32_t rate;
	/*! the number of channels in the frame */
	uint32_t channels;
	/*! the payload of the frame */
	switch_payload_t payload;
	/*! the timestamp of the frame */
	uint32_t timestamp;
	uint16_t seq;
	uint32_t ssrc;
	switch_bool_t m;
	/*! frame flags */
	switch_frame_flag_t flags;
	void *user_data;
	payload_map_t *pmap;
	switch_image_t *img;
};

SWITCH_END_EXTERN_C
#endif
/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
