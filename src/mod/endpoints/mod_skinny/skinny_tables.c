/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2010, Mathieu Parent <math.parent@gmail.com>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Mathieu Parent <math.parent@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Mathieu Parent <math.parent@gmail.com>
 *
 *
 * skinny_tables.c -- Skinny Call Control Protocol (SCCP) Endpoint Module
 *
 */

#include "skinny_protocol.h"
#include "skinny_tables.h"

/* Translation tables */
struct skinny_table SKINNY_MESSAGE_TYPES[] = {
	{KEEP_ALIVE_MESSAGE, "KeepAliveMessage"},
	{REGISTER_MESSAGE, "RegisterMessage"},
	{PORT_MESSAGE, "PortMessage"},
	{KEYPAD_BUTTON_MESSAGE, "KeypadButtonMessage"},
	{ENBLOC_CALL_MESSAGE, "EnblocCallMessage"},
	{STIMULUS_MESSAGE, "StimulusMessage"},
	{OFF_HOOK_MESSAGE, "OffHookMessage"},
	{ON_HOOK_MESSAGE, "OnHookMessage"},
	{FORWARD_STAT_REQ_MESSAGE, "ForwardStatReqMessage"},
	{SPEED_DIAL_STAT_REQ_MESSAGE, "SpeedDialStatReqMessage"},
	{LINE_STAT_REQ_MESSAGE, "LineStatReqMessage"},
	{CONFIG_STAT_REQ_MESSAGE, "ConfigStatReqMessage"},
	{TIME_DATE_REQ_MESSAGE, "TimeDateReqMessage"},
	{BUTTON_TEMPLATE_REQ_MESSAGE, "ButtonTemplateReqMessage"},
	{VERSION_REQ_MESSAGE, "VersionReqMessage"},
	{CAPABILITIES_RES_MESSAGE, "CapabilitiesReqMessage"},
	{ALARM_MESSAGE, "AlarmMessage"},
	{OPEN_RECEIVE_CHANNEL_ACK_MESSAGE, "OpenReceiveChannelAckMessage"},
	{SOFT_KEY_SET_REQ_MESSAGE, "SoftKeySetReqMessage"},
	{SOFT_KEY_EVENT_MESSAGE, "SoftKeyEventMessage"},
	{UNREGISTER_MESSAGE, "UnregisterMessage"},
	{SOFT_KEY_TEMPLATE_REQ_MESSAGE, "SoftKeyTemplateReqMessage"},
	{HEADSET_STATUS_MESSAGE, "HeadsetStatusMessage"},
	{REGISTER_AVAILABLE_LINES_MESSAGE, "RegisterAvailableLinesMessage"},
	{DEVICE_TO_USER_DATA_MESSAGE, "DeviceToUserDataMessage"},
	{DEVICE_TO_USER_DATA_RESPONSE_MESSAGE, "DeviceToUserDataResponseMessage"},
	{UPDATE_CAPABILITIES_MESSAGE, "DeviceUpdateCapabilities"},
	{SERVICE_URL_STAT_REQ_MESSAGE, "ServiceUrlStatReqMessage"},
	{FEATURE_STAT_REQ_MESSAGE, "FeatureStatReqMessage"},
	{DEVICE_TO_USER_DATA_VERSION1_MESSAGE, "DeviceToUserDataVersion1Message"},
	{DEVICE_TO_USER_DATA_RESPONSE_VERSION1_MESSAGE, "DeviceToUserDataResponseVersion1Message"},
	{DIALED_PHONE_BOOK_MESSAGE, "DialedPhoneBookMessage"},
	{ACCESSORY_STATUS_MESSAGE, "AccessoryStatusMessage"},
	{REGISTER_ACK_MESSAGE, "RegisterAckMessage"},
	{START_TONE_MESSAGE, "StartToneMessage"},
	{STOP_TONE_MESSAGE, "StopToneMessage"},
	{SET_RINGER_MESSAGE, "SetRingerMessage"},
	{SET_LAMP_MESSAGE, "SetLampMessage"},
	{SET_SPEAKER_MODE_MESSAGE, "SetSpeakerModeMessage"},
	{START_MEDIA_TRANSMISSION_MESSAGE, "StartMediaTransmissionMessage"},
	{STOP_MEDIA_TRANSMISSION_MESSAGE, "StopMediaTransmissionMessage"},
	{CALL_INFO_MESSAGE, "CallInfoMessage"},
	{FORWARD_STAT_MESSAGE, "ForwardStatMessage"},
	{SPEED_DIAL_STAT_RES_MESSAGE, "SpeedDialStatResMessage"},
	{LINE_STAT_RES_MESSAGE, "LineStatResMessage"},
	{CONFIG_STAT_RES_MESSAGE, "ConfigStatResMessage"},
	{DEFINE_TIME_DATE_MESSAGE, "DefineTimeDateMessage"},
	{BUTTON_TEMPLATE_RES_MESSAGE, "ButtonTemplateResMessage"},
	{VERSION_MESSAGE, "VersionMessage"},
	{CAPABILITIES_REQ_MESSAGE, "CapabilitiesReqMessage"},
	{SERVER_REQ_MESSAGE, "Server Request Message"},
	{REGISTER_REJECT_MESSAGE, "RegisterRejectMessage"},
	{SERVER_RESPONSE_MESSAGE, "ServerResponseMessage"},
	{RESET_MESSAGE, "ResetMessage"},
	{KEEP_ALIVE_ACK_MESSAGE, "KeepAliveAckMessage"},
	{OPEN_RECEIVE_CHANNEL_MESSAGE, "OpenReceiveChannelMessage"},
	{CLOSE_RECEIVE_CHANNEL_MESSAGE, "CloseReceiveChannelMessage"},
	{SOFT_KEY_TEMPLATE_RES_MESSAGE, "SoftKeyTemplateResMessage"},
	{SOFT_KEY_SET_RES_MESSAGE, "SoftKeySetResMessage"},
	{SELECT_SOFT_KEYS_MESSAGE, "SelectSoftKeysMessage"},
	{CALL_STATE_MESSAGE, "CallStateMessage"},
	{DISPLAY_PROMPT_STATUS_MESSAGE, "DisplayPromptStatusMessage"},
	{CLEAR_PROMPT_STATUS_MESSAGE, "ClearPromptStatusMessage"},
	{ACTIVATE_CALL_PLANE_MESSAGE, "ActivateCallPlaneMessage"},
	{UNREGISTER_ACK_MESSAGE, "UnregisterAckMessage"},
	{BACK_SPACE_REQ_MESSAGE, "BackSpaceReqMessage"},
	{DIALED_NUMBER_MESSAGE, "DialedNumberMessage"},
	{USER_TO_DEVICE_DATA_MESSAGE, "UserToDeviceDataMessage"},
	{FEATURE_STAT_RES_MESSAGE, "FeatureResMessage"},
	{DISPLAY_PRI_NOTIFY_MESSAGE, "DisplayPriNotifyMessage"},
	{SERVICE_URL_STAT_RES_MESSAGE, "ServiceUrlStatMessage"},
	{USER_TO_DEVICE_DATA_VERSION1_MESSAGE, "UserToDeviceDataVersion1Message"},
	{DIALED_PHONE_BOOK_ACK_MESSAGE, "DialedPhoneBookAckMessage"},
	{XML_ALARM_MESSAGE, "XMLAlarmMessage"},
	{0, NULL}
};
	SKINNY_DECLARE_ID2STR(skinny_message_type2str, SKINNY_MESSAGE_TYPES, "UnknownMessage")
SKINNY_DECLARE_STR2ID(skinny_str2message_type, SKINNY_MESSAGE_TYPES, -1)

	struct skinny_table SKINNY_DEVICE_TYPES[] = {
		{1, "Cisco 30 SP+"},
		{2, "Cisco 12 SP+"},
		{3, "Cisco 12 SP"},
		{4, "Cisco 12 S"},
		{5, "Cisco 30 VIP"},
		{6, "Cisco 7910"},
		{7, "Cisco 7960"},
		{8, "Cisco 7940"},
		{9, "Cisco 7935"},
		{10, "Cisco VGC Phone"},
		{11, "Cisco VGC Virtual Phone"},
		{12, "Cisco ATA 186"},
		{30, "Cisco Analog Access"},
		{40, "Cisco Digital Access"},
		{42, "Cisco Digital Access+"},
		{43, "Cisco Digital Access WS-X6608"},
		{47, "Cisco Analog Access WS-X6624"},
		{51, "Cisco Conference Bridge WS-X6608"},
		{61, "Cisco H.323 Phone"},
		{100, "Cisco Load Simulator"},
		{111, "Cisco Media Termination Point Hardware"},
		{115, "Cisco 7941"},
		{115, "Cisco CP-7941G"},
		{119, "Cisco 7971"},
		{120, "Cisco MGCP Station"},
		{121, "Cisco MGCP Trunk"},
		{124, "Cisco 7914 14-Button Line Expansion Module"},
		{302, "Cisco 7985"},
		{307, "Cisco 7911"},
		{308, "Cisco 7961G-GE"},
		{309, "Cisco 7941G-GE"},
		{335, "Cisco Motorola CN622"},
		{348, "Cisco 7931"},
		{358, "Cisco Unified Personal Communicator"},
		{365, "Cisco 7921"},
		{369, "Cisco 7906"},
		{375, "Cisco TelePresence"},
		{404, "Cisco 7962"},
		{412, "Cisco 3951"},
		{431, "Cisco 7937"},
		{434, "Cisco 7942"},
		{435, "Cisco 7945"},
		{436, "Cisco 7965"},
		{437, "Cisco 7975"},
		{446, "Cisco 3911"},
		{20000, "Cisco 7905"},
		{30002, "Cisco 7920"},
		{30006, "Cisco 7970"},
		{30007, "Cisco 7912"},
		{30008, "Cisco 7902"},
		{30018, "Cisco 7961"},
		{30019, "Cisco 7936"},
		{30027, "Cisco Analog Phone"},
		{30028, "Cisco ISDN BRI Phone"},
		{30032, "Cisco SCCP gateway virtual phone"},
		{30035, "Cisco IP-STE"},
		{0, NULL}
	};
	SKINNY_DECLARE_ID2STR(skinny_device_type2str, SKINNY_DEVICE_TYPES, "UnknownDeviceType")
SKINNY_DECLARE_STR2ID(skinny_str2device_type, SKINNY_DEVICE_TYPES, -1)

	struct skinny_table SKINNY_TONS[] = {
		{SKINNY_TONE_SILENCE, "Silence"},
		{SKINNY_TONE_DIALTONE, "DialTone"},
		{SKINNY_TONE_BUSYTONE, "BusyTone"},
		{SKINNY_TONE_ALERT, "Alert"},
		{SKINNY_TONE_REORDER, "Reorder"},
		{SKINNY_TONE_CALLWAITTONE, "CallWaitTone"},
		{SKINNY_TONE_NOTONE, "NoTone"},
		{0, NULL}
	};
	SKINNY_DECLARE_ID2STR(skinny_tone2str, SKINNY_TONS, "UnknownTone")
SKINNY_DECLARE_STR2ID(skinny_str2tone, SKINNY_TONS, -1)

	struct skinny_table SKINNY_RING_TYPES[] = {
		{SKINNY_RING_OFF, "RingOff"},
		{SKINNY_RING_INSIDE, "RingInside"},
		{SKINNY_RING_OUTSIDE, "RingOutside"},
		{SKINNY_RING_FEATURE, "RingFeature"},
		{0, NULL}
	};
	SKINNY_DECLARE_ID2STR(skinny_ring_type2str, SKINNY_RING_TYPES, "RingTypeUnknown")
SKINNY_DECLARE_STR2ID(skinny_str2ring_type, SKINNY_RING_TYPES, -1)

	struct skinny_table SKINNY_RING_MODES[] = {
		{SKINNY_RING_FOREVER, "RingForever"},
		{SKINNY_RING_ONCE, "RingOnce"},
		{0, NULL}
	};
	SKINNY_DECLARE_ID2STR(skinny_ring_mode2str, SKINNY_RING_MODES, "RingModeUnknown")
SKINNY_DECLARE_STR2ID(skinny_str2ring_mode, SKINNY_RING_MODES, -1)

	struct skinny_table SKINNY_BUTTONS[] = {
		{SKINNY_BUTTON_UNKNOWN, "Unknown"},
		{SKINNY_BUTTON_LAST_NUMBER_REDIAL, "LastNumberRedial"},
		{SKINNY_BUTTON_SPEED_DIAL, "SpeedDial"},
		{SKINNY_BUTTON_HOLD, "Hold"},
		{SKINNY_BUTTON_FORWARDALL, "ForwardAll"},
		{SKINNY_BUTTON_TRANSFER, "Transfer"},
		{SKINNY_BUTTON_LINE, "Line"},
		{SKINNY_BUTTON_VOICEMAIL, "Voicemail"},
		{SKINNY_BUTTON_PRIVACY, "Privacy"},
		{SKINNY_BUTTON_SERVICE_URL, "ServiceUrl"},
		{SKINNY_BUTTON_UNDEFINED, "Undefined"},
		{0, NULL}
	};
	SKINNY_DECLARE_ID2STR(skinny_button2str, SKINNY_BUTTONS, "Unknown")
SKINNY_DECLARE_STR2ID(skinny_str2button, SKINNY_BUTTONS, -1)

	struct skinny_table SKINNY_SOFT_KEY_EVENTS[] = {
		{SOFTKEY_REDIAL, "SoftkeyRedial"},
		{SOFTKEY_NEWCALL, "SoftkeyNewcall"},
		{SOFTKEY_HOLD, "SoftkeyHold"},
		{SOFTKEY_TRANSFER, "SoftkeyTransfer"},
		{SOFTKEY_CFWDALL, "SoftkeyCfwdall"},
		{SOFTKEY_CFWDBUSY, "SoftkeyCfwdbusy"},
		{SOFTKEY_CFWDNOANSWER, "SoftkeyCfwdnoanswer"},
		{SOFTKEY_BACKSPACE, "SoftkeyBackspace"},
		{SOFTKEY_ENDCALL, "SoftkeyEndcall"},
		{SOFTKEY_RESUME, "SoftkeyResume"},
		{SOFTKEY_ANSWER , "SoftkeyAnswer"},
		{SOFTKEY_INFO, "SoftkeyInfo"},
		{SOFTKEY_CONF, "SoftkeyConf"},
		{SOFTKEY_PARK, "SoftkeyPark"},
		{SOFTKEY_JOIN, "SoftkeyJoin"},
		{SOFTKEY_MEETME, "SoftkeyMeetme"},
		{SOFTKEY_CALLPICKUP, "SoftkeyCallpickup"},
		{SOFTKEY_GRPCALLPICKUP, "SoftkeyGrpcallpickup"},
		{SOFTKEY_DND, "SoftkeyDnd"},
		{SOFTKEY_IDIVERT, "SoftkeyIdivert"},
		{0, NULL}
	};
	SKINNY_DECLARE_ID2STR(skinny_soft_key_event2str, SKINNY_SOFT_KEY_EVENTS, "SoftkeyUnknown")
SKINNY_DECLARE_STR2ID(skinny_str2soft_key_event, SKINNY_SOFT_KEY_EVENTS, 0)

	struct skinny_table SKINNY_LAMP_MODES[] = {
		{SKINNY_LAMP_OFF, "Off"},
		{SKINNY_LAMP_ON, "On"},
		{SKINNY_LAMP_WINK, "Wink"},
		{SKINNY_LAMP_FLASH, "Flash"},
		{SKINNY_LAMP_BLINK, "Blink"},
		{0, NULL}
	};
	SKINNY_DECLARE_ID2STR(skinny_lamp_mode2str, SKINNY_LAMP_MODES, "Unknown")
SKINNY_DECLARE_STR2ID(skinny_str2lamp_mode, SKINNY_LAMP_MODES, -1)

	struct skinny_table SKINNY_SPEAKER_MODES[] = {
		{SKINNY_SPEAKER_ON, "SpeakerOn"},
		{SKINNY_SPEAKER_OFF, "SpeakerOff"},
		{0, NULL}
	};
	SKINNY_DECLARE_ID2STR(skinny_speaker_mode2str, SKINNY_SPEAKER_MODES, "Unknown")
SKINNY_DECLARE_STR2ID(skinny_str2speaker_mode, SKINNY_SPEAKER_MODES, -1)

	struct skinny_table SKINNY_KEY_SETS[] = {
		{SKINNY_KEY_SET_ON_HOOK, "KeySetOnHook"},
		{SKINNY_KEY_SET_CONNECTED, "KeySetConnected"},
		{SKINNY_KEY_SET_ON_HOLD, "KeySetOnHold"},
		{SKINNY_KEY_SET_RING_IN, "KeySetRingIn"},
		{SKINNY_KEY_SET_OFF_HOOK, "KeySetOffHook"},
		{SKINNY_KEY_SET_CONNECTED_WITH_TRANSFER, "KeySetConnectedWithTransfer"},
		{SKINNY_KEY_SET_DIGITS_AFTER_DIALING_FIRST_DIGIT, "KeySetDigitsAfterDialingFirstDigit"},
		{SKINNY_KEY_SET_CONNECTED_WITH_CONFERENCE, "KeySetConnectedWithConference"},
		{SKINNY_KEY_SET_RING_OUT, "KeySetRingOut"},
		{SKINNY_KEY_SET_OFF_HOOK_WITH_FEATURES, "KeySetOffHookWithFeatures"},
		{SKINNY_KEY_SET_IN_USE_HINT, "KeySetInUseHint"},
		{0, NULL}
	};
	SKINNY_DECLARE_ID2STR(skinny_soft_key_set2str, SKINNY_KEY_SETS, "UNKNOWN_SOFT_KEY_SET")
SKINNY_DECLARE_STR2ID(skinny_str2soft_key_set, SKINNY_KEY_SETS, -1)

	struct skinny_table SKINNY_CALL_STATES[] = {
		{SKINNY_OFF_HOOK, "OffHook"},
		{SKINNY_ON_HOOK, "OnHook"},
		{SKINNY_RING_OUT, "RingOut"},
		{SKINNY_RING_IN, "RingIn"},
		{SKINNY_CONNECTED, "Connected"},
		{SKINNY_BUSY, "Busy"},
		{SKINNY_LINE_IN_USE, "LineInUse"},
		{SKINNY_HOLD, "Hold"},
		{SKINNY_CALL_WAITING, "CallWaiting"},
		{SKINNY_CALL_TRANSFER, "CallTransfer"},
		{SKINNY_CALL_PARK, "CallPark"},
		{SKINNY_PROCEED, "Proceed"},
		{SKINNY_IN_USE_REMOTELY, "InUseRemotely"},
		{SKINNY_INVALID_NUMBER, "InvalidNumber"},
		{0, NULL}
	};
	SKINNY_DECLARE_ID2STR(skinny_call_state2str, SKINNY_CALL_STATES, "CallStateUnknown")
SKINNY_DECLARE_STR2ID(skinny_str2call_state, SKINNY_CALL_STATES, -1)

	struct skinny_table SKINNY_DEVICE_RESET_TYPES[] = {
		{SKINNY_DEVICE_RESET, "DeviceReset"},
		{SKINNY_DEVICE_RESTART, "DeviceRestart"},
		{0, NULL}
	};
	SKINNY_DECLARE_ID2STR(skinny_device_reset_type2str, SKINNY_DEVICE_RESET_TYPES, "DeviceResetTypeUnknown")
SKINNY_DECLARE_STR2ID(skinny_str2device_reset_type, SKINNY_DEVICE_RESET_TYPES, -1)

	struct skinny_table SKINNY_ACCESSORY_TYPES[] = {
		{SKINNY_ACCESSORY_NONE, "AccessoryNone"},
		{SKINNY_ACCESSORY_HEADSET, "Headset"},
		{SKINNY_ACCESSORY_HANDSET, "Handset"},
		{SKINNY_ACCESSORY_SPEAKER, "Speaker"},
		{0, NULL}
	};
	SKINNY_DECLARE_ID2STR(skinny_accessory_type2str, SKINNY_ACCESSORY_TYPES, "AccessoryUnknown")
SKINNY_DECLARE_STR2ID(skinny_str2accessory_type, SKINNY_ACCESSORY_TYPES, -1)

	struct skinny_table SKINNY_ACCESSORY_STATES[] = {
		{SKINNY_ACCESSORY_STATE_NONE, "AccessoryNoState"},
		{SKINNY_ACCESSORY_STATE_OFFHOOK, "OffHook"},
		{SKINNY_ACCESSORY_STATE_ONHOOK, "OnHook"},
		{0, NULL}
	};
	SKINNY_DECLARE_ID2STR(skinny_accessory_state2str, SKINNY_ACCESSORY_STATES, "AccessoryStateUnknown")
SKINNY_DECLARE_STR2ID(skinny_str2accessory_state, SKINNY_ACCESSORY_STATES, -1)

	struct skinny_table SKINNY_TEXTIDS[] = {
		{SKINNY_TEXTID_EMPTY, "Empty"},
		{SKINNY_TEXTID_REDIAL, "Redial"},
		{SKINNY_TEXTID_NEWCALL, "Newcall"},
		{SKINNY_TEXTID_HOLD, "Hold"},
		{SKINNY_TEXTID_TRANSFER, "Transfer"},
		{SKINNY_TEXTID_CFWDALL, "Cfwdall"},
		{SKINNY_TEXTID_CFWDBUSY, "Cfwdbusy"},
		{SKINNY_TEXTID_CFWDNOANSWER, "Cfwdnoanswer"},
		{SKINNY_TEXTID_BACKSPACE, "Backspace"},
		{SKINNY_TEXTID_ENDCALL, "Endcall"},
		{SKINNY_TEXTID_RESUME, "Resume"},
		{SKINNY_TEXTID_ANSWER, "Answer"},
		{SKINNY_TEXTID_INFO, "Info"},
		{SKINNY_TEXTID_CONF, "Conf"},
		{SKINNY_TEXTID_PARK, "Park"},
		{SKINNY_TEXTID_JOIN, "Join"},
		{SKINNY_TEXTID_MEETME, "Meetme"},
		{SKINNY_TEXTID_CALLPICKUP, "Callpickup"},
		{SKINNY_TEXTID_GRPCALLPICKUP, "Grpcallpickup"},
		{SKINNY_TEXTID_YOUR_CURRENT_OPTIONS, "Your Current Options"},
		{SKINNY_TEXTID_OFF_HOOK, "Off Hook"},
		{SKINNY_TEXTID_ON_HOOK, "On Hook"},
		{SKINNY_TEXTID_RING_OUT, "Ring Out"},
		{SKINNY_TEXTID_FROM, "From"},
		{SKINNY_TEXTID_CONNECTED, "Connected"},
		{SKINNY_TEXTID_BUSY, "Busy"},
		{SKINNY_TEXTID_LINE_IN_USE, "Line In Use"},
		{SKINNY_TEXTID_CALL_WAITING, "Call Waiting"},
		{SKINNY_TEXTID_CALL_TRANSFER, "Call Transfer"},
		{SKINNY_TEXTID_CALL_PARK, "Call Park"},
		{SKINNY_TEXTID_CALL_PROCEED, "Call Proceed"},
		{SKINNY_TEXTID_IN_USE_REMOTE, "In Use Remote"},
		{SKINNY_TEXTID_ENTER_NUMBER, "Enter Number"},
		{SKINNY_TEXTID_CALL_PARK_AT, "Call Park At"},
		{SKINNY_TEXTID_PRIMARY_ONLY, "Primary Only"},
		{SKINNY_TEXTID_TEMP_FAIL, "Temp Fail"},
		{SKINNY_TEXTID_YOU_HAVE_VOICEMAIL, "You Have Voicemail"},
		{SKINNY_TEXTID_FORWARDED_TO, "Forwarded To"},
		{SKINNY_TEXTID_CAN_NOT_COMPLETE_CONFERENCE, "Can Not Complete Conference"},
		{SKINNY_TEXTID_NO_CONFERENCE_BRIDGE, "No Conference Bridge"},
		{SKINNY_TEXTID_CAN_NOT_HOLD_PRIMARY_CONTROL, "Can Not Hold Primary Control"},
		{SKINNY_TEXTID_INVALID_CONFERENCE_PARTICIPANT, "Invalid Conference Participant"},
		{SKINNY_TEXTID_IN_CONFERENCE_ALREADY, "In Conference Already"},
		{SKINNY_TEXTID_NO_PARTICIPANT_INFO, "No Participant Info"},
		{SKINNY_TEXTID_EXCEED_MAXIMUM_PARTIES, "Exceed Maximum Parties"},
		{SKINNY_TEXTID_KEY_IS_NOT_ACTIVE, "Key Is Not Active"},
		{SKINNY_TEXTID_ERROR_NO_LICENSE, "Error No License"},
		{SKINNY_TEXTID_ERROR_DBCONFIG, "Error Dbconfig"},
		{SKINNY_TEXTID_ERROR_DATABASE, "Error Database"},
		{SKINNY_TEXTID_ERROR_PASS_LIMIT, "Error Pass Limit"},
		{SKINNY_TEXTID_ERROR_UNKNOWN, "Error Unknown"},
		{SKINNY_TEXTID_ERROR_MISMATCH, "Error Mismatch"},
		{SKINNY_TEXTID_CONFERENCE, "Conference"},
		{SKINNY_TEXTID_PARK_NUMBER, "Park Number"},
		{SKINNY_TEXTID_PRIVATE, "Private"},
		{SKINNY_TEXTID_NOT_ENOUGH_BANDWIDTH, "Not Enough Bandwidth"},
		{SKINNY_TEXTID_UNKNOWN_NUMBER, "Unknown Number"},
		{SKINNY_TEXTID_RMLSTC, "Rmlstc"},
		{SKINNY_TEXTID_VOICEMAIL, "Voicemail"},
		{SKINNY_TEXTID_IMMDIV, "Immdiv"},
		{SKINNY_TEXTID_INTRCPT, "Intrcpt"},
		{SKINNY_TEXTID_SETWTCH, "Setwtch"},
		{SKINNY_TEXTID_TRNSFVM, "Trnsfvm"},
		{SKINNY_TEXTID_DND, "Dnd"},
		{SKINNY_TEXTID_DIVALL, "Divall"},
		{SKINNY_TEXTID_CALLBACK, "Callback"},
		{SKINNY_TEXTID_NETWORK_CONGESTION_REROUTING, "Network Congestion Rerouting"},
		{SKINNY_TEXTID_BARGE, "Barge"},
		{SKINNY_TEXTID_FAILED_TO_SETUP_BARGE, "Failed To Setup Barge"},
		{SKINNY_TEXTID_ANOTHER_BARGE_EXISTS, "Another Barge Exists"},
		{SKINNY_TEXTID_INCOMPATIBLE_DEVICE_TYPE, "Incompatible Device Type"},
		{SKINNY_TEXTID_NO_PARK_NUMBER_AVAILABLE, "No Park Number Available"},
		{SKINNY_TEXTID_CALLPARK_REVERSION, "Callpark Reversion"},
		{SKINNY_TEXTID_SERVICE_IS_NOT_ACTIVE, "Service Is Not Active"},
		{SKINNY_TEXTID_HIGH_TRAFFIC_TRY_AGAIN_LATER, "High Traffic Try Again Later"},
		{SKINNY_TEXTID_QRT, "Qrt"},
		{SKINNY_TEXTID_MCID, "Mcid"},
		{SKINNY_TEXTID_DIRTRFR, "Dirtrfr"},
		{SKINNY_TEXTID_SELECT, "Select"},
		{SKINNY_TEXTID_CONFLIST, "Conflist"},
		{SKINNY_TEXTID_IDIVERT, "Idivert"},
		{SKINNY_TEXTID_CBARGE, "Cbarge"},
		{SKINNY_TEXTID_CAN_NOT_COMPLETE_TRANSFER, "Can Not Complete Transfer"},
		{SKINNY_TEXTID_CAN_NOT_JOIN_CALLS, "Can Not Join Calls"},
		{SKINNY_TEXTID_MCID_SUCCESSFUL, "Mcid Successful"},
		{SKINNY_TEXTID_NUMBER_NOT_CONFIGURED, "Number Not Configured"},
		{SKINNY_TEXTID_SECURITY_ERROR, "Security Error"},
		{SKINNY_TEXTID_VIDEO_BANDWIDTH_UNAVAILABLE, "Video Bandwidth Unavailable"},
		{SKINNY_TEXTID_VIDMODE, "Vidmode"},
		{SKINNY_TEXTID_MAX_CALL_DURATION_TIMEOUT, "Max Call Duration Timeout"},
		{SKINNY_TEXTID_MAX_HOLD_DURATION_TIMEOUT, "Max Hold Duration Timeout"},
		{SKINNY_TEXTID_OPICKUP, "Opickup"},
		{SKINNY_TEXTID_EXTERNAL_TRANSFER_RESTRICTED, "External Transfer Restricted"},
		{SKINNY_TEXTID_MAC_ADDRESS, "Mac Address"},
		{SKINNY_TEXTID_HOST_NAME, "Host Name"},
		{SKINNY_TEXTID_DOMAIN_NAME, "Domain Name"},
		{SKINNY_TEXTID_IP_ADDRESS, "Ip Address"},
		{SKINNY_TEXTID_SUBNET_MASK, "Subnet Mask"},
		{SKINNY_TEXTID_TFTP_SERVER_1, "Tftp Server 1"},
		{SKINNY_TEXTID_DEFAULT_ROUTER_1, "Default Router 1"},
		{SKINNY_TEXTID_DEFAULT_ROUTER_2, "Default Router 2"},
		{SKINNY_TEXTID_DEFAULT_ROUTER_3, "Default Router 3"},
		{SKINNY_TEXTID_DEFAULT_ROUTER_4, "Default Router 4"},
		{SKINNY_TEXTID_DEFAULT_ROUTER_5, "Default Router 5"},
		{SKINNY_TEXTID_DNS_SERVER_1, "Dns Server 1"},
		{SKINNY_TEXTID_DNS_SERVER_2, "Dns Server 2"},
		{SKINNY_TEXTID_DNS_SERVER_3, "Dns Server 3"},
		{SKINNY_TEXTID_DNS_SERVER_4, "Dns Server 4"},
		{SKINNY_TEXTID_DNS_SERVER_5, "Dns Server 5"},
		{SKINNY_TEXTID_OPERATIONAL_VLAN_ID, "Operational Vlan Id"},
		{SKINNY_TEXTID_ADMIN_VLAN_ID, "Admin Vlan Id"},
		{SKINNY_TEXTID_CALL_MANAGER_1, "Call Manager 1"},
		{SKINNY_TEXTID_CALL_MANAGER_2, "Call Manager 2"},
		{SKINNY_TEXTID_CALL_MANAGER_3, "Call Manager 3"},
		{SKINNY_TEXTID_CALL_MANAGER_4, "Call Manager 4"},
		{SKINNY_TEXTID_CALL_MANAGER_5, "Call Manager 5"},
		{SKINNY_TEXTID_INFORMATION_URL, "Information Url"},
		{SKINNY_TEXTID_DIRECTORIES_URL, "Directories Url"},
		{SKINNY_TEXTID_MESSAGES_URL, "Messages Url"},
		{SKINNY_TEXTID_SERVICES_URL, "Services Url"},
		{0, NULL}
	};
	SKINNY_DECLARE_ID2STR(skinny_textid2str, SKINNY_TEXTIDS, "Unknown")
SKINNY_DECLARE_STR2ID(skinny_str2textid, SKINNY_TEXTIDS, -1)

	/* For Emacs:
	 * Local Variables:
	 * mode:c
	 * indent-tabs-mode:t
	 * tab-width:4
	 * c-basic-offset:4
	 * End:
	 * For VIM:
	 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
	 */

