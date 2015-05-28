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

#include "khomp_pvt.h"
#include "lock.h"
#include "khomp_pvt_kxe1.h"
#include "khomp_pvt_gsm.h"
#include "khomp_pvt_fxo.h"
#include "khomp_pvt_passive.h"
#include "utils.h"

Board::VectorBoard  Board::_boards;
char                Board::_cng_buffer[Globals::cng_buffer_size];
Kommuter            Board::kommuter;


Board::KhompPvt::KhompPvt(K3LAPIBase::GenericTarget & target) :
  _target(target),
  _mutex(Globals::module_pool),
  _session(NULL),
  _caller_profile(NULL),
  _reader_frames(&_read_codec),
  _writer_frames(&_write_codec) 
{    
    _read_codec.implementation = NULL;
    _write_codec.implementation = NULL;

    _pvt_statistics = new PvtStatistics(this);

    _mohclass    = Opt::_options._global_mohclass();
    _language    = Opt::_options._global_language();
    _accountcode = Opt::_options._accountcode();
}

bool Board::initializeK3L(void)
{
    LOG(MESSAGE, "starting K3L API ..."); 

    /* Start the API and connect to KServer */
    k3lSetGlobalParam (klpResetFwOnStartup, 1);
    k3lSetGlobalParam (klpDisableInternalVoIP, 1);

    try
    {
        Globals::k3lapi.start();
    }
    catch (K3LAPI::start_failed & e)
    {
        LOG(ERROR,FMT("loading K3L API failed: %s") % e.what());
        return false;
    }

    LOG(MESSAGE, "the K3L API have been started!"); 
    
    return true;
}

bool Board::finalizeK3L(void)
{
    /* Stop the API and disconnect to KServer */
    try
    {
        Globals::k3lapi.stop();
    }
    catch(...)
    {
        LOG(ERROR, "K3L not stopped");
        return false;
    }
    LOG(MESSAGE, "K3L stopped.."); 
    return true;
}

bool Board::initializeHandlers(void)
{
    Globals::global_timer = new Globals::GlobalTimer;
    Globals::global_timer->start();

    if (Globals::k3lapi.device_count() == 0)
        return false;

    for (VectorBoard::iterator it_dev = _boards.begin();
                               it_dev != _boards.end();
                               it_dev++)
    {
        Board * device = *it_dev;
        device->_event_handler = new ChanEventHandler(device->id(), &eventThread);
        device->_command_handler = new ChanCommandHandler(device->id(), &commandThread);

        device->_timers.start();
    }

    k3lRegisterEventHandler( khompEventCallback );
    k3lRegisterAudioListener( NULL, khompAudioListener );

    LOG(MESSAGE, "K3l event and audio handlers registered."); 

    return true;
}

bool Board::finalizeHandlers(void)
{
    /* if timer still ticks, stop him */
    if(Globals::global_timer != NULL)
    {
        Globals::global_timer->stop();
        delete Globals::global_timer;
        Globals::global_timer = NULL;
    }

    if (Globals::k3lapi.device_count() == 0)
        return false;

    k3lRegisterEventHandler( NULL );
    k3lRegisterAudioListener( NULL, NULL );


    for (VectorBoard::iterator it_dev = _boards.begin();
                               it_dev != _boards.end();
                               it_dev++)
    {
        Board * device = *it_dev;        
        // stop event handler for device
        ChanEventHandler * evt_handler = device->_event_handler;
        evt_handler->fifo()->_shutdown = true;
        evt_handler->signal();
        delete evt_handler;
        device->_event_handler = NULL;
        // stop command handler for device
        ChanCommandHandler * cmd_handler = device->_command_handler;
        cmd_handler->fifo()->_shutdown = true;
        cmd_handler->signal();
        delete cmd_handler;
        device->_command_handler = NULL;
        // stop timer
        device->_timers.stop();
    }

    /* wait every thread to finalize */
    sleep(1);

    LOG(MESSAGE, "K3l event and audio handlers unregistered."); 

    return true;

}

void Board::initializeBoards(void)
{

    for (unsigned dev = 0; dev < Globals::k3lapi.device_count(); dev++)
    {
        LOG(MESSAGE,FMT("loading device %d..." ) % dev); 

        switch(Globals::k3lapi.device_type(dev))
        {
            case kdtE1:
            case kdtE1Spx:
            case kdtE1GW:
            case kdtE1IP:
            case kdtE1FXSSpx:
            case kdtFXS:
            case kdtFXSSpx:
                _boards.push_back(new BoardE1(dev));
                break;
            case kdtGSM:
            case kdtGSMSpx:
            case kdtGSMUSB:
            case kdtGSMUSBSpx:
                _boards.push_back(new BoardGSM(dev));
                break;
            case kdtFXO:
            case kdtFXOVoIP:
                switch(Globals::k3lapi.device_config(dev).DeviceModel)
                {
                case kdmFXO80:
                    _boards.push_back(new BoardFXO(dev));
                    break;
                default:
                    _boards.push_back(new BoardPassive(dev));
                    break;
                }
                break;
            case kdtPR:
                    _boards.push_back(new BoardPassive(dev));
                    break;
            default:
                _boards.push_back(new Board(dev));
                LOG(ERROR,FMT("device type %d unknown" ) %  Globals::k3lapi.device_type(dev)); 
                break;
        }
        
        _boards.back()->initializeChannels();
    }
}

void Board::initializeChannels(void)
{
    LOG(MESSAGE, "loading channels ..."); 

    for (unsigned obj = 0; obj < Globals::k3lapi.channel_count(_device_id); obj++)
    {
        K3LAPIBase::GenericTarget tgt(Globals::k3lapi, K3LAPIBase::GenericTarget::CHANNEL, _device_id, obj);

        KhompPvt * pvt;

		switch(Globals::k3lapi.channel_config(_device_id, obj).Signaling)
		{
            case ksigInactive:
            default:
                pvt = new Board::KhompPvt(tgt);
                pvt->_call = new Board::KhompPvt::Call();
                DBG(FUNC, FMT("signaling %d unknown") % Globals::k3lapi.channel_config(_device_id, obj).Signaling);
                break;
        }
        
        _channels.push_back(pvt);

        pvt->cleanup();
    }
}

void Board::finalizeBoards(void)
{
    LOG(MESSAGE, "finalizing boards ..."); 

    for (VectorBoard::iterator it_dev = _boards.begin();
                               it_dev != _boards.end();
                               it_dev++)
    {
        Board * & device_ref = *it_dev;
        Board *   device_ptr = device_ref;

        device_ptr->finalizeChannels();

        device_ref = (Board *) NULL;

        delete device_ptr;
    }

}

void Board::finalizeChannels()
{
    LOG(MESSAGE, "finalizing channels ..."); 
    for (VectorChannel::iterator it_obj = _channels.begin();
         it_obj != _channels.end();
         it_obj++)
    {
        KhompPvt * & pvt_ref = *it_obj;
        KhompPvt *   pvt_ptr = pvt_ref;

        if(!pvt_ptr)
            continue;

        try
        {
            ScopedPvtLock lock(pvt_ptr);

            if(pvt_ptr->session())
            {
                //TODO: Tratamento para desconectar do canal do FreeSwitch.
                pvt_ptr->cleanup();
            }

            delete pvt_ptr->_call;
            pvt_ptr->_call = NULL;

            pvt_ref = (KhompPvt *) NULL;
        }
        catch(...)
        {
            /* keep walking! */
        }

        delete pvt_ptr;
    }
}

void Board::initializeCngBuffer(void)
{
    bool turn = true;

    for (unsigned int i = 0; i < Globals::cng_buffer_size; i++)
    {
        _cng_buffer[i] = (turn ? 0xD5 : 0xD4);
        turn = !turn;
    }
}

bool Board::initialize(void)
{
    initializeCngBuffer();

    try
    {
       initializeBoards();
    }
    catch(K3LAPITraits::invalid_device & err)
    {
        LOG(ERROR, FMT("Invalid device at initialize boards: %s") %  err.what());
        return false;
    }
    catch(K3LAPITraits::invalid_channel & err)
    {
        LOG(ERROR, FMT("Invalid channel at initialize boards: %s") %  err.what());
        return false;
    }
    catch(K3LAPITraits::invalid_link & err)
    {
        LOG(ERROR, FMT("Invalid link at initialize boards: %s") %  err.what());
        return false;
    }

    return true;
}

bool Board::finalize(void)
{
    finalizeBoards();

    return finalizeK3L();
}

void Board::khomp_add_event_board_data(const K3LAPIBase::GenericTarget target, switch_event_t *event)
{

    //if (!event) {
        //TODO: RAISE!
    //}

    try
    {
        if (target.type == K3LAPIBase::GenericTarget::CHANNEL)
        {
            switch_core_session_t * s = get(target.device, target.object)->session();
            switch_channel_event_set_data(Board::KhompPvt::getFSChannel(s), event);
        }

        switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Khomp-DeviceId", "%u", target.device);
        switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Khomp-Object", "%u", target.object);
    }
    catch(K3LAPITraits::invalid_channel & err)
    {
        LOG(ERROR, PVT_FMT(target, "Invalid channel"));
    }
    catch(Board::KhompPvt::InvalidSwitchChannel & err)
    {
        LOG(ERROR, PVT_FMT(target, "No valid channel: %s") % err._msg.c_str());
    }
}

std::string Board::KhompPvt::getStatistics(Statistics::Type type)
{
    switch(type)
    {
        case Statistics::DETAILED:
        {
            return _pvt_statistics->getDetailed();
        }
        case Statistics::ROW:
        {
            return _pvt_statistics->getRow();
        }
        default:
            return "";
    }
}

switch_xml_t Board::KhompPvt::getStatisticsXML(Statistics::Type type)
{
    switch(type)
    {
        case Statistics::DETAILED:
        {
            return _pvt_statistics->getDetailedXML();
        }
        case Statistics::ROW:
        {
            return _pvt_statistics->getNode();
        }
        default:
            return NULL;
    }
}

std::string Board::KhompPvt::PvtStatistics::getDetailedRates()
{
    /* skip inactive channels */
    //if (_pvt->getSignaling() == ksigInactive) return "";

    /* buffer our data to return at the end */
    std::string strBuffer;

    /* this values come from kserver */
    unsigned int call_incoming = Globals::k3lapi.channel_stats(
            _pvt->target().device, _pvt->target().object, kcsiInbound);
    unsigned int call_outgoing = Globals::k3lapi.channel_stats(
            _pvt->target().device, _pvt->target().object, kcsiOutbound);

    float occupation_rate = 100.0;

    if (_pvt->call()->statistics()->_total_idle_time > 0)
    {
        occupation_rate = 100 * (
            _pvt->call()->statistics()->_total_time_incoming + 
            _pvt->call()->statistics()->_total_time_outgoing
            ) / (
            _pvt->call()->statistics()->_total_idle_time + 
            _pvt->call()->statistics()->_total_time_incoming + 
            _pvt->call()->statistics()->_total_time_outgoing);
    }

    strBuffer.append(STG(FMT("Occupation rate: \t\t%0.2f%%\n") % occupation_rate));

    if (call_incoming > 0) 
    {    
        std::string str_calls_incoming_mean = timeToString ( (time_t) (_pvt->call()->statistics()->_total_time_incoming / call_incoming) );
        strBuffer.append(STG(FMT("Mean duration time of incoming calls: %s\n") % str_calls_incoming_mean));
    }    

    if (call_outgoing > 0) 
    {    
        std::string str_calls_outgoing_mean = timeToString ( (time_t) (_pvt->call()->statistics()->_total_time_outgoing / call_outgoing) );
        strBuffer.append(STG(FMT("Mean duration time of outgoing calls: %s\n") % str_calls_outgoing_mean));
    }    

    return strBuffer;
}

switch_xml_t Board::KhompPvt::PvtStatistics::getDetailedRatesXML()
{
    switch_xml_t xrates = switch_xml_new("rates");

    /* this values come from kserver */
    unsigned int call_incoming = Globals::k3lapi.channel_stats(
            _pvt->target().device, _pvt->target().object, kcsiInbound);
    unsigned int call_outgoing = Globals::k3lapi.channel_stats(
            _pvt->target().device, _pvt->target().object, kcsiOutbound);

    float occupation_rate = 100.0;

    if (_pvt->call()->statistics()->_total_idle_time > 0)
    {
        occupation_rate = 100 * (
            _pvt->call()->statistics()->_total_time_incoming + 
            _pvt->call()->statistics()->_total_time_outgoing
            ) / (
            _pvt->call()->statistics()->_total_idle_time + 
            _pvt->call()->statistics()->_total_time_incoming + 
            _pvt->call()->statistics()->_total_time_outgoing);
    }

    switch_xml_t xoccupation = switch_xml_add_child_d(xrates,"oucpation",0);
    switch_xml_set_txt_d(xoccupation,STR(FMT("%d") % occupation_rate));

    if (call_incoming > 0) 
    {    
        std::string str_calls_incoming_mean = timeToString ( (time_t) (_pvt->call()->statistics()->_total_time_incoming / call_incoming) );
        switch_xml_t xmean_in = switch_xml_add_child_d(xrates,"incoming",0);
        switch_xml_set_txt_d(xmean_in,str_calls_incoming_mean.c_str());
    }    

    if (call_outgoing > 0) 
    {    
        std::string str_calls_outgoing_mean = timeToString ( (time_t) (_pvt->call()->statistics()->_total_time_outgoing / call_outgoing) );
        switch_xml_t xmean_out = switch_xml_add_child_d(xrates,"outgoing",0);
        switch_xml_set_txt_d(xmean_out,str_calls_outgoing_mean.c_str());
    }    

    return xrates;
}

std::string Board::KhompPvt::PvtStatistics::getDetailed()
{ 
    /* skip inactive channels */
    //if (_pvt->getSignaling() == ksigInactive) return "";

    /* buffer our data to return at the end */
    std::string strBuffer;
            
    strBuffer.append(_pvt->call()->statistics()->getDetailed());

    /* this values come from kserver */
    unsigned int call_incoming = Globals::k3lapi.channel_stats(
            _pvt->target().device, _pvt->target().object, kcsiInbound);
    unsigned int call_outgoing = Globals::k3lapi.channel_stats(
            _pvt->target().device, _pvt->target().object, kcsiOutbound);
    unsigned int call_fails    = Globals::k3lapi.channel_stats(
            _pvt->target().device, _pvt->target().object, kcsiOutFailed);

    strBuffer.append(STG(FMT("Number of incoming calls: \t%d\n") % call_incoming));
    strBuffer.append(STG(FMT("Number of outgoing calls: \t%d\n") % call_outgoing));
    strBuffer.append(STG(FMT("Number of calls failed: \t%d\n") % call_fails));

    strBuffer.append(getDetailedRates());
    
    return strBuffer;
}


switch_xml_t Board::KhompPvt::PvtStatistics::getDetailedXML()
{
    switch_xml_t xch = _pvt->call()->statistics()->getDetailedXML();

    /* this values come from kserver */
    unsigned int call_incoming = Globals::k3lapi.channel_stats(
            _pvt->target().device, _pvt->target().object, kcsiInbound);
    unsigned int call_outgoing = Globals::k3lapi.channel_stats(
            _pvt->target().device, _pvt->target().object, kcsiOutbound);
    unsigned int call_fails    = Globals::k3lapi.channel_stats(
            _pvt->target().device, _pvt->target().object, kcsiOutFailed);

    /* total/call_incoming */
    switch_xml_t xin_calls = switch_xml_add_child_d(xch,"calls_incoming",0);
    switch_xml_set_txt_d(xin_calls,STR(FMT("%d") % call_incoming));

    /* total/call_outgoing */
    switch_xml_t xout_calls = switch_xml_add_child_d(xch,"calls_outgoing",0);
    switch_xml_set_txt_d(xout_calls,STR(FMT("%d") % call_outgoing));

    /* total/calls_failed */
    switch_xml_t xfailed_calls = switch_xml_add_child_d(xch,"calls_failed",0);
    switch_xml_set_txt_d(xfailed_calls,STR(FMT("%d") % call_fails));

    return xch;
}

std::string Board::KhompPvt::PvtStatistics::getRow() 
{
    /* skip inactive channels */
    //if (_pvt->getSignaling() == ksigInactive) return "";

    time_t action_time;
    time (&action_time);
   
    action_time -= _pvt->call()->statistics()->_base_time;

    uint32 calls_incoming = Globals::k3lapi.channel_stats(
            _pvt->target().device, _pvt->target().object, kcsiInbound);
    uint32 calls_outgoing = Globals::k3lapi.channel_stats(
            _pvt->target().device, _pvt->target().object, kcsiOutbound);
    uint32 call_fails     = Globals::k3lapi.channel_stats(
            _pvt->target().device, _pvt->target().object, kcsiOutFailed);
    
    std::string string_time = "  n/a  ";
    std::string call_type   = "  none  ";

    if (_pvt->call()->_flags.check(Kflags::IS_INCOMING))
        call_type = "incoming";
    else if (_pvt->call()->_flags.check(Kflags::IS_OUTGOING))
        call_type = "outgoing";

    if (_pvt->owner())
    {    
        string_time = timeToString(action_time);
    }    

    return  STG(FMT("| %d,%02d | %8d | %8d | %8d | %7d | %10s | %8s | %8s |")
            % _pvt->target().device 
            % _pvt->target().object 
            % calls_incoming 
            % calls_outgoing 
            % _pvt->call()->statistics()->_channel_fails
            % call_fails 
            % _pvt->getStateString() 
            % call_type 
            % string_time);
}

switch_xml_t Board::KhompPvt::PvtStatistics::getNode()
{
    time_t action_time;
    time (&action_time);
   
    action_time -= _pvt->call()->statistics()->_base_time;

    uint32 calls_incoming = Globals::k3lapi.channel_stats(
            _pvt->target().device, _pvt->target().object, kcsiInbound);
    uint32 calls_outgoing = Globals::k3lapi.channel_stats(
            _pvt->target().device, _pvt->target().object, kcsiOutbound);
    uint32 call_fails     = Globals::k3lapi.channel_stats(
            _pvt->target().device, _pvt->target().object, kcsiOutFailed);
    
    std::string string_time = "  n/a  ";
    std::string call_type   = "  none  ";

    if (_pvt->call()->_flags.check(Kflags::IS_INCOMING))
        call_type = "incoming";
    else if (_pvt->call()->_flags.check(Kflags::IS_OUTGOING))
        call_type = "outgoing";

    if (_pvt->owner())
    {    
        string_time = timeToString(action_time);
    }    

    /* device/channel */
    switch_xml_t xchn = switch_xml_new("channel");
    switch_xml_set_attr_d(xchn,"id",STR(FMT("%d") % _pvt->target().object));

    /* device/channel/details */
    switch_xml_t xdetails = switch_xml_add_child_d(xchn,"details",0);

    /* device/channel/details/calls_incomming */
    switch_xml_t xin_calls = switch_xml_add_child_d(xdetails,"calls_incoming",0);
    switch_xml_set_txt_d(xin_calls,STR(FMT("%d") % calls_incoming));

    /* device/channel/details/calls_outgoing */
    switch_xml_t xout_calls = switch_xml_add_child_d(xdetails,"calls_incoming",0);
    switch_xml_set_txt_d(xout_calls,STR(FMT("%d") % calls_outgoing));

    /* device/channel/details/channel_fails */
    switch_xml_t xchn_fails = switch_xml_add_child_d(xdetails,"channel_fails",0);
    switch_xml_set_txt_d(xchn_fails,STR(FMT("%d") % _pvt->call()->statistics()->_channel_fails));

    /* device/channel/details/calls_fails */
    switch_xml_t xcall_fails = switch_xml_add_child_d(xdetails,"calls_fails",0);
    switch_xml_set_txt_d(xcall_fails,STR(FMT("%d") % call_fails));

    /* device/channel/details/state */
    switch_xml_t xstate = switch_xml_add_child_d(xdetails,"state",0);
    switch_xml_set_txt_d(xstate,_pvt->getStateString().c_str());

    /* device/channel/details/call_type */
    switch_xml_t xcall_type = switch_xml_add_child_d(xdetails,"call_type",0);
    switch_xml_set_txt_d(xcall_type,call_type.c_str());

    /* device/channel/details/time */
    switch_xml_t xtime = switch_xml_add_child_d(xdetails,"time",0);
    switch_xml_set_txt_d(xtime,string_time.c_str());

    return xchn;
}

switch_status_t Board::KhompPvt::justAlloc(bool is_answering, switch_memory_pool_t **pool)
{
    DBG(FUNC, PVT_FMT(target(), "c"));

    if(is_answering)
    {
        /* Create a new session on incoming call */
        call()->_flags.set(Kflags::IS_INCOMING);
        if(!session())
        {
#if SWITCH_LESS_THAN(1,0,6)
            session(switch_core_session_request(Globals::khomp_endpoint_interface, SWITCH_CALL_DIRECTION_INBOUND, NULL));
#else            
            session(switch_core_session_request(Globals::khomp_endpoint_interface, SWITCH_CALL_DIRECTION_INBOUND, SOF_NONE, NULL));
#endif            
        }
        else
        {
            DBG(FUNC, PVT_FMT(target(), "Session already created"));
        }
    }
    else
    {
        /* Create a new session on outgoing call */
        call()->_flags.set(Kflags::IS_OUTGOING);
#if SWITCH_LESS_THAN(1,0,6)        
        session(switch_core_session_request(Globals::khomp_endpoint_interface, SWITCH_CALL_DIRECTION_OUTBOUND, pool));
#else
        session(switch_core_session_request(Globals::khomp_endpoint_interface, SWITCH_CALL_DIRECTION_OUTBOUND, SOF_NONE, pool));
#endif
    }

    if(!session())
    {
        LOG(ERROR, PVT_FMT(target(), "r (Initilization Error, session not created!)"));
        return SWITCH_STATUS_FALSE;
    }

    switch_core_session_add_stream(session(), NULL);

    if (switch_core_codec_init(&_read_codec, "PCMA", NULL, NULL, 8000, Globals::switch_packet_duration, 1,
            SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,
                (pool ? *pool : NULL)) != SWITCH_STATUS_SUCCESS)
    {
        destroy();
        LOG(ERROR, PVT_FMT(target(), "r (Error while init read codecs)"));
        return SWITCH_STATUS_FALSE;
    }

    if (switch_core_codec_init(&_write_codec, "PCMA", NULL, NULL, 8000, Globals::switch_packet_duration, 1,
            SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,
                (pool ? *pool : NULL)) != SWITCH_STATUS_SUCCESS)
    {
        destroy();
        LOG(ERROR, PVT_FMT(target(), "r (Error while init write codecs)"));
        return SWITCH_STATUS_FALSE;
    }

    //TODO: Retirar daqui
//    switch_mutex_init(&flag_mutex, SWITCH_MUTEX_NESTED,
//                switch_core_session_get_pool(_session));

    switch_core_session_set_private(_session, this);

    if((switch_core_session_set_read_codec(_session, &_read_codec) !=
                SWITCH_STATUS_SUCCESS) ||
       (switch_core_session_set_write_codec(_session, &_write_codec) !=
                SWITCH_STATUS_SUCCESS))
    {
        destroy();
        LOG(ERROR, PVT_FMT(target(), "r (Error while set read codecs)"));
        return SWITCH_STATUS_FALSE;
    }

    try
    {
        /* accounttcode for CDR identification */
        setFSChannelVar(getFSChannel(),"accountcode",_accountcode.c_str());

        /* language for IVR machine */
        setFSChannelVar(getFSChannel(),"language",_language.c_str());
    }
    catch (Board::KhompPvt::InvalidSwitchChannel & err)
    {
        LOG(ERROR, PVT_FMT(target(), "(%s)") % err._msg.c_str() );
        return SWITCH_STATUS_FALSE;
    }

    DBG(FUNC, PVT_FMT(target(), "r"));
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t Board::KhompPvt::justStart(switch_caller_profile_t *profile)
{
    DBG(FUNC, PVT_FMT(target(), "c"));

    try
    {
        switch_channel_t *channel = getFSChannel();
        
        if(call()->_flags.check(Kflags::IS_INCOMING))
        {
            std::string exten("s");

            owner(_session);

            if(call()->_incoming_context.empty())
            {
                MatchExtension::ContextListType contexts;
                std::string context("default");

                if(!validContexts(contexts))
                {
                    destroy();
                    owner(NULL);
                    LOG(ERROR, PVT_FMT(_target, "r (Invalid context)"));
                    return SWITCH_STATUS_FALSE;
                }

                switch(MatchExtension::findExtension(exten, context, contexts, _call->_dest_addr, _call->_orig_addr)) 
                {
                    case MatchExtension::MATCH_NONE:
                        destroy();
                        owner(NULL);
                        LOG(ERROR, PVT_FMT(_target, "r (unable to find exten/context on incoming call %s/%s)")
                                % _call->_dest_addr % (contexts.size() >= 1 ? contexts[0] : "default"));
                        return SWITCH_STATUS_FALSE; 
                    default:
                        DBG(FUNC, PVT_FMT(_target, "our: dialplan '%s', context '%s', exten '%s'") % Opt::_options._dialplan() % context % exten);
                        break;
                }

                call()->_incoming_context = context;
            }
            else
            {
                exten = call()->_dest_addr;
                DBG(FUNC, PVT_FMT(target(), "already found our: dialplan '%s', context '%s', exten '%s'") % Opt::_options._dialplan() % call()->_incoming_context % exten);
            }
            
            _caller_profile = switch_caller_profile_new(switch_core_session_get_pool(_session),
                    "Khomp",                           //username
                    Opt::_options._dialplan().c_str(), //dialplan
                    NULL,                              //caller_id_name
                    _call->_orig_addr.c_str(),         //caller_id_number
                    NULL,                              //network_addr
                    _call->_orig_addr.c_str(),         //ani
                    NULL,                              //aniii
                    NULL,                              //rdnis
                    (char *) "mod_khomp",              //source
                    call()->_incoming_context.c_str(), //context
                    exten.c_str());                    //destination_number

            if(!_caller_profile)
            {
                destroy();
                owner(NULL);
                LOG(ERROR, PVT_FMT(_target, "r (Cannot create caller profile)"));
                return SWITCH_STATUS_FALSE;
            }

            std::string name = STG(FMT("Khomp/%d/%d/%s")
                    % target().device
                    % target().object
                    % _call->_dest_addr);

            DBG(FUNC, PVT_FMT(target(), "Connect inbound channel %s") % name.c_str());

            switch_channel_set_name(channel, name.c_str());
            switch_channel_set_caller_profile(channel, _caller_profile);

            switch_channel_set_state(channel, CS_INIT);

            setSpecialVariables();

            if (switch_core_session_thread_launch(_session) != SWITCH_STATUS_SUCCESS)
            {
                destroy();
                owner(NULL);
                LOG(ERROR, PVT_FMT(target(), "r (Error spawning thread)"));
                return SWITCH_STATUS_FALSE;
            }
        }
        else if(call()->_flags.check(Kflags::IS_OUTGOING))
        {
            if(_call->_orig_addr.empty())
                _call->_orig_addr = profile->caller_id_number;

            switch_channel_set_name(channel, STG(FMT("Khomp/%d/%d/%s")
                        % target().device
                        % target().object
                        % (!_call->_dest_addr.empty() ? _call->_dest_addr.c_str() : "")).c_str());
            _caller_profile = switch_caller_profile_clone(_session, profile);
            switch_channel_set_caller_profile(channel, _caller_profile);

            switch_channel_set_state(channel, CS_INIT);
        }
        else
        {
            DBG(FUNC, PVT_FMT(target(), "r (Not INCOMING or OUTGOING)"));
            return SWITCH_STATUS_FALSE;
        }
    }
    catch(Board::KhompPvt::InvalidSwitchChannel & err)
    {
        destroy();
        owner(NULL);
        DBG(FUNC, PVT_FMT(target(), "r (%s)") % err._msg.c_str()); 
        return SWITCH_STATUS_FALSE;
    }

    DBG(FUNC, PVT_FMT(target(), "r"));
    return SWITCH_STATUS_SUCCESS;
}
        
int Board::KhompPvt::makeCall(std::string params)
{
    DBG(FUNC, PVT_FMT(target(), "Dialing to %s from %s")
        % _call->_dest_addr.c_str()
        % _call->_orig_addr.c_str());

    /* Lets make the call! */
    std::string full_params;
    
    if(!_call->_dest_addr.empty())
        full_params += STG(FMT(" dest_addr=\"%s\"")
                % _call->_dest_addr);

    if(!params.empty())
        full_params += STG(FMT(" %s")
                % params);

    DBG(FUNC, PVT_FMT(target(), "We are calling with params: %s.") % full_params.c_str());

    int ret = commandState(KHOMP_LOG, CM_MAKE_CALL, (full_params != "" ? full_params.c_str() : NULL));

    if(ret != ksSuccess)
    {
        destroy();
        cleanup(CLN_FAIL);
    }

    return ret;
}

bool Board::KhompPvt::signalDTMF(char d)
{
    DBG(FUNC, PVT_FMT(target(), "c (dtmf=%c)") % d);

    try
    {
        switch_dtmf_t dtmf = { (char) d, switch_core_default_dtmf_duration(0) };
        switch_channel_queue_dtmf(getFSChannel(), &dtmf);
    }
    catch (Board::KhompPvt::InvalidSwitchChannel & err)
    {
        LOG(ERROR, PVT_FMT(target(), "r (Received a DTMF, but %s)") % err._msg.c_str() );
        return false;
    }

    DBG(FUNC, PVT_FMT(target(), "r"));

    return true;
}

bool Board::KhompPvt::indicateBusyUnlocked(int cause, bool sent_signaling)
{
    DBG(FUNC, PVT_FMT(target(), "c"));

    /* already playing! */
    if (call()->_indication != INDICA_NONE)
    {
        if (call()->_indication == INDICA_RING)
        {
            DBG(FUNC, PVT_FMT(target(), "ringback being disabled"));

            if(call()->_cadence == PLAY_RINGBACK)
                stopCadence();

            try
            {
                call()->_flags.clear(Kflags::GEN_PBX_RING);
                call()->_flags.clear(Kflags::GEN_CO_RING);

                Board::board(_target.device)->_timers.del(call()->_idx_pbx_ring);
                Board::board(_target.device)->_timers.del(call()->_idx_co_ring);
            }
            catch (K3LAPITraits::invalid_device & err)
            {
                LOG(ERROR, PVT_FMT(_target, "Unable to get device: %d!") % err.device);
            }

            mixer(KHOMP_LOG, 1, kmsGenerator, kmtSilence);
            call()->_indication = INDICA_NONE;
        }
        else
        {
            DBG(FUNC, PVT_FMT(target(), "r (already playing something: %d)") 
                    % call()->_indication);
            return false;
        }
    }

    setHangupCause(cause, false);

    call()->_indication = INDICA_BUSY;
        
    DBG(FUNC, PVT_FMT(target(), "r"));

    return true;
}

void Board::KhompPvt::destroy(switch_core_session_t * s)
{
    switch_core_session_t * tmp_session;

    if(!s)
    {
        if(!_session)
            return;
    
        tmp_session = _session;
    }
    else
    {
        tmp_session = s;
        
    }

    switch_core_session_destroy(&tmp_session);
}

void Board::KhompPvt::destroyAll()
{
    if(_session)
    {
        switch_core_session_set_private(_session, NULL);
        _session = NULL;
    }
    
    owner(NULL);

    if(_caller_profile)
    {
        //DBG(FUNC, PVT_FMT(target(), "Profile != NULL"));
        //TODO: Destruir
        _caller_profile = NULL;
    }

    if (switch_core_codec_ready(&_read_codec)) {
        //if(session())
        //    switch_core_session_lock_codec_read(_session);
        switch_core_codec_destroy(&_read_codec);
        _read_codec.implementation = NULL;
        //if(session())
        //    switch_core_session_unlock_codec_read(_session);
    }

    if (switch_core_codec_ready(&_write_codec)) {
        //if(session())
        //    switch_core_session_lock_codec_write(_session);
        switch_core_codec_destroy(&_write_codec);
        _write_codec.implementation = NULL;
        //if(session())
        //    switch_core_session_unlock_codec_write(_session);
    }

}

void Board::KhompPvt::doHangup()
{
    try
    {
        switch_channel_t *channel = getFSChannel();

        int cause_from_freeswitch = switch_channel_get_cause(channel);
        if(cause_from_freeswitch != SWITCH_CAUSE_NONE)
        {
            DBG(FUNC, PVT_FMT(_target, "cause already set to %s from freeswitch") % switch_channel_cause2str((switch_call_cause_t)cause_from_freeswitch));
        }
        else
        {
            DBG(FUNC, PVT_FMT(target(), "Hangup channel \"%s\"") % switch_channel_get_name(channel));

            switch_channel_hangup(channel, (switch_call_cause_t)call()->_hangup_cause);
        }
    }
    catch(Board::KhompPvt::InvalidSwitchChannel & err)
    {
        //DBG(FUNC, PVT_FMT(target(), "Hangup channel already done"));
    }
}

bool Board::KhompPvt::cleanup(CleanupType type)
{
//    flags = 0;

    _reader_frames.clear();
    _writer_frames.clear();
    
    call()->_flags.clear(Kflags::CONNECTED);
    
    //call()->_flags.clear(Kflags::BRIDGED);
    
    call()->_flags.clear(Kflags::DROP_COLLECT);
    
    call()->_flags.clear(Kflags::OUT_OF_BAND_DTMFS);

    call()->_flags.clear(Kflags::WAIT_SEND_DTMF);
    
    call()->_flags.clear(Kflags::GEN_PBX_RING);
    call()->_flags.clear(Kflags::GEN_CO_RING);

    try
    {
        Board::board(_target.device)->_timers.del(call()->_idx_pbx_ring);
        Board::board(_target.device)->_timers.del(call()->_idx_co_ring);
    }
    catch (K3LAPITraits::invalid_device & err)
    {
        LOG(ERROR, PVT_FMT(_target, "Unable to get device: %d!") % err.device);
    }
        
    call()->_idx_pbx_ring.reset();
    call()->_idx_co_ring.reset();

    switch (type)
    {
    case CLN_HARD:
    case CLN_FAIL:
        call()->_flags.clear(Kflags::KEEP_DTMF_SUPPRESSION);
        call()->_flags.clear(Kflags::KEEP_ECHO_CANCELLATION);
        call()->_flags.clear(Kflags::KEEP_AUTO_GAIN_CONTROL);

        stopStream();
        stopListen();
        doHangup();
        call()->_flags.clear(Kflags::IS_INCOMING);
        call()->_flags.clear(Kflags::IS_OUTGOING);
        call()->_flags.clear(Kflags::REALLY_CONNECTED);
        call()->_flags.clear(Kflags::HAS_PRE_AUDIO);
        call()->_flags.clear(Kflags::HAS_CALL_FAIL);

        /* pára cadências e limpa estado das flags */
        stopCadence();

        cleanupIndications(true);

        if(call()->_input_volume >= -10 && call()->_input_volume <= 10)
            setVolume("input" , Opt::_options._input_volume());

        if(call()->_output_volume >= -10 && call()->_output_volume <= 10)
            setVolume("output", Opt::_options._output_volume());        

        return _call->clear();
    case CLN_SOFT:
        if(call()->_cadence != PLAY_FASTBUSY)
        {    
            /* pára cadências e limpa estado das flags */
            stopCadence();
        }

        if (call()->_indication == INDICA_RING)
        {
            call()->_indication = INDICA_NONE;

            mixer(KHOMP_LOG, 1, kmsGenerator, kmtSilence);
        }
        break;
    }

    return true;
}

void Board::KhompPvt::cleanupIndications(bool force)
{
    if (call()->_indication != INDICA_NONE)
    {
        call()->_indication = INDICA_NONE;
        mixer(KHOMP_LOG, 1, kmsGenerator, kmtSilence);
    }
}

bool Board::KhompPvt::isFree(bool just_phy)
{
    //DBG(FUNC, DP(this, "c"));
	try
	{
        bool is_physical_free = isPhysicalFree();

		/* if we got here, is physically free */
		if (!is_physical_free || just_phy)
			return is_physical_free;

        ScopedPvtLock lock(this);

        if(session())
            return false;

		bool free_state = !(_call->_flags.check(Kflags::IS_INCOMING) || _call->_flags.check(Kflags::IS_OUTGOING));

		DBG(FUNC, PVT_FMT(target(), "[free = %s]") % (free_state ? "yes" : "no"));
		return free_state;
	}
    catch (ScopedLockFailed & err)
	{
		DBG(FUNC, PVT_FMT(target(), "unable to obtain lock: %s") % err._msg.c_str());
	}

	return false;
}

Board::KhompPvt * Board::queueFindFree(PriorityCallQueue &pqueue)
{
    for (PriorityCallQueue::iterator i = pqueue.begin(); i != pqueue.end(); i++)
    {
        KhompPvt *pvt = (*i);
        if (pvt && pvt->isFree())
        {
            return pvt;
        }
    }

    DBG(FUNC, D("found no free channel for fair allocation!"));
    return NULL;
}

void Board::queueAddChannel(PriorityCallQueue &pqueue, unsigned int board, unsigned int object)
{
    try
    {
        KhompPvt * pvt = get(board, object);
        pqueue.insert(pvt);
    }
    catch(K3LAPITraits::invalid_channel & err)
    {
        //...
    }
}

Board::KhompPvt * Board::findFree(unsigned int board, unsigned int object, bool fully_available)
{
    try
    {
        KhompPvt * pvt = get(board, object);
        return ((fully_available ? pvt->isFree() : pvt->isOK()) ? pvt : NULL);
    }
    catch(K3LAPITraits::invalid_channel & err)
    {
    }
    return NULL;
}

void Board::applyGlobalVolume(void)
{
    DBG(FUNC, "c");

    for (unsigned int dev = 0; dev < Globals::k3lapi.device_count(); dev++)
    {    
        for (unsigned int obj = 0; obj < Globals::k3lapi.channel_count(dev); obj++)
        {
            try
            {
                Board::get(dev,obj)->setVolume("input", Opt::_options._input_volume());
                Board::get(dev,obj)->setVolume("output",Opt::_options._output_volume());
            }
            catch(K3LAPITraits::invalid_channel & err)
            {
                DBG(FUNC, OBJ_FMT(dev, obj, "Channel not found"));
            }
        }    
    }    
    
    DBG(FUNC, "r");
}

bool Board::KhompPvt::startCadence(CadencesType type) 
{
    DBG(FUNC, PVT_FMT(target(), "c"));

    std::string tone("");

    /**/ if (type == PLAY_VM_TONE)  tone = "vm-dialtone";
    else if (type == PLAY_PBX_TONE) tone = "pbx-dialtone";
    else if (type == PLAY_PUB_TONE) tone = "co-dialtone";
    else if (type == PLAY_RINGBACK) tone = "ringback";
    else if (type == PLAY_FASTBUSY) tone = "fast-busy";


    if (tone != "")
    {    
        call()->_cadence = type;

        CadencesMapType::iterator i = Opt::_cadences.find(tone);
        std::string cmd_params;

        if (i != Opt::_cadences.end())
        {    
            CadenceType cadence = (*i).second;

            if (cadence.ring == 0 && cadence.ring_s == 0)
            {    
                cmd_params = "cadence_times=\"continuous\" mixer_track=1";
            }    
            else if (cadence.ring_ext == 0 && cadence.ring_ext_s == 0)
            {    
                cmd_params = STG(FMT("cadence_times=\"%d,%d\" mixer_track=1")
                    % cadence.ring % cadence.ring_s);
            }    
            else 
            {    
                cmd_params = STG(FMT("cadence_times=\"%d,%d,%d,%d\" mixer_track=1")
                    % cadence.ring % cadence.ring_s % cadence.ring_ext % cadence.ring_ext_s);
            }    

            command(KHOMP_LOG,CM_START_CADENCE,cmd_params.c_str());
        }
        else
        {
            LOG(ERROR, PVT_FMT(_target, "r (cadence '%s' not found)") % tone);
            return false;
        }
    }
    else
    {
        LOG(ERROR, PVT_FMT(_target, "r (unknown cadence requested )"));
        return false;
    }

    DBG(FUNC, PVT_FMT(target(), "r"));
    return true;
}

/* Helper functions - based on code from chan_khomp */
bool Board::KhompPvt::startStream(bool enable_mixer)
{
    if (call()->_flags.check(Kflags::STREAM_UP))
        return true;

    if(enable_mixer)
    {
        if(!mixer(KHOMP_LOG, 0, kmsPlay, _target.object))
            return false;
    }

    if(!command(KHOMP_LOG, CM_START_STREAM_BUFFER))
        return false;

    call()->_flags.set(Kflags::STREAM_UP);

    return true;
}

bool Board::KhompPvt::stopStream(bool enable_mixer)
{
    if (!call()->_flags.check(Kflags::STREAM_UP))
        return true;

    call()->_flags.clear(Kflags::STREAM_UP);

    if(enable_mixer)
    {
        if(!mixer(KHOMP_LOG, 0, kmsGenerator, kmtSilence))
            return false;
    }

    if(!command(KHOMP_LOG, CM_STOP_STREAM_BUFFER))
        return false;

    return true;
}

bool Board::KhompPvt::startListen(bool conn_rx)
{
    if(call()->_flags.check(Kflags::LISTEN_UP))
        return true;

    const size_t buffer_size = Globals::boards_packet_duration;

    if(conn_rx)
    {
        if(!obtainRX(false)) //no delay, by default..
            return false;
    }

    if(!command(KHOMP_LOG, CM_LISTEN, (const char *) &buffer_size))
        return false;

    call()->_flags.set(Kflags::LISTEN_UP);

    return true;
}

bool Board::KhompPvt::stopListen(void)
{
    if(!call()->_flags.check(Kflags::LISTEN_UP))
        return true;

    call()->_flags.clear(Kflags::LISTEN_UP);

    if(!command(KHOMP_LOG, CM_STOP_LISTEN))
    {
        return false;
    }

    return true;
}

bool Board::KhompPvt::obtainRX(bool with_delay)
{
    try
    {
        Globals::k3lapi.mixerRecord(_target, 0, (with_delay ? kmsChannel : kmsNoDelayChannel), target().object);
        Globals::k3lapi.mixerRecord(_target, 1, kmsGenerator, kmtSilence);
    }
    catch(K3LAPI::failed_raw_command & e)
    {
        LOG(ERROR, PVT_FMT(target(), "ERROR sending mixer command!"));
        return false;
    }    

    return true;
}

bool Board::KhompPvt::obtainTX()
{
    /* estes buffers *NAO PODEM SER ESTATICOS*! */
    char cmd1[] = { 0x3f, 0x03, 0xff, 0x00, 0x00, 0xff };
    char cmd2[] = { 0x3f, 0x03, 0xff, 0x01, 0x09, 0x0f };

    cmd1[2] = cmd1[5] = cmd2[2] = _target.object;

    try
    {
        int dsp = Globals::k3lapi.get_dsp(_target, K3LAPI::DSP_AUDIO);

        Globals::k3lapi.raw_command(_target.device, dsp, cmd1, sizeof(cmd1));

        Globals::k3lapi.raw_command(_target.device, dsp, cmd2, sizeof(cmd2));

    }
    catch(K3LAPI::failed_raw_command & e)
    {
        LOG(ERROR, PVT_FMT(target(), "ERROR sending mixer command!"));
        return false;
    }    

    return true;
}

bool Board::KhompPvt::sendDtmf(std::string digit)
{
    if(!call()->_flags.check(Kflags::STREAM_UP))
    {
        DBG(FUNC, PVT_FMT(target(), "stream down, not sending dtmf"));
        return false;
    }

    if(!call()->_flags.check(Kflags::OUT_OF_BAND_DTMFS))
    {
        DBG(FUNC, PVT_FMT(target(), "dtmf suppression disabled, not generating dtmf"));
        return false;
    }

    if(digit.empty())
    {
        DBG(FUNC, PVT_FMT(target(), "not sending dtmf (there is nothing to send)"));
        return false;
    }

    if(call()->_flags.check(Kflags::WAIT_SEND_DTMF))
    {
        DBG(FUNC, PVT_FMT(target(), "queueing dtmf (%s)") % digit);
        call()->_queued_digits_buffer += digit;
        return true;
    }

    DBG(FUNC, PVT_FMT(target(), "sending dtmf (%s)") % digit);
    
    call()->_flags.set(Kflags::WAIT_SEND_DTMF);

    return command(KHOMP_LOG, CM_DIAL_DTMF, digit.c_str());
}

bool Board::KhompPvt::setVolume(const char * type, int volume)
{
    std::string arg = STG(FMT("volume=\"%d\" type=\"%s\"") % volume % type);
    DBG(FUNC, PVT_FMT(_target, "%s") % arg);
    return command(KHOMP_LOG,CM_SET_VOLUME,arg.c_str());
}

std::string Board::KhompPvt::getStateString(void)
{
    try
    {
        switch_channel_t *c = getFSChannel();
        if(!c) return "unused";

        switch_channel_state_t state = switch_channel_get_state(c);

        /* shortcut */
        typedef std::string S;

        switch (state)
        {    
            case CS_NEW:               return S("new");
            case CS_INIT:              return S("init");
            case CS_ROUTING:           return S("routing");
            case CS_SOFT_EXECUTE:      return S("sf_exec");
            case CS_EXECUTE:           return S("execute");
            case CS_EXCHANGE_MEDIA:    return S("ex_media");
            case CS_PARK:              return S("park");
            case CS_CONSUME_MEDIA:     return S("cs_media");
            case CS_HIBERNATE:         return S("hibernat");
            case CS_RESET:             return S("reset");
            case CS_HANGUP:            return S("hangup");
            case CS_REPORTING:         return S("reportg");
            case CS_DESTROY:           return S("destroy");
            case CS_NONE:              return S("none");
            default:
                return STG(FMT("none (%d)") % state);
        }    
    }
    catch(Board::KhompPvt::InvalidSwitchChannel & err)
    {
        return "unused";
    }
}

bool Board::KhompPvt::dtmfSuppression(bool enable)
{
    if (!command(KHOMP_LOG, (enable ? CM_ENABLE_DTMF_SUPPRESSION : CM_DISABLE_DTMF_SUPPRESSION)))
        return false;

    if (enable) call()->_flags.set(Kflags::OUT_OF_BAND_DTMFS);
    else        call()->_flags.clear(Kflags::OUT_OF_BAND_DTMFS);

    DBG(FUNC, PVT_FMT(_target, "flag OUT_OF_BAND_DTMFS is : %s") % 
        (call()->_flags.check(Kflags::OUT_OF_BAND_DTMFS) ? "true" : "false"));
    
    return true;
}

bool Board::KhompPvt::echoCancellation(bool enable)
{
    const K3L_DEVICE_CONFIG & devCfg = Globals::k3lapi.device_config(_target);

    /* echo canceller should not be used for non-echo cancellable channels */
    switch (devCfg.EchoConfig)
    {
        case keccFail:
            if (!enable)
                return true;

            LOG(ERROR, PVT_FMT(_target, "unable to activate echo cancellation"));
            return false;

        case keccNotPresent:
            DBG(FUNC, PVT_FMT(_target, "echo cancellation not present, not %s.") % (enable ? "enabling" : "disabling"));
            return true;

        default:
            return command(KHOMP_LOG,(enable ? CM_ENABLE_ECHO_CANCELLER : CM_DISABLE_ECHO_CANCELLER));
    }
}

bool Board::KhompPvt::autoGainControl(bool enable)
{
    bool ret = command(KHOMP_LOG,(enable ? CM_ENABLE_AGC : CM_DISABLE_AGC));
    return ret;
}

void Board::KhompPvt::pbxRingGen(Board::KhompPvt * pvt)
{
    DBG(FUNC, PVT_FMT(pvt->target(), "Generating pbx ring"));

    try 
    {   
        ScopedPvtLock lock(pvt);

        if (!pvt->call()->_flags.check(Kflags::GEN_PBX_RING))
            return;

        if (!pvt->obtainTX())
            return;

        pvt->startCadence(PLAY_RINGBACK);
    }   
    catch (...)
    {
        LOG(ERROR, PVT_FMT(pvt->target(), "unable to lock!"));
    }
}

void Board::KhompPvt::coRingGen(Board::KhompPvt * pvt)
{
    DBG(FUNC, PVT_FMT(pvt->target(), "Generating co ring"));

    try 
    {   
        ScopedPvtLock lock(pvt);

        if (!pvt->call()->_flags.check(Kflags::GEN_CO_RING))
            return;

        pvt->startCadence(PLAY_RINGBACK);
    }   
    catch (...)
    {
        LOG(ERROR, PVT_FMT(pvt->target(), "unable to lock the pvt !"));
    }  
}

int Board::KhompPvt::getActiveChannel(bool invalid_as_not_found)
{
    // AT CONSTRUCTION
    return 0;
}

bool Board::KhompPvt::setCollectCall()
{
    DBG(FUNC, PVT_FMT(target(), "c"));
    // NEEDS LOCK !?

    std::vector< TriState > confvalues;
 
    // temporary!
    const char * tmp_var = NULL;

    // get option configuration value
    confvalues.push_back(Opt::_options._drop_collect_call() ? T_TRUE : T_FALSE);
    DBG(FUNC, PVT_FMT(_target, "option drop collect call is '%s'") % (Opt::_options._drop_collect_call() ? "yes" : "no"));

    // get global filter configuration value
    tmp_var = getFSGlobalVar("KDropCollectCall");
    confvalues.push_back(getTriStateValue(tmp_var));
    DBG(FUNC, PVT_FMT(_target, "global KDropCollectCall was '%s'") % (tmp_var ? tmp_var : "(empty)"));

    freeFSGlobalVar(&tmp_var);

    try 
    {
        // get local filter configuration value
        tmp_var = switch_channel_get_variable(getFSChannel(), "KDropCollectCall");
        confvalues.push_back(getTriStateValue(tmp_var));
        DBG(FUNC, PVT_FMT(_target, "local KDropCollectCall was '%s'") % (tmp_var ? tmp_var : "(empty)"));
    }
    catch(Board::KhompPvt::InvalidSwitchChannel & err)
    {
        LOG(ERROR, PVT_FMT(_target, "Cannot obtain the channel variable: %s") % err._msg.c_str()); 
    }

    // store last state assigned
    bool last_state = false;

    // validate the states
    std::vector< TriState >::const_iterator end = confvalues.end();
    for (std::vector< TriState >::const_iterator i = confvalues.begin(); i != end; ++i)
    {
        switch(*i)
        {
            case T_TRUE:
                last_state = true;
                break;
            case T_FALSE:
                last_state = false;
                break;
            case T_UNKNOWN:
            default:
                break;
        }
    }

    if (last_state)
        call()->_flags.set(Kflags::DROP_COLLECT);
    else
        call()->_flags.clear(Kflags::DROP_COLLECT);

    DBG(FUNC, PVT_FMT(_target, "drop collect call flag: %s.") % (last_state ? "set" : "unset"));

    return last_state;
}

bool Board::KhompPvt::onChannelRelease(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(target(), "c"));

    try
    {
        ScopedPvtLock lock(this);

        if (e->Code == EV_CHANNEL_FAIL)
        {
            call()->statistics()->incrementChannelFail();
            _has_fail = true;
            setHangupCause(SWITCH_CAUSE_NETWORK_OUT_OF_ORDER);
            cleanup(CLN_HARD);
        }
        else
        {
            if(call()->_flags.check(Kflags::REALLY_CONNECTED))
            {
                call()->statistics()->incrementHangup();
            }

            setHangupCause(SWITCH_CAUSE_NORMAL_CLEARING);
            cleanup(CLN_HARD);
        }
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(target(), "r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }

    DBG(FUNC, PVT_FMT(target(), "r"));
    return true;
}

//TODO: This method must return more information about the channel allocation
bool Board::KhompPvt::onNewCall(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(target(), "c"));

    std::string orig_addr, dest_addr;

    Globals::k3lapi.get_param(e, "orig_addr", orig_addr);
    Globals::k3lapi.get_param(e, "dest_addr", dest_addr);

    if(dest_addr.empty())
    {
        dest_addr="s";
    }

    try
    {
        ScopedPvtLock lock(this);

        _call->_orig_addr = orig_addr;
        _call->_dest_addr = dest_addr;

        if (justAlloc(true) != SWITCH_STATUS_SUCCESS)
        {
            int fail_code = callFailFromCause(SWITCH_CAUSE_UNALLOCATED_NUMBER);
            setHangupCause(SWITCH_CAUSE_UNALLOCATED_NUMBER);
            cleanup(CLN_FAIL);
            reportFailToReceive(fail_code);

            LOG(ERROR, PVT_FMT(target(), "r (Initilization Error on alloc!)"));
            return false;
        }

        if (justStart() != SWITCH_STATUS_SUCCESS)
        {
            int fail_code = callFailFromCause(SWITCH_CAUSE_REQUESTED_CHAN_UNAVAIL);
            setHangupCause(SWITCH_CAUSE_REQUESTED_CHAN_UNAVAIL);
            cleanup(CLN_FAIL);
            reportFailToReceive(fail_code);
            
            LOG(ERROR, PVT_FMT(target(), "r (Initilization Error on start!)"));
            return false;
        }
    }
    catch (ScopedLockFailed & err)
    {
        cleanup(CLN_FAIL);
        LOG(ERROR, PVT_FMT(target(), "r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }
    catch (Board::KhompPvt::InvalidSwitchChannel & err)
    {
        cleanup(CLN_FAIL);
        LOG(ERROR, PVT_FMT(target(), "r (%s)") % err._msg.c_str() );
        return false;
    }

    DBG(FUNC, PVT_FMT(target(), "r"));
    return true;
}

bool Board::KhompPvt::onDisconnect(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_target, "c"));   

/*
    try
    {
        ScopedPvtLock lock(this);

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target, "r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }
*/

    DBG(FUNC, PVT_FMT(_target, "r"));   
    return true;
}

bool Board::KhompPvt::onAudioStatus(K3L_EVENT *e)
{
    try
    {
        if(e->AddInfo != kmtSilence)
        {
            if(call()->_flags.check(Kflags::GEN_PBX_RING))
            {
                DBG(FUNC, PVT_FMT(_target, "PBX ringback being disabled..."));   
                ScopedPvtLock lock(this);

                call()->_flags.clear(Kflags::GEN_PBX_RING);
                Board::board(_target.device)->_timers.del(call()->_idx_pbx_ring);

                if(call()->_cadence != PLAY_VM_TONE)
                {    
                    stopCadence();
                }

                if (!call()->_flags.check(Kflags::CONNECTED))
                {
                    obtainRX(Opt::_options._suppression_delay());
            
                    //Marcar para o Freeswitch que jah tem audio passando
                    if (call()->_flags.check(Kflags::IS_OUTGOING))
                        switch_channel_mark_pre_answered(getFSChannel());

                }
            }
            
            if (!call()->_is_progress_sent && call()->_flags.check(Kflags::HAS_CALL_FAIL))
            {

                ScopedPvtLock lock(this);

                DBG(FUNC, PVT_FMT(_target, "Audio status progress"));   

                call()->_is_progress_sent = true;
                //Sinaliza para o Freeswitch PROGRESS

                DBG(FUNC, PVT_FMT(_target, "Pre answer"));   

                //pvt->signal_state(AST_CONTROL_PROGRESS);
                switch_channel_pre_answer(getFSChannel());
            }
        }
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target, "unable to lock %s!") % err._msg.c_str() );
        return false;

    }
    catch (Board::KhompPvt::InvalidSwitchChannel & err)
    {
        cleanup(CLN_FAIL);
        LOG(ERROR, PVT_FMT(target(), "r (%s)") % err._msg.c_str() );
        return false;
    }
    catch (K3LAPITraits::invalid_device & err)
    {
        LOG(ERROR, PVT_FMT(_target, "r (unable to get device: %d!)") % err.device);
        return false;
    }

    return true;
}

bool Board::KhompPvt::onCollectCall(K3L_EVENT *e)
{
    try  
    {    
        ScopedPvtLock lock(this);

        //TODO: AMI ? 
        //K::internal::ami_event(pvt, EVENT_FLAG_CALL, "CollectCall",
          //      STG(FMT("Channel: Khomp/B%dC%d\r\n") % pvt->boardid % pvt->objectid));

        if (Opt::_options._drop_collect_call() || _call->_flags.check(Kflags::DROP_COLLECT))
        {     
            /* disconnect! */
            //TODO: SCE_HIDE !?
            command(KHOMP_LOG,CM_DISCONNECT);
//           command(KHOMP_LOG,CM_DISCONNECT,SCE_HIDE);

        }    
    }    
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target, "unable to lock %s!") % err._msg.c_str() );
        return false;
    }

    return true;
}

bool Board::KhompPvt::onSeizureStart(K3L_EVENT *e)
{
/*
    try
    {
        ScopedPvtLock lock(this);
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target, "unable to lock %s!") % err._msg.c_str() );
        return false;
    }
*/
    return true;
}

bool Board::KhompPvt::setupConnection()
{
    if(!call()->_flags.check(Kflags::IS_INCOMING) && !call()->_flags.check(Kflags::IS_OUTGOING))
    {
        DBG(FUNC, PVT_FMT(_target, "Channel already disconnected"));   
        return false;
    }

    if(call()->_cadence != PLAY_VM_TONE)
    {    
        stopCadence();
    }
        
    if (call()->_indication != INDICA_NONE)
    {
        call()->_indication = INDICA_NONE;

        mixer(KHOMP_LOG, 1, kmsGenerator, kmtSilence);
    }

    if(call()->_flags.check(Kflags::GEN_PBX_RING))
    {
        call()->_flags.clear(Kflags::GEN_PBX_RING);
        Board::board(_target.device)->_timers.del(call()->_idx_pbx_ring);
    }

    if(call()->_flags.check(Kflags::GEN_CO_RING))
    {
        call()->_flags.clear(Kflags::GEN_CO_RING);
        Board::board(_target.device)->_timers.del(call()->_idx_co_ring);
    }

    if (!call()->_flags.check(Kflags::CONNECTED))
        call()->_flags.set(Kflags::CONNECTED);

    if (!call()->_flags.check(Kflags::REALLY_CONNECTED))
        call()->_flags.set(Kflags::REALLY_CONNECTED);

    /* Sinalizar para o Freeswitch o atendimento */

    DBG(FUNC, PVT_FMT(_target, "Call will be answered."));   

    if(call()->_flags.check(Kflags::IS_INCOMING))
    {
        switch_channel_answer(getFSChannel());
    }   
    else if(call()->_flags.check(Kflags::IS_OUTGOING))
    {
        switch_channel_mark_answered(getFSChannel());
    }

    call()->statistics()->incrementNewCall();

    return true;
}


bool Board::KhompPvt::onConnect(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_target, "c"));

    try
    {
        ScopedPvtLock lock(this);
    
        return setupConnection();
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target, "r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }
    catch (Board::KhompPvt::InvalidSwitchChannel & err)
    {
        LOG(ERROR, PVT_FMT(target(), "r (%s)") % err._msg.c_str() );
        return false;
    }
    catch (K3LAPITraits::invalid_device & err)
    {
        LOG(ERROR, PVT_FMT(_target, "r (unable to get device: %d!)") % err.device);
        return false;
    }
    
    DBG(FUNC, PVT_FMT(_target, "r"));

    return true;
}

bool Board::KhompPvt::onCallSuccess(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_target, "c"));   
 
    try
    {
        ScopedPvtLock lock(this);

        switch_channel_mark_ring_ready(getFSChannel());
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target, "r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }
    catch (Board::KhompPvt::InvalidSwitchChannel & err)
    {
        LOG(ERROR, PVT_FMT(target(), "r (%s)") % err._msg.c_str() );
        return false;
    }

    DBG(FUNC, PVT_FMT(_target, "r"));

    return true;
}

bool Board::KhompPvt::onCallFail(K3L_EVENT *e)
{
   DBG(FUNC, PVT_FMT(_target, "c"));
   try
   {
       ScopedPvtLock lock(this);

        call()->_flags.set(Kflags::HAS_CALL_FAIL);
    
       //TODO: Notificar o Freeswitch: call fail

        cleanup(CLN_SOFT);
   }
   catch (ScopedLockFailed & err)
   {
       LOG(ERROR, PVT_FMT(_target, "r (unable to lock %s!)") % err._msg.c_str() );
       return false;
   }
   
   DBG(FUNC, PVT_FMT(_target, "r"));

   return true;
}

bool Board::KhompPvt::onNoAnswer(K3L_EVENT *e)
{
    /* TODO: Destroy sessions and channels */
    DBG(FUNC, PVT_FMT(_target, "No one answered the call."));   

    // TODO: Set channel variable if we get this event 
    // TODO: Fire an event so ESL can get it? 
    // Call Analyser has to be enabled on k3lconfig 
    DBG(FUNC, PVT_FMT(_target, "Detected: \"%s\"") %  Verbose::callStartInfo((KCallStartInfo)e->AddInfo).c_str());   

    // Fire a custom event about this 
    /* MUST USE THE NEW EVENT SYSTEM
    switch_event_t * event;
    if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, KHOMP_EVENT_MAINT) == SWITCH_STATUS_SUCCESS)
    {
        Board::khomp_add_event_board_data(_target, event);
        switch_event_add_header_string(event, SWITCH_STACK_BOTTOM,
                "EV_CALL_ANSWER_INFO",
                Verbose::callStartInfo((KCallStartInfo)e->AddInfo).c_str());

        switch_event_fire(&event);
    }
    */

    return true;
}

bool Board::KhompPvt::onDtmfDetected(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_target, "c (dtmf=%c)") % (char) e->AddInfo);

    try
    {
        ScopedPvtLock lock(this);

        if (call()->_flags.check(Kflags::IS_INCOMING) || call()->_flags.check(Kflags::IS_OUTGOING)) /* is a valid call? */
        {    
            char digit = (char) e->AddInfo;

            //TODO: WTHeck ? 
            //if (!call()->_flags.check(Kflags::OUT_OF_BAND_DTMFS) && !call()->_flags.check(Kflags::BRIDGED))
            if (!call()->_flags.check(Kflags::OUT_OF_BAND_DTMFS))
            {    
                /* we do not queue dtmfs as we do not need to resend them */
                DBG(FUNC, PVT_FMT(_target, "r (not queueing dtmf, not needed.)"));
                return true;
            }    

            if (Opt::_options._ignore_letter_dtmfs())
            {    
                switch (e->AddInfo)
                {    
                    case 'A': case 'a': 
                    case 'B': case 'b': 
                    case 'C': case 'c': 
                    case 'D': case 'd': 
                        DBG(FUNC, PVT_FMT(_target, "r (not queueing dtmf, letter digit ignored!)"));
                        return true;
                    default:
                        break;
                }    
            }
            
            signalDTMF(e->AddInfo);
        }
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target, "r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }

    DBG(FUNC, PVT_FMT(_target, "r"));
    return true;
}

bool Board::KhompPvt::onDtmfSendFinish(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_target, "c"));

    try
    {
        ScopedPvtLock lock(this);

        if (call()->_flags.check(Kflags::WAIT_SEND_DTMF))
        {
            if(!call()->_queued_digits_buffer.empty())
            {
                DBG(FUNC, PVT_FMT(target(), "sending dtmf (%s)") 
                        % call()->_queued_digits_buffer);

                command(KHOMP_LOG, CM_DIAL_DTMF, 
                        call()->_queued_digits_buffer.c_str());

                /* clear the buffer that has been send */
                call()->_queued_digits_buffer.clear();
            }
            else
            {
                DBG(FUNC, PVT_FMT(target(), 
                        "finished sending some digits, cleaning up!"));
                call()->_flags.clear(Kflags::WAIT_SEND_DTMF);
            }
        } 
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target, "r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }
    
    DBG(FUNC, PVT_FMT(_target, "r"));

    return true;
}

bool Board::KhompPvt::onEvUntreated(K3L_EVENT *e)
{
    DBG(FUNC, PVT_FMT(_target, "New Event has just arrived with untreated code \"%d\" \"%s\"") % e->Code % Globals::verbose.event(_target.object, e));

    return false;
}

bool Board::KhompPvt::eventHandler(K3L_EVENT *e)
{
    DBG(STRM, D("c"));

    bool ret = true;

    switch(e->Code)
    {
    case EV_CHANNEL_FREE:
    case EV_CHANNEL_FAIL:
        ret = onChannelRelease(e);
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

    case EV_CONNECT:
        ret = onConnect(e);
        break;

    case EV_DISCONNECT:
        ret = onDisconnect(e);
        break;
    
    case EV_AUDIO_STATUS:
        ret = onAudioStatus(e);
        break;

    case EV_NO_ANSWER:
        ret = onNoAnswer(e);
        break;

    case EV_DTMF_DETECTED:
        ret = onDtmfDetected(e);
        break;

    case EV_DTMF_SEND_FINISH:
        ret = onDtmfSendFinish(e);
        break;

    case EV_COLLECT_CALL:
        ret = onCollectCall(e);
        break;

    case EV_SEIZURE_START:
        ret = onSeizureStart(e);
        break;

    case EV_CADENCE_RECOGNIZED:
        break;

    default:
        ret = onEvUntreated(e);
        break;
    }

    DBG(STRM, D("r"));

    return ret;
}

bool Board::KhompPvt::indicateProgress()
{
    DBG(FUNC, PVT_FMT(_target, "c")); 
    
    int ret = false;

    try
    {
        ScopedPvtLock lock(this);

        if (!call()->_flags.check(Kflags::CONNECTED))
        {
            bool has_audio = sendPreAudio(RingbackDefs::RB_SEND_NOTHING);

            if (has_audio)
            {
                /* start grabbing audio */
                startListen();
                /* start stream if it is not already */
                startStream();

                ret = true;
            }
        }
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target, "r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }
    
    DBG(FUNC, PVT_FMT(_target, "r"));

    return ret;
}

bool Board::KhompPvt::indicateRinging()
{
    DBG(FUNC, PVT_FMT(_target, "c")); 

    bool ret = false;
    try
    {
        ScopedPvtLock lock(this);

        /* already playing! */
        if (call()->_indication != INDICA_NONE)
        {    
            DBG(FUNC, PVT_FMT(_target, "r (already playing something: %d)") 
                    % call()->_indication);
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
                ringback_value = kq931cCallRejected;
                DBG(FUNC, PVT_FMT(_target, "ringback value adjusted to refuse collect call: %d") % ringback_value);
            }

            // send ringback too? 
            send_ringback = sendPreAudio(ringback_value);

            if (!send_ringback)
            {
                // warn the developer which may be debugging some "i do not have ringback!" issue. 
                DBG(FUNC, PVT_FMT(_target, " not sending pre connection audio"));
            }

        }

        if (send_ringback)
        {    
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
        LOG(ERROR, PVT_FMT(_target, "r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }
    catch (K3LAPITraits::invalid_device & err)
    {
        LOG(ERROR, PVT_FMT(_target, "r (unable to get device: %d!)") % err.device);
        return false;
    }

    
    DBG(FUNC, PVT_FMT(_target, "r"));
    return ret;
}

bool Board::KhompPvt::doChannelAnswer(CommandRequest &cmd)
{
    DBG(FUNC, PVT_FMT(_target, "c"));

    try
    {
        ScopedPvtLock lock(this);

        call()->_flags.set(Kflags::CONNECTED);

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target, "r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }

    DBG(FUNC, PVT_FMT(_target, "r"));
    return true;
}

bool Board::KhompPvt::doChannelHangup(CommandRequest &cmd)
{
    DBG(FUNC, PVT_FMT(_target, "c"));

    bool answered     = true;
    bool disconnected = false;

    try
    {
        ScopedPvtLock lock(this);

        if (call()->_flags.check(Kflags::IS_INCOMING))
        {
            DBG(FUNC, PVT_FMT(_target, "disconnecting incoming channel"));   

            //disconnected = command(KHOMP_LOG, CM_DISCONNECT);
        }
        else if (call()->_flags.check(Kflags::IS_OUTGOING))
        {
            if(call()->_cleanup_upon_hangup)
            {
                DBG(FUNC, PVT_FMT(_target, "disconnecting not allocated outgoing channel..."));   

                disconnected = command(KHOMP_LOG, CM_DISCONNECT);
                cleanup(KhompPvt::CLN_HARD);
                answered = false;

            }
            else
            {
                DBG(FUNC, PVT_FMT(_target, "disconnecting outgoing channel..."));   

                disconnected = command(KHOMP_LOG, CM_DISCONNECT);
            }
        }
        else
        {
            DBG(FUNC, PVT_FMT(_target, "already disconnected"));
            return true;
        }

        if(answered)
        {
            indicateBusyUnlocked(SWITCH_CAUSE_USER_BUSY, disconnected);
        }

        if (call()->_flags.check(Kflags::IS_INCOMING))
        {
            DBG(FUNC, PVT_FMT(_target, "disconnecting incoming channel..."));
            disconnected = command(KHOMP_LOG, CM_DISCONNECT);
        }

        stopStream();

        stopListen();
        
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(_target, "r (unable to lock %s!)") % err._msg.c_str() );
        return false;
    }
    

    DBG(FUNC, PVT_FMT(_target, "r"));
    return true;
}

bool Board::KhompPvt::commandHandler(CommandRequest &cmd)
{
    DBG(STRM, PVT_FMT(target(), "c"));

    bool ret = true;

    switch(cmd.type())
    {
    case CommandRequest::COMMAND:
        switch(cmd.code())
        {
        case CommandRequest::CMD_CALL:
            break;
        case CommandRequest::CMD_ANSWER:
            ret = doChannelAnswer(cmd);
            break;
        case CommandRequest::CMD_HANGUP:
            ret = doChannelHangup(cmd);
            break;
        default:
            ret = false;
        }
        break;

    case CommandRequest::ACTION:
        break;
    
    default:
        ret = false;
    }
    
    DBG(STRM, PVT_FMT(target(), "r"));
    return ret;
}

int Board::eventThread(void *void_evt)
{
    EventRequest evt(false);
    EventFifo * fifo = static_cast < ChanEventHandler * >(void_evt)->fifo();
    int devid = fifo->_device;

    for(;;)
    {
        DBG(THRD, D("(d=%d) c") % devid);
        while(1)
        {
            try
            {
                evt = fifo->_buffer.consumer_start();
                break;
            }
            catch(...) //BufferEmpty & e
            {
                DBG(THRD, D("(d=%d) buffer empty") % devid);

                fifo->_cond.wait();

                if (fifo->_shutdown)
                    return 0;

                DBG(THRD, D("(d=%d) waked up!") % devid);
            }
        }

        /*while (!fifo->_buffer.consume(evt))
        {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(d=%d) buffer empty\n", fifo->_device);

            fifo->_cond.wait();

            if (fifo->_shutdown)
                return 0;

            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(d=%d) waked up!\n", fifo->_device);
        }*/

        DBG(THRD, D("(d=%d) processing buffer...") % devid);

        try
        {
            if(!board(devid)->eventHandler(evt.obj(), evt.event()))
            {
                LOG(ERROR, D("(d=%d) Error on event (%d) \"%s\"") 
                        % devid 
                        % evt.event()->Code
                        % Globals::verbose.event(evt.obj(), evt.event()));
            }
        }
        catch (K3LAPITraits::invalid_device & invalid)
        {
            LOG(ERROR, D("invalid device on event '%s'") 
                % Verbose::eventName(evt.event()->Code).c_str());
        }

        fifo->_buffer.consumer_commit();

    }

    return 0;
}

int Board::commandThread(void *void_evt)
{
    CommandFifo * fifo = static_cast < ChanCommandHandler * >(void_evt)->fifo();
    int devid = fifo->_device;

    for(;;)
    {
        CommandRequest cmd;

        DBG(THRD, D("(d=%d) Command c") % devid);

        while (!fifo->_buffer.consume(cmd))
        {
            DBG(THRD, D("(d=%d) Command buffer empty") % devid);
            fifo->_cond.wait();

            if (fifo->_shutdown)
                return 0;

            DBG(THRD, D("(d=%d) Command waked up!") % devid);
        }

        DBG(THRD, D("(d=%d) Command processing buffer...") % devid);


        try
        {
            if(!get(devid, cmd.obj())->commandHandler(cmd))
            {
                LOG(ERROR, D("(d=%d) Error on command(%d)") % devid % cmd.code());
            }
        }
        catch (K3LAPITraits::invalid_channel & invalid)
        {
            LOG(ERROR, OBJ_FMT(devid,cmd.obj(), "invalid device on command '%d'") %  cmd.code());
        }

    }


    return 0;
}

/* This is the callback function for API events. It selects the event *
 * on a switch and forwards to the real implementation.               */
extern "C" int32 Kstdcall khompEventCallback(int32 obj, K3L_EVENT * e)
{
    //if (K::Logger::Logg.classe(C_EVENT).enabled())
    //    std::string msg = Globals::verbose.event (obj, e) + "."; 
    LOGC(EVENT, FMT("%s.") % Globals::verbose.event(obj, e));

    switch(e->Code)
    {
    case EV_WATCHDOG_COUNT: 
        Board::kommuter.initialize(e);
        break;
    case EV_HARDWARE_FAIL:
    case EV_DISK_IS_FULL:
    case EV_CLIENT_RECONNECT:
    case EV_CLIENT_BUFFERED_AUDIOLISTENER_OVERFLOW:
        LOG(ERROR, D("Audio client buffered overflow"));
        break;
    case EV_CLIENT_AUDIOLISTENER_TIMEOUT:
        LOG(ERROR, "Timeout on audio listener, registering audio listener again");
        k3lRegisterAudioListener( NULL, khompAudioListener );
        break;
    default:
        try
        {
            EventRequest e_req(obj, e);
            Board::board(e->DeviceId)->chanEventHandler()->write(e_req);
        }
        catch (K3LAPITraits::invalid_device & err)
        {
            LOG(ERROR, D("Unable to get device: %d!") % err.device);
            return ksFail;
        }
        break;
    }

    return ksSuccess;
}

