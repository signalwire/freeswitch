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

#define SIGBOOST_VERSION 103

// handy to define integer types that actually work on both Lin and Win
#include <freetdm.h>

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
       /* CALL_RELEASED is aimed to fix a race condition that became obvious
        * when the boost socket was replaced by direct function calls
        * and the channel hunting was moved to freetdm, the problem is
        * we can get CALL_STOPPED msg and reply with CALL_STOPPED_ACK
        * but the signaling module will still (in PRI) send RELEASE and
        * wait for RELEASE_COMPLETE from the isdn network before
        * marking the channel as available, therefore freetdm should
        * also not mark the channel as available until CALL_RELEASED
        * is received, for socket mode we can continue working as usual
        * with CALL_STOPPED being the last step because the hunting is
        * done in the signaling module.
        * */
	SIGBOOST_EVENT_CALL_RELEASED                    = 0x51, /* 81 */
	SIGBOOST_EVENT_CALL_PROGRESS            	= 0x50, /*decimal  80*/
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

enum e_sigboost_progress_flags
{
	SIGBOOST_PROGRESS_RING = (1 << 0),
	SIGBOOST_PROGRESS_MEDIA = (1 << 1)
};

#define MAX_DIALED_DIGITS	31

/* Next two defines are used to create the range of values for call_setup_id
 * in the t_sigboost structure.
 * 0..((CORE_MAX_SPANS * CORE_MAX_CHAN_PER_SPAN) - 1) */
#define CORE_MAX_SPANS 		200
#define CORE_MAX_CHAN_PER_SPAN 	32
#define MAX_PENDING_CALLS 	CORE_MAX_SPANS * CORE_MAX_CHAN_PER_SPAN
/* 0..(MAX_PENDING_CALLS-1) is range of call_setup_id below */

/* Should only be used by server */
#define MAX_CALL_SETUP_ID   0xFFFF

#define SIZE_CUSTOM	900
#define SIZE_RDNIS  SIZE_CUSTOM


#pragma pack(1)

typedef struct
{
	uint8_t			capability;
	uint8_t			uil1p;
} t_sigboost_bearer;

typedef struct
{
	uint8_t			digits_count;
	char			digits [MAX_DIALED_DIGITS + 1]; /* it's a null terminated string */
	uint8_t 		npi;
	uint8_t 		ton;
	uint8_t			screening_ind;
	uint8_t			presentation_ind;
}t_sigboost_digits;

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
	uint32_t		flags;
	/* struct timeval  	tv; */ 
	t_sigboost_digits called;
	t_sigboost_digits calling;
	t_sigboost_digits rdnis;
	/* ref. Q.931 Table 4-11 and Q.951 Section 3 */
	char			calling_name[MAX_DIALED_DIGITS + 1];
	t_sigboost_bearer 	bearer;
	uint8_t			hunt_group;
	uint16_t		custom_data_size;
	char			custom_data[SIZE_CUSTOM]; /* it's a null terminated string */

} t_sigboost_callstart;

#define called_number_digits_count		called.digits_count
#define called_number_digits			called.digits
#define calling_number_digits_count		calling.digits_count
#define calling_number_digits			calling.digits
#define calling_number_screening_ind	calling.screening_ind
#define calling_number_presentation		calling.presentation_ind

#define isup_in_rdnis_size				custom_data_size
#define isup_in_rdnis					custom_data


#define MIN_SIZE_CALLSTART_MSG  sizeof(t_sigboost_callstart) - SIZE_CUSTOM

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
	uint32_t		flags;
	/* struct timeval  	tv; */ 
	uint8_t			release_cause;
} t_sigboost_short;
#pragma pack()


static __inline__ int boost_full_event(int event_id)
{
        switch (event_id) {
        case SIGBOOST_EVENT_CALL_START:
        case SIGBOOST_EVENT_DIGIT_IN:
	case SIGBOOST_EVENT_CALL_PROGRESS:
		return 1;
        default:
		break;
        }

        return 0;
}

#endif
