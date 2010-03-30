/*
 * Copyright (c) 2007, Anthony Minessale II
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ZAP_BUFFER_H
#define ZAP_BUFFER_H

#include "openzap.h"

/**
 * @defgroup zap_buffer Buffer Routines
 * @ingroup buffer
 * The purpose of this module is to make a plain buffering interface that can be used for read/write buffers
 * throughout the application.
 * @{
 */
struct zap_buffer;
typedef struct zap_buffer zap_buffer_t;

/*! \brief Allocate a new dynamic zap_buffer 
 * \param buffer returned pointer to the new buffer
 * \param blocksize length to realloc by as data is added
 * \param start_len ammount of memory to reserve initially
 * \param max_len length the buffer is allowed to grow to
 * \return status
 */
OZ_DECLARE(zap_status_t) zap_buffer_create(zap_buffer_t **buffer, zap_size_t blocksize, zap_size_t start_len, zap_size_t max_len);

/*! \brief Get the length of a zap_buffer_t 
 * \param buffer any buffer of type zap_buffer_t
 * \return int size of the buffer.
 */
OZ_DECLARE(zap_size_t) zap_buffer_len(zap_buffer_t *buffer);

/*! \brief Get the freespace of a zap_buffer_t 
 * \param buffer any buffer of type zap_buffer_t
 * \return int freespace in the buffer.
 */
OZ_DECLARE(zap_size_t) zap_buffer_freespace(zap_buffer_t *buffer);

/*! \brief Get the in use amount of a zap_buffer_t 
 * \param buffer any buffer of type zap_buffer_t
 * \return int ammount of buffer curently in use
 */
OZ_DECLARE(zap_size_t) zap_buffer_inuse(zap_buffer_t *buffer);

/*! \brief Read data from a zap_buffer_t up to the ammount of datalen if it is available.  Remove read data from buffer. 
 * \param buffer any buffer of type zap_buffer_t
 * \param data pointer to the read data to be returned
 * \param datalen amount of data to be returned
 * \return int ammount of data actually read
 */
OZ_DECLARE(zap_size_t) zap_buffer_read(zap_buffer_t *buffer, void *data, zap_size_t datalen);

/*! \brief Read data endlessly from a zap_buffer_t 
 * \param buffer any buffer of type zap_buffer_t
 * \param data pointer to the read data to be returned
 * \param datalen amount of data to be returned
 * \return int ammount of data actually read
 * \note Once you have read all the data from the buffer it will loop around.
 */
OZ_DECLARE(zap_size_t) zap_buffer_read_loop(zap_buffer_t *buffer, void *data, zap_size_t datalen);

/*! \brief Assign a number of loops to read
 * \param buffer any buffer of type zap_buffer_t
 * \param loops the number of loops (-1 for infinite)
 */
OZ_DECLARE(void) zap_buffer_set_loops(zap_buffer_t *buffer, int32_t loops);

/*! \brief Write data into a zap_buffer_t up to the length of datalen
 * \param buffer any buffer of type zap_buffer_t
 * \param data pointer to the data to be written
 * \param datalen amount of data to be written
 * \return int amount of buffer used after the write, or 0 if no space available
 */
OZ_DECLARE(zap_size_t) zap_buffer_write(zap_buffer_t *buffer, const void *data, zap_size_t datalen);

/*! \brief Remove data from the buffer
 * \param buffer any buffer of type zap_buffer_t
 * \param datalen amount of data to be removed
 * \return int size of buffer, or 0 if unable to toss that much data
 */
OZ_DECLARE(zap_size_t) zap_buffer_toss(zap_buffer_t *buffer, zap_size_t datalen);

/*! \brief Remove all data from the buffer
 * \param buffer any buffer of type zap_buffer_t
 */
OZ_DECLARE(void) zap_buffer_zero(zap_buffer_t *buffer);

/*! \brief Destroy the buffer
 * \param buffer buffer to destroy
 * \note only neccessary on dynamic buffers (noop on pooled ones)
 */
OZ_DECLARE(void) zap_buffer_destroy(zap_buffer_t **buffer);

/*! \brief Seek to offset from the beginning of the buffer
 * \param buffer buffer to seek
 * \param datalen offset in bytes
 * \return new position
 */
OZ_DECLARE(zap_size_t) zap_buffer_seek(zap_buffer_t *buffer, zap_size_t datalen);

/** @} */

OZ_DECLARE(zap_size_t) zap_buffer_zwrite(zap_buffer_t *buffer, const void *data, zap_size_t datalen);

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
