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

#include "khomp_pvt_kxe1.h"
#include "lock.h"
#include "logger.h"


bool BoardE1::KhompPvtE1::isOK()
{
    try
    {
        ScopedPvtLock lock(this);

        K3L_CHANNEL_STATUS status;

        if (k3lGetDeviceStatus (_target.device, _target.object + ksoChannel, &status, sizeof (status)) != ksSuccess)
            return false;

        return   ((status.AddInfo == kecsFree) ||
                (!(status.AddInfo & kecsLocalFail) &&
                 !(status.AddInfo & kecsRemoteLock)));
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(target(), "unable to lock %s!") % err._msg.c_str() );
    }

    return false;
}

bool BoardE1::KhompPvtE1::onChannelRelease(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_target, "(E1) c"));

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

        call()->_flags.clear(Kflags::HAS_PRE_AUDIO);

        command(KHOMP_LOG, CM_ENABLE_CALL_ANSWER_INFO);

        ret = KhompPvt::onChannelRelease(e);

    }
    catch(ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(target(), "(E1) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }

    DBG(FUNC, PVT_FMT(_target, "(E1) r"));   
    return ret;
}

bool BoardE1::KhompPvtE1::onCallSuccess(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_target, "(E1) c"));

    bool ret;

    try
    {
        ScopedPvtLock lock(this);

        ret = KhompPvt::onCallSuccess(e);

        if (call()->_pre_answer)
        {
            dtmfSuppression(Opt::_options._out_of_band_dtmfs()&& !call()->_flags.check(Kflags::FAX_DETECTED));     

            startListen();
            startStream();
            switch_channel_mark_pre_answered(getFSChannel());
        }
        else
        {
            call()->_flags.set(Kflags::GEN_PBX_RING);
            call()->_idx_pbx_ring = Board::board(_target.device)->_timers.add(Opt::_options._ringback_pbx_delay(),
                                             &Board::KhompPvt::pbxRingGen,this, TM_VAL_CALL);
        }

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target, "(E1) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }
    catch (K3LAPITraits::invalid_device & err)
    {
        LOG(ERROR, PVT_FMT(_target, "(E1) r (unable to get device: %d!)") % err.device);
        return false;
    }
    
    DBG(FUNC, PVT_FMT(_target, "(E1) r"));

    return ret;
}

bool BoardE1::KhompPvtE1::onAudioStatus(K3L_EVENT *e)
{
    DBG(STRM, PVT_FMT(_target, "(E1) c"));

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
            
            DBG(STRM, PVT_FMT(_target, "is set? (%s) timeout? (%s)")
                            % (already_detected ? "true" : "false") % (detection_timeout ? "true" : "false"));

            BEGIN_CONTEXT
            {
                ScopedPvtLock lock(this);

                /* already adjusted? do not adjust again. */
                if (already_detected || detection_timeout)
                    break;

                if (callE1()->_call_info_drop != 0 || callE1()->_call_info_report)
                {    
                    /* we did not detected fax yet, send answer info! */
                    setAnswerInfo(Board::KhompPvt::CI_FAX);

                    if (callE1()->_call_info_drop & Board::KhompPvt::CI_FAX)
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
        LOG(ERROR, PVT_FMT(_target, "unable to lock %s!") % err._msg.c_str() );
        return false;
    }

    bool ret = KhompPvt::onAudioStatus(e);
    
    DBG(STRM, PVT_FMT(_target, "(E1) r"));

    return ret;
}

bool BoardE1::KhompPvtE1::onCallAnswerInfo(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_target, "(E1) c"));
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
            if (callE1()->_call_info_report)
            {    
                //TODO: HOW WE TREAT THAT 
                // make the channel export this 
                setAnswerInfo(info_code);
            }    

            if (callE1()->_call_info_drop & info_code)
            {    
                // fastest way to force a disconnection 
                //K::util::sendCmd(pvt->boardid, pvt->objectid, CM_DISCONNECT, true, false);
                command(KHOMP_LOG,CM_DISCONNECT/*,SCE_HIDE ?*/);
            }    
        }    
    }    
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target, "(E1) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }

    DBG(FUNC, PVT_FMT(_target, "(E1) r"));
    return true;
}

bool BoardE1::KhompPvtE1::onDisconnect(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_target, "(E1) c"));

    bool ret = true;

    try
    {
        ScopedPvtLock lock(this);

        if (call()->_flags.check(Kflags::IS_OUTGOING) ||
            call()->_flags.check(Kflags::IS_INCOMING))
        {
            if(Opt::_options._disconnect_delay()== 0)
            {
                DBG(FUNC, PVT_FMT(_target, "queueing disconnecting outgoing channel!"));
                command(KHOMP_LOG, CM_DISCONNECT);       
            }
            else
            {
                callE1()->_idx_disconnect = Board::board(_target.device)->_timers.add(
                    1000 * Opt::_options._disconnect_delay(),&BoardE1::KhompPvtE1::delayedDisconnect,this);
            }
        }
        else
        {
            /* in the case of a disconnect confirm is needed and a call 
            was not started e.g. just after a ev_seizure_start */
            command(KHOMP_LOG, CM_DISCONNECT);
        }
        
        ret = KhompPvt::onDisconnect(e);

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target, "(E1) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }
    catch (K3LAPITraits::invalid_device & err)
    {
        LOG(ERROR, PVT_FMT(_target, "(E1) r (unable to get device: %d!)") % err.device);
        return false;
    }

    DBG(FUNC,PVT_FMT(_target, "(E1) r"));
    return ret;
}

void BoardE1::KhompPvtE1::delayedDisconnect(Board::KhompPvt * pvt)
{
    DBG(FUNC,PVT_FMT(pvt->target(), "Delayed disconnect"));

    try  
    {    
        ScopedPvtLock lock(pvt);

        DBG(FUNC, PVT_FMT(pvt->target(), "queueing disconnecting outgoing channel after delaying!"));

        pvt->command(KHOMP_LOG,CM_DISCONNECT);
        pvt->cleanup(CLN_HARD);
    }        
    catch (...)
    {
        LOG(ERROR, PVT_FMT(pvt->target(), "r (unable to lock the pvt !)"));
    }       
}

bool BoardE1::onLinkStatus(K3L_EVENT *e)
{
    DBG(FUNC, D("Link %02d on board %02d changed") % e->AddInfo % e->DeviceId);

    /* Fire a custom event about this */
    /*
    switch_event_t * event;
    if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, KHOMP_EVENT_MAINT) == SWITCH_STATUS_SUCCESS)
    {
        //khomp_add_event_board_data(e->AddInfo, event);
        Board::khomp_add_event_board_data(K3LAPI::target(Globals::k3lapi, K3LAPI::target::LINK, e->DeviceId, e->AddInfo), event);

        switch_event_add_header(event, SWITCH_STACK_BOTTOM, "EV_LINK_STATUS", "%d", e->AddInfo);
        switch_event_fire(&event);
    }
    */

    return true;
}

bool BoardE1::KhompPvtISDN::onSyncUserInformation(K3L_EVENT *e)
{
    DBG(FUNC,PVT_FMT(_target, "Synchronizing"));

    if(callISDN()->_uui_extended)
    {
        KUserInformationEx * info = (KUserInformationEx *) (((char*)e) + sizeof(K3L_EVENT));
        callISDN()->_uui_descriptor = (long int) info->ProtocolDescriptor;
        
        /* clean string */
        callISDN()->_uui_information.clear();

        if (info->UserInfoLength)
        {    
            /* append to a clean string */
            for (unsigned int i = 0; i < info->UserInfoLength; ++i) 
                callISDN()->_uui_information += STG(FMT("%02hhx") % ((unsigned char) info->UserInfo[i]));
        }    
    }
    else
    {
        KUserInformation * info = (KUserInformation *) (((char*)e) + sizeof(K3L_EVENT));
        callISDN()->_uui_descriptor = info->ProtocolDescriptor;
        
        /* clean string */
        callISDN()->_uui_information.clear();

        if (info->UserInfoLength)
        {    
            for (unsigned int i = 0; i < info->UserInfoLength; ++i) 
                callISDN()->_uui_information += STG(FMT("%02hhx") % ((unsigned char) info->UserInfo[i]));
        }    
    }

    return true;
}

bool BoardE1::KhompPvtISDN::onIsdnProgressIndicator(K3L_EVENT *e)
{
    //TODO: Do we need return something ?
    try
    {
        ScopedPvtLock lock(this);

        switch (e->AddInfo)
        {
            case kq931pTonesMaybeAvailable:
            case kq931pTonesAvailable:
                if (!call()->_is_progress_sent)
                {
                    call()->_is_progress_sent = true;

                    //Sinaliza para o Freeswitch PROGRESS
                    DBG(FUNC, PVT_FMT(_target, "Pre answer"));

                    //pvt->signal_state(SWITCH_CONTROL_PROGRESS);
                    //switch_channel_pre_answer(channel);
                    switch_channel_mark_pre_answered(getFSChannel());

                }
                break;
            case kq931pDestinationIsNonIsdn:
            case kq931pOriginationIsNonIsdn:
            case kq931pCallReturnedToIsdn:
            default:
                break;
        }
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(target(), "unable to lock %s!") % err._msg.c_str() );
        return false;
    }
    catch(Board::KhompPvt::InvalidSwitchChannel & err)
    {
        LOG(ERROR,PVT_FMT(_target, "No valid channel: %s") % err._msg.c_str());
        return false;
    }

    return true;
}

bool BoardE1::KhompPvtISDN::onNewCall(K3L_EVENT *e)
{
    DBG(FUNC,PVT_FMT(_target, "(ISDN) c"));   

    bool isdn_reverse_charge = false;
    std::string isdn_reverse_charge_str;
    bool ret;
   
    try
    {
        callISDN()->_isdn_orig_type_of_number = Globals::k3lapi.get_param(e, "isdn_orig_type_of_number"); 
        callISDN()->_isdn_orig_numbering_plan = Globals::k3lapi.get_param(e, "isdn_orig_numbering_plan"); 
        callISDN()->_isdn_dest_type_of_number = Globals::k3lapi.get_param(e, "isdn_dest_type_of_number"); 
        callISDN()->_isdn_dest_numbering_plan = Globals::k3lapi.get_param(e, "isdn_dest_numbering_plan"); 
        callISDN()->_isdn_orig_presentation   = Globals::k3lapi.get_param(e, "isdn_orig_presentation"); 
        isdn_reverse_charge_str = Globals::k3lapi.get_param(e, "isdn_reverse_charge");
        isdn_reverse_charge = Strings::toboolean(isdn_reverse_charge_str);
    }
    catch(K3LAPI::get_param_failed & err)
    {
        LOG(WARNING, PVT_FMT(_target, "maybe the parameter is not sent (%s)'") % err.name.c_str());
    }
    catch (Strings::invalid_value & err)
    {
        LOG(ERROR, PVT_FMT(_target, "unable to get param '%s'") % err.value().c_str());
    }

    try
    {
        ScopedPvtLock lock(this);

        if(session())
        {
            bool pvt_locked = true;
            bool is_ok = false;
        
            DBG(FUNC, PVT_FMT(_target, "Session has not been destroyed yet, waiting for khompDestroy"));

            for(unsigned int sleeps = 0; sleeps < 20; sleeps++)
            {
                /* unlock our pvt struct */
                if(pvt_locked)
                {
                    _mutex.unlock();
                    pvt_locked = false;
                }

                /* wait a little while (100ms is good?) */
                usleep (100000);

                /* re-lock pvt struct */
                switch (_mutex.lock())
                {
                case SimpleLock::ISINUSE:
                case SimpleLock::FAILURE:
                    LOG(ERROR, PVT_FMT(_target, "unable to lock pvt_mutex, trying again."));
                    sched_yield();
                    continue;

                default:
                    break;
                }

                pvt_locked = true;
            
                if(!session())
                {
                    is_ok = true;
                    break;
                }
            }

            if(is_ok)
            {
                DBG(FUNC, PVT_FMT(_target, "Session destroyed properly"));
            }
            else
            {
                LOG(ERROR, PVT_FMT(target(), "(ISDN) r (Session was not destroyed, stopping to wait)"));
                return false;
            }
        }

        if(isdn_reverse_charge)
        { 
            call()->_collect_call = true;
        }

        ret = KhompPvtE1::onNewCall(e); 
    }
    catch(ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(target(), "(ISDN) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }

    DBG(FUNC, PVT_FMT(_target, "(ISDN) r"));   

    return ret;
}

bool BoardE1::KhompPvtISDN::onCallSuccess(K3L_EVENT *e)
{
    bool ret = true;

    try
    {
        ScopedPvtLock lock(this);

        if(e->AddInfo > 0)
        {
            callISDN()->_isdn_cause = e->AddInfo;
            setFSChannelVar("KISDNGotCause", Verbose::isdnCause((KQ931Cause)callISDN()->_isdn_cause).c_str());
        }

        ret = KhompPvtE1::onCallSuccess(e);

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(target(), "unable to lock %s!") % err._msg.c_str() );
        return false;
    }
    catch(Board::KhompPvt::InvalidSwitchChannel & err)
    {
        LOG(ERROR,PVT_FMT(_target, "%s") % err._msg.c_str());
        return false;
    }

    return ret;
}

bool BoardE1::KhompPvtISDN::onCallFail(K3L_EVENT *e)
{
    bool ret = true;

    try
    {
        ScopedPvtLock lock(this);

        if(e->AddInfo > 0)
        {
            callISDN()->_isdn_cause = e->AddInfo;
            setFSChannelVar("KISDNGotCause", Verbose::isdnCause((KQ931Cause)callISDN()->_isdn_cause).c_str());
        }

        setHangupCause(causeFromCallFail(e->AddInfo),true);

        ret = KhompPvtE1::onCallFail(e);

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(target(), "unable to lock %s!") % err._msg.c_str() );
        return false;
    }
    catch(Board::KhompPvt::InvalidSwitchChannel & err)
    {
        LOG(ERROR,PVT_FMT(_target, "%s") % err._msg.c_str());
        return false;
    }

    return ret;
}

RingbackDefs::RingbackStType BoardE1::KhompPvtISDN::sendRingBackStatus(int rb_value)
{
    DBG(FUNC, PVT_FMT(target(), "this is the rdsi ringback procedure"));


    std::string cause = (rb_value == -1 ? "" : STG(FMT("isdn_cause=\"%d\"") % rb_value));
    return (command(KHOMP_LOG, CM_RINGBACK, cause.c_str()) ?
            RingbackDefs::RBST_SUCCESS : RingbackDefs::RBST_FAILURE);
}

bool BoardE1::KhompPvtISDN::sendPreAudio(int rb_value)
{
    if(!KhompPvtE1::sendPreAudio(rb_value))
        return false;


    DBG(FUNC,PVT_FMT(_target, "doing the ISDN pre_connect"));   

    if (call()->_flags.check(Kflags::HAS_PRE_AUDIO))
    {
        DBG(FUNC, PVT_FMT(target(), "already pre_connect"));
        return true;
    }
    else
    {
        bool result = command(KHOMP_LOG, CM_PRE_CONNECT);

        if (result)
            call()->_flags.set(Kflags::HAS_PRE_AUDIO);

        return result;
    }
}

bool BoardE1::KhompPvtISDN::application(ApplicationType type, switch_core_session_t * session, const char *data)
{
    switch(type)
    {
        case USER_TRANSFER:
            return _transfer->userTransfer(session, data);
        default:
            return KhompPvtE1::application(type, session, data);
    }

    return true;
}

bool BoardE1::KhompPvtISDN::sendDtmf(std::string digit)
{
    if(_transfer->checkUserXferUnlocked(digit))
    {
        DBG(FUNC, PVT_FMT(target(), "started (or waiting for) an user xfer"));
        return true;
    }

    bool ret = KhompPvtE1::sendDtmf(callISDN()->_digits_buffer);
        
    callISDN()->_digits_buffer.clear();

    return ret;
}

int BoardE1::KhompPvtE1::makeCall(std::string params)
{
    if(callE1()->_call_info_drop == 0 && !callE1()->_call_info_report)
    {
        command(KHOMP_LOG, CM_DISABLE_CALL_ANSWER_INFO);
    }

    if(!_call->_orig_addr.empty())
        params += STG(FMT(" orig_addr=\"%s\"") % _call->_orig_addr);

    int ret = KhompPvt::makeCall(params);

    if(ret == ksSuccess)
    {
        startListen();
    }
    else
    {
        LOG(ERROR, PVT_FMT(target(), "Fail on make call"));
    }   

    return ret;
}

bool BoardE1::KhompPvtE1::indicateBusyUnlocked(int cause, bool sent_signaling)
{
    DBG(FUNC, PVT_FMT(_target, "(E1) c"));

    if(!KhompPvt::indicateBusyUnlocked(cause, sent_signaling))
    {
        DBG(FUNC, PVT_FMT(_target, "(E1) r (false)"));
        return false;
    }

    if(call()->_flags.check(Kflags::IS_INCOMING))
    {
        if(!call()->_flags.check(Kflags::CONNECTED) && !sent_signaling)
        {
            if(!call()->_flags.check(Kflags::HAS_PRE_AUDIO))
            {
                int rb_value = callFailFromCause(call()->_hangup_cause);
                DBG(FUNC, PVT_FMT(target(), "sending the busy status"));

                if (sendRingBackStatus(rb_value) == RingbackDefs::RBST_UNSUPPORTED)
                {
                    DBG(FUNC, PVT_FMT(target(), "falling back to audio indication!"));
                    /* stop the line audio */
                    stopStream();

                    /* just pre connect, no ringback */
                    if (!sendPreAudio())
                        DBG(FUNC, PVT_FMT(target(), "everything else failed, just sending audio indication..."));

                    /* be very specific about the situation. */
                    mixer(KHOMP_LOG, 1, kmsGenerator, kmtBusy);
                }
            }
            else
            {
                DBG(FUNC, PVT_FMT(target(), "going to play busy"));

                /* stop the line audio */
                stopStream();

                /* be very specific about the situation. */
                mixer(KHOMP_LOG, 1, kmsGenerator, kmtBusy);

            }
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

    DBG(FUNC,PVT_FMT(_target, "(E1) r"));
    
    return true; 
}

void BoardE1::KhompPvtE1::setAnswerInfo(int answer_info)
{
    const char * value = answerInfoToString(answer_info);

    if (value == NULL)
    {    
        DBG(FUNC, PVT_FMT(_target, "signaled unknown call answer info '%d', using 'Unknown'...") % answer_info);
        value = "Unknown";
    }   
    
    DBG(FUNC,PVT_FMT(_target, "KCallAnswerInfo: %s") % value);

    try
    {
        setFSChannelVar("KCallAnswerInfo",value);
    }
    catch(Board::KhompPvt::InvalidSwitchChannel & err)
    {
        LOG(ERROR,PVT_FMT(_target, "%s") % err._msg.c_str()); 
    }
}

bool BoardE1::KhompPvtE1::setupConnection()
{
    if(!call()->_flags.check(Kflags::IS_INCOMING) && !call()->_flags.check(Kflags::IS_OUTGOING))
    {
        DBG(FUNC,PVT_FMT(_target, "Channel already disconnected"));
        return false;
    }

    /* if received some disconnect from 'drop collect call'
       feature of some pbx, then leave the call rock and rolling */
    Board::board(_target.device)->_timers.del(callE1()->_idx_disconnect);

    bool fax_detected = callE1()->_flags.check(Kflags::FAX_DETECTED) || (callE1()->_var_fax_adjust == T_TRUE);
    
    bool res_out_of_band_dtmf = (call()->_var_dtmf_state == T_UNKNOWN || fax_detected ?
        Opt::_options._suppression_delay()&& Opt::_options._out_of_band_dtmfs()&& !fax_detected : (call()->_var_dtmf_state == T_TRUE));
    
    bool res_echo_cancellator = (call()->_var_echo_state == T_UNKNOWN || fax_detected ?
        Opt::_options._echo_canceller()&& !fax_detected : (call()->_var_echo_state == T_TRUE));

    bool res_auto_gain_cntrol = (call()->_var_gain_state == T_UNKNOWN || fax_detected ?
        Opt::_options._auto_gain_control()&& !fax_detected : (call()->_var_gain_state == T_TRUE));

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

        DBG(FUNC, PVT_FMT(_target, "(E1) Audio callbacks initialized successfully"));  
    }

    return Board::KhompPvt::setupConnection();
}

bool BoardE1::KhompPvtE1::application(ApplicationType type, switch_core_session_t * session, const char *data)
{
    switch(type)
    {
        case FAX_ADJUST:
            return _fax->adjustForFax();
        case FAX_SEND:
            return _fax->sendFax(session, data);
        case FAX_RECEIVE:
            return _fax->receiveFax(session, data);
        default:
            return KhompPvt::application(type, session, data);
    }

    return true;
}

bool BoardE1::KhompPvtE1::validContexts(
        MatchExtension::ContextListType & contexts, std::string extra_context)
{
    DBG(FUNC,PVT_FMT(_target, "(E1) c"));

    if(!_group_context.empty())
    {
        contexts.push_back(_group_context);
    }

    contexts.push_back(Opt::_options._context_digital());

    for (MatchExtension::ContextListType::iterator i = contexts.begin(); i != contexts.end(); i++) 
    {    
        replaceTemplate((*i), "LL", ((_target.object)/30));
        replaceTemplate((*i), "CCC", _target.object);
    }

    bool ret = Board::KhompPvt::validContexts(contexts,extra_context);

    DBG(FUNC,PVT_FMT(_target, "(E1) r"));

    return ret;
}

int BoardE1::KhompPvtISDN::makeCall(std::string params)
{
    DBG(FUNC,PVT_FMT(_target, "(ISDN) c"));   

    CallISDN * call = callISDN();

    if(call->_uui_descriptor != -1)
    {
        DBG(FUNC,PVT_FMT(_target, "got userinfo"));   

        /* grab this information first, avoiding latter side-effects */
        const bool        info_extd = call->_uui_extended;
        const long int    info_desc = call->_uui_descriptor;
        const std::string info_data = call->_uui_information.c_str();
        const size_t      info_size = std::min<size_t>(call->_uui_information.size(), (info_extd ? KMAX_USER_USER_EX_LEN : KMAX_USER_USER_LEN) << 1) >> 1;

        bool res = true;
    
        if(info_extd)
        {
            KUserInformationEx info;

            info.ProtocolDescriptor = info_desc;
            info.UserInfoLength = info_size;

            for (unsigned int pos = 0u, index = 0u; pos < info_size; index+=2, ++pos)
                info.UserInfo[pos] = (unsigned char)Strings::toulong(info_data.substr(index,2), 16); 

            if (!command(KHOMP_LOG, CM_USER_INFORMATION_EX, (const char *) &info))
            {
                LOG(ERROR,PVT_FMT(_target, "UUI could not be sent before dialing!"));   
            }
        } 
        else
        {
            KUserInformation info;

            info.ProtocolDescriptor = info_desc;
            info.UserInfoLength = info_size;

            for (unsigned int pos = 0u, index = 0u; pos < info_size; index+=2, ++pos)
                info.UserInfo[pos] = (unsigned char)Strings::toulong(info_data.substr(index,2), 16); 

            if (!command(KHOMP_LOG, CM_USER_INFORMATION, (const char *) &info))
            {
                LOG(ERROR,PVT_FMT(_target, "UUI could not be sent before dialing!"));   
            }
        }

        call->_uui_extended = false; 
        call->_uui_descriptor = -1;
        call->_uui_information.clear();
    }

    if (!callISDN()->_isdn_orig_type_of_number.empty()) 
    {
        params += "isdn_orig_type_of_number=\""; 
        params += callISDN()->_isdn_orig_type_of_number; 
        params += "\" "; 
    }

    if (!callISDN()->_isdn_dest_type_of_number.empty()) 
    { 
        params += "isdn_dest_type_of_number=\""; 
        params += callISDN()->_isdn_dest_type_of_number; 
        params += "\" "; 
    }

    if (!callISDN()->_isdn_orig_numbering_plan.empty()) 
    { 
        params += "isdn_orig_numbering_plan=\""; 
        params += callISDN()->_isdn_orig_numbering_plan; 
        params += "\" "; 
    }

    if (!callISDN()->_isdn_dest_numbering_plan.empty()) 
    { 
        params += "isdn_dest_numbering_plan=\""; 
        params += callISDN()->_isdn_dest_numbering_plan; 
        params += "\" "; 
    }

    if (!callISDN()->_isdn_orig_presentation.empty())
    { 
        params += "isdn_orig_presentation=\"";   
        params += callISDN()->_isdn_orig_presentation; 
        params += "\" "; 
    }

    int ret = KhompPvtE1::makeCall(params);

    call->_cleanup_upon_hangup = (ret == ksInvalidParams || ret == ksBusy);
    
    DBG(FUNC,PVT_FMT(_target, "(ISDN) r"));   

    return ret;
}

void BoardE1::KhompPvtISDN::reportFailToReceive(int fail_code)
{
    KhompPvt::reportFailToReceive(fail_code);

    if(fail_code != -1)
    {
        DBG(FUNC,PVT_FMT(_target, "sending a 'unknown number' message/audio")); 

        if(sendRingBackStatus(fail_code) == RingbackDefs::RBST_UNSUPPORTED)
        {
            sendPreAudio(RingbackDefs::RB_SEND_DEFAULT);
            startCadence(PLAY_FASTBUSY);
        }
    }
    else
    {
        DBG(FUNC, PVT_FMT(_target, "sending fast busy audio directly"));

        sendPreAudio(RingbackDefs::RB_SEND_DEFAULT);
        startCadence(PLAY_FASTBUSY);
    }
}

bool BoardE1::KhompPvtISDN::doChannelAnswer(CommandRequest &cmd)
{
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
        else
        {
            usleep(75000);

            DBG(FUNC, PVT_FMT(target(), "disconnecting collect call doChannelAnswer ISDN"));
            command(KHOMP_LOG,CM_DISCONNECT);

            // thou shalt not talk anymore!
            stopListen();
            stopStream();
        }

        ret = KhompPvtE1::doChannelAnswer(cmd);
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target, "unable to lock %s!") % err._msg.c_str() );
        return false;
    }

    return ret;
}

int BoardE1::KhompPvtR2::makeCall(std::string params)
{
    DBG(FUNC,PVT_FMT(_target, "(R2) c"));   

    if (callR2()->_r2_category != -1)
        params += STG(FMT(" r2_categ_a=\"%ld\"")
                % callR2()->_r2_category);

    int ret = KhompPvtE1::makeCall(params);

    call()->_cleanup_upon_hangup = (ret == ksInvalidParams);
    
    DBG(FUNC,PVT_FMT(_target, "(R2) r"));   

    return ret;
}

bool BoardE1::KhompPvtR2::doChannelAnswer(CommandRequest &cmd)
{
    bool ret = true;

    try
    {
        ScopedPvtLock lock(this);

        // is this a collect call?
        bool has_recv_collect_call = _call->_collect_call;

        // do we have to drop collect calls?
        bool has_drop_collect_call = call()->_flags.check(Kflags::DROP_COLLECT);

        // do we have to drop THIS call?
        bool do_drop_call = has_drop_collect_call && has_recv_collect_call;

        bool do_send_ring = call()->_flags.check(Kflags::NEEDS_RINGBACK_CMD);

        // do we have to send ringback? yes we need !!!
        if(do_send_ring)
        {       
            call()->_flags.clear(Kflags::NEEDS_RINGBACK_CMD);

            //TODO: callFailFromCause ??
            std::string cause = ( do_drop_call ? STG(FMT("r2_cond_b=\"%d\"") % kgbBusy) : "" );
            command(KHOMP_LOG,CM_RINGBACK,cause.c_str());

            usleep(75000);
        }

        if(!do_drop_call)
        {
            command(KHOMP_LOG, CM_CONNECT);
        }

        if(!do_send_ring && has_drop_collect_call) 
        {
            usleep(75000);

            if(has_recv_collect_call)
            {
                // thou shalt not talk anymore!
                stopListen();
                stopStream();

                if (call()->_indication == INDICA_NONE)
                {    
                    call()->_indication = INDICA_BUSY;
                    mixer(KHOMP_LOG, 1, kmsGenerator, kmtBusy);
                }    

                DBG(FUNC, PVT_FMT(_target,"forcing disconnect for collect call"));
                forceDisconnect();
            }
            else
            {
                DBG(FUNC, PVT_FMT(target(), "dropping collect call at doChannelAnswer"));
                command(KHOMP_LOG, CM_DROP_COLLECT_CALL);
            }
        }

        ret = KhompPvtE1::doChannelAnswer(cmd);
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR,PVT_FMT(_target, "unable to lock %s!") % err._msg.c_str() );
        return false;
    }

    return ret;
}

bool BoardE1::KhompPvtR2::doChannelHangup(CommandRequest &cmd)
{
    DBG(FUNC, PVT_FMT(_target, "(R2) c"));

    bool answered     = true;
    bool disconnected = false;

    try
    {
        ScopedPvtLock lock(this);

        if (call()->_flags.check(Kflags::IS_INCOMING))
        {
            DBG(FUNC,PVT_FMT(_target, "disconnecting incoming channel"));

            //disconnected = command(KHOMP_LOG, CM_DISCONNECT);
        }
        else if (call()->_flags.check(Kflags::IS_OUTGOING))
        {
            if(call()->_cleanup_upon_hangup)
            {
                DBG(FUNC,PVT_FMT(_target, "disconnecting not allocated outgoing channel..."));

                disconnected = command(KHOMP_LOG, CM_DISCONNECT);
                cleanup(KhompPvt::CLN_HARD);
                answered = false;

            }
            else
            {
                DBG(FUNC,PVT_FMT(_target, "disconnecting outgoing channel...")); 

                disconnected = command(KHOMP_LOG, CM_DISCONNECT);
            }
        }
        else
        {
            DBG(FUNC,PVT_FMT(_target, "already disconnected"));
            return true;
        }

        if(answered)
        {
            indicateBusyUnlocked(SWITCH_CAUSE_USER_BUSY, disconnected);
        }

        if (call()->_flags.check(Kflags::IS_INCOMING) && !call()->_flags.check(Kflags::NEEDS_RINGBACK_CMD))
        {
            DBG(FUNC,PVT_FMT(_target, "disconnecting incoming channel..."));
            disconnected = command(KHOMP_LOG, CM_DISCONNECT);
        }

        stopStream();

        stopListen();

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target, "(R2) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }


    DBG(FUNC, PVT_FMT(_target, "(R2) r"));
    return true;
}

int BoardE1::KhompPvtISDN::causeFromCallFail(int fail)
{
    int switch_cause = SWITCH_CAUSE_USER_BUSY;

    if (fail <= 127) 
        switch_cause = fail;
    else 
        switch_cause = SWITCH_CAUSE_INTERWORKING;

    return switch_cause;
}

void BoardE1::KhompPvtR2::reportFailToReceive(int fail_code)
{
    KhompPvt::reportFailToReceive(fail_code);

    if (Opt::_options._r2_strict_behaviour()&& fail_code != -1)
    {
        DBG(FUNC,PVT_FMT(_target, "sending a 'unknown number' message/audio")); 

        if(sendRingBackStatus(fail_code) == RingbackDefs::RBST_UNSUPPORTED)
        {
            sendPreAudio(RingbackDefs::RB_SEND_DEFAULT);
            startCadence(PLAY_FASTBUSY);
        }
    }
    else
    {
        DBG(FUNC, PVT_FMT(_target, "sending fast busy audio directly"));

        sendPreAudio(RingbackDefs::RB_SEND_DEFAULT);
        startCadence(PLAY_FASTBUSY);
    }
}

int BoardE1::KhompPvtR2::causeFromCallFail(int fail)
{
    int switch_cause = SWITCH_CAUSE_USER_BUSY;

    try
    {
        bool handled = false;

        switch (_r2_country)
        {
            case Verbose::R2_COUNTRY_ARG:
                switch (fail)
                {
                    case kgbArBusy:
                        switch_cause = SWITCH_CAUSE_USER_BUSY;
                        break;
                    case kgbArNumberChanged:
                        switch_cause = SWITCH_CAUSE_NUMBER_CHANGED;
                        break;
                    case kgbArCongestion:
                        switch_cause = SWITCH_CAUSE_NORMAL_CIRCUIT_CONGESTION;
                        break;
                    case kgbArInvalidNumber:
                        switch_cause = SWITCH_CAUSE_UNALLOCATED_NUMBER;
                        break;
                    case kgbArLineOutOfOrder:
                        switch_cause = SWITCH_CAUSE_REQUESTED_CHAN_UNAVAIL;
                        break;
                }
                handled = true;
                break;

            case Verbose::R2_COUNTRY_BRA:
                switch (fail)
                {
                    case kgbBrBusy:
                        switch_cause = SWITCH_CAUSE_USER_BUSY;
                        break;
                    case kgbBrNumberChanged:
                        switch_cause = SWITCH_CAUSE_NUMBER_CHANGED;
                        break;
                    case kgbBrCongestion:
                        switch_cause = SWITCH_CAUSE_NORMAL_CIRCUIT_CONGESTION;
                        break;
                    case kgbBrInvalidNumber:
                        switch_cause = SWITCH_CAUSE_UNALLOCATED_NUMBER;
                        break;
                    case kgbBrLineOutOfOrder:
                        switch_cause = SWITCH_CAUSE_REQUESTED_CHAN_UNAVAIL;
                        break;
                }
                handled = true;
                break;
            case Verbose::R2_COUNTRY_CHI:
                    switch (fail)
                    {
                        case kgbClBusy:
                            switch_cause = SWITCH_CAUSE_USER_BUSY;
                            break;
                        case kgbClNumberChanged:
                            switch_cause = SWITCH_CAUSE_NUMBER_CHANGED;
                            break;
                        case kgbClCongestion:
                            switch_cause = SWITCH_CAUSE_NORMAL_CIRCUIT_CONGESTION;
                            break;
                        case kgbClInvalidNumber:
                            switch_cause = SWITCH_CAUSE_UNALLOCATED_NUMBER;
                            break;
                        case kgbClLineOutOfOrder:
                            switch_cause = SWITCH_CAUSE_REQUESTED_CHAN_UNAVAIL;
                            break;
                    }
                handled = true;
                break;

            case Verbose::R2_COUNTRY_MEX:
                switch (fail)
                {
                    case kgbMxBusy:
                        switch_cause = SWITCH_CAUSE_USER_BUSY;
                        break;
                }
                handled = true;
                break;

            case Verbose::R2_COUNTRY_URY:
                switch (fail)
                {
                    case kgbUyBusy:
                        switch_cause = SWITCH_CAUSE_USER_BUSY;
                        break;
                    case kgbUyNumberChanged:
                        switch_cause = SWITCH_CAUSE_NUMBER_CHANGED;
                        break;
                    case kgbUyCongestion:
                        switch_cause = SWITCH_CAUSE_NORMAL_CIRCUIT_CONGESTION;
                        break;
                    case kgbUyInvalidNumber:
                        switch_cause = SWITCH_CAUSE_UNALLOCATED_NUMBER;
                        break;
                    case kgbUyLineOutOfOrder:
                        switch_cause = SWITCH_CAUSE_REQUESTED_CHAN_UNAVAIL;
                        break;
                }
                handled = true;
                break;
            case Verbose::R2_COUNTRY_VEN:
                switch (fail)
                {
                    case kgbVeBusy:
                        switch_cause = SWITCH_CAUSE_USER_BUSY;
                        break;
                    case kgbVeNumberChanged:
                        switch_cause = SWITCH_CAUSE_NUMBER_CHANGED;
                        break;
                    case kgbVeCongestion:
                        switch_cause = SWITCH_CAUSE_NORMAL_CIRCUIT_CONGESTION;
                        break;
                    case kgbVeLineBlocked:
                        switch_cause = SWITCH_CAUSE_OUTGOING_CALL_BARRED;
                        break;
                }
                handled = true;
                break;
        }

        if (!handled)
            throw std::runtime_error("");
    }
    catch (...)
    {
        LOG(ERROR,
                PVT_FMT(_target, "country signaling not found, unable to report R2 hangup code.."));
    }

    return switch_cause;

}

int BoardE1::KhompPvtR2::callFailFromCause(int cause)
{
    int k3l_fail = -1; // default

    try  
    {    
        bool handled = false;

        switch (_r2_country)
        {    
            case Verbose::R2_COUNTRY_ARG:
                switch (cause)
                {    
                    case SWITCH_CAUSE_UNALLOCATED_NUMBER:
                    case SWITCH_CAUSE_NO_ROUTE_TRANSIT_NET:
                    case SWITCH_CAUSE_NO_ROUTE_DESTINATION:
                    case SWITCH_CAUSE_INVALID_NUMBER_FORMAT:
                    case SWITCH_CAUSE_INVALID_GATEWAY:
                    case SWITCH_CAUSE_INVALID_URL:
                    case SWITCH_CAUSE_FACILITY_NOT_SUBSCRIBED:
                    case SWITCH_CAUSE_INCOMPATIBLE_DESTINATION:

                    case SWITCH_CAUSE_INCOMING_CALL_BARRED: /* ?? */
                    case SWITCH_CAUSE_OUTGOING_CALL_BARRED: /* ?? */
                        k3l_fail = kgbArInvalidNumber;
                        break;

                    case SWITCH_CAUSE_USER_BUSY:
                    case SWITCH_CAUSE_NO_USER_RESPONSE:
                    case SWITCH_CAUSE_CALL_REJECTED:
                        k3l_fail = kgbArBusy;
                        break;

                    case SWITCH_CAUSE_NUMBER_CHANGED:
                        k3l_fail = kgbArNumberChanged;
                        break;

                    case SWITCH_CAUSE_NORMAL_CIRCUIT_CONGESTION:
                    case SWITCH_CAUSE_SWITCH_CONGESTION:

                    case SWITCH_CAUSE_NORMAL_CLEARING:
                    case SWITCH_CAUSE_NORMAL_UNSPECIFIED:
                    case SWITCH_CAUSE_CALL_AWARDED_DELIVERED: /* ?? */
                        // this preserves semantics..
                        k3l_fail = kgbArCongestion;
                        break;

                    case SWITCH_CAUSE_REQUESTED_CHAN_UNAVAIL:
                    case SWITCH_CAUSE_CHANNEL_UNACCEPTABLE:
                    case SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER:
                    case SWITCH_CAUSE_INVALID_PROFILE:
                    case SWITCH_CAUSE_NETWORK_OUT_OF_ORDER:
                    case SWITCH_CAUSE_GATEWAY_DOWN:
                    case SWITCH_CAUSE_FACILITY_REJECTED:
                    case SWITCH_CAUSE_FACILITY_NOT_IMPLEMENTED:
                    case SWITCH_CAUSE_CHAN_NOT_IMPLEMENTED:
                    default:
                        k3l_fail = kgbArLineOutOfOrder;
                        break;
                }    
                handled = true;
                break;

            case Verbose::R2_COUNTRY_BRA:
                switch (cause)
                {    
                    case SWITCH_CAUSE_UNALLOCATED_NUMBER:
                    case SWITCH_CAUSE_NO_ROUTE_TRANSIT_NET:
                    case SWITCH_CAUSE_NO_ROUTE_DESTINATION:
                    case SWITCH_CAUSE_INVALID_NUMBER_FORMAT:
                    case SWITCH_CAUSE_INVALID_GATEWAY:
                    case SWITCH_CAUSE_INVALID_URL:
                    case SWITCH_CAUSE_FACILITY_NOT_SUBSCRIBED:
                    case SWITCH_CAUSE_INCOMPATIBLE_DESTINATION:

                    case SWITCH_CAUSE_INCOMING_CALL_BARRED: /* ?? */
                    case SWITCH_CAUSE_OUTGOING_CALL_BARRED: /* ?? */
                        k3l_fail = kgbBrInvalidNumber;
                        break;

                    case SWITCH_CAUSE_USER_BUSY:
                    case SWITCH_CAUSE_NO_USER_RESPONSE:
                    case SWITCH_CAUSE_CALL_REJECTED:
                        k3l_fail = kgbBrBusy;
                        break;

                    case SWITCH_CAUSE_NUMBER_CHANGED:
                        k3l_fail = kgbBrNumberChanged;
                        break;

                    case SWITCH_CAUSE_NORMAL_CIRCUIT_CONGESTION:
                    case SWITCH_CAUSE_SWITCH_CONGESTION:

                    case SWITCH_CAUSE_NORMAL_CLEARING:
                    case SWITCH_CAUSE_NORMAL_UNSPECIFIED:
                    case SWITCH_CAUSE_CALL_AWARDED_DELIVERED: /* ?? */
                        // this preserves semantics..
                        k3l_fail = kgbBrCongestion;
                        break;

                    case SWITCH_CAUSE_REQUESTED_CHAN_UNAVAIL:
                    case SWITCH_CAUSE_CHANNEL_UNACCEPTABLE:
                    case SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER:
                    case SWITCH_CAUSE_NETWORK_OUT_OF_ORDER:
                    case SWITCH_CAUSE_GATEWAY_DOWN:
                    case SWITCH_CAUSE_FACILITY_REJECTED:
                    case SWITCH_CAUSE_FACILITY_NOT_IMPLEMENTED:
                    case SWITCH_CAUSE_CHAN_NOT_IMPLEMENTED:
                    default:
                        k3l_fail = kgbBrLineOutOfOrder;
                        break;
                }
                handled = true;
                break;

            case Verbose::R2_COUNTRY_CHI:
                switch (cause)
                {
                    case SWITCH_CAUSE_UNALLOCATED_NUMBER:
                    case SWITCH_CAUSE_NO_ROUTE_TRANSIT_NET:
                    case SWITCH_CAUSE_NO_ROUTE_DESTINATION:
                    case SWITCH_CAUSE_INVALID_NUMBER_FORMAT:
                    case SWITCH_CAUSE_INVALID_GATEWAY:
                    case SWITCH_CAUSE_INVALID_URL:
                    case SWITCH_CAUSE_FACILITY_NOT_SUBSCRIBED:
                    case SWITCH_CAUSE_INCOMPATIBLE_DESTINATION:

                    case SWITCH_CAUSE_INCOMING_CALL_BARRED: /* ?? */
                    case SWITCH_CAUSE_OUTGOING_CALL_BARRED: /* ?? */
                        k3l_fail = kgbClInvalidNumber;
                        break;

                    case SWITCH_CAUSE_USER_BUSY:
                    case SWITCH_CAUSE_NO_USER_RESPONSE:
                    case SWITCH_CAUSE_CALL_REJECTED:
                        k3l_fail = kgbClBusy;
                        break;

                    case SWITCH_CAUSE_NUMBER_CHANGED:
                        k3l_fail = kgbClNumberChanged;
                        break;

                    case SWITCH_CAUSE_NORMAL_CIRCUIT_CONGESTION:
                    case SWITCH_CAUSE_SWITCH_CONGESTION:

                    case SWITCH_CAUSE_NORMAL_CLEARING:
                    case SWITCH_CAUSE_NORMAL_UNSPECIFIED:
                    case SWITCH_CAUSE_CALL_AWARDED_DELIVERED: /* ?? */
                        // this preserves semantics..
                        k3l_fail = kgbClCongestion;
                        break;

                    case SWITCH_CAUSE_REQUESTED_CHAN_UNAVAIL:
                    case SWITCH_CAUSE_CHANNEL_UNACCEPTABLE:
                    case SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER:
                    case SWITCH_CAUSE_NETWORK_OUT_OF_ORDER:
                    case SWITCH_CAUSE_GATEWAY_DOWN:
                    case SWITCH_CAUSE_FACILITY_REJECTED:
                    case SWITCH_CAUSE_FACILITY_NOT_IMPLEMENTED:
                    case SWITCH_CAUSE_CHAN_NOT_IMPLEMENTED:
                    default:
                        k3l_fail = kgbClLineOutOfOrder;
                        break;
                }
                handled = true;
                break;


            case Verbose::R2_COUNTRY_MEX:
                k3l_fail = kgbMxBusy;
                handled = true;
                break;


            case Verbose::R2_COUNTRY_URY:
                switch (cause)
                {
                    case SWITCH_CAUSE_UNALLOCATED_NUMBER:
                    case SWITCH_CAUSE_NO_ROUTE_TRANSIT_NET:
                    case SWITCH_CAUSE_NO_ROUTE_DESTINATION:
                    case SWITCH_CAUSE_INVALID_NUMBER_FORMAT:
                    case SWITCH_CAUSE_INVALID_GATEWAY:
                    case SWITCH_CAUSE_INVALID_URL:
                    case SWITCH_CAUSE_FACILITY_NOT_SUBSCRIBED:
                    case SWITCH_CAUSE_INCOMPATIBLE_DESTINATION:

                    case SWITCH_CAUSE_INCOMING_CALL_BARRED: /* ?? */
                    case SWITCH_CAUSE_OUTGOING_CALL_BARRED: /* ?? */
                        k3l_fail = kgbUyInvalidNumber;
                        break;

                    case SWITCH_CAUSE_USER_BUSY:
                    case SWITCH_CAUSE_NO_USER_RESPONSE:
                    case SWITCH_CAUSE_CALL_REJECTED:
                        k3l_fail = kgbUyBusy;
                        break;

                    case SWITCH_CAUSE_NUMBER_CHANGED:
                        k3l_fail = kgbUyNumberChanged;
                        break;

                    case SWITCH_CAUSE_NORMAL_CIRCUIT_CONGESTION:
                    case SWITCH_CAUSE_SWITCH_CONGESTION:

                    case SWITCH_CAUSE_NORMAL_CLEARING:
                    case SWITCH_CAUSE_NORMAL_UNSPECIFIED:
                    case SWITCH_CAUSE_CALL_AWARDED_DELIVERED: /* ?? */
                        // this preserves semantics..
                        k3l_fail = kgbUyCongestion;
                        break;

                    case SWITCH_CAUSE_REQUESTED_CHAN_UNAVAIL:
                    case SWITCH_CAUSE_CHANNEL_UNACCEPTABLE:
                    case SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER:
                    case SWITCH_CAUSE_NETWORK_OUT_OF_ORDER:
                    case SWITCH_CAUSE_GATEWAY_DOWN:
                    case SWITCH_CAUSE_FACILITY_REJECTED:
                    case SWITCH_CAUSE_FACILITY_NOT_IMPLEMENTED:
                    case SWITCH_CAUSE_CHAN_NOT_IMPLEMENTED:
                    default:
                        k3l_fail = kgbUyLineOutOfOrder;
                        break;
                }
                handled = true;
                break;


            case Verbose::R2_COUNTRY_VEN:
                switch (cause)
                {
                    case SWITCH_CAUSE_INCOMING_CALL_BARRED:
                    case SWITCH_CAUSE_OUTGOING_CALL_BARRED:
                        k3l_fail = kgbVeLineBlocked;
                        break;

                    case SWITCH_CAUSE_NUMBER_CHANGED:
                        k3l_fail = kgbVeNumberChanged;
                        break;

                    case SWITCH_CAUSE_NORMAL_CIRCUIT_CONGESTION:
                    case SWITCH_CAUSE_SWITCH_CONGESTION:

                    case SWITCH_CAUSE_NORMAL_CLEARING:
                    case SWITCH_CAUSE_NORMAL_UNSPECIFIED:
                    case SWITCH_CAUSE_CALL_AWARDED_DELIVERED: /* ?? */
                        // this preserves semantics..
                        k3l_fail = kgbVeCongestion;
                        break;

                    case SWITCH_CAUSE_UNALLOCATED_NUMBER:
                    case SWITCH_CAUSE_NO_ROUTE_TRANSIT_NET:
                    case SWITCH_CAUSE_NO_ROUTE_DESTINATION:
                    case SWITCH_CAUSE_INVALID_NUMBER_FORMAT:
                    case SWITCH_CAUSE_INVALID_GATEWAY:
                    case SWITCH_CAUSE_INVALID_URL:
                    case SWITCH_CAUSE_FACILITY_NOT_SUBSCRIBED:
                    case SWITCH_CAUSE_INCOMPATIBLE_DESTINATION:

                    case SWITCH_CAUSE_REQUESTED_CHAN_UNAVAIL:
                    case SWITCH_CAUSE_CHANNEL_UNACCEPTABLE:
                    case SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER:
                    case SWITCH_CAUSE_NETWORK_OUT_OF_ORDER:
                    case SWITCH_CAUSE_GATEWAY_DOWN:
                    case SWITCH_CAUSE_FACILITY_REJECTED:
                    case SWITCH_CAUSE_FACILITY_NOT_IMPLEMENTED:
                    case SWITCH_CAUSE_CHAN_NOT_IMPLEMENTED:

                    case SWITCH_CAUSE_USER_BUSY:
                    case SWITCH_CAUSE_NO_USER_RESPONSE:
                    case SWITCH_CAUSE_CALL_REJECTED:
                    default:
                        k3l_fail = kgbVeBusy;
                        break;
                }

                handled = true;
                break;

        }
    
        if (!handled)
            throw std::runtime_error("");
    }
    catch(...)
    {
        LOG(ERROR,PVT_FMT(_target, "country signaling not found, unable to report R2 hangup code."));
    }

    return k3l_fail;
}

int BoardE1::KhompPvtISDN::callFailFromCause(int cause)
{
    int k3l_fail = -1; // default

    if (cause <= 127) 
        k3l_fail = cause;
    else 
        k3l_fail = kq931cInterworking;

    return k3l_fail;
}

RingbackDefs::RingbackStType BoardE1::KhompPvtR2::sendRingBackStatus(int rb_value)
{
    DBG(FUNC,PVT_FMT(_target, "(p=%p) this is the r2 ringback procedure") % this);   

    std::string cause = (rb_value == -1 ? "" : STG(FMT("r2_cond_b=\"%d\"") % rb_value));
    return (command(KHOMP_LOG, CM_RINGBACK, cause.c_str()) ?
            RingbackDefs::RBST_SUCCESS : RingbackDefs::RBST_FAILURE);
}

bool BoardE1::KhompPvtR2::sendPreAudio(int rb_value)
{
    DBG(FUNC,PVT_FMT(_target, "must send R2 preaudio ?"));   
    if(!KhompPvtE1::sendPreAudio(rb_value))
        return false;


    DBG(FUNC,PVT_FMT(_target, "doing the R2 pre_connect wait..."));   

    /* wait some ms, just to be sure the command has been sent. */
    usleep(Opt::_options._r2_preconnect_wait()* 1000);

    if (call()->_flags.check(Kflags::HAS_PRE_AUDIO))
    {
        DBG(FUNC, PVT_FMT(target(), "(p=%p) already pre_connect") % this);
        return true;
    }
    else
    {
        bool result = command(KHOMP_LOG, CM_PRE_CONNECT);

        if (result)
            call()->_flags.set(Kflags::HAS_PRE_AUDIO);

        return result;
    }
}

void BoardE1::KhompPvtR2::numberDialTimer(Board::KhompPvt * pvt)
{
    try 
    {   
        ScopedPvtLock lock(pvt);

        if (!pvt->call()->_flags.check(Kflags::NUMBER_DIAL_ONGOING) ||
             pvt->call()->_flags.check(Kflags::NUMBER_DIAL_FINISHD))
        {   
            return;
        }   

        pvt->call()->_flags.set(Kflags::NUMBER_DIAL_FINISHD);

        static_cast<BoardE1::KhompPvtR2*>(pvt)->callR2()->_incoming_exten.clear();
        pvt->command(KHOMP_LOG, CM_END_OF_NUMBER);
    }   
    catch (...)
    {   
        // TODO: log something.
    }   
}

bool BoardE1::KhompPvtR2::indicateRinging()
{
    DBG(FUNC, PVT_FMT(_target, "(R2) c")); 

    bool ret = false;
    try
    {
        ScopedPvtLock lock(this);

        /* already playing! */
        if (call()->_indication != INDICA_NONE)
        {    
            DBG(FUNC, PVT_FMT(_target, "(R2) r (already playing something: %d)") % call()->_indication);
            return false;
        }    

        // any collect calls ?
        setCollectCall();

        call()->_indication = INDICA_RING;

        bool send_ringback = true;

        if (!call()->_flags.check(Kflags::CONNECTED))
        {    
            int ringback_value = RingbackDefs::RB_SEND_DEFAULT;

            bool do_drop_call = Opt::_options._drop_collect_call()
                                        || call()->_flags.check(Kflags::DROP_COLLECT);

            if (do_drop_call && call()->_collect_call)
            {
                ringback_value = kgbBusy;
                DBG(FUNC, PVT_FMT(_target, "ringback value adjusted to refuse collect call: %d") % ringback_value);
            }

            const char *condition_string = getFSChannelVar("KR2SendCondition");

            try  
            {    
                if (condition_string)
                {    
                    ringback_value = Strings::toulong(condition_string);
                    DBG(FUNC, PVT_FMT(_target, "KR2SendCondition adjusted ringback value to %d") % ringback_value);
                }    
            }    
            catch (Strings::invalid_value e)
            {    
                LOG(ERROR, PVT_FMT(_target, "invalid value '%s', adjusted in KR2SendCondition: not a valid number.")
                        % condition_string);
            }    

            if (Opt::_options._r2_strict_behaviour())
            {
                /* send ringback too? */
                send_ringback = sendPreAudio(ringback_value);

                if (!send_ringback)
                {
                    /* warn the developer which may be debugging some "i do not have ringback!" issue. */
                    DBG(FUNC, PVT_FMT(_target, "not sending pre connection audio"));
                }

                call()->_flags.clear(Kflags::NEEDS_RINGBACK_CMD);
            }

        }

        if (send_ringback)
        {             
            DBG(FUNC, PVT_FMT(_target, "Send ringback!"));

            call()->_flags.set(Kflags::GEN_CO_RING);
            call()->_idx_co_ring = Board::board(_target.device)->_timers.add(Opt::_options._ringback_co_delay(), &Board::KhompPvt::coRingGen,this);

            /* start grabbing audio */
            startListen();

            /* start stream if it is not already */
            startStream();

            ret = true; 
        }   

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target, "(R2) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }
    catch(Board::KhompPvt::InvalidSwitchChannel & err)
    {
        LOG(ERROR,PVT_FMT(_target, "(R2) r (%s)") % err._msg.c_str());
        return false;
    }
    catch (K3LAPITraits::invalid_device & err)
    {
        LOG(ERROR, PVT_FMT(_target, "(R2) r (unable to get device: %d!)") % err.device);
        return false;
    }
    
    DBG(FUNC, PVT_FMT(_target, "(R2) r"));
    return ret;
}

bool BoardE1::KhompPvtR2::onNewCall(K3L_EVENT *e)
{
    DBG(FUNC,PVT_FMT(_target, "(R2) c"));   

    std::string r2_categ_a;

    int status = Globals::k3lapi.get_param(e, "r2_categ_a", r2_categ_a);

    try
    {
        ScopedPvtLock lock(this);

        if (status == ksSuccess && !r2_categ_a.empty())
        {

            try 
            { 
                callR2()->_r2_category = Strings::toulong(r2_categ_a); 
            }
            catch (Strings::invalid_value e) 
            { 
                /* do nothing */ 
            };

            /* channel will know if is a collect call or not */
            if (callR2()->_r2_category == kg2CollectCall)
            {
                call()->_collect_call = true;
            }
        }

        bool ret = KhompPvtE1::onNewCall(e);

        if(!ret)
            return false;

        if (!Opt::_options._r2_strict_behaviour())
        {
            bool do_drop_collect = Opt::_options._drop_collect_call();
            const char* drop_str = getFSGlobalVar("KDropCollectCall");

            if(checkTrueString(drop_str))
            {
                do_drop_collect = true;
                call()->_flags.set(Kflags::DROP_COLLECT);
                DBG(FUNC,PVT_FMT(_target, "Setting DROP_COLLECT flag"));
            }

            freeFSGlobalVar(&drop_str);            

            // keeping the hardcore mode
            if (do_drop_collect && call()->_collect_call)
            {
                // kill, kill, kill!!!
                DBG(FUNC,PVT_FMT(_target, "dropping collect call at onNewCall"));
                sendRingBackStatus(callFailFromCause(SWITCH_CAUSE_CALL_REJECTED));
                usleep(75000);
            }
            else
            {
                // send ringback too! 
                sendPreAudio(RingbackDefs::RB_SEND_DEFAULT);
                startStream();
            }
        }
        else
        {
            _call->_flags.set(Kflags::NEEDS_RINGBACK_CMD);
        }
    }
    catch(ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(target(), "(R2) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }

    DBG(FUNC,PVT_FMT(_target, "(R2) r"));   

    return true;
}

bool BoardE1::KhompPvtR2::onCallSuccess(K3L_EVENT *e)
{
    try
    {
        ScopedPvtLock lock(this);

        if(e->AddInfo > 0)
        {
            callR2()->_r2_condition = e->AddInfo;
            setFSChannelVar("KR2GotCondition", Verbose::signGroupB((KSignGroupB)callR2()->_r2_condition).c_str());
        }

        KhompPvtE1::onCallSuccess(e);

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(target(), "unable to lock %s!") % err._msg.c_str() );
        return false;
    }
    catch(Board::KhompPvt::InvalidSwitchChannel & err)
    {
        LOG(ERROR,PVT_FMT(_target, "%s") % err._msg.c_str()); 
        return false;
    }

    return true;
}

bool BoardE1::KhompPvtR2::onCallFail(K3L_EVENT *e)
{
    bool ret = true;

    try
    {
        ScopedPvtLock lock(this);

        if(e->AddInfo > 0)
        {
            callR2()->_r2_condition = e->AddInfo;
            setFSChannelVar("KR2GotCondition", Verbose::signGroupB((KSignGroupB)callR2()->_r2_condition).c_str());
        }

        setHangupCause(causeFromCallFail(e->AddInfo),true);

        ret = KhompPvtE1::onCallFail(e);

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(target(), "unable to lock %s!") % err._msg.c_str() );
        return false;
    }
    catch(Board::KhompPvt::InvalidSwitchChannel & err)
    {
        LOG(ERROR,PVT_FMT(_target, "%s") % err._msg.c_str());
        return false;
    }

    return ret;
}

bool BoardE1::KhompPvtR2::onNumberDetected(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_target, "(digit=%d) c") % e->AddInfo);

    try  
    {    
        ScopedPvtLock lock(this);

        if (call()->_flags.check(Kflags::NUMBER_DIAL_FINISHD))
            return false;

        if (!call()->_flags.check(Kflags::NUMBER_DIAL_ONGOING))
        {    
            DBG(FUNC, PVT_FMT(_target, "incoming number start..."));

            call()->_flags.set(Kflags::NUMBER_DIAL_ONGOING);
            callR2()->_incoming_exten.clear();

            callR2()->_idx_number_dial = Board::board(_target.device)->_timers.add(4000,
                &BoardE1::KhompPvtR2::numberDialTimer, this, TM_VAL_CALL);
        }    
        else 
        {    
            Board::board(_target.device)->_timers.restart(callR2()->_idx_number_dial);
        }    

        callR2()->_incoming_exten += e->AddInfo;

        DBG(FUNC, PVT_FMT(_target, "incoming exten %s") % callR2()->_incoming_exten);

        /* begin context adjusting + processing */
        MatchExtension::ContextListType contexts;

        validContexts(contexts);

        /* temporary */
        std::string tmp_exten;
        std::string tmp_context;
        std::string tmp_orig("");

        switch (MatchExtension::findExtension(tmp_exten, tmp_context, contexts, callR2()->_incoming_exten,tmp_orig, false, false))
        {    
            case MatchExtension::MATCH_EXACT:
            case MatchExtension::MATCH_NONE:
                call()->_flags.set(Kflags::NUMBER_DIAL_FINISHD);

                DBG(FUNC,FMT("incoming exten matched: %s") % callR2()->_incoming_exten);

                callR2()->_incoming_exten.clear();
                command(KHOMP_LOG,CM_END_OF_NUMBER);
                break;

            case MatchExtension::MATCH_MORE:
                DBG(FUNC, "didn't match exact extension, waiting...");
                // cannot say anything exact about the number, do nothing...
                break;
        }    
    }    
    catch (ScopedLockFailed & err)
    {    
        LOG(ERROR, PVT_FMT(target(), "unable to lock %s!") % err._msg.c_str() );
        return false;
    }    
    catch (K3LAPITraits::invalid_device & err)
    {
        LOG(ERROR, PVT_FMT(_target, "unable to get device: %d!") % err.device);
        return false;
    }

    return true;
}

bool BoardE1::KhompPvtFlash::application(ApplicationType type, switch_core_session_t * session, const char *data)
{
    switch(type)
    {
        case USER_TRANSFER:
            return _transfer->userTransfer(session, data);
        default:
            return KhompPvtR2::application(type, session, data);
    }

    return true;
}

bool BoardE1::KhompPvtFlash::sendDtmf(std::string digit)
{
    if(_transfer->checkUserXferUnlocked(digit))
    {
        DBG(FUNC, PVT_FMT(target(), "started (or waiting for) an user xfer"));
        return true;
    }

    bool ret = KhompPvtR2::sendDtmf(callFlash()->_digits_buffer);

    callFlash()->_digits_buffer.clear();

    return ret;
}

OrigToNseqMapType BoardE1::KhompPvtFXS::generateNseqMap()
{
    OrigToNseqMapType fxs_nseq; /* sequence numbers on FXS */

    fxs_nseq.insert(OrigToNseqPairType("", 0)); /* global sequence */

    for (BoardToOrigMapType::iterator i = Opt::_fxs_orig_base.begin(); i != Opt::_fxs_orig_base.end(); i++)
    {
        fxs_nseq.insert(OrigToNseqPairType((*i).second, 0));
    }

    return fxs_nseq;
}


void BoardE1::KhompPvtFXS::load(OrigToNseqMapType & fxs_nseq)
{
    BoardToOrigMapType::iterator it1 = Opt::_fxs_orig_base.find(_target.device);
    OrigToNseqMapType::iterator  it2;

    std::string orig_base("invalid"); /* will have orig base */

    if (it1 == Opt::_fxs_orig_base.end())
    {
        it2 = fxs_nseq.find("");
        orig_base = Opt::_options._fxs_global_orig_base();
    }
    else
    {
        it2 = fxs_nseq.find((*it1).second);
        orig_base = (*it1).second;
    }

    if (it2 == fxs_nseq.end())
    {
        LOG(ERROR, PVT_FMT(_target, "could not find sequence number for FXS channel"));

        /* Ok, load the default options */
        loadOptions();
    }
    else
    {
        try
        {
            /* generate orig_addr, padding to the original size (adding zeros at left) */
            _fxs_fisical_addr = padOrig(orig_base, (*it2).second);

            /* Set this branch options to get right callerid before mapping */
            loadOptions();

            /* makes a "reverse mapping" for Dial using 'r' identifiers */
            Opt::_fxs_branch_map.insert(BranchToObjectPairType(_fxs_orig_addr,
                ObjectIdType(_target.device, _target.object)));

            /* increment sequence number */
            ++((*it2).second);
        }
        catch (Strings::invalid_value e)
        {
            LOG(ERROR, PVT_FMT(_target, "expected an integer, got string '%s'")
                % e.value() );
        }
    }
}

void BoardE1::KhompPvtFXS::loadOptions()
{
    /* the fxs_orig_addr can be reset on parse_branch_options */
    _fxs_orig_addr = _fxs_fisical_addr;

    /* Initialize fxs default options */
    _calleridname.clear();
    //_amaflags = Opt::_amaflags;
    _callgroup = Opt::_options._callgroup();
    _pickupgroup = Opt::_options._pickupgroup();
    _context.clear();
    _input_volume = Opt::_options._input_volume();
    _output_volume = Opt::_options._output_volume();
    _mailbox.clear();
    _flash = Opt::_options._flash();

    BranchToOptMapType::iterator it3 = Opt::_branch_options.find(_fxs_orig_addr);

    if (it3 != Opt::_branch_options.end())
    {
        parseBranchOptions(it3->second);
    }

    //TODO: Implementar o setVolume para levar em consideracao que o 
    //       padrao pode ser o da FXS
    if (_input_volume != Opt::_options._input_volume())
        setVolume("input", _input_volume);
    if (_output_volume != Opt::_options._output_volume())
        setVolume("output", _output_volume);
}

bool BoardE1::KhompPvtFXS::parseBranchOptions(std::string options_str)
{
    Strings::vector_type options;
    Strings::tokenize(options_str, options, "|/");

    if (options.size() < 1)
    {
        DBG(FUNC, PVT_FMT(_target, "[fxs-options] no options are set for branch %s.") %
                _fxs_orig_addr.c_str());
        return false;
    }

    try
    {
        for (Strings::vector_type::iterator it = options.begin(); it != options.end(); it++)
        {
            Strings::vector_type par;
            Strings::tokenize(Strings::trim(*it), par, ":");

            if ( par.size() != 2 )
            {
                LOG(WARNING, PVT_FMT(_target, "[fxs-options] error on parsing options for branch %s.") % _fxs_orig_addr.c_str());
                return false;
            }

            std::string opt_name = Strings::trim(par.at(0));
            std::string opt_value = Strings::trim(par.at(1));

            if (opt_name == "pickupgroup")
            {
               _pickupgroup = opt_value;//ast_get_group(opt_value.c_str());
            }
            else if (opt_name == "callgroup")
            {
               _callgroup = opt_value;//ast_get_group(opt_value.c_str());
            }
            /*
               else if (opt_name == "amaflags")
               {
               int amaflags = ast_cdr_amaflags2int(opt_value.c_str());

               if (amaflags < 0)
               DBG(FUNC, PVT_FMT(_target, "[fxs-options] invalid AMA flags on branch %s.") % _fxs_orig_addr.c_str());
               else
               _amaflags = amaflags;
               }
            */
            else if (opt_name == "context")
            {
                _context = opt_value;
            }
            else if (opt_name == "input-volume")
            {
                long long volume = Strings::tolong(opt_value);

                if ( (volume < -10) || (volume > 10) )
                {
                    DBG(FUNC, PVT_FMT(_target, "[fxs-options] input-volume on branch %s.") % _fxs_orig_addr.c_str());
                }
                else
                {
                    _input_volume = volume;
                }
            }
            else if (opt_name == "output-volume")
            {
                long long volume = Strings::tolong(opt_value);

                if ( (volume < -10) || (volume > 10) )
                {
                    DBG(FUNC, PVT_FMT(_target, "[fxs-options] ouput-volume on branch %s.") % _fxs_orig_addr.c_str());
                }
                else
                {
                    _output_volume = volume;
                }
            }
            else if (opt_name == "language")
            {
                _language = opt_value;
            }
            else if (opt_name == "mohclass")
            {
                _mohclass = opt_value;
            }
            else if (opt_name == "accountcode")
            {
                _accountcode = opt_value;
            }
            else if (opt_name == "calleridnum") // conscious ultra chuncho!
            {
                BranchToOptMapType::iterator it3 = Opt::_branch_options.find(_fxs_orig_addr);

                if (it3 != Opt::_branch_options.end())
                {
                    Opt::_branch_options.insert(BranchToOptPairType(opt_value, it3->second));

                    Opt::_branch_options.erase(it3);
                }

                _fxs_orig_addr = opt_value;
            }
            else if (opt_name == "calleridname")
            {
                _calleridname = opt_value;
            }
            else if (opt_name == "mailbox")
            {
                _mailbox = opt_value;
            }
            else if (opt_name == "flash-to-digits")
            {
                _flash = opt_value;
            }
            else
            {
                DBG(FUNC, PVT_FMT(_target, "[fxs-options] invalid option on branch %s: \"%s\".") % _fxs_orig_addr.c_str() % opt_name.c_str());
            }
        }
    }
    catch (Strings::invalid_value e)
    {
        LOG(WARNING, PVT_FMT(_target, "[fxs-options] number expected on Khomp configuration file, got '%s'.") % e.value().c_str());
        return false;
    }

    return true;
}

bool BoardE1::KhompPvtFXS::alloc()
{
    DBG(FUNC, PVT_FMT(_target, "(FXS) c"));

    callFXS()->_flags.set(Kflags::FXS_DIAL_FINISHD);

    if(justStart(/*need_context*/) != SWITCH_STATUS_SUCCESS)
    {
        int fail_code = callFailFromCause(SWITCH_CAUSE_REQUESTED_CHAN_UNAVAIL);
        setHangupCause(SWITCH_CAUSE_REQUESTED_CHAN_UNAVAIL);
        cleanup(CLN_FAIL);
        reportFailToReceive(fail_code);
        LOG(ERROR, PVT_FMT(target(), "(FXS) r (Initilization Error on start!)"));
        return false;
    }

    startListen();
    startStream();

    /* do this procedures early (as audio is already being heard) */
    dtmfSuppression(Opt::_options._out_of_band_dtmfs() && !callFXS()->_flags.check(Kflags::FAX_DETECTED));
    echoCancellation(Opt::_options._echo_canceller() && !callFXS()->_flags.check(Kflags::FAX_DETECTED));
    autoGainControl(Opt::_options._auto_gain_control() && !callFXS()->_flags.check(Kflags::FAX_DETECTED));

    //TODO: NEED RECORD HERE !?
    /* if it does not need context, probably it's a pickupcall and will
       not pass throw setup_connection, so we start recording here*/
    //if (!need_context && K::opt::recording && !pvt->is_recording)
    //startRecord();

    DBG(FUNC, "(FXS) r");
    return true;
}

bool BoardE1::KhompPvtFXS::onSeizureStart(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_target, "(FXS) c"));

    bool ret = true;

    try
    {
        ScopedPvtLock lock(this);
    
        /* we always have audio */
        call()->_flags.set(Kflags::HAS_PRE_AUDIO);

        ret = KhompPvt::onSeizureStart(e);

        if (justAlloc(true) != SWITCH_STATUS_SUCCESS)
        {
            int fail_code = callFailFromCause(SWITCH_CAUSE_UNALLOCATED_NUMBER);
            setHangupCause(SWITCH_CAUSE_UNALLOCATED_NUMBER);            
            cleanup(CLN_FAIL);
            reportFailToReceive(fail_code);
            LOG(ERROR,PVT_FMT(_target, "(FXS) r (Initilization Error on alloc!)"));
            return false;
        }

        /* disable to avoid problems with DTMF detection */
        echoCancellation(false);
        autoGainControl(false);

        call()->_orig_addr = _fxs_orig_addr;

        OrigToDestMapType::iterator i = Opt::_fxs_hotline.find(_fxs_orig_addr);

        if (i != Opt::_fxs_hotline.end())
        {
            /* make it burn! */
            call()->_dest_addr = (*i).second;
            alloc();
        }
        else
        {
            /* normal line */
            if (!_mailbox.empty() /*&& (ast_app_has_voicemail(pvt->fxs_opt.mailbox.c_str(), NULL) == 1)*/)
                startCadence(PLAY_VM_TONE);
            else
                startCadence(PLAY_PBX_TONE);

            call()->_flags.clear(Kflags::FXS_DIAL_ONGOING);
            call()->_flags.set(Kflags::FXS_OFFHOOK);
        }
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR,PVT_FMT(_target, "(FXS) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }
    
    DBG(FUNC, PVT_FMT(_target, "(FXS) r"));

    return ret;
}

bool BoardE1::KhompPvtFXS::onCallSuccess(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_target, "(FXS) c"));

    bool ret;

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
        else
        {
            call()->_flags.set(Kflags::GEN_PBX_RING);
            call()->_idx_pbx_ring = Board::board(_target.device)->_timers.add(
                    Opt::_options._ringback_pbx_delay(),
                    &Board::KhompPvt::pbxRingGen, 
                    this, 
                    TM_VAL_CALL);
        }

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target, "(FXS) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }
    catch (K3LAPITraits::invalid_device & err)
    {
        LOG(ERROR, PVT_FMT(_target, "(FXS) r (unable to get device: %d!)") % err.device);
        return false;
    }
    catch (Board::KhompPvt::InvalidSwitchChannel & err)
    {
        LOG(ERROR, PVT_FMT(target(), "(FXS) r (%s)") % err._msg.c_str() );
        return false;
    }
    
    DBG(FUNC, PVT_FMT(_target, "(FXS) r"));

    return ret;
}

void BoardE1::KhompPvtFXS::dialTimer(KhompPvt * pvt)
{
    DBG(FUNC, PVT_FMT(pvt->target(), "FXS Dial timer"));

    try
    {
        ScopedPvtLock lock(pvt);
    
        KhompPvtFXS * pvt_fxs = static_cast<BoardE1::KhompPvtFXS*>(pvt);

        if(!pvt_fxs->callFXS()->_flags.check(Kflags::FXS_DIAL_ONGOING) 
            || pvt_fxs->callFXS()->_flags.check(Kflags::FXS_DIAL_FINISHD))
            return;

        pvt_fxs->call()->_dest_addr = pvt_fxs->callFXS()->_incoming_exten;
        pvt_fxs->alloc();

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(pvt->target(), "unable to lock %s!") % err._msg.c_str());
    }

}

/*
void BoardE1::KhompPvtFXS::transferTimer(KhompPvt * pvt)
{
    DBG(FUNC, PVT_FMT(pvt->target(), "c"));

    try
    {
        ScopedPvtLock lock(pvt);

        KhompPvtFXS * pvt_fxs = static_cast<BoardE1::KhompPvtFXS*>(pvt);

        if(!pvt_fxs->callFXS()->_flags.check(Kflags::FXS_FLASH_TRANSFER))
        {
            DBG(FUNC, PVT_FMT(pvt->target(), "r (Flag not set)"));
            return;
        }

        pvt_fxs->callFXS()->_flags.clear(Kflags::FXS_FLASH_TRANSFER);

        if(pvt_fxs->callFXS()->_flash_transfer.empty())
        {
            DBG(FUNC, PVT_FMT(pvt->target(), "r (Number is empty)"));
            
            if(!pvt_fxs->stopTransfer())
            {
                pvt_fxs->cleanup(KhompPvt::CLN_HARD);
                DBG(FUNC, PVT_FMT(pvt_fxs->target(), "r (unable to stop transfer)"));
                return;
            }

            return;
        }

        // begin context adjusting + processing 
        MatchExtension::ContextListType contexts;

        pvt_fxs->validContexts(contexts);

        std::string tmp_exten;
        std::string tmp_context;

        switch (MatchExtension::findExtension(tmp_exten, tmp_context, contexts, pvt_fxs->callFXS()->_flash_transfer, pvt_fxs->call()->_orig_addr, false, false))
        {
            case MatchExtension::MATCH_EXACT:
            case MatchExtension::MATCH_MORE:
            {
                pvt_fxs->callFXS()->_flash_transfer = tmp_exten;
                DBG(FUNC,FMT("incoming exten matched: %s") % pvt_fxs->callFXS()->_flash_transfer);

                if(!pvt_fxs->transfer(tmp_context))
                {
                    pvt_fxs->cleanup(KhompPvt::CLN_HARD);
                    DBG(FUNC, PVT_FMT(pvt_fxs->target(), "r (unable to transfer)"));
                    return;
                }

                break;
            }
            case MatchExtension::MATCH_NONE:
            {
                DBG(FUNC, PVT_FMT(pvt_fxs->target(), "match none!"));
                
                if(!pvt_fxs->stopTransfer())
                {
                    pvt_fxs->cleanup(KhompPvt::CLN_HARD);
                    DBG(FUNC, PVT_FMT(pvt_fxs->target(), "r (unable to stop transfer)"));
                    return;
                }

                break;
            }
        }
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(pvt->target(), "r (unable to lock %s!)") % err._msg.c_str());
        return;
    }
    
    DBG(FUNC, PVT_FMT(pvt->target(), "r"));

}
*/

bool BoardE1::KhompPvtFXS::onChannelRelease(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_target, "(FXS) c"));

    bool ret = true;

    try
    {
        ScopedPvtLock lock(this);

        /*
        if(!callFXS()->_uuid_other_session.empty() && session())
        {
            
            //switch_core_session_t *hold_session;

            //if ((hold_session = switch_core_session_locate(callFXS()->_uuid_other_session.c_str()))) 
            //{
            //    switch_channel_t * hold = switch_core_session_get_channel(hold_session);
            //    switch_channel_stop_broadcast(hold);
            //    switch_channel_wait_for_flag(hold, CF_BROADCAST, SWITCH_FALSE, 5000, NULL);
            //    switch_core_session_rwunlock(hold_session);
            //}
            
            try
            {
                // get other side of the bridge 
                switch_core_session_t * peer_session = getFSLockedPartnerSession();
                unlockPartner(peer_session);
                DBG(FUNC, PVT_FMT(target(), "bridge with the new session"));
                switch_ivr_uuid_bridge(getUUID(peer_session), callFXS()->_uuid_other_session.c_str());

            }
            catch(Board::KhompPvt::InvalidSwitchChannel & err)
            {
                DBG(FUNC, PVT_FMT(target(), "no partner: %s!") % err._msg.c_str());
            }

            callFXS()->_uuid_other_session.clear();
        }
        */

        ret = KhompPvt::onChannelRelease(e);

    }
    catch(ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(target(), "(FXS) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }

    DBG(FUNC, PVT_FMT(_target, "(FXS) r"));
    return ret;
 
}
/*
bool BoardE1::KhompPvtFXS::startTransfer()
{
    DBG(FUNC, PVT_FMT(target(), "c"));

    try
    {
        switch_core_session_t *peer_session = getFSLockedPartnerSession();
        switch_channel_t *peer_channel = getFSChannel(peer_session);

        const char *stream = NULL;

        if (!(stream = getFSChannelVar(peer_channel, SWITCH_HOLD_MUSIC_VARIABLE)))
        {
            stream = "silence";
        }
        
        unlockPartner(peer_session);

        DBG(FUNC, PVT_FMT(target(), "stream=%s") % stream);

        if (stream && strcasecmp(stream, "silence"))
        {
            // Freeswitch not get/put frames 
            //switch_channel_set_flag(channel, CF_HOLD);
            switch_ivr_broadcast(getUUID(peer_session), stream, SMF_ECHO_ALEG | SMF_LOOP);
        }

        callFXS()->_flags.set(Kflags::FXS_FLASH_TRANSFER);
    
        startCadence(PLAY_PBX_TONE);

        callFXS()->_idx_transfer = Board::board(_target.device)->_timers.add(Opt::_options._fxs_digit_timeout()* 1000, &BoardE1::KhompPvtFXS::transferTimer, this, TM_VAL_CALL);        

    }
    catch(Board::KhompPvt::InvalidSwitchChannel & err)
    {
        //cleanup(KhompPvt::CLN_HARD);
        LOG(ERROR, PVT_FMT(target(), "r (no valid partner %s!)") % err._msg.c_str());
        return false;
    }
    catch (K3LAPITraits::invalid_device & err)
    {
        LOG(ERROR, PVT_FMT(_target, "unable to get device: %d!") % err.device);
    }

    //switch_ivr_hold_uuid(switch_core_session_get_uuid(session()), NULL, SWITCH_TRUE);
    DBG(FUNC, PVT_FMT(target(), "r"));

    return true;
}

bool BoardE1::KhompPvtFXS::stopTransfer()
{
    DBG(FUNC, PVT_FMT(target(), "c"));

    callFXS()->_flags.clear(Kflags::FXS_FLASH_TRANSFER);
    
    callFXS()->_flash_transfer.clear();

    stopCadence();

    try
    {
        Board::board(_target.device)->_timers.del(callFXS()->_idx_transfer);
    }
    catch (K3LAPITraits::invalid_device & err)
    {
        LOG(ERROR, PVT_FMT(_target, "unable to get device: %d!") % err.device);
    }

    try
    {
        // get other side of the bridge 
        switch_core_session_t * peer_session = getFSLockedPartnerSession();
        switch_channel_t * peer_channel = getFSChannel(peer_session);

        switch_channel_stop_broadcast(peer_channel);
        switch_channel_wait_for_flag(peer_channel, CF_BROADCAST, SWITCH_FALSE, 5000, NULL);

        unlockPartner(peer_session);

        //switch_ivr_unhold_uuid(switch_core_session_get_uuid(session()));
    }
    catch(Board::KhompPvt::InvalidSwitchChannel & err)
    {
        LOG(ERROR, PVT_FMT(target(), "r (no valid partner %s!)") % err._msg.c_str());
        return false;
    }

    DBG(FUNC, PVT_FMT(target(), "r"));
    return true;
}

static switch_status_t xferHook(switch_core_session_t *session)
{
    switch_channel_t *channel = switch_core_session_get_channel(session);
    switch_channel_state_t state = switch_channel_get_state(channel);

    DBG(FUNC, D("state change=%d") % state);

    if (state == CS_PARK) 
    {
        switch_core_event_hook_remove_state_change(session, xferHook);

        BoardE1::KhompPvtFXS * pvt = static_cast<BoardE1::KhompPvtFXS*>(switch_core_session_get_private(session));
        
        if(!pvt)
        {
            DBG(FUNC, D("pvt is NULL"));
            return SWITCH_STATUS_FALSE;
        }

        try
        {
            ScopedPvtLock lock(pvt);

            if(!pvt->callFXS()->_uuid_other_session.empty())
            {
                DBG(FUNC, D("bridge after park"));
                std::string number = pvt->callFXS()->_uuid_other_session;
                pvt->callFXS()->_uuid_other_session.clear();

                switch_ivr_uuid_bridge(pvt->getUUID(), number.c_str());
            }
        }
        catch(ScopedLockFailed & err)
        {
            LOG(ERROR, PVT_FMT(pvt->target(), "unable to lock: %s!") %  err._msg.c_str());            
            return SWITCH_STATUS_FALSE;
        }
        catch(Board::KhompPvt::InvalidSwitchChannel & err)
        {
            LOG(ERROR, PVT_FMT(pvt->target(), "%s!") %  err._msg.c_str());            
            return SWITCH_STATUS_FALSE;
        }

    }
    else if (state == CS_HANGUP) 
    {
        switch_core_event_hook_remove_state_change(session, xferHook);

        //BoardE1::KhompPvtFXS * pvt = static_cast<BoardE1::KhompPvtFXS*>(switch_core_session_get_private(session));
        //
        //if(!pvt)
        //{
        //    DBG(FUNC, D("pvt is NULL"));
        //    return SWITCH_STATUS_FALSE;
        //}

        //try
        //{
        //    ScopedPvtLock lock(pvt);

        //    if(!pvt->callFXS()->_uuid_other_session.empty())
        //    {
        //        DBG(FUNC, D("bridge after hangup"));
        //        std::string number = pvt->callFXS()->_uuid_other_session;
        //        pvt->callFXS()->_uuid_other_session.clear();

        //        switch_ivr_uuid_bridge(switch_core_session_get_uuid(session), number.c_str());
        //    }
        //}
        //catch(ScopedLockFailed & err)
        //{
        //   LOG(ERROR, PVT_FMT(pvt->target(), "unable to lock: %s!") %  err._msg.c_str());            
        //}

    }

    return SWITCH_STATUS_SUCCESS;
}

bool BoardE1::KhompPvtFXS::transfer(std::string & context, bool blind)
{
    DBG(FUNC, PVT_FMT(target(), "c"));

    callFXS()->_flags.clear(Kflags::FXS_FLASH_TRANSFER);

    std::string number = callFXS()->_flash_transfer;
    callFXS()->_flash_transfer.clear();

    try
    {
        Board::board(_target.device)->_timers.del(callFXS()->_idx_transfer);
    }
    catch (K3LAPITraits::invalid_device & err)
    {
        LOG(ERROR, PVT_FMT(_target, "unable to get device: %d!") % err.device);
    }

    try
    {
        // get other side of the bridge 
        switch_core_session_t * peer_session = getFSLockedPartnerSession();
        switch_channel_t * peer_channel = getFSChannel(peer_session);

        switch_channel_stop_broadcast(peer_channel);
        switch_channel_wait_for_flag(peer_channel, CF_BROADCAST, SWITCH_FALSE, 5000, NULL);

        unlockPartner(peer_session);

        if(blind)
        {
            DBG(FUNC, PVT_FMT(_target, "Blind Transfer"));
            switch_ivr_session_transfer(peer_session, number.c_str(), Opt::_options._dialplan().c_str(), context.c_str());
        }
        else
        {
            DBG(FUNC, PVT_FMT(_target, "Attended Transfer"));

            if(!callFXS()->_uuid_other_session.empty())
            {
                DBG(FUNC, PVT_FMT(target(), "second transfer, hang up session"));
            }
            else
            {
                DBG(FUNC, PVT_FMT(target(), "first transfer"));
                callFXS()->_uuid_other_session = getUUID(peer_session);    
                const char *stream = NULL;

                if (!(stream = switch_channel_get_hold_music(peer_channel)))
                {
                    stream = "silence";
                }

                DBG(FUNC, PVT_FMT(target(), "transfer stream=%s") % stream);

                if (stream && strcasecmp(stream, "silence"))
                {
                    std::string moh = STR(FMT("endless_playback:%s,park") % stream);
                    switch_ivr_session_transfer(peer_session, moh.c_str(), "inline", NULL);
                }
                else
                {
                    switch_ivr_session_transfer(peer_session, "endless_playback:local_stream://moh,park", "inline", NULL);
                    //switch_ivr_session_transfer(peer_session, "park", "inline", NULL);                
                }
            }

            switch_channel_t * channel = getFSChannel();
            switch_channel_set_variable(channel, SWITCH_PARK_AFTER_BRIDGE_VARIABLE, "true");
            switch_core_event_hook_add_state_change(session(), xferHook);

            switch_ivr_session_transfer(session(), number.c_str(), Opt::_options._dialplan().c_str(), context.c_str());

            DBG(FUNC, PVT_FMT(target(), "Generating ring"));
            call()->_indication = INDICA_RING;
            call()->_flags.set(Kflags::GEN_CO_RING);
            startCadence(PLAY_RINGBACK);

            
            //try
            //{
            //    call()->_idx_co_ring = Board::board(_target.device)->_timers.add(Opt::_options._ringback_co_delay(), &Board::KhompPvt::coRingGen,this);
            //}
            //catch (K3LAPITraits::invalid_device & err)
            //{
             //   LOG(ERROR, PVT_FMT(_target, "unable to get device: %d!") % err.device);
            //}
            
        }
    }
    catch(Board::KhompPvt::InvalidSwitchChannel & err)
    {
        LOG(ERROR, PVT_FMT(target(), "r (no valid partner %s!)") % err._msg.c_str());
        return false;
    }

    DBG(FUNC, PVT_FMT(target(), "r"));

    return true;
}
*/

bool BoardE1::KhompPvtFXS::onDtmfDetected(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_target, "(FXS) c (dtmf=%c)") % (char) e->AddInfo);

    bool ret = true;

    try
    {
        ScopedPvtLock lock(this);

        if (call()->_flags.check(Kflags::IS_INCOMING) && 
                callFXS()->_flags.check(Kflags::FXS_OFFHOOK) &&
                !callFXS()->_flags.check(Kflags::FXS_DIAL_FINISHD))
        {
            DBG(FUNC, PVT_FMT(_target, "dialing"));

            if (call()->_cadence == PLAY_PBX_TONE)
            {
                stopCadence();
            }

            if (call()->_cadence == PLAY_PUB_TONE)
            {
                stopCadence();
                //call()->_cadence = PLAY_NONE;
                //mixer(KHOMP_LOG, 1, kmsGenerator, kmtSilence);
            }

            if (!callFXS()->_flags.check(Kflags::FXS_DIAL_ONGOING))
            {
                DBG(FUNC, PVT_FMT(_target, "dialing started now, clearing stuff.."));

                callFXS()->_flags.set(Kflags::FXS_DIAL_ONGOING);

                callFXS()->_incoming_exten.clear();

                mixer(KHOMP_LOG, 1, kmsGenerator, kmtSilence);

                callFXS()->_idx_dial = Board::board(_target.device)->_timers.add(Opt::_options._fxs_digit_timeout()* 1000, &BoardE1::KhompPvtFXS::dialTimer, this, TM_VAL_CALL);
            }
            else
            {
                Board::board(_target.device)->_timers.restart(callFXS()->_idx_dial);
            }

            callFXS()->_incoming_exten += e->AddInfo;

            /* begin context adjusting + processing */
            MatchExtension::ContextListType contexts;

            validContexts(contexts);

            std::string tmp_exten;
            std::string tmp_context;

            switch (MatchExtension::findExtension(tmp_exten, tmp_context, contexts, callFXS()->_incoming_exten, call()->_orig_addr, false, false))
            {
                case MatchExtension::MATCH_EXACT:
                    callFXS()->_incoming_exten = tmp_exten;
                    DBG(FUNC,FMT("incoming exten matched: %s") % callFXS()->_incoming_exten);
                    Board::board(_target.device)->_timers.del(callFXS()->_idx_dial);
                    call()->_dest_addr = callFXS()->_incoming_exten;
                    call()->_incoming_context = tmp_context;

                    alloc();
                    break;

                case MatchExtension::MATCH_MORE:
                    DBG(FUNC, PVT_FMT(target(), "match more..."));

                    /* can match, will match more, and it's an external call? */
                    for (DestVectorType::const_iterator i = Opt::_options._fxs_co_dialtone().begin(); i != Opt::_options._fxs_co_dialtone().end(); i++)
                    {
                        if (callFXS()->_incoming_exten == (*i))
                        {
                            startCadence(PLAY_PUB_TONE);
                            break;
                        }
                    }

                    break;
                case MatchExtension::MATCH_NONE:
                    DBG(FUNC, PVT_FMT(target(), "match none!"));

                    std::string invalid = "i";
                    
                    Board::board(_target.device)->_timers.del(callFXS()->_idx_dial);

                    switch (MatchExtension::findExtension(tmp_exten, tmp_context, contexts, invalid, call()->_orig_addr, true, false))
                    {
                        case MatchExtension::MATCH_EXACT:
                            // this dialing is invalid, and we can handle it...
                            call()->_dest_addr = invalid;
                            call()->_incoming_context = tmp_context;
                            alloc();
                            break;
                        case MatchExtension::MATCH_MORE:
                        case MatchExtension::MATCH_NONE:
                            callFXS()->_flags.set(Kflags::FXS_DIAL_FINISHD);
                            startCadence(PLAY_FASTBUSY);
                            break;
                    }
                    break;
            }
        }
        /*
        else if(callFXS()->_flags.check(Kflags::FXS_OFFHOOK) &&
                callFXS()->_flags.check(Kflags::FXS_FLASH_TRANSFER))
        {
            Board::board(_target.device)->_timers.restart(callFXS()->_idx_transfer);
            DBG(FUNC, PVT_FMT(target(), "Flash Transfer"));

            if (call()->_cadence == PLAY_PBX_TONE)
            {
                stopCadence();
            }

            if (call()->_cadence == PLAY_PUB_TONE)
            {
                stopCadence();
                //call()->_cadence = PLAY_NONE;
                //mixer(KHOMP_LOG, 1, kmsGenerator, kmtSilence);
            }

            callFXS()->_flash_transfer += e->AddInfo;
    
            // begin context adjusting + processing
            MatchExtension::ContextListType contexts;

            validContexts(contexts);

            std::string tmp_exten;
            std::string tmp_context;
            
            switch (MatchExtension::findExtension(tmp_exten, tmp_context, contexts, callFXS()->_flash_transfer, call()->_orig_addr, false, false))
            {
                case MatchExtension::MATCH_EXACT:
                {
                    callFXS()->_flash_transfer = tmp_exten;
                    DBG(FUNC,FMT("incoming exten matched: %s") % callFXS()->_flash_transfer);

                    if(!transfer(tmp_context))
                    {
                        cleanup(KhompPvt::CLN_HARD);
                        DBG(FUNC, PVT_FMT(target(), "(FXS) r (unable to transfer)"));
                        return false;
                    }

                    break;
                }
                case MatchExtension::MATCH_MORE:
                    DBG(FUNC, PVT_FMT(target(), "match more..."));

                    // can match, will match more, and it's an external call?
                    for (DestVectorType::const_iterator i = Opt::_options._fxs_co_dialtone().begin(); i != Opt::_options._fxs_co_dialtone().end(); i++)
                    {
                        if (callFXS()->_flash_transfer == (*i))
                        {
                            startCadence(PLAY_PUB_TONE);
                            break;
                        }
                    }

                    break;
                case MatchExtension::MATCH_NONE:
                {
                    DBG(FUNC, PVT_FMT(target(), "match none!"));

                    if(!stopTransfer())
                    {
                        cleanup(KhompPvt::CLN_HARD);
                        DBG(FUNC, PVT_FMT(target(), "(FXS) r (unable to stop transfer)"));
                        return false;
                    }

                    break;
                }
            }
        }
        */
        else
        {
            ret = KhompPvt::onDtmfDetected(e);
        }

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target, "(FXS) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }
    catch (K3LAPITraits::invalid_device & err)
    {
        LOG(ERROR, PVT_FMT(_target, "(FXS) r (unable to get device: %d!)") % err.device);
        return false;
    }
    
    DBG(FUNC, PVT_FMT(_target, "(FXS) r"));

    return ret;
}

/*
bool BoardE1::KhompPvtFXS::onDtmfSendFinish(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_target, "(FXS) c"));

    bool ret = true;

    try
    {
        ScopedPvtLock lock(this);

        ret = KhompPvt::onDtmfSendFinish(e);

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target, "(FXS) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }
    
    DBG(FUNC, PVT_FMT(_target, "(FXS) r"));

    return ret;
}
*/

bool BoardE1::KhompPvtFXS::onFlashDetected(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_target, "(FXS) c (%s)") % _flash);

    try
    {
        ScopedPvtLock lock(this);

        for(std::string::const_iterator it = _flash.begin(); it != _flash.end(); it++)
        {
            signalDTMF(*it);
        }

        DBG(FUNC, PVT_FMT(_target, "(FXS) r"));

        return true;

/******************************************************************************/
        //Old implementation, not used
        /*
        if(callFXS()->_flags.check(Kflags::FXS_FLASH_TRANSFER))
        {
            DBG(FUNC, PVT_FMT(_target, "(FXS) transfer canceled"));

            if(!stopTransfer())
            {
                cleanup(KhompPvt::CLN_HARD);
                DBG(FUNC, PVT_FMT(target(), "(FXS) r (unable to stop transfer)"));
                return false;
            }

            DBG(FUNC, PVT_FMT(target(), "(FXS) r"));
            return true;
        }

        if(call()->_flags.check(Kflags::IS_INCOMING))
        {
            DBG(FUNC, PVT_FMT(_target, "incoming call"));
        }
        else if(call()->_flags.check(Kflags::IS_OUTGOING))
        {
            DBG(FUNC, PVT_FMT(_target, "outgoing call"));
        }
        else
        {
            DBG(FUNC, PVT_FMT(_target, "(FXS) r (!incoming and !outgoing call)"));
            return true;
        }

        echoCancellation(false);

        if(!startTransfer())
        {
            DBG(FUNC, PVT_FMT(target(), "(FXS) r (unable to start transfer)"));
            return false;
        }
        */
/******************************************************************************/

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target, "(FXS) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }
    
    DBG(FUNC, PVT_FMT(_target, "(FXS) r"));

    return true;
}

int BoardE1::KhompPvtFXS::makeCall(std::string params)
{
    DBG(FUNC,PVT_FMT(_target, "(FXS) c"));

    /* we always have audio */
    call()->_flags.set(Kflags::HAS_PRE_AUDIO);
    
    if (Opt::_options._fxs_bina()&& !call()->_orig_addr.empty())
    {
        /* Sending Bina DTMF*/
        callFXS()->_flags.set(Kflags::WAIT_SEND_DTMF);

        std::stringstream dial_bina;

        dial_bina << "A1" << call()->_orig_addr << "C";

        if (!command(KHOMP_LOG, CM_DIAL_DTMF, dial_bina.str().c_str()))
        {
            return ksFail;
            //throw call_error("something went while sending BINA digits to FXS branch");
        }

        int timeout = 150;

        if(!loopWhileFlagTimed(Kflags::WAIT_SEND_DTMF, timeout))
            return ksFail;
        //throw call_error("call has been dropped while sending digits");

        if(timeout <= 0)
            return ksFail;
        //throw call_error("sending number of A caused timeout of this call");
    }

    if(!call()->_orig_addr.empty())
        params += STG(FMT(" orig_addr=\"%s\"") % _call->_orig_addr);

    if (callFXS()->_ring_on != -1)
        params += STG(FMT(" ring_on=\"%ld\"") % callFXS()->_ring_on);

    if (callFXS()->_ring_off != -1)
        params += STG(FMT(" ring_off=\"%ld\"") % callFXS()->_ring_off);

    if (callFXS()->_ring_on_ext != -1)
        params += STG(FMT(" ring_on_ext=\"%ld\"") % callFXS()->_ring_on_ext);

    if (callFXS()->_ring_off_ext != -1)
        params += STG(FMT(" ring_off_ext=\"%ld\"") % callFXS()->_ring_off_ext);

    int ret = KhompPvt::makeCall(params);

    if(ret == ksSuccess)
    {
        try
        {
            switch_channel_mark_ring_ready(getFSChannel());
        //signal_state(AST_CONTROL_RINGING);
        }
        catch(Board::KhompPvt::InvalidSwitchChannel & err)
        {
            LOG(ERROR, PVT_FMT(_target, "No valid channel: %s") % err._msg.c_str());
        }
    }
    else
    {
        LOG(ERROR, PVT_FMT(target(), "Fail on make call"));
    }

    call()->_cleanup_upon_hangup = (ret == ksInvalidParams || ret == ksInvalidState);

    DBG(FUNC,PVT_FMT(_target, "(FXS) r"));
    return ret;
}

bool BoardE1::KhompPvtFXS::doChannelAnswer(CommandRequest &cmd)
{
    DBG(FUNC, PVT_FMT(_target, "(FXS) c"));
    
    bool ret = true;

    try
    {
        ScopedPvtLock lock(this);

        setupConnection();

        ret = KhompPvt::doChannelAnswer(cmd);
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR,PVT_FMT(_target, "(FXS) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }
    catch (Board::KhompPvt::InvalidSwitchChannel & err)
    {
        LOG(ERROR, PVT_FMT(target(), "r (%s)") % err._msg.c_str() );
        return false;
    }

    DBG(FUNC, PVT_FMT(_target, "(FXS) r"));

    return ret;
}

bool BoardE1::KhompPvtFXS::doChannelHangup(CommandRequest &cmd)
{
    DBG(FUNC, PVT_FMT(_target, "(FXS) c"));
    
    bool ret = true;

    bool answered     = true;
    bool disconnected = false;

    try
    {
        ScopedPvtLock lock(this);
       
        /*
        if(!callFXS()->_uuid_other_session.empty())
        {
            DBG(FUNC,PVT_FMT(_target, "unable to transfer"));
            
            switch_core_session_t *hold_session;

            if ((hold_session = switch_core_session_locate(callFXS()->_uuid_other_session.c_str()))) 
            {
                switch_channel_t *hold_channel = switch_core_session_get_channel(hold_session);
                switch_core_session_rwunlock(hold_session);

                if(hold_channel)
                    switch_channel_hangup(hold_channel, (switch_call_cause_t)call()->_hangup_cause);
                //switch_channel_hangup(hold_channel, SWITCH_CAUSE_ATTENDED_TRANSFER);
            }
            callFXS()->_uuid_other_session.clear();            
        }
        */

        if (call()->_flags.check(Kflags::IS_INCOMING))
        {
            DBG(FUNC,PVT_FMT(_target, "disconnecting incoming channel"));

        }
        else if (call()->_flags.check(Kflags::IS_OUTGOING))
        {
            if (!call()->_flags.check(Kflags::FXS_OFFHOOK))
            {
                DBG(FUNC, PVT_FMT(_target, "disconnecting not answered outgoing FXS channel..."));
                disconnected = command(KHOMP_LOG, CM_DISCONNECT);
                cleanup(KhompPvt::CLN_HARD);
                answered = false;

            }
            else if(call()->_cleanup_upon_hangup)
            {
                DBG(FUNC,PVT_FMT(_target, "disconnecting not allocated outgoing channel..."));

                disconnected = command(KHOMP_LOG, CM_DISCONNECT);
                cleanup(KhompPvt::CLN_HARD);
                answered = false;

            }
            else
            {
                DBG(FUNC,PVT_FMT(_target, "disconnecting outgoing channel...")); 

                disconnected = command(KHOMP_LOG, CM_DISCONNECT);
            }
        }
        else
        {
            DBG(FUNC,PVT_FMT(_target, "already disconnected"));
            return true;
        }

        if(answered)
        {
            indicateBusyUnlocked(SWITCH_CAUSE_USER_BUSY, disconnected);
        }

        if (call()->_flags.check(Kflags::IS_INCOMING) && !call()->_flags.check(Kflags::NEEDS_RINGBACK_CMD))
        {
            DBG(FUNC,PVT_FMT(_target, "disconnecting incoming channel..."));
            disconnected = command(KHOMP_LOG, CM_DISCONNECT);
        }

        stopStream();

        stopListen();

        //ret = KhompPvt::doChannelHangup(cmd);
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR,PVT_FMT(_target, "(FXS) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }

    DBG(FUNC, PVT_FMT(_target, "(FXS) r"));

    return ret;
}

bool BoardE1::KhompPvtFXS::setupConnection()
{
    if(!call()->_flags.check(Kflags::IS_INCOMING) && !call()->_flags.check(Kflags::IS_OUTGOING))
    {
        DBG(FUNC,PVT_FMT(_target, "Channel already disconnected"));
        return false;
    }

    bool res_out_of_band_dtmf = (call()->_var_dtmf_state == T_UNKNOWN ?
        Opt::_options._suppression_delay() && Opt::_options._out_of_band_dtmfs(): (call()->_var_dtmf_state == T_TRUE));

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

        DBG(FUNC, PVT_FMT(_target, "(FXS) Audio callbacks initialized successfully"));
    }

    call()->_flags.set(Kflags::FXS_OFFHOOK);

    return KhompPvt::setupConnection();
}

bool BoardE1::KhompPvtFXS::indicateBusyUnlocked(int cause, bool sent_signaling)
{
    DBG(FUNC, PVT_FMT(_target, "(FXS) c"));

    if(!KhompPvt::indicateBusyUnlocked(cause, sent_signaling))
    {
        DBG(FUNC, PVT_FMT(_target, "(FXS) r (false)"));
        return false;
    }

    if(call()->_flags.check(Kflags::IS_INCOMING))
    {
        if(!call()->_flags.check(Kflags::CONNECTED) && !sent_signaling)
        {
            if(!call()->_flags.check(Kflags::HAS_PRE_AUDIO))
            {
                int rb_value = callFailFromCause(call()->_hangup_cause);
                DBG(FUNC, PVT_FMT(target(), "sending the busy status"));

                if (sendRingBackStatus(rb_value) == RingbackDefs::RBST_UNSUPPORTED)
                {
                    DBG(FUNC, PVT_FMT(target(), "falling back to audio indication!"));
                    /* stop the line audio */
                    stopStream();

                    /* just pre connect, no ringback */
                    if (!sendPreAudio())
                        DBG(FUNC, PVT_FMT(target(), "everything else failed, just sending audio indication..."));

                    /* be very specific about the situation. */
                    mixer(KHOMP_LOG, 1, kmsGenerator, kmtBusy);
                }
            }
            else
            {
                DBG(FUNC, PVT_FMT(target(), "going to play busy"));

                /* stop the line audio */
                stopStream();

                /* be very specific about the situation. */
                mixer(KHOMP_LOG, 1, kmsGenerator, kmtBusy);

            }
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

    DBG(FUNC,PVT_FMT(_target, "(FXS) r"));

    return true;
}

void BoardE1::KhompPvtFXS::reportFailToReceive(int fail_code)
{
    KhompPvt::reportFailToReceive(fail_code);

    if(fail_code != -1)
    {
        DBG(FUNC,PVT_FMT(_target, "sending a 'unknown number' message/audio")); 

        if(sendRingBackStatus(fail_code) == RingbackDefs::RBST_UNSUPPORTED)
        {
            sendPreAudio(RingbackDefs::RB_SEND_DEFAULT);
            startCadence(PLAY_FASTBUSY);
        }
    }
    else
    {
        DBG(FUNC, PVT_FMT(_target, "sending fast busy audio directly"));

        sendPreAudio(RingbackDefs::RB_SEND_DEFAULT);
        startCadence(PLAY_FASTBUSY);
    }
}

bool BoardE1::KhompPvtFXS::validContexts(
        MatchExtension::ContextListType & contexts, std::string extra_context)
{
    DBG(FUNC,PVT_FMT(_target, "(FXS) c"));
    
    if(!_context.empty())
        contexts.push_back(_context);

    if(!_group_context.empty())
    {
        contexts.push_back(_group_context);
    }

    contexts.push_back(Opt::_options._context_fxs());
    contexts.push_back(Opt::_options._context2_fxs());

    for (MatchExtension::ContextListType::iterator i = contexts.begin(); i != contexts.end(); i++)
    {
        replaceTemplate((*i), "CC", _target.object);
    }

    bool ret = Board::KhompPvt::validContexts(contexts,extra_context);

    DBG(FUNC,PVT_FMT(_target, "(FXS) r"));

    return ret;
}

bool BoardE1::KhompPvtFXS::isOK()
{
    try
    {
        ScopedPvtLock lock(this);

        K3L_CHANNEL_STATUS status;

        if (k3lGetDeviceStatus (_target.device, _target.object + ksoChannel, &status, sizeof (status)) != ksSuccess)
            return false;

        return (status.AddInfo != kfxsFail);
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(target(), "unable to lock %s!") % err._msg.c_str() );
    }

    return false;
}


