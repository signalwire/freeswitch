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

#ifndef _KHOMP_PVT_FXO_H_
#define _KHOMP_PVT_FXO_H_

#include "khomp_pvt.h"
#include "applications.h"

/******************************************************************************/
/********************************* FXO Board **********************************/
/******************************************************************************/
struct BoardFXO: public Board
{
/******************************************************************************/
/******************************** FXO Channel *********************************/
struct KhompPvtFXO: public KhompPvt 
{
/********************************** FXO Call **********************************/
    struct CallFXO : public Call
    {
        CallFXO() {}

        bool process(std::string name, std::string value = "")
        {
            if (name == "pre")
            {
                DBG(FUNC, D("pre digits adjusted (%s).") % value);
                _pre_digits = value;
            }
            else if (name == "answer_info")
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
            else if (name == "usr_xfer")
            {
                _user_xfer_digits = value;
            }
            else
            {            
                return Call::process(name, value);
            }

            return true;
        }
        
        bool clear()
        {
            _pre_digits.clear();
            _call_info_report = false;
            _call_info_drop = 0;

            _user_xfer_digits = Opt::_options._user_xfer_digits();
            _user_xfer_buffer.clear();
            _digits_buffer.clear();

            _var_fax_adjust = T_UNKNOWN;
 
            return Call::clear();
        }

        std::string  _pre_digits;

        /* report what we got? */
        bool _call_info_report;

        /* what call info flags should make us drop the call? */
        long int _call_info_drop;

        /* used for xfer on user signaling */
        std::string _user_xfer_digits;
        std::string _user_xfer_buffer;
        std::string _digits_buffer;

        TriState _var_fax_adjust;

        ChanTimer::Index _busy_disconnect;        
    };
/******************************************************************************/
    KhompPvtFXO(K3LAPIBase::GenericTarget & target) : KhompPvt(target) 
    {
        _fax = new Fax(this);
        _transfer = new Transfer<CallFXO>(this);
        command(KHOMP_LOG, CM_ENABLE_CALL_ANSWER_INFO);
    }

    ~KhompPvtFXO() 
    {
        delete _fax;
        delete _transfer;
    }

    CallFXO * callFXO()
    {
        return (CallFXO *)call();
    }

    int makeCall(std::string params = "");
    bool doChannelAnswer(CommandRequest &);

    bool onNewCall(K3L_EVENT *e);
    bool onDisconnect(K3L_EVENT *e);
    bool onChannelRelease(K3L_EVENT *e);
    bool onCallSuccess(K3L_EVENT *e);
    bool onCallFail(K3L_EVENT *e);
    bool onAudioStatus(K3L_EVENT *e);
    bool onSeizeSuccess(K3L_EVENT *e);
    bool onDtmfDetected(K3L_EVENT *e);
    bool onDtmfSendFinish(K3L_EVENT *);
    bool onCallAnswerInfo(K3L_EVENT *e);

    virtual bool eventHandler(K3L_EVENT *e)
    {
        DBG(STRM, D("(FXO) c"));

        bool ret = true;

        switch(e->Code)
        {
            case EV_NEW_CALL:
                ret = onNewCall(e);
                break;
            case EV_DISCONNECT:
                ret = onDisconnect(e);
                break;
            case EV_CHANNEL_FREE:
            case EV_CHANNEL_FAIL:
                ret = onChannelRelease(e);
                break;
            case EV_CALL_SUCCESS:
                ret = onCallSuccess(e);
                break;
            case EV_CALL_FAIL:
                ret = onCallFail(e);
                break;
            case EV_AUDIO_STATUS:
                ret = onAudioStatus(e);
                break;
            case EV_SEIZE_SUCCESS:
                ret = onSeizeSuccess(e);
                break;
            case EV_DTMF_DETECTED:
            case EV_PULSE_DETECTED:
                ret = onDtmfDetected(e);
                break;
            case EV_DTMF_SEND_FINISH:
                ret = onDtmfSendFinish(e);
                break;
            case EV_CALL_ANSWER_INFO:
                ret = onCallAnswerInfo(e);
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
            case EV_POLARITY_REVERSAL:
                break;
            default:
                ret = KhompPvt::eventHandler(e);
                break;
        }

        DBG(STRM, D("(FXO) r"));

        return ret;
    }

    bool application(ApplicationType type, switch_core_session_t * session, const char *data);

    bool setupConnection();
    bool autoGainControl(bool enable);
    void setAnswerInfo(int answer_info);
    bool indicateBusyUnlocked(int cause, bool sent_signaling = false);
    static void busyDisconnect(Board::KhompPvt * pvt);
    void reportFailToReceive(int fail_code);
    bool validContexts(MatchExtension::ContextListType & contexts, 
                       std::string extra_context = "");
  
    bool isOK(void); 

    bool isPhysicalFree() 
    {
        K3L_CHANNEL_STATUS status;

        if (k3lGetDeviceStatus (_target.device, _target.object + ksoChannel, &status, sizeof (status)) != ksSuccess)
            return false; 

        bool physically_free = (status.AddInfo == kfcsEnabled);

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
            Board::board(_target.device)->_timers.del(callFXO()->_busy_disconnect);
        }
        catch (K3LAPITraits::invalid_device & err)
        {
            LOG(ERROR, PVT_FMT(target(), "Unable to get device: %d!") % err.device);
        }

        call()->_flags.clear(Kflags::CALL_WAIT_SEIZE);
        call()->_flags.clear(Kflags::EARLY_RINGBACK);

        _transfer->clear();
        callFXO()->_busy_disconnect.reset();

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

            callFXO()->_var_fax_adjust = (str_fax ? (!SAFE_strcasecmp(str_fax, "true") ? T_TRUE : T_FALSE) : T_UNKNOWN);
        }
        catch(Board::KhompPvt::InvalidSwitchChannel & err)
        {
            LOG(ERROR, PVT_FMT(_target, "(FXO) %s") % err._msg.c_str());
        }

        KhompPvt::getSpecialVariables();
    }

    bool sendDtmf(std::string digit);

    /* used by app FAX */
    Fax * _fax;

    Transfer<CallFXO> * _transfer;

//    static void delayedDisconnect(Board::KhompPvt * pvt);

    void cleanupIndications(bool force)
    {
        if (call()->_indication == INDICA_BUSY && !force)
        {
            DBG(FUNC, PVT_FMT(_target, "skipping busy indication cleanup on FXO channel."));
            return;
        }

        KhompPvt::cleanupIndications(force);
    }
};
/******************************************************************************/
/******************************************************************************/
    BoardFXO(int id) : Board(id) {}

    void initializeChannels(void)
    {
        LOG(MESSAGE, "(FXO) loading channels ...");

        for (unsigned obj = 0; obj < Globals::k3lapi.channel_count(_device_id); obj++)
        {
            K3LAPIBase::GenericTarget tgt(Globals::k3lapi, K3LAPIBase::GenericTarget::CHANNEL, _device_id, obj);
            KhompPvt * pvt;

            switch(Globals::k3lapi.channel_config(_device_id, obj).Signaling)
            {
            case ksigAnalog:
                pvt = new BoardFXO::KhompPvtFXO(tgt);
                pvt->_call = new BoardFXO::KhompPvtFXO::CallFXO();
                DBG(FUNC, "(FXO) FXO channel");
                break;
            default:
                pvt = new Board::KhompPvt(tgt);
                pvt->_call = new Board::KhompPvt::Call();
                DBG(FUNC, FMT("(FXO) signaling %d unknown") % Globals::k3lapi.channel_config(_device_id, obj).Signaling);
                break;
            }

            _channels.push_back(pvt);

            pvt->cleanup();

        }
    }

    /*
    virtual bool eventHandler(const int obj, K3L_EVENT *e)
    {
        DBG(STRM, D("(FXO Board) c"));

        bool ret = true;

        switch(e->Code)
        {
        case :
            break;
        default:
            ret = Board::eventHandler(obj, e);
            break;
        }

        DBG(STRM, D("(FXO Board) r"));

        return ret;
    }
    */
   
};
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
#endif /* _KHOMP_PVT_FXO_H_*/

