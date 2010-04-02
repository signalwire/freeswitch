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
    {"KeepAliveMessage", KEEP_ALIVE_MESSAGE},
    {"RegisterMessage", REGISTER_MESSAGE},
    {"PortMessage", PORT_MESSAGE},
    {"KeypadButtonMessage", KEYPAD_BUTTON_MESSAGE},
    {"StimulusMessage", STIMULUS_MESSAGE},
    {"OffHookMessage", OFF_HOOK_MESSAGE},
    {"OnHookMessage", ON_HOOK_MESSAGE},
    {"SpeedDialStatReqMessage", SPEED_DIAL_STAT_REQ_MESSAGE},
    {"LineStatReqMessage", LINE_STAT_REQ_MESSAGE},
    {"ConfigStatReqMessage", CONFIG_STAT_REQ_MESSAGE},
    {"TimeDateReqMessage", TIME_DATE_REQ_MESSAGE},
    {"ButtonTemplateReqMessage", BUTTON_TEMPLATE_REQ_MESSAGE},
    {"CapabilitiesReqMessage", CAPABILITIES_RES_MESSAGE},
    {"AlarmMessage", ALARM_MESSAGE},
    {"OpenReceiveChannelAckMessage", OPEN_RECEIVE_CHANNEL_ACK_MESSAGE},
    {"SoftKeySetReqMessage", SOFT_KEY_SET_REQ_MESSAGE},
    {"SoftKeyEventMessage", SOFT_KEY_EVENT_MESSAGE},
    {"UnregisterMessage", UNREGISTER_MESSAGE},
    {"SoftKeyTemplateReqMessage", SOFT_KEY_TEMPLATE_REQ_MESSAGE},
    {"ServiceUrlStatReqMessage", SERVICE_URL_STAT_REQ_MESSAGE},
    {"FeatureStatReqMessage", FEATURE_STAT_REQ_MESSAGE},
    {"HeadsetStatusMessage", HEADSET_STATUS_MESSAGE},
    {"RegisterAvailableLinesMessage", REGISTER_AVAILABLE_LINES_MESSAGE},
    {"RegisterAckMessage", REGISTER_ACK_MESSAGE},
    {"StartToneMessage", START_TONE_MESSAGE},
    {"StopToneMessage", STOP_TONE_MESSAGE},
    {"SetRingerMessage", SET_RINGER_MESSAGE},
    {"SetLampMessage", SET_LAMP_MESSAGE},
    {"SetSpeakerModeMessage", SET_SPEAKER_MODE_MESSAGE},
    {"StartMediaTransmissionMessage", START_MEDIA_TRANSMISSION_MESSAGE},
    {"StopMediaTransmissionMessage", STOP_MEDIA_TRANSMISSION_MESSAGE},
    {"CallInfoMessage", CALL_INFO_MESSAGE},
    {"SpeedDialStatResMessage", SPEED_DIAL_STAT_RES_MESSAGE},
    {"LineStatResMessage", LINE_STAT_RES_MESSAGE},
    {"ConfigStatResMessage", CONFIG_STAT_RES_MESSAGE},
    {"DefineTimeDateMessage", DEFINE_TIME_DATE_MESSAGE},
    {"ButtonTemplateResMessage", BUTTON_TEMPLATE_RES_MESSAGE},
    {"CapabilitiesReqMessage", CAPABILITIES_REQ_MESSAGE},
    {"RegisterRejectMessage", REGISTER_REJECT_MESSAGE},
    {"ResetMessage", RESET_MESSAGE},
    {"KeepAliveAckMessage", KEEP_ALIVE_ACK_MESSAGE},
    {"OpenReceiveChannelMessage", OPEN_RECEIVE_CHANNEL_MESSAGE},
    {"CloseReceiveChannelMessage", CLOSE_RECEIVE_CHANNEL_MESSAGE},
    {"SoftKeyTemplateResMessage", SOFT_KEY_TEMPLATE_RES_MESSAGE},
    {"SoftKeySetResMessage", SOFT_KEY_SET_RES_MESSAGE},
    {"SelectSoftKeysMessage", SELECT_SOFT_KEYS_MESSAGE},
    {"CallStateMessage", CALL_STATE_MESSAGE},
    {"DisplayPromptStatusMessage", DISPLAY_PROMPT_STATUS_MESSAGE},
    {"ClearPromptStatusMessage", CLEAR_PROMPT_STATUS_MESSAGE},
    {"ActivateCallPlaneMessage", ACTIVATE_CALL_PLANE_MESSAGE},
    {"UnregisterAckMessage", UNREGISTER_ACK_MESSAGE},
    {"DialedNumberMessage", DIALED_NUMBER_MESSAGE},
    {"FeatureResMessage", FEATURE_STAT_RES_MESSAGE},
    {"DisplayPriNotifyMessage", DISPLAY_PRI_NOTIFY_MESSAGE},
    {"ServiceUrlStatMessage", SERVICE_URL_STAT_RES_MESSAGE},
    {NULL, 0}
};
SKINNY_DECLARE_ID2STR(skinny_message_type2str, SKINNY_MESSAGE_TYPES, "UnknownMessage")
SKINNY_DECLARE_STR2ID(skinny_str2message_type, SKINNY_MESSAGE_TYPES, -1)

struct skinny_table SKINNY_RING_TYPES[] = {
    {"RingOff", SKINNY_RING_OFF},
    {"RingInside", SKINNY_RING_INSIDE},
    {"RingOutside", SKINNY_RING_OUTSIDE},
    {"RingFeature", SKINNY_RING_FEATURE},
    {NULL, 0}
};
SKINNY_DECLARE_ID2STR(skinny_ring_type2str, SKINNY_RING_TYPES, "RingTypeUnknown")
SKINNY_DECLARE_STR2ID(skinny_str2ring_type, SKINNY_RING_TYPES, -1)

struct skinny_table SKINNY_RING_MODES[] = {
    {"RingForever", SKINNY_RING_FOREVER},
    {"RingOnce", SKINNY_RING_ONCE},
    {NULL, 0}
};
SKINNY_DECLARE_ID2STR(skinny_ring_mode2str, SKINNY_RING_MODES, "RingModeUnknown")
SKINNY_DECLARE_STR2ID(skinny_str2ring_mode, SKINNY_RING_MODES, -1)

struct skinny_table SKINNY_BUTTONS[] = {
    {"Unknown", SKINNY_BUTTON_UNKNOWN},
    {"LastNumberRedial", SKINNY_BUTTON_LAST_NUMBER_REDIAL},
    {"SpeedDial", SKINNY_BUTTON_SPEED_DIAL},
    {"Line", SKINNY_BUTTON_LINE},
    {"Voicemail", SKINNY_BUTTON_VOICEMAIL},
    {"Privacy", SKINNY_BUTTON_PRIVACY},
    {"ServiceUrl", SKINNY_BUTTON_SERVICE_URL},
    {"Undefined", SKINNY_BUTTON_UNDEFINED},
    {NULL, 0}
};
SKINNY_DECLARE_ID2STR(skinny_button2str, SKINNY_BUTTONS, "Unknown")
SKINNY_DECLARE_STR2ID(skinny_str2button, SKINNY_BUTTONS, -1)

struct skinny_table SKINNY_LAMP_MODES[] = {
    {"Off", SKINNY_LAMP_OFF},
    {"On", SKINNY_LAMP_ON},
    {"Wink", SKINNY_LAMP_WINK},
    {"Flash", SKINNY_LAMP_FLASH},
    {"Blink", SKINNY_LAMP_BLINK},
    {NULL, 0}
};
SKINNY_DECLARE_ID2STR(skinny_lamp_mode2str, SKINNY_LAMP_MODES, "Unknown")
SKINNY_DECLARE_STR2ID(skinny_str2lamp_mode, SKINNY_LAMP_MODES, -1)

struct skinny_table SKINNY_SPEAKER_MODES[] = {
    {"SpeakerOn", SKINNY_SPEAKER_ON},
    {"SpeakerOff", SKINNY_SPEAKER_OFF},
    {NULL, 0}
};
SKINNY_DECLARE_ID2STR(skinny_speaker_mode2str, SKINNY_SPEAKER_MODES, "Unknown")
SKINNY_DECLARE_STR2ID(skinny_str2speaker_mode, SKINNY_SPEAKER_MODES, -1)

struct skinny_table SKINNY_KEY_SETS[] = {
    {"KeySetOnHook", SKINNY_KEY_SET_ON_HOOK},
    {"KeySetConnected", SKINNY_KEY_SET_CONNECTED},
    {"KeySetOnHold", SKINNY_KEY_SET_ON_HOLD},
    {"KeySetRingIn", SKINNY_KEY_SET_RING_IN},
    {"KeySetOffHook", SKINNY_KEY_SET_OFF_HOOK},
    {"KeySetConnectedWithTransfer", SKINNY_KEY_SET_CONNECTED_WITH_TRANSFER},
    {"KeySetDigitsAfterDialingFirstDigit", SKINNY_KEY_SET_DIGITS_AFTER_DIALING_FIRST_DIGIT},
    {"KeySetConnectedWithConference", SKINNY_KEY_SET_CONNECTED_WITH_CONFERENCE},
    {"KeySetRingOut", SKINNY_KEY_SET_RING_OUT},
    {"KeySetOffHookWithFeatures", SKINNY_KEY_SET_OFF_HOOK_WITH_FEATURES},
    {NULL, 0}
};
SKINNY_DECLARE_ID2STR(skinny_soft_key_set2str, SKINNY_KEY_SETS, "UNKNOWN_SOFT_KEY_SET")
SKINNY_DECLARE_STR2ID(skinny_str2soft_key_set, SKINNY_KEY_SETS, -1)

struct skinny_table SKINNY_CALL_STATES[] = {
    {"OffHook", SKINNY_OFF_HOOK},
    {"OnHook", SKINNY_ON_HOOK},
    {"RingOut", SKINNY_RING_OUT},
    {"RingIn", SKINNY_RING_IN},
    {"Connected", SKINNY_CONNECTED},
    {"Busy", SKINNY_BUSY},
    {"LineInUse", SKINNY_LINE_IN_USE},
    {"Hold", SKINNY_HOLD},
    {"CallWaiting", SKINNY_CALL_WAITING},
    {"CallTransfer", SKINNY_CALL_TRANSFER},
    {"CallPark", SKINNY_CALL_PARK},
    {"Proceed", SKINNY_PROCEED},
    {"InUseRemotely", SKINNY_IN_USE_REMOTELY},
    {"InvalidNumber", SKINNY_INVALID_NUMBER},
    {NULL, 0}
};
SKINNY_DECLARE_ID2STR(skinny_call_state2str, SKINNY_CALL_STATES, "CallStateUnknown")
SKINNY_DECLARE_STR2ID(skinny_str2call_state, SKINNY_CALL_STATES, -1)

struct skinny_table SKINNY_DEVICE_RESET_TYPES[] = {
    {"DeviceReset", SKINNY_DEVICE_RESET},
    {"DeviceRestart", SKINNY_DEVICE_RESTART},
    {NULL, 0}
};
SKINNY_DECLARE_ID2STR(skinny_device_reset_type2str, SKINNY_DEVICE_RESET_TYPES, "DeviceResetTypeUnknown")
SKINNY_DECLARE_STR2ID(skinny_str2device_reset_type, SKINNY_DEVICE_RESET_TYPES, -1)

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

