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

#include <strings.hpp>
#include <verbose.hpp>

#define PRESENTATION_CHECK_RETURN(fmt, txtexact, txthuman) \
    { \
        switch(fmt) \
        { \
            case EXACT: return txtexact; \
            case HUMAN: return txthuman; \
        } \
        return txtexact; \
    }

/********************************************/

std::string Verbose::channelStatus(int32 dev, int32 obj, int32 cs, Verbose::Presentation fmt)
{
    try
    {
        K3L_CHANNEL_CONFIG & config = _api.channel_config(dev, obj);
        return Verbose::channelStatus(config.Signaling, cs, fmt);
    }
    catch (...)
    {
        return presentation(fmt, "<unknown>", "Unknown");
    }
}

#if K3L_AT_LEAST(2,0,0)
std::string Verbose::event(int32 obj, K3L_EVENT *ev, R2CountryType r2_country, Verbose::Presentation fmt)
#else
std::string Verbose::event(int32 obj, K3L_EVENT *ev, Verbose::Presentation fmt)
#endif
{
    try
    {
        K3L_CHANNEL_CONFIG & config = _api.channel_config(ev->DeviceId, obj);
#if K3L_AT_LEAST(2,0,0)
        return Verbose::event(config.Signaling, obj, ev, r2_country, fmt);
#else
        return Verbose::event(config.Signaling, obj, ev, fmt);
#endif
    }
    catch (...)
    {
#if K3L_AT_LEAST(2,0,0)
        return Verbose::event(ksigInactive, obj, ev, r2_country, fmt);
#else
        return Verbose::event(ksigInactive, obj, ev, fmt);
#endif
    }
}

/********************************************/

std::string Verbose::echoLocation(KEchoLocation ec, Verbose::Presentation fmt)
{
    switch (ec)
    {
#if K3L_AT_LEAST(1,5,4)
        case kelNetwork: return presentation(fmt, "kelNetwork", "Network");
#else
        case kelE1:      return presentation(fmt, "kelE1",      "Network");
#endif
        case kelCtBus:   return presentation(fmt, "kelCtBus",   "CT-Bus");
    };

    return presentation(fmt, "<unknown>", "Unknown");
};

std::string Verbose::echoCancellerConfig(KEchoCancellerConfig ec, Verbose::Presentation fmt)
{
    switch (ec)
    {
        case keccNotPresent:    return presentation(fmt, "keccNotPresent",    "Not Present");
        case keccOneSingleBank: return presentation(fmt, "keccOneSingleBank", "One, Single Bank");
        case keccOneDoubleBank: return presentation(fmt, "keccOneDoubleBank", "One, Double Bank");
        case keccTwoSingleBank: return presentation(fmt, "keccTwoSingleBank", "Two, Single Bank");
        case keccTwoDoubleBank: return presentation(fmt, "keccTwoDoubleBank", "Two, Double Bank");
        case keccFail:          return presentation(fmt, "keccFail",          "Failure");
    };

    return presentation(fmt, "<unknown>", "Unknown");
};

// TODO: internal_deviceType / internal_deviceModel

std::string Verbose::deviceName(KDeviceType dt, int32 model, Verbose::Presentation fmt)
{
    try
    {
        std::string value;

        value += internal_deviceType(dt);
        value += "-";
        value += internal_deviceModel(dt, model);

        return value;
    }
    catch (internal_not_found & e)
    {
        PRESENTATION_CHECK_RETURN(fmt,
            STG(FMT("[type/model='%d/%d']") % (int)dt % (int)model),
            STG(FMT("Unknown device type/model (%d/%d)") % (int)dt % (int)model));
    }
}

std::string Verbose::deviceType(KDeviceType dt, Verbose::Presentation fmt)
{
    try
    {
        return internal_deviceType(dt);
    }
    catch (internal_not_found & e)
    {
        PRESENTATION_CHECK_RETURN(fmt,
            STG(FMT("[type='%d']") % (int)dt),
            STG(FMT("Unknown device type (%d)") % (int)dt));
    }
}

std::string Verbose::internal_deviceType(KDeviceType dt)
{
    switch (dt)
    {
        case kdtE1:            return "K2E1";

#if K3L_AT_LEAST(1,6,0)
        case kdtFXO:           return "KFXO";
#else
        case kdtFX:            return "KFXO";
#endif

        case kdtConf:          return "KCONF";
        case kdtPR:            return "KPR";
        case kdtE1GW:          return "KE1GW";

#if K3L_AT_LEAST(1,6,0)
        case kdtFXOVoIP:       return "KFXVoIP";
#else
        case kdtFXVoIP:        return "KFXVoIP";
#endif

#if K3L_AT_LEAST(1,5,0)
        case kdtE1IP:          return "K2E1";
#endif
#if K3L_AT_LEAST(1,5,1)
        case kdtE1Spx:         return "K2E1";
        case kdtGWIP:          return "KGWIP";
#endif

#if K3L_AT_LEAST(1,6,0)
        case kdtFXS:           return "KFXS";
        case kdtFXSSpx:        return "KFXS";
        case kdtGSM:           return "KGSM";
        case kdtGSMSpx:        return "KGSM";
#endif

#if K3L_AT_LEAST(2,1,0)
        case kdtGSMUSB:        return "KGSMUSB";
        case kdtGSMUSBSpx:     return "KGSMUSB";

        case kdtE1FXSSpx:      return "KE1FXS";
        case kdtDevTypeCount:  return "DevTypeCount";
#endif

#if K3L_EXACT(2,1,0)
        case kdtReserved1:     return "Reserved1";
#endif

#if K3L_AT_LEAST(2,2,0)
        case kdtE1AdHoc:       return "KE1AdHoc";
#endif
    }

    throw internal_not_found();
}

std::string Verbose::deviceModel(KDeviceType dt, int32 model, Verbose::Presentation fmt)
{
    try
    {
        return internal_deviceModel(dt, model);
    }
    catch (internal_not_found & e)
    {
        PRESENTATION_CHECK_RETURN(fmt,
            STG(FMT("[model='%d']") % (int)model),
            STG(FMT("Unknown device model (%d)") % (int)model));
    }
}

std::string Verbose::internal_deviceModel(KDeviceType dt, int32 model)
{
    switch (dt)
    {
        case kdtE1:
            switch ((KE1DeviceModel)model)
            {
                case kdmE1600:   return "600";
                case kdmE1600E:  return "600E";
#if K3L_AT_LEAST(2,0,0)
                case kdmE1600EX: return "600EX";
#endif
            }
            throw internal_not_found();

#if K3L_AT_LEAST(1,6,0)
        case kdtFXO:
            switch ((KFXODeviceModel)model)
#else
        case kdtFX:
            switch ((KFXDeviceModel)model)
#endif
            {
#if K3L_AT_LEAST(1,6,0)
                case kdmFXO80:    return "80";
                case kdmFXOHI:    return "HI";
                case kdmFXO160HI: return "160HI";
#if K3L_AT_LEAST(2,1,0)
        case kdmFXO240HI: return "240HI";
#endif
#else
                case kdmFXO80:    return "80";
#endif
            }

            throw internal_not_found();

        case kdtConf:
            switch ((KConfDeviceModel)model)
            {
                case kdmConf240:   return "240";
                case kdmConf120:   return "120";
#if K3L_AT_LEAST(2,0,0)
                case kdmConf240EX: return "240EX";
                case kdmConf120EX: return "120EX";
#endif
            }

            throw internal_not_found();

        case kdtPR:
            switch ((KPRDeviceModel)model)
            {
#if K3L_AT_LEAST(1,6,0)
                case kdmPR300v1:       return "300v1";
                case kdmPR300SpxBased: return "300S";
#if K3L_AT_LEAST(2,0,0)
                case kdmPR300EX:       return "300EX";
#endif
                case kdmPR300:         return "300";
            }
#endif
            throw internal_not_found();

#if K3L_AT_LEAST(1,4,0)
        case kdtE1GW:
            switch ((KE1GWDeviceModel)model)
            {
#if K3L_AT_LEAST(1,6,0)
                case kdmE1GW640:  return "640";
#if K3L_AT_LEAST(2,0,0)
                case kdmE1GW640EX:  return "640EX";
#endif
#else
                case kdmE1600V:  return "600V";
                case kdmE1600EV: return "600EV";
#endif
            }

            throw internal_not_found();
#endif

#if K3L_AT_LEAST(1,6,0)
        case kdtFXOVoIP:
            switch ((KFXOVoIPDeviceModel)model)
            {
                case kdmFXGW180:  return "180";
            }

            throw internal_not_found();

#elif K3L_AT_LEAST(1,4,0)
        case kdtFXVoIP:
            switch ((KFXVoIPDeviceModel)model)
            {
                case kdmFXO80V: return "80V";
            }

            throw internal_not_found();
#endif

#if K3L_AT_LEAST(1,5,0)
        case kdtE1IP:
            switch ((KE1IPDeviceModel)model)
            {
#if K3L_AT_LEAST(1,6,0)
                case kdmE1IP:  return "E1IP";
#if K3L_AT_LEAST(2,0,0)
                case kdmE1IPEX:  return "E1IPEX";
#endif
#else
                case kdmE1600EG: return "600EG";
#endif
            }

            throw internal_not_found();
#endif

#if K3L_AT_LEAST(1,5,1)
        case kdtE1Spx:
            switch ((KE1SpxDeviceModel)model)
            {
                case kdmE1Spx:    return "SPX";
                case kdm2E1Based: return "SPX-2E1";
#if K3L_AT_LEAST(2,0,0)
                case kdmE1SpxEX:    return "SPXEX";
#endif
            }
            throw internal_not_found();

        case kdtGWIP:
            switch ((KGWIPDeviceModel)model)
            {
#if K3L_AT_LEAST(1,6,0)
                case kdmGWIP:     return "GWIP";
#if K3L_AT_LEAST(2,0,0)
                case kdmGWIPEX:     return "GWIPEX";
#endif
#else
                case kdmGW600G:   return "600G";
                case kdmGW600EG:  return "600EG";
#endif
            }

            throw internal_not_found();
#endif

#if K3L_AT_LEAST(1,6,0)
        case kdtFXS:
            switch ((KFXSDeviceModel)model)
            {
                case kdmFXS300:   return "300";
#if K3L_AT_LEAST(2,0,0)
                case kdmFXS300EX:   return "300EX";
#endif
            }

            throw internal_not_found();

        case kdtFXSSpx:
            switch ((KFXSSpxDeviceModel)model)
            {
                case kdmFXSSpx300:       return "SPX";
                case kdmFXSSpx2E1Based:  return "SPX-2E1";
#if K3L_AT_LEAST(2,0,0)
                case kdmFXSSpx300EX:       return "SPXEX";
#endif
            }

            throw internal_not_found();

        case kdtGSM:
            switch ((KGSMDeviceModel)model)
            {
                case kdmGSM:       return "40";
#if K3L_AT_LEAST(2,0,0)
                case kdmGSMEX:     return "40EX";
#endif
            }

            throw internal_not_found();

        case kdtGSMSpx:
            switch ((KGSMSpxDeviceModel)model)
            {
                case kdmGSMSpx:    return "SPX";
#if K3L_AT_LEAST(2,0,0)
                case kdmGSMSpxEX:  return "SPXEX";
#endif
            }

            throw internal_not_found();

#if K3L_AT_LEAST(2,1,0)
        case kdtGSMUSB:
            switch ((KGSMUSBDeviceModel)model)
            {
                case kdmGSMUSB:    return "20";
            }

            throw internal_not_found();

        case kdtGSMUSBSpx:
            switch ((KGSMUSBSpxDeviceModel)model)
            {
                case kdmGSMUSBSpx: return "SPX";
            }

            throw internal_not_found();

        case kdtE1FXSSpx:
            switch ((KGSMSpxDeviceModel)model)
            {
                case kdmE1FXSSpx:   return "SPX";
                case kdmE1FXSSpxEX: return "SPXEX";
            }

            throw internal_not_found();
#if K3L_AT_LEAST(2,2,0)
        case kdtE1AdHoc:
            switch((KE1AdHocModel)model)
            {
                case kdmE1AdHoc100:  return "E1AdHoc100";
                case kdmE1AdHoc100E: return "E1AdHoc100E";
                case kdmE1AdHoc240:  return "E1AdHoc240";
                case kdmE1AdHoc240E: return "E1AdHoc240E";
                case kdmE1AdHoc400:  return "E1AdHoc240";
                case kdmE1AdHoc400E: return "E1AdHoc240E";
            }
            throw internal_not_found();
#endif

#if K3L_EXACT(2,1,0)
        case kdtReserved1:
#endif
        case kdtDevTypeCount:
            throw internal_not_found();

#endif
#endif
    }

    throw internal_not_found();
}

std::string Verbose::signaling(KSignaling sig, Verbose::Presentation fmt)
{
    switch (sig)
    {
        case ksigInactive:      return presentation(fmt, "ksigInactive",        "Inactive");
        case ksigAnalog:        return presentation(fmt, "ksigAnalog",          "FXO (analog)");
        case ksigContinuousEM:  return presentation(fmt, "ksigContinuousEM",    "E+M Continuous");
        case ksigPulsedEM:      return presentation(fmt, "ksigPulsedEM",        "E+M PUlsed");
        case ksigOpenCAS:       return presentation(fmt, "ksigOpenCAS",         "Open CAS");
        case ksigOpenR2:        return presentation(fmt, "ksigOpenR2",          "Open R2");
        case ksigR2Digital:     return presentation(fmt, "ksigR2Digital",       "R2/MFC");
        case ksigUserR2Digital: return presentation(fmt, "ksigUserR2Digital",   "R2/Other");
#if K3L_AT_LEAST(1,4,0)
        case ksigSIP:           return presentation(fmt, "ksigSIP",             "SIP");
#endif

#if K3L_AT_LEAST(1,5,1)
        case ksigOpenCCS:       return presentation(fmt, "ksigOpenCCS",         "Open CCS");
        case ksigPRI_EndPoint:  return presentation(fmt, "ksigPRI_EndPoint",    "ISDN Endpoint");
        case ksigPRI_Network:   return presentation(fmt, "ksigPRI_Network",     "ISDN Network");
        case ksigPRI_Passive:   return presentation(fmt, "ksigPRI_Passive",     "ISDN Passive");
#endif
#if K3L_AT_LEAST(1,5,3)
        case ksigLineSide:        return presentation(fmt, "ksigLineSide",      "Line Side");
#endif
#if K3L_AT_LEAST(1,6,0)
        case ksigAnalogTerminal: return presentation(fmt, "ksigAnalogTerminal", "FXS (analog)");
        case ksigGSM:            return presentation(fmt, "ksigGSM",            "GSM");
        case ksigCAS_EL7:        return presentation(fmt, "ksigCAS_EL7",        "CAS EL7");
        case ksigE1LC:           return presentation(fmt, "ksigE1LC",           "E1 LC");
#endif
#if K3L_AT_LEAST(2,1,0)
        case ksigISUP:           return presentation(fmt, "ksigISUP",           "ISUP");
#endif
#if K3L_EXACT(2,1,0)
        case ksigFax:            return presentation(fmt, "ksigFax",            "Fax");
#endif
#if K3L_AT_LEAST(2,2,0)
        case ksigISUPPassive:    return presentation(fmt, "ksigISUPPassive",    "ISUP Passive");
#endif
    }

    PRESENTATION_CHECK_RETURN(fmt,
        STG(FMT("[KSignaling='%d']") % (int)sig),
        STG(FMT("Unknown signaling (%d)") % (int)sig));
}

std::string Verbose::systemObject(KSystemObject so, Verbose::Presentation fmt)
{
    switch (so)
    {
        case ksoLink:      return presentation(fmt, "ksoLink",      "Link");
        case ksoLinkMon:   return presentation(fmt, "ksoLinkMon",   "Link Monitor");
        case ksoChannel:   return presentation(fmt, "ksoChannel",   "Channel");
#if K3L_AT_LEAST(2,1,0)
        case ksoGsmChannel:return presentation(fmt, "ksoGsmChannel","GsmChannel");
#endif
        case ksoH100:      return presentation(fmt, "ksoH100",      "H.100");
        case ksoFirmware:  return presentation(fmt, "ksoFirmware",  "Firmware");
        case ksoDevice:    return presentation(fmt, "ksoDevice",    "Device");
        case ksoAPI:       return presentation(fmt, "ksoAPI",       "Software Layer");
    }

    return presentation(fmt,
        STG(FMT("[KSystemObject='%d']") % (int)so),
        STG(FMT("Unknown object (%d)") % (int)so));
}

std::string Verbose::mixerTone(KMixerTone mt, Verbose::Presentation fmt)
{
    switch (mt)
    {
        case kmtSilence:   return presentation(fmt, "kmtSilence",   "Silence");
        case kmtDial:      return presentation(fmt, "kmtDial",      "Dialtone begin");
        case kmtEndOf425:  return presentation(fmt, "kmtEndOf425",  "Dialtone end");
        case kmtBusy:      return presentation(fmt, "kmtBusy",      "Busy");
        case kmtFax:       return presentation(fmt, "kmtFax",       "Fax");
        case kmtVoice:     return presentation(fmt, "kmtVoice",     "Voice");
#if K3L_AT_LEAST(1,5,0)
        case kmtCollect:   return presentation(fmt, "kmtCollect",   "Collect Call");
#endif
#if K3L_AT_LEAST(1,5,1)
        case kmtEndOfDtmf: return presentation(fmt, "kmtEndOfDtmf", "DTMF end");
#endif
    }

    PRESENTATION_CHECK_RETURN(fmt,
        STG(FMT("[KMixerTone='%d']") % (int)mt),
        STG(FMT("Unknonwn tone (%d)") % (int)mt));
}

std::string Verbose::mixerSource(KMixerSource ms, Verbose::Presentation fmt)
{
    switch (ms)
    {
        case kmsChannel:        return presentation(fmt, "kmsChannel",        "Channel");
        case kmsPlay:           return presentation(fmt, "kmsPlay",           "Player");
        case kmsGenerator:      return presentation(fmt, "kmsGenerator",      "Generator");
        case kmsCTbus:          return presentation(fmt, "kmsCTbus",          "CT-bus");
#if (K3L_AT_LEAST(1,4,0) && !K3L_AT_LEAST(1,6,0))
        case kmsVoIP:           return presentation(fmt, "kmsVoIP",           "VoIP");
#endif
#if K3L_AT_LEAST(1,6,0)
        case kmsNoDelayChannel: return presentation(fmt, "kmsNoDelayChannel", "No delay channel");
#endif
    }

    PRESENTATION_CHECK_RETURN(fmt,
        STG(FMT("[KMixerSource='%d']") % (int)ms),
        STG(FMT("Unknonwn source (%d)") % (int)ms));
}

std::string Verbose::channelFeatures(int32 flags, Verbose::Presentation fmt)
{
    if (0x00 != flags)
    {
        Strings::Merger strs;

        if (kcfDtmfSuppression & flags)   strs.add(presentation(fmt, "DtmfSuppression",   "DTMF Suppression"));
        if (kcfCallProgress & flags)      strs.add(presentation(fmt, "CallProgress",      "Call Progress"));
        if (kcfPulseDetection & flags)    strs.add(presentation(fmt, "PulseDetection",    "Pulse Detection"));
        if (kcfAudioNotification & flags) strs.add(presentation(fmt, "AudioNotification", "Audio Notification"));
        if (kcfEchoCanceller & flags)     strs.add(presentation(fmt, "EchoCanceller",     "Echo Canceller"));
        if (kcfAutoGainControl & flags)   strs.add(presentation(fmt, "AutoGainControl",   "Input AGC"));
        if (kcfHighImpEvents & flags)     strs.add(presentation(fmt, "HighImpEvents",     "High Impedance Events"));
#if K3L_AT_LEAST(1,6,0)
        if (kcfCallAnswerInfo & flags)    strs.add(presentation(fmt, "CallAnswerInfo",    "Call Answer Info"));
#if !K3L_AT_LEAST(2,2,0)
        if (kcfOutputVolume & flags)      strs.add(presentation(fmt, "OutputVolume",      "Output Volume"));
#endif
        if (kcfPlayerAGC & flags)         strs.add(presentation(fmt, "PlayerAGC",         "Player AGC"));
#endif

        return presentation(fmt,
            STG(FMT("kcf{%s}") % strs.merge(",")),
            STG(FMT("%s") % strs.merge(", ")));
    };

    PRESENTATION_CHECK_RETURN(fmt, "", "No features");
}

std::string Verbose::seizeFail(KSeizeFail sf, Verbose::Presentation fmt)
{
    switch (sf)
    {
        case ksfChannelLocked:   return presentation(fmt, "ksfChannelLocked",   "Channel Locked");
        case ksfChannelBusy:     return presentation(fmt, "ksfChannelBusy",     "Channel Busy");
        case ksfIncomingChannel: return presentation(fmt, "ksfIncomingChannel", "Incoming Channel");
        case ksfDoubleSeizure:   return presentation(fmt, "ksfDoubleSeizure",   "Double Seizure");
        case ksfCongestion:      return presentation(fmt, "ksfCongestion",      "Congestion");
        case ksfNoDialTone:      return presentation(fmt, "ksfNoDialTone",      "No Dial Tone");
    }

    PRESENTATION_CHECK_RETURN(fmt,
        STG(FMT("[KSeizeFail='%d']") % (int)sf),
        STG(FMT("Unknown seize fail (%d)") % (int)sf));
}

#if K3L_AT_LEAST(1,5,0)
std::string Verbose::internal_sipFailures(KSIP_Failures code, Verbose::Presentation fmt)
{
    switch (code)
    {
#if K3L_AT_LEAST(1,6,0)
        case kveResponse_200_OK_Success:                 return presentation(fmt, "kveResponse_200_OK_Success",                 "200 OK");
#endif
        case kveRedirection_300_MultipleChoices:         return presentation(fmt, "kveRedirection_300_MultipleChoices",         "300 Multiple Choices");
        case kveRedirection_301_MovedPermanently:        return presentation(fmt, "kveRedirection_301_MovedPermanently",        "301 Moved Permanently");
        case kveRedirection_302_MovedTemporarily:        return presentation(fmt, "kveRedirection_302_MovedTemporarily",        "302 Moved Temporarily");
        case kveRedirection_305_UseProxy:                return presentation(fmt, "kveRedirection_305_UseProxy",                "305 Use Proxy");
        case kveRedirection_380_AlternativeService:      return presentation(fmt, "kveRedirection_380_AlternativeService",      "380 Alternate Service");
        case kveFailure_400_BadRequest:                  return presentation(fmt, "kveFailure_400_BadRequest",                  "400 Bad Request");
        case kveFailure_401_Unauthorized:                return presentation(fmt, "kveFailure_401_Unauthorized",                "401 Unauthorized");
        case kveFailure_402_PaymentRequired:             return presentation(fmt, "kveFailure_402_PaymentRequired",             "402 Payment Required");
        case kveFailure_403_Forbidden:                   return presentation(fmt, "kveFailure_403_Forbidden",                   "403 Forbidden");
        case kveFailure_404_NotFound:                    return presentation(fmt, "kveFailure_404_NotFound",                    "404 Not Found");
        case kveFailure_405_MethodNotAllowed:            return presentation(fmt, "kveFailure_405_MethodNotAllowed",            "405 Method Not Allowed");
        case kveFailure_406_NotAcceptable:               return presentation(fmt, "kveFailure_406_NotAcceptable",               "406 Not Acceptable");
        case kveFailure_407_ProxyAuthenticationRequired: return presentation(fmt, "kveFailure_407_ProxyAuthenticationRequired", "407 Proxy Authentication Required");
        case kveFailure_408_RequestTimeout:              return presentation(fmt, "kveFailure_408_RequestTimeout",              "408 Request Timeout");
        case kveFailure_410_Gone:                        return presentation(fmt, "kveFailure_410_Gone",                        "410 Gone");
        case kveFailure_413_RequestEntityTooLarge:       return presentation(fmt, "kveFailure_413_RequestEntityTooLarge",       "413 Request Entity Too Large");
        case kveFailure_414_RequestURI_TooLong:          return presentation(fmt, "kveFailure_414_RequestURI_TooLong",          "414 Request URI Too Long");
        case kveFailure_415_UnsupportedMediaType:        return presentation(fmt, "kveFailure_415_UnsupportedMediaType",        "415 Unsupported Media Type");
        case kveFailure_416_UnsupportedURI_Scheme:       return presentation(fmt, "kveFailure_416_UnsupportedURI_Scheme",       "416 Unsupported URI Scheme");
        case kveFailure_420_BadExtension:                return presentation(fmt, "kveFailure_420_BadExtension",                "420 Bad Extension");
        case kveFailure_421_ExtensionRequired:           return presentation(fmt, "kveFailure_421_ExtensionRequired",           "421 Extension Required");
        case kveFailure_423_IntervalTooBrief:            return presentation(fmt, "kveFailure_423_IntervalTooBrief",            "423 Internal Too Brief");
        case kveFailure_480_TemporarilyUnavailable:      return presentation(fmt, "kveFailure_480_TemporarilyUnavailable",      "480 Temporarily Unavailable");
        case kveFailure_481_CallDoesNotExist:            return presentation(fmt, "kveFailure_481_CallDoesNotExist",            "481 Call Does Not Exist");
        case kveFailure_482_LoopDetected:                return presentation(fmt, "kveFailure_482_LoopDetected",                "482 Loop Detected");
        case kveFailure_483_TooManyHops:                 return presentation(fmt, "kveFailure_483_TooManyHops",                 "483 Too Many Hops");
        case kveFailure_484_AddressIncomplete:           return presentation(fmt, "kveFailure_484_AddressIncomplete",           "484 Address Incomplete");
        case kveFailure_485_Ambiguous:                   return presentation(fmt, "kveFailure_485_Ambiguous",                   "485 Ambiguous");
        case kveFailure_486_BusyHere:                    return presentation(fmt, "kveFailure_486_BusyHere",                    "486 Busy Here");
        case kveFailure_487_RequestTerminated:           return presentation(fmt, "kveFailure_487_RequestTerminated",           "487 Request Terminated");
        case kveFailure_488_NotAcceptableHere:           return presentation(fmt, "kveFailure_488_NotAcceptableHere",           "488 Not Acceptable Here");
        case kveFailure_491_RequestPending:              return presentation(fmt, "kveFailure_491_RequestPending",              "491 Request Pending");
        case kveFailure_493_Undecipherable:              return presentation(fmt, "kveFailure_493_Undecipherable",              "493 Undecipherable");
        case kveServer_500_InternalError:                return presentation(fmt, "kveServer_500_InternalError",                "500 Internal Error");
        case kveServer_501_NotImplemented:               return presentation(fmt, "kveServer_501_NotImplemented",               "501 Not Implemented");
        case kveServer_502_BadGateway:                   return presentation(fmt, "kveServer_502_BadGateway",                   "502 Bad Gateway");
        case kveServer_503_ServiceUnavailable:           return presentation(fmt, "kveServer_503_ServiceUnavailable",           "503 Service Unavailable");
        case kveServer_504_TimeOut:                      return presentation(fmt, "kveServer_504_TimeOut",                      "504 Timed Out");
        case kveServer_505_VersionNotSupported:          return presentation(fmt, "kveServer_505_VersionNotSupported",          "505 Version Not Supported");
        case kveServer_513_MessageTooLarge:              return presentation(fmt, "kveServer_513_MessageTooLarge",              "513 Message Too Large");
        case kveGlobalFailure_600_BusyEverywhere:        return presentation(fmt, "kveGlobalFailure_600_BusyEverywhere",        "600 Busy Everywhere");
        case kveGlobalFailure_603_Decline:               return presentation(fmt, "kveGlobalFailure_603_Decline",               "603 Decline");
        case kveGlobalFailure_604_DoesNotExistAnywhere:  return presentation(fmt, "kveGlobalFailure_604_DoesNotExistAnywhere",  "604 Does Not Exist Anywhere");
        case kveGlobalFailure_606_NotAcceptable:         return presentation(fmt, "kveGlobalFailure_606_NotAcceptable",         "606 Not Acceptable");
    }

    throw internal_not_found();
}

std::string Verbose::sipFailures(KSIP_Failures code, Verbose::Presentation fmt)
{
    try
    {
        return internal_sipFailures(code, fmt);
    }
    catch (internal_not_found & e)
    {
        PRESENTATION_CHECK_RETURN(fmt,
            STG(FMT("[KSIP_Failures='%d']") % (int)code),
            STG(FMT("Unknown SIP failure (%d)") % (int)code));
    }
}

#endif

#if K3L_AT_LEAST(1,5,1)
std::string Verbose::internal_isdnCause(KQ931Cause code, Verbose::Presentation fmt)
{
    switch (code)
    {
        case kq931cNone:                           return presentation(fmt, "kq931cNone",                           "None");
        case kq931cUnallocatedNumber:              return presentation(fmt, "kq931cUnallocatedNumber",              "Unallocated number");
        case kq931cNoRouteToTransitNet:            return presentation(fmt, "kq931cNoRouteToTransitNet",            "No route to transmit to network");
        case kq931cNoRouteToDest:                  return presentation(fmt, "kq931cNoRouteToDest",                  "No route to destination");
#if 1 /* this changed during K3L 1.6.0 development cycle... */
        case kq931cSendSpecialInfoTone:            return presentation(fmt, "kq931cSendSpecialInfoTone",            "Send special information tone");
        case kq931cMisdialedTrunkPrefix:           return presentation(fmt, "kq931cMisdialedTrunkPrefix",           "Misdialed trunk prefix");
#endif
        case kq931cChannelUnacceptable:            return presentation(fmt, "kq931cChannelUnacceptable",            "Channel unacceptable");
        case kq931cCallAwarded:                    return presentation(fmt, "kq931cCallAwarded",                    "Call awarded");
#if 1 /* this changed during K3L 1.6.0 development cycle... */
        case kq931cPreemption:                     return presentation(fmt, "kq931cPreemption",                     "Preemption");
        case kq931cPreemptionCircuitReuse:         return presentation(fmt, "kq931cPreemptionCircuitReuse",         "Preemption circuit reuse");
        case kq931cQoR_PortedNumber:               return presentation(fmt, "kq931cQoR_PortedNumber",               "QoR ported number");
#endif
        case kq931cNormalCallClear:                return presentation(fmt, "kq931cNormalCallClear",                "Normal call clear");
        case kq931cUserBusy:                       return presentation(fmt, "kq931cUserBusy",                       "User busy");
        case kq931cNoUserResponding:               return presentation(fmt, "kq931cNoUserResponding",               "No user responding");
        case kq931cNoAnswerFromUser:               return presentation(fmt, "kq931cNoAnswerFromUser",               "No answer from user");
#if 1 /* this changed during K3L 1.6.0 development cycle... */
        case kq931cSubscriberAbsent:               return presentation(fmt, "kq931cSubscriberAbsent",               "Subscriber absent");
#endif
        case kq931cCallRejected:                   return presentation(fmt, "kq931cCallRejected",                   "Call rejected");
        case kq931cNumberChanged:                  return presentation(fmt, "kq931cNumberChanged",                  "Number changed");
#if 1 /* this changed during K3L 1.6.0 development cycle... */
        case kq931cRedirectionToNewDest:           return presentation(fmt, "kq931cRedirectionToNewDest",           "Redirection to new destination");
        case kq931cCallRejectedFeatureDest:        return presentation(fmt, "kq931cCallRejectedFeatureDest",        "Call rejected feature destination");
        case kq931cExchangeRoutingError:           return presentation(fmt, "kq931cExchangeRoutingError",           "Exchange routing error");
#endif
        case kq931cNonSelectedUserClear:           return presentation(fmt, "kq931cNonSelectedUserClear",           "Non selected user clear");
        case kq931cDestinationOutOfOrder:          return presentation(fmt, "kq931cDestinationOutOfOrder",          "Destination out of order");
        case kq931cInvalidNumberFormat:            return presentation(fmt, "kq931cInvalidNumberFormat",            "Invalid number format");
        case kq931cFacilityRejected:               return presentation(fmt, "kq931cFacilityRejected",               "Facility rejected");
        case kq931cRespStatusEnquiry:              return presentation(fmt, "kq931cRespStatusEnquiry",              "Response status enquiry");
        case kq931cNormalUnspecified:              return presentation(fmt, "kq931cNormalUnspecified",              "Normal unespecified");
        case kq931cNoCircuitChannelAvail:          return presentation(fmt, "kq931cNoCircuitChannelAvail",          "No circuit channel available");
        case kq931cNetworkOutOfOrder:              return presentation(fmt, "kq931cNetworkOutOfOrder",              "Network out of order");
#if 1 /* this changed during K3L 1.6.0 development cycle... */
        case kq931cPermanentFrameConnOutOfService: return presentation(fmt, "kq931cPermanentFrameConnOutOfService", "Permanent frame connection out of service");
        case kq931cPermanentFrameConnOperational:  return presentation(fmt, "kq931cPermanentFrameConnOperational",  "Permanent frame connection operational");
#endif
        case kq931cTemporaryFailure:               return presentation(fmt, "kq931cTemporaryFailure",               "Temporary failure");
        case kq931cSwitchCongestion:               return presentation(fmt, "kq931cSwitchCongestion",               "Switch congestion");
        case kq931cAccessInfoDiscarded:            return presentation(fmt, "kq931cAccessInfoDiscarded",            "Access information discarded");
        case kq931cRequestedChannelUnav:           return presentation(fmt, "kq931cRequestedChannelUnav",           "Requested channel unavailable");
        case kq931cPrecedenceCallBlocked:          return presentation(fmt, "kq931cPrecedenceCallBlocked",          "Precedence call blocked");
        case kq931cResourceUnavailable:            return presentation(fmt, "kq931cResourceUnavailable",            "Request resource unavailable");
        case kq931cQosUnavailable:                 return presentation(fmt, "kq931cQosUnavailable",                 "QoS unavailable");
        case kq931cReqFacilityNotSubsc:            return presentation(fmt, "kq931cReqFacilityNotSubsc",            "Request facility not subscribed");
        case kq931cOutCallsBarredWithinCUG:        return presentation(fmt, "kq931cOutCallsBarredWithinCUG",        "Out calls barred within UG");
        case kq931cInCallsBarredWithinCUG:         return presentation(fmt, "kq931cInCallsBarredWithinCUG",         "In calls barred within UG");
        case kq931cBearerCapabNotAuthor:           return presentation(fmt, "kq931cBearerCapabNotAuthor",           "Bearer capability not authorized");
        case kq931cBearerCapabNotAvail:            return presentation(fmt, "kq931cBearerCapabNotAvail",            "Bearer capability not available");
#if 1 /* this changed during K3L 1.6.0 development cycle... */
        case kq931cInconsistency:                  return presentation(fmt, "kq931cInconsistency",                  "Inconsistency");
#endif
        case kq931cServiceNotAvailable:            return presentation(fmt, "kq931cServiceNotAvailable",            "Service not available");
        case kq931cBcNotImplemented:               return presentation(fmt, "kq931cBcNotImplemented",               "Bearer capability not implemented");
        case kq931cChannelTypeNotImplem:           return presentation(fmt, "kq931cChannelTypeNotImplem",           "Channel type not implemented");
        case kq931cReqFacilityNotImplem:           return presentation(fmt, "kq931cReqFacilityNotImplem",           "Request facility not implemented");
        case kq931cOnlyRestrictedBcAvail:          return presentation(fmt, "kq931cOnlyRestrictedBcAvail",          "Only restricted bearer capability available");
        case kq931cServiceNotImplemented:          return presentation(fmt, "kq931cServiceNotImplemented",          "Service not implemented");
        case kq931cInvalidCrv:                     return presentation(fmt, "kq931cInvalidCrv",                     "Invalid call reference value");
        case kq931cChannelDoesNotExist:            return presentation(fmt, "kq931cChannelDoesNotExist",            "Channel does not exist");
        case kq931cCallIdDoesNotExist:             return presentation(fmt, "kq931cCallIdDoesNotExist",             "Call identification does not exist");
        case kq931cCallIdInUse:                    return presentation(fmt, "kq931cCallIdInUse",                    "Call identification in use");
        case kq931cNoCallSuspended:                return presentation(fmt, "kq931cNoCallSuspended",                "No call suspended");
        case kq931cCallIdCleared:                  return presentation(fmt, "kq931cCallIdCleared",                  "Call identification cleared");
#if 1 /* this changed during K3L 1.6.0 development cycle... */
        case kq931cUserNotMemberofCUG:             return presentation(fmt, "kq931cUserNotMemberofCUG",             "User not member of UG");
#endif
        case kq931cIncompatibleDestination:        return presentation(fmt, "kq931cIncompatibleDestination",        "Incompatible destination");
        case kq931cInvalidTransitNetSel:           return presentation(fmt, "kq931cInvalidTransitNetSel",           "Invalid transit network selected");
        case kq931cInvalidMessage:                 return presentation(fmt, "kq931cInvalidMessage",                 "Invalid message");
        case kq931cMissingMandatoryIe:             return presentation(fmt, "kq931cMissingMandatoryIe",             "Missing mandatory information element");
        case kq931cMsgTypeNotImplemented:          return presentation(fmt, "kq931cMsgTypeNotImplemented",          "Message type not implemented");
        case kq931cMsgIncompatWithState:           return presentation(fmt, "kq931cMsgIncompatWithState",           "Message incompatible with state");
        case kq931cIeNotImplemented:               return presentation(fmt, "kq931cIeNotImplemented",               "Information element not implemented");
        case kq931cInvalidIe:                      return presentation(fmt, "kq931cInvalidIe",                      "Invalid information element");
        case kq931cMsgIncompatWithState2:          return presentation(fmt, "kq931cMsgIncompatWithState2",          "Message incompatible with state (2)");
        case kq931cRecoveryOnTimerExpiry:          return presentation(fmt, "kq931cRecoveryOnTimerExpiry",          "Recovery on timer expiry");
        case kq931cProtocolError:                  return presentation(fmt, "kq931cProtocolError",                  "Protocol error");
#if 1 /* this changed during K3L 1.6.0 development cycle... */
        case kq931cMessageWithUnrecognizedParam:   return presentation(fmt, "kq931cMessageWithUnrecognizedParam",   "Message with unrecognized parameters");
        case kq931cProtocolErrorUnspecified:       return presentation(fmt, "kq931cProtocolErrorUnspecified",       "Protocol error unspecified");
#endif
        case kq931cInterworking:                   return presentation(fmt, "kq931cInterworking",                   "Interworking");
        case kq931cCallConnected:                  return presentation(fmt, "kq931cCallConnected",                  "Call connected");
        case kq931cCallTimedOut:                   return presentation(fmt, "kq931cCallTimedOut",                   "Call timeout");
        case kq931cCallNotFound:                   return presentation(fmt, "kq931cCallNotFound",                   "Call not found");
        case kq931cCantReleaseCall:                return presentation(fmt, "kq931cCantReleaseCall",                "Cannot realese call");
        case kq931cNetworkFailure:                 return presentation(fmt, "kq931cNetworkFailure",                 "Network failure");
        case kq931cNetworkRestart:                 return presentation(fmt, "kq931cNetworkRestart",                 "Network restart");
    }

    throw internal_not_found();
}

std::string Verbose::isdnCause(KQ931Cause code, Verbose::Presentation fmt)
{
    try
    {
        return internal_isdnCause(code);
    }
    catch (internal_not_found & e)
    {
        return STG(FMT("[KQ931Cause='%d']") % (int)code);
    }
}
#endif

#if K3L_AT_LEAST(1,5,2)
std::string Verbose::isdnDebug(int32 flags, Verbose::Presentation fmt)
{
    if (0x00 != flags)
    {
        Strings::Merger strs;

        if (flags & kidfQ931)   strs.add("Q931");
        if (flags & kidfLAPD)   strs.add("LAPD");
        if (flags & kidfSystem) strs.add("System");

        PRESENTATION_CHECK_RETURN(fmt,
            STG(FMT("kidf{%s}") % strs.merge(",")),
            strs.merge(", "));
    }

    return presentation(fmt, "", "No debug active");
}
#endif

#if K3L_AT_LEAST(2,0,0)
std::string Verbose::internal_signGroupB(KSignGroupB group, R2CountryType country, Verbose::Presentation fmt)
#else
std::string Verbose::internal_signGroupB(KSignGroupB group, Verbose::Presentation fmt)
#endif
{
#if K3L_AT_LEAST(2,0,0)
    switch (country)
    {
        case R2_COUNTRY_ARG:
            switch ((KSignGroupB_Argentina)group)
            {
                case kgbArNumberChanged:       return presentation(fmt, "kgbArNumberChanged",      "Number Changed");
                case kgbArBusy:                return presentation(fmt, "kgbArBusy",               "Busy");
                case kgbArCongestion:          return presentation(fmt, "kgbArCongestion",         "Congestion");
                case kgbArInvalidNumber:       return presentation(fmt, "kgbArInvalidNumber",      "Invalid Number");
                case kgbArLineFreeCharged:     return presentation(fmt, "kgbArLineFreeCharged",    "Line Free Charged");
                case kgbArLineFreeNotCharged:  return presentation(fmt, "kgbArLineFreeNotCharged", "Line Free Not Charged");
                case kgbArLineOutOfOrder:      return presentation(fmt, "kgbArLineOutOfOrder",     "Line Out Of Order");
                case kgbArNone:                return presentation(fmt, "kgbArNone",               "None");
            }
            break;

        case R2_COUNTRY_BRA:
            switch ((KSignGroupB_Brazil)group)
            {
                case kgbBrLineFreeCharged:     return presentation(fmt, "kgbBrLineFreeCharged",    "Line Free Charged");
                case kgbBrLineFreeNotCharged:  return presentation(fmt, "kgbBrLineFreeNotCharged", "Line Free Not Charged");
                case kgbBrLineFreeChargedLPR:  return presentation(fmt, "kgbBrLineFreeChargedLPR", "Line Free Charged PLR");
                case kgbBrBusy:                return presentation(fmt, "kgbBrBusy",               "Busy");
                case kgbBrNumberChanged:       return presentation(fmt, "kgbBrNumberChanged",      "Number Changed");
                case kgbBrCongestion:          return presentation(fmt, "kgbBrCongestion",         "Congestion");
                case kgbBrInvalidNumber:       return presentation(fmt, "kgbBrInvalidNumber",      "Invalid Number");
                case kgbBrLineOutOfOrder:      return presentation(fmt, "kgbBrLineOutOfOrder",     "Line Out Of Order");
                case kgbBrNone:                return presentation(fmt, "kgbBrNone",               "None");
            }
            break;

        case R2_COUNTRY_CHI:
            switch ((KSignGroupB_Chile)group)
            {
                case kgbClNumberChanged:       return presentation(fmt, "kgbClNumberChanged",      "Number Changed");
                case kgbClBusy:                return presentation(fmt, "kgbClBusy",               "Busy");
                case kgbClCongestion:          return presentation(fmt, "kgbClCongestion",         "Congestion");
                case kgbClInvalidNumber:       return presentation(fmt, "kgbClInvalidNumber",      "Invalid Number");
                case kgbClLineFreeCharged:     return presentation(fmt, "kgbClLineFreeCharged",    "Line Free Charged");
                case kgbClLineFreeNotCharged:  return presentation(fmt, "kgbClLineFreeNotCharged", "Line Free Not Charged");
                case kgbClLineOutOfOrder:      return presentation(fmt, "kgbClLineOutOfOrder",     "Line Out Of Order");
                case kgbClNone:                return presentation(fmt, "kgbClNone",               "None");
            }
            break;

        case R2_COUNTRY_MEX:
            switch ((KSignGroupB_Mexico)group)
            {
                case kgbMxLineFreeCharged:     return presentation(fmt, "kgbMxLineFreeCharged",    "Line Free Charged");
                case kgbMxBusy:                return presentation(fmt, "kgbMxBusy",               "Busy");
                case kgbMxLineFreeNotCharged:  return presentation(fmt, "kgbMxLineFreeNotCharged", "Line Free Not Charged");
                case kgbMxNone:                return presentation(fmt, "kgbMxNone",               "None");
            }
            break;

        case R2_COUNTRY_URY:
            switch ((KSignGroupB_Uruguay)group)
            {
                case kgbUyNumberChanged:       return presentation(fmt, "kgbUyNumberChanged",      "Number Changed");
                case kgbUyBusy:                return presentation(fmt, "kgbUyBusy",               "Busy");
                case kgbUyCongestion:          return presentation(fmt, "kgbUyCongestion",         "Congestion");
                case kgbUyInvalidNumber:       return presentation(fmt, "kgbUyInvalidNumber",      "Invalid Number");
                case kgbUyLineFreeCharged:     return presentation(fmt, "kgbUyLineFreeCharged",    "Line Free Charged");
                case kgbUyLineFreeNotCharged:  return presentation(fmt, "kgbUyLineFreeNotCharged", "Line Free Not Charged");
                case kgbUyLineOutOfOrder:      return presentation(fmt, "kgbUyLineOutOfOrder",     "Line Out Of Order");
                case kgbUyNone:                return presentation(fmt, "kgbUyNone",               "None");
            }
            break;

        case R2_COUNTRY_VEN:
            switch ((KSignGroupB_Venezuela)group)
            {
                case kgbVeLineFreeChargedLPR:  return presentation(fmt, "kgbVeLineFreeChargedLPR", "Line Free Charged PLR");
                case kgbVeNumberChanged:       return presentation(fmt, "kgbVeNumberChanged",      "Number Changed");
                case kgbVeBusy:                return presentation(fmt, "kgbVeBusy",               "Busy");
                case kgbVeCongestion:          return presentation(fmt, "kgbVeCongestion",         "Congestion");
                case kgbVeInformationTone:     return presentation(fmt, "kgbVeInformationTone",    "Information Tone");
                case kgbVeLineFreeCharged:     return presentation(fmt, "kgbVeLineFreeCharged",    "Line Free Charged");
                case kgbVeLineFreeNotCharged:  return presentation(fmt, "kgbVeLineFreeNotCharged", "Line Free Not Charged");
                case kgbVeLineBlocked:         return presentation(fmt, "kgbVeLineBlocked",        "Line Blocked");
                case kgbVeIntercepted:         return presentation(fmt, "kgbVeIntercepted",        "Intercepted");
                case kgbVeDataTrans:           return presentation(fmt, "kgbVeDataTrans",          "Data Transfer");
                case kgbVeNone:                return presentation(fmt, "kgbVeNone",               "None");
            }
            break;
    }
#else
    switch ((KSignGroupB)group)
    {
        case kgbLineFreeCharged:     return presentation(fmt, "kgbLineFreeCharged",    "Line Free Charged");
        case kgbLineFreeNotCharged:  return presentation(fmt, "kgbLineFreeNotCharged", "Line Free Not Charged");
        case kgbLineFreeChargedLPR:  return presentation(fmt, "kgbLineFreeChargedLPR", "Line Free Charged PLR");
        case kgbBusy:                return presentation(fmt, "kgbBusy",               "Busy");
        case kgbNumberChanged:       return presentation(fmt, "kgbNumberChanged",      "Number Changed");
        case kgbCongestion:          return presentation(fmt, "kgbCongestion",         "Congestion");
        case kgbInvalidNumber:       return presentation(fmt, "kgbInvalidNumber",      "Invalid Number");
        case kgbLineOutOfOrder:      return presentation(fmt, "kgbLineOutOfOrder",     "Line Out Of Order");
        case kgbNone:                return presentation(fmt, "kgbNone",               "None");
    }
#endif

    throw internal_not_found();
}

#if K3L_AT_LEAST(2,0,0)
std::string Verbose::signGroupB(KSignGroupB group, R2CountryType r2_country, Verbose::Presentation fmt)
#else
std::string Verbose::signGroupB(KSignGroupB group, Verbose::Presentation fmt)
#endif
{
    try
    {
#if K3L_AT_LEAST(2,0,0)
        return internal_signGroupB(group, r2_country, fmt);
#else
        return internal_signGroupB(group, fmt);
#endif
    }
    catch (internal_not_found & e)
    {
        PRESENTATION_CHECK_RETURN(fmt,
            STG(FMT("[KSignGroupB='%d']") % (int)group),
            STG(FMT("Unknown group B (%d)") % (int)group));
    }
}

#if K3L_AT_LEAST(2,0,0)
std::string Verbose::internal_signGroupII(KSignGroupII group, R2CountryType country, Verbose::Presentation fmt)
#else
std::string Verbose::internal_signGroupII(KSignGroupII group, Verbose::Presentation fmt)
#endif
{
#if K3L_AT_LEAST(2,0,0)
    switch (country)
    {
        case R2_COUNTRY_ARG:
            switch ((KSignGroupII_Argentina)group)
            {
                case kg2ArOrdinary:             return presentation(fmt, "kg2ArOrdinary",            "Ordinary");
                case kg2ArPriority:             return presentation(fmt, "kg2ArPriority",            "Priority");
                case kg2ArMaintenance:          return presentation(fmt, "kg2ArMaintenance",         "Maintenance");
                case kg2ArLocalPayPhone:        return presentation(fmt, "kg2ArLocalPayPhone",       "Local pay phone");
                case kg2ArTrunkOperator:        return presentation(fmt, "kg2ArTrunkOperator",       "Trunk operator");
                case kg2ArDataTrans:            return presentation(fmt, "kg2ArDataTrans",           "Data transfer");
                case kg2ArCPTP:                 return presentation(fmt, "kg2ArCPTP",                "CPTP");
                case kg2ArSpecialLine:          return presentation(fmt, "kg2ArSpecialLine",         "Special line");
                case kg2ArMobileUser:           return presentation(fmt, "kg2ArMobileUser",          "Mobile user");
                case kg2ArPrivateRedLine:       return presentation(fmt, "kg2ArPrivateRedLine",      "Private red line");
                case kg2ArSpecialPayPhoneLine:  return presentation(fmt, "kg2ArSpecialPayPhoneLine", "Special pay phone line");
            }
            break;

        case R2_COUNTRY_BRA:
            switch ((KSignGroupII_Brazil)group)
            {
                case kg2BrOrdinary:         return presentation(fmt, "kg2BrOrdinary",            "Ordinary");
                case kg2BrPriority:         return presentation(fmt, "kg2BrPriority",            "Priority");
                case kg2BrMaintenance:      return presentation(fmt, "kg2BrMaintenance",         "Maintenance");
                case kg2BrLocalPayPhone:    return presentation(fmt, "kg2BrLocalPayPhone",       "Local pay phone");
                case kg2BrTrunkOperator:    return presentation(fmt, "kg2BrTrunkOperator",       "Trunk operator");
                case kg2BrDataTrans:        return presentation(fmt, "kg2BrDataTrans",           "Data transfer");
                case kg2BrNonLocalPayPhone: return presentation(fmt, "kg2BrNonLocalPayPhone",    "Non local pay phone");
                case kg2BrCollectCall:      return presentation(fmt, "kg2BrCollectCall",         "Collect call");
                case kg2BrOrdinaryInter:    return presentation(fmt, "kg2BrOrdinaryInter",       "Ordinary international");
                case kg2BrTransfered:       return presentation(fmt, "kg2BrTransfered",          "Transfered");
            }
            break;

        case R2_COUNTRY_CHI:
            switch ((KSignGroupII_Chile)group)
            {
                case kg2ClOrdinary:               return presentation(fmt, "kg2ClOrdinary",               "Ordinary");
                case kg2ClPriority:               return presentation(fmt, "kg2ClPriority",               "Priority");
                case kg2ClMaintenance:            return presentation(fmt, "kg2ClMaintenance",            "Maintenance");
                case kg2ClTrunkOperator:          return presentation(fmt, "kg2ClTrunkOperator",          "Trunk operator");
                case kg2ClDataTrans:              return presentation(fmt, "kg2ClDataTrans",              "Data transfer");
                case kg2ClUnidentifiedSubscriber: return presentation(fmt, "kg2ClUnidentifiedSubscriber", "Unidentified subscriber");
            }
            break;

        case R2_COUNTRY_MEX:
            switch ((KSignGroupII_Mexico)group)
            {
                case kg2MxTrunkOperator:    return presentation(fmt, "kg2MxTrunkOperator",       "Trunk operator");
                case kg2MxOrdinary:         return presentation(fmt, "kg2MxOrdinary",            "Ordinary");
                case kg2MxMaintenance:      return presentation(fmt, "kg2MxMaintenance",         "Maintenance");
            }
            break;

        case R2_COUNTRY_URY:
            switch ((KSignGroupII_Uruguay)group)
            {
                case kg2UyOrdinary:         return presentation(fmt, "kg2UyOrdinary",            "Ordinary");
                case kg2UyPriority:         return presentation(fmt, "kg2UyPriority",            "Priority");
                case kg2UyMaintenance:      return presentation(fmt, "kg2UyMaintenance",         "Maintenance");
                case kg2UyLocalPayPhone:    return presentation(fmt, "kg2UyLocalPayPhone",       "Local pay phone");
                case kg2UyTrunkOperator:    return presentation(fmt, "kg2UyTrunkOperator",       "Trunk operator");
                case kg2UyDataTrans:        return presentation(fmt, "kg2UyDataTrans",           "Data transfer");
                case kg2UyInternSubscriber: return presentation(fmt, "kg2UyInternSubscriber",    "International subscriber");
            }
            break;

        case R2_COUNTRY_VEN:
            switch ((KSignGroupII_Venezuela)group)
            {
                case kg2VeOrdinary:           return presentation(fmt, "kg2VeOrdinary",            "Ordinary");
                case kg2VePriority:           return presentation(fmt, "kg2VePriority",            "Priority");
                case kg2VeMaintenance:        return presentation(fmt, "kg2VeMaintenance",         "Maintenance");
                case kg2VeLocalPayPhone:      return presentation(fmt, "kg2VeLocalPayPhone",       "Local pay phone");
                case kg2VeTrunkOperator:      return presentation(fmt, "kg2VeTrunkOperator",       "Trunk operator");
                case kg2VeDataTrans:          return presentation(fmt, "kg2VeDataTrans",           "Data transfer");
                case kg2VeNoTransferFacility: return presentation(fmt, "kg2VeNoTransferFacility",  "No transfer facility");
            }
            break;
    }
#else
    switch ((KSignGroupII)group)
    {
        case kg2Ordinary:         return presentation(fmt, "kg2Ordinary",            "Ordinary");
        case kg2Priority:         return presentation(fmt, "kg2Priority",            "Priority");
        case kg2Maintenance:      return presentation(fmt, "kg2Maintenance",         "Maintenance");
        case kg2LocalPayPhone:    return presentation(fmt, "kg2LocalPayPhone",       "Local pay phone");
        case kg2TrunkOperator:    return presentation(fmt, "kg2TrunkOperator",       "Trunk operator");
        case kg2DataTrans:        return presentation(fmt, "kg2DataTrans",           "Data transfer");
        case kg2NonLocalPayPhone: return presentation(fmt, "kg2NonLocalPayPhone",    "Non local pay phone");
        case kg2CollectCall:      return presentation(fmt, "kg2CollectCall",         "Collect call");
        case kg2OrdinaryInter:    return presentation(fmt, "kg2OrdinaryInter",       "Ordinary international");
        case kg2Transfered:       return presentation(fmt, "kg2Transfered",          "Transfered");
    }
#endif

    throw internal_not_found();
}

#if K3L_AT_LEAST(2,0,0)
std::string Verbose::signGroupII(KSignGroupII group, R2CountryType r2_country, Verbose::Presentation fmt)
#else
std::string Verbose::signGroupII(KSignGroupII group, Verbose::Presentation fmt)
#endif
{
    try
    {
#if K3L_AT_LEAST(2,0,0)
        return internal_signGroupII(group, r2_country);
#else
        return internal_signGroupII(group);
#endif
    }
    catch (internal_not_found & e)
    {
        PRESENTATION_CHECK_RETURN(fmt,
            STG(FMT("[KSignGroupII='%d']") % (int)group),
            STG(FMT("Unknown group II (%d)") % (int)group));
    }
}

#if K3L_AT_LEAST(2,0,0)
std::string Verbose::callFail(KSignaling sig, R2CountryType country, int32 info, Verbose::Presentation fmt)
#else
std::string Verbose::callFail(KSignaling sig, int32 info, Verbose::Presentation fmt)
#endif
{
    try
    {
        switch (sig)
        {
            case ksigInactive:
                throw internal_not_found();

            case ksigAnalog:
                if (('a' <= ((char)info) && 'z' >= ((char)info)) || ('A' <= ((char)info) && 'Z' >= ((char)info)))
                    return STG(FMT("%c") % (char)info);
                else
                    throw internal_not_found();

#if K3L_AT_LEAST(1,5,4)
            case ksigLineSide:
#endif
#if K3L_EXACT(2,1,0)
            case ksigISUP:
            case ksigFax:
#endif
#if K3L_AT_LEAST(1,6,0)
            case ksigCAS_EL7:
            case ksigE1LC:
                return "NOT IMPLEMENTED";

            case ksigAnalogTerminal:
#endif
            case ksigContinuousEM:
            case ksigPulsedEM:

            case ksigR2Digital:
            case ksigOpenR2:
#if K3L_AT_LEAST(2,0,0)
                return internal_signGroupB((KSignGroupB)info, country);
#else
                return internal_signGroupB((KSignGroupB)info);
#endif

            case ksigOpenCAS:
            case ksigUserR2Digital:
#if K3L_AT_LEAST(2,0,0)
                return internal_signGroupB((KSignGroupB)info, R2_COUNTRY_BRA);
#else
                return internal_signGroupB((KSignGroupB)info);
#endif

#if K3L_AT_LEAST(1,5,0)
            case ksigSIP:
                return internal_sipFailures((KSIP_Failures)info);
#endif

#if K3L_AT_LEAST(1,5,1)
            case ksigOpenCCS:
            case ksigPRI_EndPoint:
            case ksigPRI_Network:
            case ksigPRI_Passive:
#if K3L_AT_LEAST(2,2,0)
            case ksigISUP:
            case ksigISUPPassive:
#endif
                return internal_isdnCause((KQ931Cause)info);
#endif

#if K3L_AT_LEAST(1,6,0)
            case ksigGSM:
                return internal_gsmCallCause((KGsmCallCause)info);
#endif
        }
    }
    catch (internal_not_found & e)
    {
        /* this exception is used for breaking the control flow */
    }

    PRESENTATION_CHECK_RETURN(fmt,
        STG(FMT("[%s, callFail='%d']") % signaling(sig, fmt) % (int)info),
        STG(FMT("Unknown call fail code for '%s' (%d)") % signaling(sig, fmt) % (int)info));
}

std::string Verbose::channelFail(KSignaling sig, int32 code, Verbose::Presentation fmt)
{
    try
    {
        switch (sig)
        {
            case ksigInactive:
            case ksigAnalog:
            case ksigSIP:
#if K3L_EXACT(2,1,0)
            case ksigISUP:
            case ksigFax:
#endif
                throw internal_not_found();

#if K3L_AT_LEAST(1,6,0)
            case ksigGSM:
                return internal_gsmMobileCause((KGsmMobileCause)code);

            case ksigAnalogTerminal:
            case ksigCAS_EL7:
            case ksigE1LC:
#endif

            case ksigContinuousEM:
            case ksigPulsedEM:

            case ksigLineSide:

            case ksigOpenCAS:
            case ksigOpenR2:
            case ksigR2Digital:
            case ksigUserR2Digital:
                switch ((KChannelFail)code)
                {
                    case kfcRemoteFail:         return presentation(fmt, "kfcRemoteFail",         "Remote failure");
                    case kfcLocalFail:          return presentation(fmt, "kfcLocalFail",          "Local failure");
                    case kfcRemoteLock:         return presentation(fmt, "kfcRemoteLock",         "Remote lock");
                    case kfcLineSignalFail:     return presentation(fmt, "kfcLineSignalFail",     "Line signal failure");
                    case kfcAcousticSignalFail: return presentation(fmt, "kfcAcousticSignalFail", "Acoustic signal failure");
                }

                throw internal_not_found();

#if K3L_AT_LEAST(1,5,1)
            case ksigOpenCCS:
            case ksigPRI_EndPoint:
            case ksigPRI_Network:
            case ksigPRI_Passive:
#if K3L_AT_LEAST(2,2,0)
            case ksigISUP:
            case ksigISUPPassive:
#endif
                return internal_isdnCause((KQ931Cause)code);
#endif
        }
    }
    catch (internal_not_found & e)
    {
        /* this exception is used for breaking the control flow */
    }

    PRESENTATION_CHECK_RETURN(fmt,
        STG(FMT("[%s, channelFail='%d']") % signaling(sig, fmt) % (int)code),
        STG(FMT("Unknown channel fail code for '%s' (%d)") % signaling(sig, fmt) % (int)code));
}

std::string Verbose::internalFail(KInternalFail inf, Verbose::Presentation fmt)
{
    switch (inf)
    {
        case kifInterruptCtrl:     return presentation(fmt, "kifInterruptCtrl",     "Interrupt control");
        case kifCommunicationFail: return presentation(fmt, "kifCommunicationFail", "Communication failure");
        case kifProtocolFail:      return presentation(fmt, "kifProtocolFail",      "Protocol failure");
        case kifInternalBuffer:    return presentation(fmt, "kifInternalBuffer",    "Internal buffer");
        case kifMonitorBuffer:     return presentation(fmt, "kifMonitorBuffer",     "Monitor buffer");
        case kifInitialization:    return presentation(fmt, "kifInitialization",    "Initialization");
        case kifInterfaceFail:     return presentation(fmt, "kifInterfaceFail",     "Interface failure");
        case kifClientCommFail:    return presentation(fmt, "kifClientCommFail",    "Client communication failure");
    }

    PRESENTATION_CHECK_RETURN(fmt,
        STG(FMT("[KInternalFail='%d']") % (int)inf),
        STG(FMT("Unknown internal failure (%d)") % (int)inf));
}

std::string Verbose::linkErrorCounter(KLinkErrorCounter ec, Verbose::Presentation fmt)
{
    switch (ec)
    {
        case klecChangesToLock:     return presentation(fmt, "klecChangesToLock",     "Changes to lock");
        case klecLostOfSignal:      return presentation(fmt, "klecLostOfSignal",      "Lost of signal");
        case klecAlarmNotification: return presentation(fmt, "klecAlarmNotification", "Alarm notification");
        case klecLostOfFrame:       return presentation(fmt, "klecLostOfFrame",       "Lost of frame");
        case klecLostOfMultiframe:  return presentation(fmt, "klecLostOfMultiframe",  "Lost of multiframe");
        case klecRemoteAlarm:       return presentation(fmt, "klecRemoteAlarm",       "Remote alarm");
        case klecUnknowAlarm:       return presentation(fmt, "klecUnknowAlarm",       "Slip alarm");
        case klecPRBS:              return presentation(fmt, "klecPRBS",              "PRBS");
        case klecWrogrBits:         return presentation(fmt, "klecWrongBits",         "Wrong bits");
        case klecJitterVariation:   return presentation(fmt, "klecJitterVariation",   "Jitter variation");
        case klecFramesWithoutSync: return presentation(fmt, "klecFramesWithoutSync", "Frames without sync");
        case klecMultiframeSignal:  return presentation(fmt, "klecMultiframeSignal",  "Multiframe Signal");
        case klecFrameError:        return presentation(fmt, "klecFrameError",        "Frame error");
        case klecBipolarViolation:  return presentation(fmt, "klecBipolarViolation",  "Bipolar violation");
        case klecCRC4:              return presentation(fmt, "klecCRC4",              "CRC4 error");
        case klecCount:             return ""; /* this should never be verbosed */
    }

    PRESENTATION_CHECK_RETURN(fmt,
        STG(FMT("[KLinkErrorCounter='%d']") % (int)ec),
        STG(FMT("Unknown link error counter (%d)") % (int)ec));
}

std::string Verbose::callStatus(KCallStatus code, Verbose::Presentation fmt)
{
    switch (code)
    {
        case kcsFree:       return presentation(fmt, "kcsFree",     "Free");
        case kcsIncoming:   return presentation(fmt, "kcsIncoming", "Incoming");
        case kcsOutgoing:   return presentation(fmt, "kcsOutgoing", "Outgoing");
        case kcsFail:       return presentation(fmt, "kcsFail",     "Failure");
    }

    PRESENTATION_CHECK_RETURN(fmt,
        STG(FMT("[KCallStatus='%d']") % (int)code),
        STG(FMT("Unknown call status (%d)") % (int)code));
}

std::string Verbose::linkStatus(KSignaling sig, int32 code, Verbose::Presentation fmt)
{
    switch (sig)
    {
        case ksigInactive:
            return presentation(fmt, "[ksigInactive]", "Inactive trunk");

        case ksigAnalog:
            return presentation(fmt, "[ksigAnalog]", "Analog trunk");

#if K3L_AT_LEAST(1,4,1)
        case ksigSIP:
            return presentation(fmt, "[ksigSIP]", "SIP trunk");
#endif

#if K3L_AT_LEAST(1,6,0)
        case ksigGSM:
            return presentation(fmt, "[ksigGSM]", "GSM trunk");
#endif

#if K3L_EXACT(2,1,0)
        case ksigFax:
            return presentation(fmt, "[ksigFax]", "FAX");
#endif
        case ksigContinuousEM:
        case ksigPulsedEM:

        case ksigOpenCAS:
        case ksigOpenR2:
        case ksigR2Digital:
        case ksigUserR2Digital:

#if K3L_AT_LEAST(1,5,1)
        case ksigOpenCCS:
        case ksigPRI_EndPoint:
        case ksigPRI_Network:
        case ksigPRI_Passive:
#endif
#if K3L_AT_LEAST(1,5,3)
        case ksigLineSide:
#endif
#if K3L_AT_LEAST(1,6,0)
        case ksigAnalogTerminal:
        case ksigCAS_EL7:
        case ksigE1LC:
#endif
#if K3L_AT_LEAST(2,2,0)
        case ksigISUP:
        case ksigISUPPassive:
#endif
            if (kesOk == code)
            {
                return presentation(fmt, "kesOk", "Up");
            }
            else
            {
                Strings::Merger strs;

                if (kesSignalLost & code)         strs.add(presentation(fmt, "SignalLost",         "Signal lost"));
                if (kesNetworkAlarm & code)       strs.add(presentation(fmt, "NetworkAlarm",       "Network alarm"));
                if (kesFrameSyncLost & code)      strs.add(presentation(fmt, "FrameSyncLost",      "Frame sync lost"));
                if (kesMultiframeSyncLost & code) strs.add(presentation(fmt, "MultiframeSyncLost", "Multiframe sync lost"));
                if (kesRemoteAlarm & code)        strs.add(presentation(fmt, "RemoteAlarm",        "Remote alarm"));
                if (kesHighErrorRate & code)      strs.add(presentation(fmt, "HighErrorRate",      "High error rate"));
                if (kesUnknownAlarm & code)       strs.add(presentation(fmt, "UnknownAlarm",       "Slip alarm"));
                if (kesE1Error & code)            strs.add(presentation(fmt, "E1Error",            "E1 error"));

                PRESENTATION_CHECK_RETURN(fmt,
                    STG(FMT("kes{%s}") % strs.merge(",")),
                    strs.merge(", "));
            }
    }

    PRESENTATION_CHECK_RETURN(fmt,
        STG(FMT("[%s, linkStatus='%d']") % signaling(sig) % (int)code),
        STG(FMT("Unknown link status for '%s' (%d)") % signaling(sig) % (int)code));
}

std::string Verbose::channelStatus(KSignaling sig, int32 flags, Verbose::Presentation fmt)
{
    try
    {
        switch (sig)
        {
            case ksigInactive:
                return presentation(fmt, "[ksigInactive]", "Inactive channel");

#if K3L_AT_LEAST(1,4,1)
            case ksigSIP:
                return presentation(fmt, "[ksigSIP]", "SIP channel");
#endif
#if K3L_EXACT(2,1,0)
            case ksigISUP:
                return presentation(fmt, "[ksigISUP]", "ISUP trunk");
            case ksigFax:
                return presentation(fmt, "[ksigFax]", "FAX");
#endif

            case ksigAnalog:
#if K3L_AT_LEAST(1,6,0)
                switch ((KFXOChannelStatus)flags)
#else
                switch ((KFXChannelStatus)flags)
#endif
                {
                    case kfcsDisabled:   return presentation(fmt, "kfcsDisabled", "Disabled");
                    case kfcsEnabled:    return presentation(fmt, "kfcsEnabled",  "Enabled");
                }

                throw internal_not_found();

#if K3L_AT_LEAST(1,6,0)
            case ksigAnalogTerminal:
                switch ((KFXSChannelStatus)flags)
                {
                    case kfxsOnHook:   return presentation(fmt, "kfxsOnHook",  "On Hook");
                    case kfxsOffHook:  return presentation(fmt, "kfxsOffHook", "Off Hook");
                    case kfxsRinging:  return presentation(fmt, "kfxsRinging", "Ringing");
                    case kfxsFail:     return presentation(fmt, "kfxsFail",    "Failure");
                }

                throw internal_not_found();

            case ksigGSM:
                switch ((KGsmChannelStatus)flags)
                {
                    case kgsmIdle:            return presentation(fmt, "kgsmIdle",           "Idle");
                    case kgsmCallInProgress:  return presentation(fmt, "kgsmCallInProgress", "Call in progress");
                    case kgsmSMSInProgress:   return presentation(fmt, "kgsmSMSInProgress",  "SMS in progress");
                    case kgsmModemError:      return presentation(fmt, "kgsmModemError",     "Modem error");
                    case kgsmSIMCardError:    return presentation(fmt, "kgsmSIMCardError",   "SIM card error");
                    case kgsmNetworkError:    return presentation(fmt, "kgsmNetworkError",   "Network error");
                    case kgsmNotReady:        return presentation(fmt, "kgsmNotReady",       "Initializing");
                }

                throw internal_not_found();
#endif

            /* deprecated, but still.. */
            case ksigPulsedEM:
            case ksigContinuousEM:

            case ksigOpenCAS:
            case ksigOpenR2:
            case ksigR2Digital:
            case ksigUserR2Digital:

#if K3L_AT_LEAST(1,5,1)
            case ksigOpenCCS:
            case ksigPRI_EndPoint:
            case ksigPRI_Network:
            case ksigPRI_Passive:
#endif
#if K3L_AT_LEAST(1,5,3)
            case ksigLineSide:
#endif
#if K3L_AT_LEAST(1,6,0)
            case ksigCAS_EL7:
            case ksigE1LC:
#endif
#if K3L_AT_LEAST(2,2,0)
            case ksigISUP:
            case ksigISUPPassive:
#endif
            {
                if (flags == kecsFree)
                {
                    return presentation(fmt, "kecsFree", "Free");
                }
                else
                {
                    Strings::Merger strs;

                    if (flags & kecsBusy)
                        strs.add("Busy");

                    switch (flags & 0x06)
                    {
                        case kecsOutgoing:
                            strs.add("Outgoing");
                             break;
                        case kecsIncoming:
                            strs.add("Incoming");
                            break;
                        case kecsLocked:
                            strs.add("Locked");
                        default:
                            break;
                    }

                    int32 value = (flags & 0xf0);

                    if (kecsOutgoingLock & value)
                        strs.add(presentation(fmt, "OutgoingLock", "Outgoing Lock"));

                    if (kecsLocalFail & value)
                        strs.add(presentation(fmt, "LocalFail", "Local Failure"));

                    if (kecsIncomingLock & value)
                        strs.add(presentation(fmt, "IncomingLock", "Incoming Lock"));

                    if (kecsRemoteLock & value)
                        strs.add(presentation(fmt, "RemoteLock", "Remote Lock"));

                    PRESENTATION_CHECK_RETURN(fmt,
                        STG(FMT("kecs{%s}") % strs.merge(",")),
                        strs.merge(", "));
                }

                throw internal_not_found();
            }
        }
    }
    catch (internal_not_found & e)
    {
        /* we use this exception to break the control flow */
    }

    PRESENTATION_CHECK_RETURN(fmt,
        STG(FMT("[%s, channelStatus='%d']") % signaling(sig) % flags),
        STG(FMT("Unknown channel status for '%s' (%d)") % signaling(sig) % flags));
}

std::string Verbose::status(KLibraryStatus code, Verbose::Presentation fmt)
{
    switch (code)
    {
        case ksSuccess:        return presentation(fmt, "ksSuccess",        "Success");
        case ksFail:           return presentation(fmt, "ksFail",           "Failure");
        case ksTimeOut:        return presentation(fmt, "ksTimeOut",        "Time Out");
        case ksBusy:           return presentation(fmt, "ksBusy",           "Busy");
        case ksLocked:         return presentation(fmt, "ksLocked",         "Locked");
        case ksInvalidParams:  return presentation(fmt, "ksInvalidParams",  "Invalid Parameters");
        case ksEndOfFile:      return presentation(fmt, "ksEndOfFile",      "End of File");
        case ksInvalidState:   return presentation(fmt, "ksInvalidState",   "Invalid State");
        case ksServerCommFail: return presentation(fmt, "ksServerCommFail", "Communication Failure");
        case ksOverflow:       return presentation(fmt, "ksOverflow",       "Overflow");
        case ksUnderrun:       return presentation(fmt, "ksUnderrun",       "Underrun");

#if K3L_AT_LEAST(1,4,0)
        case ksNotFound:       return presentation(fmt, "ksNotFound",       "Not Found");
        case ksNotAvailable:   return presentation(fmt, "ksNotAvaiable",    "Not Available");
#endif
    }

    PRESENTATION_CHECK_RETURN(fmt,
        STG(FMT("[KLibraryStatus='%d']") % (int)code),
        STG(FMT("Unknown library status (%d)") % (int)code));
}

std::string Verbose::h100configIndex(KH100ConfigIndex code, Verbose::Presentation fmt)
{
    switch (code)
    {
        case khciDeviceMode:         return presentation(fmt, "khciDeviceMode",      "Device Mode");
        case khciMasterGenClock:     return presentation(fmt, "khciMasterGenClock",  "Master Generated Clock");
        case khciCTNetRefEnable:     return presentation(fmt, "khciCTNetRefEnable",  "CTBus Network Reference Enable");
        case khciSCbusEnable:        return presentation(fmt, "khciSCbusEnable",     "SCBus Enable");
        case khciHMVipEnable:        return presentation(fmt, "khciHMVipEnable",     "HMVip Enable");
        case khciMVip90Enable:       return presentation(fmt, "khciMVip90Enable",    "MVip90 Enable");
        case khciCTbusDataEnable:    return presentation(fmt, "khciCTbusDataEnable", "CTBus Data Enable");
        case khciCTbusFreq03_00:     return presentation(fmt, "khciCTbusFreq03_00",  "CTBus Frequency 03 00"); // TODO: find better name
        case khciCTbusFreq07_04:     return presentation(fmt, "khciCTbusFreq07_04",  "CTBus Frequency 07 04"); // TODO: find better name
        case khciCTbusFreq11_08:     return presentation(fmt, "khciCTbusFreq11_08",  "CTBus Frequency 11 08"); // TODO: find better name
        case khciCTbusFreq15_12:     return presentation(fmt, "khciCTbusFreq15_12",  "CTBus Frequency 15 12"); // TODO: find better name
        case khciMax:                return presentation(fmt, "khciMax",             "Max"); // TODO: find better name
        case khciMasterDevId:        return presentation(fmt, "khciMasterDevId",     "Master Device Number");
        case khciSecMasterDevId:     return presentation(fmt, "khciSecMasterDevId",  "Secondary Master Device Number");
        case khciCtNetrefDevId:      return presentation(fmt, "khciCtNetrefDevId",   "CTBus Network Reference Device Number");
#if K3L_AT_LEAST(1,6,0)
        case khciMaxH100ConfigIndex: return ""; /* do not verbose this value */
#endif
    }

    PRESENTATION_CHECK_RETURN(fmt,
        STG(FMT("[KH100ConfigIndex='%d']") % (int)code),
        STG(FMT("Unknown H.100 config index (%d)") % (int)code));
}

#if K3L_AT_LEAST(1,6,0)
std::string Verbose::callStartInfo(KCallStartInfo code, Verbose::Presentation fmt)
{
    switch (code)
    {
        case kcsiHumanAnswer:         return presentation(fmt, "kcsiHumanAnswer",         "Human Answer");
        case kcsiAnsweringMachine:    return presentation(fmt, "kcsiAnsweringMachine",    "Answering Machine");
        case kcsiCellPhoneMessageBox: return presentation(fmt, "kcsiCellPhoneMessageBox", "Cell Phone Message Box");
        case kcsiCarrierMessage:      return presentation(fmt, "kcsiCarrierMessage",      "Carrier Message");
        case kcsiUnknown:             return presentation(fmt, "kcsiUnknown",             "Unknown");
    }

    PRESENTATION_CHECK_RETURN(fmt,
        STG(FMT("[KCallStartInfo='%d']") % (int)code),
        STG(FMT("Unknown call answer info (%d)") % (int)code));
}

std::string Verbose::gsmCallCause(KGsmCallCause code, Verbose::Presentation fmt)
{
    try
    {
        return internal_gsmCallCause(code, fmt);
    }
    catch (internal_not_found & e)
    {
        PRESENTATION_CHECK_RETURN(fmt,
            STG(FMT("[KGsmCallCause='%d']") % (int)code),
            STG(FMT("Unknown GSM call cause (%d)") % (int)code));
    }
}

std::string Verbose::internal_gsmCallCause(KGsmCallCause code, Verbose::Presentation fmt)
{
    switch (code)
    {
        case kgccNone:                               return presentation(fmt, "kgccNone",                      "None");
        case kgccUnallocatedNumber:                  return presentation(fmt, "kgccUnallocatedNumber",         "Unallocated number");
        case kgccNoRouteToDest:                      return presentation(fmt, "kgccNoRouteToDest",             "No route to destination");
        case kgccChannelUnacceptable:                return presentation(fmt, "kgccChannelUnacceptable",       "Channel unacceptable");
        case kgccOperatorDeterminedBarring:          return presentation(fmt, "kgccOperatorDeterminedBarring", "Operator determined barring");
        case kgccNormalCallClear:                    return presentation(fmt, "kgccNormalCallClear",           "Normal call clear");
        case kgccUserBusy:                           return presentation(fmt, "kgccUserBusy",                  "User busy");
        case kgccNoUserResponding:                   return presentation(fmt, "kgccNoUserResponding",          "No user responding");
        case kgccNoAnswerFromUser:                   return presentation(fmt, "kgccNoAnswerFromUser",          "No answer from user");
        case kgccCallRejected:                       return presentation(fmt, "kgccCallRejected",              "Call rejected");
        case kgccNumberChanged:                      return presentation(fmt, "kgccNumberChanged",             "Number changed");
        case kgccNonSelectedUserClear:               return presentation(fmt, "kgccNonSelectedUserClear",      "Non Selected user clear");
        case kgccDestinationOutOfOrder:              return presentation(fmt, "kgccDestinationOutOfOrder",     "Destination out of order");
        case kgccInvalidNumberFormat:                return presentation(fmt, "kgccInvalidNumberFormat",       "Invalid number format");
        case kgccFacilityRejected:                   return presentation(fmt, "kgccFacilityRejected",          "Facility rejected");
        case kgccRespStatusEnquiry:                  return presentation(fmt, "kgccRespStatusEnquiry",         "Response status enquiry");
        case kgccNormalUnspecified:                  return presentation(fmt, "kgccNormalUnspecified",         "Normal, unspecified");
        case kgccNoCircuitChannelAvail:              return presentation(fmt, "kgccNoCircuitChannelAvail",     "No circuit channel available");
        case kgccNetworkOutOfOrder:                  return presentation(fmt, "kgccNetworkOutOfOrder",         "Network out of order");
        case kgccTemporaryFailure:                   return presentation(fmt, "kgccTemporaryFailure",          "Temporary failure");
        case kgccSwitchCongestion:                   return presentation(fmt, "kgccSwitchCongestion",          "Switch congestion");
        case kgccAccessInfoDiscarded:                return presentation(fmt, "kgccAccessInfoDiscarded",       "Access information discarded");
        case kgccRequestedChannelUnav:               return presentation(fmt, "kgccRequestedChannelUnav",      "Requested channel unavailable");
        case kgccResourceUnavailable:                return presentation(fmt, "kgccResourceUnavailable",       "Resource unavailable");
        case kgccQosUnavailable:                     return presentation(fmt, "kgccQosUnavailable",            "QoS unavailable");
        case kgccReqFacilityNotSubsc:                return presentation(fmt, "kgccReqFacilityNotSubsc",       "Request facility not subscribed");
        case kgccCallBarredWitchCUG:                 return presentation(fmt, "kgccCallBarredWitchCUG",        "Call barred with UG");
        case kgccBearerCapabNotAuthor:               return presentation(fmt, "kgccBearerCapabNotAuthor",      "Bearer capability not authorized");
        case kgccBearerCapabNotAvail:                return presentation(fmt, "kgccBearerCapabNotAvail",       "Bearer capability not available");
        case kgccServiceNotAvailable:                return presentation(fmt, "kgccServiceNotAvailable",       "Service not available");
        case kgccBcNotImplemented:                   return presentation(fmt, "kgccBcNotImplemented",          "Bearer capability not implemented");
        case kgccReqFacilityNotImplem:               return presentation(fmt, "kgccReqFacilityNotImplem",      "Request facility not implemented");
        case kgccOnlyRestrictedBcAvail:              return presentation(fmt, "kgccOnlyRestrictedBcAvail",     "Only restricted bearer capability available");
        case kgccServiceNotImplemented:              return presentation(fmt, "kgccServiceNotImplemented",     "Service not implemented");
        case kgccInvalidCrv:                         return presentation(fmt, "kgccInvalidCrv",                "Invalid call reference value");
        case kgccUserNotMemberOfCUG:                 return presentation(fmt, "kgccUserNotMemberOfCUG",        "User not member of UG");
        case kgccIncompatibleDestination:            return presentation(fmt, "kgccIncompatibleDestination",   "Incompatible destination");
        case kgccInvalidTransitNetSel:               return presentation(fmt, "kgccInvalidTransitNetSel",      "Invalid transit network selected");
        case kgccInvalidMessage:                     return presentation(fmt, "kgccInvalidMessage",            "Invalid message");
        case kgccMissingMandatoryIe:                 return presentation(fmt, "kgccMissingMandatoryIe",        "Missing mandatory information element");
        case kgccMsgTypeNotImplemented:              return presentation(fmt, "kgccMsgTypeNotImplemented",     "Message type not implemented");
        case kgccMsgIncompatWithState:               return presentation(fmt, "kgccMsgIncompatWithState",      "Message incompatible with state");
        case kgccIeNotImplemented:                   return presentation(fmt, "kgccIeNotImplemented",          "Information element not implemented");
        case kgccInvalidIe:                          return presentation(fmt, "kgccInvalidIe",                 "Invalid information element");
        case kgccMsgIncompatWithState2:              return presentation(fmt, "kgccMsgIncompatWithState2",     "Message incompatible with state (2)");
        case kgccRecoveryOnTimerExpiry:              return presentation(fmt, "kgccRecoveryOnTimerExpiry",     "Recovery on timer expiry");
        case kgccProtocolError:                      return presentation(fmt, "kgccProtocolError",             "Protocol error");
        case kgccInterworking:                       return presentation(fmt, "kgccInterworking",              "Interworking");
    }

    throw internal_not_found();
}

std::string Verbose::gsmMobileCause(KGsmMobileCause code, Verbose::Presentation fmt)
{
    try
    {
        return internal_gsmMobileCause(code, fmt);
    }
    catch (internal_not_found & e)
    {
        PRESENTATION_CHECK_RETURN(fmt,
            STG(FMT("[KGsmMobileCause='%d']") % (int)code),
            STG(FMT("Unknown GSM mobile cause (%d)") % (int)code));
    }
}

std::string Verbose::internal_gsmMobileCause(KGsmMobileCause code, Verbose::Presentation fmt)
{
    switch (code)
    {
        case kgmcPhoneFailure:                    return presentation(fmt, "kgmcPhoneFailure",               "Phone failure");
        case kgmcNoConnectionToPhone:             return presentation(fmt, "kgmcNoConnectionToPhone",        "No connection to phone");
        case kgmcPhoneAdaptorLinkReserved:        return presentation(fmt, "kgmcPhoneAdaptorLinkReserved",   "Phone adaptor link reserved");
#if 0 /* this changed during K3L 1.6.0 development cycle... */
        case kgmcCoperationNotAllowed:            return presentation(fmt, "kgmcCoperationNotAllowed",       "");
        case kgmcCoperationNotSupported:          return presentation(fmt, "kgmcCoperationNotSupported",     "");
#else
        case kgmcOperationNotAllowed:             return presentation(fmt, "kgmcOperationNotAllowed",        "Operation not allowed");
        case kgmcOperationNotSupported:           return presentation(fmt, "kgmcOperationNotSupported",      "Operation not supported");
#endif
        case kgmcPH_SIMPINRequired:               return presentation(fmt, "kgmcPH_SIMPINRequired",          "Phone SIM PIN required");
        case kgmcPH_FSIMPINRequired:              return presentation(fmt, "kgmcPH_FSIMPINRequired",         "Phone FSIM PIN required");
        case kgmcPH_FSIMPUKRequired:              return presentation(fmt, "kgmcPH_FSIMPUKRequired",         "Phone FSIM PUK required");
        case kgmcSIMNotInserted:                  return presentation(fmt, "kgmcSIMNotInserted",             "SIM not inserted");
        case kgmcSIMPINRequired:                  return presentation(fmt, "kgmcSIMPINRequired",             "SIM PIN required");
        case kgmcSIMPUKRequired:                  return presentation(fmt, "kgmcSIMPUKRequired",             "SIM PUK required");
        case kgmcSIMFailure:                      return presentation(fmt, "kgmcSIMFailure",                 "SIM failure");
        case kgmcSIMBusy:                         return presentation(fmt, "kgmcSIMBusy",                    "SIM busy");
        case kgmcSIMWrong:                        return presentation(fmt, "kgmcSIMWrong",                   "SIM wrong");
        case kgmcIncorrectPassword:               return presentation(fmt, "kgmcIncorrectPassword",          "Incorrect password");
        case kgmcSIMPIN2Required:                 return presentation(fmt, "kgmcSIMPIN2Required",            "SIM PIN2 required");
        case kgmcSIMPUK2Required:                 return presentation(fmt, "kgmcSIMPUK2Required",            "SIM PUK2 required");
        case kgmcMemoryFull:                      return presentation(fmt, "kgmcMemoryFull",                 "Memory full");
        case kgmcInvalidIndex:                    return presentation(fmt, "kgmcInvalidIndex",               "Invalid index");
        case kgmcNotFound:                        return presentation(fmt, "kgmcNotFound",                   "Not found");
        case kgmcMemoryFailure:                   return presentation(fmt, "kgmcMemoryFailure",              "Memory failure");
        case kgmcTextStringTooLong:               return presentation(fmt, "kgmcTextStringTooLong",          "Text string too long");
        case kgmcInvalidCharInTextString:         return presentation(fmt, "kgmcInvalidCharInTextString",    "Invalid character in text string");
        case kgmcDialStringTooLong:               return presentation(fmt, "kgmcDialStringTooLong",          "Dial string too long");
        case kgmcInvalidCharInDialString:         return presentation(fmt, "kgmcInvalidCharInDialString",    "Invalid character in dial string");
        case kgmcNoNetworkService:                return presentation(fmt, "kgmcNoNetworkService",           "No network service");
        case kgmcNetworkTimeout:                  return presentation(fmt, "kgmcNetworkTimeout",             "Network timeout");
        case kgmcNetworkNotAllowed:               return presentation(fmt, "kgmcNetworkNotAllowed",          "Network not allowed");
        case kgmcCommandAborted:                  return presentation(fmt, "kgmcCommandAborted",             "Command aborted");
        case kgmcNumParamInsteadTextParam:        return presentation(fmt, "kgmcNumParamInsteadTextParam",   "Number parameter instead of text parameter");
        case kgmcTextParamInsteadNumParam:        return presentation(fmt, "kgmcTextParamInsteadNumParam",   "Text parameter instead of number parameter");
        case kgmcNumericParamOutOfBounds:         return presentation(fmt, "kgmcNumericParamOutOfBounds",    "Numeric parameter out of bounds");
        case kgmcTextStringTooShort:              return presentation(fmt, "kgmcTextStringTooShort",         "Text string too short");
        case kgmcNetworkPINRequired:              return presentation(fmt, "kgmcNetworkPINRequired",         "Network PIN required");
        case kgmcNetworkPUKRequired:              return presentation(fmt, "kgmcNetworkPUKRequired",         "Network PUK required");
        case kgmcNetworkSubsetPINRequired:        return presentation(fmt, "kgmcNetworkSubsetPINRequired",   "Network subset PIN required");
        case kgmcNetworkSubnetPUKRequired:        return presentation(fmt, "kgmcNetworkSubnetPUKRequired",   "Network subset PUK required");
        case kgmcServiceProviderPINRequired:      return presentation(fmt, "kgmcServiceProviderPINRequired", "Network service provider PIN required");
        case kgmcServiceProviderPUKRequired:      return presentation(fmt, "kgmcServiceProviderPUKRequired", "Network service provider PUK required");
        case kgmcCorporatePINRequired:            return presentation(fmt, "kgmcCorporatePINRequired",       "Corporate PIN required");
        case kgmcCorporatePUKRequired:            return presentation(fmt, "kgmcCorporatePUKRequired",       "Corporate PUK required");
        case kgmcSIMServiceOptNotSupported:       return presentation(fmt, "kgmcSIMServiceOptNotSupported",  "SIM Service option not supported");
        case kgmcUnknown:                         return presentation(fmt, "kgmcUnknown",                    "Unknown");
        case kgmcIllegalMS_N3:                    return presentation(fmt, "kgmcIllegalMS_N3",               "Illegal MS #3");
        case kgmcIllegalME_N6:                    return presentation(fmt, "kgmcIllegalME_N6",               "Illegal MS #6");
        case kgmcGPRSServicesNotAllowed_N7:       return presentation(fmt, "kgmcGPRSServicesNotAllowed_N7",  "GPRS service not allowed #7");
        case kgmcPLMNNotAllowed_No11:             return presentation(fmt, "kgmcPLMNNotAllowed_No11",        "PLMN not allowed #11");
        case kgmcLocationAreaNotAllowed_N12:      return presentation(fmt, "kgmcLocationAreaNotAllowed_N12", "Location area not allowed #12");
        case kgmcRoamingNotAllowed_N13:           return presentation(fmt, "kgmcRoamingNotAllowed_N13",      "Roaming not allowed #13");
        case kgmcServiceOptNotSupported_N32:      return presentation(fmt, "kgmcServiceOptNotSupported_N32", "Service option not supported #32");
        case kgmcReqServOptNotSubscribed_N33:     return presentation(fmt, "kgmcReqServOptNotSubscribed_N33", "Registration service option not subscribed #33");
        case kgmcServOptTempOutOfOrder_N34:       return presentation(fmt, "kgmcServOptTempOutOfOrder_N34",   "Service option temporary out of order #34");
        case kgmcLongContextActivation:           return presentation(fmt, "kgmcLongContextActivation",       "Long context activation");
        case kgmcUnspecifiedGPRSError:            return presentation(fmt, "kgmcUnspecifiedGPRSError",        "Unspecified GPRS error");
        case kgmcPDPAuthenticationFailure:        return presentation(fmt, "kgmcPDPAuthenticationFailure",    "PDP authentication failure");
        case kgmcInvalidMobileClass:              return presentation(fmt, "kgmcInvalidMobileClass",          "Invalid mobile class");
        case kgmcGPRSDisconnectionTmrActive:      return presentation(fmt, "kgmcGPRSDisconnectionTmrActive",  "GPRS disconnection TMR active");
        case kgmcTooManyActiveCalls:              return presentation(fmt, "kgmcTooManyActiveCalls",          "Too many active calls");
        case kgmcCallRejected:                    return presentation(fmt, "kgmcCallRejected",                "Call rejected");
        case kgmcUnansweredCallPending:           return presentation(fmt, "kgmcUnansweredCallPending",       "Unanswered call pending");
        case kgmcUnknownCallingError:             return presentation(fmt, "kgmcUnknownCallingError",         "Unknown calling error");
        case kgmcNoPhoneNumRecognized:            return presentation(fmt, "kgmcNoPhoneNumRecognized",        "No phone number recognized");
        case kgmcCallStateNotIdle:                return presentation(fmt, "kgmcCallStateNotIdle",            "Call state not idle");
        case kgmcCallInProgress:                  return presentation(fmt, "kgmcCallInProgress",              "Call in progress");
        case kgmcDialStateError:                  return presentation(fmt, "kgmcDialStateError",              "Dial state error");
        case kgmcUnlockCodeRequired:              return presentation(fmt, "kgmcUnlockCodeRequired",          "Unlock code required");
        case kgmcNetworkBusy:                     return presentation(fmt, "kgmcNetworkBusy",                 "Network busy");
        case kgmcInvalidPhoneNumber:              return presentation(fmt, "kgmcInvalidPhoneNumber",          "Invalid phone number");
        case kgmcNumberEntryAlreadyStarted:       return presentation(fmt, "kgmcNumberEntryAlreadyStarted",   "Number entry already started");
        case kgmcCancelledByUser:                 return presentation(fmt, "kgmcCancelledByUser",             "Cancelled by user");
        case kgmcNumEntryCouldNotBeStarted:       return presentation(fmt, "kgmcNumEntryCouldNotBeStarted",   "Number entry could not be started");
        case kgmcDataLost:                        return presentation(fmt, "kgmcDataLost",                    "Data lost");
        case kgmcInvalidBessageBodyLength:        return presentation(fmt, "kgmcInvalidBessageBodyLength",    "Invalid message body length");
        case kgmcInactiveSocket:                  return presentation(fmt, "kgmcInactiveSocket",              "Inactive socket");
        case kgmcSocketAlreadyOpen:               return presentation(fmt, "kgmcSocketAlreadyOpen",           "Socket already open");
#if K3L_AT_LEAST(2,1,0)
        case kgmcSuccess:                         return presentation(fmt, "kgmcSuccess",                     "Success");
#endif
    }

    throw internal_not_found();
}

std::string Verbose::gsmSmsCause(KGsmSmsCause code, Verbose::Presentation fmt)
{
    try
    {
        return internal_gsmSmsCause(code, fmt);
    }
    catch (internal_not_found & e)
    {
        PRESENTATION_CHECK_RETURN(fmt,
            STG(FMT("[KGsmSmsCause='%d']") % (int)code),
            STG(FMT("Unknown GSM SMS cause (%d)") % (int)code));
    }
}

std::string Verbose::internal_gsmSmsCause(KGsmSmsCause code, Verbose::Presentation fmt)
{
    switch (code)
    {
#if K3L_AT_LEAST(2,1,0)
        case kgscNone:                           return presentation(fmt, "kgscNone",                         "None");
#endif
        case kgscUnassigned:                     return presentation(fmt, "kgscUnassigned",                   "Unassigned number");
        case kgscOperatorDeterminedBarring:      return presentation(fmt, "kgscOperatorDeterminedBarring",    "Operator determined barring");
        case kgscCallBarred:                     return presentation(fmt, "kgscCallBarred",                   "Call barred");
        case kgscSMSTransferRejected:            return presentation(fmt, "kgscSMSTransferRejected",          "SMS transfer rejected");
        case kgscDestinationOutOfService:        return presentation(fmt, "kgscDestinationOutOfService",      "Destination out of service");
        case kgscUnidentifiedSubscriber:         return presentation(fmt, "kgscUnidentifiedSubscriber",       "Unidentified subscriber");
        case kgscFacilityRejected:               return presentation(fmt, "kgscFacilityRejected",             "Facility rejected");
        case kgscUnknownSubscriber:              return presentation(fmt, "kgscUnknownSubscriber",            "Unknown subscriber");
        case kgscNetworkOutOfOrder:              return presentation(fmt, "kgscNetworkOutOfOrder",            "Network out of order");
        case kgscTemporaryFailure:               return presentation(fmt, "kgscTemporaryFailure",             "Temporary failure");
        case kgscCongestion:                     return presentation(fmt, "kgscCongestion",                   "Congestion");
        case kgscResourcesUnavailable:           return presentation(fmt, "kgscResourcesUnavailable",         "Resources unavailable");
        case kgscFacilityNotSubscribed:          return presentation(fmt, "kgscFacilityNotSubscribed",        "Facility not subscribed");
        case kgscFacilityNotImplemented:         return presentation(fmt, "kgscFacilityNotImplemented",       "Facility not implemented");
        case kgscInvalidSMSTransferRefValue:     return presentation(fmt, "kgscInvalidSMSTransferRefValue",   "Invalid SMS transfer reference value");
        case kgscInvalidMessage:                 return presentation(fmt, "kgscInvalidMessage",               "Invalid message");
        case kgscInvalidMandatoryInformation:    return presentation(fmt, "kgscInvalidMandatoryInformation",  "Invalid mandatory information");
        case kgscMessageTypeNonExistent:         return presentation(fmt, "kgscMessageTypeNonExistent",       "Message type non existent");
        case kgscMsgNotCompatWithSMProtState:    return presentation(fmt, "kgscMsgNotCompatWithSMProtState",  "Message not compatible with SMS protection state");
        case kgscInformationElementNonExiste:    return presentation(fmt, "kgscInformationElementNonExiste",  "Information element non existent");
        case kgscProtocolError:                  return presentation(fmt, "kgscProtocolError",                "Protocol error");
        case kgscInterworking:                   return presentation(fmt, "kgscInterworking",                 "Interworking");
        case kgscTelematicInterworkingNotSup:    return presentation(fmt, "kgscTelematicInterworkingNotSup",  "Telematic interworking not supported");
        case kgscSMSTypeZeroNotSupported:        return presentation(fmt, "kgscSMSTypeZeroNotSupported",      "SMS type zero not supported");
        case kgscCannotReplaceSMS:               return presentation(fmt, "kgscCannotReplaceSMS",             "Cannot replace SMS");
        case kgscUnspecifiedTPPIDError:          return presentation(fmt, "kgscUnspecifiedTPPIDError",        "Unspecified TPPID error");
        case kgscAlphabetNotSupported:           return presentation(fmt, "kgscAlphabetNotSupported",         "Alphabet not supported");
        case kgscMessageClassNotSupported:       return presentation(fmt, "kgscMessageClassNotSupported",     "Message class not supported");
        case kgscUnspecifiedTPDCSError:          return presentation(fmt, "kgscUnspecifiedTPDCSError",        "Unspecified TPDCS error");
        case kgscCommandCannotBeActioned:        return presentation(fmt, "kgscCommandCannotBeActioned",      "Command cannot be actioned");
        case kgscCommandUnsupported:             return presentation(fmt, "kgscCommandUnsupported",           "Command unsupported");
        case kgscUnspecifiedTPCommandError:      return presentation(fmt, "kgscUnspecifiedTPCommandError",    "Unspecified TP command error");
        case kgscTPDUNotSupported:               return presentation(fmt, "kgscTPDUNotSupported",             "TPDU not supported");
        case kgscSCBusy:                         return presentation(fmt, "kgscSCBusy",                       "SC busy");
        case kgscNoSCSubscription:               return presentation(fmt, "kgscNoSCSubscription",             "No SC subscription");
        case kgscSCSystemFailure:                return presentation(fmt, "kgscSCSystemFailure",              "SC system failure");
        case kgscInvalidSMEAddress:              return presentation(fmt, "kgscInvalidSMEAddress",            "Invalid SME address");
        case kgscDestinationSMEBarred:           return presentation(fmt, "kgscDestinationSMEBarred",         "Destination SME barred");
        case kgscSMRejectedDuplicateSM:          return presentation(fmt, "kgscSMRejectedDuplicateSM",        "SM rejected duplicate SM");
        case kgscTPVPFNotSupported:              return presentation(fmt, "kgscTPVPFNotSupported",            "TPVPF not supported");
        case kgscTPVPNotSupported:               return presentation(fmt, "kgscTPVPNotSupported",             "TPVP not supported");
        case kgscSIMSMSStorageFull:              return presentation(fmt, "kgscSIMSMSStorageFull",            "SIM SMS storage full");
        case kgscNoSMSStorageCapabilityInSIM:    return presentation(fmt, "kgscNoSMSStorageCapabilityInSIM",  "No SMS storage capability in SIM");
        case kgscErrorInMS:                      return presentation(fmt, "kgscErrorInMS",                    "Error in SMS");
        case kgscMemoryCapacityExceeded:         return presentation(fmt, "kgscMemoryCapacityExceeded",       "Memory capatity exceeded");
        case kgscSIMDataDownloadError:           return presentation(fmt, "kgscSIMDataDownloadError",         "SIM data download error");
        case kgscUnspecifiedError:               return presentation(fmt, "kgscUnspecifiedError",             "Unspecified error");
        case kgscPhoneFailure:                   return presentation(fmt, "kgscPhoneFailure",                 "Phone failure");
        case kgscSmsServiceReserved:             return presentation(fmt, "kgscSmsServiceReserved",           "SMS service reserved");
        case kgscOperationNotAllowed:            return presentation(fmt, "kgscOperationNotAllowed",          "Operation not allowed");
        case kgscOperationNotSupported:          return presentation(fmt, "kgscOperationNotSupported",        "Operation not supported");
        case kgscInvalidPDUModeParameter:        return presentation(fmt, "kgscInvalidPDUModeParameter",      "Invalid PDU mode parameter");
        case kgscInvalidTextModeParameter:       return presentation(fmt, "kgscInvalidTextModeParameter",     "Invalid text mode parameter");
        case kgscSIMNotInserted:                 return presentation(fmt, "kgscSIMNotInserted",               "SIM not inserted");
        case kgscSIMPINNecessary:                return presentation(fmt, "kgscSIMPINNecessary",              "SIM PIN necessary");
        case kgscPH_SIMPINNecessary:             return presentation(fmt, "kgscPH_SIMPINNecessary",           "Phone SIM PIN necessary");
        case kgscSIMFailure:                     return presentation(fmt, "kgscSIMFailure",                   "SIM failure");
        case kgscSIMBusy:                        return presentation(fmt, "kgscSIMBusy",                      "SIM busy");
        case kgscSIMWrong:                       return presentation(fmt, "kgscSIMWrong",                     "SIM wrong");
        case kgscMemoryFailure:                  return presentation(fmt, "kgscMemoryFailure",                "Memory failure");
        case kgscInvalidMemoryIndex:             return presentation(fmt, "kgscInvalidMemoryIndex",           "Invalid memory index");
        case kgscMemoryFull:                     return presentation(fmt, "kgscMemoryFull",                   "Memory full");
        case kgscSMSCAddressUnknown:             return presentation(fmt, "kgscSMSCAddressUnknown",           "SMSC address unknown");
        case kgscNoNetworkService:               return presentation(fmt, "kgscNoNetworkService",             "No network service");
        case kgscNetworkTimeout:                 return presentation(fmt, "kgscNetworkTimeout",               "Network timeout");
        case kgscUnknownError:                   return presentation(fmt, "kgscUnknownError",                 "Unknown error");
        case kgscNetworkBusy:                    return presentation(fmt, "kgscNetworkBusy",                  "Network busy");
        case kgscInvalidDestinationAddress:      return presentation(fmt, "kgscInvalidDestinationAddress",    "Invalid destination address");
        case kgscInvalidMessageBodyLength:       return presentation(fmt, "kgscInvalidMessageBodyLength",     "Invalid message body length");
        case kgscPhoneIsNotInService:            return presentation(fmt, "kgscPhoneIsNotInService",          "Phone is not in service");
        case kgscInvalidPreferredMemStorage:     return presentation(fmt, "kgscInvalidPreferredMemStorage",   "Invalid preferred memory storage");
        case kgscUserTerminated:                 return presentation(fmt, "kgscUserTerminated",               "User terminated");
    }

    throw internal_not_found();
}

std::string Verbose::q931ProgressIndication(KQ931ProgressIndication code, Verbose::Presentation fmt)
{
    try
    {
        return internal_q931ProgressIndication(code);
    }
    catch (internal_not_found & e)
    {
        PRESENTATION_CHECK_RETURN(fmt,
            STG(FMT("[KQ931ProgressIndication='%d']") % (int)code),
            STG(FMT("Unknown Q931 progress indication (%d)") % (int)code));
    }
}

std::string Verbose::internal_q931ProgressIndication(KQ931ProgressIndication code, Verbose::Presentation fmt)
{
    switch (code)
    {
        case kq931pTonesMaybeAvailable:     return presentation(fmt, "kq931pTonesMaybeAvailable",  "Tones may be available");
        case kq931pDestinationIsNonIsdn:    return presentation(fmt, "kq931pDestinationIsNonIsdn", "Destination is not ISDN");
        case kq931pOriginationIsNonIsdn:    return presentation(fmt, "kq931pOriginationIsNonIsdn", "Origination is not ISDN");
        case kq931pCallReturnedToIsdn:      return presentation(fmt, "kq931pCallReturnedToIsdn",   "Call returned to ISDN");
        case kq931pTonesAvailable:          return presentation(fmt, "kq931pTonesAvailable",       "Tones available");
    }

    throw internal_not_found();
}

#endif /* K3L_AT_LEAST(1,6,0) */




#if K3L_AT_LEAST(2,1,0)
std::string Verbose::faxResult(KFaxResult code, Verbose::Presentation fmt)
{
    switch (code)
    {
        case kfaxrEndOfTransmission:    return presentation(fmt, "kfaxrEndOfTransmission", "EndOfTransmission");
        case kfaxrStoppedByCommand:     return presentation(fmt, "kfaxrStoppedByCommand", "StoppedByCommand");
        case kfaxrProtocolTimeout:      return presentation(fmt, "kfaxrProtocolTimeout", "ProtocolTimeout");
        case kfaxrProtocolError:        return presentation(fmt, "kfaxrProtocolError", "ProtocolError");
        case kfaxrRemoteDisconnection:  return presentation(fmt, "kfaxrRemoteDisconnection", "RemoteDisconnection");
        case kfaxrFileError:            return presentation(fmt, "kfaxrFileError", "FileError");
        case kfaxrUnknown:              return presentation(fmt, "kfaxrUnknown", "Unknown");
        case kfaxrEndOfReception:       return presentation(fmt, "kfaxrEndOfReception", "EndOfReception");
        case kfaxrCompatibilityError:   return presentation(fmt, "kfaxrCompatibilityError", "CompatibilityError");
        case kfaxrQualityError:         return presentation(fmt, "kfaxrQualityError", "QualityError");
    }

    PRESENTATION_CHECK_RETURN(fmt,
        STG(FMT("[KFaxResult='%d']") % (int)code),
        STG(FMT("Unknown fax result (%d)") % (int)code));
}

std::string Verbose::faxFileErrorCause(KFaxFileErrorCause code, Verbose::Presentation fmt)
{
    switch (code)
    {
        case kfaxfecTransmissionStopped:       return presentation(fmt, "kfaxfecTransmissionStopped", "TransmissionStopped");
        case kfaxfecTransmissionError:          return presentation(fmt, "kfaxfecTransmissionError", "TransmissionError");
        case kfaxfecListCleared:                return presentation(fmt, "kfaxfecListCleared", "ListCleared");
        case kfaxfecCouldNotOpen:               return presentation(fmt, "kfaxfecCouldNotOpen", "CouldNotOpen");
        case kfaxfecInvalidHeader:              return presentation(fmt, "kfaxfecInvalidHeader", "InvalidHeader");
        case kfaxfecDataNotFound:               return presentation(fmt, "kfaxfecDataNotFound", "DataNotFound");
        case kfaxfecInvalidHeight:              return presentation(fmt, "kfaxfecInvalidHeight", "InvalidHeight");
        case kfaxfecUnsupportedWidth:           return presentation(fmt, "kfaxfecUnsupportedWidth", "UnsupportedWidth");
        case kfaxfecUnsupportedCompression:     return presentation(fmt, "kfaxfecUnsupportedCompression", "UnsupportedCompression");
        case kfaxfecUnsupportedRowsPerStrip:    return presentation(fmt, "kfaxfecUnsupportedRowsPerStrip", "UnsupportedRowsPerStrip");
        case kfaxfecUnknown:                    return presentation(fmt, "kfaxfecUnknown", "Unknown");
    }

    PRESENTATION_CHECK_RETURN(fmt,
        STG(FMT("[KFaxFileErrorCause='%d']") % (int)code),
        STG(FMT("Unknown fax file error cause (%d)") % (int)code));
}

#endif


/********/

std::string Verbose::commandName(int32 code)
{
    switch ((kcommand)code)
    {
        case K_CM_SEIZE:                    return "CM_SEIZE";
        case K_CM_SYNC_SEIZE:               return "CM_SYNC_SEIZE";
        case K_CM_DIAL_DTMF:                return "CM_DIAL_DTMF";
#if K3L_AT_LEAST(1,6,0)
        case K_CM_SIP_REGISTER:             return "CM_SIP_REGISTER";
#endif
        case K_CM_DISCONNECT:               return "CM_DISCONNECT";
        case K_CM_CONNECT:                  return "CM_CONNECT";
        case K_CM_PRE_CONNECT:              return "CM_PRE_CONNECT";
        case K_CM_CAS_CHANGE_LINE_STT:      return "CM_CAS_CHANGE_LINE_STT";
        case K_CM_CAS_SEND_MFC:             return "CM_CAS_SEND_MFC";
        case K_CM_SET_FORWARD_CHANNEL:      return "CM_SET_FORWARD_CHANNEL";
        case K_CM_CAS_SET_MFC_DETECT_MODE:  return "CM_CAS_SET_MFC_DETECT_MODE";
        case K_CM_DROP_COLLECT_CALL:        return "CM_DROP_COLLECT_CALL";

#if K3L_AT_LEAST(1,5,0)
        case K_CM_MAKE_CALL:                return "CM_MAKE_CALL";
#endif

#if K3L_AT_LEAST(1,4,0)
        case K_CM_RINGBACK:                 return "CM_RINGBACK";
#endif

#if K3L_AT_LEAST(1,5,1)
        case K_CM_USER_INFORMATION:         return "CM_USER_INFORMATION";
#endif

#if K3L_AT_LEAST(1,4,0) && !K3L_AT_LEAST(2,2,0)
        case K_CM_VOIP_SEIZE:               return "CM_VOIP_SEIZE";

#if !K3L_AT_LEAST(2,0,0)
        /* internal commands */
        case K_CM_VOIP_START_DEBUG:         return "CM_VOIP_START_DEBUG";
        case K_CM_VOIP_STOP_DEBUG:          return "CM_VOIP_STOP_DEBUG";
        case K_CM_VOIP_DUMP_STAT:           return "CM_VOIP_DUMP_STAT";
#endif
#endif

#if K3L_AT_LEAST(1,5,2) && !K3L_AT_LEAST(2,0,0)
        /* internal command */
        case K_CM_ISDN_DEBUG:               return "CM_ISDN_DEBUG";
#endif

        case K_CM_LOCK_INCOMING:            return "CM_LOCK_INCOMING";
        case K_CM_UNLOCK_INCOMING:          return "CM_UNLOCK_INCOMING";
        case K_CM_LOCK_OUTGOING:            return "CM_LOCK_OUTGOING";
        case K_CM_UNLOCK_OUTGOING:          return "CM_UNLOCK_OUTGOING";

        case K_CM_START_SEND_FAIL:          return "CM_START_SEND_FAIL";
        case K_CM_STOP_SEND_FAIL:           return "CM_STOP_SEND_FAIL";

#if K3L_AT_LEAST(1,5,3)
        case K_CM_END_OF_NUMBER:            return "CM_END_OF_NUMBER";
#endif

#if K3L_AT_LEAST(1,6,0)
        case K_CM_SS_TRANSFER:              return "CM_SS_TRANSFER";
        case K_CM_GET_SMS:                  return "CM_GET_SMS";
        case K_CM_PREPARE_SMS:              return "CM_PREPARE_SMS";
        case K_CM_SEND_SMS:                 return "CM_SEND_SMS";
#endif
#if K3L_HAS_MPTY_SUPPORT
        case K_CM_HOLD_SWITCH:              return "CM_HOLD_SWITCH";
        case K_CM_MPTY_CONF:                return "CM_MPTY_CONF";
        case K_CM_MPTY_SPLIT:               return "CM_MPTY_SPLIT";
#endif

        case K_CM_ENABLE_DTMF_SUPPRESSION:  return "CM_ENABLE_DTMF_SUPPRESSION";
        case K_CM_DISABLE_DTMF_SUPPRESSION: return "CM_DISABLE_DTMF_SUPPRESSION";
        case K_CM_ENABLE_AUDIO_EVENTS:      return "CM_ENABLE_AUDIO_EVENTS";
        case K_CM_DISABLE_AUDIO_EVENTS:     return "CM_DISABLE_AUDIO_EVENTS";
        case K_CM_ENABLE_CALL_PROGRESS:     return "CM_ENABLE_CALL_PROGRESS";
        case K_CM_DISABLE_CALL_PROGRESS:    return "CM_DISABLE_CALL_PROGRESS";
        case K_CM_FLASH:                    return "CM_FLASH";
        case K_CM_ENABLE_PULSE_DETECTION:   return "CM_ENABLE_PULSE_DETECTION";
        case K_CM_DISABLE_PULSE_DETECTION:  return "CM_DISABLE_PULSE_DETECTION";
        case K_CM_ENABLE_ECHO_CANCELLER:    return "CM_ENABLE_ECHO_CANCELLER";
        case K_CM_DISABLE_ECHO_CANCELLER:   return "CM_DISABLE_ECHO_CANCELLER";
        case K_CM_ENABLE_AGC:               return "CM_ENABLE_AGC";
        case K_CM_DISABLE_AGC:              return "CM_DISABLE_AGC";
        case K_CM_ENABLE_HIGH_IMP_EVENTS:   return "CM_ENABLE_HIGH_IMP_EVENTS";
        case K_CM_DISABLE_HIGH_IMP_EVENTS:  return "CM_DISABLE_HIGH_IMP_EVENTS";

#if K3L_AT_LEAST(1,6,0)
        case K_CM_ENABLE_CALL_ANSWER_INFO:  return "CM_ENABLE_CALL_ANSWER_INFO";
        case K_CM_DISABLE_CALL_ANSWER_INFO: return "CM_DISABLE_CALL_ANSWER_INFO";
#endif

        case K_CM_RESET_LINK:               return "CM_RESET_LINK";

#if K3L_AT_LEAST(1,6,0)
        case K_CM_CLEAR_LINK_ERROR_COUNTER: return "CM_CLEAR_LINK_ERROR_COUNTER";
#endif

        case K_CM_SEND_DTMF:                return "CM_SEND_DTMF";
        case K_CM_STOP_AUDIO:               return "CM_STOP_AUDIO";
        case K_CM_HARD_RESET:               return "CM_HARD_RESET";

        case K_CM_SEND_TO_CTBUS:            return "CM_SEND_TO_CTBUS";
        case K_CM_RECV_FROM_CTBUS:          return "CM_RECV_FROM_CTBUS";
        case K_CM_SETUP_H100:               return "CM_SETUP_H100";

        case K_CM_MIXER:                    return "CM_MIXER";
        case K_CM_CLEAR_MIXER:              return "CM_CLEAR_MIXER";
        case K_CM_PLAY_FROM_FILE:           return "CM_PLAY_FROM_FILE";
        case K_CM_RECORD_TO_FILE:           return "CM_RECORD_TO_FILE";
        case K_CM_PLAY_FROM_STREAM:         return "CM_PLAY_FROM_STREAM";
        case K_CM_STOP_PLAY:                return "CM_STOP_PLAY";
        case K_CM_STOP_RECORD:              return "CM_STOP_RECORD";
        case K_CM_PAUSE_PLAY:               return "CM_PAUSE_PLAY";
        case K_CM_PAUSE_RECORD:             return "CM_PAUSE_RECORD";
        case K_CM_INCREASE_VOLUME:          return "CM_INCREASE_VOLUME";
        case K_CM_DECREASE_VOLUME:          return "CM_DECREASE_VOLUME";
        case K_CM_LISTEN:                   return "CM_LISTEN";
        case K_CM_STOP_LISTEN:              return "CM_STOP_LISTEN";
        case K_CM_PREPARE_FOR_LISTEN:       return "CM_PREPARE_FOR_LISTEN";

        case K_CM_PLAY_SOUND_CARD:          return "CM_PLAY_SOUND_CARD";
        case K_CM_STOP_SOUND_CARD:          return "CM_STOP_SOUND_CARD";

        case K_CM_MIXER_CTBUS:              return "CM_MIXER_CTBUS";
        case K_CM_PLAY_FROM_STREAM_EX:      return "CM_PLAY_FROM_STREAM_EX";
        case K_CM_ENABLE_PLAYER_AGC:        return "CM_ENABLE_PLAYER_AGC";
        case K_CM_DISABLE_PLAYER_AGC:       return "CM_DISABLE_PLAYER_AGC";
        case K_CM_START_STREAM_BUFFER:      return "CM_START_STREAM_BUFFER";
        case K_CM_ADD_STREAM_BUFFER:        return "CM_ADD_STREAM_BUFFER";
        case K_CM_STOP_STREAM_BUFFER:       return "CM_STOP_STREAM_BUFFER";
        case K_CM_SEND_BEEP:                return "CM_SEND_BEEP";
        case K_CM_SEND_BEEP_CONF:           return "CM_SEND_BEEP_CONF";
        case K_CM_ADD_TO_CONF:              return "CM_ADD_TO_CONF";
        case K_CM_REMOVE_FROM_CONF:         return "CM_REMOVE_FROM_CONF";
        case K_CM_RECORD_TO_FILE_EX:        return "CM_RECORD_TO_FILE_EX";

#if K3L_AT_LEAST(1,5,4)
        case K_CM_SET_VOLUME:               return "CM_SET_VOLUME";
#endif
        case K_CM_SET_LINE_CONDITION:       return "CM_SET_LINE_CONDITION";
        case K_CM_SEND_LINE_CONDITION:      return "CM_SEND_LINE_CONDITION";
        case K_CM_SET_CALLER_CATEGORY:      return "CM_SET_CALLER_CATEGORY";
        case K_CM_DIAL_MFC:                 return "CM_DIAL_MFC";

        case K_CM_INTERNAL_PLAY:            return "CM_INTERNAL_PLAY";
        case K_CM_RESUME_PLAY:              return "CM_RESUME_PLAY";
        case K_CM_RESUME_RECORD:            return "CM_RESUME_RECORD";
        case K_CM_INTERNAL_PLAY_EX:         return "CM_INTERNAL_PLAY_EX";
#if !K3L_AT_LEAST(2,0,0)
        case K_CM_PING:                     return "CM_PING";
#if K3L_AT_LEAST(1,6,0)
        case K_CM_LOG_REQUEST:              return "CM_LOG_REQUEST";
        case K_CM_LOG_CREATE_DISPATCHER:    return "CM_LOG_CREATE_DISPATCHER";
        case K_CM_LOG_DESTROY_DISPATCHER:   return "CM_LOG_DESTROY_DISPATCHER";
#endif
#endif

#if K3L_AT_LEAST(1,6,0)
        case K_CM_START_CADENCE:            return "CM_START_CADENCE";
        case K_CM_STOP_CADENCE:             return "CM_STOP_CADENCE";
        case K_CM_CHECK_NEW_SMS:            return "CM_CHECK_NEW_SMS";
        case K_CM_SEND_TO_MODEM:            return "CM_SEND_TO_MODEM";
#endif
#if K3L_AT_LEAST(2,1,0)
        case K_CM_START_FAX_TX:             return "CM_START_FAX_TX";
        case K_CM_STOP_FAX_TX:              return "CM_STOP_FAX_TX";
        case K_CM_ADD_FAX_FILE:             return "CM_ADD_FAX_FILE";
        case K_CM_ADD_FAX_PAGE_BREAK:       return "CM_ADD_FAX_PAGE_BREAK";
        case K_CM_START_FAX_RX:             return "CM_START_FAX_RX";
        case K_CM_STOP_FAX_RX:              return "CM_STOP_FAX_RX";
        case K_CM_SIM_CARD_SELECT:          return "CM_SIM_CARD_SELECT";
#endif

#if K3L_AT_LEAST(2,1,0)
       case K_CM_NOTIFY_WATCHDOG:           return "CM_NOTIFY_WATCHDOG";
       case K_CM_STOP_WATCHDOG:             return "CM_STOP_WATCHDOG";
       case K_CM_WATCHDOG_COUNT:            return "CM_WATCHDOG_COUNT";
       case K_CM_START_WATCHDOG:            return "CM_START_WATCHDOG";
#endif

    }

    return STG(FMT("[command='%d']") % code);
}

std::string Verbose::eventName(int32 code)
{
    switch ((kevent)code)
    {
        case K_EV_CHANNEL_FREE:         return "EV_CHANNEL_FREE";
        case K_EV_CONNECT:              return "EV_CONNECT";
        case K_EV_DISCONNECT:           return "EV_DISCONNECT";
        case K_EV_CALL_SUCCESS:         return "EV_CALL_SUCCESS";
        case K_EV_CALL_FAIL:            return "EV_CALL_FAIL";
        case K_EV_NO_ANSWER:            return "EV_NO_ANSWER";
        case K_EV_BILLING_PULSE:        return "EV_BILLING_PULSE";
        case K_EV_SEIZE_SUCCESS:        return "EV_SEIZE_SUCCESS";
        case K_EV_SEIZE_FAIL:           return "EV_SEIZE_FAIL";
        case K_EV_SEIZURE_START:        return "EV_SEIZURE_START";
        case K_EV_CAS_LINE_STT_CHANGED: return "EV_CAS_LINE_STT_CHANGED";
        case K_EV_CAS_MFC_RECV:         return "EV_CAS_MFC_RECV";

#if K3L_AT_LEAST(1,5,0)
        case K_EV_NEW_CALL:             return "EV_NEW_CALL";
#endif

#if K3L_AT_LEAST(1,5,1)
        case K_EV_USER_INFORMATION:     return "EV_USER_INFORMATION";
#endif

#if K3L_AT_LEAST(1,5,3)
        case K_EV_DIALED_DIGIT:         return "EV_DIALED_DIGIT";
#endif

#if K3L_AT_LEAST(1,6,0)
        case K_EV_SIP_REGISTER_INFO:    return "EV_SIP_REGISTER_INFO";
#endif

#if K3L_AT_LEAST(1,4,0)
        case K_EV_CALL_HOLD_START:      return "EV_CALL_HOLD_START";
        case K_EV_CALL_HOLD_STOP:       return "EV_CALL_HOLD_STOP";
#endif

#if K3L_AT_LEAST(1,6,0)
        case K_EV_SS_TRANSFER_FAIL:     return "EV_SS_TRANSFER_FAIL";
        case K_EV_FLASH:                return "EV_FLASH";
#endif

        case K_EV_DTMF_DETECTED:        return "EV_DTMF_DETECTED";
        case K_EV_DTMF_SEND_FINISH:     return "EV_DTMF_SEND_FINISH";
        case K_EV_AUDIO_STATUS:         return "EV_AUDIO_STATUS";
        case K_EV_CADENCE_RECOGNIZED:   return "EV_CADENCE_RECOGNIZED";

        case K_EV_END_OF_STREAM:        return "EV_END_OF_STREAM";
        case K_EV_PULSE_DETECTED:       return "EV_PULSE_DETECTED";

#if K3L_AT_LEAST(1,5,1)
        case K_EV_POLARITY_REVERSAL:    return "EV_POLARITY_REVERSAL";
#endif

#if K3L_AT_LEAST(1,6,0)
        case K_EV_ISDN_PROGRESS_INDICATOR: return "EV_ISDN_PROGRESS_INDICATOR";
        case K_EV_CALL_ANSWER_INFO:        return "EV_CALL_ANSWER_INFO";
        case K_EV_COLLECT_CALL:            return "EV_COLLECT_CALL";
        case K_EV_SIP_DTMF_DETECTED:       return "EV_SIP_DTMF_DETECTED";

        case K_EV_RECV_FROM_MODEM:      return "EV_RECV_FROM_MODEM";
        case K_EV_NEW_SMS:              return "EV_NEW_SMS";
        case K_EV_SMS_INFO:             return "EV_SMS_INFO";
        case K_EV_SMS_DATA:             return "EV_SMS_DATA";
        case K_EV_SMS_SEND_RESULT:      return "EV_SMS_SEND_RESULT";
        case K_EV_RING_DETECTED:        return "EV_RING_DETECTED";
        case K_EV_PHYSICAL_LINK_DOWN:   return "EV_PHYSICAL_LINK_DOWN";
        case K_EV_PHYSICAL_LINK_UP:     return "EV_PHYSICAL_LINK_UP";
#endif
#if K3L_HAS_MPTY_SUPPORT
        case K_EV_CALL_MPTY_START:      return "EV_CALL_MPTY_START";
        case K_EV_CALL_MPTY_STOP:       return "EV_CALL_MPTY_STOP";
        case K_EV_GSM_COMMAND_STATUS:   return "EV_GSM_COMMAND_STATUS";
#endif
#if !K3L_AT_LEAST(2,0,0)
        case K_EV_PONG:                 return "EV_PONG";
#endif
        case K_EV_CHANNEL_FAIL:         return "EV_CHANNEL_FAIL";
        case K_EV_REFERENCE_FAIL:       return "EV_REFERENCE_FAIL";
        case K_EV_INTERNAL_FAIL:        return "EV_INTERNAL_FAIL";
        case K_EV_HARDWARE_FAIL:        return "EV_HARDWARE_FAIL";
        case K_EV_LINK_STATUS:          return "EV_LINK_STATUS";

#if K3L_AT_LEAST(1,4,0)
        case K_EV_CLIENT_RECONNECT:     return "EV_CLIENT_RECONNECT";
        case K_EV_VOIP_SEIZURE:         return "EV_VOIP_SEIZURE";
#endif
        case K_EV_SEIZURE:              return "EV_SEIZURE";
#if K3L_AT_LEAST(2,1,0)
        case K_EV_FAX_CHANNEL_FREE:     return "EV_FAX_CHANNEL_FREE";
        case K_EV_FAX_FILE_SENT:        return "EV_FAX_FILE_SENT";
        case K_EV_FAX_FILE_FAIL:        return "EV_FAX_FILE_FAIL";
    /*case K_EV_FAX_MESSAGE_CONFIRMATION:return "EV_FAX_MESSAGE_CONFIRMATION";*/
        case K_EV_FAX_TX_TIMEOUT:       return "EV_FAX_TX_TIMEOUT";
        case K_EV_FAX_PAGE_CONFIRMATION:return "EV_FAX_PAGE_CONFIRMATION";
        case K_EV_FAX_REMOTE_INFO:      return "EV_FAX_REMOTE_INFO";
#endif

#if K3L_AT_LEAST(2,1,0)
        case K_EV_WATCHDOG_COUNT:       return "EV_WATCHDOG_COUNT";
#endif
    }

    return STG(FMT("[event='%d']") % code);
}


#if K3L_AT_LEAST(2,0,0)
std::string Verbose::command(int32 dev, K3L_COMMAND *k3lcmd, R2CountryType r2_country, Verbose::Presentation fmt)
#else
std::string Verbose::command(int32 dev, K3L_COMMAND *k3lcmd, Verbose::Presentation fmt)
#endif
{
#if K3L_AT_LEAST(2,0,0)
    return command(k3lcmd->Cmd, dev, k3lcmd->Object, (const char *) k3lcmd->Params, r2_country, fmt);
#else
    return command(k3lcmd->Cmd, dev, k3lcmd->Object, (const char *) k3lcmd->Params, fmt);
#endif
}

#if K3L_AT_LEAST(2,0,0)
std::string Verbose::command(int32 cmd_code, int32 dev_idx, int32 obj_idx, const char * params, R2CountryType r2_country, Verbose::Presentation fmt)
#else
std::string Verbose::command(int32 cmd_code, int32 dev_idx, int32 obj_idx, const char * params, Verbose::Presentation fmt)
#endif
{
    unsigned short int dev = (unsigned short int) dev_idx;
    unsigned short int obj = (unsigned short int) obj_idx;

    kcommand code = (kcommand) cmd_code;

    std::string buf, extra;

    switch (code)
    {
        case K_CM_SEIZE:
        case K_CM_SYNC_SEIZE:
        //case K_CM_VOIP_SEIZE://deprecated
        case K_CM_DIAL_MFC:
        case K_CM_DIAL_DTMF:

        case K_CM_CONNECT:
        case K_CM_PRE_CONNECT:
        case K_CM_DISCONNECT:
        case K_CM_DROP_COLLECT_CALL:

        case K_CM_START_SEND_FAIL:
        case K_CM_STOP_SEND_FAIL:

        case K_CM_ENABLE_DTMF_SUPPRESSION:
        case K_CM_DISABLE_DTMF_SUPPRESSION:
        case K_CM_ENABLE_AUDIO_EVENTS:
        case K_CM_DISABLE_AUDIO_EVENTS:
        case K_CM_ENABLE_CALL_PROGRESS:
        case K_CM_DISABLE_CALL_PROGRESS:
        case K_CM_ENABLE_PULSE_DETECTION:
        case K_CM_DISABLE_PULSE_DETECTION:
        case K_CM_ENABLE_ECHO_CANCELLER:
        case K_CM_DISABLE_ECHO_CANCELLER:
        case K_CM_ENABLE_AGC:
        case K_CM_DISABLE_AGC:
        case K_CM_ENABLE_HIGH_IMP_EVENTS:
        case K_CM_DISABLE_HIGH_IMP_EVENTS:

        case K_CM_FLASH:
        case K_CM_RESET_LINK:
        case K_CM_CLEAR_MIXER:

        case K_CM_LOCK_INCOMING:
        case K_CM_UNLOCK_INCOMING:
        case K_CM_LOCK_OUTGOING:
        case K_CM_UNLOCK_OUTGOING:

        case K_CM_INCREASE_VOLUME:
        case K_CM_DECREASE_VOLUME:

        case K_CM_STOP_RECORD:
        case K_CM_PAUSE_RECORD:
        case K_CM_RESUME_RECORD:

        case K_CM_STOP_LISTEN:

        case K_CM_PLAY_SOUND_CARD:
        case K_CM_STOP_SOUND_CARD:
        case K_CM_RINGBACK:
#if K3L_AT_LEAST(1,4,0) && !K3L_AT_LEAST(2,0,0)
        case K_CM_VOIP_START_DEBUG:
        case K_CM_VOIP_STOP_DEBUG:
        case K_CM_VOIP_DUMP_STAT:
#endif

#if K3L_AT_LEAST(1,5,3)
        case K_CM_END_OF_NUMBER:
#endif

#if K3L_AT_LEAST(1,5,4)
        case K_CM_SET_VOLUME:
#endif

#if K3L_AT_LEAST(1,6,0)
        case K_CM_ENABLE_CALL_ANSWER_INFO:
        case K_CM_DISABLE_CALL_ANSWER_INFO:

        case K_CM_SS_TRANSFER:

        case K_CM_CHECK_NEW_SMS:
        case K_CM_GET_SMS:
        case K_CM_PREPARE_SMS:
        case K_CM_SEND_SMS:

        case K_CM_START_CADENCE:
        case K_CM_STOP_CADENCE:
        case K_CM_SEND_TO_MODEM:
#endif
#if K3L_HAS_MPTY_SUPPORT
        case K_CM_HOLD_SWITCH:
        case K_CM_MPTY_CONF:
        case K_CM_MPTY_SPLIT:
#endif
#if K3L_AT_LEAST(2,1,0)
        case K_CM_SIM_CARD_SELECT:
#endif
            if (params != NULL)
            {
                extra += "param='";
                extra += (params ? params : "<empty>");
                extra += "'";

                return show(buf, commandName(code), Target(CHANNEL, dev, obj), extra);
            }
            else
            {
                return show(buf, commandName(code), Target(CHANNEL, dev, obj));
            }

        case K_CM_SEND_DTMF: /* ?? */
            return show(buf, commandName(code), Target(CHANNEL, dev, obj));

        /****/

        case K_CM_STOP_AUDIO:
            extra  = "stop='";
            switch ((params ? (int)(*params) : -1))
            {
                case 1:   extra += "tx";
                case 2:   extra += "rx";
                case 3:   extra += "tx+rx";
                default:  extra += "<unknown>";
            }
            extra  = "'";

            return show(buf, commandName(code), Target(CHANNEL, dev, obj), extra);

        /****/

#if K3L_AT_LEAST(1,5,2) && !K3L_AT_LEAST(2,0,0)
        case K_CM_ISDN_DEBUG:
            extra  = "flags='";
            extra += isdnDebug((unsigned long)params);
            extra += "'";

            return show(buf, commandName(code), Target(NONE), extra);
#endif

        /****/

#if K3L_AT_LEAST(1,5,1)
        case K_CM_USER_INFORMATION:
#endif
            if (params != NULL)
            {
                KUserInformation * userinfo = (KUserInformation *)params;

                std::string tmp((const char*) userinfo->UserInfo, userinfo->UserInfoLength);

                extra = STG(FMT("proto='%d',length='%d',data='%s'")
                        % userinfo->ProtocolDescriptor % userinfo->UserInfoLength % tmp);

                return show(buf, commandName(code), Target(CHANNEL, dev, obj), extra);
            }
            else
            {
                return show(buf, commandName(code), Target(CHANNEL, dev, obj));
            }

        /****/



        case K_CM_CAS_CHANGE_LINE_STT:
        {
            const char status = (params ? *params : 0x00);

            extra += "status='";
            extra += (status & 0x01 ? "1" : "0");
            extra += (status & 0x02 ? "1" : "0");
            extra += (status & 0x04 ? "1" : "0");
            extra += (status & 0x08 ? "1" : "0");
            extra += "'";

            return show(buf, commandName(code), Target(CHANNEL, dev, obj), extra);
        }

        case K_CM_CAS_SEND_MFC:
        {
            char mfc = (params ? *params : 0xff);

            extra = STG(FMT("mfc='%d'") % (int) mfc);

            return show(buf, commandName(code), Target(CHANNEL, dev, obj), extra);
        }

        case K_CM_CAS_SET_MFC_DETECT_MODE:
        {
            int mode = (params ? *((int *)params) : -1);

            extra = STG(FMT("mode='%d'") % mode);

            return show(buf, commandName(code), Target(CHANNEL, dev, obj), extra);
        }

        case K_CM_SET_FORWARD_CHANNEL:
        {
            int channel = (params ? *((int*) params) : -1);

            extra = STG(FMT("forward='%03d'") % channel);

            return show(buf, commandName(code), Target(CHANNEL, dev, obj), extra);
        }

#if K3L_AT_LEAST(1,5,0)
        case K_CM_MAKE_CALL:
            extra  = "options='";
            extra += (params ? params : "<empty>");
            extra += "'";

            return show(buf, commandName(code), Target(CHANNEL, dev, obj), extra);
#endif

        case K_CM_MIXER:
        case K_CM_MIXER_CTBUS:
        {
            if (params)
            {
                KMixerCommand *m = (KMixerCommand*)params;

                std::string src = mixerSource((KMixerSource)m->Source);
                std::string idx("<unknown>");

                switch (m->Source)
                {
                    case kmsChannel:
                    case kmsPlay:
                    case kmsCTbus:
#if (K3L_AT_LEAST(1,4,0) && !K3L_AT_LEAST(1,6,0))
                    case kmsVoIP:
#endif
#if K3L_AT_LEAST(1,6,0)
                    case kmsNoDelayChannel:
#endif
                        idx = STG(FMT("%02d") % (int)m->SourceIndex);
                        break;

                    case kmsGenerator:
                        idx = mixerTone((KMixerTone)m->SourceIndex);
                        break;
                };

                extra = STG(FMT("track='%d',src='%s',index='%s'") % (int)m->Track % src % idx);
            }
            else
            {
                extra = "<unknown>";
            }

            return show(buf, commandName(code), Target(MIXER, dev, obj), extra);
        };

        case K_CM_PLAY_FROM_FILE:
            extra  = "file='";
            extra += (params ? params : "<empty>");
            extra += "'";

            return show(buf, commandName(code), Target(PLAYER, dev, obj), extra);

        case K_CM_RECORD_TO_FILE:
            extra  = "file='";
            extra += (params ? params : "<empty>");
            extra += "'";

            return show(buf, commandName(code), Target(PLAYER, dev, obj), extra);

        case K_CM_RECORD_TO_FILE_EX:
            extra  = "params='";
            extra += (params ? params : "<empty>");
            extra += "'";

            return show(buf, commandName(code), Target(PLAYER, dev, obj), extra);

        case K_CM_PLAY_FROM_STREAM:
        case K_CM_ADD_STREAM_BUFFER:
        {
            struct buffer_param
            {
                const void * ptr;
                const int   size;
            }
            *p = (buffer_param *) params;

            std::stringstream stream;

            extra = STG(FMT("buffer='%p',size='%d'")
                % (const void *) p->ptr % (const int) p->size);

            return show(buf, commandName(code), Target(PLAYER, dev, obj), extra);
        }

        case K_CM_PLAY_FROM_STREAM_EX:
        {
            struct buffer_param
            {
                const void  *  ptr;
                const int     size;
                const char   codec;
            }
            *p = (buffer_param *) params;

            std::string codec;

            switch (p->codec)
            {
                case 0:  codec = "A-Law";
                case 1:  codec = "PCM-08khz";
                case 2:  codec = "PCM-11khz";
                default: codec = "<unknown>";
            }

            std::stringstream stream;

            extra = STG(FMT("buffer='%p',size='%d',codec='%s'")
                % (const void *) p->ptr % (const int) p->size % codec);

            return show(buf, commandName(code), Target(PLAYER, dev, obj), extra);
        }

        case K_CM_STOP_PLAY:
        case K_CM_PAUSE_PLAY:
        case K_CM_RESUME_PLAY:

        case K_CM_START_STREAM_BUFFER:
        case K_CM_STOP_STREAM_BUFFER:

        case K_CM_ENABLE_PLAYER_AGC:
        case K_CM_DISABLE_PLAYER_AGC:

        case K_CM_SEND_BEEP:
        case K_CM_SEND_BEEP_CONF:

        case K_CM_INTERNAL_PLAY:
        case K_CM_INTERNAL_PLAY_EX:
            return show(buf, commandName(code), Target(PLAYER, dev, obj));

        case K_CM_ADD_TO_CONF:
            extra += "conference='";
            extra += (params ? (int) (*params) : -1);
            extra += "'";

            return show(buf, commandName(code), Target(MIXER, dev, obj), extra);

        case CM_REMOVE_FROM_CONF:
            return show(buf, commandName(code), Target(MIXER, dev, obj));

        case K_CM_LISTEN:
        case K_CM_PREPARE_FOR_LISTEN:
        {
            int msecs = (params ? *((int*)params) : -1);

            extra = STG(FMT("msecs='%d'") % msecs);

            return show(buf, commandName(code), Target(PLAYER, dev, obj), extra);
        }

        case K_CM_SEND_TO_CTBUS:
        case K_CM_RECV_FROM_CTBUS:
        {
            KCtbusCommand *p = (KCtbusCommand*)(params);

            extra = STG(FMT("stream='%02d',timeslot='%02d',enable='%d'")
                % (int)p->Stream % (int)p->TimeSlot % (int)p->Enable);

            return show(buf, commandName(code), Target(CHANNEL, dev, obj), extra);
        }

        case K_CM_SET_LINE_CONDITION:
        case K_CM_SEND_LINE_CONDITION:
            extra  = "condition='";
#if K3L_AT_LEAST(2,0,0)
            extra += signGroupB((KSignGroupB) *((int *) params), r2_country);
#else
            extra += signGroupB((KSignGroupB) *((int *) params));
#endif
            extra += "'";

            return show(buf, commandName(code), Target(CHANNEL, dev, obj), extra);

        case K_CM_SET_CALLER_CATEGORY:
            extra  = "category='";
#if K3L_AT_LEAST(2,0,0)
            extra += signGroupII((KSignGroupII) *((int *) params), r2_country);
#else
            extra += signGroupII((KSignGroupII) *((int *) params));
#endif
            extra += "'";

            return show(buf, commandName(code), Target(CHANNEL, dev, obj), extra);

#if K3L_AT_LEAST(1,6,0)
        case K_CM_CLEAR_LINK_ERROR_COUNTER:
            return show(buf, commandName(code), Target(LINK, dev, obj));

        case K_CM_SIP_REGISTER:
            if (params != NULL)
            {
                extra += "param='";
                extra += (params ? params : "<empty>");
                extra += "'";

                return show(buf, commandName(code), Target(DEVICE, dev), extra);
            }
            else
            {
                return show(buf, commandName(code), Target(DEVICE, dev));
            }
#endif

        case K_CM_SETUP_H100:
            extra += "option='";
            extra += h100configIndex((KH100ConfigIndex)obj_idx);
            extra += "'value='";
            extra += (params ? STG(FMT("%02d") % (int)(*params)) : "<empty>");
            extra += "'";

            return show(buf, commandName(code), Target(DEVICE, dev), extra);

        case K_CM_HARD_RESET:
            return show(buf, commandName(code), Target(LINK, dev, obj));

#if !K3L_AT_LEAST(2,0,0)
        /* como funciona? */
        case K_CM_LOG_REQUEST:
        case K_CM_LOG_CREATE_DISPATCHER:
        case K_CM_LOG_DESTROY_DISPATCHER:

        case K_CM_PING:
#endif
            return show(buf, commandName(code), Target(NONE));
#if K3L_AT_LEAST(2,1,0)
        case K_CM_START_FAX_TX:
        case K_CM_START_FAX_RX:
        case K_CM_ADD_FAX_FILE:
            extra  = "params='";
            extra += (params ? params : "<empty>");
            extra += "'";
            return show(buf, commandName(code), Target(CHANNEL, dev, obj), extra);
        case K_CM_STOP_FAX_TX:
        case K_CM_STOP_FAX_RX:
        case K_CM_ADD_FAX_PAGE_BREAK:
            return show(buf, commandName(code), Target(CHANNEL, dev, obj));
#endif

#if K3L_AT_LEAST(2,1,0)
        case K_CM_NOTIFY_WATCHDOG:
        case K_CM_STOP_WATCHDOG:
        case K_CM_START_WATCHDOG:
            return show(buf, commandName(code) , Target(DEVICE, obj));
        case K_CM_WATCHDOG_COUNT:
            return show(buf, commandName(code) , Target(NONE));
#endif

    }

    /* default command handler */
    return show(buf, commandName(code), Target(CHANNEL, dev, obj));
}

#if K3L_AT_LEAST(2,0,0)
std::string Verbose::event(KSignaling sig, int32 obj_idx, K3L_EVENT *ev, R2CountryType r2_country, Verbose::Presentation fmt)
#else
std::string Verbose::event(KSignaling sig, int32 obj_idx, K3L_EVENT *ev, Verbose::Presentation fmt)
#endif
{
    unsigned short int dev = (unsigned short int) ev->DeviceId;
    unsigned short int obj = (unsigned short int) obj_idx;

    kevent code = (kevent) ev->Code;

    std::string buf, extra;

    switch (code)
    {
        case K_EV_CHANNEL_FREE:
        case K_EV_SEIZE_SUCCESS:
        case K_EV_CALL_SUCCESS:
        case K_EV_NO_ANSWER:
        case K_EV_CONNECT:
        case K_EV_DTMF_SEND_FINISH:
        case K_EV_SEIZURE_START:
        case K_EV_BILLING_PULSE:
        case K_EV_REFERENCE_FAIL:

#if K3L_AT_LEAST(1,4,0)
        case K_EV_CALL_HOLD_START:
        case K_EV_CALL_HOLD_STOP:
#endif

#if K3L_AT_LEAST(1,5,0)
        case K_EV_NEW_CALL:
#endif

#if K3L_AT_LEAST(1,6,0)
        case K_EV_FLASH:
        case K_EV_POLARITY_REVERSAL:
        case K_EV_COLLECT_CALL:
        case K_EV_SS_TRANSFER_FAIL:
        case K_EV_RING_DETECTED:
#endif
#if K3L_HAS_MPTY_SUPPORT
        case K_EV_CALL_MPTY_START:
        case K_EV_CALL_MPTY_STOP:
#endif
            break;

#if K3L_AT_LEAST(1,6,0)
        case K_EV_RECV_FROM_MODEM:
        case K_EV_SMS_INFO:
        case K_EV_SMS_DATA:
            extra  = "data='";
            extra += (ev->Params ? (const char *)(ev->Params) : "<empty>");
            extra += "'";

            return show(buf, eventName(code), Target(CHANNEL, dev, obj), extra);
#endif

#if K3L_AT_LEAST(1,6,0)
        case K_EV_SMS_SEND_RESULT:
            extra  = "result='";
#if K3L_AT_LEAST(2,0,0)
            extra += gsmSmsCause((KGsmSmsCause)ev->AddInfo);
#else
            extra += gsmCallCause((KGsmCallCause)ev->AddInfo);
#endif
            extra += "'";
            return show(buf, eventName(code), Target(CHANNEL, dev, obj), extra);

#if K3L_HAS_MPTY_SUPPORT
        case K_EV_GSM_COMMAND_STATUS:
            extra  = "result='";
            extra += gsmMobileCause((KGsmMobileCause)ev->AddInfo);
            extra += "'";
            return show(buf, eventName(code), Target(CHANNEL, dev, obj), extra);
#endif

        case K_EV_CALL_ANSWER_INFO:
            extra  = "info='";
            extra += callStartInfo((KCallStartInfo)ev->AddInfo);
            extra += "'";
            return show(buf, eventName(code), Target(CHANNEL, dev, obj), extra);

        case K_EV_NEW_SMS:
            if (ev->AddInfo != 0)
            {
                extra  = "messages='";
                extra += STG(FMT("%d") % ev->AddInfo);
                extra += "'";
                return show(buf, eventName(code), Target(CHANNEL, dev, obj), extra);
            }
            else
            {
                return show(buf, eventName(code), Target(CHANNEL, dev, obj));
            }

        case K_EV_ISDN_PROGRESS_INDICATOR:
            if (ev->AddInfo != 0)
            {
                extra  = "indication='";
                extra += q931ProgressIndication((KQ931ProgressIndication)ev->AddInfo);
                extra += "'";
                return show(buf, eventName(code), Target(CHANNEL, dev, obj), extra);
            }
            else
            {
                return show(buf, eventName(code), Target(CHANNEL, dev, obj));
            }
#endif

        case K_EV_CAS_LINE_STT_CHANGED:
            extra = STG(FMT("[a=%d,b=%d,c=%d,d=%d]")
                % ((ev->AddInfo & 0x8) >> 3) % ((ev->AddInfo & 0x4) >> 2)
                % ((ev->AddInfo & 0x2) >> 1) %  (ev->AddInfo & 0x1));

            return show(buf, eventName(code), Target(CHANNEL, dev, obj), extra);

        case K_EV_CAS_MFC_RECV:
            extra = STG(FMT("digit='%d'") % ev->AddInfo);

            return show(buf, eventName(code), Target(CHANNEL, dev, obj), extra);

        case K_EV_CALL_FAIL:
            extra  = "cause='";
#if K3L_AT_LEAST(2,0,0)
            extra += callFail(sig, r2_country, ev->AddInfo);
#else
            extra += callFail(sig, ev->AddInfo);
#endif
            extra += "'";

            if (ev->Params != NULL && ev->ParamSize != 0)
            {
                if (!extra.empty())
                    extra += ",";

                extra += "params='";
                extra += (const char *) ev->Params;
                extra += "'";
            }

            return show(buf, eventName(code), Target(CHANNEL, dev, obj), extra);

        case K_EV_DISCONNECT:
            switch (sig)
            {
#if K3L_AT_LEAST(1,5,1)
                case ksigOpenCCS:
                case ksigPRI_EndPoint:
                case ksigPRI_Network:
                case ksigPRI_Passive:
                    extra  = "cause='";
                    extra += isdnCause((KQ931Cause) ev->AddInfo);
                    extra += "'";
#endif
                default:
                    break;
            }

            if (ev->Params != NULL && ev->ParamSize != 0)
            {
                if (!extra.empty())
                    extra += ",";

                extra += "params='";
                extra += (const char *) ev->Params;
                extra += "'";
            }

            if (!extra.empty())
                return show(buf, eventName(code), Target(CHANNEL, dev, obj), extra);
            else
                return show(buf, eventName(code), Target(CHANNEL, dev, obj));

            break;

#if K3L_AT_LEAST(1,6,0)
        case K_EV_SIP_DTMF_DETECTED:
#endif
        case K_EV_DTMF_DETECTED:
        case K_EV_PULSE_DETECTED:
        case K_EV_DIALED_DIGIT:
            extra = STG(FMT("digit='%c'") % (char)ev->AddInfo);

            return show(buf, eventName(code), Target(CHANNEL, dev, obj), extra);

        case K_EV_SEIZURE:
        {
            KIncomingSeizeParams *n = (KIncomingSeizeParams *)
                ( ((char*)ev) + sizeof(K3L_EVENT) );

            extra += "orig_addr='";
            extra += n->NumberA;
            extra += "',dest_addr='";
            extra += n->NumberB;
            extra += "'";

            return show(buf, eventName(code), Target(CHANNEL, dev, obj), extra);
        }

#if K3L_AT_LEAST(1,4,0)
        case K_EV_VOIP_SEIZURE:
        {
            char *numB = ((char*)ev) + sizeof(K3L_EVENT);
            char *numA = numB + 61;

            extra  = "numberA='";
            extra += numA;
            extra += "',numberB='";
            extra += numB;
            extra += "'";

            return show(buf, eventName(code), Target(CHANNEL, dev, obj), extra);
        }
#endif


        case K_EV_END_OF_STREAM:
            return show(buf, eventName(code), Target(PLAYER, dev, obj));

        case K_EV_AUDIO_STATUS:
            extra  = "tone='";
            extra += mixerTone((KMixerTone)ev->AddInfo);
            extra += "'";

            return show(buf, eventName(code), Target(CHANNEL, dev, obj), extra);

        case K_EV_CADENCE_RECOGNIZED:
            extra = STG(FMT("cadence='%c'") % (char)(ev->AddInfo));

            return show(buf, eventName(code), Target(CHANNEL, dev, obj), extra);

        case K_EV_CHANNEL_FAIL:
            extra  = "reason='";
            extra += channelFail(sig, ev->AddInfo);
            extra += "'";

            return show(buf, eventName(code), Target(CHANNEL, dev, obj), extra);

        case K_EV_SEIZE_FAIL:
            extra  = "reason='";
            extra += seizeFail((KSeizeFail) ev->AddInfo);
            extra += "'";

            return show(buf, eventName(code), Target(CHANNEL, dev, obj), extra);

        case K_EV_INTERNAL_FAIL:
            extra  = "reason='";
            extra += internalFail((KInternalFail) ev->AddInfo);
            extra += "'";

            return show(buf, eventName(code), Target(CHANNEL, dev, obj), extra);

        case K_EV_HARDWARE_FAIL:
            extra  = "component='";
            extra += systemObject((KSystemObject) ev->AddInfo);
            extra += "'";

            switch (ev->AddInfo)
            {
                case ksoChannel:
                    return show(buf, eventName(code), Target(CHANNEL, dev, obj), extra);
                case ksoLink:
                    return show(buf, eventName(code), Target(LINK, dev, obj), extra);
                case ksoLinkMon:
                case ksoH100:
                case ksoFirmware:
                case ksoDevice:
                    return show(buf, eventName(code), Target(DEVICE, dev), extra);
                case ksoAPI:
                    return show(buf, eventName(code), Target(NONE), extra);
            }


        case K_EV_LINK_STATUS:
            // EV_LINK_STATUS has always zero in ObjectInfo (and AddInfo!)
            /* fall throught... */

#if K3L_AT_LEAST(1,6,0)
        case K_EV_PHYSICAL_LINK_UP:
        case K_EV_PHYSICAL_LINK_DOWN:
            return show(buf, eventName(code), Target(LINK, dev, obj));
#endif

#if K3L_AT_LEAST(1,5,1)
        case K_EV_USER_INFORMATION:
        {
            KUserInformation *info = (KUserInformation *)(ev->Params);

            std::string data((const char *)info->UserInfo,
                std::min<size_t>(info->UserInfoLength, KMAX_USER_USER_LEN));

            extra = STG(FMT("proto='%d',length='%d',data='%s'")
                % info->ProtocolDescriptor % info->UserInfoLength % data);

            return show(buf, eventName(code), Target(CHANNEL, dev, obj), extra);
        }
#endif

#if K3L_AT_LEAST(1,6,0)
        case K_EV_SIP_REGISTER_INFO:
            extra  = "params='";
            extra += (ev->Params ? (const char *) (ev->Params) : "<unknown>");
            extra += "',status='";
            extra += sipFailures((KSIP_Failures)(ev->AddInfo));
            extra += "'";

            return show(buf, eventName(code), Target(DEVICE, dev), extra);
#endif

#if !K3L_AT_LEAST(2,0,0)
        case K_EV_PONG:
#endif

#if K3L_AT_LEAST(1,4,0)
        case K_EV_CLIENT_RECONNECT:
#endif
            return show(buf, eventName(code), Target(NONE));

#if K3L_AT_LEAST(2,1,0)
        case K_EV_FAX_CHANNEL_FREE:
            extra  = "status='";
            extra += faxResult((KFaxResult)ev->AddInfo);
            extra += "'";
            return show(buf, eventName(code), Target(CHANNEL, dev, obj), extra);

        case K_EV_FAX_FILE_SENT:
            extra  = "filename='";
            extra += (ev->Params ? (const char *) (ev->Params) : "<unknown>");
            extra += "'";
            return show(buf, eventName(code), Target(CHANNEL, dev, obj), extra);

        case K_EV_FAX_FILE_FAIL:
            extra  = "cause='";
            extra += faxFileErrorCause((KFaxFileErrorCause)ev->AddInfo);
            extra += "',filename='";
            extra += (ev->Params ? (const char *) (ev->Params) : "<unknown>");
            extra += "'";
            return show(buf, eventName(code), Target(CHANNEL, dev, obj), extra);

        case K_EV_FAX_REMOTE_INFO:
            extra = ((ev->Params && ev->ParamSize != 0) ? (const char *) ev->Params : "");
            return show(buf, eventName(code), Target(CHANNEL, dev, obj), extra);

        /*case K_EV_FAX_MESSAGE_CONFIRMATION:*/
        case K_EV_FAX_PAGE_CONFIRMATION:
        case K_EV_FAX_TX_TIMEOUT:
            return show(buf, eventName(code), Target(CHANNEL, dev, obj));
#endif

#if K3L_AT_LEAST(2,1,0)
        case K_EV_WATCHDOG_COUNT:
            extra = STG(FMT("count='%d'") % (char)ev->AddInfo);
            return show(buf , eventName(code), Target(NONE), extra);
#endif

    }

    // default handler...
    if (ev->Params != NULL && ev->ParamSize != 0)
    {
        extra += "params='";
        extra.append((const char *) ev->Params, (unsigned int) std::max<int>(ev->ParamSize - 1, 0));
        extra += "'";

        return show(buf, eventName(code), Target(CHANNEL, dev, obj), extra);
    }
    else
        return show(buf, eventName(code), Target(CHANNEL, dev, obj));
}

/********************************************/

std::string Verbose::show(std::string & buf, std::string name, Target tgt, std::string & extra)
{
    if (tgt.type == NONE)
    {
        generate(buf, name, tgt, extra);
    }
    else
    {
        std::string tmp(",");
        tmp += extra;

        generate(buf, name, tgt, tmp);
    }

    return buf;
}

std::string Verbose::show(std::string & buf, std::string name, Target tgt)
{
    std::string tmp("");

    generate(buf, name, tgt, tmp);
    return buf;
}

void Verbose::generate(std::string &buf, std::string &name, Target tgt, std::string &extra)
{
    switch (tgt.type)
    {
        case NONE:
            if (extra.empty())
                buf += STG(FMT("<%s>") % name);
            else
                buf += STG(FMT("<%s> (%s)") % name % extra);
            break;

        case DEVICE:
            buf += STG(FMT("<%s> (d=%02d%s)")
                % name % tgt.device % extra);
            break;

        default:
        {
            const char *kind = "o";

            switch (tgt.type)
            {
                case CHANNEL:
                    kind = "c";
                    break;
                case PLAYER:
                    kind = "p";
                    break;
                case MIXER:
                    kind = "m";
                    break;
                case LINK:
                    kind = "l";
                default:
                    break;
            }

            buf += STG(FMT("<%s> (d=%02d,%s=%03d%s)")
                % name % tgt.device % kind % tgt.object % extra);
            break;
        }
    }
}

