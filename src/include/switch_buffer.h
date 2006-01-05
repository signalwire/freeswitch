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
 * switch_buffer.h -- Data Buffering Code
 *
 */
/*! \file switch_buffer.h
    \brief Data Buffering Code

	The purpose of this module is to make a plain buffering interface that can be used for read/write buffers
	throughout the application.  The first implementation was done to provide the functionality and the interface
	and I think it can be optimized under the hood as we go using bucket brigades and/or ring buffering techniques.
*/

#ifndef SWITCH_BUFFER_H
#define SWITCH_BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <switch.h>

/**
 * @defgroup switch_buffer Buffer Routines
 * @ingroup FREESWITCH 
 * @{
 */

struct switch_buffer;


/*! \brief Allocate a new switch_buffer 
 * \param pool Pool to allocate the buffer from
 * \param buffer returned pointer to the new buffer
 * \param max_len length required by the buffer
 * \return status
 */
SWITCH_DECLARE(switch_status) switch_buffer_create(switch_memory_pool *pool, switch_buffer **buffer, size_t max_len);

/*! \brief Get the length of a switch_buffer 
 * \param buffer any buffer of type switch_buffer
 * \return int size of the buffer.
 */
SWITCH_DECLARE(int) switch_buffer_len(switch_buffer *buffer);

/*! \brief Get the freespace of a switch_buffer 
 * \param buffer any buffer of type switch_buffer
 * \return int freespace in the buffer.
 */
SWITCH_DECLARE(int) switch_buffer_freespace(switch_buffer *buffer);

/*! \brief Get the in use amount of a switch_buffer 
 * \param buffer any buffer of type switch_buffer
 * \return int ammount of buffer curently in use
 */
SWITCH_DECLARE(int) switch_buffer_inuse(switch_buffer *buffer);

/*! \brief Read data from a switch_buffer up to the ammount of datalen if it is available.  Remove read data from buffer. 
 * \param buffer any buffer of type switch_buffer
 * \param data pointer to the read data to be returned
 * \param datalen amount of data to be returned
 * \return int ammount of data actually read
 */
SWITCH_DECLARE(int) switch_buffer_read(switch_buffer *buffer, void *data, size_t datalen);

/*! \brief Write data into a switch_buffer up to the length of datalen
 * \param buffer any buffer of type switch_buffer
 * \param data pointer to the data to be written
 * \param datalen amount of data to be written
 * \return int amount of buffer used after the write, or 0 if no space available
 */
SWITCH_DECLARE(int) switch_buffer_write(switch_buffer *buffer, void *data, size_t datalen);

/*! \brief Remove data from the buffer
 * \param buffer any buffer of type switch_buffer
 * \param datalen amount of data to be removed
 * \return int size of buffer, or 0 if unable to toss that much data
 */
SWITCH_DECLARE(int) switch_buffer_toss(switch_buffer *buffer, size_t datalen);
/** @} */


#ifdef __cplusplus
}
#endif


#endif
