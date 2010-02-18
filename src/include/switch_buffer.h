/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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
 * switch_buffer.h -- Data Buffering Code
 *
 */
/** 
 * @file switch_buffer.h
 * @brief Data Buffering Code
 * @see switch_buffer
 */

#ifndef SWITCH_BUFFER_H
#define SWITCH_BUFFER_H

#include <switch.h>

SWITCH_BEGIN_EXTERN_C
/**
 * @defgroup switch_buffer Buffer Routines
 * @ingroup core1
 * The purpose of this module is to make a plain buffering interface that can be used for read/write buffers
 * throughout the application.  The first implementation was done to provide the functionality and the interface
 * and I think it can be optimized under the hood as we go using bucket brigades and/or ring buffering techniques.
 * @{
 */
	struct switch_buffer;


/*! \brief Allocate a new switch_buffer 
 * \param pool Pool to allocate the buffer from
 * \param buffer returned pointer to the new buffer
 * \param max_len length required by the buffer
 * \return status
 */
SWITCH_DECLARE(switch_status_t) switch_buffer_create(_In_ switch_memory_pool_t *pool, _Out_ switch_buffer_t **buffer, _In_ switch_size_t max_len);

/*! \brief Allocate a new dynamic switch_buffer 
 * \param buffer returned pointer to the new buffer
 * \param blocksize length to realloc by as data is added
 * \param start_len ammount of memory to reserve initially
 * \param max_len length the buffer is allowed to grow to
 * \return status
 */
SWITCH_DECLARE(switch_status_t) switch_buffer_create_dynamic(_Out_ switch_buffer_t **buffer, _In_ switch_size_t blocksize, _In_ switch_size_t start_len,
															 _In_ switch_size_t max_len);

SWITCH_DECLARE(void) switch_buffer_add_mutex(_In_ switch_buffer_t *buffer, _In_ switch_mutex_t *mutex);
SWITCH_DECLARE(void) switch_buffer_lock(_In_ switch_buffer_t *buffer);
SWITCH_DECLARE(switch_status_t) switch_buffer_trylock(_In_ switch_buffer_t *buffer);
SWITCH_DECLARE(void) switch_buffer_unlock(_In_ switch_buffer_t *buffer);

/*! \brief Get the length of a switch_buffer_t 
 * \param buffer any buffer of type switch_buffer_t
 * \return int size of the buffer.
 */
SWITCH_DECLARE(switch_size_t) switch_buffer_len(_In_ switch_buffer_t *buffer);

/*! \brief Get the freespace of a switch_buffer_t 
 * \param buffer any buffer of type switch_buffer_t
 * \return int freespace in the buffer.
 */
SWITCH_DECLARE(switch_size_t) switch_buffer_freespace(_In_ switch_buffer_t *buffer);

/*! \brief Get the in use amount of a switch_buffer_t 
 * \param buffer any buffer of type switch_buffer_t
 * \return int ammount of buffer curently in use
 */
SWITCH_DECLARE(switch_size_t) switch_buffer_inuse(_In_ switch_buffer_t *buffer);

/*! \brief Read data from a switch_buffer_t up to the ammount of datalen if it is available.  Remove read data from buffer. 
 * \param buffer any buffer of type switch_buffer_t
 * \param data pointer to the read data to be returned
 * \param datalen amount of data to be returned
 * \return int ammount of data actually read
 */
SWITCH_DECLARE(switch_size_t) switch_buffer_read(_In_ switch_buffer_t *buffer, _In_ void *data, _In_ switch_size_t datalen);

/*! \brief Read data from a switch_buffer_t up to the ammount of datalen if it is available, without removing read data from buffer. 
 * \param buffer any buffer of type switch_buffer_t
 * \param data pointer to the read data to be returned
 * \param datalen amount of data to be returned
 * \return int ammount of data actually read
 */
SWITCH_DECLARE(switch_size_t) switch_buffer_peek(_In_ switch_buffer_t *buffer, _In_ void *data, _In_ switch_size_t datalen);

/*! \brief Read data endlessly from a switch_buffer_t 
 * \param buffer any buffer of type switch_buffer_t
 * \param data pointer to the read data to be returned
 * \param datalen amount of data to be returned
 * \return int ammount of data actually read
 * \note Once you have read all the data from the buffer it will loop around.
 */
SWITCH_DECLARE(switch_size_t) switch_buffer_read_loop(_In_ switch_buffer_t *buffer, _In_ void *data, _In_ switch_size_t datalen);

/*! \brief Assign a number of loops to read
 * \param buffer any buffer of type switch_buffer_t
 * \param loops the number of loops (-1 for infinite)
 */
SWITCH_DECLARE(void) switch_buffer_set_loops(_In_ switch_buffer_t *buffer, _In_ int32_t loops);

/*! \brief Write data into a switch_buffer_t up to the length of datalen
 * \param buffer any buffer of type switch_buffer_t
 * \param data pointer to the data to be written
 * \param datalen amount of data to be written
 * \return int amount of buffer used after the write, or 0 if no space available
 */
SWITCH_DECLARE(switch_size_t) switch_buffer_write(_In_ switch_buffer_t *buffer, _In_bytecount_(datalen)
												  const void *data, _In_ switch_size_t datalen);

/*! \brief Remove data from the buffer
 * \param buffer any buffer of type switch_buffer_t
 * \param datalen amount of data to be removed
 * \return int size of buffer, or 0 if unable to toss that much data
 */
SWITCH_DECLARE(switch_size_t) switch_buffer_toss(_In_ switch_buffer_t *buffer, _In_ switch_size_t datalen);

/*! \brief Remove all data from the buffer
 * \param buffer any buffer of type switch_buffer_t
 */
SWITCH_DECLARE(void) switch_buffer_zero(_In_ switch_buffer_t *buffer);

SWITCH_DECLARE(switch_size_t) switch_buffer_slide_write(switch_buffer_t *buffer, const void *data, switch_size_t datalen);

/*! \brief Destroy the buffer
 * \param buffer buffer to destroy
 * \note only neccessary on dynamic buffers (noop on pooled ones)
 */
SWITCH_DECLARE(void) switch_buffer_destroy(switch_buffer_t **buffer);

SWITCH_DECLARE(switch_size_t) switch_buffer_zwrite(_In_ switch_buffer_t *buffer, _In_bytecount_(datalen)
												   const void *data, _In_ switch_size_t datalen);

/** @} */

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
