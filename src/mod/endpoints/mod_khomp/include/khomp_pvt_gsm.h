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

#ifndef _KHOMP_PVT_GSM_H_
#define _KHOMP_PVT_GSM_H_

#include "khomp_pvt.h"
#include "applications.h"

/******************************************************************************/
/********************************* GSM Board **********************************/
/******************************************************************************/
struct BoardGSM: public Board
{
/******************************************************************************/
/******************************** GSM Channel *********************************/
struct KhompPvtGSM: public KhompPvt 
{
/********************************** GSM Call **********************************/
    struct CallGSM : public Call
    {
        CallGSM() {}

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
 
            return Call::clear();
        }

        /* report what we got? */
        bool _call_info_report;

        /* what call info flags should make us drop the call? */
        long int _call_info_drop;
    };

/******************************************************************************/
    KhompPvtGSM(K3LAPIBase::GenericTarget & target) : KhompPvt(target) 
    {
        _sms = new SMS(this);
        command(KHOMP_LOG, CM_ENABLE_CALL_ANSWER_INFO);
    }

    ~KhompPvtGSM() 
    {
        delete _sms;
    }

    CallGSM * callGSM()
    {
        return (CallGSM *)call();
    }

    int makeCall(std::string params = "");
    bool doChannelAnswer(CommandRequest &);
    bool doChannelHangup(CommandRequest &);

    //bool onNewCall(K3L_EVENT *e);
    bool onChannelRelease(K3L_EVENT *e);
    bool onCallFail(K3L_EVENT *e);
    bool onCallSuccess(K3L_EVENT *e);
    bool onCallAnswerInfo(K3L_EVENT *e);
    bool onDisconnect(K3L_EVENT *e);
    //bool onDtmfDetected(K3L_EVENT *e);

    virtual bool eventHandler(K3L_EVENT *e)
    {
        DBG(STRM, D("(GSM) c"));

        bool ret = true;

        switch(e->Code)
        {
            /*
            case EV_NEW_CALL:
                onNewCall(e);
                break;
            */
            case EV_CHANNEL_FREE:
            case EV_CHANNEL_FAIL:
                ret = onChannelRelease(e);
                break;
            case EV_CALL_SUCCESS:
                ret = onCallSuccess(e);
                break;
            case EV_DISCONNECT:
                ret = onDisconnect(e);
                break;
            case EV_CALL_FAIL:
                ret = onCallFail(e);
                break;
            case EV_CALL_ANSWER_INFO:
                ret = onCallAnswerInfo(e);
                break;
            case EV_RECV_FROM_MODEM:
                break;
            case EV_NEW_SMS:
                ret = _sms->onNewSMS(e);
                break;
            case EV_SMS_INFO:
                ret = _sms->onSMSInfo(e);
                break;
            case EV_SMS_DATA:
                ret = _sms->onSMSData(e);
                break;
            case EV_SMS_SEND_RESULT:
                ret = _sms->onSMSSendResult(e);
                break;
            default:
                ret = KhompPvt::eventHandler(e);
                break;
        }        

        DBG(STRM, D("(GSM) r"));

        return ret;
    }

    bool application(ApplicationType type, switch_core_session_t * session, const char *data);    
  
    bool setupConnection();
    void setAnswerInfo(int answer_info);
    bool indicateBusyUnlocked(int cause, bool sent_signaling = false);
    void reportFailToReceive(int fail_code);
    int causeFromCallFail(int fail);
    int callFailFromCause(int cause);
    bool isOK(void); 

    bool isPhysicalFree() 
    {
        K3L_CHANNEL_STATUS status;

        if (k3lGetDeviceStatus (_target.device, _target.object + ksoChannel, &status, sizeof (status)) != ksSuccess)
            return false; 

        bool physically_free = (status.AddInfo == kgsmIdle);

        if(status.CallStatus != kcsFree || !physically_free)
        {
            DBG(FUNC, PVT_FMT(_target, "call status not free, or not physically free!"));
            return false;
        }

        return true;
    }

    bool sendPreAudio(int rb_value = RingbackDefs::RB_SEND_NOTHING)
    {
        return false;
    }

    bool selectSimCard(const char * sim_card)
    {
        return command(KHOMP_LOG, CM_SIM_CARD_SELECT, sim_card);
    }

    std::string getStatistics(Statistics::Type type)
    {
        switch(type)
        {
            case Statistics::DETAILED:
            {
                /* buffer our data to return at the end */
                std::string strBuffer;

                strBuffer.append(_pvt_statistics->getDetailed());
                strBuffer.append(_sms->statistics()->getDetailed());
                return strBuffer;
            }
            case Statistics::ROW:
            {
                return _pvt_statistics->getRow();
            }
            default:
                return "";
        }
    }

    void clearStatistics()
    {
        KhompPvt::clearStatistics();
        _sms->statistics()->clear();
    }

    bool validContexts(MatchExtension::ContextListType & contexts, 
                       std::string extra_context = "");
        
    SMS * _sms;

};
/******************************************************************************/
/******************************************************************************/
    BoardGSM(int id) : Board(id) {}

    void initializeChannels(void)
    {
        LOG(MESSAGE, "(GSM) loading channels ...");

        for (unsigned obj = 0; obj < Globals::k3lapi.channel_count(_device_id); obj++)
        {
            K3LAPIBase::GenericTarget tgt(Globals::k3lapi, K3LAPIBase::GenericTarget::CHANNEL, _device_id, obj);
            KhompPvt * pvt;

            switch(Globals::k3lapi.channel_config(_device_id, obj).Signaling)
            {
            case ksigGSM:
                pvt = new BoardGSM::KhompPvtGSM(tgt);
                pvt->_call = new BoardGSM::KhompPvtGSM::CallGSM();
                ((BoardGSM::KhompPvtGSM *)pvt)->_sms->start();
                DBG(FUNC, "(GSM) GSM channel");
                break;
            default:
                pvt = new Board::KhompPvt(tgt);
                pvt->_call = new Board::KhompPvt::Call();
                DBG(FUNC, FMT("(GSM) signaling %d unknown") % Globals::k3lapi.channel_config(_device_id, obj).Signaling);
                break;
            }

            _channels.push_back(pvt);

            pvt->cleanup();

        }
    }

/*
    virtual bool eventHandler(const int obj, K3L_EVENT *e)
    {
        DBG(STRM, D("(GSM Board) c"));

        bool ret = true;

        switch(e->Code)
        {
        case :
            break;
        default:
            ret = Board::eventHandler(obj, e);
            break;
        }

        DBG(STRM, D("(GSM Board) r"));

        return ret;
    }
*/
   
};
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
#endif /* _KHOMP_PVT_GSM_H_*/

