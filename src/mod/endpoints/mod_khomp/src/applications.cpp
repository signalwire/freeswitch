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

#include "applications.h"
#include "lock.h"


bool Fax::adjustForFax()
{
    /* Don't worry, let the man work */
    DBG(FUNC, PVT_FMT(_pvt->target(),"Channel is now being adjusted for fax!"));
    
    try
    {
        ScopedPvtLock lock(_pvt);

        _pvt->echoCancellation(false);
        _pvt->dtmfSuppression(false);
        _pvt->autoGainControl(false);

        _pvt->call()->_input_volume = 0;
        _pvt->setVolume("input" , 0);
        _pvt->call()->_output_volume = 0;
        _pvt->setVolume("output", 0);
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_pvt->target(), "r (unable to lock: %s!)") %  err._msg.c_str());
        return false;
    }

    return true;
}

bool Fax::sendFax(switch_core_session_t * session, const char *data)
{
    DBG(FUNC, PVT_FMT(_pvt->target(), "c (%s)") % data);

    try
    {
        switch_channel_t *channel = _pvt->getFSChannel(session);

        switch_channel_set_variable(channel, "KFaxSent", "no");
        switch_channel_set_variable(channel, "KFaxResult", "none");

        std::string           fax_string((const char *)data);
        Strings::vector_type  fax_args;
        Strings::vector_type  fax_files;

        Strings::tokenize(fax_string, fax_args, "|,");

        if(fax_args.size() != 1 && fax_args.size() != 2)
        {
            LOG(ERROR, PVT_FMT(_pvt->target(), "r (invalid string '%s': wrong number of separators.)") % fax_string);
            return false;
        }

        Strings::tokenize(fax_args[0], fax_files, ":");

        if(fax_files.size() <= 0)
        {
            LOG(ERROR, PVT_FMT(_pvt->target(), "r (invalid string '%s': wrong number of separators.)") % fax_string);
            return false;
        }

        ScopedPvtLock lock(_pvt);

        std::string id;
        
        if(fax_args.size() == 2)
        {
            id = fax_args[1];
        }
        else if(!_pvt->call()->_orig_addr.empty())
        {
            id = _pvt->call()->_orig_addr;
        }

        const char * orig_addr = id.empty() ? NULL : id.c_str();

        int timeout = 5;

        if(!_pvt->call()->_flags.check(Kflags::REALLY_CONNECTED) && !_pvt->loopWhileFlagTimed(Kflags::REALLY_CONNECTED, timeout, false))
            return false;
        
        if(!startFaxTX(orig_addr))
        {
            LOG(ERROR, PVT_FMT(_pvt->target(), "r (unable to start send fax)"));
            return false;
        }

        for(unsigned int i = 0; i < fax_files.size() ; ++i)
        {
            if (!addFaxFile(fax_files[i].c_str(), (i == fax_files.size()-1)? true : false))
            {
                DBG(FUNC, D("unable to add fax file='%s'")
                        % fax_files[i].c_str());
            }
        }
        
        _fax_cond.reset();

        lock.unlock();

        _fax_cond.wait();
        
        switch_channel_set_variable(channel, "KFaxSent", ((_fax_result == kfaxrEndOfTransmission) ? "yes" : "no"));
        switch_channel_set_variable(channel, "KFaxResult", (Verbose::faxResult((KFaxResult)_fax_result).c_str()));

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_pvt->target(), "r (unable to lock: %s!)") %  err._msg.c_str());
        return false;
    }
    catch(Board::KhompPvt::InvalidSwitchChannel & err)
    {
        LOG(ERROR, PVT_FMT(_pvt->target(), "r (%s)") % err._msg.c_str());
        return false;
    }

    DBG(FUNC, PVT_FMT(_pvt->target(), "r"));
    return true;
}


bool Fax::receiveFax(switch_core_session_t * session, const char *data)
{
    DBG(FUNC, PVT_FMT(_pvt->target(), "c (%s)") % data);

    try
    {
        switch_channel_t *channel = _pvt->getFSChannel(session);

        switch_channel_set_variable(channel, "KFaxReceived", "no");
        switch_channel_set_variable(channel, "KFaxResult", "none");

        std::string          fax_string((const char *)data);
        Strings::vector_type fax_args;

        Strings::tokenize(fax_string, fax_args, "|,");

        if(fax_args.size() != 1 && fax_args.size() != 2)
        {
            LOG(ERROR, PVT_FMT(_pvt->target(), "r (invalid string '%s': wrong number of separators.)") % fax_string);
            return false;
        }

        ScopedPvtLock lock(_pvt);

        std::string id;

        if(fax_args.size() == 2)
        {
            id = fax_args[1];
        }
        else if(!_pvt->call()->_dest_addr.empty())
        {
            id = _pvt->call()->_dest_addr;
        }

        const char * orig_addr = id.empty() ? NULL : id.c_str();

        int timeout = 5;

        if(!_pvt->call()->_flags.check(Kflags::REALLY_CONNECTED) && !_pvt->loopWhileFlagTimed(Kflags::REALLY_CONNECTED, timeout, false))
            return false;

        if(!startFaxRX(fax_args[0].c_str(), orig_addr))
        {
            LOG(ERROR, PVT_FMT(_pvt->target(), "r (unable to start receive fax)"));
            return false;
        }
        
        _fax_cond.reset();

        lock.unlock();

        _fax_cond.wait();

        switch_channel_set_variable(channel, "KFaxReceived", ((_fax_result == kfaxrEndOfReception) ? "yes" : "no"));
        switch_channel_set_variable(channel, "KFaxResult", (Verbose::faxResult((KFaxResult)_fax_result).c_str()));

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_pvt->target(), "r (unable to lock: %s!)") %  err._msg.c_str());
        return false;
    }
    catch(Board::KhompPvt::InvalidSwitchChannel & err)
    {
        LOG(ERROR, PVT_FMT(_pvt->target(), "r (%s)") % err._msg.c_str());
        return false;
    }
    
    DBG(FUNC, PVT_FMT(_pvt->target(), "r"));

    return true;
}

bool Fax::onFaxChannelRelease(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_pvt->target(), "c"));

    try
    {
        ScopedPvtLock lock(_pvt);

        if (_pvt->call()->_flags.check(Kflags::FAX_SENDING))
        {
            _pvt->call()->_flags.clear(Kflags::FAX_SENDING);

            /* make audio come back.. */
            _pvt->startStream(false);
            _pvt->startListen();
        }
        else if (_pvt->call()->_flags.check(Kflags::FAX_RECEIVING))
        {
            _pvt->call()->_flags.clear(Kflags::FAX_RECEIVING);

            /* make audio come back.. */
            _pvt->startStream(false);
            _pvt->startListen();
        }

        _fax_result = (KFaxResult)e->AddInfo;

        _fax_cond.signal();

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_pvt->target(), "r (unable to lock %s!)") % err._msg.c_str());
        return false;
    };

    DBG(FUNC, PVT_FMT(_pvt->target(), "r"));

    return true;
}

bool Fax::startFaxTX(const char * orig_addr)
{
    /* no audio after this point, so stop streams! */
    _pvt->stopStream(false);
    _pvt->stopListen();

    std::string id;

    if(orig_addr)
        id = STG(FMT("orig_addr=\"%s\"") % orig_addr);

    const char * param = id.empty() ? NULL : id.c_str();


    if (!_pvt->command(KHOMP_LOG, CM_START_FAX_TX, param))
    {
        _pvt->startStream(false);
        _pvt->startListen();
        return false;
    }

    _pvt->call()->_flags.set(Kflags::FAX_SENDING);

    return true;

}

bool Fax::stopFaxTX(void)
{
    /* EV_FAX_CHANNEL_FREE , EV_FAX_FILE_FAIL */
    if (!_pvt->command(KHOMP_LOG, CM_STOP_FAX_TX))
        return false;

    _pvt->call()->_flags.clear(Kflags::FAX_SENDING);

    /* make audio come back.. */
    _pvt->startStream(false);
    _pvt->startListen();

    return true;
}



bool Fax::startFaxRX(const char * filename, const char * orig_addr)
{
    if(!filename)
        return false;

    std::string param = STG(FMT("filename=\"%s\"") % filename);

    if(orig_addr)
        param += STG(FMT(" orig_addr=\"%s\"") % orig_addr);

    /* no audio after this point, so stop streams! */
    _pvt->stopStream(false);
    _pvt->stopListen();

    if (!_pvt->command(KHOMP_LOG, CM_START_FAX_RX, param.c_str()))
    {
        _pvt->startStream(false);
        _pvt->startListen();
        return false;
    }

    _pvt->call()->_flags.set(Kflags::FAX_RECEIVING);

    return true;
}

bool Fax::stopFaxRX()
{
    /* Events: EV_FAX_CHANNEL_FREE */
    if (!_pvt->command(KHOMP_LOG, CM_STOP_FAX_RX))
        return false;

    _pvt->call()->_flags.clear(Kflags::FAX_RECEIVING);

    /* make audio come back.. */
    _pvt->startStream(false);
    _pvt->startListen();

    return true;
}

bool Fax::addFaxFile(const char * filename, bool last)
{
    /* Events: EV_FAX_FILE_SENT , EV_FAX_FILE_FAIL , EV_FAX_TX_TIMEOUT */
    if(!filename)
        return false;

    std::string params = STG(FMT("filename=\"%s\" last=\"%s\"") % filename
            % (last ? "true": "false"));

    if (!_pvt->command(KHOMP_LOG, CM_ADD_FAX_FILE, params.c_str()))
        return false;

    return true;
}

/******************************************************************************/
SMS::_SMSEvent SMS::SMSEvent;

bool SMS::justAlloc(unsigned int count)
{
    /* incoming contexts */
    MatchExtension::ContextListType contexts;

    contexts.push_back(Opt::_options._context_gsm_sms());

    /* temporary variables */
    std::string context;
    std::string exten("s");

    const K3L_DEVICE_CONFIG & dev_cfg = Globals::k3lapi.device_config(_pvt->_target);

    for (MatchExtension::ContextListType::iterator i = contexts.begin(); i != contexts.end(); i++)
    {
        replaceTemplate((*i), "DD", _pvt->_target.device);
        replaceTemplate((*i), "CC", _pvt->_target.object);
        replaceTemplate((*i), "SSSS", atoi(dev_cfg.SerialNumber));
    }

    //switch(_pvt->findExtension(exten, context, contexts, _got_sms._type, _got_sms._from, false, true))
    switch(MatchExtension::findExtension(exten, context, contexts, exten, _got_sms._from, false, true))
    {
    case MatchExtension::MATCH_NONE:
        if( _got_sms._type != "broadcast")
        {
            LOG(WARNING, PVT_FMT(_pvt->target(), "unable to find context/exten for incoming SMS (s/%s), processing disabled for this channel.")
                    % (contexts.size() >= 1 ? contexts[0] : "default"));

        }

        return false;

    default:
        DBG(FUNC, PVT_FMT(_pvt->target(), "our: context '%s', exten '%s'") % context % exten);
        break;
    }

    for(unsigned int c = 0; c < count; c++)
    {

#if SWITCH_LESS_THAN(1,0,6)
        switch_core_session_t *session = switch_core_session_request(Globals::khomp_sms_endpoint_interface, SWITCH_CALL_DIRECTION_INBOUND, NULL);
#else
        switch_core_session_t *session = switch_core_session_request(Globals::khomp_sms_endpoint_interface, SWITCH_CALL_DIRECTION_INBOUND, SOF_NONE, NULL);
#endif

        if(!session)
        {
            LOG(ERROR, PVT_FMT(_pvt->target(), "Initilization Error, session not created!"));
            return false;
        }

        switch_core_session_add_stream(session, NULL);

        switch_core_session_set_private(session, _pvt);

        switch_caller_profile_t *caller_profile = switch_caller_profile_new(switch_core_session_get_pool(session),
                "Khomp_SMS",                       //username
                Opt::_options._dialplan().c_str(), //dialplan
                NULL,                              //caller_id_name
                _got_sms._from.c_str(),            //caller_id_number
                NULL,                              //network_addr
                _got_sms._from.c_str(),            //ani
                NULL,                              //aniii
                NULL,                              //rdnis
                (char *) "mod_khomp",              //source
                context.c_str(),                   //context
                exten.c_str());                    //destination_number

        if(!caller_profile)
        {
            _pvt->destroy(session);        
            LOG(ERROR, PVT_FMT(_pvt->target(), "r (Cannot create caller profile)"));
            return false;
        }

        switch_channel_t *channel = switch_core_session_get_channel(session);

        if(!channel)
        {
            _pvt->destroy(session);
            LOG(ERROR, PVT_FMT(_pvt->target(), "r (Cannot get channel)"));
            return false;
        }

        std::string name = STG(FMT("Khomp_SMS/%d/%d")
                % _pvt->target().device
                % _pvt->target().object);

        switch_channel_set_name(channel, name.c_str());
        switch_channel_set_caller_profile(channel, caller_profile);
    
        //DBG(FUNC, PVT_FMT(_pvt->target(), "Connect inbound SMS channel %s") % name.c_str());

        switch_channel_set_state(channel, CS_INIT);


        _owners.push_front(session);
    }
    
    return true;
}

bool SMS::justStart()
{
    OwnersList::iterator i = _owners.begin();

    if(i == _owners.end())
    {
        LOG(ERROR, PVT_FMT(_pvt->target(), "Cannot get session"));
        return false;
    }

    switch_core_session_t *session = (*i);

    _owners.pop_front();

    if(!session)
    {
        LOG(ERROR, PVT_FMT(_pvt->target(), "Cannot get session"));
        return false;
    }

    switch_channel_t *channel = switch_core_session_get_channel(session);

    if(!channel)
    {
        _pvt->destroy(session);
        LOG(ERROR, PVT_FMT(_pvt->target(), "r (Cannot get channel)"));
        return false;
    }

    switch_channel_set_variable(channel, "KSmsType", _got_sms._type.c_str());

    if (_got_sms._type == "message" || _got_sms._type == "confirm")
    {
        switch_channel_set_variable(channel, "KSmsFrom", _got_sms._from.c_str());
        switch_channel_set_variable(channel, "KSmsDate", _got_sms._date.c_str());
    }

    if(_got_sms._type == "confirm")
    {
        switch_channel_set_variable(channel, "KSmsDelivery", _got_sms._sc_date.c_str());
        switch_channel_set_variable(channel, "KSmsStatus", _got_sms._status.c_str());
    }
    else
    {
        if(_got_sms._type == "broadcast")
        {
            switch_channel_set_variable(channel, "KSmsSerial", _got_sms._serial.c_str());
            switch_channel_set_variable(channel, "KSmsPage", _got_sms._page.c_str());
            switch_channel_set_variable(channel, "KSmsPages", _got_sms._pages.c_str());
        }
        switch_channel_set_variable(channel, "KSmsSize", _got_sms._size.c_str());
        switch_channel_set_variable(channel, "KSmsMode", _got_sms._coding.c_str());
        switch_channel_set_variable(channel, "KSmsBody", _got_sms._body.c_str());
    }


    if (switch_core_session_thread_launch(session) != SWITCH_STATUS_SUCCESS)
    {
        _pvt->destroy(session);
        LOG(ERROR, PVT_FMT(_pvt->target(), "Error spawning thread"));
        return false;
    }

    return true;
}

bool SMS::sendSMS(switch_core_session_t * session, const char *data)
{
    DBG(FUNC, PVT_FMT(_pvt->target(), "(SMS) c"));

    volatile bool finished = false;
    volatile KGsmCallCause result = kgccNone; 

    try
    {
        //ScopedAllocLock alloc_lock;

        if(!_can_send)
        {
            LOG(ERROR, PVT_FMT(_pvt->target(), "(SMS) r (cannot send SMS messages, modem NOT initialized!)"));
            return false;
        }

        std::string           sms_string((const char *)data);
        Strings::vector_type  sms_args;

        Strings::tokenize(sms_string, sms_args, "/|,", 3);

        if (sms_args.size() != 3)
        {
            LOG(ERROR, PVT_FMT(_pvt->target(), "(SMS) r (invalid dial string '%s': wrong number of separators.)") % sms_string);
            return false;
        }

        std::string dest(sms_args[1]);

        bool conf = false;

        if (dest[0] == '!')
        {
            dest.erase(0,1);
            conf = true;
        }

        if (dest[dest.size()-1] == '!')
        {
            dest.erase(dest.size()-1,1);
            conf = true;
        }

        // get options/values 
        
        _send_sms._dest = dest;
        _send_sms._conf = conf;
        _send_sms._body = sms_args[2];

        Request request_sms(_send_sms, &finished, &result);

        _mutex.lock();
        bool status = _buffer.provide(request_sms);
        _mutex.unlock();

        _pvt->_mutex.unlock();

        if(status)
            _cond.signal();
    
        while (!finished)
        {
            usleep(200000);
        }

        _pvt->_mutex.lock();

        switch_channel_t *channel = _pvt->getFSChannel(session);

        switch_channel_set_variable(channel, "KSmsDelivered", ((KGsmSmsCause)result == ((KGsmSmsCause)0) ? "yes" : "no"));
        switch_channel_set_variable(channel, "KSmsErrorCode", STG(FMT("%d") % result).c_str());
        switch_channel_set_variable(channel, "KSmsErrorName", ((KGsmSmsCause)result == ((KGsmSmsCause)0) ? "None" : Verbose::gsmSmsCause((KGsmSmsCause)result).c_str()));

    }
    catch(ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_pvt->target(), "(SMS) r (unable to global alloc lock)"));
        return false;
    }
    catch(Board::KhompPvt::InvalidSwitchChannel & err)
    {
        LOG(ERROR, PVT_FMT(_pvt->target(), "(SMS) %s") % err._msg.c_str());
        //return false;
    }

    bool ret = ((KGsmSmsCause)result == ((KGsmSmsCause)0));

    DBG(FUNC, PVT_FMT(_pvt->target(), "(SMS) r (%s)") % (ret ? "true" : "false"));

    //return (KGsmSmsCause)result;
    return ret;
}

bool SMS::onNewSMS(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_pvt->target(), "(SMS) c"));

    bool ret = true;

    try
    {
        ScopedPvtLock lock(_pvt);

        if(!_can_receive)
        {
            DBG(FUNC, PVT_FMT(_pvt->target(), "(SMS) received new SMS message(s), but receiving is disabled. keeping the message(s) at the SIM card."));
            ret = false;
        }
        else if(!justAlloc(e->AddInfo))
        {
            LOG(WARNING, PVT_FMT(_pvt->target(), "(SMS) unable to allocate channel for new SMS message(s). disabling processing to prevent messages from being lost."));
            _can_receive = false;
            ret = false;
        }
        else
        {

            DBG(FUNC, PVT_FMT(_pvt->target(), "downloading %d SMS message(s) on the SIM card.") % e->AddInfo);

            _pvt->command(KHOMP_LOG, CM_GET_SMS);
        }

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_pvt->target(), "(SMS) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }

    DBG(FUNC, PVT_FMT(_pvt->target(), "(SMS) r"));

    return ret;
}

bool SMS::onSMSInfo(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_pvt->target(), "(SMS) c"));

    try
    {
        ScopedPvtLock lock(_pvt);

        Globals::k3lapi.get_param(e, "sms_type",    _got_sms._type);
        Globals::k3lapi.get_param(e, "sms_from",    _got_sms._from);
        Globals::k3lapi.get_param(e, "sms_date",    _got_sms._date);
        Globals::k3lapi.get_param(e, "sms_size",    _got_sms._size);
        Globals::k3lapi.get_param(e, "sms_coding",  _got_sms._coding);
        Globals::k3lapi.get_param(e, "sms_serial",  _got_sms._serial);
        Globals::k3lapi.get_param(e, "sms_id",      _got_sms._id);
        Globals::k3lapi.get_param(e, "sms_page",    _got_sms._page);
        Globals::k3lapi.get_param(e, "sms_pages",   _got_sms._pages);
        Globals::k3lapi.get_param(e, "sms_sc_date", _got_sms._sc_date);
        Globals::k3lapi.get_param(e, "sms_status",  _got_sms._status);

        //DBG(FUNC, PVT_FMT(_pvt->target(), "(SMS) type=%s from=%s date=%s size=%s coding=%s serial=%s id=%s page=%s pages=%s sc_date=%s status=%s") % _got_sms._type % _got_sms._from % _got_sms._date % _got_sms._size % _got_sms._coding % _got_sms._serial % _got_sms._id % _got_sms._page % _got_sms._pages % _got_sms._sc_date % _got_sms._status);

        if (_owners.empty() && !justAlloc(1) && (_got_sms._type.compare("broadcast") != 0))
        {
            // this error is fatal
            LOG(ERROR, PVT_FMT(_pvt->target(), "unable to allocate channel, new SMS message from %s will not be sent to dialplan!") % _got_sms._from);
            return false;
        }
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_pvt->target(), "(SMS) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }

    DBG(FUNC, PVT_FMT(_pvt->target(), "(SMS) r"));

    return true;
}

bool SMS::onSMSData(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_pvt->target(), "(SMS) c"));

    try
    {
        ScopedPvtLock lock(_pvt);

        if (_owners.empty())
        {
            if(_got_sms._type != "broadcast")
            {
                LOG(WARNING, PVT_FMT(_pvt->target(), "unable to allocate channel for new SMS message(s). disabling processing to prevent messages from being lost."));
                _can_receive = false;
            }
        }
        else
        {
            _got_sms._body = (const char *)(e->Params ? e->Params : "");

            SMSEvent(_pvt, _got_sms);

            if(!justStart())
            {
                if(_got_sms._type != "broadcast")
                {
                    LOG(ERROR, PVT_FMT(_pvt->target(), "unable to receive SMS from '%s', something wrong!") % _got_sms._from);
                    LOG(ERROR, PVT_FMT(_pvt->target(), "disabling SMS processing to prevent messages from being lost."));
    
                    _can_receive = false;
                }
            }
        }
        
        if(_can_receive)
        {
            /* stats update! */
            if(_got_sms._type == "message")
            {
                statistics<SMSStatistics>()->incrementIncoming();
            }
            else if(_got_sms._type == "confirm")
            {
                statistics<SMSStatistics>()->incrementConfirm();
            }
            else
            {
                statistics<SMSStatistics>()->incrementBroadcast();
            }
        }
   
        /* reset data stuff */
        _got_sms.clear();
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_pvt->target(), "(SMS) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }

    DBG(FUNC, PVT_FMT(_pvt->target(), "(SMS) r"));
    return true;
}

bool SMS::onSMSSendResult(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_pvt->target(), "(SMS) c"));

    try
    {
        ScopedPvtLock lock(_pvt);

        _result = (KGsmCallCause)e->AddInfo;
        _pvt->call()->_flags.clear(Kflags::SMS_DOING_UPLOAD);

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_pvt->target(), "(SMS) r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }

    DBG(FUNC, PVT_FMT(_pvt->target(), "(SMS) r"));

    return true;
}

int SMS::smsThread(void * sms_ptr)
{
    SMS * sms = static_cast < SMS * > (sms_ptr);
    
    Board::KhompPvt * pvt = static_cast < Board::KhompPvt * > (sms->_pvt);

    DBG(THRD, PVT_FMT(pvt->target(), "c"));

    bool loop = true;

    while(loop)
    {
        if(sms->_cond.wait(1000))
        {
            if(sms->_shutdown)
                return 0;
        }

        try
        {
            ScopedPvtLock lock(pvt);

            K3L_CHANNEL_STATUS status;

            KLibraryStatus ret = (KLibraryStatus) k3lGetDeviceStatus(
                    pvt->_target.device,
                    pvt->_target.object + ksoChannel,
                    &status,
                    sizeof(status));

            if (ret == ksSuccess)
            {
                switch (status.AddInfo)
                {
                    case kgsmModemError:
                    case kgsmSIMCardError:
                        return 0;

                    case kgsmIdle:
                    {
                        /* pede registro do canal.. */
                        pvt->command(KHOMP_LOG, CM_SEND_TO_MODEM, "AT+COPS?");

                        /* pede estado da antena.. */
                        pvt->command(KHOMP_LOG, CM_SEND_TO_MODEM, "AT+CSQ?");

                        /* forca tipo padrao a ser message... */
                        sms->_got_sms._type = "message";

                        /* pre-aloca canal para receber mensagens SMS. */
                        if (sms->justAlloc())
                        {
                            /* habilita processamento de SMSs entrantes */
                            sms->_can_receive = true;

                            /* envia comando para "limpar" SMSs do SIM card */
                            pvt->command(KHOMP_LOG, CM_CHECK_NEW_SMS);
                        }

                        sms->_got_sms.clear();

                        /* sai fora do loop! */
                        loop = false;
                    }
                    default:
                        break;
                }
            }
        }
        catch (ScopedLockFailed & err)
        {
            DBG(FUNC, PVT_FMT(pvt->_target, "unable to obtain lock: %s") % err._msg.c_str());
        }

    }

    sms->_can_send = true;

    while(true)
    {
        Request request_sms;

        DBG(THRD, PVT_FMT(pvt->_target, "begin"));

        while(!sms->_buffer.consume(request_sms))
        {
            DBG(THRD, PVT_FMT(pvt->_target, "buffer empty"));

            sms->_cond.wait();

            DBG(THRD, PVT_FMT(pvt->_target, "waked up!"));

            if(sms->_shutdown)
                return 0;
        }

        DBG(THRD, PVT_FMT(pvt->_target, "processing buffer..."));

        try
        {
            int timeout = 30;

            ScopedPvtLock lock(pvt);

            pvt->call()->_flags.set(Kflags::SMS_DOING_UPLOAD);

            if (pvt->command(KHOMP_LOG, CM_PREPARE_SMS, request_sms._send_sms._body.c_str()))
            {
                std::string extra = (request_sms._send_sms._conf ? " sms_confirm=\"TRUE\"" : "");

                if (pvt->command(KHOMP_LOG, CM_SEND_SMS,
                            STG(FMT("sms_to=\"%s\"%s") % request_sms._send_sms._dest % extra).c_str()))
                {
                    if (!pvt->loopWhileFlagTimed(Kflags::SMS_DOING_UPLOAD, timeout))
                        break;
                }
                else
                {
                    sms->_result = kgccResourceUnavailable;
                }
            }
            else
            {
                sms->_result = kgccInvalidMessage;
            }
            
            pvt->call()->_flags.clear(Kflags::SMS_DOING_UPLOAD);

            if (request_sms._cause)
                *(request_sms._cause) = (KGsmCallCause)sms->_result;

            if (request_sms._finished)
                *(request_sms._finished) = true;

            if (sms->_result == kgccNone)
            {
                sms->SMSEvent(pvt, sms->_send_sms);

                /* stats update if sent! */
                sms->statistics<SMSStatistics>()->incrementOutgoing();
            }

            sms->_send_sms.clear();

        }
        catch (ScopedLockFailed & err)
        {
            DBG(FUNC, PVT_FMT(pvt->_target, "unable to obtain lock: %s") % err._msg.c_str());
        }

        DBG(FUNC, PVT_FMT(pvt->target(), "ok, going to loop..."));

    }


    DBG(FUNC, PVT_FMT(pvt->target(), "r"));

    return 0;
}

