/****************************************************************************
 * sigboost.h     $Revision: 1.1 $
 *
 * Definitions for the sigboost interface.
 *
 * WARNING WARNING WARNING
 *
 * This file is used by sangoma_mgd and perhaps other programs. Any changes 
 * to this file must be coordinated with other user programs,
 *
 * Copyright (C) 2005  Xygnada Technology, Inc.
 * 
****************************************************************************/
#ifndef _SIGBOOST_H_
#define _SIGBOOST_H_

#include <stdint.h>

enum	e_sigboost_event_id_values
{
	SIGBOOST_EVENT_CALL_START			= 0x80, /*128*/
	SIGBOOST_EVENT_CALL_START_ACK			= 0x81, /*129*/
	SIGBOOST_EVENT_CALL_START_NACK			= 0x82, /*130*/
	SIGBOOST_EVENT_CALL_START_NACK_ACK		= 0x83, /*131*/
	SIGBOOST_EVENT_CALL_ANSWERED			= 0x84, /*132*/
	SIGBOOST_EVENT_CALL_STOPPED			= 0x85, /*133*/
	SIGBOOST_EVENT_CALL_STOPPED_ACK			= 0x86, /*134*/
	SIGBOOST_EVENT_SYSTEM_RESTART			= 0x87, /*135*/
	SIGBOOST_EVENT_HEARTBEAT			= 0x88, /*136*/
};

enum	e_sigboost_release_cause_values
{
	SIGBOOST_RELEASE_CAUSE_UNDEFINED		= 0x00,
	SIGBOOST_RELEASE_CAUSE_NORMAL			= 0x90,
	SIGBOOST_RELEASE_CAUSE_BUSY			= 0x91,
	SIGBOOST_RELEASE_CAUSE_CALLED_NOT_EXIST		= 0x92,
	SIGBOOST_RELEASE_CAUSE_CIRCUIT_RESET		= 0x93,
	SIGBOOST_RELEASE_CAUSE_NOANSWER			= 0x94
};

enum	e_sigboost_call_setup_ack_nack_cause_values
{
	SIGBOOST_CALL_SETUP_CIRCUIT_RESET		= 0x10,
	SIGBOOST_CALL_SETUP_NACK_CKT_START_TIMEOUT	= 0x11,
	SIGBOOST_CALL_SETUP_NACK_ALL_CKTS_BUSY		= 0x12,
	SIGBOOST_CALL_SETUP_NACK_CALLED_NUM_TOO_BIG	= 0x13,
	SIGBOOST_CALL_SETUP_NACK_CALLING_NUM_TOO_BIG	= 0x14,
	SIGBOOST_CALL_SETUP_NACK_CALLED_NUM_TOO_SMALL	= 0x15,
	SIGBOOST_CALL_SETUP_NACK_CALLING_NUM_TOO_SMALL	= 0x16,
};

#define MAX_DIALED_DIGITS	31

/* Next two defines are used to create the range of values for call_setup_id
 * in the t_sigboost structure.
 * 0..((CORE_MAX_SPANS * CORE_MAX_CHAN_PER_SPAN) - 1) */
#define CORE_MAX_SPANS 		200
#define CORE_MAX_CHAN_PER_SPAN 	30
#define MAX_PENDING_CALLS 	CORE_MAX_SPANS * CORE_MAX_CHAN_PER_SPAN
/* 0..(MAX_PENDING_CALLS-1) is range of call_setup_id below */

#pragma pack(1)
typedef struct
{
	uint32_t	event_id;
	uint32_t	seqno;
	uint32_t	call_setup_id;
	uint32_t	trunk_group;
	uint32_t	span;
	uint32_t	chan;
	uint32_t	called_number_digits_count;
	int8_t		called_number_digits [MAX_DIALED_DIGITS + 1]; /* it's a null terminated string */
	uint32_t	calling_number_digits_count; /* it's an array */
	int8_t		calling_number_digits [MAX_DIALED_DIGITS + 1]; /* it's a null terminated string */
	uint32_t	release_cause;
	struct timeval  tv;
	uint32_t	calling_number_presentation;
} t_sigboost;
#pragma pack()

#endif
