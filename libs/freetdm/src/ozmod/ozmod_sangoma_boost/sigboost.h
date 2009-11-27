/****************************************************************************
 * sigboost.h     $Revision: 1.13 $
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

#define SIGBOOST_VERSION 100

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
	SIGBOOST_EVENT_CALL_PROGRESS            = 0x50, /*decimal  80*/
	/* Following IDs are ss7boost to sangoma_mgd only. */
	SIGBOOST_EVENT_HEARTBEAT			= 0x89, /*137*/
	SIGBOOST_EVENT_INSERT_CHECK_LOOP		= 0x8a, /*138*/
	SIGBOOST_EVENT_REMOVE_CHECK_LOOP		= 0x8b, /*139*/
	SIGBOOST_EVENT_AUTO_CALL_GAP_ABATE		= 0x8c, /*140*/
	SIGBOOST_EVENT_DIGIT_IN					= 0x8d, /*141*/
};
enum	e_sigboost_release_cause_values
{
	SIGBOOST_RELEASE_CAUSE_UNDEFINED		= 0,
	SIGBOOST_RELEASE_CAUSE_NORMAL			= 16,
	/* probable elimination */
	//SIGBOOST_RELEASE_CAUSE_BUSY			= 0x91, /* 145 */
	//SIGBOOST_RELEASE_CAUSE_CALLED_NOT_EXIST	= 0x92, /* 146 */
	//SIGBOOST_RELEASE_CAUSE_CIRCUIT_RESET		= 0x93, /* 147 */
	//SIGBOOST_RELEASE_CAUSE_NOANSWER		= 0x94, /* 148 */
};

enum	e_sigboost_call_setup_ack_nack_cause_values
{
	//SIGBOOST_CALL_SETUP_NACK_ALL_CKTS_BUSY		= 34,  /* Q.850 value - don't use */
	SIGBOOST_CALL_SETUP_NACK_ALL_CKTS_BUSY		= 117,  /* non Q.850 value indicates local all ckt busy 
								   causing sangoma_mgd to perform automatic call 
								   gapping*/
	SIGBOOST_CALL_SETUP_NACK_TEST_CKT_BUSY		= 17,  /* Q.850 value */
	SIGBOOST_CALL_SETUP_NACK_INVALID_NUMBER		= 28,  /* Q.850 value */
	SIGBOOST_CALL_SETUP_CSUPID_DBL_USE		= 200, /* unused Q.850 value */
};


enum	e_sigboost_huntgroup_values
{
	SIGBOOST_HUNTGRP_SEQ_ASC	= 0x00, /* sequential with lowest available first */
	SIGBOOST_HUNTGRP_SEQ_DESC	= 0x01, /* sequential with highest available first */
	SIGBOOST_HUNTGRP_RR_ASC		= 0x02, /* round-robin with lowest available first */
	SIGBOOST_HUNTGRP_RR_DESC	= 0x03, /* round-robin with highest available first */
};

enum e_sigboost_event_info_par_values
{
  	SIGBOOST_EVI_SPARE 				   	 	= 0x00, 
  	SIGBOOST_EVI_ALERTING 					= 0x01, 
  	SIGBOOST_EVI_PROGRESS 					= 0x02, 
};


#define MAX_DIALED_DIGITS	31

/* Next two defines are used to create the range of values for call_setup_id
 * in the t_sigboost structure.
 * 0..((CORE_MAX_SPANS * CORE_MAX_CHAN_PER_SPAN) - 1) */
#define CORE_MAX_SPANS 		200
#define CORE_MAX_CHAN_PER_SPAN 	32
#define MAX_PENDING_CALLS 	CORE_MAX_SPANS * CORE_MAX_CHAN_PER_SPAN
/* 0..(MAX_PENDING_CALLS-1) is range of call_setup_id below */
#define SIZE_RDNIS	900


#pragma pack(1)

typedef struct
{
	uint8_t			capability;
	uint8_t			uil1p;
}t_sigboost_bearer;

typedef struct
{
	uint16_t		version;
	uint32_t		event_id;
	/* delete sequence numbers - SCTP does not need them */
	uint32_t		fseqno;
	uint32_t		bseqno;
	uint16_t		call_setup_id;
	uint32_t		trunk_group;
	uint8_t			span;
	uint8_t			chan;
	/* struct timeval  	tv; */ 
	uint8_t			called_number_digits_count;
	char			called_number_digits [MAX_DIALED_DIGITS + 1]; /* it's a null terminated string */
	uint8_t			calling_number_digits_count; /* it's an array */
	char			calling_number_digits [MAX_DIALED_DIGITS + 1]; /* it's a null terminated string */
	/* ref. Q.931 Table 4-11 and Q.951 Section 3 */
	uint8_t			calling_number_screening_ind;
	uint8_t			calling_number_presentation;
	char			calling_name[MAX_DIALED_DIGITS + 1];
	t_sigboost_bearer 	bearer;
	uint8_t			hunt_group;
	uint16_t		isup_in_rdnis_size;
	char			isup_in_rdnis [SIZE_RDNIS]; /* it's a null terminated string */
} t_sigboost_callstart;

#define MIN_SIZE_CALLSTART_MSG  sizeof(t_sigboost_callstart) - SIZE_RDNIS

typedef struct
{
	uint16_t		version;
	uint32_t		event_id;
	/* delete sequence numbers - SCTP does not need them */
	uint32_t		fseqno;
	uint32_t		bseqno;
	uint16_t		call_setup_id;
	uint32_t		trunk_group;
	uint8_t			span;
	uint8_t			chan;
	/* struct timeval  	tv; */ 
	uint8_t			release_cause;
} t_sigboost_short;
#pragma pack()


static inline int boost_full_event(int event_id)
{
        switch (event_id) {
        case SIGBOOST_EVENT_CALL_START:
        case SIGBOOST_EVENT_DIGIT_IN:
		case SIGBOOST_EVENT_CALL_PROGRESS:
                return 1;
        default:
                return 0;
        }

        return 0;
}

#endif
