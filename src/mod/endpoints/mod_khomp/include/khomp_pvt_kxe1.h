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

#ifndef _KHOMP_PVT_E1_H_
#define _KHOMP_PVT_E1_H_

#include "khomp_pvt.h"

#include "applications.h"

/******************************************************************************/
/********************************** E1 Board **********************************/
/******************************************************************************/
struct BoardE1: public Board
{
/******************************************************************************/
/********************************* E1 Channel *********************************/
struct KhompPvtE1: public KhompPvt 
{
/*********************************** E1 Call **********************************/
    struct CallE1 : public Call
    {
        CallE1() {}

        bool process(std::string name, std::string value = "")
        {
            if (name == "answer_info")
            {
                _call_info_report = true;
            }
            else if (name == "drop_on")
            {
                _call_info_report = true;

                Strings::vector_type drop_item;
                Strings::tokenize (value, drop_item, ".+");

                for (Strings::vector_type::iterator i = drop_item.begin(); i != drop_item.end(); i++)
                {

                         if ((*i) == "message_box")        _call_info_drop |= CI_MESSAGE_BOX;
                    else if ((*i) == "human_answer")       _call_info_drop |= CI_HUMAN_ANSWER;
                    else if ((*i) == "answering_machine")  _call_info_drop |= CI_ANSWERING_MACHINE;
                    else if ((*i) == "carrier_message")    _call_info_drop |= CI_CARRIER_MESSAGE;
                    else if ((*i) == "unknown")            _call_info_drop |= CI_UNKNOWN;
                    else
                    {
                        LOG(ERROR, FMT("unknown paramenter to 'calldrop' Dial option: '%s'.") % (*i));
                        continue;
                    }

                    DBG(FUNC, FMT("droping call on '%s'.") % (*i));
                }
            }
            else
            {            
                return Call::process(name, value);
            }

            return true;
        }
        
        bool clear()
        {
            _call_info_report = false;
            _call_info_drop = 0;

            _var_fax_adjust = T_UNKNOWN;
 
            return Call::clear();
        }

        /* report what we got? */
        bool _call_info_report;

        /* what call info flags should make us drop the call? */
        long int _call_info_drop;

        TriState _var_fax_adjust;

        ChanTimer::Index _idx_disconnect;
    
    };
/******************************************************************************/
    KhompPvtE1(K3LAPIBase::GenericTarget & target) : KhompPvt(target)
    {
        _fax = new Fax(this);
        command(KHOMP_LOG,CM_ENABLE_CALL_ANSWER_INFO); 
    }

    ~KhompPvtE1() 
    {
        delete _fax;
    }

    CallE1 * callE1()
    {
        return (CallE1 *)call();
    }

    int makeCall(std::string params = "");

    bool onChannelRelease(K3L_EVENT *e);
    bool onCallSuccess(K3L_EVENT *e);
    bool onAudioStatus(K3L_EVENT *e);
    bool onCallAnswerInfo(K3L_EVENT *e);
    bool onDisconnect(K3L_EVENT *e);
    
    virtual bool eventHandler(K3L_EVENT *e)
    {
        DBG(STRM, D("(E1) c"));

        bool ret = true;

        switch(e->Code)
        {
            case EV_CHANNEL_FREE:
            case EV_CHANNEL_FAIL:
                ret = onChannelRelease(e);
                break;
            case EV_CALL_ANSWER_INFO:
                ret = onCallAnswerInfo(e);
                break;
            case EV_DISCONNECT:
                ret = onDisconnect(e);
                break;                            
            case EV_AUDIO_STATUS:
                ret = onAudioStatus(e);
                break;
            case EV_FAX_CHANNEL_FREE:
                ret = _fax->onFaxChannelRelease(e);
                break;
            case EV_FAX_FILE_SENT:
            case EV_FAX_FILE_FAIL:
            case EV_FAX_TX_TIMEOUT:
            case EV_FAX_PAGE_CONFIRMATION:
            case EV_FAX_REMOTE_INFO:
                break;
            default:
                ret = KhompPvt::eventHandler(e);
                break;
        }        

        DBG(STRM, D("(E1) r"));

        return ret;
    }
    
    bool application(ApplicationType type, switch_core_session_t * session, const char *data);
    
    bool setupConnection();
    bool indicateBusyUnlocked(int cause, bool sent_signaling = false);
    void setAnswerInfo(int answer_info);
    bool validContexts(MatchExtension::ContextListType & contexts, 
                       std::string extra_context = "");
    bool isOK(void); 

    bool isPhysicalFree() 
    {
        K3L_CHANNEL_STATUS status;

        if (k3lGetDeviceStatus (_target.device, _target.object + ksoChannel, &status, sizeof (status)) != ksSuccess)
            return false; 

        bool physically_free = (status.AddInfo == kecsFree);

        if(status.CallStatus != kcsFree || !physically_free)
        {
            DBG(FUNC, PVT_FMT(_target, "call status not free, or not physically free!"));
            return false;
        }

        return true;
    }

    virtual bool cleanup(CleanupType type = CLN_HARD)
    {
        try
        {
            Board::board(_target.device)->_timers.del(callE1()->_idx_disconnect);
        }
        catch (K3LAPITraits::invalid_device & err)
        {
            LOG(ERROR, PVT_FMT(target(), "Unable to get device: %d!") % err.device);
        }

        callE1()->_idx_disconnect.reset();

        switch (type)
        {
        case CLN_HARD:
        case CLN_FAIL:
            call()->_flags.clear(Kflags::FAX_DETECTED);
            break;
        case CLN_SOFT:
            break;
        }

        return KhompPvt::cleanup(type);
    }

    virtual void getSpecialVariables()
    {
        try
        {
            const char * str_fax = getFSChannelVar("KAdjustForFax");

            callE1()->_var_fax_adjust = (str_fax ? (!SAFE_strcasecmp(str_fax, "true") ? T_TRUE : T_FALSE) : T_UNKNOWN);
        }
        catch(Board::KhompPvt::InvalidSwitchChannel & err)
        {
            LOG(ERROR, PVT_FMT(_target, "(E1) %s") % err._msg.c_str());
        }

        KhompPvt::getSpecialVariables();
    }
        
    /* used by app FAX */
    Fax * _fax;

    static void delayedDisconnect(Board::KhompPvt * pvt);
};
/******************************************************************************/
/********************************** ISDN Channel ******************************/
struct KhompPvtISDN: public KhompPvtE1 
{
/********************************** ISDN Call *********************************/
    struct CallISDN : public CallE1
    {
    
        CallISDN() {}

        bool process(std::string name, std::string value = "")
        {
            if ((name == "uui" ) || (name == "uui_ex"))
            {
                if(value.find("#") != std::string::npos)
                {
                    Strings::vector_type values;
                    Strings::tokenize(value, values, "#", 2);

                    try
                    {
                        std::string uui_proto_s = values[0];
                        _uui_extended = (name == "uui_ex");
                        _uui_descriptor = Strings::toulong(uui_proto_s);
                        _uui_information.clear();

                        for (unsigned int i = 0; i < values[1].size(); ++i) 
                            _uui_information += STG(FMT("%02hhx") % ((unsigned char)values[1][i]));

                        DBG(FUNC, FMT("uui adjusted (ex=%s, proto=%s, data='%s')!") 
                                % (_uui_extended ? "true" : "false")
                                % uui_proto_s.c_str()
                                % _uui_information.c_str());
                    }
                    catch (...)
                    {
                        LOG(ERROR, FMT("invalid %s protocol descriptor: '%s' is not a number.") 
                                % (_uui_extended ? "uui_ex" : "uui")
                                % value.c_str());
                    }
                } 
                else
                {
                    LOG(ERROR, FMT("invalid %s protocol descriptor, need a '#'.") 
                            % (_uui_extended ? "uui_ex" : "uui"))
                }
            }
            else if (name == "usr_xfer")
            {
                _user_xfer_digits = value;
            }
            else
            {            
                return CallE1::process(name, value);
            }

            return true;
        }
    
        bool clear()
        {
            _uui_extended = false;
            _uui_descriptor = -1;
            _uui_information.clear();
            _isdn_cause = -1;

            _user_xfer_digits = Opt::_options._user_xfer_digits();
            _user_xfer_buffer.clear();
            _digits_buffer.clear();
            _qsig_number.clear();
    
            return CallE1::clear();
        }
        
        /* used for isdn EV_USER_INFORMATION */
        long int     _uui_descriptor;
        std::string  _uui_information;
        long int     _isdn_cause;
        bool         _uui_extended;

        /* what should we dial to trigger an user-signaled transfer? */
        /* used for xfer on user signaling */
        std::string _user_xfer_digits;
        std::string _user_xfer_buffer;
        std::string _digits_buffer;
        std::string _qsig_number;
        
        /* isdn information  */
        std::string _isdn_orig_type_of_number;
        std::string _isdn_orig_numbering_plan;
        std::string _isdn_dest_type_of_number;
        std::string _isdn_dest_numbering_plan;
        std::string _isdn_orig_presentation;

    };
/******************************************************************************/
    KhompPvtISDN(K3LAPIBase::GenericTarget & target) : KhompPvtE1(target) 
    {
        _transfer = new Transfer<CallISDN, false>(this);
    }

    ~KhompPvtISDN() 
    {
        delete _transfer;
    }

    CallISDN * callISDN()
    {
        return (CallISDN *)call();
    }
    
    int makeCall(std::string params = "");
    bool doChannelAnswer(CommandRequest &); 

    bool onSyncUserInformation(K3L_EVENT *e);
    bool onIsdnProgressIndicator(K3L_EVENT *e);
    bool onNewCall(K3L_EVENT *e);
    bool onCallSuccess(K3L_EVENT *e);
    bool onCallFail(K3L_EVENT *e);

    virtual bool eventHandler(K3L_EVENT *e)
    {
        DBG(STRM, D("(ISDN) c"));

        bool ret = true;
        
        switch(e->Code)
        {
            case EV_USER_INFORMATION:
                ret = onSyncUserInformation(e);
                break;
            case EV_ISDN_PROGRESS_INDICATOR:
                ret = onIsdnProgressIndicator(e);
                break;
            case EV_NEW_CALL:
                ret = onNewCall(e);
                break;
            case EV_CALL_SUCCESS:
                ret = onCallSuccess(e);
                break;
            case EV_CALL_FAIL:
                ret = onCallFail(e);
                break;
            case EV_SS_TRANSFER_FAIL:
                break;
            default:
                ret = KhompPvtE1::eventHandler(e);
                break;
        }        

        DBG(STRM, D("(ISDN) r"));

        return ret;
    }
    
    bool application(ApplicationType type, switch_core_session_t * session, const char *data);

    int causeFromCallFail(int fail);
    int callFailFromCause(int cause);
    void reportFailToReceive(int fail_code);
    RingbackDefs::RingbackStType sendRingBackStatus(int rb_value = RingbackDefs::RB_SEND_DEFAULT); 
    bool sendPreAudio(int rb_value = RingbackDefs::RB_SEND_NOTHING);
    bool sendDtmf(std::string digit);

    virtual bool cleanup(CleanupType type = CLN_HARD)
    {
        _transfer->clear();
        
        /*
        switch (type)
        {
        case CLN_HARD:
        case CLN_FAIL:
            break;
        case CLN_SOFT:
            break;
        }
        */

        return KhompPvtE1::cleanup(type);
    }

    virtual void setSpecialVariables()
    {
        try
        {
            /* rdsi user info */
            if (callISDN()->_uui_descriptor != -1)
            {
                DBG(FUNC,"Setting ISDN descriptor");

                std::string descriptor = STG(FMT("%d") % callISDN()->_uui_descriptor);

                setFSChannelVar("KUserInfoExtended", (callISDN()->_uui_extended ? "true" : "false"));
                setFSChannelVar("KUserInfoDescriptor", descriptor.c_str());
                setFSChannelVar("KUserInfoData", callISDN()->_uui_information.c_str());

                callISDN()->_uui_extended = false; 
                callISDN()->_uui_descriptor = -1;
                callISDN()->_uui_information.clear();
            }

            if (!callISDN()->_isdn_orig_type_of_number.empty())
                setFSChannelVar("KISDNOrigTypeOfNumber", callISDN()->_isdn_orig_type_of_number.c_str());

            if (!callISDN()->_isdn_dest_type_of_number.empty())
                setFSChannelVar("KISDNDestTypeOfNumber", callISDN()->_isdn_dest_type_of_number.c_str());

            if (!callISDN()->_isdn_orig_numbering_plan.empty())
                setFSChannelVar("KISDNOrigNumberingPlan", callISDN()->_isdn_orig_numbering_plan.c_str());

            if (!callISDN()->_isdn_dest_numbering_plan.empty())
                setFSChannelVar("KISDNDestNumberingPlan", callISDN()->_isdn_dest_numbering_plan.c_str());

            if (!callISDN()->_isdn_orig_presentation.empty())
                setFSChannelVar("KISDNOrigPresentation", callISDN()->_isdn_orig_presentation.c_str());
        }
        catch(Board::KhompPvt::InvalidSwitchChannel & err)
        {
            LOG(ERROR, PVT_FMT(_target, "(ISDN) %s") % err._msg.c_str());
        }

        KhompPvtE1::setSpecialVariables();
    }

    virtual void getSpecialVariables()
    {
        try
        {
            const char * isdn_orig_type = getFSChannelVar("KISDNOrigTypeOfNumber");
            const char * isdn_dest_type = getFSChannelVar("KISDNDestTypeOfNumber");
            const char * isdn_orig_numbering = getFSChannelVar("KISDNOrigNumberingPlan");
            const char * isdn_dest_numbering = getFSChannelVar("KISDNDestNumberingPlan");
            const char * isdn_orig_presentation = getFSChannelVar("KISDNOrigPresentation");

            LOG(ERROR, PVT_FMT(_target,"ISDNORIG: %s") % (isdn_orig_type ? isdn_orig_type : ""));

            callISDN()->_isdn_orig_type_of_number = (isdn_orig_type ? isdn_orig_type : "");
            callISDN()->_isdn_dest_type_of_number = (isdn_dest_type ? isdn_dest_type : "");
            callISDN()->_isdn_orig_numbering_plan = (isdn_orig_numbering ? isdn_orig_numbering : "");
            callISDN()->_isdn_dest_numbering_plan = (isdn_dest_numbering ? isdn_dest_numbering : "");
            callISDN()->_isdn_orig_presentation   = (isdn_orig_presentation ? isdn_orig_presentation : "");
        }
        catch(Board::KhompPvt::InvalidSwitchChannel & err)
        {
            LOG(ERROR, PVT_FMT(_target, "(ISDN) %s") % err._msg.c_str());
        }

        KhompPvt::getSpecialVariables();
    }
       
    Transfer<CallISDN, false> * _transfer;
};
/******************************************************************************/
/********************************* R2 Channel *********************************/
struct KhompPvtR2: public KhompPvtE1
{
/********************************* R2 Call ************************************/
    struct CallR2 : public CallE1
    {
    
        CallR2() {}


        bool process(std::string name, std::string value = "")
        {
            if (name == "category")
            {
                try
                {
                    unsigned long int category = Strings::toulong (value);
                    DBG(FUNC, FMT("r2 category adjusted (%s)!") % value.c_str());
                    _r2_category = category;
                }
                catch (...)
                {
                   LOG(ERROR, FMT("invalid r2 category: '%s' is not a number.") % value.c_str());
                }
            }
            else
            {
                return CallE1::process(name, value);
            }
            return true;
        }
        
        bool clear()
        {
            _r2_category  = -1; 
            _r2_condition = -1;

            return CallE1::clear();
        }

        long int _r2_category;
        long int _r2_condition;

        ChanTimer::Index _idx_number_dial;
        std::string _incoming_exten;
    };
/******************************************************************************/
    KhompPvtR2(K3LAPIBase::GenericTarget & target) : KhompPvtE1(target) 
    {
        K3L_E1600A_FW_CONFIG dspAcfg;

        if (k3lGetDeviceConfig(_target.device, ksoFirmware + kfiE1600A, &dspAcfg, sizeof(dspAcfg)) != ksSuccess)
        {
            DBG(FUNC, PVT_FMT(target, "unable to get signaling locality for board: assuming brazilian signaling"));
 
            _r2_country = Verbose::R2_COUNTRY_BRA;
           
            return;
        }
        Regex::Expression e(".+\\((Arg|Bra|Chi|Mex|Ury|Ven)\\).+", Regex::E_EXTENDED);
        std::string fwname(dspAcfg.FwVersion);

        Regex::Match what(fwname, e);

        if (!what.matched() || !what.matched(1))
        {
            DBG(FUNC, PVT_FMT(target, "invalid firmware string, unable to find country code: assuming brazilian signaling.\n"));
            
            _r2_country = Verbose::R2_COUNTRY_BRA;
            return;
        }

        std::string country = what.submatch(1);
    
        /**/ if (country == "Arg")
            _r2_country = Verbose::R2_COUNTRY_ARG;
        else if (country == "Bra")
            _r2_country = Verbose::R2_COUNTRY_BRA;
        else if (country == "Chi")
            _r2_country = Verbose::R2_COUNTRY_CHI;
        else if (country == "Mex")
            _r2_country = Verbose::R2_COUNTRY_MEX;
        else if (country == "Ury")
            _r2_country = Verbose::R2_COUNTRY_URY;
        else if (country == "Ven")
            _r2_country = Verbose::R2_COUNTRY_VEN;
        else
        {
            DBG(FUNC, PVT_FMT(target, "invalid firmware string (%s), assuming brazilian signaling.") % country.c_str());

            _r2_country = Verbose::R2_COUNTRY_BRA;
            return;
        }
    
        DBG(FUNC, PVT_FMT(target, "adjusting country signaling to code '%s'...")
            % country.c_str());

    }

    ~KhompPvtR2() {}
    
    CallR2 * callR2()
    {
        return (CallR2 *)call();
    }

    bool forceDisconnect(void)
    {
        char cmd[] = { 0x07, (char)(_target.object + 1) };
        
        try
        {
            Globals::k3lapi.raw_command(_target.device, 0, cmd, sizeof(cmd));
        }
        catch(K3LAPI::failed_raw_command &e)
        {
            return false;
        }

        return true;
    };

    int makeCall(std::string params = "");
    bool doChannelAnswer(CommandRequest &); 
    bool doChannelHangup(CommandRequest &);

    bool onNewCall(K3L_EVENT *e);
    bool onCallSuccess(K3L_EVENT *e);
    bool onCallFail(K3L_EVENT *e);
    bool onNumberDetected(K3L_EVENT *e);
    
    virtual bool eventHandler(K3L_EVENT *e)
    {
        DBG(STRM, D("(R2) c"));

        bool ret = true;

        switch(e->Code)
        {
            case EV_NEW_CALL:
                ret = onNewCall(e);
                break;
            case EV_CALL_SUCCESS:
                ret = onCallSuccess(e);
                break;
            case EV_CALL_FAIL:
                ret = onCallFail(e);
                break;
            case EV_DIALED_DIGIT:
                ret = onNumberDetected(e);
                break;
            default:
                ret = KhompPvtE1::eventHandler(e);
                break;
        }        

        DBG(STRM, D("(R2) r"));

        return ret;

    }
    
    int causeFromCallFail(int fail);
    int callFailFromCause(int cause);
    void reportFailToReceive(int fail_code);
    RingbackDefs::RingbackStType sendRingBackStatus(int rb_value = RingbackDefs::RB_SEND_DEFAULT); 
    bool sendPreAudio(int rb_value = RingbackDefs::RB_SEND_NOTHING);
    bool indicateRinging();

    virtual void setSpecialVariables()
    {
        try
        {
            /* r2 caller category */
            if (callR2()->_r2_category != -1)
            {
                setFSChannelVar("KR2GotCategory",STG(FMT("%d") % callR2()->_r2_category).c_str());
                setFSChannelVar("KR2StrCategory",Verbose::signGroupII((KSignGroupII)callR2()->_r2_category).c_str());
            }
        }
        catch(Board::KhompPvt::InvalidSwitchChannel & err)
        {
            LOG(ERROR, PVT_FMT(_target, "(R2) %s") % err._msg.c_str());
        }

        KhompPvtE1::setSpecialVariables();
    }

    virtual bool cleanup(CleanupType type = CLN_HARD)
    {
        call()->_flags.clear(Kflags::NEEDS_RINGBACK_CMD);
        call()->_flags.clear(Kflags::NUMBER_DIAL_ONGOING);
        call()->_flags.clear(Kflags::NUMBER_DIAL_FINISHD);

        switch (type)
        {
        case CLN_HARD:
        case CLN_FAIL:
            break;
        case CLN_SOFT:
            break;
        }

        return KhompPvtE1::cleanup(type);
    }

    static void numberDialTimer(Board::KhompPvt * pvt);

public:
    Verbose::R2CountryType _r2_country;

};
/******************************************************************************/
/********************************* FLASH Channel ******************************/
/* ksigLineSide ksigCAS_EL7 ksigE1LC */
struct KhompPvtFlash: public KhompPvtR2 
{
/********************************* R2 Call ************************************/
    struct CallFlash : public CallR2
    {
    
        CallFlash() {}

        bool process(std::string name, std::string value = "")
        {
            if (name == "usr_xfer")
            {
                _user_xfer_digits = value;
            }
            else
            {
                return CallR2::process(name, value);
            }

            return true;
        }
        
        bool clear()
        {
            _user_xfer_digits = Opt::_options._user_xfer_digits();
            _user_xfer_buffer.clear();
            _digits_buffer.clear();

            return CallR2::clear();
        }

        /* used for xfer on user signaling */
        std::string _user_xfer_digits;
        std::string _user_xfer_buffer;
        std::string _digits_buffer;

    };
/******************************************************************************/
    KhompPvtFlash(K3LAPIBase::GenericTarget & target) : KhompPvtR2(target) 
    {
        _transfer = new Transfer<CallFlash>(this);
    }
    
    ~KhompPvtFlash() 
    {
        delete _transfer;
    }
    
    CallFlash * callFlash()
    {
        return (CallFlash *)call();
    }

    bool application(ApplicationType type, switch_core_session_t * session, const char *data);

    bool sendDtmf(std::string digit);

    virtual bool cleanup(CleanupType type = CLN_HARD)
    {
        _transfer->clear();

        /*
        switch (type)
        {
        case CLN_HARD:
        case CLN_FAIL:
            break;
        case CLN_SOFT:
            break;
        }
        */

        return KhompPvtR2::cleanup(type);
    }

    Transfer<CallFlash> * _transfer;
};
/******************************************************************************/
/********************************* FSX Channel ********************************/
struct KhompPvtFXS: public KhompPvt 
{
/********************************** FXS Call **********************************/
    struct CallFXS : public Call
    {
        CallFXS() {}

        bool process(std::string name, std::string value = "")
        {
            if (name == "ring")
            {
                Strings::vector_type ring_item;
                Strings::tokenize (value, ring_item, ".");

                if (ring_item.size() != 2)
                {
                    LOG(ERROR, FMT("invalid values on ring string: two numbers, dot separated, are needed."));
                    return false;
                }

                try
                {
                    unsigned long int time_on = Strings::toulong (ring_item[0]);
                    unsigned long int time_off = Strings::toulong (ring_item[1]);

                    _ring_on = time_on;
                    _ring_off = time_off;

                    DBG(FUNC, D("ring values adjusted (%i,%i).") % time_on % time_off);
                }
                catch (...)
                {
                    LOG(ERROR, FMT("invalid number on ring string."));
                }
            }
            else if (name == "ring_ext")
            {
                if (_ring_on == -1) // so ring was not set.
                {
                    LOG(ERROR, FMT("ring_ext only make sense if ring values are set."));
                    return false;
                }

                Strings::vector_type ring_item;
                Strings::tokenize (value, ring_item, ".");

                if (ring_item.size() != 2)
                {
                    LOG(ERROR, FMT("invalid values on ring_ext string: two numbers, dot separated, are needed."));
                    return false;
                }

                try
                {
                    unsigned long int time_on = Strings::toulong (ring_item[0]);
                    unsigned long int time_off = Strings::toulong (ring_item[1]);

                    _ring_on_ext = time_on;
                    _ring_off_ext = time_off;

                    DBG(FUNC, D("ring_ext values adjusted (%i,%i).") % time_on % time_off);
                }
                catch (...)
                {
                    LOG(ERROR, FMT("invalid number on ring_ext string."));
                }
            }
            else if (name == "ring_cadence")
            {
                CadencesMapType::iterator i = Opt::_cadences.find(value);

                if (i != Opt::_cadences.end())
                {
                    CadenceType cadence = (*i).second;

                    _ring_on      = cadence.ring;
                    _ring_off     = cadence.ring_s;
                    _ring_on_ext  = cadence.ring_ext;
                    _ring_off_ext = cadence.ring_ext_s;

                    DBG(FUNC, D("cadence adjusted (%i,%i,%i,%i).") 
                            % cadence.ring
                            % cadence.ring_s 
                            % cadence.ring_ext 
                            % cadence.ring_ext_s);
                }
                else
                {
                    LOG(ERROR, FMT("unable to find cadence '%s'!") % value);
                }
            }
            else
            {            
                return Call::process(name, value);
            }

            return true;
        }
        
        bool clear()
        {
            _ring_on      = -1;
            _ring_off     = -1;
            _ring_on_ext  = -1;
            _ring_off_ext = -1;

            _incoming_exten.clear();
            //_flash_transfer.clear();

            //_uuid_other_session.clear();
 
            return Call::clear();
        }

        long int _ring_on;
        long int _ring_off;
        long int _ring_on_ext;
        long int _ring_off_ext;

        ChanTimer::Index _idx_dial;

        std::string _incoming_exten;
        
        //ChanTimer::Index _idx_transfer;

        //std::string _flash_transfer;
        //std::string _uuid_other_session;
        

    };
/******************************************************************************/
    KhompPvtFXS(K3LAPIBase::GenericTarget & target) : KhompPvt(target)
    {
       //command(KHOMP_LOG,CM_ENABLE_CALL_ANSWER_INFO);
       /* sequence numbers on FXS */
       static OrigToNseqMapType fxs_nseq = generateNseqMap();

       load(fxs_nseq);
    }

    ~KhompPvtFXS() {}

    CallFXS * callFXS()
    {
        return (CallFXS *)call();
    }

    int makeCall(std::string params = "");
    bool doChannelAnswer(CommandRequest &);
    bool doChannelHangup(CommandRequest &);
   
    bool onChannelRelease(K3L_EVENT *e);
    bool onSeizureStart(K3L_EVENT *);
    bool onCallSuccess(K3L_EVENT *);
    bool onDtmfDetected(K3L_EVENT *);
    //bool onDtmfSendFinish(K3L_EVENT *);
    bool onFlashDetected(K3L_EVENT *);
    
    virtual bool eventHandler(K3L_EVENT *e)
    {
        DBG(STRM, D("(FXS) c"));

        bool ret = true;

        switch(e->Code)
        {
            case EV_CHANNEL_FREE:
            case EV_CHANNEL_FAIL:
                ret = onChannelRelease(e);
                break;
            case EV_SEIZURE_START:
                ret = onSeizureStart(e);                
                break;
            case EV_CALL_SUCCESS:
                ret = onCallSuccess(e);
                break;
            case EV_DTMF_DETECTED:
            case EV_PULSE_DETECTED:
                ret = onDtmfDetected(e);
                break;
            /*
            case EV_DTMF_SEND_FINISH:
                ret = onDtmfSendFinish(e);
                break;
            */
            case EV_FLASH:
                ret = onFlashDetected(e);
                break;
            default:
                ret = KhompPvt::eventHandler(e);
                break;
        }        

        DBG(STRM, D("(FXS) r"));

        return ret;
    }

    bool setupConnection();
    void load(OrigToNseqMapType & fxs_nseq);
    void loadOptions();
    bool parseBranchOptions(std::string options_str);
    bool alloc();
    bool indicateBusyUnlocked(int cause, bool sent_signaling = false);
    void reportFailToReceive(int fail_code);
    bool validContexts(MatchExtension::ContextListType & contexts, 
                       std::string extra_context = "");
    bool isOK(void);

    //bool startTransfer();
    //bool stopTransfer();
    //bool transfer(std::string & context, bool blind = false);

    bool hasNumberDial() { return false; }
    
    bool isPhysicalFree() 
    {
        K3L_CHANNEL_STATUS status;

        if (k3lGetDeviceStatus (_target.device, _target.object + ksoChannel, &status, sizeof (status)) != ksSuccess)
            return false; 

        bool physically_free = (status.AddInfo == kfxsOnHook);

        if(status.CallStatus != kcsFree || !physically_free)
        {
            DBG(FUNC, PVT_FMT(_target, "call status not free, or not physically free!"));
            return false;
        }

        return true;
    }
    
    virtual bool cleanup(CleanupType type = CLN_HARD)
    {
        callFXS()->_flags.clear(Kflags::FXS_DIAL_ONGOING);
        callFXS()->_flags.clear(Kflags::FXS_DIAL_FINISHD);
        callFXS()->_flags.clear(Kflags::FXS_FLASH_TRANSFER);

        switch (type)
        {
        case CLN_HARD:
        case CLN_FAIL:
            callFXS()->_flags.clear(Kflags::FXS_OFFHOOK);        
            break;
        case CLN_SOFT:
            break;
        }

        return KhompPvt::cleanup(type);
    }

    
    /* number based on fxs-global-orig */
    std::string _fxs_fisical_addr; 
    /* the branch number, possibly the new calleridnum */
    std::string _fxs_orig_addr; 
    std::string _calleridname;
    int         _amaflags;
    std::string _callgroup;
    std::string _pickupgroup;
    std::string _context;
    int         _input_volume;
    int         _output_volume;
    std::string _mailbox;
    std::string _flash;

    static OrigToNseqMapType generateNseqMap();
    static void dialTimer(KhompPvt * pvt);
    //static void transferTimer(KhompPvt * pvt);

    static std::string padOrig(std::string orig_base, unsigned int padding)
    {
        unsigned int orig_size = orig_base.size();
        unsigned int orig_numb = 0;

        try
        {
            orig_numb = Strings::toulong(orig_base);
        }
        catch(Strings::invalid_value & e)
        {
            LOG(ERROR, D("invalid numeric value: %s") % e.value());
        }

        return STG(FMT(STG(FMT("%%0%dd") % orig_size)) % (orig_numb + padding));
    }

    void cleanupIndications(bool force)
    {
        if (call()->_indication == INDICA_BUSY && !force)
        {
            DBG(FUNC, PVT_FMT(_target, "skipping busy indication cleanup on FXS channel."));
            return;
        }

        KhompPvt::cleanupIndications(force);
    }
};
/******************************************************************************/
/******************************************************************************/
    BoardE1(int id) : Board(id) {}

    void initializeChannels(void)
    {
        LOG(MESSAGE, "(E1) loading channels ...");

        for (unsigned obj = 0; obj < Globals::k3lapi.channel_count(_device_id); obj++)
        {
            K3LAPIBase::GenericTarget tgt(Globals::k3lapi, K3LAPIBase::GenericTarget::CHANNEL, _device_id, obj);
            KhompPvt * pvt;

            switch(Globals::k3lapi.channel_config(_device_id, obj).Signaling)
            {
            CASE_RDSI_SIG:
                pvt = new BoardE1::KhompPvtISDN(tgt);
                pvt->_call = new BoardE1::KhompPvtISDN::CallISDN();
                DBG(FUNC, "(E1) ISDN channel");
                break;
            CASE_R2_SIG:
                pvt = new BoardE1::KhompPvtR2(tgt);
                pvt->_call = new BoardE1::KhompPvtR2::CallR2();
                pvt->command(KHOMP_LOG, CM_DISCONNECT);
                DBG(FUNC, "(E1) R2 channel");
                break;
            CASE_FLASH_GRP:
                pvt = new BoardE1::KhompPvtFlash(tgt);
                pvt->_call = new BoardE1::KhompPvtFlash::CallFlash();
                pvt->command(KHOMP_LOG, CM_DISCONNECT);
                DBG(FUNC, "(E1) \"Flash\" channel");
                break;
            case ksigAnalogTerminal:
                pvt = new BoardE1::KhompPvtFXS(tgt);
                pvt->_call = new BoardE1::KhompPvtFXS::CallFXS();
                DBG(FUNC, "(E1) FXS channel");
                break;
            default:
                pvt = new Board::KhompPvt(tgt);
                pvt->_call = new Board::KhompPvt::Call();
                DBG(FUNC, FMT("(E1) signaling %d unknown") % Globals::k3lapi.channel_config(_device_id, obj).Signaling);
                break;
            }

            _channels.push_back(pvt);

            pvt->cleanup();

        }
    }

    bool onLinkStatus(K3L_EVENT *e);

    virtual bool eventHandler(const int obj, K3L_EVENT *e)
    {
        DBG(STRM, D("(E1 Board) c"));

        int ret = true;

        switch(e->Code)
        {
        case EV_LINK_STATUS:
        case EV_PHYSICAL_LINK_DOWN:
        case EV_PHYSICAL_LINK_UP:

            if (K::Logger::Logg.classe(C_LINK_STT).enabled() || K::Logger::Logg.classe(C_EVENT).enabled())
            {    
                std::string msg = Globals::verbose.event (obj, e) + "."; 
                LOG(LINK_STT, msg);
            }            

            ret = onLinkStatus(e);
            break;
        default:
            ret = Board::eventHandler(obj, e);
            break;
        }

        DBG(STRM, D("(E1 Board) r"));

        return ret;
    }
   
};
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
#endif /* _KHOMP_PVT_H_*/

