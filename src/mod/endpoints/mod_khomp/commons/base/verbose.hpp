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

#ifndef _VERBOSE_HPP_
#define _VERBOSE_HPP_

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

#include <verbose_traits.hpp>

struct Verbose
{
    typedef enum
    {
        R2_COUNTRY_BRA = 1,
        R2_COUNTRY_ARG = 2,
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

    Verbose(const K3LAPI & api)
    : _api(api) {};

#if K3L_AT_LEAST(2,0,0)
    std::string event(const int32, const K3L_EVENT * const,
                      const R2CountryType r2_country = R2_COUNTRY_BRA,
                      const Presentation fmt = HUMAN) const;
#else
    std::string event(const int32, const K3L_EVENT * const,
                      const Presentation fmt = HUMAN) const;
#endif

    std::string channelStatus(const int32, const int32, const int32,
                              const Presentation fmt = HUMAN) const;

    /* end of dynamic (object) stuff */

 protected:
    const K3LAPI & _api;

    /* used internally */
    struct internal_not_found {};

 public:

    /* static (class) stuff */

    static std::string echoLocation(const KEchoLocation, const Presentation fmt = HUMAN);
    static std::string echoCancellerConfig(const KEchoCancellerConfig, const Presentation fmt = HUMAN);

#if K3L_AT_LEAST(2,0,0)
    static std::string event(const KSignaling, const int32, const K3L_EVENT * const,
                             const R2CountryType = R2_COUNTRY_BRA, Presentation fmt = HUMAN);
#else
    static std::string event(const KSignaling, const int32, const K3L_EVENT * const,
                             const Presentation fmt = HUMAN);
#endif

#if K3L_AT_LEAST(2,0,0)
    static std::string command(const int32, const K3L_COMMAND * const,
                               const R2CountryType = R2_COUNTRY_BRA,
                               const Presentation fmt = HUMAN);

    static std::string command(const int32, const int32, const int32, const char * const,
                               const R2CountryType = R2_COUNTRY_BRA,
                               const Presentation fmt = HUMAN);
#else
    static std::string command(const int32, const K3L_COMMAND * const,
                               const Presentation fmt = HUMAN);

    static std::string command(const int32, const int32, const int32, const char * const,
                               const Presentation fmt = HUMAN);
#endif

    static std::string deviceName(const KDeviceType, const int32, const int32 count = 0, const Presentation fmt = HUMAN);
    static std::string deviceName(const KDeviceType, const int32, const Presentation fmt);

    static std::string deviceType(const KDeviceType, const int32 count = 0, const Presentation fmt = HUMAN);
    static std::string deviceType(const KDeviceType, const Presentation fmt);

    static std::string deviceModel(const KDeviceType, const int32, const int32 count = 0, const Presentation fmt = HUMAN);
    static std::string deviceModel(const KDeviceType, const int32, const Presentation fmt);

    static std::string channelFeatures(const int32, const Presentation fmt = HUMAN);
    static std::string signaling(const KSignaling, const Presentation fmt = HUMAN);
    static std::string systemObject(const KSystemObject, const Presentation fmt = HUMAN);
    static std::string mixerTone(const KMixerTone, const Presentation fmt = HUMAN);
    static std::string mixerSource(const KMixerSource, const Presentation fmt = HUMAN);

    static std::string seizeFail(const KSeizeFail, const Presentation fmt = HUMAN);

#if K3L_AT_LEAST(2,0,0)
    static std::string callFail(const KSignaling, const R2CountryType, const int32, const Presentation fmt = HUMAN);
#else
    static std::string callFail(const KSignaling, const int32, const Presentation fmt = HUMAN);
#endif

    static std::string channelFail(const KSignaling, const int32, const Presentation fmt = HUMAN);
    static std::string internalFail(const KInternalFail, const Presentation fmt = HUMAN);

    static std::string linkErrorCounter(const KLinkErrorCounter, const Presentation fmt = HUMAN);

    static std::string linkStatus(const KSignaling, const int32, const Presentation fmt = HUMAN, const bool simpleStatus = false);
    static std::string channelStatus(const KSignaling, const int32, const Presentation fmt = HUMAN);
    static std::string callStatus(const KCallStatus, const Presentation fmt = HUMAN);
    static std::string status(const KLibraryStatus, const Presentation fmt = HUMAN);

#if K3L_AT_LEAST(2,0,0)
    static std::string signGroupB(const KSignGroupB, const R2CountryType contry = R2_COUNTRY_BRA,
        Presentation fmt = HUMAN);
#else
    static std::string signGroupB(const KSignGroupB, const Presentation fmt = HUMAN);
#endif

#if K3L_AT_LEAST(2,0,0)
    static std::string signGroupII(const KSignGroupII, const R2CountryType contry = R2_COUNTRY_BRA,
        Presentation fmt = HUMAN);
#else
    static std::string signGroupII(const KSignGroupII, const Presentation fmt = HUMAN);
#endif

    static std::string h100configIndex(const KH100ConfigIndex, const Presentation fmt = HUMAN);

    static std::string eventName(const int32 value)
    {
        return VerboseTraits::eventName((VerboseTraits::Event)value);
    };

    static std::string commandName(const int32 value)
    {
        return VerboseTraits::commandName((VerboseTraits::Command)value);
    };

#if K3L_AT_LEAST(1,5,0)
    static std::string sipFailures(const KSIP_Failures, const Presentation fmt = HUMAN);
#endif

#if K3L_AT_LEAST(1,5,1)
    static std::string isdnCause(const KQ931Cause, const Presentation fmt = HUMAN);
#endif

#if K3L_AT_LEAST(1,5,2)
    static std::string isdnDebug(const int32, const Presentation fmt = HUMAN);
#endif

#if K3L_AT_LEAST(1,6,0)
    static std::string callStartInfo(const KCallStartInfo, const Presentation fmt = HUMAN);

    static std::string gsmCallCause(const KGsmCallCause, const Presentation fmt = HUMAN);
    static std::string gsmMobileCause(const KGsmMobileCause, const Presentation fmt = HUMAN);
    static std::string gsmSmsCause(const KGsmSmsCause, const Presentation fmt = HUMAN);

    static std::string q931ProgressIndication(const KQ931ProgressIndication,
        Presentation fmt = HUMAN);
#endif

#if K3L_AT_LEAST(2,1,0)
    static std::string faxResult(const KFaxResult code, const Presentation fmt = HUMAN);
    static std::string faxFileErrorCause(const KFaxFileErrorCause code, const Presentation fmt = HUMAN);
#endif

    /* end of static (class) stuff */

 private:
    static std::string internal_deviceType(const KDeviceType, const int32);
    static std::string internal_deviceModel(const KDeviceType, const int32, const int32);

#if K3L_AT_LEAST(1,5,0)
    static std::string internal_sipFailures(const KSIP_Failures, const Presentation fmt = HUMAN);
#endif
#if K3L_AT_LEAST(1,5,1)
    static std::string internal_isdnCause(const KQ931Cause, const Presentation fmt = HUMAN);
#endif

#if K3L_AT_LEAST(2,0,0)
    static std::string internal_signGroupB(const KSignGroupB, const R2CountryType contry, const Presentation fmt = HUMAN);
    static std::string internal_signGroupII(const KSignGroupII, const R2CountryType contry, const Presentation fmt = HUMAN);
#else
    static std::string internal_signGroupB(const KSignGroupB, const Presentation fmt = HUMAN);
    static std::string internal_signGroupII(const KSignGroupII, const Presentation fmt = HUMAN);
#endif

#if K3L_AT_LEAST(1,6,0)
    static std::string internal_gsmCallCause(const KGsmCallCause, const Presentation fmt = HUMAN);
    static std::string internal_gsmMobileCause(const KGsmMobileCause, const Presentation fmt = HUMAN);
    static std::string internal_gsmSmsCause(const KGsmSmsCause, const Presentation fmt = HUMAN);

    static std::string internal_q931ProgressIndication(const KQ931ProgressIndication, const Presentation fmt = HUMAN);
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

        const Type    type;
        const int32 device;
        const int32 object;
    };

    static void generate(std::string &, const std::string &, const Target, const std::string &);

    static std::string show(std::string &, const std::string &, const Target, const std::string &);
    static std::string show(std::string &, const std::string &, const Target);

    template < typename ReturnType >
    static ReturnType presentation(const Presentation fmt, ReturnType str_exact, ReturnType str_human)
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
