/*
    KHOMP generic endpoint/channel library.
    Copyright (C) 2007-2009 Khomp Ind. & Com.

  The contents of this file are subject to the Mozilla Public License Version 1.1
  (the "License"); you may not use this file except in compliance with the
  License. You may obtain a copy of the License at http://www.mozilla.org/MPL/

  Software distributed under the License is distributed on an "AS IS" basis,
  WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for
  the specific language governing rights and limitations under the License.

  Alternatively, the contents of this file may be used under the terms of the
  "GNU Lesser General Public License 2.1" license (the “LGPL" License), in which
  case the provisions of "LGPL License" are applicable instead of those above.

  If you wish to allow use of your version of this file only under the terms of
  the LGPL License and not to allow others to use your version of this file under
  the MPL, indicate your decision by deleting the provisions above and replace them
  with the notice and other provisions required by the LGPL License. If you do not
  delete the provisions above, a recipient may use your version of this file under
  either the MPL or the LGPL License.

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
    along with this library; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include <string>
#include <sstream>
#include <map>

#include <k3l.h>

// k3lApiMajorVersion
#ifndef CM_PING
# include <k3lVersion.h>
# include <KTools.h>
#endif

#include <types.hpp>
#include <k3lapi.hpp>
#include <format.hpp>

#ifndef _VERBOSE_HPP_
#define _VERBOSE_HPP_

struct Verbose
{
    typedef enum
    {
        K_CM_SEIZE                           = CM_SEIZE,
        K_CM_SYNC_SEIZE                      = CM_SYNC_SEIZE,
#if K3L_AT_LEAST(1,6,0)
        K_CM_SIP_REGISTER                    = CM_SIP_REGISTER,
#endif
        K_CM_DIAL_DTMF                       = CM_DIAL_DTMF,
        K_CM_DISCONNECT                      = CM_DISCONNECT,
        K_CM_CONNECT                         = CM_CONNECT,
        K_CM_PRE_CONNECT                     = CM_PRE_CONNECT,
        K_CM_CAS_CHANGE_LINE_STT             = CM_CAS_CHANGE_LINE_STT,
        K_CM_CAS_SEND_MFC                    = CM_CAS_SEND_MFC,
        K_CM_SET_FORWARD_CHANNEL             = CM_SET_FORWARD_CHANNEL,
        K_CM_CAS_SET_MFC_DETECT_MODE         = CM_CAS_SET_MFC_DETECT_MODE,
        K_CM_DROP_COLLECT_CALL               = CM_DROP_COLLECT_CALL,
        K_CM_MAKE_CALL                       = CM_MAKE_CALL,
        K_CM_RINGBACK                        = CM_RINGBACK,
        K_CM_USER_INFORMATION                = CM_USER_INFORMATION,
#if !K3L_AT_LEAST(2,2,0)
        K_CM_VOIP_SEIZE                      = CM_VOIP_SEIZE, //deprecated
#endif
        K_CM_LOCK_INCOMING                   = CM_LOCK_INCOMING,
        K_CM_UNLOCK_INCOMING                 = CM_UNLOCK_INCOMING,
        K_CM_LOCK_OUTGOING                   = CM_LOCK_OUTGOING,
        K_CM_UNLOCK_OUTGOING                 = CM_UNLOCK_OUTGOING,
        K_CM_START_SEND_FAIL                 = CM_START_SEND_FAIL,
        K_CM_STOP_SEND_FAIL                  = CM_STOP_SEND_FAIL,
        K_CM_END_OF_NUMBER                   = CM_END_OF_NUMBER,
#if K3L_AT_LEAST(1,6,0)
        K_CM_SS_TRANSFER                     = CM_SS_TRANSFER,
        K_CM_GET_SMS                         = CM_GET_SMS,
        K_CM_PREPARE_SMS                     = CM_PREPARE_SMS,
        K_CM_SEND_SMS                        = CM_SEND_SMS,
#endif
#if K3L_HAS_MPTY_SUPPORT
        K_CM_HOLD_SWITCH                     = CM_HOLD_SWITCH,
        K_CM_MPTY_CONF                       = CM_MPTY_CONF,
        K_CM_MPTY_SPLIT                      = CM_MPTY_SPLIT,
#endif
        K_CM_ENABLE_DTMF_SUPPRESSION         = CM_ENABLE_DTMF_SUPPRESSION,
        K_CM_DISABLE_DTMF_SUPPRESSION        = CM_DISABLE_DTMF_SUPPRESSION,
        K_CM_ENABLE_AUDIO_EVENTS             = CM_ENABLE_AUDIO_EVENTS,
        K_CM_DISABLE_AUDIO_EVENTS            = CM_DISABLE_AUDIO_EVENTS,
        K_CM_ENABLE_CALL_PROGRESS            = CM_ENABLE_CALL_PROGRESS,
        K_CM_DISABLE_CALL_PROGRESS           = CM_DISABLE_CALL_PROGRESS,
        K_CM_FLASH                           = CM_FLASH,
        K_CM_ENABLE_PULSE_DETECTION          = CM_ENABLE_PULSE_DETECTION,
        K_CM_DISABLE_PULSE_DETECTION         = CM_DISABLE_PULSE_DETECTION,
        K_CM_ENABLE_ECHO_CANCELLER           = CM_ENABLE_ECHO_CANCELLER,
        K_CM_DISABLE_ECHO_CANCELLER          = CM_DISABLE_ECHO_CANCELLER,
        K_CM_ENABLE_AGC                      = CM_ENABLE_AGC,
        K_CM_DISABLE_AGC                     = CM_DISABLE_AGC,
        K_CM_ENABLE_HIGH_IMP_EVENTS          = CM_ENABLE_HIGH_IMP_EVENTS,
        K_CM_DISABLE_HIGH_IMP_EVENTS         = CM_DISABLE_HIGH_IMP_EVENTS,
#if K3L_AT_LEAST(1,6,0)
        K_CM_ENABLE_CALL_ANSWER_INFO         = CM_ENABLE_CALL_ANSWER_INFO,
        K_CM_DISABLE_CALL_ANSWER_INFO        = CM_DISABLE_CALL_ANSWER_INFO,
#endif
        K_CM_RESET_LINK                      = CM_RESET_LINK,
#if K3L_AT_LEAST(1,6,0)
        K_CM_CLEAR_LINK_ERROR_COUNTER        = CM_CLEAR_LINK_ERROR_COUNTER,
#endif
        K_CM_SEND_DTMF                       = CM_SEND_DTMF,
        K_CM_STOP_AUDIO                      = CM_STOP_AUDIO,
        K_CM_HARD_RESET                      = CM_HARD_RESET,
        K_CM_SEND_TO_CTBUS                   = CM_SEND_TO_CTBUS,
        K_CM_RECV_FROM_CTBUS                 = CM_RECV_FROM_CTBUS,
        K_CM_SETUP_H100                      = CM_SETUP_H100,
        K_CM_MIXER                           = CM_MIXER,
        K_CM_CLEAR_MIXER                     = CM_CLEAR_MIXER,
        K_CM_PLAY_FROM_FILE                  = CM_PLAY_FROM_FILE,
        K_CM_RECORD_TO_FILE                  = CM_RECORD_TO_FILE,
        K_CM_PLAY_FROM_STREAM                = CM_PLAY_FROM_STREAM,
        K_CM_INTERNAL_PLAY                   = CM_INTERNAL_PLAY,
        K_CM_STOP_PLAY                       = CM_STOP_PLAY,
        K_CM_STOP_RECORD                     = CM_STOP_RECORD,
        K_CM_PAUSE_PLAY                      = CM_PAUSE_PLAY,
        K_CM_PAUSE_RECORD                    = CM_PAUSE_RECORD,
        K_CM_RESUME_PLAY                     = CM_RESUME_PLAY,
        K_CM_RESUME_RECORD                   = CM_RESUME_RECORD,
        K_CM_INCREASE_VOLUME                 = CM_INCREASE_VOLUME,
        K_CM_DECREASE_VOLUME                 = CM_DECREASE_VOLUME,
        K_CM_LISTEN                          = CM_LISTEN,
        K_CM_STOP_LISTEN                     = CM_STOP_LISTEN,
        K_CM_PREPARE_FOR_LISTEN              = CM_PREPARE_FOR_LISTEN,
        K_CM_PLAY_SOUND_CARD                 = CM_PLAY_SOUND_CARD,
        K_CM_STOP_SOUND_CARD                 = CM_STOP_SOUND_CARD,
        K_CM_MIXER_CTBUS                     = CM_MIXER_CTBUS,
        K_CM_PLAY_FROM_STREAM_EX             = CM_PLAY_FROM_STREAM_EX,
        K_CM_INTERNAL_PLAY_EX                = CM_INTERNAL_PLAY_EX,
        K_CM_ENABLE_PLAYER_AGC               = CM_ENABLE_PLAYER_AGC,
        K_CM_DISABLE_PLAYER_AGC              = CM_DISABLE_PLAYER_AGC,
        K_CM_START_STREAM_BUFFER             = CM_START_STREAM_BUFFER,
        K_CM_ADD_STREAM_BUFFER               = CM_ADD_STREAM_BUFFER,
        K_CM_STOP_STREAM_BUFFER              = CM_STOP_STREAM_BUFFER,
        K_CM_SEND_BEEP                       = CM_SEND_BEEP,
        K_CM_SEND_BEEP_CONF                  = CM_SEND_BEEP_CONF,
        K_CM_ADD_TO_CONF                     = CM_ADD_TO_CONF,
        K_CM_REMOVE_FROM_CONF                = CM_REMOVE_FROM_CONF,
        K_CM_RECORD_TO_FILE_EX               = CM_RECORD_TO_FILE_EX,
        K_CM_SET_VOLUME                      = CM_SET_VOLUME,
        K_CM_SET_LINE_CONDITION              = CM_SET_LINE_CONDITION,
        K_CM_SEND_LINE_CONDITION             = CM_SEND_LINE_CONDITION,
        K_CM_SET_CALLER_CATEGORY             = CM_SET_CALLER_CATEGORY,
        K_CM_DIAL_MFC                        = CM_DIAL_MFC,
#if !K3L_AT_LEAST(2,0,0)
        K_CM_VOIP_START_DEBUG                = CM_VOIP_START_DEBUG,
        K_CM_VOIP_STOP_DEBUG                 = CM_VOIP_STOP_DEBUG,
        K_CM_VOIP_DUMP_STAT                  = CM_VOIP_DUMP_STAT,
        K_CM_ISDN_DEBUG                      = CM_ISDN_DEBUG,
        K_CM_PING                            = CM_PING,
        K_CM_LOG_REQUEST                     = CM_LOG_REQUEST,
        K_CM_LOG_CREATE_DISPATCHER           = CM_LOG_CREATE_DISPATCHER,
        K_CM_LOG_DESTROY_DISPATCHER          = CM_LOG_DESTROY_DISPATCHER,
#endif
#if K3L_AT_LEAST(1,6,0)
        K_CM_START_CADENCE                   = CM_START_CADENCE,
        K_CM_STOP_CADENCE                    = CM_STOP_CADENCE,
        K_CM_CHECK_NEW_SMS                   = CM_CHECK_NEW_SMS,
        K_CM_SEND_TO_MODEM                   = CM_SEND_TO_MODEM,
#endif
#if K3L_AT_LEAST(2,1,0)
        K_CM_START_FAX_TX                    = CM_START_FAX_TX,
        K_CM_STOP_FAX_TX                     = CM_STOP_FAX_TX,
        K_CM_ADD_FAX_FILE                    = CM_ADD_FAX_FILE,
        K_CM_ADD_FAX_PAGE_BREAK              = CM_ADD_FAX_PAGE_BREAK,
        K_CM_START_FAX_RX                    = CM_START_FAX_RX,
        K_CM_STOP_FAX_RX                     = CM_STOP_FAX_RX,
        K_CM_SIM_CARD_SELECT                 = CM_SIM_CARD_SELECT,
#endif

#if K3L_AT_LEAST(2,1,0)
        K_CM_NOTIFY_WATCHDOG                = CM_NOTIFY_WATCHDOG,
        K_CM_STOP_WATCHDOG                  = CM_STOP_WATCHDOG,
        K_CM_WATCHDOG_COUNT                 = CM_WATCHDOG_COUNT,
        K_CM_START_WATCHDOG                 = CM_START_WATCHDOG,
#endif

    }
    kcommand;

    typedef enum
    {
        K_EV_CHANNEL_FREE                    = EV_CHANNEL_FREE,
        K_EV_CONNECT                         = EV_CONNECT,
        K_EV_DISCONNECT                      = EV_DISCONNECT,
        K_EV_CALL_SUCCESS                    = EV_CALL_SUCCESS,
        K_EV_CALL_FAIL                       = EV_CALL_FAIL,
        K_EV_NO_ANSWER                       = EV_NO_ANSWER,
        K_EV_BILLING_PULSE                   = EV_BILLING_PULSE,
        K_EV_SEIZE_SUCCESS                   = EV_SEIZE_SUCCESS,
        K_EV_SEIZE_FAIL                      = EV_SEIZE_FAIL,
        K_EV_SEIZURE_START                   = EV_SEIZURE_START,
        K_EV_CAS_LINE_STT_CHANGED            = EV_CAS_LINE_STT_CHANGED,
        K_EV_CAS_MFC_RECV                    = EV_CAS_MFC_RECV,
        K_EV_NEW_CALL                        = EV_NEW_CALL,
        K_EV_USER_INFORMATION                = EV_USER_INFORMATION,
        K_EV_DIALED_DIGIT                    = EV_DIALED_DIGIT,
#if K3L_AT_LEAST(1,6,0)
        K_EV_SIP_REGISTER_INFO               = EV_SIP_REGISTER_INFO,
        K_EV_RING_DETECTED                   = EV_RING_DETECTED,
#endif
        K_EV_CALL_HOLD_START                 = EV_CALL_HOLD_START,
        K_EV_CALL_HOLD_STOP                  = EV_CALL_HOLD_STOP,
#if K3L_AT_LEAST(1,6,0)
        K_EV_SS_TRANSFER_FAIL                = EV_SS_TRANSFER_FAIL,
        K_EV_FLASH                           = EV_FLASH,
#endif
        K_EV_DTMF_DETECTED                   = EV_DTMF_DETECTED,
        K_EV_DTMF_SEND_FINISH                = EV_DTMF_SEND_FINISH,
        K_EV_AUDIO_STATUS                    = EV_AUDIO_STATUS,
        K_EV_CADENCE_RECOGNIZED              = EV_CADENCE_RECOGNIZED,
        K_EV_END_OF_STREAM                   = EV_END_OF_STREAM,
        K_EV_PULSE_DETECTED                  = EV_PULSE_DETECTED,
        K_EV_POLARITY_REVERSAL               = EV_POLARITY_REVERSAL,
#if K3L_AT_LEAST(1,6,0)
        K_EV_ISDN_PROGRESS_INDICATOR         = EV_ISDN_PROGRESS_INDICATOR,
        K_EV_CALL_ANSWER_INFO                = EV_CALL_ANSWER_INFO,
        K_EV_COLLECT_CALL                    = EV_COLLECT_CALL,
        K_EV_SIP_DTMF_DETECTED               = EV_SIP_DTMF_DETECTED,
        K_EV_RECV_FROM_MODEM                 = EV_RECV_FROM_MODEM,
        K_EV_NEW_SMS                         = EV_NEW_SMS,
        K_EV_SMS_INFO                        = EV_SMS_INFO,
        K_EV_SMS_DATA                        = EV_SMS_DATA,
        K_EV_SMS_SEND_RESULT                 = EV_SMS_SEND_RESULT,
#endif
#if K3L_HAS_MPTY_SUPPORT
        K_EV_CALL_MPTY_START                 = EV_CALL_MPTY_START,
        K_EV_CALL_MPTY_STOP                  = EV_CALL_MPTY_STOP,
        K_EV_GSM_COMMAND_STATUS              = EV_GSM_COMMAND_STATUS,
#endif
        K_EV_CHANNEL_FAIL                    = EV_CHANNEL_FAIL,
        K_EV_REFERENCE_FAIL                  = EV_REFERENCE_FAIL,
        K_EV_INTERNAL_FAIL                   = EV_INTERNAL_FAIL,
        K_EV_HARDWARE_FAIL                   = EV_HARDWARE_FAIL,
        K_EV_LINK_STATUS                     = EV_LINK_STATUS,
#if K3L_AT_LEAST(1,6,0)
        K_EV_PHYSICAL_LINK_UP                = EV_PHYSICAL_LINK_UP,
        K_EV_PHYSICAL_LINK_DOWN              = EV_PHYSICAL_LINK_DOWN,
#endif
        K_EV_CLIENT_RECONNECT                = EV_CLIENT_RECONNECT,
        K_EV_VOIP_SEIZURE                    = EV_VOIP_SEIZURE,
        K_EV_SEIZURE                         = EV_SEIZURE,

#if K3L_AT_LEAST(2,1,0)
        K_EV_FAX_CHANNEL_FREE                = EV_FAX_CHANNEL_FREE,
        K_EV_FAX_FILE_SENT                   = EV_FAX_FILE_SENT,
        K_EV_FAX_FILE_FAIL                   = EV_FAX_FILE_FAIL,
        K_EV_FAX_MESSAGE_CONFIRMATION        = EV_FAX_MESSAGE_CONFIRMATION,
        K_EV_FAX_TX_TIMEOUT                  = EV_FAX_TX_TIMEOUT,
        K_EV_FAX_PAGE_CONFIRMATION           = EV_FAX_PAGE_CONFIRMATION,
        K_EV_FAX_REMOTE_INFO                 = EV_FAX_REMOTE_INFO,
#endif

#if K3L_AT_LEAST(2,1,0)
        K_EV_WATCHDOG_COUNT                 = EV_WATCHDOG_COUNT,
#endif

#if !K3L_AT_LEAST(2,0,0)
        K_EV_PONG                            = EV_PONG,
#endif
    }
    kevent;

    typedef enum
    {
        R2_COUNTRY_ARG = 1,
        R2_COUNTRY_BRA = 2,
        R2_COUNTRY_CHI = 3,
        R2_COUNTRY_MEX = 4,
        R2_COUNTRY_URY = 5,
        R2_COUNTRY_VEN = 6
    }
    R2CountryType;

    typedef enum
    {
        HUMAN,
        EXACT
    }
    Presentation;

    /* dynamic (object) stuff */

    Verbose(K3LAPI & api): _api(api) {};

#if K3L_AT_LEAST(2,0,0)
    std::string event(int32, K3L_EVENT*, R2CountryType r2_country = R2_COUNTRY_BRA,
        Presentation fmt = HUMAN);
#else
    std::string event(int32, K3L_EVENT*, Presentation fmt = HUMAN);
#endif

    std::string channelStatus(int32, int32, int32, Presentation fmt = HUMAN);

    /* end of dynamic (object) stuff */

 protected:
    K3LAPI & _api;

    /* used internally */
    struct internal_not_found {};

 public:

    /* static (class) stuff */

    static std::string echoLocation(KEchoLocation, Presentation fmt = HUMAN);
    static std::string echoCancellerConfig(KEchoCancellerConfig, Presentation fmt = HUMAN);

#if K3L_AT_LEAST(2,0,0)
    static std::string event(KSignaling, int32, K3L_EVENT*, R2CountryType = R2_COUNTRY_BRA,
        Presentation fmt = HUMAN);
#else
    static std::string event(KSignaling, int32, K3L_EVENT*, Presentation fmt = HUMAN);
#endif

#if K3L_AT_LEAST(2,0,0)
    static std::string command(int32, K3L_COMMAND*, R2CountryType = R2_COUNTRY_BRA,
         Presentation fmt = HUMAN);
    static std::string command(int32, int32, int32, const char *, R2CountryType = R2_COUNTRY_BRA,
         Presentation fmt = HUMAN);
#else
    static std::string command(int32, K3L_COMMAND*, Presentation fmt = HUMAN);
    static std::string command(int32, int32, int32, const char *, Presentation fmt = HUMAN);
#endif


    static std::string deviceName(KDeviceType, int32, Presentation fmt = HUMAN);

    static std::string deviceType(KDeviceType, Presentation fmt = HUMAN);
    static std::string deviceModel(KDeviceType, int32, Presentation fmt = HUMAN);

    static std::string channelFeatures(int32, Presentation fmt = HUMAN);
    static std::string signaling(KSignaling, Presentation fmt = HUMAN);
    static std::string systemObject(KSystemObject, Presentation fmt = HUMAN);
    static std::string mixerTone(KMixerTone, Presentation fmt = HUMAN);
    static std::string mixerSource(KMixerSource, Presentation fmt = HUMAN);

    static std::string seizeFail(KSeizeFail, Presentation fmt = HUMAN);

#if K3L_AT_LEAST(2,0,0)
    static std::string callFail(KSignaling, R2CountryType, int32, Presentation fmt = HUMAN);
#else
    static std::string callFail(KSignaling, int32, Presentation fmt = HUMAN);
#endif

    static std::string channelFail(KSignaling, int32, Presentation fmt = HUMAN);
    static std::string internalFail(KInternalFail, Presentation fmt = HUMAN);

    static std::string linkErrorCounter(KLinkErrorCounter, Presentation fmt = HUMAN);

    static std::string linkStatus(KSignaling, int32, Presentation fmt = HUMAN);
    static std::string channelStatus(KSignaling, int32, Presentation fmt = HUMAN);
    static std::string callStatus(KCallStatus, Presentation fmt = HUMAN);
    static std::string status(KLibraryStatus, Presentation fmt = HUMAN);

#if K3L_AT_LEAST(2,0,0)
    static std::string signGroupB(KSignGroupB, R2CountryType contry = R2_COUNTRY_BRA,
        Presentation fmt = HUMAN);
#else
    static std::string signGroupB(KSignGroupB, Presentation fmt = HUMAN);
#endif

#if K3L_AT_LEAST(2,0,0)
    static std::string signGroupII(KSignGroupII, R2CountryType contry = R2_COUNTRY_BRA,
        Presentation fmt = HUMAN);
#else
    static std::string signGroupII(KSignGroupII, Presentation fmt = HUMAN);
#endif

    static std::string h100configIndex(KH100ConfigIndex, Presentation fmt = HUMAN);

    static std::string eventName(int32);
    static std::string commandName(int32);

#if K3L_AT_LEAST(1,5,0)
    static std::string sipFailures(KSIP_Failures, Presentation fmt = HUMAN);
#endif

#if K3L_AT_LEAST(1,5,1)
    static std::string isdnCause(KQ931Cause, Presentation fmt = HUMAN);
#endif

#if K3L_AT_LEAST(1,5,2)
    static std::string isdnDebug(int32, Presentation fmt = HUMAN);
#endif

#if K3L_AT_LEAST(1,6,0)
    static std::string callStartInfo(KCallStartInfo, Presentation fmt = HUMAN);

    static std::string gsmCallCause(KGsmCallCause, Presentation fmt = HUMAN);
    static std::string gsmMobileCause(KGsmMobileCause, Presentation fmt = HUMAN);
    static std::string gsmSmsCause(KGsmSmsCause, Presentation fmt = HUMAN);

    static std::string q931ProgressIndication(KQ931ProgressIndication,
        Presentation fmt = HUMAN);
#endif

#if K3L_AT_LEAST(2,1,0)
    static std::string faxResult(KFaxResult code, Presentation fmt = HUMAN);
    static std::string faxFileErrorCause(KFaxFileErrorCause code, Presentation fmt = HUMAN);
#endif

    /* end of static (class) stuff */

 private:
    static std::string internal_deviceType(KDeviceType);
    static std::string internal_deviceModel(KDeviceType, int32);

#if K3L_AT_LEAST(1,5,0)
    static std::string internal_sipFailures(KSIP_Failures, Presentation fmt = HUMAN);
#endif
#if K3L_AT_LEAST(1,5,1)
    static std::string internal_isdnCause(KQ931Cause, Presentation fmt = HUMAN);
#endif

#if K3L_AT_LEAST(2,0,0)
    static std::string internal_signGroupB(KSignGroupB, R2CountryType contry, Presentation fmt = HUMAN);
    static std::string internal_signGroupII(KSignGroupII, R2CountryType contry, Presentation fmt = HUMAN);
#else
    static std::string internal_signGroupB(KSignGroupB, Presentation fmt = HUMAN);
    static std::string internal_signGroupII(KSignGroupII, Presentation fmt = HUMAN);
#endif

#if K3L_AT_LEAST(1,6,0)
    static std::string internal_gsmCallCause(KGsmCallCause, Presentation fmt = HUMAN);
    static std::string internal_gsmMobileCause(KGsmMobileCause, Presentation fmt = HUMAN);
    static std::string internal_gsmSmsCause(KGsmSmsCause, Presentation fmt = HUMAN);

    static std::string internal_q931ProgressIndication(KQ931ProgressIndication, Presentation fmt = HUMAN);
#endif

 private:
    enum Type
    {
        DEVICE,
        CHANNEL,
        PLAYER,
        MIXER,
        LINK,
        NONE
    };

    struct Target
    {
        Target(Type _type)
        : type(_type), device(-1), object(-1)
        {};

        Target(Type _type, int32 _device)
        : type(_type), device(_device), object(-1)
        {};

        Target(Type _type, int32 _device, int32 _object)
        : type(_type), device(_device), object(_object)
        {};

        Type  type;
        int32 device;
        int32 object;
    };

    static void generate(std::string &, std::string &, Target, std::string &);

    static std::string show(std::string &, std::string, Target, std::string &);
    static std::string show(std::string &, std::string, Target);

    template < typename ReturnType >
    static ReturnType presentation(Presentation fmt, ReturnType str_exact, ReturnType str_human)
    {
        switch (fmt)
        {
            case HUMAN: return str_human;
            case EXACT: return str_exact;
        };

        return str_exact;
    }
};

#endif /* _VERBOSE_HPP_ */
