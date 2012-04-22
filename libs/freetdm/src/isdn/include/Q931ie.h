/******************************************************************************

  FileName:         Q931ie.h

  Contents:         Header and definition for the ITU-T Q.931 ie
					structures and functions

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

#ifndef _Q931IE_NL
#define _Q931IE_NL

/* Codesets */

typedef enum {

	Q931_CODESET_0			= ( 0 ),
	Q931_CODESET_1			= ( 1 << 8 ),
	Q931_CODESET_2			= ( 2 << 8 ),
	Q931_CODESET_3			= ( 3 << 8 ),
	Q931_CODESET_4			= ( 4 << 8 ),
	Q931_CODESET_5			= ( 5 << 8 ),
	Q931_CODESET_6			= ( 6 << 8 ),
	Q931_CODESET_7			= ( 7 << 8 )

} q931_codeset_t;

/* Single octet information elements */
#define Q931ie_SHIFT                            0x90 /* 1001 ----       */
#define Q931ie_MORE_DATA                        0xa0 /* 1010 ----       */
#define Q931ie_SENDING_COMPLETE                 0xa1 /* 1010 0001       */
#define Q931ie_CONGESTION_LEVEL                 0xb0 /* 1011 ----       */
#define Q931ie_REPEAT_INDICATOR                 0xd0 /* 1101 ----       */

/* Variable Length Information Elements */
#define Q931ie_SEGMENTED_MESSAGE                0x00 /* 0000 0000       */
#define Q931ie_CHANGE_STATUS                    0x01 /* 0000 0001       */
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

/* Variable Length Codeset 6 Information Elements */
#define Q931ie_GENERIC_DIGITS                   0x37 /* 0011 0111       */

/* Variable Length Information Element to shut up BRI testing */
#define Q931ie_CONNECTED_NUMBER                 0x4c /* 0100 1101 */
#define Q931ie_FACILITY                         0x1c /* 0001 1101 */


/*****************************************************************************

  Struct:        Q931ie_BearerCap

  Description:   Bearer Capability Information Element.

*****************************************************************************/

typedef struct {

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

} Q931ie_BearerCap;

/*****************************************************************************

  Struct:       Q931ie_CallID


*****************************************************************************/

typedef struct {

    L3UCHAR IEId;                   /* 00010000                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR CallId[1];				/* Call identity                        */

} Q931ie_CallID;

/*****************************************************************************

  Struct:       Q931ie_CallState


*****************************************************************************/

typedef struct {

    L3UCHAR IEId;                   /* 00010100                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR CodStand;               /* Coding Standard                      */
    L3UCHAR CallState;              /* Call State Value                     */

} Q931ie_CallState;

/*****************************************************************************

  Struct:		Q931ie_Cause

  Description:	Cause IE as described in Q.850

*****************************************************************************/
typedef struct {

    L3UCHAR IEId;                   /* 00010100                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR CodStand;               /* Coding Standard                      */
	L3UCHAR Location;				/* Location								*/
	L3UCHAR Recom;					/* Recommendation						*/
	L3UCHAR Value;					/* Cause Value							*/
	L3UCHAR	Diag[1];				/* Optional Diagnostics Field			*/

} Q931ie_Cause;

/*****************************************************************************

  Struct:        Q931ie_CalledNum

*****************************************************************************/
typedef struct {

    L3UCHAR IEId;                   /* 01110000                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR TypNum;                 /* Type of Number                       */
    L3UCHAR NumPlanID;              /* Numbering plan identification        */
    L3UCHAR Digit[1];				/* Digit (IA5)                          */

} Q931ie_CalledNum;

/*****************************************************************************

  Struct:       Q931ie_CalledSub

  Description:  Called party subaddress

*****************************************************************************/

typedef struct {

    L3UCHAR IEId;                   /* 01110001                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR TypNum;                 /* Type of subaddress                   */
    L3UCHAR OddEvenInd;             /* Odd/Even indicator                   */
    L3UCHAR Digit[1];				/* digits                               */

} Q931ie_CalledSub;

/*****************************************************************************

  Struct:       Q931ie_CallingNum

  Description:  Calling party number

*****************************************************************************/

typedef struct {

    L3UCHAR IEId;                   /* 01101100                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR TypNum;                 /* Type of number                       */
    L3UCHAR NumPlanID;              /* Numbering plan identification        */
    L3UCHAR PresInd;                /* Presentation indicator               */
    L3UCHAR ScreenInd;              /* Screening indicator                  */
    L3UCHAR Digit[1];				/* Number digits (IA5)                  */

} Q931ie_CallingNum;

/*****************************************************************************

  Struct:        Q931ie_CallingSub

  Description:   Calling party subaddress

*****************************************************************************/

typedef struct {

    L3UCHAR IEId;                   /* 01101101                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR TypNum;                 /* Type of subaddress                   */
    L3UCHAR OddEvenInd;             /* Odd/Even indicator                   */
    L3UCHAR Digit[1];				/* digits                               */

} Q931ie_CallingSub;

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

typedef struct {

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

} Q931ie_ChanID;

/*****************************************************************************

  Struct:       Q931ie_DateTime

  Description:  Date/time

*****************************************************************************/

typedef struct {

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
} Q931ie_DateTime;

/*****************************************************************************

  Struct:       Q931ie_Display

  Description:  Display

*****************************************************************************/

typedef struct {

    L3UCHAR IEId;                   /* 00101000                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR Display[1];             /* Display information (IA5)            */

} Q931ie_Display;

/*****************************************************************************

  Struct:       Q931ie_HLComp

  Description:  High layer compatibility

*****************************************************************************/

typedef struct {

    L3UCHAR IEId;                   /* 01111101                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR CodStand;               /* Coding standard                      */
    L3UCHAR Interpret;              /* Interpretation                       */
    L3UCHAR PresMeth;               /* Presentation methor of prot. profile */
    L3UCHAR HLCharID;               /* High layer characteristics id.       */
    L3UCHAR EHLCharID;              /* Extended high layer character. id.   */
    L3UCHAR EVideoTlfCharID;        /* Ext. videotelephony char. id.        */

} Q931ie_HLComp;

/*****************************************************************************

  Struct:       Q931ie_KeypadFac

  Description:  Keypad facility

*****************************************************************************/

typedef struct {
    L3UCHAR IEId;                   /* 00101100                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR KeypadFac[1];           /* dynamic buffer                       */

} Q931ie_KeypadFac;

/*****************************************************************************

  Struct:       Q931ie_LLComp

  Description:  Low layer compatibility

*****************************************************************************/

typedef struct {

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

} Q931ie_LLComp;

/*****************************************************************************

  Struct:       Q931ie_NetFac;

  Description:  Network-specific facilities

*****************************************************************************/

typedef struct {

    L3UCHAR IEId;                   /* 00100000                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR LenNetID;               /* Length of network facilities id.     */
    L3UCHAR TypeNetID;              /* Type of network identification       */
    L3UCHAR NetIDPlan;              /* Network identification plan.         */
    L3UCHAR NetFac;                 /* Network specific facility spec.      */
    L3UCHAR NetID[1];               /* Network id. (IA5)                    */

} Q931ie_NetFac;

/*****************************************************************************

  Struct:       Q931ie_NotifInd;

  Description:  Notification Indicator

*****************************************************************************/

typedef struct {

    L3UCHAR IEId;                   /* 00100000                             */
    L3UCHAR Size;                   /* Length of Information Element        */
	L3UCHAR Notification;			/* Notification descriptor				*/

} Q931ie_NotifInd;

/*****************************************************************************

  Struct:       Q931ie_ProgInd

  Description:  Progress indicator

*****************************************************************************/

typedef struct {

    L3UCHAR IEId;                   /* 00011110                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR CodStand;               /* Coding standard                      */
    L3UCHAR Location;               /* Location                             */
    L3UCHAR ProgDesc;               /* Progress description                 */

} Q931ie_ProgInd;

/*****************************************************************************

  Struct;       Q931ie_Segment

  Description:  Segmented message

*****************************************************************************/

typedef struct {

    L3UCHAR IEId;                   /* 00000000                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR FSI;                    /* First segment indicator              */
    L3UCHAR NumSegRem;              /* Number of segments remaining         */
    L3UCHAR SegType;                /* Segment message type                 */

} Q931ie_Segment;


typedef struct {

    L3UCHAR IEId;                   /* 00000000                             */
    L3UCHAR Size;                   /* Length of Information Element        */

} Q931ie_SendComplete;

/*****************************************************************************

  Struct:       Q931ie_Signal

  Description:  Signal

*****************************************************************************/

typedef struct {

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
} Q931ie_Signal;

/*****************************************************************************

  Struct:       Q931ie_TransDelSelInd

  description:  Transit delay selection and indication

*****************************************************************************/
#ifdef Q931_X25_SUPPORT
typedef struct {

    L3UCHAR IEId;                   /* 00000000                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3ULONG TxDSIValue;             /* Trans. delay sel. & ind. value       */ 

} Q931ie_TransDelSelInd;
#endif

/*****************************************************************************

  Struct:       Q931ie_TransNetSel

  Description:  Transit network selection

*****************************************************************************/

typedef struct {

    L3UCHAR IEId;                   /* 01111000                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR Type;                   /* Type of network identifier           */
    L3UCHAR NetIDPlan;              /* Network idetification plan           */
    L3UCHAR NetID[1];               /* Network identification(IA5)          */

} Q931ie_TransNetSel;

/*****************************************************************************

  Struct:       Q931ie_UserUser

  Description:  User-user

*****************************************************************************/

typedef struct {

    L3UCHAR IEId;                   /* 01111110                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR ProtDisc;               /* Protocol discriminator               */
    L3UCHAR User[1];                /* User information                     */

} Q931ie_UserUser;

/*****************************************************************************

  Struct:       Q931ie_ClosedUserGrp

  Description:  Closed user group

*****************************************************************************/
#ifdef Q931_X25_SUPPORT
typedef struct {

    L3UCHAR IEId;                   /* 01000111                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR CUGInd;                 /* CUG indication                       */
    L3UCHAR CUG[1];                 /* CUG index code (IA5)                 */

} Q931ie_ClosedUserGrp;
#endif

/*****************************************************************************

  Struct:		Q931ie_CongLevel

  Description:	Congestion Level

*****************************************************************************/
typedef struct {

    L3UCHAR IEId;                   /* 01000111                             */
    L3UCHAR Size;                   /* Length of Information Element        */
	L3UCHAR CongLevel;				/* Conguestion Level					*/

} Q931ie_CongLevel;

/*****************************************************************************

  Struct:       Q931ie_EndEndTxDelay

  Description:  End to end transit delay

*****************************************************************************/
#ifdef Q931_X25_SUPPORT
typedef struct {

    L3UCHAR IEId;                   /* 01000010                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3ULONG CumTxDelay;             /* Cumulative transit delay value       */
    L3ULONG ReqTxDelay;             /* Requested end to end transit delay   */
    L3ULONG MaxTxDelay;             /* Maximum transit delay                */

} Q931ie_EndEndTxDelay;
#endif

/*****************************************************************************

  Struct:       Q931ie_InfoRate

  Description:  Information Rate

*****************************************************************************/
#ifdef Q931_X25_SUPPORT
typedef struct {

    L3UCHAR IEId;                   /* 01100000                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR InInfoRate;             /* Incoming information rate            */
    L3UCHAR OutInfoRate;            /* Outgoing information rate            */
    L3UCHAR MinInInfoRate;          /* Minimum incoming information rate    */
    L3UCHAR MinOutInfoRate;         /* Minimum outgoing information rate    */

} Q931ie_InfoRate;
#endif

/*****************************************************************************

  Struct:       Q931ie_PackParam

  Description:  Packed layer binary parameters

*****************************************************************************/
#ifdef Q931_X25_SUPPORT
typedef struct {

    L3UCHAR IEId;                   /* 01000100                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR FastSel;                /* Fast selected                        */
    L3UCHAR ExpData;                /* Exp. data                            */
    L3UCHAR DelConf;                /* Delivery conf                        */
    L3UCHAR Modulus;                /* Modulus                              */

} Q931ie_PackParam;
#endif

/*****************************************************************************

  Struct:       Q931ie_PackWinSize

  Description:  Packed window size

*****************************************************************************/
#ifdef Q931_X25_SUPPORT
typedef struct {

    L3UCHAR IEId;                   /* 01000101                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR ForwardValue;           /* Forward value                        */
    L3UCHAR BackwardValue;          /* Backward value                       */

} Q931ie_PackWinSize;
#endif

/*****************************************************************************

  Struct:       Q931ie_PackSize

  Description:  Packet size

*****************************************************************************/
#ifdef Q931_X25_SUPPORT
typedef struct {

    L3UCHAR IEId;                   /* 01000110                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR ForwardValue;           /* Forward value                        */
    L3UCHAR BackwardValue;          /* Backward value                       */

} Q931ie_PackSize;
#endif

/*****************************************************************************

  Struct:       Q931ie_RedirNum

  Description:  Redirecting number

*****************************************************************************/
#ifdef Q931_X25_SUPPORT
typedef struct {

    L3UCHAR IEId;                   /* 01110100                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR TypeNum;                /* Type of number                       */
    L3UCHAR NumPlanID;              /* Number plan identification           */
    L3UCHAR PresInd;                /* Presentation indicator               */
    L3UCHAR ScreenInd;              /* Screening indicator                  */
    L3UCHAR Reason;                 /* Reason for redirection               */
    L3UCHAR Digit[1];               /* Number digits (IA5)                  */

} Q931ie_RedirNum;
#endif

typedef struct {

    L3UCHAR IEId;                   /* 01110100                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR RepeatInd;              /* 0010 Prioritized list for selecting  */
                                    /* one possible.                        */
} Q931ie_RepeatInd;

typedef struct {

    L3UCHAR IEId;                   /* 01110100                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR Spare;                  /* Spare                                */
    L3UCHAR Class;                  /* Class                                */
                                    /*  000 Indicate channels               */
                                    /*  110 Single interface                */
                                    /*  111 All interfaces                  */
} Q931ie_RestartInd;

typedef struct {

    L3UCHAR IEId;                   /* 01110100                             */
    L3UCHAR Size;                   /* Length of Information Element        */
	L3UCHAR Preference;             /* Preference 0 = reserved, 1 = channel */
	L3UCHAR Spare;                  /* Spare                                */
    L3UCHAR NewStatus;              /* NewStatus                            */
                                    /*  000 In service                      */
                                    /*  001 Maintenance                     */
                                    /*  010 Out of service                  */
} Q931ie_ChangeStatus;

/*****************************************************************************

  Struct:       Q931ie_GenericDigits


*****************************************************************************/

typedef struct {

    L3UCHAR IEId;                   /* 00110111                             */
    L3UCHAR Size;                   /* Length of Information Element        */
    L3UCHAR Type;					/* Type of number                       */
    L3UCHAR Encoding;				/* Encoding of number                   */
    L3UCHAR Digit[1];				/* Number digits (IA5)                  */

} Q931ie_GenericDigits;


/*****************************************************************************

  Q.931 Information Element Pack/Unpack functions. Implemented in Q931ie.c

*****************************************************************************/
q931pie_func_t Q931Pie_ChangeStatus;
q931pie_func_t Q931Pie_BearerCap;
q931pie_func_t Q931Pie_ChanID;
q931pie_func_t Q931Pie_ProgInd;
q931pie_func_t Q931Pie_Display;
q931pie_func_t Q931Pie_Signal;
q931pie_func_t Q931Pie_HLComp;
q931pie_func_t Q931Pie_Segment;
q931pie_func_t Q931Pie_DateTime;
q931pie_func_t Q931Pie_Cause;
q931pie_func_t Q931Pie_SendComplete;
q931pie_func_t Q931Pie_KeypadFac;
q931pie_func_t Q931Pie_NotifInd;
q931pie_func_t Q931Pie_CallID;
q931pie_func_t Q931Pie_RepeatInd;
q931pie_func_t Q931Pie_NetFac;
q931pie_func_t Q931Pie_CallingNum;
q931pie_func_t Q931Pie_CallingSub;
q931pie_func_t Q931Pie_CalledNum;
q931pie_func_t Q931Pie_CalledSub;
q931pie_func_t Q931Pie_CalledNum;
q931pie_func_t Q931Pie_TransNetSel;
q931pie_func_t Q931Pie_LLComp;
q931pie_func_t Q931Pie_CallState;
q931pie_func_t Q931Pie_RestartInd;
q931pie_func_t Q931Pie_UserUser;

q931pie_func_t Q931Pie_GenericDigits;

L3USHORT Q931Uie_CRV(Q931_TrunkInfo_t *pTrunk,L3UCHAR * IBuf, L3UCHAR *OBuf, L3INT *IOff, L3INT *OOff);

q931uie_func_t Q931Uie_ChangeStatus;
q931uie_func_t Q931Uie_BearerCap;
q931uie_func_t Q931Uie_ChanID;
q931uie_func_t Q931Uie_ProgInd;
q931uie_func_t Q931Uie_Display;
q931uie_func_t Q931Uie_Signal;
q931uie_func_t Q931Uie_HLComp;
q931uie_func_t Q931Uie_Segment;
q931uie_func_t Q931Uie_DateTime;
q931uie_func_t Q931Uie_Cause;
q931uie_func_t Q931Uie_SendComplete;
q931uie_func_t Q931Uie_KeypadFac;
q931uie_func_t Q931Uie_NotifInd;
q931uie_func_t Q931Uie_CallID;
q931uie_func_t Q931Uie_RepeatInd;
q931uie_func_t Q931Uie_NetFac;
q931uie_func_t Q931Uie_CallingNum;
q931uie_func_t Q931Uie_CallingSub;
q931uie_func_t Q931Uie_CalledNum;
q931uie_func_t Q931Uie_CalledSub;
q931uie_func_t Q931Uie_TransNetSel;
q931uie_func_t Q931Uie_LLComp;
q931uie_func_t Q931Uie_CallState;
q931uie_func_t Q931Uie_RestartInd;
q931uie_func_t Q931Uie_UserUser;

q931uie_func_t Q931Uie_GenericDigits;


L3INT Q931ReadExt(L3UCHAR * IBuf, L3INT Off);
L3INT Q931Uie_CongLevel(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff);
L3INT Q931Pie_CongLevel(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);
L3INT Q931Uie_RevChargeInd(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR * IBuf, L3UCHAR * OBuf, L3INT *IOff, L3INT *OOff);
L3INT Q931Pie_RevChargeInd(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);

L3INT Q931Uie_Generic(Q931_TrunkInfo_t *pTrunk, Q931mes_Generic *pMsg, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *IOff, L3INT *OOff);
L3INT Q931Pie_Generic(Q931_TrunkInfo_t *pTrunk, L3UCHAR *IBuf, L3UCHAR *OBuf, L3INT *Octet);

#endif /* _Q931IE_NL */
