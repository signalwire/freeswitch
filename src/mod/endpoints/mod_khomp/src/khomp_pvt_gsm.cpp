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

#include "khomp_pvt_gsm.h"
#include "lock.h"
#include "logger.h"

bool BoardGSM::KhompPvtGSM::isOK()
{
    try
    {
        ScopedPvtLock lock(this);

        K3L_CHANNEL_STATUS status;

        if (k3lGetDeviceStatus (_target.device, _target.object + ksoChannel, &status, sizeof (status)) != ksSuccess)
            return false;

        switch (status.AddInfo)
        {
            case kgsmModemError:
            case kgsmSIMCardError:
            case kgsmNetworkError:
            case kgsmNotReady:
                return false;
            default:
                return true;
        }

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(target(), "unable to lock %s!") % err._msg.c_str() );
    }

    return false;
}

bool BoardGSM::KhompPvtGSM::onChannelRelease(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_target, "(GSM) c"));

    bool ret = true;

    try
    {
        ScopedPvtLock lock(this);

        call()->_flags.clear(Kflags::HAS_PRE_AUDIO);
        command(KHOMP_LOG, CM_ENABLE_CALL_ANSWER_INFO);
   
        ret = KhompPvt::onChannelRelease(e);
    }
    catch(ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(target(), "(GSM) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }
    
    DBG(FUNC, PVT_FMT(_target, "(GSM) r"));   
    return ret;
}

bool BoardGSM::KhompPvtGSM::onCallSuccess(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_target, "(GSM) c"));

    bool ret = true;

    try
    {
        ScopedPvtLock lock(this);

        ret = KhompPvt::onCallSuccess(e);

        if (call()->_pre_answer)
        {
            dtmfSuppression(Opt::_options._out_of_band_dtmfs());

            startListen();
            startStream();
            switch_channel_mark_pre_answered(getFSChannel());
        }
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target, "(GSM) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }
    catch (Board::KhompPvt::InvalidSwitchChannel & err)
    {
        LOG(ERROR, PVT_FMT(target(), "(GSM) r (%s)") % err._msg.c_str() );
        return false;
    }
        
    DBG(FUNC, PVT_FMT(_target, "(GSM) r"));

    return ret;
}

void BoardGSM::KhompPvtGSM::setAnswerInfo(int answer_info)
{
    const char * value = answerInfoToString(answer_info);

    if (value == NULL)
    {
        DBG(FUNC, PVT_FMT(_target,"signaled unknown call answer info '%d', using 'Unknown'...") % answer_info);
        value = "Unknown";
    }
   
    DBG(FUNC,PVT_FMT(_target,"KCallAnswerInfo: %s") % value);

    try
    {
        setFSChannelVar("KCallAnswerInfo",value);
    }
    catch(Board::KhompPvt::InvalidSwitchChannel & err)
    {
        LOG(ERROR,PVT_FMT(_target,"Cannot obtain the channel variable: %s") % err._msg.c_str());
    }
}

bool BoardGSM::KhompPvtGSM::onCallAnswerInfo(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_target, "(GSM) c"));
    try
    {
        ScopedPvtLock lock(this);

        int info_code = -1;

        switch (e->AddInfo)
        {
            case kcsiCellPhoneMessageBox:
                info_code = CI_MESSAGE_BOX;
                break;
            case kcsiHumanAnswer:
                info_code = CI_HUMAN_ANSWER;
                break;
            case kcsiAnsweringMachine:
                info_code = CI_ANSWERING_MACHINE;
                break;
            case kcsiCarrierMessage:
                info_code = CI_CARRIER_MESSAGE;
                break;
            case kcsiUnknown:
                info_code = CI_UNKNOWN;
                break;
            default:
                DBG(FUNC, PVT_FMT(_target, "got an unknown call answer info '%d', ignoring...") % e->AddInfo);
                break;
        }

        if (info_code != -1)
        {
            if (callGSM()->_call_info_report)
            {
                //TODO: HOW WE TREAT THAT 
                // make the channel export this 
                setAnswerInfo(info_code);
            }

            if (callGSM()->_call_info_drop & info_code)
            {
                command(KHOMP_LOG, CM_SEND_TO_MODEM, "ATH");
            }
        }
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target,"(GSM) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }

    DBG(FUNC, PVT_FMT(_target, "(GSM) r"));
    return true;
}

/*
bool BoardGSM::KhompPvtGSM::onDtmfDetected(K3L_EVENT *e)
{
    bool ret = KhompPvt::onDtmfDetected(e);

    try
    {
        ScopedPvtLock lock(this);
        

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target, "(GSM) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }
    return ret;
}

bool BoardGSM::KhompPvtGSM::onNewCall(K3L_EVENT *e)
{
    DBG(FUNC,PVT_FMT(_target,"(GSM) c"));
    
    try
    {
        ScopedPvtLock lock(this);

        bool ret = KhompPvtGSM::onNewCall(e); 

    }
    catch(ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(target(), "(GSM) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }

    DBG(FUNC, PVT_FMT(_target, "(GSM) r"));

    return ret;
}
*/



bool BoardGSM::KhompPvtGSM::onCallFail(K3L_EVENT *e)
{
    bool ret = true; 
    try
    {
        ScopedPvtLock lock(this);

        //K::internal::gsm_cleanup_and_restart(pvt, owner_nr, true);
        setHangupCause(causeFromCallFail(e->AddInfo), true);

        ret = KhompPvt::onCallFail(e);

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(target(), "unable to lock %s!") % err._msg.c_str() );
        return false;
    }

    return ret;
}

bool BoardGSM::KhompPvtGSM::onDisconnect(K3L_EVENT *e)
{
    bool ret = true;
    try
    {
        ScopedPvtLock lock(this);

        //gsm_cleanup_and_restart(pvt, (int)evt.gsm_call_ref);

        ret = KhompPvt::onDisconnect(e);

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(target(), "(GSM) unable to lock %s!") % err._msg.c_str() );
        return false;
    }

    return ret;
}

int BoardGSM::KhompPvtGSM::makeCall(std::string params)
{
    DBG(FUNC,PVT_FMT(_target, "(GSM) c"));
    
    if(callGSM()->_call_info_drop == 0 && !callGSM()->_call_info_report)
    {
        command(KHOMP_LOG, CM_DISABLE_CALL_ANSWER_INFO);
    }

    if(!_call->_orig_addr.compare("restricted"))
        params += " orig_addr=\"restricted\"";

    int ret = KhompPvt::makeCall(params);

    if(ret != ksSuccess)
    {
        LOG(ERROR, PVT_FMT(target(), "Fail on make call"));
    }

    call()->_cleanup_upon_hangup = (ret == ksInvalidParams || ret == ksInvalidState);

    DBG(FUNC,PVT_FMT(_target, "(GSM) r"));
    return ret;
}

bool BoardGSM::KhompPvtGSM::doChannelAnswer(CommandRequest &cmd)
{
    DBG(FUNC, PVT_FMT(_target, "(GSM) c"));

    bool ret = true;

    try
    {
        ScopedPvtLock lock(this);

        command(KHOMP_LOG, CM_CONNECT);

        ret = KhompPvt::doChannelAnswer(cmd);

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target,"(GSM) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }

    DBG(FUNC, PVT_FMT(_target, "(GSM) r"));

    return ret;
}

bool BoardGSM::KhompPvtGSM::doChannelHangup(CommandRequest &cmd)
{
    DBG(FUNC, PVT_FMT(_target, "(GSM) c"));

    bool ret = true;

    try
    {
        ScopedPvtLock lock(this);

        int owner_nr = 0;

        command(KHOMP_LOG, CM_DISCONNECT, 
                STG(FMT("gsm_call_ref=%d") % (int)owner_nr).c_str());


        //ret = KhompPvt::doChannelHangup(cmd);

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target,"(GSM) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }

    DBG(FUNC, PVT_FMT(_target, "(GSM) r"));

    return ret;
}

bool BoardGSM::KhompPvtGSM::application(ApplicationType type, switch_core_session_t * session, const char *data)
{
    switch(type)
    {
        case SMS_CHECK:
            return true;
        case SMS_SEND:
            return _sms->sendSMS(session, data);
        case SELECT_SIM_CARD:
            return selectSimCard(data);
        default:
            return KhompPvt::application(type, session, data);
    }

    return true;
}

bool BoardGSM::KhompPvtGSM::setupConnection()
{
    if(!call()->_flags.check(Kflags::IS_INCOMING) && !call()->_flags.check(Kflags::IS_OUTGOING))
    {
        DBG(FUNC,PVT_FMT(_target, "Channel already disconnected"));
        return false;
    }

    bool res_out_of_band_dtmf = (call()->_var_dtmf_state == T_UNKNOWN ?
        Opt::_options._suppression_delay() && Opt::_options._out_of_band_dtmfs() : (call()->_var_dtmf_state == T_TRUE));

    bool res_echo_cancellator = (call()->_var_echo_state == T_UNKNOWN ?
        Opt::_options._echo_canceller() : (call()->_var_echo_state == T_TRUE));


    bool res_auto_gain_cntrol = (call()->_var_gain_state == T_UNKNOWN ?
        Opt::_options._auto_gain_control() : (call()->_var_gain_state == T_TRUE));


    if (!call()->_flags.check(Kflags::REALLY_CONNECTED))
    {
        obtainRX(res_out_of_band_dtmf);

        /* esvazia buffers de leitura/escrita */
        cleanupBuffers();

        if (!call()->_flags.check(Kflags::KEEP_DTMF_SUPPRESSION))
            dtmfSuppression(res_out_of_band_dtmf);

        if (!call()->_flags.check(Kflags::KEEP_ECHO_CANCELLATION))
            echoCancellation(res_echo_cancellator);

        if (!call()->_flags.check(Kflags::KEEP_AUTO_GAIN_CONTROL))
            autoGainControl(res_auto_gain_cntrol);

        startListen(false);

        startStream();

        DBG(FUNC, PVT_FMT(_target, "(GSM) Audio callbacks initialized successfully"));
    }

    return Board::KhompPvt::setupConnection();
}

bool BoardGSM::KhompPvtGSM::indicateBusyUnlocked(int cause, bool sent_signaling)
{
    DBG(FUNC, PVT_FMT(_target, "(GSM) c"));

    if(!KhompPvt::indicateBusyUnlocked(cause, sent_signaling))
    {
        DBG(FUNC, PVT_FMT(_target, "(GSM) r (false)"));
        return false;
    }

    if(call()->_flags.check(Kflags::IS_INCOMING))
    {
        if(!call()->_flags.check(Kflags::CONNECTED) && !sent_signaling)
        {
            /* we are talking about branches, not trunks */
            command(KHOMP_LOG, CM_DISCONNECT);
        }
        else
        {
            /* already connected or sent signaling... */
            mixer(KHOMP_LOG, 1, kmsGenerator, kmtBusy);
        }
    }
    else if(call()->_flags.check(Kflags::IS_OUTGOING))
    {
        /* already connected or sent signaling... */
        mixer(KHOMP_LOG, 1, kmsGenerator, kmtBusy);
    }

    DBG(FUNC,PVT_FMT(_target, "(GSM) r"));
    
    return true; 
}

void BoardGSM::KhompPvtGSM::reportFailToReceive(int fail_code)
{
    KhompPvt::reportFailToReceive(fail_code);

    command(KHOMP_LOG, CM_CONNECT);

    /* K3L may fail depending on its configuration (GsmModem) */
    if (!command(KHOMP_LOG, CM_DISCONNECT))
    {
        int owner_nr = 0;
        command(KHOMP_LOG, CM_DISCONNECT,
                STG(FMT("gsm_call_ref=\"%d\"") % owner_nr).c_str());
    }

}

int BoardGSM::KhompPvtGSM::causeFromCallFail(int fail)
{
    int switch_cause = SWITCH_CAUSE_USER_BUSY;

    if (fail <= 127) 
        switch_cause = fail;
    else 
        switch_cause = SWITCH_CAUSE_INTERWORKING;

    return switch_cause;
}

int BoardGSM::KhompPvtGSM::callFailFromCause(int cause)
{
    int k3l_fail = -1; // default

    if (cause <= 127) 
        k3l_fail = cause;
    else
        k3l_fail = kgccInterworking; 

    return k3l_fail;
}

bool BoardGSM::KhompPvtGSM::validContexts(
        MatchExtension::ContextListType & contexts, std::string extra_context)
{
    DBG(FUNC,PVT_FMT(_target,"(GSM) c"));

    if(!_group_context.empty())
    {
        contexts.push_back(_group_context);
    }

    if (!extra_context.empty())
    {    
        if (!_group_context.empty())
        {    
            std::string pvt_context(_group_context);
            pvt_context += "-"; 
            pvt_context += extra_context;
            contexts.push_back(pvt_context);
        }    

        if (!Opt::_options._context_gsm_call().empty())
        {    
            std::string context(Opt::_options._context_gsm_call());
            context += "-"; 
            context += extra_context;
            contexts.push_back(_group_context);
        }

        if (!Opt::_options._context2_gsm_call().empty())
        {    
            std::string context(Opt::_options._context2_gsm_call());
            context += "-"; 
            context += extra_context;
            contexts.push_back(_group_context);
        }    
    }  

    contexts.push_back(Opt::_options._context_gsm_call());
    contexts.push_back(Opt::_options._context2_gsm_call());

    for (MatchExtension::ContextListType::iterator i = contexts.begin(); i != contexts.end(); i++) 
        replaceTemplate((*i), "CC", _target.object);

    bool ret = Board::KhompPvt::validContexts(contexts,extra_context);

    DBG(FUNC,PVT_FMT(_target,"(GSM) r"));

    return ret;
}

