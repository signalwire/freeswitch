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
					from a hdlc receiver without usage of queues for optimized 
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
#endif

/*****************************************************************************

  Error Codes

*****************************************************************************/
#define Q931E_NO_ERROR					0
#define Q931E_UNKNOWN_MESSAGE			-3001
#define Q931E_ILLEGAL_IE				-3002
#define Q931E_UNKNOWN_IE				-3003
#define Q931E_BEARERCAP					-3004
#define Q931E_HLCOMP					-3005
#define Q931E_LLCOMP					-3006
#define Q931E_INTERNAL                  -3007
#define Q931E_MISSING_CB                -3008
#define Q931E_UNEXPECTED_MESSAGE        -3009
#define Q931E_ILLEGAL_MESSAGE			-3010
#define Q931E_TOMANYCALLS               -3011
#define Q931E_INVALID_CRV               -3012
#define Q931E_CALLID                    -3013
#define Q931E_CALLSTATE                 -3014
#define Q931E_CALLEDSUB                 -3015
#define Q931E_CALLEDNUM                 -3016
#define Q931E_CALLINGNUM                -3017
#define Q931E_CALLINGSUB                -3018
#define Q931E_CAUSE                     -3019
#define Q931E_CHANID                    -3020
#define Q931E_DATETIME                  -3021
#define Q931E_DISPLAY                   -3022
#define Q931E_KEYPADFAC                 -3023
#define Q931E_NETFAC                    -3024
#define Q931E_NOTIFIND                  -3025
#define Q931E_PROGIND                   -3026
#define Q931E_RESTARTIND                -3027
#define Q931E_SEGMENT                   -3028
#define Q931E_SIGNAL                    -3029

/*****************************************************************************

	Some speed optimization can be achieved by changing all variables to the 
	word size of your processor. A 32 bit processor have to do a lot of extra 
	work to read a packed 8 bit integer. Changing all fields to 32 bit integer 
	will ressult in usage of some extra space, but speed up the stack.

	The stack have been designed to allow L3UCHAR etc. to be any size of 8 bit
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
	
	MAXTRUNKS sets how many physical trunks this system might have. This 
	number should be keept at a minimum since it will use global space.

	It is recommended that you leave MAXCHPERTRUNK as is

*****************************************************************************/

#define Q931L4BUF  1000             /* size of message buffer               */

#define Q931L2BUF	300				/* size of message buffer				*/

#define Q931MAXTRUNKS	4			/* Total number of trunks that will be	*/
									/* processed by this instance of the	*/
									/* stack								*/

#define Q931MAXCHPERTRUNK	32		/* Number of channels per trunk. The	*/
									/* stack uses a static set of 32		*/
									/* channels regardless if it is E1, T1	*/
									/* or BRI that actually is used.		*/

#define Q931MAXCALLPERTRUNK (Q931MAXCHPERTRUNK * 2)
                                    /* Number of max active CRV per trunk.  */
									/* Q.931 can have more calls than there */
									/* are channels.						*/

/*****************************************************************************

  The following defines control the dialect switch tables and should only be
  changed when a new dialect needs to be inserted into the stack.   

  This stack uses an array of functions to know which function to call as   
  it receives a SETUP message etc. A new dialect can when schoose to use
  the proc etc for standard Q.931 or insert a modified proc.

  This technique has also been used to distinguish between user and network
  mode to make the code as easy to read and maintainable as possible.

  A message and IE index have been used to save space. These indexes allowes
  the message or IE code to be used directly and will give back a new index
  into the table.

*****************************************************************************/

/* WARNING! Initialize Q931CreateDialectCB[] will NULL when increasing the  */
/* Q931MAXDLCT value to avoid Q931Initialize from crashing if one entry is  */
/* not used.																*/
#define Q931MAXDLCT 2               /* Max dialects included in this        */
                                    /* compile. User and Network count as   */
                                    /* one dialect each.                    */


#define Q931MAXMES  255             /* Number of messages                   */

#define Q931MAXIE  255              /* Number of IE                         */

#define Q931MAXSTATE 100			/* Size of state tables					*/

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
#define Q931_N10     (0x0100 | 11)
#define Q931_N11     (0x0100 | 11)
#define Q931_N12     (0x0100 | 12)
#define Q931_N15     (0x0100 | 15)
#define Q931_N17     (0x0100 | 17)
#define Q931_N19     (0x0100 | 19)
#define Q931_N22     (0x0100 | 22)
#define Q931_N25     (0x0100 | 25)

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
#define Q931mes_INFORMATION          0x7a /* 0111 1011                   */
#define Q931mes_NOTIFY               0x6e /* 0110 1110                   */
#define Q931mes_STATUS               0x7d /* 0111 1101                   */
#define Q931mes_STATUS_ENQUIRY       0x75 /* 0111 0101                   */
#define Q931mes_SEGMENT              0x60 /* 0110 0000                   */


/* Single octet information elements                                */
#define Q931ie_SHIFT                            0x90 /* 1001 ----       */
#define Q931ie_MORE_DATA                        0xa0 /* 1010 ----       */
#define Q931ie_SENDING_COMPLETE                 0xa1 /* 1010 0000       */
#define Q931ie_CONGESTION_LEVEL                 0xb0 /* 1011 ----       */
#define Q931ie_REPEAT_INDICATOR                 0xd0 /* 1101 ----       */

/* Variable Length Information Elements */
#define Q931ie_SEGMENTED_MESSAGE                0x00 /* 0000 0000       */
#define Q931ie_BEARER_CAPABILITY                0x04 /* 0000 0100       */
#define Q931ie_CAUSE                            0x08 /* 0000 1000       */
#define Q931ie_CALL_IDENTITY                    0x10 /* 0001 0000       */
#define Q931ie_CALL_STATE                       0x14 /* 0001 0100       */
#define Q931ie_CHANNEL_IDENTIFICATION           0x18 /* 0001 1000       */
#define Q931ie_PROGRESS_INDICATOR               0x1e /* 0001 1110       */
#define Q931ie_NETWORK_SPECIFIC_FACILITIES      0x20 /* 0010 0000       */
#define Q931ie_NOTIFICATION_INDICATOR           0x27 /* 0010 0111       */
#define Q931ie_DISPLAY                          0x28 /* 0010 1000       */
#define Q931ie_DATETIME                         0x29 /* 0010 1001       */
#define Q931ie_KEYPAD_FACILITY                  0x2c /* 0010 1100       */
#define Q931ie_SIGNAL                           0x34 /* 0011 0100       */
#define Q931ie_SWITCHOOK                        0x36 /* 0011 0110       */
#define Q931ie_FEATURE_ACTIVATION               0x38 /* 0011 1000       */
#define Q931ie_FEATURE_INDICATION               0x39 /* 0011 1001       */
#define Q931ie_INFORMATION_RATE                 0x40 /* 0100 0000       */
#define Q931ie_END_TO_END_TRANSIT_DELAY         0x42 /* 0100 0010       */
#define Q931ie_TRANSIT_DELAY_SELECTION_AND_IND  0x43 /* 0100 0011       */
#define Q931ie_PACKED_LAYER_BIMARY_PARAMETERS   0x44 /* 0100 0100       */
#define Q931ie_PACKED_LAYER_WINDOW_SIZE         0x45 /* 0100 0101       */
#define Q931ie_PACKED_SIZE                      0x46 /* 0100 0110       */
#define Q931ie_CALLING_PARTY_NUMBER             0x6c /* 0110 1100       */
#define Q931ie_CALLING_PARTY_SUBADDRESS         0x6d /* 0110 1101       */
#define Q931ie_CALLED_PARTY_NUMBER              0x70 /* 0111 0000       */
#define Q931ie_CALLED_PARTY_SUBADDRESS          0x71 /* 0111 0001       */
#define Q931ie_REDIRECTING_NUMBER               0x74 /* 0111 0100       */
#define Q931ie_TRANSIT_NETWORK_SELECTION        0x78 /* 0111 1000       */
#define Q931ie_RESTART_INDICATOR                0x79 /* 0111 1001       */
#define Q931ie_LOW_LAYER_COMPATIBILITY          0x7c /* 0111 1100       */
#define Q931ie_HIGH_LAYER_COMPATIBILITY         0x7d /* 0111 1101       */
#define Q931ie_USER_USER                        0x7e /* 0111 1110       */
#define Q931ie_ESCAPE_FOR_EX                    0x7f /* 0111 1111       */

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
                                    /* ie == 0xffff indicate error          */

/*****************************************************************************

  Struct:        Q931ie_BearerCap

  Description:   Bearer Capability Information Element.

*****************************************************************************/

typedef struct
{
    L3UCHAR IEId;                   /* 00000100 for Bearer Capability       */
 
    L3UCHAR Size;                   /* Length of Information Element        */

    L3UCHAR CodStand;               /* Coding Standard.                     */
                                    /*  00 - ITU-T                          */
                                    /*  01 - ISO/IEC                        */
                                    /*  10 - National standard              */
                                    /*  11 - Network side spesific          */

    L3UCHAR ITC;                    /* Information Transfer Capability      */
                                    /*  00000 - Speech                      */
                                    /*  01000 - Unrestricted digital info   */
                                    /*  01001 - Restricted digital info     */
                                    /*  10000 - 3.1 kHz audio               */
                                    /*  10001 - Unrestricted with tones     */
                                    /*  11000 - Video                       */

    L3UCHAR TransMode;              /* Transfer Mode.                       */
                                    /*  00 - Circuit mode                   */
                                    /*  10 - Packet mode                    */

    L3UCHAR ITR;                    /* Information Transfer Rate.           */
                                    /*  00000 - Packed mode                 */
                                    /*  10000 - 64 kbit/s                   */
                                    /*  10001 - 2 x 64 kbit/s               */
                                    /*  10011 - 384 kbit/s                  */
                                    /*  10101 - 1536 kbit/s                 */
                                    /*  10111 - 1920 kbit/s                 */
                                    /*  11000 - Multirat (64 kbit/s base)   */

    L3UCHAR RateMul;                /* Rate Multiplier                      */

    L3UCHAR Layer1Ident;			/* Layer 1 Ident.						*/

    L3UCHAR UIL1Prot;               /* User Information Layer 1 Protocol    */
									/*	00001 : ITU-T V.110, I.460 and X.30 */
									/*  00010 : G.711 my-law				*/
									/*  00011 : G.711 A-law					*/
									/*  00100 : G.721						*/
                                    /*  00101 : H.221 and H.242				*/
									/*  00110 : H.223 and H.245				*/
									/*  00111 : Non ITU-T Standard			*/
									/*  01000 : ITU-T V.120					*/
									/*  01001 : ITU-T X.31 HDLC flag stuff.	*/

    L3UCHAR SyncAsync;              /* Sync/Async                           */
									/*	0 : Syncronous data					*/
									/*	1 : Asyncronous data				*/

    L3UCHAR Negot;					/* Negotiation							*/
									/*	0 : In-band negotiation not possib.	*/
									/*  1 : In-band negotiation possible	*/

    L3UCHAR UserRate;				/* User rate							*/
									/*	00000 : I.460, V.110, X,30			*/
									/*  00001 : 0.6 kbit/s x.1				*/
									/*  00010 : 1.2 kbit/s					*/
									/*  00011 : 2.4 kbit/s					*/
									/*  00100 : 3.6 kbit/s					*/
									/*  00101 : 4.8 kbit/s					*/
									/*  00110 : 7.2 kbit/s					*/
									/*  00111 : 8 kbit/s I.460				*/
									/*  01000 : 9.6 kbit/s					*/
									/*  01001 : 14.4 kbit/s					*/
									/*  01010 : 16 kbit/s					*/
									/*  01011 :	19.2 kbit/s					*/
									/*  01100 : 32 kbit/s					*/
									/*  01101 : 38.4 kbit/s					*/
									/*  01110 : 48 kbit/s					*/
									/*  01111 : 56 kbit/s					*/
									/*  10000 : 57.6 kbit/s					*/
									/*  10010 : 28.8 kbit/s					*/
									/*  10100 : 24 kbit/s					*/
									/*  10101 : 0.1345 kbit/s				*/
									/*  10110 : 0.100 kbit/s				*/
									/*  10111 : 0.075/1.2 kbit/s			*/
									/*  11000 : 1.2/0.075/kbit/s			*/
									/*  11001 : 0.050 kbit/s				*/
									/*  11010 : 0.075 kbit/s				*/
									/*  11011 : 0.110 kbit/s				*/
									/*  11100 : 0.150 kbit/s				*/
									/*  11101 : 0.200 kbit/s				*/
									/*  11110 : 0.300 kbit/s				*/
									/*  11111 : 12 kbit/s					*/

    L3UCHAR InterRate;              /* Intermediate Rate                    */
									/*	00 : Not used						*/
									/*  01 : 8 kbit/s						*/
									/*  10 : 16 kbit/s						*/
									/*  11 : 32 kbit/s						*/

    L3UCHAR NIConTx;				/* Network Indepentend Clock on transmit*/
									/*	0 : Not required to send data  clc  */
									/*  1 : Send data w/NIC clc				*/

    L3UCHAR NIConRx;				/* NIC on Rx							*/
									/*	0 : Cannot accept indep. clc		*/
									/*  1 : data with indep. clc accepted	*/

    L3UCHAR FlowCtlTx;              /* Flow control on Tx                   */
									/*  0 : Send Flow ctrl not required		*/
									/*  1 : Send flow ctrl required			*/

    L3UCHAR FlowCtlRx;              /* Flow control on Rx                   */
									/*  0 : cannot use receive flow ctrl	*/
									/*  1 : Receive flow ctrl accepted		*/
    L3UCHAR HDR;					/* HDR/No HDR							*/
    L3UCHAR MultiFrame;             /* Multi frame support                  */
									/*  0 : multiframe not supported		*/
									/*  1 : multiframe supported			*/

    L3UCHAR Mode;					/* Mode of operation					*/
									/*	0 : bit transparent mode of operat.	*/
									/*	1 : protocol sesitive mode of op.	*/

    L3UCHAR LLInegot;				/* Logical link id negotiation (oct. 5b)*/
									/*  0 : default LLI=256 only			*/
									/*  1 : Full protocol negotiation		*/

    L3UCHAR Assignor;               /* Assignor/assignee                    */
									/*  0 : Default Asignee					*/
									/*  1 : Assignor only					*/

    L3UCHAR InBandNeg;              /* In-band/out-band negot.              */
									/*  0 : negot done w/ USER INFO mes		*/
									/*  1 : negot done in-band w/link zero	*/

    L3UCHAR NumStopBits;            /* Number of stop bits					*/
									/*  00 : Not used						*/
									/*  01 : 1 bit							*/
									/*  10 : 1.5 bits						*/
									/*  11 : 2 bits							*/

    L3UCHAR NumDataBits;            /* Number of data bits.                 */
									/*  00 : not used						*/
									/*  01 : 5 bits							*/
									/*  10 : 7 bits							*/
									/*  11 : 8 bits							*/

    L3UCHAR Parity;					/* Parity Information					*/
									/*	000 : Odd							*/
									/*  010 : Even							*/
									/*  011 : None							*/
									/*  100 : Forced to 0					*/
									/*  101 : Forced to 1					*/

    L3UCHAR DuplexMode;				/* Mode duplex							*/
									/*  0 : Half duplex						*/
									/*  1 : Full duplex						*/

    L3UCHAR ModemType;				/* Modem type, see Q.931 p 64			*/

    L3UCHAR Layer2Ident;			/* Layer 2 Ident						*/

    L3UCHAR UIL2Prot;               /* User Information Layer 2 Protocol    */
									/*	00010 : Q.921/I.441					*/
									/*  00110 : X.25						*/
									/*  01100 : LAN logical link			*/

    L3UCHAR Layer3Ident;            /* Layer 3 ident.						*/

    L3UCHAR UIL3Prot;               /* User Information Layer 3 Protocol    */
									/*	00010 : Q.931						*/
									/*  00110 : X.25						*/
									/*  01011 : ISO/IEC TR 9577				*/

    L3UCHAR AL3Info1;				/* additional layer 3 info 1			*/

    L3UCHAR AL3Info2;				/* additional layer 3 info 2			*/
}Q931ie_BearerCap;

/*****************************************************************************

  Struct:       Q931ie_CallID


*****************************************************************************/

typedef struct
{
    L3UCHAR IEId;                   /* 00010000                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR CallId[1];				/* Call identity                        */
}Q931ie_CallID;

/*****************************************************************************

  Struct:       Q931ie_CallState


*****************************************************************************/

typedef struct
{
    L3UCHAR IEId;                   /* 00010100                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR CodStand;               /* Coding Standard                      */
    L3UCHAR CallState;              /* Call State Value                     */
}Q931ie_CallState;

/*****************************************************************************

  Struct:		Q931ie_Cause

  Description:	Cause IE as described in Q.850

*****************************************************************************/
typedef struct
{
    L3UCHAR IEId;                   /* 00010100                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR CodStand;               /* Coding Standard                      */
	L3UCHAR Location;				/* Location								*/
	L3UCHAR Recom;					/* Recommendation						*/
	L3UCHAR Value;					/* Cause Value							*/
	L3UCHAR	Diag[1];				/* Optional Diagnostics Field			*/
}Q931ie_Cause;

/*****************************************************************************

  Struct:        Q931ie_CalledNum

*****************************************************************************/
typedef struct
{
    L3UCHAR IEId;                   /* 01110000                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR TypNum;                 /* Type of Number                       */
    L3UCHAR NumPlanID;              /* Numbering plan identification        */
    L3UCHAR Digit[1];				/* Digit (IA5)                          */
}Q931ie_CalledNum;

/*****************************************************************************

  Struct:       Q931ie_CalledSub

  Description:  Called party subaddress

*****************************************************************************/

typedef struct
{
    L3UCHAR IEId;                   /* 01110001                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR TypNum;                 /* Type of subaddress                   */
    L3UCHAR OddEvenInd;             /* Odd/Even indicator                   */
    L3UCHAR Digit[1];				/* digits                               */
}Q931ie_CalledSub;

/*****************************************************************************

  Struct:       Q931ie_CallingNum

  Description:  Calling party number

*****************************************************************************/

typedef struct
{
    L3UCHAR IEId;                   /* 01101100                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR TypNum;                 /* Type of number                       */
    L3UCHAR NumPlanID;              /* Numbering plan identification        */
    L3UCHAR PresInd;                /* Presentation indicator               */
    L3UCHAR ScreenInd;              /* Screening indicator                  */
    L3UCHAR Digit[1];				/* Number digits (IA5)                  */
}Q931ie_CallingNum;

/*****************************************************************************

  Struct:        Q931ie_CallingSub

  Description:   Calling party subaddress

*****************************************************************************/

typedef struct
{
    L3UCHAR IEId;                   /* 01101101                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR TypNum;                 /* Type of subaddress                   */
    L3UCHAR OddEvenInd;             /* Odd/Even indicator                   */
    L3UCHAR Digit[1];				/* digits                               */
}Q931ie_CallingSub;

/*****************************************************************************

  Struct:       Q931ie_ChanID

  Description:  Channel identification

				Channel Identificationis one of the IE elements that differ
				between BRI and PRI. IntType = 1 = BRI and ChanSlot is used
				for channel number, while InfoChanSel is used for BRI.

				ChanID is one of the most important IE as it is passed	
				either though SETUP or CALL PROCEEDING to select the channel
				to be used.

*****************************************************************************/

typedef struct
{
    L3UCHAR IEId;                   /* 00011000                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR IntIDPresent;           /* Int. id. present                     */
    L3UCHAR IntType;                /* Interface Type                       */
									/*	0 : Basic Interface	(BRI)			*/
									/*  1 : Other interfaces, PRI etc.		*/

    L3UCHAR PrefExcl;               /* Pref./Excl.                          */
									/*	0 : Indicated channel is preffered	*/
									/*  1 : Exclusive, no other accepted	*/

    L3UCHAR DChanInd;               /* D-channel ind.                       */
									/*  0 : chan is NOT D chan.				*/
									/*  1 : chan is D chan					*/

    L3UCHAR InfoChanSel;            /* Info. channel selection              */
									/*  00 : No channel						*/
									/*  01 : B1 channel						*/
									/*  10 : B2 channel						*/
									/*  11 : Any channel					*/

    L3UCHAR InterfaceID;            /* Interface identifier                 */

    L3UCHAR CodStand;		        /* Code standard                        */
									/*  00 : ITU-T standardization coding	*/
									/*  01 : ISO/IEC Standard				*/
									/*  10 : National Standard				*/
									/*  11 : Standard def. by network.		*/

    L3UCHAR NumMap;                 /* Number/Map                           */
									/*  0 : chan is in following octet		*/
									/*  1 : chan is indicated by slot map	*/

    L3UCHAR ChanMapType;            /* Channel type/Map element type        */
									/*  0011 : B Channel units				*/
									/*  0110 : H0 channel units				*/
									/*  1000 : H11 channel units			*/
									/*  1001 : H12 channel units			*/

    L3UCHAR ChanSlot;               /* Channel number						*/
}Q931ie_ChanID;

/*****************************************************************************

  Struct:       Q931ie_DateTime

  Description:  Date/time

*****************************************************************************/

typedef struct
{
    L3UCHAR IEId;                   /* 00101001                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR Year;                   /* Year                                 */
    L3UCHAR Month;                  /* Month                                */
    L3UCHAR Day;                    /* Day                                  */
    L3UCHAR Hour;                   /* Hour                                 */
    L3UCHAR Minute;                 /* Minute                               */
    L3UCHAR Second;                 /* Second                               */
	L3UCHAR Format;					/* Indicate presense of Hour, Min & sec */
									/*	0 : Only Date						*/
									/*  1 : Hour present					*/
									/*  2 : Hour and Minute present			*/
									/*  3 : Hour, Minute and Second present	*/
}Q931ie_DateTime;

/*****************************************************************************

  Struct:       Q931ie_Display

  Description:  Display

*****************************************************************************/

typedef struct
{
    L3UCHAR IEId;                   /* 00101000                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR Display[1];             /* Display information (IA5)            */
}Q931ie_Display;

/*****************************************************************************

  Struct:       Q931ie_HLComp

  Description:  High layer compatibility

*****************************************************************************/

typedef struct
{
    L3UCHAR IEId;                   /* 01111101                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR CodStand;               /* Coding standard                      */
    L3UCHAR Interpret;              /* Interpretation                       */
    L3UCHAR PresMeth;               /* Presentation methor of prot. profile */
    L3UCHAR HLCharID;               /* High layer characteristics id.       */
    L3UCHAR EHLCharID;              /* Extended high layer character. id.   */
    L3UCHAR EVideoTlfCharID;        /* Ext. videotelephony char. id.        */
}Q931ie_HLComp;

/*****************************************************************************

  Struct:       Q931ie_KeypadFac

  Description:  Keypad facility

*****************************************************************************/

typedef struct
{
    L3UCHAR IEId;                   /* 00101100                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR KeypadFac[1];           /* dynamic buffer                       */
}Q931ie_KeypadFac;

/*****************************************************************************

  Struct:       Q931ie_LLComp

  Description:  Low layer compatibility

*****************************************************************************/

typedef struct
{
    L3UCHAR IEId;                   /* 01111100                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR CodStand;               /* Coding standard                      */
                                    /*  00 - ITU-T                          */
                                    /*  01 - ISO/IEC                        */
                                    /*  10 - National standard              */
                                    /*  11 - Network side spesific          */

    L3UCHAR ITransCap;              /* Information transfer capability      */
                                    /*  00000 - Speech                      */
                                    /*  01000 - Unrestricted digital info   */
                                    /*  01001 - Restricted digital info     */
                                    /*  10000 - 3.1 kHz audio               */
                                    /*  10001 - Unrestricted with tones     */
                                    /*  11000 - Video                       */

    L3UCHAR NegotInd;               /* Negot indic.                         */
									/*	0 : Out-band neg. not possib.       */
									/*  1 : Out-band neg. possible	        */

    L3UCHAR TransMode;              /* Transfer Mode                        */
                                    /*  00 : Circuit Mode                   */
                                    /*  10 : Packed Mode                    */

    L3UCHAR InfoRate;               /* Information transfer rate            */
                                    /*  00000 - Packed mode                 */
                                    /*  10000 - 64 kbit/s                   */
                                    /*  10001 - 2 x 64 kbit/s               */
                                    /*  10011 - 384 kbit/s                  */
                                    /*  10101 - 1536 kbit/s                 */
                                    /*  10111 - 1920 kbit/s                 */
                                    /*  11000 - Multirat (64 kbit/s base)   */

    L3UCHAR RateMul;                /* Rate multiplier                      */
    L3UCHAR Layer1Ident;            /* Layer 1 ident.                       */
    L3UCHAR UIL1Prot;               /* User information layer 1 protocol    */
									/*	00001 : ITU-T V.110, I.460 and X.30 */
									/*  00010 : G.711 my-law				*/
									/*  00011 : G.711 A-law					*/
									/*  00100 : G.721						*/
                                    /*  00101 : H.221 and H.242				*/
									/*  00110 : H.223 and H.245				*/
									/*  00111 : Non ITU-T Standard			*/
									/*  01000 : ITU-T V.120					*/
									/*  01001 : ITU-T X.31 HDLC flag stuff.	*/

    L3UCHAR SyncAsync;              /* Synch/asynch                         */
									/*	0 : Syncronous data					*/
									/*	1 : Asyncronous data				*/

    L3UCHAR Negot;                  /* Negot                                */
									/*	0 : In-band negotiation not possib.	*/
									/*  1 : In-band negotiation possible	*/

    L3UCHAR UserRate;               /* User rate                            */
									/*	00000 : I.460, V.110, X,30			*/
									/*  00001 : 0.6 kbit/s x.1				*/
									/*  00010 : 1.2 kbit/s					*/
									/*  00011 : 2.4 kbit/s					*/
									/*  00100 : 3.6 kbit/s					*/
									/*  00101 : 4.8 kbit/s					*/
									/*  00110 : 7.2 kbit/s					*/
									/*  00111 : 8 kbit/s I.460				*/
									/*  01000 : 9.6 kbit/s					*/
									/*  01001 : 14.4 kbit/s					*/
									/*  01010 : 16 kbit/s					*/
									/*  01011 :	19.2 kbit/s					*/
									/*  01100 : 32 kbit/s					*/
									/*  01101 : 38.4 kbit/s					*/
									/*  01110 : 48 kbit/s					*/
									/*  01111 : 56 kbit/s					*/
									/*  10000 : 57.6 kbit/s					*/
									/*  10010 : 28.8 kbit/s					*/
									/*  10100 : 24 kbit/s					*/
									/*  10101 : 0.1345 kbit/s				*/
									/*  10110 : 0.100 kbit/s				*/
									/*  10111 : 0.075/1.2 kbit/s			*/
									/*  11000 : 1.2/0.075/kbit/s			*/
									/*  11001 : 0.050 kbit/s				*/
									/*  11010 : 0.075 kbit/s				*/
									/*  11011 : 0.110 kbit/s				*/
									/*  11100 : 0.150 kbit/s				*/
									/*  11101 : 0.200 kbit/s				*/
									/*  11110 : 0.300 kbit/s				*/
									/*  11111 : 12 kbit/s					*/

    L3UCHAR InterRate;              /* Intermediate rate                    */
									/*	00 : Not used						*/
									/*  01 : 8 kbit/s						*/
									/*  10 : 16 kbit/s						*/
									/*  11 : 32 kbit/s						*/

    L3UCHAR NIConTx;				/* Network Indepentend Clock on transmit*/
									/*	0 : Not required to send data  clc  */
									/*  1 : Send data w/NIC clc				*/

    L3UCHAR NIConRx;				/* NIC on Rx							*/
									/*	0 : Cannot accept indep. clc		*/
									/*  1 : data with indep. clc accepted	*/

    L3UCHAR FlowCtlTx;              /* Flow control on Tx                   */
									/*  0 : Send Flow ctrl not required		*/
									/*  1 : Send flow ctrl required			*/

    L3UCHAR FlowCtlRx;              /* Flow control on Rx                   */
									/*  0 : cannot use receive flow ctrl	*/
									/*  1 : Receive flow ctrl accepted		*/
    L3UCHAR HDR;					/* HDR/No HDR							*/
    L3UCHAR MultiFrame;             /* Multi frame support                  */
									/*  0 : multiframe not supported		*/
									/*  1 : multiframe supported			*/

	L3UCHAR ModeL1;					/* Mode L1								*/
									/*	0 : bit transparent mode of operat.	*/
									/*	1 : protocol sesitive mode of op.	*/

    L3UCHAR NegotLLI;               /* Negot. LLI                           */
									/*  0 : default LLI=256 only			*/
									/*  1 : Full protocol negotiation		*/

    L3UCHAR Assignor;               /* Assignor/Assignor ee                 */
									/*  0 : Default Asignee					*/
									/*  1 : Assignor only					*/

    L3UCHAR InBandNeg;              /* In-band negot.                       */
									/*  0 : negot done w/ USER INFO mes		*/
									/*  1 : negot done in-band w/link zero	*/

    L3UCHAR NumStopBits;            /* Number of stop bits					*/
									/*  00 : Not used						*/
									/*  01 : 1 bit							*/
									/*  10 : 1.5 bits						*/
									/*  11 : 2 bits							*/

    L3UCHAR NumDataBits;            /* Number of data bits.                 */
									/*  00 : not used						*/
									/*  01 : 5 bits							*/
									/*  10 : 7 bits							*/
									/*  11 : 8 bits							*/

    L3UCHAR Parity;					/* Parity Information					*/
									/*	000 : Odd							*/
									/*  010 : Even							*/
									/*  011 : None							*/
									/*  100 : Forced to 0					*/
									/*  101 : Forced to 1					*/

    L3UCHAR DuplexMode;				/* Mode duplex							*/
									/*  0 : Half duplex						*/
									/*  1 : Full duplex						*/

    L3UCHAR ModemType;				/* Modem type, see Q.931 p 89			*/

    L3UCHAR Layer2Ident;            /* Layer 2 ident.                       */

    L3UCHAR UIL2Prot;               /* User information layer 2 protocol    */
                                    /*  00001 : Basic mode ISO 1745         */
									/*	00010 : Q.921/I.441					*/
									/*  00110 : X.25 single link			*/
                                    /*  00111 : X.25 multilink              */
                                    /*  01000 : Extended LAPB T.71          */
                                    /*  01001 : HDLC ARM                    */
                                    /*  01010 : HDLC NRM                    */
                                    /*  01011 : HDLC ABM                    */
									/*  01100 : LAN logical link			*/
                                    /*  01101 : X.75 SLP                    */
                                    /*  01110 : Q.922                       */
                                    /*  01111 : Q.922 core aspect           */
                                    /*  10000 : User specified              */
                                    /*  10001 : ISO/IEC 7776 DTE-DCE        */

    L3UCHAR ModeL2;                 /* Mode                                 */
									/*	01 : Normal Mode of operation    	*/
									/*	10 : Extended mode of operation  	*/

    L3UCHAR Q933use;                /* Q.9333 use                           */

    L3UCHAR UsrSpcL2Prot;           /* User specified layer 2 protocol info */

    L3UCHAR WindowSize;             /* Window size (k)                      */

    L3UCHAR Layer3Ident;            /* Layer 3 ident                        */

    L3UCHAR UIL3Prot;				/* User Information Layer 3 protocol	*/
									/*	00010 : Q.931						*/
									/*  00110 : X.25						*/
                                    /*  00111 : 8208                        */
                                    /*  01000 : X.233 ...                   */
                                    /*  01001 : 6473                        */
                                    /*  01010 : T.70                        */
									/*  01011 : ISO/IEC TR 9577				*/
                                    /*  10000 : User specified              */
    L3UCHAR OptL3Info;              /* Optional Leyer 3 info                */

    L3UCHAR ModeL3;                 /* Mode of operation                    */
                                    /*  01 : Normal packed seq. numbering   */
                                    /*  10 : Extended packed seq. numbering */

    L3UCHAR DefPackSize;            /* Default packet size                  */

    L3UCHAR PackWinSize;            /* Packet window size                   */

    L3UCHAR AddL3Info;              /* Additional Layer 3 protocol info     */
}Q931ie_LLComp;

/*****************************************************************************

  Struct:       Q931ie_NetFac;

  Description:  Network-specific facilities

*****************************************************************************/

typedef struct
{
    L3UCHAR IEId;                   /* 00100000                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR LenNetID;               /* Length of network facilities id.     */
    L3UCHAR TypeNetID;              /* Type of network identification       */
    L3UCHAR NetIDPlan;              /* Network identification plan.         */
    L3UCHAR NetFac;                 /* Network specific facility spec.      */
    L3UCHAR NetID[1];               /* Network id. (IA5)                    */
}Q931ie_NetFac;

/*****************************************************************************

  Struct:       Q931ie_NotifInd;

  Description:  Notification Indicator

*****************************************************************************/

typedef struct
{
    L3UCHAR IEId;                   /* 00100000                             */
    L3UCHAR Size;                   /* Length of Information Element        */
	L3UCHAR Notification;			/* Notification descriptor				*/
}Q931ie_NotifInd;

/*****************************************************************************

  Struct:       Q931ie_ProgInd

  Description:  Progress indicator

*****************************************************************************/

typedef struct
{
    L3UCHAR IEId;                   /* 00011110                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR CodStand;               /* Coding standard                      */
    L3UCHAR Location;               /* Location                             */
    L3UCHAR ProgDesc;               /* Progress description                 */
}Q931ie_ProgInd;

/*****************************************************************************

  Struct;       Q931ie_Segment

  Description:  Segmented message

*****************************************************************************/

typedef struct
{
    L3UCHAR IEId;                   /* 00000000                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR FSI;                    /* First segment indicator              */
    L3UCHAR NumSegRem;              /* Number of segments remaining         */
    L3UCHAR SegType;                /* Segment message type                 */
}Q931ie_Segment;

typedef struct
{
    L3UCHAR IEId;                   /* 00000000                             */
    L3UCHAR Size;                   /* Length of Information Element        */
}Q931ie_SendComplete;

/*****************************************************************************

  Struct:       Q931ie_Signal

  Description:  Signal

*****************************************************************************/

typedef struct
{
    L3UCHAR IEId;                   /* 00000000                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR Signal;                 /* Signal value                         */
                                    /*  00000000    Dial tone on            */
                                    /*  00000001    Ring back tone on       */
                                    /*  00000010    Intercept tone on       */
                                    /*  00000011    Network congestion on   */
                                    /*  00000100    Busy tone on            */
                                    /*  00000101    Confirm tone on         */
                                    /*  00000110    Answer tone on          */
                                    /*  00000111    Call waiting tone       */
                                    /*  00001000    Off-hook warning tone   */
                                    /*  00001001    Pre-emption tone on     */
                                    /*  00111111    Tones off               */
                                    /*  01000000    Alerting on - pattern 0 */
                                    /*  01000001    Alerting on - pattern 1 */
                                    /*  01000010    Alerting on - pattern 2 */
                                    /*  01000011    Alerting on - pattern 3 */
                                    /*  01000100    Alerting on - pattern 4 */
                                    /*  01000101    Alerting on - pattern 5 */
                                    /*  01000110    Alerting on - pattern 6 */
                                    /*  01000111    Alerting on - pattern 7 */
                                    /*  01001111    Alerting off            */
}Q931ie_Signal;

/*****************************************************************************

  Struct:       Q931ie_TransDelSelInd

  description:  Transit delay selection and indication

*****************************************************************************/
#ifdef Q931_X25_SUPPORT
typedef struct
{
    L3UCHAR IEId;                   /* 00000000                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3ULONG TxDSIValue;             /* Trans. delay sel. & ind. value       */ 
}Q931ie_TransDelSelInd;
#endif

/*****************************************************************************

  Struct:       Q931ie_TransNetSel

  Description:  Transit network selection

*****************************************************************************/

typedef struct
{
    L3UCHAR IEId;                   /* 01111000                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR Type;                   /* Type of network identifier           */
    L3UCHAR NetIDPlan;              /* Network idetification plan           */
    L3UCHAR NetID[1];               /* Network identification(IA5)          */
}Q931ie_TransNetSel;

/*****************************************************************************

  Struct:       Q931ie_UserUser

  Description:  User-user

*****************************************************************************/

typedef struct
{
    L3UCHAR IEId;                   /* 01111110                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR ProtDisc;               /* Protocol discriminator               */
    L3UCHAR User[1];                /* User information                     */
}Q931ie_UserUser;

/*****************************************************************************

  Struct:       Q931ie_ClosedUserGrp

  Description:  Closed user group

*****************************************************************************/
#ifdef Q931_X25_SUPPORT
typedef struct
{
    L3UCHAR IEId;                   /* 01000111                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR CUGInd;                 /* CUG indication                       */
    L3UCHAR CUG[1];                 /* CUG index code (IA5)                 */
}Q931ie_ClosedUserGrp;
#endif

/*****************************************************************************

  Struct:		Q931ie_CongLevel

  Description:	Congestion Level

*****************************************************************************/
typedef struct
{
    L3UCHAR IEId;                   /* 01000111                             */
    L3UCHAR Size;                   /* Length of Information Element        */
	L3UCHAR CongLevel;				/* Conguestion Level					*/
}Q931ie_CongLevel;

/*****************************************************************************

  Struct:       Q931ie_EndEndTxDelay

  Description:  End to end transit delay

*****************************************************************************/
#ifdef Q931_X25_SUPPORT
typedef struct
{
    L3UCHAR IEId;                   /* 01000010                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3ULONG CumTxDelay;             /* Cumulative transit delay value       */
    L3ULONG ReqTxDelay;             /* Requested end to end transit delay   */
    L3ULONG MaxTxDelay;             /* Maximum transit delay                */
}Q931ie_EndEndTxDelay;
#endif

/*****************************************************************************

  Struct:       Q931ie_InfoRate

  Description:  Information Rate

*****************************************************************************/
#ifdef Q931_X25_SUPPORT
typedef struct
{
    L3UCHAR IEId;                   /* 01100000                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR InInfoRate;             /* Incoming information rate            */
    L3UCHAR OutInfoRate;            /* Outgoing information rate            */
    L3UCHAR MinInInfoRate;          /* Minimum incoming information rate    */
    L3UCHAR MinOutInfoRate;         /* Minimum outgoing information rate    */
}Q931ie_InfoRate;
#endif

/*****************************************************************************

  Struct:       Q931ie_PackParam

  Description:  Packed layer binary parameters

*****************************************************************************/
#ifdef Q931_X25_SUPPORT
typedef struct
{
    L3UCHAR IEId;                   /* 01000100                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR FastSel;                /* Fast selected                        */
    L3UCHAR ExpData;                /* Exp. data                            */
    L3UCHAR DelConf;                /* Delivery conf                        */
    L3UCHAR Modulus;                /* Modulus                              */
}Q931ie_PackParam;
#endif

/*****************************************************************************

  Struct:       Q931ie_PackWinSize

  Description:  Packed window size

*****************************************************************************/
#ifdef Q931_X25_SUPPORT
typedef struct
{
    L3UCHAR IEId;                   /* 01000101                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR ForwardValue;           /* Forward value                        */
    L3UCHAR BackwardValue;          /* Backward value                       */
}Q931ie_PackWinSize;
#endif

/*****************************************************************************

  Struct:       Q931ie_PackSize

  Description:  Packet size

*****************************************************************************/
#ifdef Q931_X25_SUPPORT
typedef struct
{
    L3UCHAR IEId;                   /* 01000110                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR ForwardValue;           /* Forward value                        */
    L3UCHAR BackwardValue;          /* Backward value                       */
}Q931ie_PackSize;
#endif

/*****************************************************************************

  Struct:       Q931ie_RedirNum

  Description:  Redirecting number

*****************************************************************************/
#ifdef Q931_X25_SUPPORT
typedef struct
{
    L3UCHAR IEId;                   /* 01110100                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR TypeNum;                /* Type of number                       */
    L3UCHAR NumPlanID;              /* Number plan identification           */
    L3UCHAR PresInd;                /* Presentation indicator               */
    L3UCHAR ScreenInd;              /* Screening indicator                  */
    L3UCHAR Reason;                 /* Reason for redirection               */
    L3UCHAR Digit[1];               /* Number digits (IA5)                  */
}Q931ie_RedirNum;
#endif

typedef struct
{
    L3UCHAR IEId;                   /* 01110100                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR RepeatInd;              /* 0010 Prioritized list for selecting  */
                                    /* one possible.                        */
}Q931ie_RepeatInd;

typedef struct
{
    L3UCHAR IEId;                   /* 01110100                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR Class;                  /* Class                                */
                                    /*  000 Indicate channels               */
                                    /*  110 Single interface                */
                                    /*  111 All interfaces                  */
}Q931ie_RestartInd;

/*****************************************************************************

  Struct:       Q931mes_Header

  Description:  Used to read the header & message code.

*****************************************************************************/
typedef struct
{
    L3UINT			Size;           /* Size of message in bytes             */
    L3UCHAR         ProtDisc;       /* Protocol Discriminator               */
    L3UCHAR         MesType;        /* Message type                         */
    L3INT           CRV;            /* Call reference value                 */
}Q931mes_Header;

/*****************************************************************************

  Struct:       Q931mes_Generic

  Description:  Generic header containing all IE's. This is not used, but is
				provided in case a proprietary variant needs it.

*****************************************************************************/
typedef struct
{
    L3UINT			Size;           /* Size of message in bytes             */
    L3UCHAR         ProtDisc;       /* Protocol Discriminator               */
    L3UCHAR         MesType;        /* Message type                         */
    L3INT           CRV;            /* Call reference value                 */

    ie              Shift;
    ie              MoreData;
    ie              SendComplete;
    ie              CongestionLevel;
    ie              RepeatInd;

    ie              Segment;        /* Segmented message                    */
    ie              BearerCap;      /* Bearer Capability                    */
    ie              Cause;          /* Cause                                */
    ie              CallID;			/* Call Identity                        */
    ie              ChanID;         /* Channel Identification               */
    ie              ProgInd;        /* Progress Indicator                   */
    ie              NetFac;         /* Network Spesific Facilities          */
    ie              NotifInd;       /* Notification Indicator               */
    ie              Display;        /* Display                              */
    ie              DateTime;       /* Date/Time                            */
    ie              KeypadFac;      /* Keypad Facility                      */
    ie              Signal;         /* Signal                               */
    ie              InfoRate;       /* Information rate                     */
    ie              EndEndTxDelay;  /* End to End Transmit Delay            */
    ie              TransDelSelInd; /* Transmit Delay Sel. and Ind.         */
    ie              PackParam;      /* Packed Layer Binary parameters       */
    ie              PackWinSize;    /* Packet Layer Window Size             */
    ie              PackSize;       /* Packed Size                          */
    ie              ClosedUserGrp;  /* Closed User Group                    */
    ie              RevChargeInd;   /* Reverse Charging Indicator           */
    ie              CalledNum;      /* Called Party Number                  */
    ie              CalledSub;      /* Called Party subaddress              */
    ie              CallingNum;     /* Calling Party Number                 */
    ie              CallingSub;     /* Calling Party Subaddress             */
    ie              RedirNum;       /* Redirection Number                   */
    ie              TransNetSel;    /* Transmit Network Selection           */
    ie              RestartInd;     /* Restart Indicator                    */
    ie              LLComp;         /* Low Layer Compatibility              */
    ie              HLComp;         /* High Layer Compatibility             */
    ie              UserUser;       /* User-user                            */
    ie              Escape;         /* Escape for extension                 */
	L3UCHAR			buf[1];			/* Buffer for IE's						*/

}Q931mes_Generic;

/*****************************************************************************

  Struct:        Q931mes_Alerting

  Description:   This message is send by the called party to indicate that
                 the phone is ringing.

				 The message consist of an header that MUST be identical with
				 Q931mes_Header, a fixed number of IE flags w/offset, and
				 a dynamic buffer storing normalized IE's.

*****************************************************************************/
typedef struct
{
    L3UINT			Size;           /* Size of message in bytes             */
    L3UCHAR         ProtDisc;       /* Protocol Discriminator               */
    L3UCHAR         MesType;        /* Message type                         */
    L3INT           CRV;            /* Call reference value                 */
    ie              BearerCap;      /* Bearer Capability                    */
    ie              ChanID;         /* Channel Identification               */
    ie              ProgInd;        /* Progress Indicator                   */
    ie              Display;        /* Display                              */
    ie              Signal;         /* Signal                               */
    ie              HLComp;         /* High Layer Compatibility             */
    L3UCHAR         buf[1];         /* Dynamic buffer                       */
}Q931mes_Alerting;

/*****************************************************************************

  Struct:        Q931mes_CallProceeding

*****************************************************************************/

typedef struct
{
    L3UINT			Size;           /* Size of message in bytes             */
    L3UCHAR         ProtDisc;       /* Protocol Discriminator               */
    L3UCHAR         MesType;        /* Message type                         */
    L3INT           CRV;            /* Call reference value                 */
    ie              BearerCap;      /* Bearer Capability                    */
    ie              ChanID;         /* Channel Identification               */
    ie              ProgInd;        /* Progress Indicator                   */
    ie              Display;        /* Display                              */
    ie              HLComp;         /* High Layer Compatibility             */
    L3UCHAR         buf[1];         /* Dynamic buffer                       */
}Q931mes_CallProceeding;

/*****************************************************************************

  Struct:        Q931mes_Connect

*****************************************************************************/

typedef struct
{
    L3UINT          Size;           /* Size of message in bytes             */
    L3UCHAR         ProtDisc;       /* Protocol Discriminator               */
    L3UCHAR         MesType;        /* Message type                         */
    L3INT           CRV;            /* Call reference value                 */
    ie              BearerCap;      /* Bearer Capability                    */
    ie              ChanID;         /* Channel Identification               */
    ie              ProgInd;        /* Progress Indicator                   */
    ie              Display;        /* Display                              */
    ie              DateTime;       /* Date/Time                            */
    ie              Signal;         /* Signal                               */
    ie              LLComp;         /* Low Layer Compatibility              */
    ie              HLComp;         /* High layer Compatibility             */
    L3UCHAR         buf[1];         /* Dynamic buffer                       */
}Q931mes_Connect;

/*****************************************************************************

  Struct:        Q931mes_ConnectAck

*****************************************************************************/

typedef struct
{
    L3UINT          Size;           /* Size of message in bytes             */
    L3UCHAR         ProtDisc;       /* Protocol Discriminator               */
    L3UCHAR         MesType;        /* Message type                         */
    L3INT           CRV;            /* Call reference value                 */
    ie              Display;        /* Display                              */
    ie              Signal;         /* Signal                               */
    L3UCHAR         buf[1];         /* Dynamic buffer                       */
}Q931mes_ConnectAck;

/*****************************************************************************

  Struct:        Q931mes_Disconnect

*****************************************************************************/

typedef struct
{
    L3UINT          Size;           /* Size of message in bytes             */
    L3UCHAR         ProtDisc;       /* Protocol Discriminator               */
    L3UCHAR         MesType;        /* Message type                         */
    L3INT           CRV;            /* Call reference value                 */
    ie              Cause;          /* Cause                                */
    ie              ProgInd;        /* Progress Indicator                   */
    ie              Display;        /* Display                              */
    ie              Signal;         /* Signal                               */
#ifdef Q931_X25_SUPPORT
	ie              UserUser;       /* User - user. Packed Mode.            */
#endif
    L3UCHAR         buf[1];         /* Dynamic buffer                       */
}Q931mes_Disconnect;

/*****************************************************************************

  Struct:        Q931mes_Information

*****************************************************************************/

typedef struct
{
    L3UINT          Size;           /* Size of message in bytes             */
    L3UCHAR         ProtDisc;       /* Protocol Discriminator               */
    L3UCHAR         MesType;        /* Message type                         */
    L3INT           CRV;            /* Call reference value                 */
    ie              SendComplete;   /* Sending Complete                     */
    ie              Display;        /* Display                              */
    ie              KeypadFac;      /* Keypad facility                      */
    ie              Signal;         /* Signal                               */
    ie              CalledNum;      /* Called party number                  */
#ifdef Q931_X25_SUPPORT
    ie              Cause;          /* Cause (packed only)                  */
#endif
    L3UCHAR         buf[1];         /* Dynamic buffer                       */
}Q931mes_Information;

/*****************************************************************************

  Struct:        Q931mes_Notify

*****************************************************************************/

typedef struct
{
    L3UINT          Size;           /* Size of message in bytes             */
    L3UCHAR         ProtDisc;       /* Protocol Discriminator               */
    L3UCHAR         MesType;        /* Message type                         */
    L3INT           CRV;            /* Call reference value                 */
    ie              BearerCap;      /* Bearer Capability                    */
    ie              NotifInd;       /* Notification Indicator               */
    ie              Display;        /* Display                              */
    L3UCHAR         buf[1];         /* Dynamic buffer                       */
}Q931mes_Notify;

/*****************************************************************************

  Struct:        Q931mes_Progress

*****************************************************************************/

typedef struct
{
    L3UINT          Size;           /* Size of message in bytes             */
    L3UCHAR         ProtDisc;       /* Protocol Discriminator               */
    L3UCHAR         MesType;        /* Message type                         */
    L3INT           CRV;            /* Call reference value                 */
    ie              BearerCap;      /* Bearer Capability                    */
    ie              Cause;          /* Cause                                */
    ie              ProgInd;        /* Progress Indicator                   */
    ie              Display;        /* Display                              */
    ie              HLComp;         /* High Layer Compatibility             */
    L3UCHAR         buf[1];         /* Dynamic buffer                       */
}Q931mes_Progress;

/*****************************************************************************

  Struct:        Q931mes_Release

*****************************************************************************/

typedef struct
{
    L3UINT          Size;           /* Size of message in bytes             */
    L3UCHAR         ProtDisc;       /* Protocol Discriminator               */
    L3UCHAR         MesType;        /* Message type                         */
    L3INT           CRV;            /* Call reference value                 */
    ie              Cause;          /* Cause                                */
    ie              Display;        /* Display                              */
    ie              Signal;         /* Signal                               */
#ifdef Q931_X25_SUPPORT
    ie              UserUser;       /* User-User packed mode                */
#endif
    L3UCHAR         buf[1];         /* Dynamic buffer                       */
}Q931mes_Release;

/*****************************************************************************

  Struct:        Q931mes_ReleaseComplete

*****************************************************************************/

typedef struct
{
    L3UINT          Size;           /* Size of message in bytes             */
    L3UCHAR         ProtDisc;       /* Protocol Discriminator               */
    L3UCHAR         MesType;        /* Message type                         */
    L3INT           CRV;            /* Call reference value                 */
    ie              Cause;          /* Cause                                */
    ie              Display;        /* Display                              */
    ie              Signal;         /* Signal                               */
#ifdef Q931_X25_SUPPORT
    ie              UserUser;       /* User-User packed mode                */
#endif
    L3UCHAR          buf[1];         /* Dynamic buffer                       */
}Q931mes_ReleaseComplete;

/*****************************************************************************

  Struct:        Q931mes_Resume

*****************************************************************************/
typedef struct
{
    L3UINT          Size;           /* Size of message in bytes             */
    L3UCHAR         ProtDisc;       /* Protocol Discriminator               */
    L3UCHAR         MesType;        /* Message type                         */
    L3INT           CRV;            /* Call reference value                 */
    ie              CallID;         /* Call Identity                        */
	L3UCHAR			buf[1];
}Q931mes_Resume;

/*****************************************************************************

  Struct:        Q931mes_ResumeAck

*****************************************************************************/

typedef struct
{
    L3UINT          Size;           /* Size of message in bytes             */
    L3UCHAR         ProtDisc;       /* Protocol Discriminator               */
    L3UCHAR         MesType;        /* Message type                         */
    L3INT           CRV;            /* Call reference value                 */
    ie              ChanID;         /* Channel ID                           */
    ie              Display;        /* Display                              */
    L3UCHAR         buf[1];         /* Dynamic buffer                       */
}Q931mes_ResumeAck;

/*****************************************************************************

  Struct:        Q931mes_ResumeReject

*****************************************************************************/

typedef struct
{
    L3UINT          Size;           /* Size of message in bytes             */
    L3UCHAR         ProtDisc;       /* Protocol Discriminator               */
    L3UCHAR         MesType;        /* Message type                         */
    L3INT           CRV;            /* Call reference value                 */
    ie              Cause;          /* Cause                                */
    ie              Display;        /* Display                              */
    L3UCHAR         buf[1];         /* Dynamic buffer                       */
}Q931mes_ResumeReject;

/*****************************************************************************

  Struct:        Q931mes_Segment

*****************************************************************************/

typedef struct
{
    L3UINT          Size;           /* Size of message in bytes             */
    L3UCHAR         ProtDisc;       /* Protocol Discriminator               */
    L3UCHAR         MesType;        /* Message type                         */
    L3INT           CRV;            /* Call reference value                 */
    L3UCHAR         buf[1];         /* Dynamic buffer                       */
}Q931mes_Segment;

/*****************************************************************************

  Struct:        Q931mes_Setup

*****************************************************************************/

typedef struct
{
    L3UINT          Size;           /* Size of message in bytes             */
    L3UCHAR         ProtDisc;       /* Protocol Discriminator               */
    L3UCHAR         MesType;        /* Message type                         */
    L3INT           CRV;            /* Call reference value                 */
    ie              SendComplete;   /* Sending Complete                     */
    ie              RepeatInd;      /* Repeat Indicator                     */
    ie              BearerCap;      /* Bearer Capability                    */
    ie              ChanID;         /* Channel ID                           */
    ie              ProgInd;        /* Progress Indicator                   */
    ie              NetFac;         /* Network-specific facilities          */
                                    /* Note:    Up to 4 NetFac's may be     */
                                    /*          included.                   */
    ie              Display;        /* Display                              */
    ie              DateTime;       /* Date/Time                            */
    ie              KeypadFac;      /* Keypad Facility                      */
    ie              Signal;         /* Signal                               */
    ie              CallingNum;     /* Calling party number                 */
    ie              CallingSub;     /* Calling party sub address            */
    ie              CalledNum;      /* Called party number                  */
    ie              CalledSub;      /* Called party sub address             */
    ie              TransNetSel;    /* Transit network selection            */
    ie              LLRepeatInd;    /* Repeat Indicator 2 LLComp            */
    ie              LLComp;         /* Low layer compatibility              */
    ie              HLComp;         /* High layer compatibility             */

#ifdef Q931_X25_SUPPORT
    /* Packed mode additions */
    ie              IndoRate;       /* Information Rate                     */
    ie              EndEndDelay;    /* End-end transit delay                */
    ie              TransDelayInd;  /* TRansit delay selection and ind.     */
    ie              PackParam;      /* Packed layer binary parameters       */
    ie              PackWinSize;    /* Packed layer window size             */
    ie              PackSize;       /* Packet Size                          */
    ie              ClosedUserGrp;  /* Closed User Group                    */
    ie              RevChargInd;    /* Reverse charhing indicator           */
    ie              RedirNum;       /* Redirection number                   */
    ie              UserUser;       /* User-user.                           */
#endif
    L3UCHAR         buf[1];         /* Dynamic buffer                       */
}Q931mes_Setup;

/*****************************************************************************

  Struct:        Q931mes_SetupAck

*****************************************************************************/

typedef struct
{
    L3UINT          Size;           /* Size of message in bytes             */
    L3UCHAR         ProtDisc;       /* Protocol Discriminator               */
    L3UCHAR         MesType;        /* Message type                         */
    L3INT           CRV;            /* Call reference value                 */
    ie              ChanID;         /* Channel ID                           */
    ie              ProgInd;        /* Progress Indicator                   */
    ie              Display;        /* Display                              */
    ie              Signal;         /* Signal                               */
    L3UCHAR         buf[1];         /* Dynamic buffer                       */
}Q931mes_SetupAck;

/*****************************************************************************

  Struct:        Q931mes_Status

*****************************************************************************/

typedef struct
{
    L3UINT          Size;           /* Size of message in bytes             */
    L3UCHAR         ProtDisc;       /* Protocol Discriminator               */
    L3UCHAR         MesType;        /* Message type                         */
    L3INT           CRV;            /* Call reference value                 */
    ie              Cause;          /* Cause                                */
    ie              CallState;      /* Call State                           */
    ie              Display;        /* Display                              */
    L3UCHAR         buf[1];         /* Dynamic buffer                       */
}Q931mes_Status;

/*****************************************************************************

  Struct:        Q931mes_StatusEnquire

*****************************************************************************/

typedef struct
{
    L3UINT          Size;           /* Size of message in bytes             */
    L3UCHAR         ProtDisc;       /* Protocol Discriminator               */
    L3UCHAR         MesType;        /* Message type                         */
    L3INT           CRV;            /* Call reference value                 */
    ie              Display;        /* Display                              */
    L3UCHAR         buf[1];         /* Dynamic buffer                       */
}Q931mes_StatusEnquiry;

/*****************************************************************************

  Struct:        Q931mes_Suspend

*****************************************************************************/

typedef struct
{
    L3UINT          Size;           /* Size of message in bytes             */
    L3UCHAR         ProtDisc;       /* Protocol Discriminator               */
    L3UCHAR         MesType;        /* Message type                         */
    L3INT           CRV;            /* Call reference value                 */
    ie              CallID;         /* Call Identity                        */
    L3UCHAR         buf[1];         /* Dynamic buffer                       */
}Q931mes_Suspend;

/*****************************************************************************

  Struct:        Q931mes_SuspendAck

*****************************************************************************/

typedef struct
{
    L3UINT          Size;           /* Size of message in bytes             */
    L3UCHAR         ProtDisc;       /* Protocol Discriminator               */
    L3UCHAR         MesType;        /* Message type                         */
    L3INT           CRV;            /* Call reference value                 */
    ie              Display;        /* Display                              */
    L3UCHAR         buf[1];         /* Dynamic buffer                       */
}Q931mes_SuspendAck;

/*****************************************************************************

  Struct:        Q931mes_SuspendReject

*****************************************************************************/

typedef struct
{
    L3UINT          Size;           /* Size of message in bytes             */
    L3UCHAR         ProtDisc;       /* Protocol Discriminator               */
    L3UCHAR         MesType;        /* Message type                         */
    L3INT           CRV;            /* Call reference value                 */
    ie              Cause;          /* Cause                                */
    ie              Display;        /* Display                              */
    L3UCHAR         buf[1];         /* Dynamic buffer                       */
}Q931mes_SuspendReject;

/*****************************************************************************

  Struct:        Q931mes_CongestionControl

*****************************************************************************/

typedef struct
{
    L3UINT          Size;           /* Size of message in bytes             */
    L3UCHAR         ProtDisc;       /* Protocol Discriminator               */
    L3UCHAR         MesType;        /* Message type                         */
    L3INT           CRV;            /* Call reference value                 */
    ie              CongLevel;      /* Congestion level                     */
    ie              Cause;          /* Cause                                */
    ie              Display;        /* Display                              */
    L3UCHAR         buf[1];         /* Dynamic buffer                       */
}Q931mes_CongestionControl;

/*****************************************************************************

  Struct:        Q931mes_UserInformation

*****************************************************************************/

typedef struct
{
    L3UINT          Size;           /* Size of message in bytes             */
    L3UCHAR         ProtDisc;       /* Protocol Discriminator               */
    L3UCHAR         MesType;        /* Message type                         */
    L3INT           CRV;            /* Call reference value                 */
    ie              MoreData;       /* More data                            */
    ie              UserUser;       /* User-user                            */
    L3UCHAR         buf[1];         /* Dynamic buffer                       */
}Q931mes_UserInformation;

/*****************************************************************************

  Struct:        Q931mes_Restart

*****************************************************************************/

typedef struct
{
    L3UINT          Size;           /* Size of message in bytes             */
    L3UCHAR         ProtDisc;       /* Protocol Discriminator               */
    L3UCHAR         MesType;        /* Message type                         */
    L3INT           CRV;            /* Call reference value                 */
    ie              ChanID;         /* Channel identification               */
    ie              Display;        /* Display                              */
    ie              RestartInd;     /* Restart indicator                    */
    L3UCHAR         buf[1];         /* Dynamic buffer                       */
}Q931mes_Restart;

/*****************************************************************************

  Struct:        Q931mes_RestartAck

*****************************************************************************/

typedef struct
{
    L3UINT          Size;           /* Size of message in bytes             */
    L3UCHAR         ProtDisc;       /* Protocol Discriminator               */
    L3UCHAR         MesType;        /* Message type                         */
    L3INT           CRV;            /* Call reference value                 */
    ie              ChanID;         /* Channel identification               */
    ie              Display;        /* Display                              */
    ie              RestartWin;     /* Restart Window                       */
    ie              RestartInd;     /* Restart Indicator                    */
    L3UCHAR         buf[1];         /* Dynamic buffer                       */
}Q931mes_RestartAck;

/*****************************************************************************

  Struct:       Q931_TrunkInfo

  Description:  TrunkInfo is the struct entry used to store Q.931 related 
                information and state for E1/T1/J1 trunks and assosiated 
                channels in the system. 

				The user should store this information outside this stack
				and need to feed the interface functions with a pointer to
				the trunk Info entry.

*****************************************************************************/
typedef struct Q931_TrunkInfo Q931_TrunkInfo;

typedef L3INT (*Q931TxCB_t) (void *,L3UCHAR *, L3INT);
typedef L3INT (*Q931ErrorCB_t) (void *,L3INT,L3INT,L3INT);

typedef enum						/* Network/User Mode.                   */
{
	Q931_TE=0,						/*  0 : User Mode                       */
    Q931_NT=1						/*  1 : Network Mode                    */
} Q931NetUser_t;

typedef enum						/* Dialect enum                         */
{
	Q931_Dialect_Q931 = 0,

	Q931_Dialect_Count
} Q931Dialect_t;

typedef enum						/* Trunk Line Type.                     */
{
	Q931_TrType_E1=0,				/*  0 : E1 Trunk                        */
    Q931_TrType_T1=1,				/*  1 : T1 Trunk                        */
    Q931_TrType_J1=2,				/*  2 : J1 Trunk                        */
    Q931_TrType_BRI=3				/*  3 : BRI Trunk                       */
} Q931_TrunkType_t;

typedef enum						/* Trunk State							*/
{
	Q931_TrState_NoAlignment=0,		/* Trunk not aligned					*/
	Q931_TrState_Aligning=1,		/* Aligning in progress					*/
	Q931_TrState_Aligned=2			/* Trunk Aligned						*/
} Q931_TrunkState_t;

typedef enum {
	Q931_ChType_NotUsed=0,			/* Unused Channel						*/
	Q931_ChType_B=1,				/* B Channel (Voice)					*/		
	Q931_ChType_D=2,				/* D Channel (Signalling)				*/
	Q931_ChType_Sync=3				/* Sync Channel							*/
} Q931_ChanType_t;

struct Q931_TrunkInfo
{
	Q931NetUser_t NetUser;			/* Network/User Mode.                   */

    Q931Dialect_t Dialect;			/* Q.931 Based dielact index.           */

	Q931_TrunkType_t TrunkType;		/* Trunk Line Type.                     */

	Q931TxCB_t	Q931Tx34CBProc;
	Q931TxCB_t	Q931Tx32CBProc;
	Q931ErrorCB_t Q931ErrorCBProc;
	void *PrivateData32;
	void *PrivateData34;

	L3UCHAR     Enabled;            /* Enabled/Disabled                     */
                                    /*  0 = Disabled                        */
                                    /*  1 = Enabled                         */

	Q931_TrunkState_t TrunkState;

    L3INT       LastCRV;            /* Last used crv for the trunk.         */

    L3UCHAR L3Buf[Q931L4BUF];		/* message buffer for messages to be    */
                                    /* send from Q.931 L4.                  */

    L3UCHAR L2Buf[Q931L2BUF];		/* buffer for messages send to L2.      */

	/* The auto flags below switch on/off automatic Ack messages. SETUP ACK */
	/* as an example can be send by the stack in response to SETUP to buy   */
	/* time in processing on L4. Setting this to true will cause the stack  */
	/* to automatically send this.											*/

	L3BOOL	autoSetupAck;			/* Indicate if the stack should send    */
									/* SETUP ACK or not. 0=No, 1 = Yes.		*/

	L3BOOL  autoConnectAck;			/* Indicate if the stack should send    */
									/* CONNECT ACT or not. 0=No, 1=Yes.		*/

	L3BOOL  autoRestartAck;			/* Indicate if the stack should send    */
									/* RESTART ACK or not. 0=No, 1=Yes.		*/

    /* channel array holding info per channel. Usually defined to 32		*/
	/* channels to fit an E1 since T1/J1 and BRI will fit inside a E1.		*/
    struct _charray
    {
		Q931_ChanType_t ChanType;	/* Unused, B, D, Sync */

        L3UCHAR Available;          /* Channel Available Flag               */
                                    /*  0 : Avaiabled                       */
                                    /*  1 : Used                            */

        L3INT   CRV;                /* Associated CRV                       */

    }ch[Q931MAXCHPERTRUNK];

    /* Active Call information indentified by CRV. See Q931AllocateCRV for  */
	/* initialization of call table.										*/
    struct _ccarray
    {
        L3UCHAR InUse;              /* Indicate if entry is in use.         */
                                    /*  0 = Not in Use                      */
                                    /*  1 = Active Call.                    */

        L3UCHAR BChan;              /* Associated B Channel.                */
									/* 0 - 31 valid B chan					*/
									/* 255 = Not allocated					*/

        L3INT   CRV;                /* Associated CRV.                      */

        L3UINT  State;              /* Call State.                          */
									/*  0 is Idle, but other values are		*/
									/*  defined per dialect.				*/
									/*  Default usage is 1-99 for TE and    */
									/*  101 - 199 for NT.					*/
        
        L3ULONG Timer;              /* Timer in ms. The TimeTick will check */
									/* if this has exceeded the timeout, and*/
									/* if so call the timers timeout proc.	*/

        L3USHORT TimerID;           /* Timer Identification/State           */
									/* actual values defined by dialect		*/
                                    /*  0 : No timer running                */
                                    /*  ITU-T Q.931:301 - 322 Timer running */

    }call[Q931MAXCALLPERTRUNK];

};

/*****************************************************************************
  
  Struct:		Q931State

  Description:	Define a Q931 State, legal events and next state for each
				event. Used to simplify the state engine logic. Each state
				engine define it's own state table and the logic only need
				to call a helper function to check if the message is legal
				at this stage.

*****************************************************************************/
typedef struct
{
	L3INT		State;
	L3INT		Message;
	L3UCHAR		Direction;
}Q931State;

/*****************************************************************************

  Proc table external references. 

  The proc tables are defined in Q931.c and initialized in Q931Initialize.

*****************************************************************************/
extern L3INT (*Q931Proc  [Q931MAXDLCT][Q931MAXMES])   (Q931_TrunkInfo *pTrunk, L3UCHAR *,L3INT);

extern L3INT (*Q931Umes  [Q931MAXDLCT][Q931MAXMES])   (Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT IOff, L3INT Size);
extern L3INT (*Q931Pmes  [Q931MAXDLCT][Q931MAXMES])   (Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);

extern L3INT (*Q931Uie   [Q931MAXDLCT][Q931MAXIE] )   (Q931_TrunkInfo *pTrunk,ie *pIE,L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff);
extern L3INT (*Q931Pie   [Q931MAXDLCT][Q931MAXIE] )   (Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);

/*****************************************************************************
    
  Macro:        GetIETotoSize

  Syntax:       L3INT GetIETotSize(InfoElem ie);

  Description:  Compute the total size in bytes of an info element including 
                size of 'header'.    

*****************************************************************************/
#define Q931GetIETotSize(ie)    (((ie.InfoID & 0x80) !=0) ? 1:ie.LenIE)+2)

/*****************************************************************************

  Macro:        IsIEPresent

  Syntax:       BOOL IsIEPresent(ie InfoElement);

  Description:  Return TRUE if the Information Element is included.

*****************************************************************************/
#define Q931IsIEPresent(x)    ((x & 0x8000) != 0)

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
#define Q931GetIEValue(x) (x & 0x7fff)

/*****************************************************************************

  Macro:        Q931GetIEPtr

  Syntax:       void * Q931GetIEPtr(ie InfoElement, L3UCHAR * Buf);

  Description:  Compute a Ptr to the information element.

*****************************************************************************/
#define Q931GetIEPtr(ie,buf) (&buf[Q931GetIEOffset(ie)])

/*****************************************************************************

  Macro:        SetIE

  Syntax:       void SetIE(ie InfoElement, L3INT Offset);

  Description:  Set an information element.

*****************************************************************************/
#define Q931SetIE(x,o) {x = (ie)(o) | 0x8000;}

/*****************************************************************************

  Macro:        IsQ931Ext

  Syntax        BOOL IsQ931Ext(L3UCHAR c)

  Description:  Return true Check if the msb (bit 8) is 0. This indicate
                that the octet is extended.

*****************************************************************************/
#define IsQ931Ext(x) ((x&0x80)==0)

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
#define NoWarning(x) (x=x)

/*****************************************************************************

  External references. See Q931.c for details.

*****************************************************************************/

extern Q931_TrunkInfo Q931Trunk[Q931MAXTRUNKS];

#include "Q932.h"

/*****************************************************************************

  Q.931 Information Element Pack/Unpack functions. Implemented in Q931ie.c

*****************************************************************************/

L3INT Q931Pie_BearerCap(Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);
L3INT Q931Pie_ChanID(Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);
L3INT Q931Pie_ProgInd(Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);
L3INT Q931Pie_Display(Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);
L3INT Q931Pie_Signal(Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);
L3INT Q931Pie_HLComp(Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);
L3INT Q931Pie_Segment(Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);
L3INT Q931Pie_DateTime(Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);
L3INT Q931Pie_Cause(Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);
L3INT Q931Pie_SendComplete(Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);
L3INT Q931Pie_KeypadFac(Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);
L3INT Q931Pie_NotifInd(Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);
L3INT Q931Pie_CallID(Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);
L3INT Q931Pie_RepeatInd(Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);
L3INT Q931Pie_NetFac(Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);
L3INT Q931Pie_CallingNum(Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);
L3INT Q931Pie_CallingSub(Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);
L3INT Q931Pie_CalledNum(Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);
L3INT Q931Pie_CalledSub(Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);
L3INT Q931Pie_CalledNum(Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);
L3INT Q931Pie_TransNetSel(Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);
L3INT Q931Pie_LLComp(Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);
L3INT Q931Pie_CallState(Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);
L3INT Q931Pie_RestartInd(Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);
L3INT Q931Pie_UserUser(Q931_TrunkInfo *pTrunk,L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);

L3INT Q931Uie_BearerCap(Q931_TrunkInfo *pTrunk,ie *pIE, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff);
L3USHORT Q931Uie_CRV(Q931_TrunkInfo *pTrunk,L3UCHAR * IBuf, L3UCHAR *OBuf, L3INT *IOff, L3INT *OOff);
L3INT Q931Uie_ChanID(Q931_TrunkInfo *pTrunk,ie *pIE, L3UCHAR * IBuf, L3UCHAR *OBuf, L3INT *IOff, L3INT *OOff);
L3INT Q931Uie_ProgInd(Q931_TrunkInfo *pTrunk,ie *pIE, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff);
L3INT Q931Uie_Display(Q931_TrunkInfo *pTrunk,ie *pIE, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff);
L3INT Q931Uie_Signal(Q931_TrunkInfo *pTrunk,ie *pIE, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff);
L3INT Q931Uie_HLComp(Q931_TrunkInfo *pTrunk,ie *pIE, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff);
L3INT Q931Uie_Segment(Q931_TrunkInfo *pTrunk,ie *pIE, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff);
L3INT Q931Uie_DateTime(Q931_TrunkInfo *pTrunk,ie *pIE, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff);
L3INT Q931Uie_Cause(Q931_TrunkInfo *pTrunk,ie *pIE, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff);
L3INT Q931Uie_SendComplete(Q931_TrunkInfo *pTrunk,ie *pIE, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff);
L3INT Q931Uie_KeypadFac(Q931_TrunkInfo *pTrunk,ie *pIE, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff);
L3INT Q931Uie_NotifInd(Q931_TrunkInfo *pTrunk,ie *pIE, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff);
L3INT Q931Uie_CallID(Q931_TrunkInfo *pTrunk,ie *pIE, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff);
L3INT Q931Uie_RepeatInd(Q931_TrunkInfo *pTrunk,ie *pIE, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff);
L3INT Q931Uie_NetFac(Q931_TrunkInfo *pTrunk,ie *pIE, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff);
L3INT Q931Uie_CallingNum(Q931_TrunkInfo *pTrunk,ie *pIE, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff);
L3INT Q931Uie_CallingSub(Q931_TrunkInfo *pTrunk,ie *pIE, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff);
L3INT Q931Uie_CalledNum(Q931_TrunkInfo *pTrunk,ie *pIE, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff);
L3INT Q931Uie_CalledSub(Q931_TrunkInfo *pTrunk,ie *pIE, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff);
L3INT Q931Uie_TransNetSel(Q931_TrunkInfo *pTrunk,ie *pIE, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff);
L3INT Q931Uie_LLComp(Q931_TrunkInfo *pTrunk,ie *pIE, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff);
L3INT Q931Uie_CallState(Q931_TrunkInfo *pTrunk,ie *pIE, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff);
L3INT Q931Uie_RestartInd(Q931_TrunkInfo *pTrunk,ie *pIE, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff);
L3INT Q931Uie_UserUser(Q931_TrunkInfo *pTrunk,ie *pIE, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff);

/*****************************************************************************

  Q.931 Message Pack/Unpack functions. Implemented in Q931mes.c

*****************************************************************************/
L3INT Q931Pmes_Alerting(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_CallProceeding(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_Connect(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_ConnectAck(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_Progress(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_Setup(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_SetupAck(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_Resume(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_ResumeAck(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_ResumeReject(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_Suspend(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_SuspendAck(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_SuspendReject(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_UserInformation(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_Disconnect(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_Release(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_ReleaseComplete(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_Restart(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_RestartAck(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_CongestionControl(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_Information(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_Notify(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_Segment(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_Status(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);
L3INT Q931Pmes_StatusEnquiry(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize);

L3INT Q931Umes_Alerting(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR * OBuf, L3INT I, L3INT O);
L3INT Q931Umes_CallProceeding(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR * OBuf, L3INT I, L3INT O);
L3INT Q931Umes_Connect(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR * OBuf, L3INT I, L3INT O);
L3INT Q931Umes_ConnectAck(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR * OBuf, L3INT I, L3INT O);
L3INT Q931Umes_Progress(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR * OBuf, L3INT I, L3INT O);
L3INT Q931Umes_Setup(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR * OBuf, L3INT I, L3INT O);
L3INT Q931Umes_SetupAck(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR * OBuf, L3INT I, L3INT O);
L3INT Q931Umes_Resume(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR * OBuf, L3INT I, L3INT O);
L3INT Q931Umes_ResumeAck(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR * OBuf, L3INT I, L3INT O);
L3INT Q931Umes_ResumeReject(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR * OBuf, L3INT I, L3INT O);
L3INT Q931Umes_Suspend(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR * OBuf, L3INT I, L3INT O);
L3INT Q931Umes_SuspendAck(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR * OBuf, L3INT I, L3INT O);
L3INT Q931Umes_SuspendReject(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR * OBuf, L3INT I, L3INT O);
L3INT Q931Umes_UserInformation(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR * OBuf, L3INT I, L3INT O);
L3INT Q931Umes_Disconnect(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR * OBuf, L3INT I, L3INT O);
L3INT Q931Umes_Release(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR * OBuf, L3INT I, L3INT O);
L3INT Q931Umes_ReleaseComplete(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR * OBuf, L3INT I, L3INT O);
L3INT Q931Umes_Restart(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR * OBuf, L3INT I, L3INT O);
L3INT Q931Umes_RestartAck(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR * OBuf, L3INT I, L3INT O);
L3INT Q931Umes_CongestionControl(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR * OBuf, L3INT I, L3INT O);
L3INT Q931Umes_Information(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR * OBuf, L3INT I, L3INT O);
L3INT Q931Umes_Notify(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR * OBuf, L3INT I, L3INT O);
L3INT Q931Umes_Segment(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR * OBuf, L3INT I, L3INT O);
L3INT Q931Umes_Status(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR * OBuf, L3INT I, L3INT O);
L3INT Q931Umes_StatusEnquiry(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR * OBuf, L3INT I, L3INT O);

/*****************************************************************************

  Q.931 Process Function Prototyping. Implemented in Q931StateTE.c

*****************************************************************************/
L3INT Q931ProcAlertingTE(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcCallProceedingTE(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcConnectTE(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcConnectAckTE(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcProgressTE(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcSetupTE(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcSetupAckTE(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcResumeTE(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcResumeAckTE(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcResumeRejectTE(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcSuspendTE(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcSuspendAckTE(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcSuspendRejectTE(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcUserInformationTE(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcDisconnectTE(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcReleaseTE(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcReleaseCompleteTE(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcRestartTE(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcRestartAckTE(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcCongestionControlTE(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcInformationTE(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcNotifyTE(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcStatusTE(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcStatusEnquiryTE(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcSegmentTE(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);

L3INT Q931ProcAlertingNT(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcCallProceedingNT(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcConnectNT(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcConnectAckNT(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcProgressNT(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcSetupNT(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcSetupAckNT(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcResumeNT(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcResumeAckNT(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcResumeRejectNT(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcSuspendNT(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcSuspendAckNT(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcSuspendRejectNT(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcUserInformationNT(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcDisconnectNT(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcReleaseNT(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcReleaseCompleteNT(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcRestartNT(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcRestartAckNT(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcCongestionControlNT(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcInformationNT(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcNotifyNT(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcStatusNT(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcStatusEnquiryNT(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcSegmentNT(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);

L3INT Q931ProcUnknownMessage(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);
L3INT Q931ProcUnexpectedMessage(Q931_TrunkInfo *pTrunk,L3UCHAR * b, L3INT iFrom);

/*****************************************************************************

  Interface Function Prototypes. Implemented in Q931.c

*****************************************************************************/
void    Q931TimeTick(Q931_TrunkInfo *pTrunk, L3ULONG ms);
L3INT   Q931Rx23(Q931_TrunkInfo *pTrunk, L3UCHAR * Mes, L3INT Size);
L3INT   Q931Tx32(Q931_TrunkInfo *pTrunk, L3UCHAR * Mes, L3INT Size);
L3INT   Q931Rx43(Q931_TrunkInfo *pTrunk, L3UCHAR * Mes, L3INT Size);
L3INT   Q931Tx34(Q931_TrunkInfo *pTrunk, L3UCHAR * Mes, L3INT Size);
void    Q931SetError(Q931_TrunkInfo *pTrunk,L3INT ErrID, L3INT ErrPar1, L3INT ErrPar2);

void	Q931SetDefaultErrorCB(Q931ErrorCB_t Q931ErrorPar);

void    Q931CreateTE(L3UCHAR i);
void    Q931CreateNT(L3UCHAR i);
void    Q931SetMesCreateCB(L3INT (*callback)());
void    Q931SetDialectCreateCB(L3INT (*callback)(L3INT));
void    Q931SetHeaderSpace(L3INT space);
void Q931SetMesProc(L3UCHAR mes, L3UCHAR dialect, 
                L3INT (*Q931ProcFunc)(Q931_TrunkInfo *pTrunk, L3UCHAR * b, L3INT iFrom),
                L3INT (*Q931UmesFunc)(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT IOff, L3INT Size),
                L3INT (*Q931PmesFunc)(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3INT ISize, L3UCHAR *OBuf, L3INT *OSize));

void Q931SetIEProc(L3UCHAR iec, L3UCHAR dialect, 
			   L3INT (*Q931PieProc)(Q931_TrunkInfo *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet),
			   L3INT (*Q931UieProc)(Q931_TrunkInfo *pTrunk, ie *pIE, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff)); 

void Q931Initialize();
void Q931AddDialect(L3UCHAR iDialect, void (*Q931CreateDialectCB)(L3UCHAR iDialect));
L3INT Q931InitMesSetup(Q931mes_Setup *p);

L3INT	Q931CreateCRV(Q931_TrunkInfo *pTrunk, L3INT * callIndex);
L3INT	Q931AllocateCRV(Q931_TrunkInfo *pTrunk, L3INT iCRV, L3INT * callIndex);
L3INT   Q931FindCRV(Q931_TrunkInfo *pTrunk, L3INT crv, L3INT *callindex);
L3INT	Q931GetCallState(Q931_TrunkInfo *pTrunk, L3INT iCRV);
L3INT	Q931StartTimer(Q931_TrunkInfo *pTrunk, L3INT callIndex, L3USHORT iTimer);
L3INT	Q931StopTimer(Q931_TrunkInfo *pTrunk, L3INT callindex, L3USHORT iTimer);
L3INT	Q931SetState(Q931_TrunkInfo *pTrunk, L3INT callIndex, L3INT iState);
L3ULONG Q931GetTime();
void	Q931AddStateEntry(L3UCHAR iD, L3INT iState, L3INT iMes, L3UCHAR cDir);
L3BOOL	Q931IsEventLegal(L3UCHAR iD, L3INT iState, L3INT iMes, L3UCHAR cDir);

/*****************************************************************************

  Q.931 Low Level API Function Prototyping. Implemented in Q931API.c

*****************************************************************************/
ie Q931AppendIE(L3UCHAR *pm, L3UCHAR *pi);
L3INT Q931GetUniqueCRV(Q931_TrunkInfo *pTrunk);

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

L3INT Q931Disconnect(Q931_TrunkInfo *pTrunk, L3INT iTo, L3INT iCRV, L3INT iCause);
L3INT Q931ReleaseComplete(Q931_TrunkInfo *pTrunk, L3INT iTo);

L3INT Q931Api_InitTrunk(Q931_TrunkInfo *pTrunk,
						Q931Dialect_t Dialect,
						Q931NetUser_t NetUser,
						Q931_TrunkType_t TrunkType,
						Q931TxCB_t Q931Tx34CBProc,
						Q931TxCB_t Q931Tx32CBProc,
						Q931ErrorCB_t Q931ErrorCBProc,
						void *PrivateData32,
						void *PrivateData34);

#endif /* _Q931_NL */
