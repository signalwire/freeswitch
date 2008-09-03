/*
 * SpanDSP - a series of DSP components for telephony
 *
 * async.h - Asynchronous serial bit stream encoding and decoding
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: async.h,v 1.16 2008/07/17 14:27:11 steveu Exp $
 */

/*! \file */

/*! \page async_page Asynchronous bit stream processing
\section async_page_sec_1 What does it do?
The asynchronous serial bit stream processing module provides
generation and decoding facilities for most asynchronous data
formats. It supports:
 - 1 or 2 stop bits.
 - Odd, even or no parity.
 - 5, 6, 7, or 8 bit characters.
 - V.14 rate adaption.
The input to this module is a bit stream. This means any symbol synchronisation
and decoding must occur before data is fed to this module.

\section async_page_sec_2 The transmitter
???.

\section async_page_sec_3 The receiver
???.
*/

#if !defined(_SPANDSP_ASYNC_H_)
#define _SPANDSP_ASYNC_H_

/*! Special "bit" values for the put and get bit functions */
enum
{
    /*! \brief The carrier signal has dropped. */
    PUTBIT_CARRIER_DOWN = -1,
    /*! \brief The carrier signal is up. This merely indicates that carrier
         energy has been seen. It is not an indication that the carrier is either
         valid, or of the expected type. */
    PUTBIT_CARRIER_UP = -2,
    /*! \brief The modem is training. This is an early indication that the
        signal seems to be of the right type. This may be needed in time critical
        applications, like T.38, to forward an early indication of what is happening
        on the wire. */
    PUTBIT_TRAINING_IN_PROGRESS = -3,
    /*! \brief The modem has trained, and is ready for data exchange. */
    PUTBIT_TRAINING_SUCCEEDED = -4,
    /*! \brief The modem has failed to train. */
    PUTBIT_TRAINING_FAILED = -5,
    /*! \brief Packet framing (e.g. HDLC framing) is OK. */
    PUTBIT_FRAMING_OK = -6,
    /*! \brief The data stream has ended. */
    PUTBIT_END_OF_DATA = -7,
    /*! \brief An abort signal (e.g. an HDLC abort) has been received. */
    PUTBIT_ABORT = -8,
    /*! \brief A break signal (e.g. an async break) has been received. */
    PUTBIT_BREAK = -9,
    /*! \brief Regular octet report for things like HDLC to the MTP standards. */
    PUTBIT_OCTET_REPORT = -10
};

enum
{
    /*! \brief The data source for a transmitter is exhausted. */
    MODEM_TX_STATUS_DATA_EXHAUSTED = -1,
    /*! \brief The transmitter has completed its task, and shut down. */
    MODEM_TX_STATUS_SHUTDOWN_COMPLETE = -2
};

/*! Message put function for data pumps */
typedef void (*put_msg_func_t)(void *user_data, const uint8_t *msg, int len);

/*! Message get function for data pumps */
typedef int (*get_msg_func_t)(void *user_data, uint8_t *msg, int max_len);

/*! Byte put function for data pumps */
typedef void (*put_byte_func_t)(void *user_data, int byte);

/*! Byte get function for data pumps */
typedef int (*get_byte_func_t)(void *user_data);

/*! Bit put function for data pumps */
typedef void (*put_bit_func_t)(void *user_data, int bit);

/*! Bit get function for data pumps */
typedef int (*get_bit_func_t)(void *user_data);

/*! Completion callback function for tx data pumps */
typedef int (*modem_tx_status_func_t)(void *user_data, int status);

/*! Completion callback function for rx data pumps */
typedef int (*modem_rx_status_func_t)(void *user_data, int status);

enum
{
    /*! No parity bit should be used */
    ASYNC_PARITY_NONE = 0,
    /*! An even parity bit will exist, after the data bits */
    ASYNC_PARITY_EVEN,
    /*! An odd parity bit will exist, after the data bits */
    ASYNC_PARITY_ODD
};

/*!
    Asynchronous data transmit descriptor. This defines the state of a single
    working instance of a byte to asynchronous serial converter, for use
    in FSK modems.
*/
typedef struct
{
    /*! \brief The number of data bits per character. */
    int data_bits;
    /*! \brief The type of parity. */
    int parity;
    /*! \brief The number of stop bits per character. */
    int stop_bits;
    /*! \brief A pointer to the callback routine used to get characters to be transmitted. */
    get_byte_func_t get_byte;
    /*! \brief An opaque pointer passed when calling get_byte. */
    void *user_data;

    /*! \brief A current, partially transmitted, character. */
    int byte_in_progress;
    /*! \brief The current bit position within a partially transmitted character. */
    int bitpos;
    /*! \brief Parity bit. */
    int parity_bit;
} async_tx_state_t;

/*!
    Asynchronous data receive descriptor. This defines the state of a single
    working instance of an asynchronous serial to byte converter, for use
    in FSK modems.
*/
typedef struct
{
    /*! \brief The number of data bits per character. */
    int data_bits;
    /*! \brief The type of parity. */
    int parity;
    /*! \brief The number of stop bits per character. */
    int stop_bits;
    /*! \brief TRUE if V.14 rate adaption processing should be performed. */
    int use_v14;
    /*! \brief A pointer to the callback routine used to handle received characters. */
    put_byte_func_t put_byte;
    /*! \brief An opaque pointer passed when calling put_byte. */
    void *user_data;

    /*! \brief A current, partially complete, character. */
    int byte_in_progress;
    /*! \brief The current bit position within a partially complete character. */
    int bitpos;
    /*! \brief Parity bit. */
    int parity_bit;

    /*! A count of the number of parity errors seen. */
    int parity_errors;
    /*! A count of the number of character framing errors seen. */
    int framing_errors;
} async_rx_state_t;

#if defined(__cplusplus)
extern "C"
{
#endif

/*! Initialise an asynchronous data transmit context.
    \brief Initialise an asynchronous data transmit context.
    \param s The transmitter context.
    \param data_bits The number of data bit.
    \param parity_bits The type of parity.
    \param stop_bits The number of stop bits.
    \param use_v14 TRUE if V.14 rate adaption processing should be used.
    \param get_byte The callback routine used to get the data to be transmitted.
    \param user_data An opaque pointer.
    \return A pointer to the initialised context, or NULL if there was a problem. */
async_tx_state_t *async_tx_init(async_tx_state_t *s,
                                int data_bits,
                                int parity_bits,
                                int stop_bits,
                                int use_v14,
                                get_byte_func_t get_byte,
                                void *user_data);

/*! Get the next bit of a transmitted serial bit stream.
    \brief Get the next bit of a transmitted serial bit stream.
    \param user_data An opaque point which must point to a transmitter context.
    \return the next bit, or PUTBIT_END_OF_DATA to indicate the data stream has ended. */
int async_tx_get_bit(void *user_data);

/*! Initialise an asynchronous data receiver context.
    \brief Initialise an asynchronous data receiver context.
    \param s The receiver context.
    \param data_bits The number of data bits.
    \param parity_bits The type of parity.
    \param stop_bits The number of stop bits.
    \param use_v14 TRUE if V.14 rate adaption processing should be used.
    \param put_byte The callback routine used to put the received data.
    \param user_data An opaque pointer.
    \return A pointer to the initialised context, or NULL if there was a problem. */
async_rx_state_t *async_rx_init(async_rx_state_t *s,
                                int data_bits,
                                int parity_bits,
                                int stop_bits,
                                int use_v14,
                                put_byte_func_t put_byte,
                                void *user_data);

/*! Accept a bit from a received serial bit stream
    \brief Accept a bit from a received serial bit stream
    \param user_data An opaque point which must point to a receiver context.
    \param bit The new bit. Some special values are supported for this field.
        - PUTBIT_CARRIER_UP
        - PUTBIT_CARRIER_DOWN
        - PUTBIT_TRAINING_SUCCEEDED
        - PUTBIT_TRAINING_FAILED
        - PUTBIT_END_OF_DATA */
void async_rx_put_bit(void *user_data, int bit);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/
