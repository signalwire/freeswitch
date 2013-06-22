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
	{ FORWARD_STAT_REQ_MESSAGE, "ForwardStatReqMessage"},
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
	{REGISTER_REJECT_MESSAGE, "RegisterRejectMessage"},
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
		{4, "Cisco 12"},
		{5, "Cisco 30 VIP"},
		{6, "Cisco IP Phone 7910"},
		{7, "Cisco IP Phone 7960"},
		{8, "Cisco IP Phone 7940"},
		{9, "Cisco IP Phone 7935"},
		{12, "Cisco ATA 186"},
		{365, "Cisco IP Phone CP-7921G"},
		{404, "Cisco IP Phone CP-7962G"},
		{436, "Cisco IP Phone CP-7965G"},
		{30018, "Cisco IP Phone CP-7961G"},
		{30019, "Cisco IP Phone 7936"},
		{0, NULL}
	};
	SKINNY_DECLARE_ID2STR(skinny_device_type2str, SKINNY_DEVICE_TYPES, "UnknownDeviceType")
SKINNY_DECLARE_STR2ID(skinny_str2device_type, SKINNY_DEVICE_TYPES, -1)

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
		{SOFTKEY_MEETMECONF, "SoftkeyMeetmeconfrm"},
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

	/* For Emacs:
	 * Local Variables:
	 * mode:c
	 * indent-tabs-mode:t
	 * tab-width:4
	 * c-basic-offset:4
	 * End:
	 * For VIM:
	 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
	 */

