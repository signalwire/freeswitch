/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
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

#ifdef __cplusplus
extern "C" {
#endif

#include <switch.h>

/*! \brief An abstraction of a data frame */
struct switch_frame {
	/*! a pointer to the codec information */
	switch_codec *codec;
	/*! the frame data */
	void *data;
	/*! the size of the buffer that is in use */
	size_t datalen;
	/*! the entire size of the buffer */
	size_t buflen;
	/*! the number of audio samples present (audio only) */
	int samples;
	/*! the rate of the frame */
	int rate;
};

#ifdef __cplusplus
}
#endif

#endif
