/******************************************************************************

  FileName:         Q931.h

  Contents:         Header and definition for the ITU-T Q.931 stack. The 
                    header contents the following parts:

                    - Definition of codes
                    - Definition of information elements (Q931ie_).
                    - Definition of messages (Q931mes_).
                    - Definitian of variables (var_).
                    - Function prototypes.

  Description:		The Q.931 stack provided here covers ITU-T Q.931 w/Q.932
                    supplementary services for both PRI, BRI and variants. 
                    The stack is generic and designed to deal with variants as
                    needed.

                    The stack uses the following interface functions:

                    - Q931Initialize	Initialize the Q.931 stack.
                    - Q931Rx23			Receive a message from layer 2
                    - Q931Tx32			Send a message to layer 2
                    - Q931Rx43			Receive a message from layer 4 or above.
                    - Q931Tx34			Send a message to layer 4 or above.
                    - Q931TimeTick		Periodical timer processing.
                    - Q931ErrorProc		Callback for stack error message.

                    The protocol is a module with no external dependencies and
                    can easely be ported to any operating system like Windows,
                    Linux, rtos and others.

  Related Files:	Q931.h				Q.931 Definitions
                    Q931.c				Q.931 Interface Functions.
                    Q931api.c			Low level L4 API functions.

                    Q932.h				Q.932 Suplementary Services
                    Q932mes.c			Q.932 encoders/coders

                    Q931mes.c			Q.931 Message encoders/coders
                    Q931ie.c			Q.931 IE encoders/coders
                    Q931StateTE.c		Generic Q.931 TE State Engine
                    Q931StateNT.c		Generic Q.931 NT State Engine

  Design Note 1:	For each variant please add separate files starting with 
                    the	variant short-name as follows:

                    <variant>.h			Spesific headers needed.
                    <variant>mes.c		Message encoders/decores.
                    <variant>ie.c		IE encoders/decoders.
                    <variant>StateTE.c	TE side state engine.
                    <variant>StateNT.c	NT side state engine.

  Design Note 2:	The stack is deliberatly made non-threading. Use 1 
                    thread per Trunk, but lock access from the timertick
                    and rx, tx functions. And make sure the callbacks only
                    dump messages to a queue, no time-consuming processing
                    inside stack processing. 

                    All stack processing is async 'fire and forget', meaning
                    that there are not, and should not be any time-consuming
                    processing within the stack-time. The best way to thread 
                    a stack is to use one single thread that signal 5 queues.
                    
                    - Incoming L2 queue.
                    - Incoming L4 queue.
                    - Outgoing L2 queue.
                    - Outgoing L4 queue.
                    - Error/Trace queue.

  Design Note 3:	DSP optimization. The L3 (Rx23) can be called directly
                    from a HDLC receiver without usage of queues for optimized 
                    processing. But keep in mind that Q.931 calls Tx34 or Tx32 
                    as part	of receiving a message from Layer 2.

  License/Copyright:

  Copyright (c) 2007, Jan Vidar Berger, Case Labs, Ltd. All rights reserved.
  email:janvb@caselaboratories.com  

  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions are 
  met:

    * Redistributions of source code must retain the above copyright notice, 
	  this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, 
	  this list of conditions and the following disclaimer in the documentation 
	  and/or other materials provided with the distribution.
    * Neither the name of the Case Labs, Ltd nor the names of its contributors 
	  may be used to endorse or promote products derived from this software 
	  without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
  POSSIBILITY OF SUCH DAMAGE.
******************************************************************************/

#ifndef _Q931_NL
#define _Q931_NL

/* uncomment the #define below to add x.25 support to the Q.931				*/
/* #define Q931_X25_SUPPORT */

#include "stdio.h"

#ifdef _MSC_VER
#pragma warning(disable:4100)
#ifndef strcasecmp
#define strcasecmp(s1, s2) _stricmp(s1, s2)
#endif
#endif
#include <string.h>


/*****************************************************************************

  Enum helper macros  <Need description of these macros>

*****************************************************************************/
#define Q931_ENUM_NAMES(_NAME, _STRINGS) static const char * _NAME [] = { _STRINGS , NULL };
#define Q931_STR2ENUM_P(_FUNC1, _FUNC2, _TYPE) _TYPE _FUNC1 (const char *name); const char * _FUNC2 (_TYPE type);
#define Q931_STR2ENUM(_FUNC1, _FUNC2, _TYPE, _STRINGS, _MAX)	\
	_TYPE _FUNC1 (const char *name)								\
	{														\
		int i;												\
		_TYPE t = _MAX ;									\
															\
		for (i = 0; i < _MAX ; i++) {						\
			if (!strcasecmp(name, _STRINGS[i])) {			\
				t = (_TYPE) i;								\
				break;										\
			}												\
		}													\
															\
		return t;											\
	}														\
	const char * _FUNC2 (_TYPE type)						\
	{														\
		if (type > _MAX) {									\
			type = _MAX;									\
		}													\
		return _STRINGS[(int)type];							\
	}														\

/*****************************************************************************

  Error Codes

*****************************************************************************/
typedef enum {
	Q931E_NO_ERROR				=	0,

	Q931E_UNKNOWN_MESSAGE		=	-3001,
	Q931E_ILLEGAL_IE			=	-3002,
	Q931E_UNKNOWN_IE			=	-3003,
	Q931E_BEARERCAP				=	-3004,
	Q931E_HLCOMP				=	-3005,
	Q931E_LLCOMP				=	-3006,
	Q931E_INTERNAL              =	-3007,
	Q931E_MISSING_CB            =	-3008,
	Q931E_UNEXPECTED_MESSAGE    =	-3009,
	Q931E_ILLEGAL_MESSAGE		=	-3010,
	Q931E_TOMANYCALLS           =	-3011,
	Q931E_INVALID_CRV           =	-3012,
	Q931E_CALLID                =	-3013,
	Q931E_CALLSTATE             =	-3014,
	Q931E_CALLEDSUB             =	-3015,
	Q931E_CALLEDNUM             =	-3016,
	Q931E_CALLINGNUM            =	-3017,
	Q931E_CALLINGSUB            =	-3018,
	Q931E_CAUSE                 =	-3019,
	Q931E_CHANID                =	-3020,
	Q931E_DATETIME              =	-3021,
	Q931E_DISPLAY               =	-3022,
	Q931E_KEYPADFAC             =	-3023,
	Q931E_NETFAC                =	-3024,
	Q931E_NOTIFIND              =	-3025,
	Q931E_PROGIND               =	-3026,
	Q931E_RESTARTIND            =	-3027,
	Q931E_SEGMENT               =	-3028,
	Q931E_SIGNAL                =	-3029,
	Q931E_GENERIC_DIGITS		=	-3030
} q931_error_t;

/* The q931_error_t enum should be kept in sync with the q931_error_names array in Q931.c */ 

const char *q931_error_to_name(q931_error_t error);

/*****************************************************************************

	Some speed optimization can be achieved by changing all variables to the 
	word size of your processor. A 32 bit processor has to do a lot of extra 
	work to read a packed 8 bit integer. Changing all fields to 32 bit integer 
	will result in usage of some extra space, but will speed up the stack.

	The stack has been designed to allow L3UCHAR etc. to be any size of 8 bit
	or larger.

*****************************************************************************/

#define L3UCHAR		unsigned char		/* Min 8 bit						*/
#define L3USHORT	unsigned short		/* Min 16 bit unsigned				*/
#define L3UINT		unsigned int		/* Min 16 bit unsigned	            */
#define L3INT       int                 /* Min 16 bit signed                */
#define L3ULONG		unsigned long		/* Min 32 bit						*/
#define L3BOOL      char				/* Min 1 bit, valuse 0 & 1 only		*/

#define L3TRUE      1
#define L3FALSE     0 

/*****************************************************************************

  Global defines.

*****************************************************************************/

typedef L3USHORT ie;                /* Special data type to hold a dynamic  */
                                    /* or optional information element as   */
                                    /* part of a message struct. MSB = 1    */
                                    /* indicate that the ie is present, the */
                                    /* last 15 bits is an offset ( or the   */
                                    /* value for single octet ) to the      */
                                    /* struct holding the ie. Offset = 0    */
                                    /* is buf[1] etc.                       */
                                    /* ie == 0xffff indicates error         */

/*****************************************************************************
	
	MAXTRUNKS sets how many physical trunks this system might have. This 
	number should be keept at a minimum since it will use global space.

	It is recommended that you leave MAXCHPERTRUNK as is

*****************************************************************************/

#define	Q931_LOGBUFSIZE	1024		/* size of logging buffer		*/

#define Q931L4BUF	1000		/* size of message buffer		*/

#define Q931L2BUF	300		/* size of message buffer		*/

#define Q931MAXTRUNKS	4		/* Total number of trunks that will be	*/
					/* processed by this instance of the	*/
					/* stack				*/

#define Q931MAXCHPERTRUNK	32	/* Number of channels per trunk. The	*/
					/* stack uses a static set of 32	*/
					/* channels regardless if it is E1, T1	*/
					/* or BRI that actually is used.	*/

#define Q931MAXCALLPERTRUNK	(Q931MAXCHPERTRUNK * 2)
					/* Number of max active CRV per trunk.  */
					/* Q.931 can have more calls than there */
					/* are channels.			*/


#define Q931_IS_BRI(x)		((x)->TrunkType == Q931_TrType_BRI || (x)->TrunkType == Q931_TrType_BRI_PTMP)
#define Q931_IS_PRI(x)		(!Q931_IS_BRI(x))

#define Q931_IS_PTP(x)		((x)->TrunkType != Q931_TrType_BRI_PTMP)
#define Q931_IS_PTMP(X)		((x)->TrunkType == Q931_TrType_BRI_PTMP)

#define Q931_BRI_MAX_CRV	127
#define Q931_PRI_MAX_CRV	32767

/*****************************************************************************

  The following defines control the dialect switch tables and should only be
  changed when a new dialect needs to be inserted into the stack.   

  This stack uses an array of functions to know which function to call as   
  it receives a SETUP message etc. A new dialect can when choose to use
  the proc etc. for standard Q.931 or insert a modified proc.

  This technique has also been used to distinguish between user and network
  mode to make the code as easy to read and maintainable as possible.

  A message and IE index have been used to save space. These indexes allowes
  the message or IE code to be used directly and will give back a new index
  into the table.

*****************************************************************************/

/* WARNING! Initialize Q931CreateDialectCB[] will NULL when increasing the */
/* Q931MAXDLCT value to avoid Q931Initialize from crashing if one entry is */
/* not used.								   */
#define Q931MAXDLCT	8	/* Max dialects included in this        */
				/* compile. User and Network count as   */
				/* one dialect each.                    */

#define Q931MAXMES	128		/* Number of messages				*/
#define Q931MAXIE	255		/* Number of IE					*/
#define Q931MAXUSEDIE	50		/* Maximum number of ie types per Dialect	*/
#define Q931MAXCODESETS	7		/* Maximum number of codests (by spec, 0-7)	*/
#define Q931MAXSTATE	100		/* Size of state tables			        */
#define Q931MAXTIMER	25		/* Maximum number of timers 			*/

/*****************************************************************************

  Call States for ITU-T Q.931 TE (User Mode)

*****************************************************************************/

#define Q931_U0     0
#define Q931_U1     1
#define Q931_U2     2
#define Q931_U3     3
#define Q931_U4     4
#define Q931_U6     6
#define Q931_U7     7
#define Q931_U8     8
#define Q931_U9     9
#define Q931_U10    10
#define Q931_U11    11
#define Q931_U12    12
#define Q931_U15    15
#define Q931_U17    17
#define Q931_U19    19
#define Q931_U25    25

/*****************************************************************************

  Call States for ITU-T Q.931 NT (Network Mode)

*****************************************************************************/
#define Q931_N0     (0x0100 | 0)
#define Q931_N1     (0x0100 | 1)
#define Q931_N2     (0x0100 | 2)
#define Q931_N3     (0x0100 | 3)
#define Q931_N4     (0x0100 | 4)
#define Q931_N6     (0x0100 | 6)
#define Q931_N7     (0x0100 | 7)
#define Q931_N8     (0x0100 | 8)
#define Q931_N9     (0x0100 | 9)
#define Q931_N10    (0x0100 | 11)
#define Q931_N11    (0x0100 | 11)
#define Q931_N12    (0x0100 | 12)
#define Q931_N15    (0x0100 | 15)
#define Q931_N17    (0x0100 | 17)
#define Q931_N19    (0x0100 | 19)
#define Q931_N22    (0x0100 | 22)
#define Q931_N25    (0x0100 | 25)

/*****************************************************************************

  Q.931 Message codes
  
*****************************************************************************/

#define Q931mes_ALERTING             0x01 /* 0000 0001                   */        
#define Q931mes_CALL_PROCEEDING      0x02 /* 0000 0010                   */
#define Q931mes_CONNECT              0x07 /* 0000 0111                   */
#define Q931mes_CONNECT_ACKNOWLEDGE  0x0f /* 0000 1111                   */
#define Q931mes_PROGRESS             0x03 /* 0000 0011                   */
#define Q931mes_SETUP                0x05 /* 0000 0101                   */
#define Q931mes_SETUP_ACKNOWLEDGE    0x0d /* 0000 1101                   */
#define Q931mes_RESUME               0x26 /* 0010 0110                   */
#define Q931mes_RESUME_ACKNOWLEDGE   0x2e /* 0010 1110                   */
#define Q931mes_RESUME_REJECT        0x22 /* 0010 0010                   */
#define Q931mes_SUSPEND              0x25 /* 0010 0101                   */
#define Q931mes_SUSPEND_ACKNOWLEDGE  0x2d /* 0010 1101                   */
#define Q931mes_SUSPEND_REJECT       0x21 /* 0010 0001                   */
#define Q931mes_USER_INFORMATION     0x20 /* 0010 0000                   */
#define Q931mes_DISCONNECT           0x45 /* 0100 0101                   */
#define Q931mes_RELEASE              0x4d /* 0100 1101                   */
#define Q931mes_RELEASE_COMPLETE     0x5a /* 0101 1010                   */
#define Q931mes_RESTART              0x46 /* 0100 0110                   */
#define Q931mes_RESTART_ACKNOWLEDGE  0x4e /* 0100 1110                   */
#define Q931mes_CONGESTION_CONTROL   0x79 /* 0111 1001                   */
#define Q931mes_INFORMATION          0x7b /* 0111 1011                   */
#define Q931mes_NOTIFY               0x6e /* 0110 1110                   */
#define Q931mes_STATUS               0x7d /* 0111 1101                   */
#define Q931mes_STATUS_ENQUIRY       0x75 /* 0111 0101                   */
#define Q931mes_SEGMENT              0x60 /* 0110 0000                   */

#define Q931mes_SERVICE              0x0f /* 0000 1111                   */
#define Q931mes_SERVICE_ACKNOWLEDGE  0x07 /* 0000 0111                   */


/**
 * Generic Q.931 Timers
 */
enum {
	Q931_TIMER_T300	= 1,		/* */
	Q931_TIMER_T301,
	Q931_TIMER_T302,
	Q931_TIMER_T303,
	Q931_TIMER_T304,
	Q931_TIMER_T305,
	Q931_TIMER_T306,
	Q931_TIMER_T307,
	Q931_TIMER_T308,
	Q931_TIMER_T309,
	Q931_TIMER_T310,
	Q931_TIMER_T311,
	Q931_TIMER_T312,
	Q931_TIMER_T313,
	Q931_TIMER_T314,
	Q931_TIMER_T315,
	Q931_TIMER_T316,
	Q931_TIMER_T317,
	Q931_TIMER_T318,
	Q931_TIMER_T319,
	Q931_TIMER_T320,
	Q931_TIMER_T321,
	Q931_TIMER_T322,
};

/**
 * Q.931 ToN
 */
enum {
	Q931_TON_UNKNOWN		= 0x00,
	Q931_TON_INTERNATIONAL		= 0x01,
	Q931_TON_NATIONAL		= 0x02,
	Q931_TON_NETWORK_SPECIFIC	= 0x03,
	Q931_TON_SUBSCRIBER_NUMBER	= 0x04,
	Q931_TON_ABBREVIATED_NUMBER	= 0x06,
	Q931_TON_RESERVED		= 0x07
};

/**
 * Q.931 Numbering Plan
 */
enum {
	Q931_NUMPLAN_UNKNOWN		= 0x00,
	Q931_NUMPLAN_E164		= 0x01,
	Q931_NUMPLAN_X121		= 0x03,
	Q931_NUMPLAN_F69		= 0x04,
	Q931_NUMPLAN_NATIONAL		= 0x08,
	Q931_NUMPLAN_PRIVATE		= 0x09,
	Q931_NUMPLAN_RESERVED		= 0x0e
};

/**
 * Q.931 Presentation Indicator
 */
enum {
	Q931_PRES_ALLOWED		= 0x00,
	Q931_PRES_RESTRICTED		= 0x01,
	Q931_PRES_NOT_AVAILABLE		= 0x02,
	Q931_PRES_RESERVED		= 0x03
};

/**
 * Q.931 Screening Indicator
 */
enum {
	Q931_SCREEN_USER_NOT_SCREENED		= 0x00,
	Q931_SCREEN_USER_VERIFIED_PASSED	= 0x01,
	Q931_SCREEN_USER_VERIFIED_FAILED	= 0x02,
	Q931_SCREEN_NETWORK			= 0x03
};

/**
 * Q.931 Coding Standard
 */
enum {
	Q931_CODING_ITU		= 0x00,
	Q931_CODING_ISO		= 0x01,
	Q931_CODING_NATIONAL	= 0x02,
	Q931_CODING_NETWORK	= 0x03
};

/**
 * Q.931 High layer characteristik id
 */
enum {
	Q931_HLCHAR_TELEPHONY	= 0x01,
	Q931_HLCHAR_FAX_G23	= 0x04,
	Q931_HLCHAR_FAX_G4	= 0x21,
	Q931_HLCHAR_FAX_G4II	= 0x24,
	Q931_HLCHAR_T102	= 0x32,
	Q931_HLCHAR_T101	= 0x33,
	Q931_HLCHAR_F60		= 0x35,
	Q931_HLCHAR_X400	= 0x38,
	Q931_HLCHAR_X200	= 0x41
};

/**
 * Q.931 User information layer 1 protocol
 */
enum {
	Q931_UIL1P_V110		= 0x01,
	Q931_UIL1P_I460		= 0x01,
	Q931_UIL1P_X30		= 0x01,

	Q931_UIL1P_G711U	= 0x02,
	Q931_UIL1P_G711A	= 0x03,
	Q931_UIL1P_G721		= 0x04,

	Q931_UIL1P_H221		= 0x05,
	Q931_UIL1P_H242		= 0x05,

	Q931_UIL1P_H223		= 0x06,
	Q931_UIL1P_H245		= 0x06,

	Q931_UIL1P_RATE_ADAP	= 0x07,

	Q931_UIL1P_V120		= 0x08,
	Q931_UIL1P_X31		= 0x09
};

/**
 * Q.931 Information Transfer Capability
 */
enum {
	Q931_ITC_SPEECH			= 0x00,
	Q931_ITC_UNRESTRICTED		= 0x08,
	Q931_ITC_RESTRICTED		= 0x09,
	Q931_ITC_3K1_AUDIO		= 0x10,
	Q931_ITC_UNRESTRICTED_TONES	= 0x11,
	Q931_ITC_VIDEO			= 0x18
};

/**
 * Q.931 Information transfer rate
 */
enum {
	Q931_ITR_PACKET	= 0x00,
	Q931_ITR_64K	= 0x10,
	Q931_ITR_128K	= 0x11,
	Q931_ITR_384K	= 0x13,
	Q931_ITR_1536K	= 0x15,
	Q931_ITR_1920K	= 0x17,
	Q931_ITR_MULTI	= 0x18
};

/*****************************************************************************

  Struct:       Q931mes_Header

  Description:  Used to read the header & message code.

*****************************************************************************/
typedef struct {
	L3UINT	Size;           /* Size of message in bytes             */
	L3UCHAR	ProtDisc;       /* Protocol Discriminator               */
	L3UCHAR	MesType;        /* Message type                         */
	L3UCHAR	CRVFlag;        /* Call reference value flag            */
	L3INT	CRV;            /* Call reference value                 */

} Q931mes_Header;

/*****************************************************************************

  Struct:       Q931mes_Generic

  Description:  Generic header containing all IE's. This is not used, but is
				provided in case a proprietary variant needs it.

*****************************************************************************/
typedef struct {
	L3UINT		Size;           /* Size of message in bytes             */
	L3UCHAR		ProtDisc;       /* Protocol Discriminator               */
	L3UCHAR		MesType;        /* Message type                         */
	L3UCHAR		CRVFlag;        /* Call reference value flag            */
	L3INT		CRV;            /* Call reference value                 */

	/* WARNING: don't touch anything above this line (TODO: use Q931mes_Header directly to make sure it's the same) */

	L3UCHAR		Tei;            /* TEI                                  */

	ie		Shift;
	ie		MoreData;
	ie		SendComplete;
	ie		CongestionLevel;
	ie		RepeatInd;

	ie		Segment;        /* Segmented message                    */
	ie		BearerCap;      /* Bearer Capability                    */
	ie		Cause;          /* Cause                                */
	ie		CallState;      /* Call State                           */
	ie		CallID;			/* Call Identity                        */
	ie		ChanID;         /* Channel Identification               */
	ie		ChangeStatus;   /* Change Staus                         */
	ie		ProgInd;        /* Progress Indicator                   */
	ie		NetFac;         /* Network Spesific Facilities          */
	ie		NotifInd;       /* Notification Indicator               */
	ie		Display;        /* Display                              */
	ie		DateTime;       /* Date/Time                            */
	ie		KeypadFac;      /* Keypad Facility                      */
	ie		Signal;         /* Signal                               */
	ie		InfoRate;       /* Information rate                     */
	ie		EndEndTxDelay;  /* End to End Transmit Delay            */
	ie		TransDelSelInd; /* Transmit Delay Sel. and Ind.         */
	ie		PackParam;      /* Packed Layer Binary parameters       */
	ie		PackWinSize;    /* Packet Layer Window Size             */
	ie		PackSize;       /* Packed Size                          */
	ie		ClosedUserGrp;  /* Closed User Group                    */
	ie		RevChargeInd;   /* Reverse Charging Indicator           */
	ie		CalledNum;      /* Called Party Number                  */
	ie		CalledSub;      /* Called Party subaddress              */
	ie		CallingNum;     /* Calling Party Number                 */
	ie		CallingSub;     /* Calling Party Subaddress             */
	ie		RedirNum;       /* Redirection Number                   */
	ie		TransNetSel;    /* Transmit Network Selection           */
	ie		LLRepeatInd;    /* Repeat Indicator 2 LLComp            */
	ie		RestartWin;     /* Restart Window                       */
	ie		RestartInd;     /* Restart Indicator                    */
	ie		LLComp;         /* Low Layer Compatibility              */
	ie		HLComp;         /* High Layer Compatibility             */
	ie		UserUser;       /* User-user                            */
	ie		Escape;         /* Escape for extension                 */
	ie		Switchhook;
	ie		FeatAct;
	ie		FeatInd;
	ie		GenericDigits;

	L3UCHAR		buf[1];			/* Buffer for IE's						*/

} Q931mes_Generic;


/*****************************************************************************

  Struct:       Q931_TrunkInfo

  Description:  TrunkInfo is the struct entry used to store Q.931 related 
                information and state for E1/T1/J1 trunks and associated 
                channels in the system. 

				The user should store this information outside this stack
				and needs to feed the interface functions with a pointer to
				the TrunkInfo entry.

*****************************************************************************/
typedef struct Q931_TrunkInfo Q931_TrunkInfo_t;

typedef enum {
    Q931_LOG_NONE = -1,
    Q931_LOG_EMERG,
    Q931_LOG_ALERT,
    Q931_LOG_CRIT,
    Q931_LOG_ERROR,
    Q931_LOG_WARNING,
    Q931_LOG_NOTICE,
    Q931_LOG_INFO,
    Q931_LOG_DEBUG
} Q931LogLevel_t;

typedef L3INT (*Q931Tx34CB_t) (void *,L3UCHAR *, L3INT);
typedef L3INT (*Q931Tx32CB_t) (void *, L3INT, L3UCHAR, L3UCHAR *, L3INT);
typedef L3INT (*Q931ErrorCB_t) (void *,L3INT,L3INT,L3INT);
typedef L3INT (*Q931LogCB_t) (void *, Q931LogLevel_t, char *, L3INT);

typedef enum {					/* Network/User Mode.			*/
	Q931_TE=0,				/*  0 : User Mode			*/
	Q931_NT=1				/*  1 : Network Mode			*/
} Q931NetUser_t;

typedef enum {					/* Dialect enum                         */
	Q931_Dialect_Q931     = 0,
	Q931_Dialect_National = 2,
	Q931_Dialect_DMS      = 4,
	Q931_Dialect_5ESS     = 6,		/* Coming soon to a PRI stack near you! */

	Q931_Dialect_Count
} Q931Dialect_t;
#define DIALECT_STRINGS "q931", "", "national", "", "dms", "", "5ess", ""
Q931_STR2ENUM_P(q931_str2Q931Dialect_type, q931_Q931Dialect_type2str, Q931Dialect_t)

typedef enum {					/* Trunk Line Type.			*/
	Q931_TrType_E1 = 0,			/*  0 : E1 Trunk			*/
	Q931_TrType_T1 = 1,			/*  1 : T1 Trunk			*/
	Q931_TrType_J1 = 2,			/*  2 : J1 Trunk			*/
	Q931_TrType_BRI	= 3,			/*  3 : BRI Trunk			*/
	Q931_TrType_BRI_PTMP = 4		/*  4 : BRI PTMP Trunk			*/
} Q931_TrunkType_t;

typedef enum {					/* Trunk State			*/
	Q931_TrState_NoAlignment=0,		/* Trunk not aligned		*/
	Q931_TrState_Aligning=1,		/* Aligning in progress		*/
	Q931_TrState_Aligned=2			/* Trunk Aligned		*/
} Q931_TrunkState_t;

typedef enum {
	Q931_ChType_NotUsed=0,			/* Unused Channel						*/
	Q931_ChType_B=1,			/* B Channel (Voice)					*/
	Q931_ChType_D=2,			/* D Channel (Signalling)				*/
	Q931_ChType_Sync=3			/* Sync Channel							*/
} Q931_ChanType_t;

struct Q931_Call
{
	L3UCHAR InUse;			/* Indicate if entry is in use.         */
					/*  0 = Not in Use                      */
					/*  1 = Active Call.                    */

	L3UCHAR Tei;			/* Associated TEI 			*/

	L3UCHAR BChan;			/* Associated B Channel.                */
					/* 0 - 31 valid B chan			*/
					/* 255 = Not allocated			*/

	L3INT   CRV;			/* Associated CRV.                      */

	L3UINT  State;			/* Call State.                          */
					/*  0 is Idle, but other values are	*/
					/*  defined per dialect.		*/
					/*  Default usage is 1-99 for TE and    */
					/*  101 - 199 for NT.			*/
        
	L3ULONG Timer;			/* Timer in ms. The TimeTick will check		*/
					/* if this has exceeded the timeout, and	*/
					/* if so call the timers timeout proc.		*/

	L3USHORT TimerID;		/* Timer Identification/State           */
					/* actual values defined by dialect	*/
					/*  0 : No timer running                */
					/*  ITU-T Q.931:301 - 322 Timer running */
};

struct Q931_TrunkInfo
{
	Q931NetUser_t    NetUser;		/* Network/User Mode.                   */
	Q931_TrunkType_t TrunkType;		/* Trunk Line Type.                     */
	Q931Dialect_t    Dialect;		/* Q.931 Based dialect index.           */

	Q931Tx34CB_t     Q931Tx34CBProc;
	Q931Tx32CB_t     Q931Tx32CBProc;
	Q931ErrorCB_t    Q931ErrorCBProc;
	Q931LogCB_t      Q931LogCBProc;
	void *PrivateData32;
	void *PrivateData34;
	void *PrivateDataLog;

	Q931LogLevel_t   loglevel;

	L3UCHAR     Enabled;            /* Enabled/Disabled                     */
                                    /*  0 = Disabled                        */
                                    /*  1 = Enabled                         */

	Q931_TrunkState_t TrunkState;

    L3INT       LastCRV;            /* Last used crv for the trunk.         */

    L3UCHAR L3Buf[Q931L4BUF];		/* message buffer for messages to be    */
                                    /* send from Q.931 L4.                  */

    L3UCHAR L2Buf[Q931L2BUF];		/* buffer for messages send to L2.      */

	/* The auto flags below switch on/off automatic Ack messages. SETUP ACK */
	/* as an example can be sent by the stack in response to SETUP to buy   */
	/* time in processing on L4. Setting this to true will cause the stack  */
	/* to automatically send this.											*/

	L3BOOL	autoSetupAck;			/* Indicate if the stack should send    */
									/* SETUP ACK or not. 0=No, 1 = Yes.		*/

	L3BOOL  autoConnectAck;			/* Indicate if the stack should send    */
									/* CONNECT ACT or not. 0=No, 1=Yes.		*/

	L3BOOL  autoRestartAck;			/* Indicate if the stack should send    */
									/* RESTART ACK or not. 0=No, 1=Yes.		*/

	L3BOOL  autoServiceAck;			/* Indicate if the stack should send    */
									/* SERVICE ACK or not. 0=No, 1=Yes.		*/

	/* channel array holding info per channel. Usually defined to 32		*/
	/* channels to fit an E1 since T1/J1 and BRI will fit inside a E1.		*/
    struct _charray
    {
		Q931_ChanType_t ChanType;	/* Unused, B, D, Sync */

        L3UCHAR Available;          /* Channel Available Flag               */
                                    /*  0 : Avaiabled                       */
                                    /*  1 : Used                            */

        L3INT   CRV;                /* Associated CRV                       */

    } ch[Q931MAXCHPERTRUNK];

	/* Active Call information indentified by CRV. See Q931AllocateCRV for  */
	/* initialization of call table.					*/
	struct Q931_Call	call[Q931MAXCALLPERTRUNK];
};

/*****************************************************************************
  
  Struct:		Q931State

  Description:	Define a Q931 State, legal events and next state for each
				event. Used to simplify the state engine logic. Each state
				engine defines its own state table and the logic need only
				call a helper function to check if the message is legal
				at this stage.

*****************************************************************************/
typedef struct
{
	L3INT		State;
	L3INT		Message;
	L3UCHAR		Direction;
} Q931State;

/*****************************************************************************

  Proc table external references. 

  The proc tables are defined in Q931.c and initialized in Q931Initialize.

*****************************************************************************/
typedef L3INT (q931proc_func_t) (Q931_TrunkInfo_t *pTrunk, L3UCHAR *, L3INT);

typedef L3INT (q931umes_func_t) (Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT IOff, L3INT Size);
typedef L3INT (q931pmes_func_t) (Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);

typedef L3INT (q931uie_func_t) (Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff);
typedef L3INT (q931pie_func_t) (Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);

typedef L3INT (q931timeout_func_t) (Q931_TrunkInfo_t *pTrunk, L3INT callIndex);
typedef L3ULONG q931timer_t;

extern q931proc_func_t *Q931Proc[Q931MAXDLCT][Q931MAXMES];

extern q931umes_func_t *Q931Umes[Q931MAXDLCT][Q931MAXMES];
extern q931pmes_func_t *Q931Pmes[Q931MAXDLCT][Q931MAXMES];

extern q931uie_func_t *Q931Uie[Q931MAXDLCT][Q931MAXIE];
extern q931pie_func_t *Q931Pie[Q931MAXDLCT][Q931MAXIE];

extern q931timeout_func_t *Q931Timeout[Q931MAXDLCT][Q931MAXTIMER];
extern q931timer_t         Q931Timer[Q931MAXDLCT][Q931MAXTIMER];


/*****************************************************************************
    
  Macro:        GetIETotoSize

  Syntax:       L3INT GetIETotSize(InfoElem ie);

  Description:  Compute the total size in bytes of an info element including 
                size of 'header'.    

*****************************************************************************/
#define Q931GetIETotSize(ie) (((ie.InfoID & 0x80) != 0) ? 1 : ie.LenIE) + 2)

/*****************************************************************************

  Macro:        IsIEPresent

  Syntax:       BOOL IsIEPresent(ie InfoElement);

  Description:  Return TRUE if the Information Element is included.

*****************************************************************************/
#define Q931IsIEPresent(x) ((x & 0x8000) != 0)

/*****************************************************************************

  Macro:        GetIEOffset and GetIEValue
    
  Syntax:       L3INT GetIEOffset(ie InfoElement)
                L3INT GetIEValue(ie InfoElement)

  Description:  Returns the offset (or the value )to the Information Element.

  Note:         GetIEValue assumes that the 15 lsb bit is the value of a 
                single octet information element. This macro can not be used
                on a variable information element.

*****************************************************************************/
#define Q931GetIEOffset(x) (x & 0x7fff)
#define Q931GetIEValue(x)  (x & 0x7fff)

/*****************************************************************************

  Macro:        Q931GetIEPtr

  Syntax:       void * Q931GetIEPtr(ie InfoElement, L3UCHAR * Buf);

  Description:  Compute a Ptr to the information element.

*****************************************************************************/
#define Q931GetIEPtr(ie,buf) ((void *)&buf[Q931GetIEOffset(ie)])

/*****************************************************************************

  Macro:        SetIE

  Syntax:       void SetIE(ie InfoElement, L3INT Offset);

  Description:  Set an information element.

*****************************************************************************/
#define Q931SetIE(x,o) { x = (ie)(o) | 0x8000; }

/*****************************************************************************

  Macro:        IsQ931Ext

  Syntax        BOOL IsQ931Ext(L3UCHAR c)

  Description:  Return true Check if the msb (bit 8) is 0. This indicate
                that the octet is extended.

*****************************************************************************/
#define IsQ931Ext(x) ((x & 0x80) == 0)

/*****************************************************************************

  Macro:        ieGetOctet

  Syntax:       unsigned L3UCHAR ieGetOctet(L3INT e)

  Description:  Macro to fetch one byte from an integer. Mostly used to 
                avoid warnings.

*****************************************************************************/
#define ieGetOctet(x) ((L3UCHAR)(x))

/*****************************************************************************

  Macro:        NoWarning

  Syntax:       void NoWarning(x)

  Description:  Macro to suppress unreferenced formal parameter warnings

                Used during creation of the stack since the stack is 
                developed for Warning Level 4 and this creates a lot of 
                warning for the initial empty functions.

*****************************************************************************/
#define NoWarning(x) (x = x)

/*****************************************************************************

  External references. See Q931.c for details.

*****************************************************************************/

#include "Q931ie.h"

#include "Q932.h"

/*****************************************************************************

  Q.931 Message Pack/Unpack functions. Implemented in Q931mes.c

*****************************************************************************/
L3INT Q931Pmes_Alerting(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_CallProceeding(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_Connect(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_ConnectAck(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_Progress(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_Setup(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_SetupAck(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_Resume(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_ResumeAck(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_ResumeReject(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_Suspend(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_SuspendAck(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_SuspendReject(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_UserInformation(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_Disconnect(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_Release(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_ReleaseComplete(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_Restart(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_RestartAck(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_CongestionControl(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_Information(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_Notify(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_Segment(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_Status(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_StatusEnquiry(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_Service(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_ServiceAck(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);


L3INT Q931Umes_Alerting(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT I, L3INT O);
L3INT Q931Umes_CallProceeding(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT I, L3INT O);
L3INT Q931Umes_Connect(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT I, L3INT O);
L3INT Q931Umes_ConnectAck(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT I, L3INT O);
L3INT Q931Umes_Progress(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT I, L3INT O);
L3INT Q931Umes_Setup(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT I, L3INT O);
L3INT Q931Umes_SetupAck(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT I, L3INT O);
L3INT Q931Umes_Resume(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT I, L3INT O);
L3INT Q931Umes_ResumeAck(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT I, L3INT O);
L3INT Q931Umes_ResumeReject(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT I, L3INT O);
L3INT Q931Umes_Suspend(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT I, L3INT O);
L3INT Q931Umes_SuspendAck(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT I, L3INT O);
L3INT Q931Umes_SuspendReject(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT I, L3INT O);
L3INT Q931Umes_UserInformation(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT I, L3INT O);
L3INT Q931Umes_Disconnect(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT I, L3INT O);
L3INT Q931Umes_Release(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT I, L3INT O);
L3INT Q931Umes_ReleaseComplete(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT I, L3INT O);
L3INT Q931Umes_Restart(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT I, L3INT O);
L3INT Q931Umes_RestartAck(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT I, L3INT O);
L3INT Q931Umes_CongestionControl(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT I, L3INT O);
L3INT Q931Umes_Information(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT I, L3INT O);
L3INT Q931Umes_Notify(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT I, L3INT O);
L3INT Q931Umes_Segment(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT I, L3INT O);
L3INT Q931Umes_Status(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT I, L3INT O);
L3INT Q931Umes_StatusEnquiry(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT I, L3INT O);
L3INT Q931Umes_Service(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size);
L3INT Q931Umes_ServiceAck(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, Q931mes_Generic *mes, L3INT IOff, L3INT Size);


/*****************************************************************************

  Q.931 Process Function Prototyping. Implemented in Q931StateTE.c

*****************************************************************************/
L3INT Q931ProcAlertingTE(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcCallProceedingTE(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcConnectTE(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcConnectAckTE(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcProgressTE(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcSetupTE(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcSetupAckTE(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcResumeTE(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcResumeAckTE(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcResumeRejectTE(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcSuspendTE(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcSuspendAckTE(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcSuspendRejectTE(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcUserInformationTE(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcDisconnectTE(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcReleaseTE(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcReleaseCompleteTE(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcRestartTE(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcRestartAckTE(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcCongestionControlTE(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcInformationTE(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcNotifyTE(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcStatusTE(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcStatusEnquiryTE(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcSegmentTE(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);

L3INT Q931ProcAlertingNT(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcCallProceedingNT(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcConnectNT(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcConnectAckNT(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcProgressNT(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcSetupNT(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcSetupAckNT(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcResumeNT(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcResumeAckNT(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcResumeRejectNT(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcSuspendNT(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcSuspendAckNT(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcSuspendRejectNT(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcUserInformationNT(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcDisconnectNT(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcReleaseNT(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcReleaseCompleteNT(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcRestartNT(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcRestartAckNT(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcCongestionControlNT(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcInformationNT(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcNotifyNT(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcStatusNT(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcStatusEnquiryNT(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcSegmentNT(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);

L3INT Q931ProcUnknownMessage(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcUnexpectedMessage(Q931_TrunkInfo_t *pTrunk,L3UCHAR * b, L3INT iFrom);

/*****************************************************************************

  Interface Function Prototypes. Implemented in Q931.c

*****************************************************************************/
void    Q931TimerTick(Q931_TrunkInfo_t *pTrunk);
L3INT   Q931Rx23(Q931_TrunkInfo_t *pTrunk, L3INT ind, L3UCHAR tei, L3UCHAR * Mes, L3INT Size);
L3INT   Q931Tx32Data(Q931_TrunkInfo_t *pTrunk, L3UCHAR bcast, L3UCHAR * Mes, L3INT Size);
L3INT   Q931Rx43(Q931_TrunkInfo_t *pTrunk, L3UCHAR * Mes, L3INT Size);
L3INT   Q931Tx34(Q931_TrunkInfo_t *pTrunk, L3UCHAR * Mes, L3INT Size);
void    Q931SetError(Q931_TrunkInfo_t *pTrunk,L3INT ErrID, L3INT ErrPar1, L3INT ErrPar2);

void	Q931SetDefaultErrorCB(Q931ErrorCB_t Q931ErrorPar);

void    Q931CreateTE(L3UCHAR i);
void    Q931CreateNT(L3UCHAR i);
void    Q931SetMesCreateCB(L3INT (*callback)(void));
void    Q931SetDialectCreateCB(L3INT (*callback)(L3INT));
void    Q931SetHeaderSpace(L3INT space);

void Q931SetMesProc(L3UCHAR mes, L3UCHAR dialect, q931proc_func_t *Q931ProcFunc, q931umes_func_t *Q931UmesFunc, q931pmes_func_t *Q931PmesFunc);
void Q931SetIEProc(L3UCHAR iec, L3UCHAR dialect, q931pie_func_t *Q931PieProc, q931uie_func_t *Q931UieProc);
void Q931SetTimeoutProc(L3UCHAR dialect, L3UCHAR timer, q931timeout_func_t *Q931TimeoutProc);
void Q931SetTimerDefault(L3UCHAR dialect, L3UCHAR timer, q931timer_t timeout);

void Q931Initialize(void);
void Q931AddDialect(L3UCHAR iDialect, void (*Q931CreateDialectCB)(L3UCHAR iDialect));
L3INT Q931InitMesSetup(Q931mes_Generic *p);
L3INT Q931InitMesRestartAck(Q931mes_Generic * pMes);
L3INT Q931InitMesGeneric(Q931mes_Generic *pMes);

L3INT	Q931CreateCRV(Q931_TrunkInfo_t *pTrunk, L3INT * callIndex);
L3INT	Q931ReleaseCRV(Q931_TrunkInfo_t *pTrunk, L3INT CRV);
L3INT	Q931AllocateCRV(Q931_TrunkInfo_t *pTrunk, L3INT iCRV, L3INT * callIndex);
L3INT   Q931FindCRV(Q931_TrunkInfo_t *pTrunk, L3INT crv, L3INT *callindex);
L3INT	Q931GetCallState(Q931_TrunkInfo_t *pTrunk, L3INT iCRV);
L3INT	Q931StartTimer(Q931_TrunkInfo_t *pTrunk, L3INT callIndex, L3USHORT iTimer);
L3INT	Q931StopTimer(Q931_TrunkInfo_t *pTrunk, L3INT callindex, L3USHORT iTimer);
L3INT	Q931SetState(Q931_TrunkInfo_t *pTrunk, L3INT callIndex, L3INT iState);
L3ULONG Q931GetTime(void);
void    Q931SetGetTimeCB(L3ULONG (*callback)(void));
void	Q931AddStateEntry(L3UCHAR iD, L3INT iState, L3INT iMes, L3UCHAR cDir);
L3BOOL	Q931IsEventLegal(L3UCHAR iD, L3INT iState, L3INT iMes, L3UCHAR cDir);

/*****************************************************************************

  Q.931 Low Level API Function Prototyping. Implemented in Q931API.c

*****************************************************************************/
ie Q931AppendIE(L3UCHAR *pm, L3UCHAR *pi);
L3INT Q931GetUniqueCRV(Q931_TrunkInfo_t *pTrunk);

L3INT Q931InitIEBearerCap(Q931ie_BearerCap *p);
L3INT Q931InitIEChanID(Q931ie_ChanID *p);
L3INT Q931InitIEProgInd(Q931ie_ProgInd *p);
L3INT Q931InitIENetFac(Q931ie_NetFac * pIE);
L3INT Q931InitIEDisplay(Q931ie_Display * pIE);
L3INT Q931InitIEDateTime(Q931ie_DateTime * pIE);
L3INT Q931InitIEKeypadFac(Q931ie_KeypadFac * pIE);
L3INT Q931InitIESignal(Q931ie_Signal * pIE);
L3INT Q931InitIECallingNum(Q931ie_CallingNum * pIE);
L3INT Q931InitIECallingSub(Q931ie_CallingSub * pIE);
L3INT Q931InitIECalledNum(Q931ie_CalledNum * pIE);
L3INT Q931InitIECalledSub(Q931ie_CalledSub * pIE);
L3INT Q931InitIETransNetSel(Q931ie_TransNetSel * pIE);
L3INT Q931InitIELLComp(Q931ie_LLComp * pIE);
L3INT Q931InitIEHLComp(Q931ie_HLComp * pIE);

L3INT Q931Disconnect(Q931_TrunkInfo_t *pTrunk, L3INT iTo, L3INT iCRV, L3INT iCause);
L3INT Q931ReleaseComplete(Q931_TrunkInfo_t *pTrunk, L3UCHAR *buf);
L3INT Q931AckRestart(Q931_TrunkInfo_t *pTrunk, L3UCHAR *buf);
L3INT Q931AckConnect(Q931_TrunkInfo_t *pTrunk, L3UCHAR *buf);
L3INT Q931AckSetup(Q931_TrunkInfo_t *pTrunk, L3UCHAR *buf);
L3INT Q931AckService(Q931_TrunkInfo_t *pTrunk, L3UCHAR *buf);

L3INT Q931Api_InitTrunk(Q931_TrunkInfo_t *pTrunk,
						Q931Dialect_t Dialect,
						Q931NetUser_t NetUser,
						Q931_TrunkType_t TrunkType,
						Q931Tx34CB_t Q931Tx34CBProc,
						Q931Tx32CB_t Q931Tx32CBProc,
						Q931ErrorCB_t Q931ErrorCBProc,
						void *PrivateData32,
						void *PrivateData34);

L3INT Q931GetMesSize(Q931mes_Generic *pMes);
L3INT Q931InitMesResume(Q931mes_Generic * pMes);

L3INT Q931Log(Q931_TrunkInfo_t *trunk, Q931LogLevel_t level, const char *fmt, ...);
void Q931SetLogCB(Q931_TrunkInfo_t *trunk, Q931LogCB_t func, void *priv);
void Q931SetLogLevel(Q931_TrunkInfo_t *trunk, Q931LogLevel_t level);

void Q931SetL4HeaderSpace(L3INT space);
void Q931SetL2HeaderSpace(L3INT space);
L3INT Q931ProcDummy(Q931_TrunkInfo_t *pTrunk, L3UCHAR * b,L3INT c);
L3INT Q931UmesDummy(Q931_TrunkInfo_t *pTrunk,L3UCHAR *IBuf, Q931mes_Generic *OBuf, L3INT IOff, L3INT Size);
L3INT Q931UieDummy(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff);
L3INT Q931PmesDummy(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931PieDummy(Q931_TrunkInfo_t *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);
L3INT Q931TxDummy(Q931_TrunkInfo_t *pTrunk, L3UCHAR * b, L3INT n);
L3INT Q931ErrorDummy(void *priv, L3INT a, L3INT b, L3INT c);
L3INT Q931TimeoutDummy(Q931_TrunkInfo_t *pTrunk, L3INT callIndex);

L3INT Q931MesgHeader(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *mes, L3UCHAR *OBuf, L3INT Size, L3INT *IOff);

#endif /* _Q931_NL */
