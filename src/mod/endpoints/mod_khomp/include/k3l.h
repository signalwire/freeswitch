/*******************************************************************************

    KHOMP generic endpoint/channel library.
    Copyright (C) 2007-2010 Khomp Ind. & Com.

  The contents of this file are subject to the Mozilla Public License
  Version 1.1 (the "License"); you may not use this file except in compliance
  with the License. You may obtain a copy of the License at
  http://www.mozilla.org/MPL/

  Software distributed under the License is distributed on an "AS IS" basis,
  WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for
  the specific language governing rights and limitations under the License.

  Alternatively, the contents of this file may be used under the terms of the
  "GNU Lesser General Public License 2.1" license (the “LGPL" License), in which
  case the provisions of "LGPL License" are applicable instead of those above.

  If you wish to allow use of your version of this file only under the terms of
  the LGPL License and not to allow others to use your version of this file
  under the MPL, indicate your decision by deleting the provisions above and
  replace them with the notice and other provisions required by the LGPL
  License. If you do not delete the provisions above, a recipient may use your
  version of this file under either the MPL or the LGPL License.

  The LGPL header follows below:

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; if not, write to the Free Software Foundation,
    Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*******************************************************************************/

#ifndef _GENERATED_K3L_H_
#define _GENERATED_K3L_H_
#if !defined KLTYPEDEFS_H
#define KLTYPEDEFS_H
#if defined( _WINDOWS ) || defined( _Windows ) || defined( _WIN32 )
	#ifndef KWIN32
	#define KWIN32 1
	#endif
#endif

#ifdef KWIN32
	typedef __int64				int64;
	typedef unsigned __int64	uint64;
	#define Kstdcall __stdcall
#else
	typedef long long				int64;
	typedef unsigned long long		uint64;
	#define Kstdcall 
#endif
typedef int					int32;
typedef unsigned int		uint32;
typedef uint64			    intptr;
typedef intptr              stackint;
typedef short int			int16;
typedef unsigned short int	uint16;
typedef char				int8;
typedef unsigned char		uint8;
typedef unsigned char		byte;
typedef char				sbyte;
typedef double				float64;
typedef float				float32;
typedef int32				stt_code;

enum KLibraryStatus 
{
	ksSuccess =			0,
	ksFail =			1,
	ksTimeOut =			2,
	ksBusy =			3,
	ksLocked =			4,
	ksInvalidParams =	5,
	ksEndOfFile =		6,
	ksInvalidState =	7,
	ksServerCommFail =	8,
	ksOverflow =		9,
    ksUnderrun =        10,
	ksNotFound =		11,
    ksNotAvailable =    12
};	
enum KTxRx
{
	kNoTxRx	= 0x0,
	kTx		= 0x1,
	kRx		= 0x2,
	kBoth	= 0x3
}; 

#define KMAX_SERIAL_NUMBER			12 
#define KMAX_E1_CHANNELS			30
#define KMAX_DIAL_NUMBER	        20
#define KMAX_ADDRESS				60
#define KMAX_DSP_NAME		        8
#define KMAX_STR_VERSION	        80
#define KMAX_BUFFER_ADDRESSES		16
#define KMAX_LOG                    1024
#define KMAX_SIP_DATA               248
#define KMAX_GSM_CALLS              6
#define KMAX_GSM_OPER_NAME          32
#define KMAX_GSM_IMEI_SIZE          16
#endif
#if !defined KVOIPDEFS_H
#define KVOIPDEFS_H
enum KRejectReason
{
	UserBusy = 0,
	UserNotFound,
	NoAnswer,
	Decline,
	ServiceUnavailable,
	ServerInternalError,
	UnknownRejectReason
};
enum KSIP_Failures
{
	kveResponse_200_OK_Success             		= 200,
    kveRedirection_300_MultipleChoices			= 300,
	kveRedirection_301_MovedPermanently			= 301,
	kveRedirection_302_MovedTemporarily			= 302,
	kveRedirection_305_UseProxy					= 305,
	kveRedirection_380_AlternativeService		= 380,
	kveFailure_400_BadRequest					= 400,
	kveFailure_401_Unauthorized					= 401,
	kveFailure_402_PaymentRequired				= 402,
	kveFailure_403_Forbidden					= 403,
	kveFailure_404_NotFound						= 404,
	kveFailure_405_MethodNotAllowed				= 405,
	kveFailure_406_NotAcceptable				= 406,
	kveFailure_407_ProxyAuthenticationRequired	= 407,
	kveFailure_408_RequestTimeout				= 408,
	kveFailure_410_Gone							= 410,
	kveFailure_413_RequestEntityTooLarge		= 413,
	kveFailure_414_RequestURI_TooLong			= 414,
	kveFailure_415_UnsupportedMediaType			= 415,
	kveFailure_416_UnsupportedURI_Scheme		= 416,
	kveFailure_420_BadExtension					= 420,
	kveFailure_421_ExtensionRequired			= 421,
	kveFailure_423_IntervalTooBrief				= 423,
	kveFailure_480_TemporarilyUnavailable		= 480,
	kveFailure_481_CallDoesNotExist				= 481,
	kveFailure_482_LoopDetected					= 482,
	kveFailure_483_TooManyHops					= 483,
	kveFailure_484_AddressIncomplete			= 484,
	kveFailure_485_Ambiguous					= 485,
	kveFailure_486_BusyHere						= 486,
	kveFailure_487_RequestTerminated			= 487,
	kveFailure_488_NotAcceptableHere			= 488,
	kveFailure_491_RequestPending				= 491,
	kveFailure_493_Undecipherable				= 493,
	kveServer_500_InternalError					= 500,
	kveServer_501_NotImplemented				= 501,
	kveServer_502_BadGateway					= 502,
	kveServer_503_ServiceUnavailable			= 503,
	kveServer_504_TimeOut						= 504,
	kveServer_505_VersionNotSupported			= 505,
	kveServer_513_MessageTooLarge				= 513,
	kveGlobalFailure_600_BusyEverywhere			= 600,
	kveGlobalFailure_603_Decline				= 603,
	kveGlobalFailure_604_DoesNotExistAnywhere	= 604,
	kveGlobalFailure_606_NotAcceptable			= 606
};
enum KVoIPRegTypes
{
   kvrtRegister    =   0,
   kvrtUnregister  =   1
};

struct KVoIPCallParams
{
	sbyte ToUser[ KMAX_ADDRESS + 1 ];
	sbyte FromUser[ KMAX_ADDRESS + 1 ];
    sbyte FromUserIP[ KMAX_ADDRESS + 1 ];
};
struct KVoIPEvRegisterParams
{
    KVoIPRegTypes Register;
	sbyte User[ KMAX_ADDRESS + 1 ];
	sbyte ProxyIP[ KMAX_ADDRESS + 1 ];
};
struct KVoIPSeize 
{
	sbyte FromUser[ KMAX_ADDRESS + 1 ];
	sbyte ToUser[ KMAX_ADDRESS + 1 ];
    sbyte ProxyIP[ KMAX_ADDRESS + 1 ];
};
#endif
#if !defined KLDEFS_H
#define KLDEFS_H



#define CM_SEIZE					0x01
     
#define CM_SYNC_SEIZE				0x02

#define CM_SIP_REGISTER             0x03

#define CM_DIAL_DTMF				0x04

#define CM_DISCONNECT				0x05

#define CM_CONNECT					0x06

#define CM_PRE_CONNECT				0x07

#define CM_CAS_CHANGE_LINE_STT		0x08

#define CM_CAS_SEND_MFC				0x09

#define CM_SET_FORWARD_CHANNEL		0x0A

#define CM_CAS_SET_MFC_DETECT_MODE	0x0B

#define CM_DROP_COLLECT_CALL		0x0C

#define CM_MAKE_CALL				0x0D

#define CM_RINGBACK 				0x0E

#define CM_USER_INFORMATION         0x0F

#define CM_USER_INFORMATION_EX      0x2B

#define CM_VOIP_SEIZE				0x23


#define	CM_LOCK_INCOMING			0x10

#define CM_UNLOCK_INCOMING			0x11

#define CM_LOCK_OUTGOING			0x12

#define CM_UNLOCK_OUTGOING			0x13

#define CM_START_SEND_FAIL			0x14

#define CM_STOP_SEND_FAIL			0x15

#define CM_END_OF_NUMBER            0x16

#define CM_SEND_SIP_DATA			0x17

#define CM_SS_TRANSFER              0x18

#define CM_GET_SMS                  0x19

#define CM_PREPARE_SMS              0x1A

#define CM_SEND_SMS                 0x1B

#define CM_SEND_TO_MODEM            0x1C

#define CM_CHECK_NEW_SMS            0x1D

#define CM_ISDN_SEND_SUBADDRESSES   0x1E

#define CM_CT_TRANSFER              0x1F

#define CM_ENABLE_DTMF_SUPPRESSION	0x30

#define CM_DISABLE_DTMF_SUPPRESSION	0x31

#define CM_ENABLE_AUDIO_EVENTS		0x32

#define CM_DISABLE_AUDIO_EVENTS		0x33

#define CM_ENABLE_CALL_PROGRESS		0x34

#define CM_DISABLE_CALL_PROGRESS	0x35

#define CM_FLASH					0x36

#define CM_ENABLE_PULSE_DETECTION	0x37

#define CM_DISABLE_PULSE_DETECTION	0x38

#define CM_ENABLE_ECHO_CANCELLER	0x39

#define CM_DISABLE_ECHO_CANCELLER	0x3A

#define CM_ENABLE_AGC				0x3B

#define CM_DISABLE_AGC				0x3C

#define CM_ENABLE_HIGH_IMP_EVENTS	0x3D

#define CM_DISABLE_HIGH_IMP_EVENTS	0x3E

#define CM_ENABLE_CALL_ANSWER_INFO  0x40

#define CM_DISABLE_CALL_ANSWER_INFO 0x41

#define CM_START_WATCHDOG           0x45

#define CM_STOP_WATCHDOG            0x46

#define CM_NOTIFY_WATCHDOG          0x47

#define CM_WATCHDOG_COUNT           0x48

#define CM_HOLD_SWITCH             0x4A

#define CM_MPTY_CONF               0x4B

#define CM_MPTY_SPLIT              0x4C

#define CM_SIM_CARD_SELECT         0x4D

#define CM_START_FAX_TX				0x50

#define CM_STOP_FAX_TX			0x51

#define CM_ADD_FAX_FILE				0x52

#define CM_ADD_FAX_PAGE_BREAK		0x53

#define CM_START_FAX_RX				0x54

#define CM_STOP_FAX_RX			    0x55

#define CM_RESET_LINK				0xF1

#define CM_CLEAR_LINK_ERROR_COUNTER 0xF2

#define CM_SEND_DEVICE_SECURITY_KEY 0xF3

#define CM_RESET_MODEM              0xF4

#define CM_ISDN_DISABLE_LINK        0xF5

#define CM_ISDN_ENABLE_LINK         0xF6


#define EV_CHANNEL_FREE				0x01

#define EV_CONNECT					0x03

#define EV_DISCONNECT				0x04

#define EV_CALL_SUCCESS				0x05

#define EV_CALL_FAIL				0x06

#define EV_NO_ANSWER				0x07

#define EV_BILLING_PULSE			0x08

#define EV_SEIZE_SUCCESS			0x09

#define EV_SEIZE_FAIL				0x0A

#define EV_SEIZURE_START			0x0B

#define EV_CAS_LINE_STT_CHANGED		0x0C

#define EV_CAS_MFC_RECV				0x0D

#define EV_NEW_CALL                 0x0E

#define EV_USER_INFORMATION         0x0F

#define EV_USER_INFORMATION_EX      0x1D 

#define EV_DIALED_DIGIT             0x10

#define EV_SIP_REGISTER_INFO        0x11

#define EV_RING_DETECTED            0x12

#define EV_ISDN_SUBADDRESSES        0x13

#define EV_CALL_HOLD_START			0x16

#define EV_CALL_HOLD_STOP			0x17

#define EV_SS_TRANSFER_FAIL         0x18

#define EV_FLASH                    0x19

#define EV_ISDN_PROGRESS_INDICATOR	0x1A

#define EV_CT_TRANSFER_SUCCESS      0x1B

#define EV_CT_TRANSFER_FAIL         0x1C

#define EV_DTMF_DETECTED			0x20

#define EV_DTMF_SEND_FINISH			0x21

#define EV_AUDIO_STATUS				0x22

#define EV_CADENCE_RECOGNIZED		0x23
#define EV_CALL_PROGRESS			EV_CADENCE_RECOGNIZED

#define EV_END_OF_STREAM			0x24

#define EV_PULSE_DETECTED			0x25

#define EV_POLARITY_REVERSAL		0x26

#define EV_CALL_ANSWER_INFO     	0x27

#define EV_COLLECT_CALL          	0x28

#define EV_SIP_DTMF_DETECTED        0x29

#define EV_SIP_DATA			        0x2A

#define EV_RECV_FROM_MODEM			0x42

#define EV_NEW_SMS                  0x43

#define EV_SMS_INFO                 0x44

#define EV_SMS_DATA                 0x45

#define EV_SMS_SEND_RESULT          0x46

#define EV_CALL_MPTY_START			0x47

#define EV_CALL_MPTY_STOP			0x48

#define EV_GSM_COMMAND_STATUS		0x49

#define EV_WATCHDOG_COUNT           0x60

#define EV_CHANNEL_FAIL				0x30

#define EV_REFERENCE_FAIL			0x31

#define EV_INTERNAL_FAIL			0x32

#define EV_HARDWARE_FAIL			0x33

#define EV_LINK_STATUS				0x34

#define EV_PHYSICAL_LINK_UP			0x35

#define EV_PHYSICAL_LINK_DOWN		0x36

#define EV_CLIENT_RECONNECT		0xF0

#define EV_CLIENT_AUDIOLISTENER_TIMEOUT     0xF1

#define EV_CLIENT_BUFFERED_AUDIOLISTENER_OVERFLOW   0xF2

#define EV_REQUEST_DEVICE_SECURITY_KEY 0xF3

#define EV_DISK_IS_FULL		0xF4


#define CM_SEND_DTMF				0xD1

#define CM_STOP_AUDIO 				0xD2

#define CM_HARD_RESET				0xF0

#define EV_VOIP_SEIZURE					0x40

#define EV_SEIZURE					0x41

#define EV_FAX_CHANNEL_FREE			0x50

#define EV_FAX_FILE_SENT			0x51
#define EV_FAX_FILE_FAIL			0x52
#define EV_FAX_MESSAGE_CONFIRMATION 0x53
#define EV_FAX_TX_TIMEOUT			0x54
#define EV_FAX_PAGE_CONFIRMATION EV_FAX_MESSAGE_CONFIRMATION
#define EV_FAX_REMOTE_INFO			0x55

#endif

   #define FC_REMOTE_FAIL			0x01
   #define FC_LOCAL_FAIL			0x02
 
   #define FC_REMOTE_LOCK			0x03
  
   #define FC_LINE_SIGNAL_FAIL		0x04
   #define FC_ACOUSTIC_SIGNAL_FAIL	0x05

enum KChannelFail
{
	kfcRemoteFail = FC_REMOTE_FAIL,
	kfcLocalFail = FC_LOCAL_FAIL,
	kfcRemoteLock = FC_REMOTE_LOCK,
	kfcLineSignalFail = FC_LINE_SIGNAL_FAIL,
	kfcAcousticSignalFail = FC_ACOUSTIC_SIGNAL_FAIL
};

   #define ER_INTERRUPT_CTRL		0x01
   #define ER_COMMUNICATION_FAIL	0x02
   #define ER_PROTOCOL_FAIL			0x03
   #define ER_INTERNAL_BUFFER		0x04
	#define ER_MONITOR_BUFFER		0x05
	#define ER_INITIALIZATION		0x06
	#define ER_INTERFACE_FAIL		0x07
	#define ER_CLIENT_COMM_FAIL		0x08
	#define ER_POLL_CTRL			0x09
	#define ER_EVT_BUFFER_CTRL		0x0A

	#define ER_INVALID_CONFIG_VALUE 0x0B

	#define ER_INTERNAL_GENERIC_FAIL 0x0C

enum KInternalFail
{
	kifInterruptCtrl = ER_INTERRUPT_CTRL,
	kifCommunicationFail = ER_COMMUNICATION_FAIL,
	kifProtocolFail = ER_PROTOCOL_FAIL,
	kifInternalBuffer = ER_INTERNAL_BUFFER,
	kifMonitorBuffer = ER_MONITOR_BUFFER,
	kifInitialization = ER_INITIALIZATION,
	kifInterfaceFail = ER_INTERFACE_FAIL,
	kifClientCommFail = ER_CLIENT_COMM_FAIL
};	

   #define FS_CHANNEL_LOCKED		0x01
   #define FS_INCOMING_CHANNEL		0x02
   #define FS_CHANNEL_NOT_FREE		0x03
   #define FS_DOUBLE_SEIZE			0x04
   #define FS_LOCAL_CONGESTION		0x06
   #define FS_NO_DIAL_TONE			0x07
enum KSeizeFail
{
	ksfChannelLocked = FS_CHANNEL_LOCKED,
	ksfIncomingChannel = FS_INCOMING_CHANNEL,
	ksfChannelBusy = FS_CHANNEL_NOT_FREE,
	ksfDoubleSeizure = FS_DOUBLE_SEIZE,
	ksfCongestion = FS_LOCAL_CONGESTION,
	ksfNoDialTone = FS_NO_DIAL_TONE
};
#if !defined KH100DEFS_H
#define KH100DEFS_H

#define CM_SEND_TO_CTBUS			0x90

#define CM_RECV_FROM_CTBUS			0x91

#define CM_SEND_RANGE_TO_CTBUS		0x92

#define CM_SETUP_H100				0x93

enum KH100ConfigIndex
{
	khciDeviceMode = 0,
	khciMasterGenClock = 1,
	khciCTNetRefEnable = 4,
	khciSCbusEnable = 6,
	khciHMVipEnable = 7,
	khciMVip90Enable = 8,
	khciCTbusDataEnable = 9,
	khciCTbusFreq03_00 = 10,
	khciCTbusFreq07_04 = 11,
	khciCTbusFreq11_08 = 12,
	khciCTbusFreq15_12 = 13,
	khciMax = 14,
	khciMasterDevId = 20,
	khciSecMasterDevId = 21,
	khciCtNetrefDevId = 22,
    khciMaxH100ConfigIndex
};	

enum KMasterPLLClockReference
{
    h100_Ref_FreeRun = 0,
    h100_Ref_holdover = 1,
	h100_Automatic = 2,
    h100_Ref_ctnetref = 7,
    h100_Ref_link0 = 8,
    h100_Ref_link1 = 9
};
enum KSlavePLLClockReference
{
    h100_PllLoc_ClkA = 0,
    h100_PllLoc_ClkB = 1,
    h100_PllLoc_SCBus = 2,
    h100_PllLoc_MVIP90 = 3,
    h100_PllLoc_Link0 = 4,
    h100_PllLoc_Link1 = 5,
    h100_PllLoc_Error = 6
};

#define H100_DEVICE_MODE				khciDeviceMode
	enum KH100Mode
	{
		h100_Slave,
		h100_Master,
		h100_StandbyMaster,
		h100_Diagnostic,
		h100_NotConnected
	};
#define H100_MASTER_GEN_CLOCK			khciMasterGenClock
	enum KH100SelectCtbusClock
	{
		h100_scClockA,
		h100_scClockB
	};

#define H100_CT_NETREF_ENABLE			khciCTNetRefEnable
	enum KH100CtNetref
	{
		h100_nrOff,
		h100_nrEnable
	};
#define	H100_SCBUS_ENABLE				khciSCbusEnable
	enum KH100ScbusEnable
	{
		h100_seOff,
		h100_seOn2Mhz,
		h100_seOn4Mhz,
		h100_seOn8Mhz
	};
#define H100_HMVIP_ENABLE				khciHMVipEnable

#define H100_MVIP90_ENABLE				khciMVip90Enable

#define H100_CTBUS_DATA_ENABLE			khciCTbusDataEnable

enum KH100Enable
{
	h100_On = 0x01,
	h100_Off = 0x00
};
	
#define H100_CTBUS_FREQ_03_00			khciCTbusFreq03_00
#define H100_CTBUS_FREQ_07_04			khciCTbusFreq07_04
#define H100_CTBUS_FREQ_11_08			khciCTbusFreq11_08
#define H100_CTBUS_FREQ_15_12			khciCTbusFreq15_12

enum KH100CtbusFreq
{
	h100_cf_2Mhz	=	0,
	h100_cf_4Mhz	=	1,
	h100_cf_8Mhz	=	2 
};

#endif

#define CM_MIXER					0x60

#define CM_CLEAR_MIXER				0x61

#define CM_PLAY_FROM_FILE			0x62

#define CM_RECORD_TO_FILE			0x63
	
#define CM_PLAY_FROM_STREAM			0x64 

#define CM_INTERNAL_PLAY			0x65 

#define CM_STOP_PLAY				0x66

#define CM_STOP_RECORD				0x67

#define CM_PAUSE_PLAY				0x68

#define CM_PAUSE_RECORD				0x69

#define CM_RESUME_PLAY				0x6A

#define CM_RESUME_RECORD			0x6B

#define CM_INCREASE_VOLUME			0x6C

#define CM_DECREASE_VOLUME			0x6D

#define CM_LISTEN					0x6E

#define CM_STOP_LISTEN				0x6F

#define CM_PREPARE_FOR_LISTEN		0x70

#define CM_PLAY_SOUND_CARD			0x71

#define CM_STOP_SOUND_CARD			0x72

#define CM_MIXER_CTBUS				0x73

#define CM_PLAY_FROM_STREAM_EX			0x74 

#define CM_INTERNAL_PLAY_EX			0x75 

#define CM_ENABLE_PLAYER_AGC		0x76

#define CM_DISABLE_PLAYER_AGC		0x77

#define CM_START_STREAM_BUFFER		0x78

#define CM_ADD_STREAM_BUFFER		0x79

#define CM_STOP_STREAM_BUFFER		0x7A

#define CM_SEND_BEEP				0x7B

#define CM_SEND_BEEP_CONF			0x7C

#define CM_ADD_TO_CONF				0x7D

#define CM_REMOVE_FROM_CONF			0x7E

#define CM_RECORD_TO_FILE_EX		0x7F

#define CM_SET_VOLUME				0xA0

#define CM_START_CADENCE			0xA1

#define CM_STOP_CADENCE				0xA2

#define CM_SET_INPUT_MODE				0xA3
#if !defined KR2D_H
#define KR2D_H

#define CM_SET_LINE_CONDITION		0x80

#define CM_SEND_LINE_CONDITION		0x81

#define CM_SET_CALLER_CATEGORY		0x82
      
	
#define CM_DIAL_MFC					0x83



enum KSignGroupII_Brazil
{
	kg2BrOrdinary		        = 0x01,     
	kg2BrPriority		        = 0x02,     
	kg2BrMaintenance	        = 0x03,     
	kg2BrLocalPayPhone	        = 0x04,     
	kg2BrTrunkOperator	        = 0x05,     
	kg2BrDataTrans	            = 0x06,     
	kg2BrNonLocalPayPhone       = 0x07,     
	kg2BrCollectCall	        = 0x08,     
	kg2BrOrdinaryInter	        = 0x09,     
	kg2BrTransfered		        = 0x0B,     
};

enum KSignGroupB_Brazil
{
	kgbBrLineFreeCharged		= 0x01,     
	kgbBrBusy					= 0x02,     
	kgbBrNumberChanged			= 0x03,     
	kgbBrCongestion				= 0x04,     
	kgbBrLineFreeNotCharged		= 0x05,     
	kgbBrLineFreeChargedLPR		= 0x06,     
	kgbBrInvalidNumber			= 0x07,     
	kgbBrLineOutOfOrder			= 0x08,     
	kgbBrNone					= 0xFF      
};


enum KSignGroupII_Argentina
{
	kg2ArOrdinary		        = 0x01,     
	kg2ArPriority		        = 0x02,     
	kg2ArMaintenance	        = 0x03,     
	kg2ArLocalPayPhone	        = 0x04,     
	kg2ArTrunkOperator	        = 0x05,     
	kg2ArDataTrans	            = 0x06,     
    kg2ArCPTP                   = 0x0B,     
    kg2ArSpecialLine            = 0x0C,     
    kg2ArMobileUser             = 0x0D,     
    kg2ArPrivateRedLine         = 0x0E,     
    kg2ArSpecialPayPhoneLine    = 0x0F,     
};

enum KSignGroupB_Argentina
{
	kgbArNumberChanged			= 0x02,     
	kgbArBusy					= 0x03,     
	kgbArCongestion				= 0x04,     
	kgbArInvalidNumber			= 0x05,     
	kgbArLineFreeCharged		= 0x06,     
	kgbArLineFreeNotCharged		= 0x07,     
	kgbArLineOutOfOrder			= 0x08,     
	kgbArNone					= 0xFF      
};


enum KSignGroupII_Chile
{
    kg2ClOrdinary		        = 0x01,     
	kg2ClPriority		        = 0x02,     
	kg2ClMaintenance	        = 0x03,     
	kg2ClTrunkOperator	        = 0x05,     
	kg2ClDataTrans	            = 0x06,     
    kg2ClUnidentifiedSubscriber = 0x0B,     
};

enum KSignGroupB_Chile
{
	kgbClNumberChanged			= 0x02,     
	kgbClBusy					= 0x03,     
	kgbClCongestion				= 0x04,     
	kgbClInvalidNumber			= 0x05,     
	kgbClLineFreeCharged		= 0x06,     
	kgbClLineFreeNotCharged		= 0x07,     
	kgbClLineOutOfOrder			= 0x08,     
	kgbClNone					= 0xFF      
};


enum KSignGroupII_Mexico
{
	kg2MxTrunkOperator	        = 0x01,     
	kg2MxOrdinary		        = 0x02,     
	kg2MxMaintenance	        = 0x06,     
};

enum KSignGroupB_Mexico
{
	kgbMxLineFreeCharged		= 0x01,     
	kgbMxBusy					= 0x02,     
	kgbMxLineFreeNotCharged		= 0x05,     
	kgbMxNone				    = 0xFF      
};


enum KSignGroupII_Uruguay
{
	kg2UyOrdinary		        = 0x01,     
	kg2UyPriority		        = 0x02,     
	kg2UyMaintenance	        = 0x03,     
	kg2UyLocalPayPhone	        = 0x04,     
	kg2UyTrunkOperator	        = 0x05,     
	kg2UyDataTrans	            = 0x06,     
    kg2UyInternSubscriber       = 0x07,     
};

enum KSignGroupB_Uruguay
{
	kgbUyNumberChanged			= 0x02,     
	kgbUyBusy					= 0x03,     
	kgbUyCongestion				= 0x04,     
	kgbUyInvalidNumber			= 0x05,     
	kgbUyLineFreeCharged		= 0x06,     
	kgbUyLineFreeNotCharged		= 0x07,     
	kgbUyLineOutOfOrder			= 0x08,     
	kgbUyNone					= 0xFF      
};


enum KSignGroupII_Venezuela
{
	kg2VeOrdinary		        = 0x01,     
	kg2VePriority		        = 0x02,     
	kg2VeMaintenance	        = 0x03,     
	kg2VeLocalPayPhone	        = 0x04,     
	kg2VeTrunkOperator	        = 0x05,     
	kg2VeDataTrans	            = 0x06,     
    kg2VeNoTransferFacility     = 0x07,     
};

enum KSignGroupB_Venezuela
{
	kgbVeLineFreeChargedLPR		= 0x01,     
	kgbVeNumberChanged			= 0x02,     
	kgbVeBusy					= 0x03,     
	kgbVeCongestion				= 0x04,     
	kgbVeInformationTone		= 0x05,     
	kgbVeLineFreeCharged		= 0x06,     
	kgbVeLineFreeNotCharged		= 0x07,     
    kgbVeLineBlocked            = 0x08,     
    kgbVeIntercepted            = 0x09,     
    kgbVeDataTrans              = 0x0A,     
	kgbVeNone					= 0xFF      
};





enum KSignGroupB
{
	kgbLineFreeCharged			= 0x01,
	kgbLineFreeNotCharged		= 0x05,
	kgbLineFreeChargedLPR		= 0x06,
	kgbBusy						= 0x02,
	kgbNumberChanged			= 0x03,
	kgbCongestion				= 0x04,
	kgbInvalidNumber			= 0x07,
	kgbLineOutOfOrder			= 0x08,
	kgbNone						= 0xFF
};
#define STT_GB_LINEFREE_CHARGED				0x01
#define STT_GB_LINEFREE_NOTCHARGED			0x05
#define STT_GB_LINEFREE_CHARGED_LPR			0x06
#define	STT_GB_BUSY							0x02
#define STT_GB_NUMBER_CHANGED				0x03
#define STT_GB_CONGESTION					0x04
#define STT_GB_UNALLOCATED_NUMBER			0x07
#define STT_GB_LINE_OUT_OF_ORDER			0x08
#define STT_GB_NONE							0xFF

enum KSignGroupII
{
	kg2Ordinary			= 0x01,
	kg2Priority			= 0x02,
	kg2Maintenance		= 0x03,
	kg2LocalPayPhone	= 0x04,
	kg2TrunkOperator	= 0x05,
	kg2DataTrans		= 0x06,
	kg2NonLocalPayPhone = 0x07,
	kg2CollectCall		= 0x08,
	kg2OrdinaryInter	= 0x09,
	kg2Transfered		= 0x0B,
};
#define STT_GII_ORDINARY					0x01
#define STT_GII_PRIORITY					0x02
#define STT_GII_MAINTENANCE					0x03
#define STT_GII_LOCAL_PAY_PHONE				0x04
#define STT_GII_TRUNK_OPERATOR				0x05
#define STT_GII_DATA_TRANS					0x06
#define STT_GII_NON_LOCAL_PAY_PHONE			0x07		
#define STT_GII_COLLECT_CALL				0x08
#define STT_GII_ORDINARY_INTERNATIONAL		0x09
#define STT_GII_TRANSFERED					0x0B
#endif
#ifndef _KISDN_H_
#define _KISDN_H_
#define KMAX_USER_USER_LEN              32
#define KMAX_USER_USER_EX_LEN           254
#define KMAX_SUBADRESS_INFORMATION_LEN  20 

enum KQ931Cause
{
    kq931cNone                    			= 0,
    kq931cUnallocatedNumber       			= 1,
    kq931cNoRouteToTransitNet     			= 2,
    kq931cNoRouteToDest           			= 3,
	kq931cSendSpecialInfoTone				= 4,
	kq931cMisdialedTrunkPrefix				= 5,
    kq931cChannelUnacceptable     			= 6,
    kq931cCallAwarded             			= 7,
	kq931cPreemption						= 8,
	kq931cPreemptionCircuitReuse			= 9,
	kq931cQoR_PortedNumber					= 14,
    kq931cNormalCallClear         			= 16,
    kq931cUserBusy                			= 17,
    kq931cNoUserResponding        			= 18,
    kq931cNoAnswerFromUser        			= 19,
	kq931cSubscriberAbsent					= 20,
    kq931cCallRejected            			= 21,
    kq931cNumberChanged           			= 22,
	kq931cRedirectionToNewDest				= 23,
	kq931cCallRejectedFeatureDest			= 24,
	kq931cExchangeRoutingError				= 25,
    kq931cNonSelectedUserClear    			= 26,
    kq931cDestinationOutOfOrder   			= 27,
    kq931cInvalidNumberFormat     			= 28,
    kq931cFacilityRejected        			= 29,
    kq931cRespStatusEnquiry       			= 30,
    kq931cNormalUnspecified       			= 31,
    kq931cNoCircuitChannelAvail   			= 34,
    kq931cNetworkOutOfOrder       			= 38,
	kq931cPermanentFrameConnOutOfService	= 39,
	kq931cPermanentFrameConnOperational		= 40,
    kq931cTemporaryFailure          		= 41,
    kq931cSwitchCongestion          		= 42,
    kq931cAccessInfoDiscarded       		= 43,
    kq931cRequestedChannelUnav      		= 44,
	kq931cPrecedenceCallBlocked				= 46,
    kq931cResourceUnavailable       		= 47,
    kq931cQosUnavailable            		= 49,
    kq931cReqFacilityNotSubsc       		= 50,
	kq931cOutCallsBarredWithinCUG			= 53,
	kq931cInCallsBarredWithinCUG			= 55,
    kq931cBearerCapabNotAuthor      		= 57,
    kq931cBearerCapabNotAvail       		= 58,
	kq931cInconsistency						= 62,
    kq931cServiceNotAvailable       		= 63,
    kq931cBcNotImplemented          		= 65,
    kq931cChannelTypeNotImplem      		= 66,
    kq931cReqFacilityNotImplem      		= 69,
    kq931cOnlyRestrictedBcAvail     		= 70,
    kq931cServiceNotImplemented     		= 79,
    kq931cInvalidCrv                		= 81,
    kq931cChannelDoesNotExist       		= 82,
    kq931cCallIdDoesNotExist        		= 83,
    kq931cCallIdInUse               		= 84,
    kq931cNoCallSuspended           		= 85,
    kq931cCallIdCleared             		= 86,
	kq931cUserNotMemberofCUG				= 87,
    kq931cIncompatibleDestination   		= 88,
    kq931cInvalidTransitNetSel      		= 91,
    kq931cInvalidMessage            		= 95,
    kq931cMissingMandatoryIe        		= 96,
    kq931cMsgTypeNotImplemented     		= 97,
    kq931cMsgIncompatWithState      		= 98,
    kq931cIeNotImplemented          		= 99,
    kq931cInvalidIe                 		= 100,
    kq931cMsgIncompatWithState2     		= 101,
    kq931cRecoveryOnTimerExpiry     		= 102,
    kq931cProtocolError             		= 103,
	kq931cMessageWithUnrecognizedParam		= 110,
	kq931cProtocolErrorUnspecified			= 111,
    kq931cInterworking              		= 127,
    kq931cCallConnected             		= 128,
    kq931cCallTimedOut              		= 129,
    kq931cCallNotFound              		= 130,
    kq931cCantReleaseCall           		= 131,
    kq931cNetworkFailure            		= 132,
    kq931cNetworkRestart            		= 133,
    kq931cLastValidCause            		= kq931cNetworkRestart,
};

enum KQ931ProgressIndication
{
    kq931pTonesMaybeAvailable       = 1,
    kq931pDestinationIsNonIsdn      = 2,
    kq931pOriginationIsNonIsdn      = 3,
    kq931pCallReturnedToIsdn        = 4,
    kq931pTonesAvailable            = 8,
};

enum KQ931Hlc
{
    kq931hTelefony                  = 0x81,
    k1931hFaxGroup23                = 0x84,
    k1931hFaxGroup4                 = 0xa1,
    kq931hTeletexF184               = 0xa4,
    kq931hTeletexF220               = 0xa8,
    kq931hTeletexf200               = 0xb1,
    kq931hVideotex                  = 0xb2,
    kq931hTelexF60                  = 0xb5,
    kq931hMhs                       = 0xb8,
    kq931hOsiApp                    = 0xc1,
    kq931hMaintenance               = 0xde,
    kq931hManagement                = 0xdf,
};

enum KQ931BearerCapability
{
    kq931bSpeech                    = 0x00,
    kq931bUnrestrictedDigital       = 0x08,
    kq931bAudio31kHz                = 0x10,
    kq931bAudio7kHz                 = 0x11,
    kq931bVideo                     = 0x18,
};

enum KQ931TypeOfNumber
{
    kq931tUnknownNumber             = 0x00,
    kq931tInternationalNumber       = 0x10,
    kq931tNationalNumber            = 0x20,
    kq931tNetworkSpecificNumber     = 0x30,
    kq931tSubscriberNumber          = 0x40,
    kq931tAbbreviatedNumber         = 0x60,
    kq931tReservedNumber            = 0x70,
    kq931tDefaultNumber             = kq931tUnknownNumber,
};

enum KQ931NumberingPlan
{
    kq931pUnknownPlan               = 0x00,
    kq931pIsdnPlan                  = 0x01,
    kq931pDataPlan                  = 0x03,
    kq931pTelexPlan                 = 0x04,
    kq931pNationalPlan              = 0x08,
    kq931pPrivatePlan               = 0x09,
    kq931pReservedPlan              = 0x0F,
    kq931pDefaultPlan               = kq931pUnknownPlan,
};

enum KQ931UserInfoProtocolDescriptor
{
    kq931uuUserSpecific             = 0x00,
    kq931uuOSI_HighLayer            = 0x01,
    kq931uuX244                     = 0x02,
    kq931uuIA5_Chars                = 0x04,
    kq931uuX208_X209                = 0x05,
    kq931uuV120                     = 0x07,
    kq931uuQ931_CallControl         = 0x08,
    kq931uuNational                 = 0x40 
};
enum KQ931PresentationIndicator
{
	kq931piPresentationAllowed					= 0x00,
	kq931piPresentationRestricted				= 0x01,
	kq931piNumberNotAvailableDueToInterworking	= 0x02,
};
enum KQ931ScreeningIndicator
{
	kq931siUserProvidedNotScreened				= 0x00,
	kq931siUserProvidedVerifiedAndPassed		= 0x01,
	kq931siUserProvidedVerifiedAndFailed		= 0x02,
	kq931siNetworkProvided						= 0x03,
};
enum KQ931TypeOfSubaddress
{
    kq931tsNSAP                                 = 0x00,
    kq931tsUserSpecified                        = 0x01,
};
#endif 
#ifndef KGSM_H
#define KGSM_H

enum KGsmCallCause
{
    kgccNone                      = 0,
    kgccUnallocatedNumber         = 1,
    kgccNoRouteToDest             = 3,
    kgccChannelUnacceptable       = 6,
    kgccOperatorDeterminedBarring = 8,
    kgccNormalCallClear           = 16,
    kgccUserBusy                  = 17,
    kgccNoUserResponding          = 18,
    kgccNoAnswerFromUser          = 19,
    kgccCallRejected              = 21,
    kgccNumberChanged             = 22,
    kgccNonSelectedUserClear      = 26,
    kgccDestinationOutOfOrder     = 27,
    kgccInvalidNumberFormat       = 28,
    kgccFacilityRejected          = 29,
    kgccRespStatusEnquiry         = 30,
    kgccNormalUnspecified         = 31,
    kgccNoCircuitChannelAvail     = 34,
    kgccNetworkOutOfOrder         = 38,
    kgccTemporaryFailure          = 41,
    kgccSwitchCongestion          = 42,
    kgccAccessInfoDiscarded       = 43,
    kgccRequestedChannelUnav      = 44,
    kgccResourceUnavailable       = 47,
    kgccQosUnavailable            = 49,
    kgccReqFacilityNotSubsc       = 50,
    kgccCallBarredWitchCUG        = 55,
    kgccBearerCapabNotAuthor      = 57,
    kgccBearerCapabNotAvail       = 58,
    kgccServiceNotAvailable       = 63,
    kgccBcNotImplemented          = 65,
    kgccReqFacilityNotImplem      = 69,
    kgccOnlyRestrictedBcAvail     = 70,
    kgccServiceNotImplemented     = 79,
    kgccInvalidCrv                = 81,
    kgccUserNotMemberOfCUG        = 82,
    kgccIncompatibleDestination   = 88,
    kgccInvalidTransitNetSel      = 91,
    kgccInvalidMessage            = 95,
    kgccMissingMandatoryIe        = 96,
    kgccMsgTypeNotImplemented     = 97,
    kgccMsgIncompatWithState      = 98,
    kgccIeNotImplemented          = 99,
    kgccInvalidIe                 = 100,
    kgccMsgIncompatWithState2     = 101,
    kgccRecoveryOnTimerExpiry     = 102,
    kgccProtocolError             = 111,
    kgccInterworking              = 127,
};

enum KGsmMobileCause
{
    kgmcPhoneFailure                = 0,
    kgmcNoConnectionToPhone         = 1,
    kgmcPhoneAdaptorLinkReserved    = 2,
    kgmcOperationNotAllowed         = 3,
    kgmcOperationNotSupported       = 4,
    kgmcPH_SIMPINRequired           = 5,
    kgmcPH_FSIMPINRequired          = 6,
    kgmcPH_FSIMPUKRequired          = 7,
    kgmcSIMNotInserted              = 10,
    kgmcSIMPINRequired              = 11,
    kgmcSIMPUKRequired              = 12,
    kgmcSIMFailure                  = 13,
    kgmcSIMBusy                     = 14,
    kgmcSIMWrong                    = 15,
    kgmcIncorrectPassword           = 16,
    kgmcSIMPIN2Required             = 17,
    kgmcSIMPUK2Required             = 18,
    kgmcMemoryFull                  = 20,
    kgmcInvalidIndex                = 21,
    kgmcNotFound                    = 22,
    kgmcMemoryFailure               = 23,
    kgmcTextStringTooLong           = 24,
    kgmcInvalidCharInTextString     = 25,
    kgmcDialStringTooLong           = 26,
    kgmcInvalidCharInDialString     = 27,
    kgmcNoNetworkService            = 30,  
    kgmcNetworkTimeout              = 31,  
    kgmcNetworkNotAllowed           = 32,
    kgmcCommandAborted              = 33,
    kgmcNumParamInsteadTextParam    = 34,
    kgmcTextParamInsteadNumParam    = 35,
    kgmcNumericParamOutOfBounds     = 36,
    kgmcTextStringTooShort          = 37,
    kgmcNetworkPINRequired          = 40,
    kgmcNetworkPUKRequired          = 41,
    kgmcNetworkSubsetPINRequired    = 42,
    kgmcNetworkSubnetPUKRequired    = 43,
    kgmcServiceProviderPINRequired  = 44,
    kgmcServiceProviderPUKRequired  = 45,
    kgmcCorporatePINRequired        = 46,
    kgmcCorporatePUKRequired        = 47,
    kgmcSIMServiceOptNotSupported   = 60,
    kgmcUnknown                     = 100,
    kgmcIllegalMS_N3                = 103,
    kgmcIllegalME_N6                = 106,
    kgmcGPRSServicesNotAllowed_N7   = 107,
    kgmcPLMNNotAllowed_No11         = 111,
    kgmcLocationAreaNotAllowed_N12  = 112,
    kgmcRoamingNotAllowed_N13       = 113,
    kgmcServiceOptNotSupported_N32  = 132,
    kgmcReqServOptNotSubscribed_N33 = 133,
    kgmcServOptTempOutOfOrder_N34   = 134,
    kgmcLongContextActivation       = 147,
    kgmcUnspecifiedGPRSError        = 148,
    kgmcPDPAuthenticationFailure    = 149,
    kgmcInvalidMobileClass          = 150,
    kgmcGPRSDisconnectionTmrActive  = 151,
    kgmcTooManyActiveCalls          = 256,
    kgmcCallRejected                = 257,
    kgmcUnansweredCallPending       = 258,
    kgmcUnknownCallingError         = 259,
    kgmcNoPhoneNumRecognized        = 260,
    kgmcCallStateNotIdle            = 261,
    kgmcCallInProgress              = 262,
    kgmcDialStateError              = 263,
    kgmcUnlockCodeRequired          = 264,
    kgmcNetworkBusy                 = 265,
    kgmcInvalidPhoneNumber          = 266,
    kgmcNumberEntryAlreadyStarted   = 267,
    kgmcCancelledByUser             = 268,
    kgmcNumEntryCouldNotBeStarted   = 269,
    kgmcDataLost                    = 280,
    kgmcInvalidBessageBodyLength    = 281,
    kgmcInactiveSocket              = 282,
    kgmcSocketAlreadyOpen           = 283,
    
    kgmcSuccess                     = 0x7fff
};

enum KGsmSmsCause
{
    kgscNone                        = 0,
    kgscUnassigned                  = 1,
    kgscOperatorDeterminedBarring   = 8,
    kgscCallBarred                  = 10,
    kgscSMSTransferRejected         = 21,
    kgscDestinationOutOfService     = 27,
    kgscUnidentifiedSubscriber      = 28,
    kgscFacilityRejected            = 29,
    kgscUnknownSubscriber           = 30,
    kgscNetworkOutOfOrder           = 38,
    kgscTemporaryFailure            = 41,
    kgscCongestion                  = 42,
    kgscResourcesUnavailable        = 47,
    kgscFacilityNotSubscribed       = 50,
    kgscFacilityNotImplemented      = 69,
    kgscInvalidSMSTransferRefValue  = 81,
    kgscInvalidMessage              = 95,
    kgscInvalidMandatoryInformation = 96,
    kgscMessageTypeNonExistent      = 97,
    kgscMsgNotCompatWithSMProtState = 98,
    kgscInformationElementNonExiste = 99,
    kgscProtocolError               = 111,
    kgscInterworking                = 127,
    kgscTelematicInterworkingNotSup = 128,
    kgscSMSTypeZeroNotSupported     = 129,
    kgscCannotReplaceSMS            = 130,
    kgscUnspecifiedTPPIDError       = 143,
    kgscAlphabetNotSupported        = 144,
    kgscMessageClassNotSupported    = 145,
    kgscUnspecifiedTPDCSError       = 159,
    kgscCommandCannotBeActioned     = 160,
    kgscCommandUnsupported          = 161,
    kgscUnspecifiedTPCommandError   = 175,
    kgscTPDUNotSupported            = 176,
    kgscSCBusy                      = 192,
    kgscNoSCSubscription            = 193,
    kgscSCSystemFailure             = 194,
    kgscInvalidSMEAddress           = 195,
    kgscDestinationSMEBarred        = 196,
    kgscSMRejectedDuplicateSM       = 197,
    kgscTPVPFNotSupported           = 198,
    kgscTPVPNotSupported            = 199,
    kgscSIMSMSStorageFull           = 208,
    kgscNoSMSStorageCapabilityInSIM = 209,
    kgscErrorInMS                   = 210,
    kgscMemoryCapacityExceeded      = 211,
    kgscSIMDataDownloadError        = 213,
    kgscUnspecifiedError            = 255,
    kgscPhoneFailure                = 300,
    kgscSmsServiceReserved          = 301,
    kgscOperationNotAllowed         = 302,
    kgscOperationNotSupported       = 303,
    kgscInvalidPDUModeParameter     = 304,
    kgscInvalidTextModeParameter    = 305,
    kgscSIMNotInserted              = 310,
    kgscSIMPINNecessary             = 311,
    kgscPH_SIMPINNecessary          = 312,
    kgscSIMFailure                  = 313,
    kgscSIMBusy                     = 314,
    kgscSIMWrong                    = 315,
    kgscMemoryFailure               = 320,
    kgscInvalidMemoryIndex          = 321,
    kgscMemoryFull                  = 322,
    kgscSMSCAddressUnknown          = 330,
    kgscNoNetworkService            = 331,
    kgscNetworkTimeout              = 332,
    kgscUnknownError                = 500,
    kgscNetworkBusy                 = 512,
    kgscInvalidDestinationAddress   = 513,
    kgscInvalidMessageBodyLength    = 514,
    kgscPhoneIsNotInService         = 515,
    kgscInvalidPreferredMemStorage  = 516,
    kgscUserTerminated              = 517
};

enum KGsmRegistryStatus
{
	kgrsNotRegistered = 0x00,
	kgrsRegistered    = 0x01,
	kgrsSearching     = 0x02,
	kgrsDenied        = 0x03,
	kgrsUnknown       = 0x04,
	kgrsRoaming       = 0x05,
	kgrsInitializing  = 0xff,
};
enum KGsmCallStatus
{
	kgcstActive   = 0x0,
	kgcstHeld     = 0x1,
	kgcstDialing  = 0x2,
	kgcstAlerting = 0x3,
	kgcstIncoming = 0x4,
	kgcstWaiting  = 0x5,
	kgcstReleased = 0x6,
};
enum KGsmCallMode
{
	kgcmVoice   = 0x0,
	kgcmData    = 0x1,
	kgcmFax     = 0x2,
	kgcmUnknown = 0x3
};
enum KGsmCallFlags
{
	kgcflMultiparty           = 0x01,
	kgcflInternationalNumber  = 0x02,
	kgcflMobileTerminatedCall = 0x04,
};
enum KGsmChannelFeatures
{
	kgcfMultiparty  = 0x01,
	kgcfCallForward = 0x02, 
};
#endif
#ifndef K3L_STATS_H
#define K3L_STATS_H

enum KGeneralCallStatIndex
{
    kcsiInbound,            
    kcsiOutbound,           
    kcsiOutCompleted,       
    kcsiOutFailed,          
    kcsiRemoteDisc,         
    kcsiLocalDisc,          
    kcsiLastGeneralCallStat
};

enum KFailedCallStatIndex
{
    kcsiFailedBusy = kcsiLastGeneralCallStat,
    kcsiFailedNoAnswer,
    kcsiFailedRejected,
    kcsiFailedAddrChanged,
    kcsiFailedInvalidAddr,
    kcsiFailedOutOfService,
    kcsiFailedCongestion,
    kcsiFailedNetworkFailure,
    kcsiFailedOther,
    kcsiLastFailedCallStat
};
const uint32 kcsiMaxCallStats = kcsiLastFailedCallStat;
typedef uint32 KStatIndex;

#endif
#ifndef k3lVersion_h
#define k3lVersion_h
#define k3lApiMajorVersion	2
#define k3lApiMinorVersion	1
#define k3lApiBuildVersion	0
#endif
#if !defined K3L_H
#define K3L_H

enum KDeviceType
{
	kdtE1           = 0,
	kdtFXO          = 1,
	kdtConf         = 2,
	kdtPR           = 3,
	kdtE1GW         = 4,
	kdtFXOVoIP      = 5,
	kdtE1IP	        = 6,
	kdtE1Spx        = 7,
    kdtGWIP         = 8,
    kdtFXS          = 9,
    kdtFXSSpx       = 10,
    kdtGSM          = 11,
    kdtGSMSpx       = 12,
	kdtReserved1  	= 13,
    kdtGSMUSB       = 14,
    kdtGSMUSBSpx    = 15,
    kdtE1FXSSpx 	= 16,
	kdtDevTypeCount
};
enum KSignaling
{
	ksigInactive		= 0,
	ksigR2Digital		= 1,
	ksigContinuousEM	= 2,
	ksigPulsedEM		= 3,
	ksigUserR2Digital	= 4,
	ksigAnalog			= 5,
	ksigOpenCAS			= 6,
	ksigOpenR2			= 7,
	ksigSIP 			= 8,
    ksigOpenCCS         = 9,
    ksigPRI_EndPoint    = 10,
    ksigAnalogTerminal  = 11,
    ksigPRI_Network     = 12,
    ksigPRI_Passive     = 13,
	ksigLineSide  		= 14,
	ksigCAS_EL7			= 15,
    ksigGSM             = 16,
	ksigE1LC			= 17,
	ksigISUP			= 18,
	ksigFax				= 19,
};
enum KE1DeviceModel
{
	kdmE1600	= 0,
	kdmE1600E	= 1,
	kdmE1600EX  = 2
};
enum KE1GWDeviceModel 
{
	kdmE1GW640   = 1,
    kdmE1GW640EX = 2
};
enum KE1IPDeviceModel 
{
	kdmE1IP   = 1,
    kdmE1IPEX = 2
};
enum KGWIPDeviceModel 
{
	kdmGWIP   = 1,
	kdmGWIPEX = 2
};
enum KFXODeviceModel
{
	kdmFXO80    = 0,
	kdmFXOHI    = 1,
	kdmFXO160HI = 2,
    kdmFXO240HI = 3
};
enum KFXOVoIPDeviceModel 
{
	kdmFXGW180 = kdmFXO80
};
enum KConfDeviceModel
{
	kdmConf240   = 0,
	kdmConf120   = 1,
	kdmConf240EX = 2,
	kdmConf120EX = 3
};
enum KPRDeviceModel
{
	kdmPR300v1         = 0,
    kdmPR300           = 1,
	kdmPR300SpxBased   = 2,
    kdmPR300EX         = 3
};
enum KFXSDeviceModel
{
    kdmFXS300   = 1,
    kdmFXS300EX = 2
};
enum KFXSSpxDeviceModel
{
    kdmFXSSpx300      = 0,
    kdmFXSSpx2E1Based = 1,
    kdmFXSSpx300EX    = 2
};
enum KE1SpxDeviceModel
{
	kdmE1Spx    = 0,
	kdm2E1Based = 1,
    kdmE1SpxEX  = 2
};
enum KGSMDeviceModel
{
    kdmGSM      = 0,
    kdmGSMEX    = 1
};
enum KGSMSpxDeviceModel
{
    kdmGSMSpx   = 0,
    kdmGSMSpxEX = 1
};
enum KGSMUSBDeviceModel
{
    kdmGSMUSB      = 0
};
enum KGSMUSBSpxDeviceModel
{
    kdmGSMUSBSpx   = 0
};
enum KE1FXSDeviceModel
{
    kdmE1FXSSpx     = 0,
    kdmE1FXSSpxEX   = 1
};
enum KSystemObject
{
	ksoLink       = 0x00,   
	ksoLinkMon    = 0x20,   
	ksoFirmware   = 0x80,   
	ksoDevice     = 0x100,  
	ksoAPI        = 0x150,  
	ksoH100       = 0x200,  
	ksoChannel	  = 0x1000,	
	ksoGsmChannel = 0x2000, 
};
enum KFirmwareId
{
	kfiE1600A,				
	kfiE1600B,				
	kfiFXO80,				
    kfiGSM40,               
    kfiGSMUSB
};
enum KE1Status
{
	kesOk					= 0x00,		
	kesSignalLost			= 0x01,		
	kesNetworkAlarm			= 0x02,		
	kesFrameSyncLost		= 0x04,		
	kesMultiframeSyncLost	= 0x08,		
	kesRemoteAlarm			= 0x10,		
	kesHighErrorRate		= 0x20,		
	kesUnknownAlarm			= 0x40,		
	kesE1Error				= 0x80,		
    kesNotInitialized       = 0xFF      
};
enum KE1ChannelStatus
{
	kecsFree			= 0x00,			
	kecsBusy			= 0x01,			
	kecsOutgoing		= 0x02,			
	kecsIncoming		= 0x04,			
	kecsLocked			= 0x06,			
	kecsOutgoingLock	= 0x10,			
	kecsLocalFail		= 0x20,			
	kecsIncomingLock	= 0x40,			
	kecsRemoteLock		= 0x80			
};
enum KVoIPChannelStatus
{
	kipcsFree			= kecsFree,		
	kipcsOutgoingLock	= kecsOutgoingLock,
	kipcsIncomingLock	= kecsIncomingLock
};
enum KFXOChannelStatus
{
	kfcsDisabled	= 0x00,		
	kfcsEnabled		= 0x01		
};
enum KFXSChannelStatus
{
    kfxsOnHook,
    kfxsOffHook,
    kfxsRinging,
    kfxsFail
};
enum KGsmChannelStatus
{
    kgsmIdle,
    kgsmCallInProgress,
    kgsmSMSInProgress,
    kgsmModemError,
    kgsmSIMCardError,
    kgsmNetworkError,
    kgsmNotReady
};
enum KCallStatus
{
	kcsFree		= 0x00,				
	kcsIncoming = 0x01,				
	kcsOutgoing = 0x02,				
	kcsFail		= 0x04				
};
enum KCallStartInfo
{
    kcsiHumanAnswer,
    kcsiAnsweringMachine,
    kcsiCellPhoneMessageBox,
    kcsiUnknown,
    kcsiCarrierMessage
};
enum KChannelFeatures
{
	kcfDtmfSuppression	= 0x0001,
	kcfCallProgress		= 0x0002,
	kcfPulseDetection	= 0x0004,
	kcfAudioNotification= 0x0008,
	kcfEchoCanceller	= 0x0010,
	kcfAutoGainControl	= 0x0020,
	kcfHighImpEvents	= 0x0080,
    kcfCallAnswerInfo   = 0x0100,
    kcfOutputVolume     = 0x0200,
    kcfPlayerAGC        = 0x0400
};
enum KMixerSource
{
	kmsChannel,
	kmsPlay,
	kmsGenerator,
	kmsCTbus,
    kmsNoDelayChannel
};
struct KMixerCommand
{
    int32 Track;
    int32 Source;        
    int32 SourceIndex;
};
struct KPlayFromStreamCommand
{
	void *Buffer;
	uint32 BufferSize;
	int32 CodecIndex;
};
struct KBufferParam
{
	void *Buffer;
	uint32 BufferSize;
};
enum KMixerTone
{
	kmtSilence	= 0x00,	
	kmtDial		= 0x01, 
	kmtBusy		= 0x02, 
	kmtFax		= 0x03, 
	kmtVoice	= 0x04, 
	kmtEndOf425 = 0x05,	
    kmtCollect  = 0x06, 
    kmtEndOfDtmf= 0x07, 
};
struct K3L_CHANNEL_STATUS
{
 	KCallStatus CallStatus;
	KMixerTone AudioStatus;
	int32 AddInfo;			
	int32 EnabledFeatures;	
};
struct K3L_GSM_CALL_STATUS
{
	KGsmCallStatus Status;
	KGsmCallMode Mode;
	char Number[ KMAX_DIAL_NUMBER ];
	int32 Flags; 
};
struct K3L_GSM_CHANNEL_STATUS
{
	uint8 SignalStrength; 
	uint8 ErrorRate;      
	KGsmRegistryStatus RegistryStatus;
	char OperName[ KMAX_GSM_OPER_NAME ];
	K3L_GSM_CALL_STATUS CallStatus[ KMAX_GSM_CALLS ];
	int32 UnreadSmsCount;  
	int32 EnabledFeatures; 
    char IMEI[ KMAX_GSM_IMEI_SIZE ];
    unsigned char SIM; 
    unsigned char Reserved[15]; 
};
struct K3L_LINK_STATUS
{
	int16 E1; 
	byte Channels[ KMAX_E1_CHANNELS ];	
};
enum KPllErrors
{
	kpeClockAError    = 0x01,
	kpeClockBError    = 0x02,
	kpeSCbusError     = 0x04,
	kpeMVIPError	  = 0x08,
	kpeMasterPllError = 0x10,
	kpeModeError      = 0x20,
	kpeLocalRefError  = 0x40,
	kpeInternalError  = 0x80
};
struct K3L_H100_STATUS
{
	int32 Mode;
	int32 MasterClock;
	int32 Enable;
	int32 Reference;
	int32 SCbus;
	int32 HMVIP;
	int32 MVIP90;
	int32 CT_NETREF;
	int32 PllLocalRef;
	int32 PllErrors;		
};
enum KEchoLocation
{
	kelNetwork	= 0x0,
	kelCtBus	= 0x1
};
enum KCodecIndex
{
	kci8kHzALAW	= 0x00,
	kci8kHzPCM	= 0x01,  
	kci11kHzPCM	= 0x02,  
	kci8kHzGSM	= 0x03, 
	kci8kHzADPCM= 0x04, 
	kci8kHzULAW	= 0x05,
	kciLastCodecEntryMark
};
enum KEchoCancellerConfig
{
    keccNotPresent,
    keccOneSingleBank,
    keccOneDoubleBank,
    keccTwoSingleBank,
    keccTwoDoubleBank,
    keccFail
};
struct K3L_DEVICE_CONFIG
{
	int32 LinkCount;
	int32 ChannelCount;
	int32 EnabledChannelCount;
	int32 MixerCount;
	int32 MixerCapacity;
	int32 WorkStatus;
	int32 DeviceModel;		
	int32 H100_Mode;		
	int32 PciBus;
	int32 PciSlot;
	int32 PlayerCount;
	int32 VoIPChannelCount;
    int32 CTbusCount;
    KEchoCancellerConfig EchoConfig;
    KEchoLocation EchoLocation;
	sbyte SerialNumber[ KMAX_SERIAL_NUMBER ];
};

typedef K3L_DEVICE_CONFIG K3L_E1_DEVICE_CONFIG;
typedef K3L_DEVICE_CONFIG K3L_FX_DEVICE_CONFIG;
struct K3L_API_CONFIG
{
	int32 MajorVersion;
	int32 MinorVersion;
	int32 BuildVersion;
    int32 SvnRevision;
	int32 RawCmdLogging;
	int32 VpdVersionNeeded;
	sbyte StrVersion[ KMAX_STR_VERSION ];
};
struct K3L_LINK_CONFIG
{
	KSignaling Signaling;
	int32 IncomingDigitsRequest;
	int32 IdentificationRequestPos;
	int32 ChannelCount;
	int32 ReceivingClock;
	sbyte NumberA[ KMAX_DIAL_NUMBER + 1 + 3 ]; 
};
struct K3L_CHANNEL_CONFIG
{
	KSignaling Signaling;
    int32 AutoEnableFeatures;   
    int32 CapableFeatures;      
};
struct K3L_E1600A_FW_CONFIG
{
	int32 MfcExchangersCount;
	int32 MonitorBufferSize;
	sbyte FwVersion[ KMAX_STR_VERSION ];
	sbyte DspVersion[ KMAX_DSP_NAME ];
};
struct K3L_E1600B_FW_CONFIG
{
	int32 AudioBufferSize;
	int32 FilterCount;
	int32 MixerCount;
	int32 MixerCapacity;
	sbyte FwVersion[ KMAX_STR_VERSION ];
	sbyte DspVersion[ KMAX_DSP_NAME ];
};
typedef K3L_E1600B_FW_CONFIG K3L_GSM40_FW_CONFIG;
typedef K3L_E1600B_FW_CONFIG K3L_FXO80_FW_CONFIG;
typedef K3L_E1600B_FW_CONFIG K3L_GSMUSB_FW_CONFIG;
struct K3L_COMMAND
{
	int32 Object;			
	int32 Cmd;				
	byte *Params;			
};

enum KEventObjectId
{
	koiDevice		= 0x00,
	koiChannel		= 0x01,
	koiPlayer		= 0x02,
	koiLink			= 0x03,
    koiSystem       = 0x04
};

struct K3L_EVENT
{
	int32 Code;				
	int32 AddInfo;			
	int32 DeviceId;			
	int32 ObjectInfo;		
	void *Params;			
	int32 ParamSize;		
	int32 ObjectId;			
};
struct KIncomingSeizeParams
{
	sbyte NumberB[ KMAX_DIAL_NUMBER + 1 ];
	sbyte NumberA[ KMAX_DIAL_NUMBER + 1 ];
	sbyte Padding[2];  
};
struct KCtbusCommand
{
	int32 Stream;
	int32 TimeSlot;
	int32 Enable;
};
struct KUserInformation
{
    int32 ProtocolDescriptor;
    int32 UserInfoLength;
    byte  UserInfo[ KMAX_USER_USER_LEN ];
};
struct KUserInformationEx
{
    int32 ProtocolDescriptor;
    int32 UserInfoLength;
    byte  UserInfo[ KMAX_USER_USER_EX_LEN ];
};
struct KISDNSubaddressInformation
{
    KQ931TypeOfSubaddress TypeOfSubaddress;
    bool  OddNumberOfSignals;
    int32 InformationLength;
    byte  Information[ KMAX_SUBADRESS_INFORMATION_LEN ];
};
struct KISDNSubaddresses
{
    KISDNSubaddressInformation Called;
    KISDNSubaddressInformation Calling;
};
enum KCTTransferResult
{
    TransferByJoin          = 0,
    TransferByRerouteing    = 1
};
enum KLinkErrorCounter
{
	klecChangesToLock		=  0,
	klecLostOfSignal		=  1,
	klecAlarmNotification   =  2,
	klecLostOfFrame         =  3,
	klecLostOfMultiframe    =  4,
	klecRemoteAlarm			=  5,
    klecSlipAlarm           =  6,
	klecUnknowAlarm			=  klecSlipAlarm,
	klecPRBS				=  7,
	klecWrogrBits			=  8,
	klecJitterVariation		=  9,
	klecFramesWithoutSync	= 10,
	klecMultiframeSignal	= 11,
	klecFrameError			= 12,
	klecBipolarViolation	= 13,
	klecCRC4				= 14,
	klecCount				= 15
};
struct K3L_LINK_ERROR_COUNTER
{
	int32 ErrorCounters[ klecCount ];
};
struct KSipData
{
    int32 DataLength;
    byte  Data[ KMAX_SIP_DATA ];
};

enum KLibParams
{
	klpDebugFirmware,           
    klpResetFwOnStartup,		
    klpResetFwOnShutdown,       
    klpSeizureEventCompat,      
    klpDisableTDMBufferWarnings,
    klpDisableInternalVoIP,     
    klpLogCallControl,          
    klpDoNotLogApiInterface,    
	klpMaxParams
};

enum KFaxChannelStatus
{
	kfaxcsFree,
	kfaxcsWaitingForFaxSignal,
	kfaxcsSendingFax,
	kfaxcsReceivingFax,
	kfaxcsFail
};
enum KFaxResult
{
	kfaxrEndOfTransmission,
	kfaxrStoppedByCommand,
	kfaxrProtocolTimeout,
	kfaxrProtocolError,
	kfaxrRemoteDisconnection,
	kfaxrFileError,
	kfaxrUnknown,
    kfaxrEndOfReception,
	kfaxrCompatibilityError,
	kfaxrQualityError,
	kfaxrChannelReleased
};
enum KFaxFileErrorCause
{
	kfaxfecTransmissionStopped,
	kfaxfecTransmissionError,
	kfaxfecListCleared,
	kfaxfecCouldNotOpen,	
	kfaxfecInvalidHeader,
	kfaxfecDataNotFound,
	kfaxfecInvalidHeight,
	kfaxfecUnsupportedWidth,
	kfaxfecUnsupportedCompression,
	kfaxfecUnsupportedRowsPerStrip,
	kfaxfecUnknown
};

typedef stt_code ( Kstdcall K3L_CALLBACK )();
typedef stt_code ( Kstdcall K3L_MONITOR_CALLBACK )( byte *, byte );
typedef stt_code ( Kstdcall K3L_EVENT_CALLBACK )( int32 Object, K3L_EVENT * );
typedef stt_code ( Kstdcall *K3L_THREAD_CALLBACK )( void * );
typedef void	 ( Kstdcall K3L_AUDIO_CALLBACK )( int32 DevId, int32 Channel, byte *Buffer, int32 Size );
#if __GNUC__ >= 4
#pragma GCC visibility push(default)
#endif
extern "C"
{
sbyte *Kstdcall k3lStart( int32 Major, int32 Minor, int32 Build );
void Kstdcall k3lStop();
void Kstdcall k3lRegisterEventHandler( K3L_EVENT_CALLBACK *Function );
void Kstdcall k3lRegisterAudioListener( K3L_AUDIO_CALLBACK *Player, K3L_AUDIO_CALLBACK *Recorder );
int32 Kstdcall k3lSendCommand( int32 DeviceId, K3L_COMMAND *Cmd );
int32 Kstdcall k3lSendRawCommand( int32 DeviceId, int32 IntfId, void *Command, int32 CmdSize );
int32 Kstdcall k3lRegisterMonitor( K3L_MONITOR_CALLBACK *EventMonitor, K3L_MONITOR_CALLBACK *CommandMonitor, K3L_MONITOR_CALLBACK *BufferMonitor );
int32 Kstdcall k3lGetDeviceConfig( int32 DeviceId, int32 Object, void *Data, int32 DataSize );
int32 Kstdcall k3lGetDeviceStatus( int32 DeviceId, int32 Object, void *Data, int32 DataSize );
int32 Kstdcall k3lGetDeviceCount();
int32 Kstdcall k3lGetDeviceType( int32 DeviceId );
int32 Kstdcall k3lGetEventParam( K3L_EVENT *Evt, const sbyte *Name, sbyte *Buffer, int32 BufferSize );
int32 Kstdcall k3lSetGlobalParam( int32 ParamIndex, int32 ParamValue );
int32 Kstdcall k3lGetChannelStats( int32 DevId, int32 Channel, KStatIndex si, uint32 *StatData );
}
#if __GNUC__ >= 4
#pragma GCC visibility pop
#endif
#endif


enum KISDNDebugFlag
{
    kidfQ931                        = 0x01,
    kidfLAPD                        = 0x02,
    kidfSystem                      = 0x04,
    kidfInvalid                     = 0x08,
};

#define CM_VOIP_START_DEBUG      0x20
#define CM_VOIP_STOP_DEBUG       0x21
#define CM_VOIP_DUMP_STAT        0x22

#define CM_ISDN_DEBUG            0x24

#define CM_PING     0x123456
#define EV_PONG     0x654321

#define CM_LOG_UPDATE            0x100 
#define EV_LOG_UPDATE            0x100 

#endif /* _GENERATED_K3L_H_ */
