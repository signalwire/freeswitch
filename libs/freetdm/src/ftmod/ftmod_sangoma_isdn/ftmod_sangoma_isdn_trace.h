/*
 * Copyright (c) 2010, Sangoma Technologies 
 * David Yat Sin <davidy@sangoma.com>
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
#ifndef __FTMOD_SANGOMA_ISDN_TRACE_H__
#define __FTMOD_SANGOMA_ISDN_TRACE_H__

#define MX_CODE_TXT_LEN 70
#define Q931_LOCKING_SHIFT			0x90
#define Q931_NON_LOCKING_SHIFT		0x98

#define PROT_Q931_RELEASE_CAUSE_MISDIALED_TRUNK_PREFIX	5
#define PROT_Q931_RELEASE_CAUSE_INVALID_NUMBER_FORMAT	28
#define PROT_Q931_RELEASE_CAUSE_NO_CHAN_AVAIL 			34
#define PROT_Q931_RELEASE_CAUSE_DEST_OUT_OF_ORDER 		27
#define PROT_Q931_RELEASE_CAUSE_IE_NOT_EXIST 			99
#define PROT_Q931_RECOVERY_ON_TIMER_EXPIRE				102
#define PROT_Q931_RELEASE_CAUSE_WRONG_CALL_STATE		101


#define PROT_Q931_IE_SEGMENTED_MESSAGE			0x00
#define PROT_Q931_IE_BEARER_CAP					0x04
#define PROT_Q931_IE_CAUSE						0x08
#define PROT_Q931_IE_CALL_IDENTITY				0x10
#define PROT_Q931_IE_CALL_STATE					0x14
#define PROT_Q931_IE_CHANNEL_ID					0x18
#define PROT_Q931_IE_FACILITY					0x1c
#define PROT_Q931_IE_PROGRESS_IND				0x1e
#define PROT_Q931_IE_NETWORK_SPF_FACILITY		0x20
#define PROT_Q931_IE_NOTIFICATION_IND			0x27
#define PROT_Q931_IE_DISPLAY					0x28
#define PROT_Q931_IE_DATE_TIME					0x29
#define PROT_Q931_IE_KEYPAD_FACILITY			0x2c
#define PROT_Q931_IE_INFORMATION_REQUEST		0x32
#define PROT_Q931_IE_SIGNAL						0x34
#define PROT_Q931_IE_SWITCHOOK					0x36
#define PROT_Q931_IE_GENERIC_DIGITS				0x37
#define PROT_Q931_IE_FEATURE_ACT				0x38
#define PROT_Q931_IE_FEATURE_IND				0x39
#define PROT_Q931_IE_INFORMATION_RATE			0x40
#define PROT_Q931_IE_END_TO_END_TRANSIT_DELAY	0x42
#define PROT_Q931_IE_TRANSIT_DELAY_SELECT_IND	0x43
#define PROT_Q931_IE_PACKET_LAYER_BINARY_PARAMS	0x44
#define PROT_Q931_IE_PACKET_LAYER_WINDOW_SIZE	0x45
#define PROT_Q931_IE_PACKET_LAYER_SIZE			0x46
#define PROT_Q931_IE_CALLING_PARTY_NUMBER		0x6c
#define PROT_Q931_IE_CALLING_PARTY_SUBADDRESS	0x6d
#define PROT_Q931_IE_CALLED_PARTY_NUMBER		0x70
#define PROT_Q931_IE_CALLED_PARTY_SUBADDRESS	0x71
#define PROT_Q931_IE_REDIRECTING_NUMBER			0x74
#define PROT_Q931_IE_REDIRECTION_NUMBER			0x76
#define PROT_Q931_IE_TRANSIT_NETWORK_SELECTION	0x78
#define PROT_Q931_IE_RESTART_IND				0x79
#define PROT_Q931_IE_LOW_LAYER_COMPAT			0x7c
#define PROT_Q931_IE_HIGH_LAYER_COMPAT			0x7d
#define PROT_Q931_IE_USER_USER					0x7e
#define PROT_Q931_IE_SENDING_COMPLETE			0xa1
#define PROT_Q931_IE_ESCAPE_FOR_EXTENSION		0x7f
#define PROT_Q931_IE_SENDING_COMPLETE			0xa1

#define NULL_CHAR 0


struct code2str
{
	int code;
	char text[MX_CODE_TXT_LEN];
};

enum {
	I_FRAME = 1,	/* Information frame */
	S_FRAME,		/* Supervisory frame */
	U_FRAME,		/* Unnumbered frame */
};

char ia5[16][8]={{NULL_CHAR,NULL_CHAR,' ','0','@','P','`','p'},
				{NULL_CHAR,NULL_CHAR,'!','1','A','Q','a','q'},
				{NULL_CHAR,NULL_CHAR,'"','2','B','R','b','r'},
				{NULL_CHAR,NULL_CHAR,'#','3','C','S','c','s'},
				{NULL_CHAR,NULL_CHAR,'$','4','D','T','d','t'},
				{NULL_CHAR,NULL_CHAR,'%','5','E','U','e','u'},
				{NULL_CHAR,NULL_CHAR,'&','6','F','V','f','v'},
				{NULL_CHAR,NULL_CHAR,'\'','7','G','W','g','w'},
				{NULL_CHAR,NULL_CHAR,'(','8','H','X','h','x'},
				{NULL_CHAR,NULL_CHAR,')','9','I','Y','i','y'},
				{NULL_CHAR,NULL_CHAR,'*',':','J','Z','j','z'},
				{NULL_CHAR,NULL_CHAR,'+',';','K','[','k','{'},
				{NULL_CHAR,NULL_CHAR,',','<','L','\\','l','|'},
				{NULL_CHAR,NULL_CHAR,'-','=','M',']','m','}'},
				{NULL_CHAR,NULL_CHAR,'.','>','N','^','n','~'},
				{NULL_CHAR,NULL_CHAR,'/','?','O','_','o',NULL_CHAR}};

/* Based on Table 4 - pg 15 of Q.921 Recommendation */
struct code2str dcodQ921FrameFormatTable[] = {
	{I_FRAME, "Information"},
	{S_FRAME, "Supervisory"},
	{U_FRAME, "Unnumbered"},
	{-1, "?"},
};


/* Based on Table 5 - pg 15 of Q.921 Recommendation */
struct code2str dcodQ921SupervisoryCmdTable[] = {
	{0, "RR - receive ready"},
	{1, "RNR - receive not ready"},
	{2, "REJ - reject"},
	{-1, "Unknown"},
};

/* Based on Table 5 - pg 15 of Q.921 Recommendation */
struct code2str dcodQ921UnnumberedCmdTable[] = {
	{0x0F, "SABME - set async balanced mode extended"},
	{0x03, "DM - disconnected mode"},
	{0x00, "UI - unnumbered information"},
	{0x08, "DISC - disconnect"},
	{0x0C, "UA - unnumbered acknowledgement"},
	{0x11, "FRMR - frame reject"},
	{0x17, "XID - Exchange Identification)"},
	{-1, "Unknown"},
};

struct code2str dcodQ931ProtDiscTable[] = {
	{0x08, "Q.931/I.451"},
	{0x09, "Q.2931"},
	{-1, "Unknown"},
};

struct code2str dcodQ931CallRefHiTable[] = {
	{0, "0"},
	{16, "1"},
	{32, "2"},
	{48, "3"},
	{64, "4"},
	{80, "5"},
	{96, "6"},
	{112, "7"},
	{128, "8"},
	{144, "9"},
	{160, "A"},
	{176, "B"},
	{192, "C"},
	{208, "D"},
	{224, "E"},
	{240, "F"},
	{-1,"?"},
};

struct code2str dcodQ931CallRefLoTable[] = {
	{0, "0"},
	{1, "1"},
	{2, "2"},
	{3, "3"},
	{4, "4"},
	{5, "5"},
	{6, "6"},
	{7, "7"},
	{8, "8"},
	{9, "9"},
	{10, "A"},
	{11, "B"},
	{12, "C"},
	{13, "D"},
	{14, "E"},
	{15, "F"},
	{-1, "?"},
};

#define PROT_Q931_MSGTYPE_ALERTING			1
#define PROT_Q931_MSGTYPE_PROCEEDING		2
#define PROT_Q931_MSGTYPE_PROGRESS			3
#define PROT_Q931_MSGTYPE_SETUP				5
#define PROT_Q931_MSGTYPE_CONNECT 			7
#define PROT_Q931_MSGTYPE_SETUP_ACK			13
#define PROT_Q931_MSGTYPE_CONNECT_ACK		15
#define PROT_Q931_MSGTYPE_USER_INFO			32
#define PROT_Q931_MSGTYPE_SUSPEND_REJ		33
#define PROT_Q931_MSGTYPE_RESUME_REJ		34
#define PROT_Q931_MSGTYPE_SUSPEND			37
#define PROT_Q931_MSGTYPE_RESUME			38
#define PROT_Q931_MSGTYPE_SUSPEND_ACK		45
#define PROT_Q931_MSGTYPE_RESUME_ACK		46
#define PROT_Q931_MSGTYPE_DISCONNECT		69
#define PROT_Q931_MSGTYPE_RESTART			70
#define PROT_Q931_MSGTYPE_RELEASE			77
#define PROT_Q931_MSGTYPE_RESTART_ACK		78
#define PROT_Q931_MSGTYPE_RELEASE_COMPLETE	90
#define PROT_Q931_MSGTYPE_SEGMENT			96
#define PROT_Q931_MSGTYPE_FACILITY			98
#define PROT_Q931_MSGTYPE_NOTIFY			110
#define PROT_Q931_MSGTYPE_STATUS_ENQUIRY	117
#define PROT_Q931_MSGTYPE_CONGESTION_CNTRL	121
#define PROT_Q931_MSGTYPE_INFORMATION		123
#define PROT_Q931_MSGTYPE_STATUS			125


struct code2str dcodQ931MsgTypeTable[] = {
	{PROT_Q931_MSGTYPE_ALERTING, "ALERT"},
	{PROT_Q931_MSGTYPE_PROCEEDING, "PROCEED"},
	{PROT_Q931_MSGTYPE_PROGRESS, "PROGRESS"},
	{PROT_Q931_MSGTYPE_SETUP, "SETUP"},
	{PROT_Q931_MSGTYPE_CONNECT, "CONNECT"},
	{PROT_Q931_MSGTYPE_SETUP_ACK, "SETUP ACK"},
	{PROT_Q931_MSGTYPE_CONNECT_ACK, "CONNECT ACK"},
	{PROT_Q931_MSGTYPE_USER_INFO, "USER INFO"},
	{PROT_Q931_MSGTYPE_SUSPEND_REJ, "SUSPEND REJ"},
	{PROT_Q931_MSGTYPE_RESUME_REJ, "RESUME REJ"},
	{PROT_Q931_MSGTYPE_SUSPEND, "SUSPEND"},
	{PROT_Q931_MSGTYPE_RESUME, "RESUME"},
	{PROT_Q931_MSGTYPE_SUSPEND_ACK, "SUSPEND ACK"},
	{PROT_Q931_MSGTYPE_RESUME_ACK, "RESUME ACK"},
	{PROT_Q931_MSGTYPE_DISCONNECT, "DISCONNECT"},
	{PROT_Q931_MSGTYPE_RESTART, "RESTART"},
	{PROT_Q931_MSGTYPE_RELEASE, "RELEASE"},
	{PROT_Q931_MSGTYPE_RESTART_ACK, "RESTART ACK"},
	{PROT_Q931_MSGTYPE_RELEASE_COMPLETE, "RELEASE COMPLETE"},
	{PROT_Q931_MSGTYPE_SEGMENT, "SEGMENT"},
	{PROT_Q931_MSGTYPE_FACILITY, "FACILITY"},
	{PROT_Q931_MSGTYPE_NOTIFY, "NOTIFY"},
	{PROT_Q931_MSGTYPE_STATUS_ENQUIRY, "STATUS ENQ"},
	{PROT_Q931_MSGTYPE_CONGESTION_CNTRL, "CONGESTION CTRL"},
	{PROT_Q931_MSGTYPE_INFORMATION, "INFO"},
	{PROT_Q931_MSGTYPE_STATUS, "STATUS"},
	{-1, "UNKNOWN"},
};

struct code2str dcodQ931CauseCodeTable[] = {
	{1, "Unallocated (unassigned) number"},
	{2, "No route to specified network"},
	{3, "No route to destination"},
	{4, "Send special information tone"},
	{5, "Misdialed trunk prefix"},
	{6, "Channel Unacceptable"},
	{7, "Call awarded and channel established"},
	{8, "Pre-emption"},
	{9, "Pre-emption-circuit reserved"},
	{16, "Normal call clearing"},
	{17, "User Busy"},
	{18, "No User Responding"},
	{19, "No Answer from User"},
	{20, "Subscriber Absent"},
	{21, "Call Rejected"},
	{22, "Number Changed"},
	{26, "Non-Selected User Clearing"},
	{27, "Destination Out-of-Order"},
	{28, "Invalid Number Format"},
	{29, "Facility Rejected"},
	{30, "Response to Status Enquiry"},
	{31, "Normal, Unspecified"},
	{34, "No Circuit/Channel Available"},
	{38, "Network Out-of-Order"},
	{39, "Permanent Frame Mode OOS"},
	{40, "Permanent Frame Mode Operational"},
	{41, "Temporary Failure"},
	{42, "Switching Equipment Congestion"},
	{43, "Access Information Discarded"},
	{44, "Requested Circuit/Channel not available"},
	{47, "Resource Unavailable, Unspecified"},
	{49, "Quality of Service not available"},
	{50, "Requested facility not subscribed"},
	{53, "Outgoing calls barred within CUG"},
	{55, "Incoming calls barred within CUG"},
	{57, "Bearer capability not authorized"},
	{58, "Bearer capability not presently available"},
	{62, "Inconsistency in access inf and subscriber"},
	{63, "Service or Option not available"},
	{65, "Bearer capability not implemented"},
	{66, "Channel type not implemented"},
	{69, "Requested facility not implemented"},
	{70, "Only restricted digital BC available"},
	{79, "Service or option not implemented"},
	{81, "Invalid call reference value"},
	{82, "Identified channel does not exist"},
	{83, "Suspended call exists"},
	{84, "Call identity in use"},
	{85, "No call suspended"},
	{86, "Call already cleared"},
	{87, "User not member of CUG"},
	{88, "Incompatible destination"},
	{90, "Non existent CUG"},
	{91, "Invalid transit network selection"},
	{95, "Invalid message, unspecified"},
	{96, "Mandatory IE missing"}, 
	{97, "Message type non-existent, not implemented"},
	{98, "Message not compatible with call state"},
	{99, "An IE or parameter does not exist"},
	{100, "Invalid IE contents"},
	{101, "Message not compatible with call state"},
	{102, "Recovery on timer expired"},
	{103, "Parameter non-existent, not impl"},
	{110, "Message with unrecognized parameter"},
	{111, "Protocol error, unspecified"},
	{127, "Interworking, unspecified"},
	{-1, "Unknown"},
};

struct code2str dcodQ931IEIDTable[] = {
	{PROT_Q931_IE_SEGMENTED_MESSAGE, "Segmented Message"},
	{PROT_Q931_IE_BEARER_CAP, "Bearer Capability"},
	{PROT_Q931_IE_CAUSE, "Cause"},
	{PROT_Q931_IE_CALL_IDENTITY, "Call Identity"},
	{PROT_Q931_IE_CALL_STATE, "Call State"},
	{PROT_Q931_IE_CHANNEL_ID, "Channel Id"},
	{PROT_Q931_IE_FACILITY, "Facility"},
	{PROT_Q931_IE_PROGRESS_IND, "Progress Indicator"},
	{PROT_Q931_IE_NETWORK_SPF_FACILITY, "Network Specific Facilities"},
	{PROT_Q931_IE_NOTIFICATION_IND, "Notification Indicator"},
	{PROT_Q931_IE_DISPLAY, "Display"},
	{PROT_Q931_IE_DATE_TIME, "Date/Time"},
	{PROT_Q931_IE_KEYPAD_FACILITY, "Keypad Facility"},
	{PROT_Q931_IE_INFORMATION_REQUEST, "Information Request"},
	{PROT_Q931_IE_SIGNAL, "Signal"},
	{PROT_Q931_IE_SWITCHOOK, "Switchhook"},
	{PROT_Q931_IE_GENERIC_DIGITS, "Generic Digits"},
	{PROT_Q931_IE_FEATURE_ACT, "Feature Activation"},
	{PROT_Q931_IE_FEATURE_IND, "Feature Indication"},
	{PROT_Q931_IE_INFORMATION_RATE, "Information Rate"},
	{PROT_Q931_IE_END_TO_END_TRANSIT_DELAY, "End-to-end Transit Delay"},
	{PROT_Q931_IE_TRANSIT_DELAY_SELECT_IND, "Transit Delay Selection and Indication"},
	{PROT_Q931_IE_PACKET_LAYER_BINARY_PARAMS, "Packet layer binary parameters"},
	{PROT_Q931_IE_PACKET_LAYER_WINDOW_SIZE, "Packet layer Window Size"},
	{PROT_Q931_IE_PACKET_LAYER_SIZE, "Packet layer Size"},
	{PROT_Q931_IE_CALLING_PARTY_NUMBER, "Calling Party Number"},
	{PROT_Q931_IE_CALLING_PARTY_SUBADDRESS, "Calling Party Subaddress"},
	{PROT_Q931_IE_CALLED_PARTY_NUMBER, "Called Party Number"},
	{PROT_Q931_IE_CALLED_PARTY_SUBADDRESS, "Called Party Subaddress"},
	{PROT_Q931_IE_REDIRECTING_NUMBER, "Redirecting Number"},
	{PROT_Q931_IE_REDIRECTION_NUMBER, "Redirection Number"},
	{PROT_Q931_IE_TRANSIT_NETWORK_SELECTION, "Transit Network Selection"},
	{PROT_Q931_IE_RESTART_IND, "Restart Indicator"},
	{PROT_Q931_IE_LOW_LAYER_COMPAT, "Low-Layer Compatibility"},
	{PROT_Q931_IE_HIGH_LAYER_COMPAT, "High-Layer Compatibility"},
	{PROT_Q931_IE_USER_USER, "User-User"},
	{PROT_Q931_IE_SENDING_COMPLETE, "Sending complete"},
	{PROT_Q931_IE_ESCAPE_FOR_EXTENSION, "Escape for extension"},
	{-1,"Unknown"},
};

struct code2str dcodQ931NumberingPlanTable[] = {
	{0, "unknown"},
	{1, "isdn"},
	{3, "data"},
	{4, "telex"},
	{8, "national"},
	{9, "private"},
	{15, "reserved"},
	{-1, "invalid"},
};

struct code2str dcodQ931TypeofNumberTable[] = {
	{0, "unknown"},
	{1, "international"},
	{2, "national"},
	{3, "network spf"},
	{4, "subscriber"},
	{6, "abbreviated"},
	{7, "reserved"},
	{-1, "invalid" },
};

struct code2str dcodQ931PresentationTable[] = {
	{0, "allowed"},
	{1, "restricted"},
	{2, "not available"},
	{-1, "invalid" },
};

struct code2str dcodQ931ScreeningTable[] = {
	{0, "user, not screened"},
	{1, "user, passed"},
	{2, "user, failed"},
	{3, "network, provided"},
	{-1, "invalid" },
};

struct code2str dcodQ931InfoChannelSelTable[] = {
	{0, "No Chan"},
	{1, "B1"},
	{2, "B2"},
	{3, "Any Chan"},
	{-1, "invalid" },
};

struct code2str dcodQ931ReasonTable[] = {
	{0x0, "Unknown"},
	{0x1, "Call forwarding busy"},
	{0x2, "Call forwarding no reply"},
	{0x4, "Call deflection"},
	{0x9, "Called DTE out of order"},
	{0xA, "Call forwarding by the called DTE"},
	{0xF, "Call forwarding unconditional"},
	{-1, "reserved" },
};

struct code2str dcodQ931BcCodingStandardTable[] = {
	{0x0, "ITU-T"},
	{0x1, "ISO/IEC"},
	{0x2, "National"},
	{0x3, "Defined standard"},
	{-1, "unknown"},
};

struct code2str dcodQ931BcInfTransferCapTable[] = {
	{0x00, "Speech"},
	{0x08, "Unrestricted digital"},
	{0x09, "Restricted digital"},
	{0x10, "3.1Khz audio"},
	{0x11, "Unrestricted digital w/ tones"},
	{0x18, "Video"},
	{-1, "reserved"},	
};

struct code2str dcodQ931BcInfTransferRateTable[] = {
	{0x00, "n/a"}, /* for packet-mode calls */
	{0x10, "64 Kbit/s"},
	{0x11, "2x64 Kbit/s"},
	{0x13, "384 Kbit/s"},
	{0x15, "1536 Kbit/s"},
	{0x17, "1920 Kbit/s"},
	{0x18, "Multirate"},
	{-1, "reserved"},
};


struct code2str dcodQ931BcusrL1ProtTable[] = {
	{0x01, "ITU-T rate/V.110/I.460/X.30"},	
	{0x02, "G.711 u-Law"},
	{0x03, "G.711 A-Law"},
	{0x04, "G.721/I.460"},
	{0x05, "H.221/H.242"},
	{0x06, "H.223/H.245"},
	{0x07, "Non-ITU-T rate"},	
	{0x08, "V.120"},
	{0x09, "X.31 HDLC"},
	{-1, "reserved"},
};

struct code2str dcodQ931UuiProtDiscrTable[] = {
	{0x00, "User-specific"},
	{0x01, "OSI high layer prot"},
	{0x02, "Recommendation X.244"},
	{0x03, "System management"},
	{0x04, "IA5 Chars"},
	{0x05, "X.208/X.209"},
	{0x07, "V.120"},
	{0x08, "Q.931/I.451"},
	{0x10, "X.25"},
	{-1,"reserved"},
};

struct code2str dcodQ931ChanTypeTable[] = {
	{0x3,"B-chans"},
	{0x6,"H0-chans"},
	{0x8,"H11-chans"},
	{0x9,"H12-chans"},
	{-1,"reserved"},
};

struct code2str dcodQ931RestartIndClassTable[] = {
	{0x0 ,"Indicated in channel IE"},
	{0x6 ,"Single interface"},
	{0x7 ,"All interfaces"},
	{-1, "reserved"},
};

struct code2str dcodQ931IelocationTable[] = {
	{0x0, "User"},
	{0x1, "Private network, local user"},
	{0x2, "Public network, local user"},
	{0x3, "Transit network"},
	{0x4, "Public network, remote user"},
	{0x5, "Private network, remote user"},
	{0xA, "Beyond interworking point"},
	{-1, "reserved"},
};

struct code2str dcodQ931IeprogressDescrTable[] = {
	{0x01, "Further info maybe available"},
	{0x02, "Destination is non-ISDN"},
	{0x03, "Origination address is non-ISDN"},
	{0x04, "Call returned to ISDN"},
	{0x08, "In-band data ready"},
	{-1, "reserved"},
};

struct code2str dcodQ931IeFacilityProtProfileTable[] = {
	{0x11, "Remote Operations Protocol"},
	{0x12, "CMIP Protocol"},
	{0x13, "ACSE Protocol"},
	{0x16, "GAT Protocol"},
	{0x1F, "Networking Extensions"},
	{-1, "reserved"},
};

//from www.voip-info.org/wiki/ANI2  - NANPA
struct code2str dcodQ931LineInfoTable[] = {
	{0,  "Plain Old Telephone Service(POTS)" },
	{1,  "Multiparty line"},
	{2,  "ANI Failure"},
	{6,  "Station Level Rating"},
	{7,  "Special Operator Handling Required"},
	{20, "Automatic Identified Outward Dialing (AIOD)"},
	{23, "Coin or Non-coin"},
	{24, "Toll free service, POTS originated for non-pay station"},
	{25, "Toll free service, POTS originated for pay station"},
	{27, "Pay station with coin control"},
	{29, "Prison-Inmate service"},
	{30, "Intercept - blank"},
	{31, "Intercept - trouble"},
	{32, "Intercept - regular"},
	{34, "Telco operator handled call"},
	{52, "Outward Wide Area Telecommunications Service(OUTWATS)"},
	{60, "TRS call - from unrestricted line"},
	{61, "Cellular-Wireless PCS Type 1"},
	{62, "Cellular-Wireless PCS Type 2"},
	{63, "Cellular-Wireless PCS Type Roaming"},
	{66, "TRS call - from hotel/motel"},
	{67, "TRS call - from restricted line"},
	{70, "Line connected to pay station"},
	{93, "Private virtual network call"},
	{-1, "Unassigned"},
};


struct code2str dcodQ931GenDigitsEncodingTable[] = {
	{0, "BCD even"},
	{1, "BCD odd"},
	{2, "IA5"},
	{3, "Binary"},
	{-1, "Invalid"},
};


struct code2str dcodQ931GenDigitsTypeTable[] = {
	{ 0, "Account Code"},
	{ 1, "Auth Code"},
	{ 2, "Customer ID" },
	{ 3, "Universal Access"},
	{ 4, "Info Digits"},
	{ 5, "Callid"},
	{ 6, "Opart"},
	{ 7, "TCN"},
	{ 9, "Adin"},
	{-1, "Invalid"},
};

struct code2str dcodQ931TypeOfSubaddressTable[] = {
	{ 0x00, "NSAP"},
	{ 0x02, "User-specified"},
	{ -1,   "Invalid"},
};

struct code2str dcodQ931DisplayTypeTable[] = {
	{ 0x00, "Calling Party Name"},
	{ 0x01, "Connected Party Name"},
	{ 0x05, "Original Called Party Name"},
	{ -1,   "Invalid"},
};

struct code2str dcodQ931AssocInfoTable[] = {
	{ 0x00, "Requested"},
	{ 0x03, "Included"},
	{ -1,   "Invalid"},
};


struct code2str dcodQ931NotificationIndTable[] = {
	{ 0x71, "Call Information/event"},
	{ -1,   "Invalid"},
};
#endif /* __FTMOD_SANGOMA_ISDN_TRACE_H__ */
