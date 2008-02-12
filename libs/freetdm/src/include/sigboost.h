/****************************************************************************
 * sigboost.h     $Revision: 1.3 $
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
#include <sys/time.h>

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
	SIGBOOST_EVENT_SYSTEM_RESTART_ACK		= 0x88, /*136*/
	/* Following IDs are ss7boost to sangoma_mgd only. */
	SIGBOOST_EVENT_HEARTBEAT			= 0x89, /*137*/
	SIGBOOST_EVENT_INSERT_CHECK_LOOP		= 0x8a, /*138*/
	SIGBOOST_EVENT_REMOVE_CHECK_LOOP		= 0x8b, /*139*/
	SIGBOOST_EVENT_AUTO_CALL_GAP_ABATE		= 0x8c, /*140*/
};
enum	e_sigboost_release_cause_values
{
	SIGBOOST_RELEASE_CAUSE_UNDEFINED		= 0,
	SIGBOOST_RELEASE_CAUSE_NORMAL			= 16,
	SIGBOOST_RELEASE_CAUSE_BUSY			= 17,
	/* probable elimination */
	//SIGBOOST_RELEASE_CAUSE_BUSY			= 0x91, /* 145 */
	//SIGBOOST_RELEASE_CAUSE_CALLED_NOT_EXIST	= 0x92, /* 146 */
	//SIGBOOST_RELEASE_CAUSE_CIRCUIT_RESET		= 0x93, /* 147 */
	//SIGBOOST_RELEASE_CAUSE_NOANSWER		= 0x94, /* 148 */
};

enum	e_sigboost_call_setup_ack_nack_cause_values
{
	SIGBOOST_CALL_SETUP_NACK_ALL_CKTS_BUSY		= 117, /* unused Q.850 value */
	SIGBOOST_CALL_SETUP_NACK_TEST_CKT_BUSY		= 118, /* unused Q.850 value */
	SIGBOOST_CALL_SETUP_NACK_INVALID_NUMBER		= 28,
	/* probable elimination */
	//SIGBOOST_CALL_SETUP_RESERVED			= 0x00,
	//SIGBOOST_CALL_SETUP_CIRCUIT_RESET		= 0x10,
	//SIGBOOST_CALL_SETUP_NACK_CKT_START_TIMEOUT	= 0x11,
	//SIGBOOST_CALL_SETUP_NACK_AUTO_CALL_GAP	= 0x17,
};

#define MAX_DIALED_DIGITS	31
#define MAX_CALLING_NAME	31

/* Next two defines are used to create the range of values for call_setup_id
 * in the t_sigboost structure.
 * 0..((CORE_MAX_SPANS * CORE_MAX_CHAN_PER_SPAN) - 1) */
#define CORE_MAX_SPANS 		200
#define CORE_MAX_CHAN_PER_SPAN 	30
#define MAX_PENDING_CALLS 	CORE_MAX_SPANS * CORE_MAX_CHAN_PER_SPAN
/* 0..(MAX_PENDING_CALLS-1) is range of call_setup_id below */
#define SIZE_RDNIS		80

//#undef MSGWINDOW
#define MSGWINDOW

#pragma pack(1)
typedef struct
{
	uint32_t	event_id;
	uint32_t	fseqno;
#ifdef MSGWINDOW
	uint32_t	bseqno;
#endif
	uint16_t	call_setup_id;
	uint32_t	trunk_group;
	uint32_t	span;
	uint32_t	chan;
	uint8_t		called_number_digits_count;
	char		called_number_digits [MAX_DIALED_DIGITS + 1]; /* it's a null terminated string */
	uint8_t		calling_number_digits_count; /* it's an array */
	char		calling_number_digits [MAX_DIALED_DIGITS + 1]; /* it's a null terminated string */
	uint8_t		release_cause;
	struct timeval  tv;
	/* ref. Q.931 Table 4-11 and Q.951 Section 3 */
	uint8_t		calling_number_screening_ind;
	uint8_t		calling_number_presentation;
	char		redirection_string [SIZE_RDNIS]; /* it's a null terminated string */
	/* redir string format: 
	 * http://www.ss7box.com/wiki/tiki-index.php?page=Call+Redirection
	 * */
} t_sigboost;
#pragma pack()

#endif
