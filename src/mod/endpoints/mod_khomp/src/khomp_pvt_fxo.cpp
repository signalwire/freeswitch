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

#include "khomp_pvt_fxo.h"
#include "lock.h"
#include "logger.h"

int BoardFXO::KhompPvtFXO::makeCall(std::string params)
{
    DBG(FUNC,PVT_FMT(_target, "(FXO) c"));

    int ret = ksSuccess;

    /* we always have audio */
    call()->_flags.set(Kflags::HAS_PRE_AUDIO);
    
    if(callFXO()->_call_info_drop == 0 && !callFXO()->_call_info_report)
    {
        command(KHOMP_LOG, CM_DISABLE_CALL_ANSWER_INFO);
    }

    if (!callFXO()->_pre_digits.empty())
    {
        /* Seize the line at local PABX. */

        callFXO()->_flags.set(Kflags::CALL_WAIT_SEIZE);

        if (!command(KHOMP_LOG, CM_SEIZE, call()->_orig_addr.c_str()))
            return ksFail;            

        int timeout = 150;

        if(!loopWhileFlagTimed(Kflags::CALL_WAIT_SEIZE, timeout))
            return ksFail;

        if (callFXO()->_flags.check(Kflags::CALL_WAIT_SEIZE) || (timeout <= 0))
            return ksFail;

        /* Grab line from local PABX. */

        callFXO()->_flags.set(Kflags::WAIT_SEND_DTMF);

        if (!command(KHOMP_LOG, CM_DIAL_DTMF, callFXO()->_pre_digits.c_str()))
            return ksFail;            
        
        if(!loopWhileFlagTimed(Kflags::WAIT_SEND_DTMF, timeout))
            return ksFail;

        if (callFXO()->_flags.check(Kflags::WAIT_SEND_DTMF) || (timeout <= 0))
            return ksFail;

        /* Seize line from public central (works because the     *
        * continuous cadence is always detected by the k3l api. */

        callFXO()->_flags.set(Kflags::CALL_WAIT_SEIZE);

        if (!command(KHOMP_LOG, CM_SEIZE, call()->_orig_addr.c_str()))
            return ksFail;

        if(!loopWhileFlagTimed(Kflags::CALL_WAIT_SEIZE, timeout))
            return ksFail;

        if (callFXO()->_flags.check(Kflags::CALL_WAIT_SEIZE) || (timeout <= 0))
            return ksFail;

        /* we want the audio as soon as dialing ends */
        callFXO()->_flags.set(Kflags::EARLY_RINGBACK);
    
        if (!command(KHOMP_LOG, CM_DIAL_DTMF, call()->_dest_addr.c_str()))
            return ksFail;
    }
    else
    {
        /* we want the audio as soon as dialing ends */
        callFXO()->_flags.set(Kflags::EARLY_RINGBACK);


        if(!call()->_orig_addr.empty())
            params += STG(FMT(" orig_addr=\"%s\"") % _call->_orig_addr);


        ret = KhompPvt::makeCall(params);

        if(ret != ksSuccess)
        {
            LOG(ERROR, PVT_FMT(target(), "Fail on make call"));
        }

        call()->_cleanup_upon_hangup = (ret == ksInvalidParams);
    }

    DBG(FUNC,PVT_FMT(_target, "(FXO) r"));
    return ret;
}

bool BoardFXO::KhompPvtFXO::doChannelAnswer(CommandRequest &cmd)
{
    DBG(FUNC, PVT_FMT(_target, "(FXO) c"));

    bool ret = true;

    try
    {
        ScopedPvtLock lock(this);

        // is this a collect call?
        bool has_recv_collect_call = _call->_collect_call;

        if(has_recv_collect_call)
            DBG(FUNC, PVT_FMT(target(), "receive a collect call"));

        if(call()->_flags.check(Kflags::DROP_COLLECT))
            DBG(FUNC, PVT_FMT(target(), "flag DROP_COLLECT == true"));

        // do we have to drop collect calls?
        bool has_drop_collect_call = call()->_flags.check(Kflags::DROP_COLLECT);

        // do we have to drop THIS call?
        bool do_drop_call = has_drop_collect_call && has_recv_collect_call;

        if(!do_drop_call)
        {
            command(KHOMP_LOG, CM_CONNECT);
        }

        if(has_drop_collect_call)
        {
            if(has_recv_collect_call)
            {
                usleep(75000);

                DBG(FUNC, PVT_FMT(target(), "disconnecting collect call doChannelAnswer FXO"));
                command(KHOMP_LOG,CM_DISCONNECT);

                // thou shalt not talk anymore!
                stopListen();
                stopStream();
            }
            else
            {
                DBG(FUNC, PVT_FMT(target(), "dropping collect call at doChannelAnswer FXO"));
                command(KHOMP_LOG, CM_DROP_COLLECT_CALL);
            }
        }

        ret = KhompPvt::doChannelAnswer(cmd);

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target,"(FXO) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }

    DBG(FUNC, PVT_FMT(_target, "(FXO) r"));

    return ret;
}

bool BoardFXO::KhompPvtFXO::onNewCall(K3L_EVENT *e)
{
    DBG(FUNC,PVT_FMT(_target,"(FXO) c"));
   
    bool ret = true;

    try
    {
        ScopedPvtLock lock(this);

        /* we always have audio */
        call()->_flags.set(Kflags::HAS_PRE_AUDIO);

        ret = KhompPvt::onNewCall(e);

        if(!ret)
            return false;

        startListen();
        startStream();

    }
    catch(ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(target(), "(FXO) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }

    DBG(FUNC, PVT_FMT(_target, "(FXO) r"));

    return ret;
}

bool BoardFXO::KhompPvtFXO::onDisconnect(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_target, "(FXO) c"));

    bool ret = true;

    try
    {
        ScopedPvtLock lock(this);

        command(KHOMP_LOG, CM_DISCONNECT);

        ret = KhompPvt::onDisconnect(e);

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(target(), "(FXO) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }

    DBG(FUNC, PVT_FMT(_target, "(FXO) r"));

    return ret;
}

bool BoardFXO::KhompPvtFXO::onChannelRelease(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_target, "(FXO) c"));

    bool ret = true;

    try
    {
        ScopedPvtLock lock(this);

        if (call()->_flags.check(Kflags::FAX_SENDING))
        {
            DBG(FUNC, PVT_FMT(_target, "stopping fax tx"));
            _fax->stopFaxTX();
        }
        else if (call()->_flags.check(Kflags::FAX_RECEIVING))
        {
            DBG(FUNC, PVT_FMT(_target, "stopping fax rx"));
            _fax->stopFaxRX();
        }

        command(KHOMP_LOG, CM_ENABLE_CALL_ANSWER_INFO);
   
        ret = KhompPvt::onChannelRelease(e);
    }
    catch(ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(target(), "(FXO) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }
    
    DBG(FUNC, PVT_FMT(_target, "(FXO) r"));   
    return ret;
}

bool BoardFXO::KhompPvtFXO::onCallSuccess(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_target, "(FXO) c"));

    bool ret = true;

    try
    {
        ScopedPvtLock lock(this);

        ret = KhompPvt::onCallSuccess(e);

        if (call()->_pre_answer)
        {
            dtmfSuppression(Opt::_options._out_of_band_dtmfs() && !call()->_flags.check(Kflags::FAX_DETECTED));

            startListen();
            startStream();
            switch_channel_mark_pre_answered(getFSChannel());
        }
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target, "(FXO) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }
    catch (Board::KhompPvt::InvalidSwitchChannel & err)
    {
        LOG(ERROR, PVT_FMT(target(), "(FXO) r (%s)") % err._msg.c_str() );
        return false;
    }
        
    DBG(FUNC, PVT_FMT(_target, "(FXO) r"));

    return ret;
}

bool BoardFXO::KhompPvtFXO::onCallFail(K3L_EVENT *e)
{
    bool ret = true; 
    try
    {
        ScopedPvtLock lock(this);

        command(KHOMP_LOG, CM_DISCONNECT);

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

bool BoardFXO::KhompPvtFXO::onAudioStatus(K3L_EVENT *e)
{
    DBG(STRM, PVT_FMT(_target, "(FXO) c"));

    try
    {
        //ScopedPvtLock lock(this);

        if(e->AddInfo == kmtFax)
        {
            DBG(STRM, PVT_FMT(_target, "Fax detected"));

            /* hadn't we did this already? */
            bool already_detected = call()->_flags.check(Kflags::FAX_DETECTED);            

            time_t time_was = call()->_call_statistics->_base_time;
            time_t time_now = time(NULL);

            bool detection_timeout = (time_now > (time_was + (time_t) (Opt::_options._fax_adjustment_timeout())));
            
            BEGIN_CONTEXT
            {
                ScopedPvtLock lock(this);

                /* already adjusted? do not adjust again. */
                if (already_detected || detection_timeout)
                    break;

                if (callFXO()->_call_info_drop != 0 || callFXO()->_call_info_report)
                {
                    /* we did not detected fax yet, send answer info! */
                    setAnswerInfo(Board::KhompPvt::CI_FAX);

                    if (callFXO()->_call_info_drop & Board::KhompPvt::CI_FAX)
                    {
                        /* fastest way to force a disconnection */
                        command(KHOMP_LOG,CM_DISCONNECT);//,SCE_HIDE);
                    }
                }

                if (Opt::_options._auto_fax_adjustment())
                {
                    DBG(FUNC, PVT_FMT(_target, "communication will be adjusted for fax!"));
                    _fax->adjustForFax();
                }
            }
            END_CONTEXT

            if (!already_detected)
            {
                ScopedPvtLock lock(this);

                /* adjust the flag */
                call()->_flags.set(Kflags::FAX_DETECTED);
            }
        }
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target, "(FXO) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }

    bool ret = KhompPvt::onAudioStatus(e);

    DBG(STRM, PVT_FMT(_target, "(FXO) r"));

    return ret;
}

bool BoardFXO::KhompPvtFXO::onSeizeSuccess(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_target, "(FXO) c"));

    try
    {
        ScopedPvtLock lock(this);

        callFXO()->_flags.clear(Kflags::CALL_WAIT_SEIZE);

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target, "(FXO) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }

    DBG(FUNC, PVT_FMT(_target, "(FXO) r"));

    return true;
}

bool BoardFXO::KhompPvtFXO::onDtmfDetected(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_target, "(FXO) c"));

    bool ret = true;

    try
    {
        ScopedPvtLock lock(this);

        if (callFXO()->_flags.check(Kflags::WAIT_SEND_DTMF) ||
                callFXO()->_flags.check(Kflags::CALL_WAIT_SEIZE))
        {
            /* waiting digit or seize means DEADLOCK if we try to 
             *  queue something below. */
            DBG(FUNC, PVT_FMT(_target, "not queueing dtmf, waiting stuff!"));
            return true;
        }

        ret = KhompPvt::onDtmfDetected(e);
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target, "(FXO) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }

    DBG(FUNC, PVT_FMT(_target, "(FXO) r"));

    return ret;
}

bool BoardFXO::KhompPvtFXO::onDtmfSendFinish(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_target, "(FXO) c"));

    bool ret = true;

    try
    {
        ScopedPvtLock lock(this);

        ret = KhompPvt::onDtmfSendFinish(e);

        if (callFXO()->_flags.check(Kflags::EARLY_RINGBACK))
        {
            callFXO()->_flags.clear(Kflags::EARLY_RINGBACK);

            /* start grabbing */
            startListen();

            /* activate resources early... */
            bool fax_detected = callFXO()->_flags.check(Kflags::FAX_DETECTED);

            bool res_out_of_band_dtmf = Opt::_options._suppression_delay() && Opt::_options._out_of_band_dtmfs() && !fax_detected;
            bool res_echo_cancellator = Opt::_options._echo_canceller() && !fax_detected;
            bool res_auto_gain_cntrol = Opt::_options._auto_gain_control() && !fax_detected;

            if (!call()->_flags.check(Kflags::KEEP_DTMF_SUPPRESSION))
                dtmfSuppression(res_out_of_band_dtmf);

            if (!call()->_flags.check(Kflags::KEEP_ECHO_CANCELLATION))
                echoCancellation(res_echo_cancellator);

            if (!call()->_flags.check(Kflags::KEEP_AUTO_GAIN_CONTROL))
                autoGainControl(res_auto_gain_cntrol);

            /* start sending audio if wanted so */
            if (Opt::_options._fxo_send_pre_audio())
                startStream();

            //TODO: Verificar isso aqui
            if (call()->_pre_answer)
            {
                /* tell the user we are answered! */
                switch_channel_mark_answered(getFSChannel());
                //pvt->signal_state(AST_CONTROL_ANSWER);
            }
            else
            {
                /* are we ringing, now? lets try this way! */
                switch_channel_mark_ring_ready(getFSChannel());
                //pvt->signal_state(AST_CONTROL_RINGING);
            }
        }
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target, "(FXO) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }
    catch(Board::KhompPvt::InvalidSwitchChannel & err)
    {
        LOG(ERROR, PVT_FMT(_target, "(FXO) r (no valid channel: %s)") % err._msg.c_str());
        return false;
    }

    DBG(FUNC, PVT_FMT(_target, "(FXO) r"));

    return ret;
}

bool BoardFXO::KhompPvtFXO::onCallAnswerInfo(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_target, "(FXO) c"));
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
            if (callFXO()->_call_info_report)
            {
                //TODO: HOW WE TREAT THAT 
                // make the channel export this 
                setAnswerInfo(info_code);
            }

            if (callFXO()->_call_info_drop & info_code)
            {
                command(KHOMP_LOG,CM_DISCONNECT);
            }
        }
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target,"(FXO) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }

    DBG(FUNC, PVT_FMT(_target, "(FXO) r"));
    return true;
}

bool BoardFXO::KhompPvtFXO::application(ApplicationType type, switch_core_session_t * session, const char *data)
{
    switch(type)
    {
        case FAX_ADJUST:
            return _fax->adjustForFax();
        case FAX_SEND:
            return _fax->sendFax(session, data);
        case FAX_RECEIVE:
            return _fax->receiveFax(session, data);
        case USER_TRANSFER:
            return _transfer->userTransfer(session, data);
        default:
            return KhompPvt::application(type, session, data);
    }

    return true;
}

bool BoardFXO::KhompPvtFXO::setupConnection()
{
    if(!call()->_flags.check(Kflags::IS_INCOMING) && !call()->_flags.check(Kflags::IS_OUTGOING))
    {
        DBG(FUNC,PVT_FMT(_target, "Channel already disconnected"));
        return false;
    }

    callFXO()->_flags.clear(Kflags::CALL_WAIT_SEIZE);
    callFXO()->_flags.clear(Kflags::WAIT_SEND_DTMF);

    /* if received some disconnect from 'drop collect call'
       feature of some pbx, then leave the call rock and rolling */
    //Board::board(_target.device)->_timers.del(callFXO()->_idx_disconnect);

    bool fax_detected = callFXO()->_flags.check(Kflags::FAX_DETECTED) || (callFXO()->_var_fax_adjust == T_TRUE);

    bool res_out_of_band_dtmf = (call()->_var_dtmf_state == T_UNKNOWN || fax_detected ?
        Opt::_options._suppression_delay() && Opt::_options._out_of_band_dtmfs() && !fax_detected : (call()->_var_dtmf_state == T_TRUE));

    bool res_echo_cancellator = (call()->_var_echo_state == T_UNKNOWN || fax_detected ?
        Opt::_options._echo_canceller() && !fax_detected : (call()->_var_echo_state == T_TRUE));

    bool res_auto_gain_cntrol = (call()->_var_gain_state == T_UNKNOWN || fax_detected ?
        Opt::_options._auto_gain_control() && !fax_detected : (call()->_var_gain_state == T_TRUE));

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

        DBG(FUNC, PVT_FMT(_target, "(FXO) Audio callbacks initialized successfully"));
    }

    return KhompPvt::setupConnection();
}

bool BoardFXO::KhompPvtFXO::autoGainControl(bool enable)
{
    bool ret = KhompPvt::autoGainControl(enable);
    
    /* enable this AGC also, can be very useful */
    ret &= command(KHOMP_LOG, (enable ? CM_ENABLE_PLAYER_AGC : CM_DISABLE_PLAYER_AGC));
    return ret;
}

void BoardFXO::KhompPvtFXO::setAnswerInfo(int answer_info)
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
        setFSChannelVar("KCallAnswerInfo", value);
    }
    catch(Board::KhompPvt::InvalidSwitchChannel & err)
    {
        LOG(ERROR,PVT_FMT(_target,"Cannot obtain the channel variable: %s") % err._msg.c_str());
    }
}

bool BoardFXO::KhompPvtFXO::indicateBusyUnlocked(int cause, bool sent_signaling)
{
    DBG(FUNC, PVT_FMT(_target, "(FXO) c"));

    if(!KhompPvt::indicateBusyUnlocked(cause, sent_signaling))
    {
        DBG(FUNC, PVT_FMT(_target, "(FXO) r (false)"));
        return false;
    }

    if(call()->_flags.check(Kflags::IS_INCOMING))
    {
        /* already connected or sent signaling... */
        mixer(KHOMP_LOG, 1, kmsGenerator, kmtBusy);
        
        if(!call()->_flags.check(Kflags::CONNECTED) && !sent_signaling)
        {
            /* we are talking about branches, not trunks */ 
            command(KHOMP_LOG, CM_CONNECT);
            callFXO()->_busy_disconnect = Board::board(_target.device)->_timers.add(Opt::_options._fxo_busy_disconnection(), &BoardFXO::KhompPvtFXO::busyDisconnect, this);
        }
    }
    else if(call()->_flags.check(Kflags::IS_OUTGOING))
    {
        /* already connected or sent signaling... */
        mixer(KHOMP_LOG, 1, kmsGenerator, kmtBusy);
    }

    DBG(FUNC,PVT_FMT(_target, "(FXO) r"));
    return true; 
}

void BoardFXO::KhompPvtFXO::busyDisconnect(Board::KhompPvt * pvt)
{
    DBG(FUNC, PVT_FMT(pvt->target(), "Disconnecting FXO"));

    try 
    {   
        ScopedPvtLock lock(pvt);
        pvt->command(KHOMP_LOG, CM_DISCONNECT);
    }   
    catch (...)
    {
        LOG(ERROR, PVT_FMT(pvt->target(), "unable to lock the pvt !"));
    }  
}

void BoardFXO::KhompPvtFXO::reportFailToReceive(int fail_code)
{
    KhompPvt::reportFailToReceive(fail_code);

    command(KHOMP_LOG, CM_CONNECT);
    command(KHOMP_LOG, CM_DISCONNECT);
}

bool BoardFXO::KhompPvtFXO::validContexts(
        MatchExtension::ContextListType & contexts, std::string extra_context)
{
    DBG(FUNC,PVT_FMT(_target,"(FXO) c"));

    if(!_group_context.empty())
    {
        contexts.push_back(_group_context);
    }

    contexts.push_back(Opt::_options._context_fxo());
    contexts.push_back(Opt::_options._context2_fxo());

    for (MatchExtension::ContextListType::iterator i = contexts.begin(); i != contexts.end(); i++)
    {
        replaceTemplate((*i), "CC", _target.object);
    }

    bool ret = Board::KhompPvt::validContexts(contexts,extra_context);

    DBG(FUNC,PVT_FMT(_target,"(FXO) r"));

    return ret;
}

bool BoardFXO::KhompPvtFXO::isOK()
{
    try
    {
        ScopedPvtLock lock(this);

        K3L_CHANNEL_STATUS status;

        if (k3lGetDeviceStatus (_target.device, _target.object + ksoChannel, &status, sizeof (status)) != ksSuccess)
            return false;

        return (status.AddInfo == kfcsEnabled);
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(target(), "unable to lock %s!") % err._msg.c_str() );
    }

    return false;
}

bool BoardFXO::KhompPvtFXO::sendDtmf(std::string digit)
{
    if(_transfer->checkUserXferUnlocked(digit))
    {
        DBG(FUNC, PVT_FMT(target(), "started (or waiting for) an user xfer"));
        return true;
    }

    bool ret = KhompPvt::sendDtmf(callFXO()->_digits_buffer);

    callFXO()->_digits_buffer.clear();

    return ret;
}

