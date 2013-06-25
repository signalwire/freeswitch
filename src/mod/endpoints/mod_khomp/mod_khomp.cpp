/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Khomp development team
 * Geovani Ricardo Wiedenhoft <grw.freeswitch  (at) gmail.com>
 * Leonardo Lang              <lang.freeswitch (at) gmail.com> 
 * Eduardo Nunes              <eduardonunesp   (at) gmail.com>
 * Joao Mesquita              <mesquita        (at) khomp.com.br>
 * Raul Fragoso               <raulfragoso     (at) gmail.com>
 *
 *
 * mod_khomp.c -- Khomp board Endpoint Module
 *
 */

/**
  \mainpage Khomp board Endpoint Module
  
  \section Introduction
  This module has been developed to make a nice, affordable brazilian board
  called Khomp (http://www.khomp.com.br) compatible with FreeSWITCH.
  This module is supported by the manufacturer.

  \section Contributors
  \li Khomp development team
  \li Geovani Ricardo Wiedenhoft <grw.freeswitch  (at) gmail.com>
  \li Leonardo Lang              <lang.freeswitch (at) gmail.com>
  \li Eduardo Nunes              <eduardonunesp   (at) gmail.com>
  \li Joao Mesquita              <jmesquita       (at) gmail.com>
  \li Raul Fragoso               <raulfragoso     (at) gmail.com>
**/

/**
 * @file mod_khomp.cpp
 * @brief Khomp Endpoint Module
 * @see mod_khomp
 */

#include <string>
#include "khomp_pvt.h"
#include "spec.h"
#include "lock.h"
#include "logger.h"
#include "opt.h"
#include "globals.h"
#include "cli.h"

extern "C" void Kstdcall khompAudioListener(int32 deviceid, int32 objectid,
                                          byte * read_buffer, int32 read_size);

/*!
  \brief Load the module. Expadend by a FreeSWITCH macro.
  Things we do here:
  \li Initialize a static structure on KhompPvt
  \li Load the configuration
  \li Start the K3L API, responsible for connecting to KServer
  \li Register mod APIs and APPs
  \li Register audio callback for KServer
  \li Register event callback for KServer
  \see Opt Where all the configs are handled
  \see khompEventCallback To where we bind the event handler
  \see khompAudioListener To where we bind the audio handlers
  */
SWITCH_MODULE_LOAD_FUNCTION(mod_khomp_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_khomp_shutdown);
SWITCH_MODULE_DEFINITION(mod_khomp, mod_khomp_load, mod_khomp_shutdown, NULL);

/*!
 \defgroup fs_states FreeSWITCH State Handlers
            We get called back from FreeSWITCH core everytime we make a
            transition to any of these states. Refer to FreeSWITCH docs for a
            detailed explanation of each state.
 */
/*@{*/
switch_status_t khompInit(switch_core_session_t *session);
switch_status_t khompRouting(switch_core_session_t *session);
switch_status_t khompExecute(switch_core_session_t *session);
switch_status_t khompHangup(switch_core_session_t *session);
switch_status_t khompExchangeMedia(switch_core_session_t *session);
switch_status_t khompSoftExecute(switch_core_session_t *session);
switch_status_t khompDestroy(switch_core_session_t *session);
/*@}*/

switch_state_handler_table_t khomp_state_handlers = {
    khompInit,          /*.on_init */
    khompRouting,       /*.on_routing */
    khompExecute,       /*.on_execute */
    khompHangup,        /*.on_hangup */
    khompExchangeMedia, /*.on_exchange_media */
    khompSoftExecute,   /*.on_soft_execute */
    NULL,               /*.on_consume_media */
    NULL,               /*.on_hibernate */
    NULL,               /*.on_reset */
    NULL,               /*.on_park*/
    NULL,               /*.on_reporting*/
    khompDestroy        /*.on_destroy*/
};

switch_status_t khompSMSDestroy(switch_core_session_t *session);

switch_state_handler_table_t khomp_sms_state_handlers = {
    khompInit,          /*.on_init */
    khompRouting,       /*.on_routing */
    khompExecute,       /*.on_execute */
    NULL,               /*.on_hangup */
    khompExchangeMedia, /*.on_exchange_media */
    khompSoftExecute,   /*.on_soft_execute */
    NULL,               /*.on_consume_media */
    NULL,               /*.on_hibernate */
    NULL,               /*.on_reset */
    NULL,               /*.on_park*/
    NULL,               /*.on_reporting*/
    khompSMSDestroy     /*.on_destroy*/
};

switch_status_t khompPRHangup(switch_core_session_t *session);

switch_state_handler_table_t khomp_pr_state_handlers = {
    khompInit,          /*.on_init */
    khompRouting,       /*.on_routing */
    khompExecute,       /*.on_execute */
    khompPRHangup,      /*.on_hangup */
    khompExchangeMedia, /*.on_exchange_media */
    khompSoftExecute,   /*.on_soft_execute */
    NULL,               /*.on_consume_media */
    NULL,               /*.on_hibernate */
    NULL,               /*.on_reset */
    NULL,               /*.on_park*/
    NULL,               /*.on_reporting*/
    khompDestroy        /*.on_destroy*/
};

/* Callbacks for FreeSWITCH */
switch_call_cause_t khompCall(
        switch_core_session_t *session,
        switch_event_t *var_event,
        switch_caller_profile_t *outbound_profile,
        switch_core_session_t **new_session,
        switch_memory_pool_t **pool,
        switch_originate_flag_t flags,
        switch_call_cause_t *cancel_cause);
switch_status_t khompRead(switch_core_session_t *session,
        switch_frame_t **frame,
        switch_io_flag_t flags,
        int stream_id);
switch_status_t khompWrite(switch_core_session_t *session,
        switch_frame_t *frame,
        switch_io_flag_t flags,
        int stream_id);
switch_status_t khompKill(switch_core_session_t *session,
        int sig);
switch_status_t khompDigit(switch_core_session_t *session,
        const switch_dtmf_t *dtmf);
switch_status_t khompReceiveMessage(switch_core_session_t *session,
        switch_core_session_message_t *msg);
switch_status_t khompReceiveEvent(switch_core_session_t *session,
        switch_event_t *event);

/*!
 \ingroup fs_states
 */
switch_io_routines_t khomp_io_routines = {
    khompCall,           /*.outgoing_channel */
    khompRead,           /*.read_frame */
    khompWrite,          /*.write_frame */
    khompKill,           /*.kill_channel */
    khompDigit,          /*.send_dtmf */
    khompReceiveMessage, /*.receive_message */
    khompReceiveEvent    /*.receive_event */
};

switch_io_routines_t khomp_sms_io_routines = {
    NULL,           /*.outgoing_channel */
    NULL,           /*.read_frame */
    NULL,           /*.write_frame */
    khompKill,      /*.kill_channel */
    NULL,           /*.send_dtmf */
    NULL,           /*.receive_message */
    NULL            /*.receive_event */
};

switch_status_t khompPRWrite(switch_core_session_t *session,
        switch_frame_t *frame,
        switch_io_flag_t flags,
        int stream_id);

switch_io_routines_t khomp_pr_io_routines = {
    NULL,                /*.outgoing_channel */
    khompRead,           /*.read_frame */
    khompPRWrite,        /*.write_frame */
    khompKill,           /*.kill_channel */
    NULL,                /*.send_dtmf */
    NULL,                /*.receive_message */
    NULL                 /*.receive_event */
};

/* Macros to define specific API functions */
SWITCH_STANDARD_API(apiKhomp);

/*!
 \brief Print link status. [khomp show links]
 */
void apiPrintLinks(switch_stream_handle_t* stream, unsigned int device,
        unsigned int link);
/*!
 \brief Print board channel status. [khomp show channels]
 */
void apiPrintChannels(switch_stream_handle_t* stream);

/*!
   \brief State methods they get called when the state changes to the specific state
   returning SWITCH_STATUS_SUCCESS tells the core to execute the standard state method next
   so if you fully implement the state you can return SWITCH_STATUS_FALSE to skip it.
*/
switch_status_t khompInit(switch_core_session_t *session)
{
    DBG(FUNC, "CHANNEL INIT")
    switch_channel_t *channel = switch_core_session_get_channel(session);

    if(!channel)
    {
        DBG(FUNC, D("Channel is NULL"));
        return SWITCH_STATUS_FALSE;
    }

    /* Move channel's state machine to ROUTING. This means the call is trying
       to get from the initial start where the call because, to the point
       where a destination has been identified. If the channel is simply
       left in the initial state, nothing will happen. */
    switch_channel_set_state(channel, CS_ROUTING);

    return SWITCH_STATUS_SUCCESS;
}

switch_status_t khompRouting(switch_core_session_t *session)
{
    DBG(FUNC, "CHANNEL ROUTING");
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t khompExecute(switch_core_session_t *session)
{

    DBG(FUNC, "CHANNEL EXECUTE");
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t khompHangup(switch_core_session_t *session)
{
    Board::KhompPvt *tech_pvt = NULL;

    if(!session)
    {
        LOG(ERROR, D("cr (Session is NULL)"));
        return SWITCH_STATUS_FALSE;
    }
    
    tech_pvt = static_cast<Board::KhompPvt*>(switch_core_session_get_private(session));
    if(!tech_pvt)
    {
        LOG(ERROR, D("cr (pvt is NULL)"));
        return SWITCH_STATUS_FALSE;
    }
    
    DBG(FUNC, PVT_FMT(tech_pvt->target(), "c"));

    try
    {       
        ScopedPvtLock lock(tech_pvt);

        if(tech_pvt->freeState())
        {
            DBG(FUNC, PVT_FMT(tech_pvt->target(), "r (Already disconnected)"));
            return SWITCH_STATUS_SUCCESS;
        }

        tech_pvt->setHangupCause(SWITCH_CAUSE_NORMAL_CLEARING, true);

        //tech_pvt->doHangup();

        lock.unlock();

        CommandRequest c_req(CommandRequest::COMMAND, CommandRequest::CMD_HANGUP, tech_pvt->target().object);

        Board::board(tech_pvt->target().device)->chanCommandHandler()->write(c_req);

    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(tech_pvt->target(), "r (unable to lock: %s!)") %  err._msg.c_str());
        return SWITCH_STATUS_FALSE;
    }
    catch (K3LAPITraits::invalid_device & err)
    {
        LOG(ERROR, PVT_FMT(tech_pvt->target(), "r (unable to get device: %d!)") % err.device);
        return SWITCH_STATUS_FALSE;
    }

    DBG(FUNC, PVT_FMT(tech_pvt->target(), "r"))

    return SWITCH_STATUS_SUCCESS;
}

switch_status_t khompPRHangup(switch_core_session_t *session)
{
    DBG(FUNC, D("CHANNEL HANGUP"))
}


switch_status_t khompDestroy(switch_core_session_t *session)
{
    if(!session)
    {
        LOG(ERROR, D("cr (session is null)"))
        return SWITCH_STATUS_FALSE;
    }
    
    Board::KhompPvt *tech_pvt = NULL;

    tech_pvt = static_cast<Board::KhompPvt*>(switch_core_session_get_private(session));

    if(!tech_pvt)
    {
        LOG(ERROR, D("cr (pvt is null)"))
        return SWITCH_STATUS_FALSE;
    }

    DBG(FUNC, PVT_FMT(tech_pvt->target(), "c"))

    do
    {
        try
        {
            ScopedPvtLock lock(tech_pvt);

            switch_core_session_set_private(session, NULL);
            tech_pvt->destroyAll();
            
            DBG(FUNC, PVT_FMT(tech_pvt->target(), "r"));

            return SWITCH_STATUS_SUCCESS;
        }
        catch (ScopedLockFailed & err)
        {
            LOG(ERROR, PVT_FMT(tech_pvt->target(), "unable to lock: %s!: try again...") %  err._msg.c_str());
        }

    }
    while(true);
            
    DBG(FUNC, PVT_FMT(tech_pvt->target(), "r (unable to lock!)"));

    return SWITCH_STATUS_FALSE;
}

switch_status_t khompSMSDestroy(switch_core_session_t *session)
{
    DBG(FUNC, D("c"))

    if(!session)
    {
        DBG(FUNC, D("r (session is null)"))
        return SWITCH_STATUS_FALSE;
    }
/*    
    Board::KhompPvt *tech_pvt = NULL;

    tech_pvt = static_cast<Board::KhompPvt*>(switch_core_session_get_private(session));

    if(!tech_pvt)
    {
        DBG(FUNC, D("r (pvt is null)"))
        return SWITCH_STATUS_FALSE;
    }
    
    try
    {       
        ScopedPvtLock lock(tech_pvt);
*/
        switch_core_session_set_private(session, NULL);
/*

    }
    catch (ScopedLockFailed & err)
    {
        DBG(FUNC, PVT_FMT(tech_pvt->target(), "r (unable to lock: %s!)") %  err._msg.c_str());
    }
*/

    DBG(FUNC, D("r"))

    return SWITCH_STATUS_SUCCESS;
}

switch_status_t khompKill(switch_core_session_t *session, int sig)
{
    //DBG(FUNC,FMT("CHANNEL KILL, kill = %d") % sig) 
    Board::KhompPvt *tech_pvt = NULL;

    tech_pvt = static_cast<Board::KhompPvt*>(switch_core_session_get_private(session));

    if(!tech_pvt)
    {
        return SWITCH_STATUS_FALSE;
    }

    switch (sig) 
    {
    case SWITCH_SIG_NONE:
        DBG(FUNC, PVT_FMT(tech_pvt->target(), "CHANNEL KILL,  NONE!"));
        break;
    case SWITCH_SIG_KILL:
        DBG(FUNC, PVT_FMT(tech_pvt->target(), "CHANNEL KILL, SIGKILL!"));
        //switch_thread_cond_signal(tech_pvt->_cond);
        break;
    case SWITCH_SIG_XFER:
        DBG(FUNC, PVT_FMT(tech_pvt->target(), "CHANNEL KILL, SIGXFER!"));
        break;
    case SWITCH_SIG_BREAK:
        DBG(FUNC, PVT_FMT(tech_pvt->target(), "CHANNEL KILL, BREAK!"));
        break;
    default:
        DBG(FUNC, PVT_FMT(tech_pvt->target(), "CHANNEL KILL, WHAT?!"));
        break;
    }

    return SWITCH_STATUS_SUCCESS;
}

switch_status_t khompExchangeMedia(switch_core_session_t *session)
{
    DBG(FUNC, "CHANNEL LOOPBACK")
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t khompSoftExecute(switch_core_session_t *session)
{
    DBG(FUNC, "CHANNEL TRANSMIT")
    return SWITCH_STATUS_SUCCESS;
}

switch_status_t khompDigit(switch_core_session_t *session, const switch_dtmf_t *dtmf)
{
    Board::KhompPvt *tech_pvt = static_cast<Board::KhompPvt*>(switch_core_session_get_private(session));
    if(!tech_pvt)
    {
        LOG(ERROR, D("pvt is NULL"));
        return SWITCH_STATUS_FALSE;
    }
    
    DBG(FUNC, PVT_FMT(tech_pvt->target(), "c (%c)") % dtmf->digit);

    try
    {
        ScopedPvtLock lock(tech_pvt);

        char s[] = { dtmf->digit, '\0' };

        tech_pvt->sendDtmf(s);
    }
    catch(ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(tech_pvt->target(), "r (unable to lock: %s!)") % err._msg.c_str());
        return SWITCH_STATUS_FALSE;
    }

    DBG(FUNC, PVT_FMT(tech_pvt->target(), "r"));

    return SWITCH_STATUS_SUCCESS;
}

switch_status_t khompRead(switch_core_session_t *session, switch_frame_t **frame, switch_io_flag_t flags, int stream_id)
{
    Board::KhompPvt *tech_pvt = NULL;
    //switch_time_t started = switch_time_now();
    //unsigned int elapsed;
    //switch_byte_t *data;

    tech_pvt = static_cast<Board::KhompPvt*>(switch_core_session_get_private(session));
    if(!tech_pvt)
    {
        DBG(FUNC, D("pvt is NULL"))
        return SWITCH_STATUS_FALSE;
    }

    *frame = NULL;

    while (true)
    {
        /*
        if (switch_test_flag(tech_pvt, TFLAG_BREAK))
        {
            switch_clear_flag_locked(tech_pvt, TFLAG_BREAK);

            *frame = tech_pvt->_reader_frames.cng();
            return SWITCH_STATUS_SUCCESS;
        }
        */

        if (tech_pvt->call()->_flags.check(Kflags::LISTEN_UP))
        {
            *frame = tech_pvt->_reader_frames.pick();

            if (!*frame)
            {
                //Reader buffer empty, waiting...
                switch_cond_next();
                continue;
            }
        }
        else
        {
            switch_yield(20000);
            *frame = tech_pvt->_reader_frames.cng();
        }
/*
#ifdef BIGENDIAN
        if (switch_test_flag(tech_pvt, TFLAG_LINEAR))
        {
            switch_swap_linear((*frame)->data, (int) (*frame)->datalen / 2);
        }
#endif
*/
        return SWITCH_STATUS_SUCCESS;
    }

    return SWITCH_STATUS_FALSE;
}

switch_status_t khompWrite(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
    Board::KhompPvt *tech_pvt = NULL;

    tech_pvt = static_cast<Board::KhompPvt*>(switch_core_session_get_private(session));
    if(!tech_pvt)
    {
        LOG(ERROR, D("pvt is NULL"));
        return SWITCH_STATUS_FALSE;
    }

/*
#ifdef BIGENDIAN
    if (switch_test_flag(tech_pvt, TFLAG_LINEAR))
    {
        switch_swap_linear(frame->data, (int) frame->datalen / 2);
    }
#endif
*/

    if (frame) // && frame->flags != SFF_CNG)
    {
        if(frame->datalen != 0 && tech_pvt->call()->_flags.check(Kflags::GEN_CO_RING))
        {
            tech_pvt->call()->_flags.clear(Kflags::GEN_CO_RING);

            try
            {
                Board::board(tech_pvt->target().device)->_timers.del(tech_pvt->call()->_idx_co_ring);
            }
            catch (K3LAPITraits::invalid_device & err)
            {
                LOG(ERROR, PVT_FMT(tech_pvt->target(), "Unable to get device: %d!") % err.device);
            }

            if(tech_pvt->call()->_cadence != Board::KhompPvt::PLAY_VM_TONE)
            {    
                tech_pvt->stopCadence();
            } 
        }

        if (!tech_pvt->_writer_frames.give((const char *)frame->data, (size_t)frame->datalen))
        {
            DBG(STRM, PVT_FMT(tech_pvt->target(), "Writer buffer full!"));
            //Writer buffer full!
        }
    }

    return SWITCH_STATUS_SUCCESS;

}

switch_status_t khompPRWrite(switch_core_session_t *session, switch_frame_t *frame, switch_io_flag_t flags, int stream_id)
{
    /*
    Board::KhompPvt *tech_pvt = NULL;

    tech_pvt = static_cast<Board::KhompPvt*>(switch_core_session_get_private(session));
    if(!tech_pvt)
    {
        DBG(FUNC, D("pvt is NULL"));
        return SWITCH_STATUS_FALSE;
    }
    */

    return SWITCH_STATUS_SUCCESS;
}

switch_status_t khompAnswer(switch_core_session_t *session)
{
    Board::KhompPvt *tech_pvt;
    switch_channel_t *channel = NULL;

    channel = switch_core_session_get_channel(session);

    if(!channel)
    {
        LOG(ERROR, D("channel is NULL"));
        return SWITCH_STATUS_FALSE;
    }

    tech_pvt = static_cast<Board::KhompPvt*>(switch_core_session_get_private(session));
    
    if(!tech_pvt)
    {
        LOG(ERROR, D("pvt is NULL"));
        return SWITCH_STATUS_FALSE;
    }

    DBG(FUNC, PVT_FMT(tech_pvt->target(), "CHANNEL ANSWER"));

    try
    {
        ScopedPvtLock lock(tech_pvt);

        if(!tech_pvt->session() || !tech_pvt->call()->_flags.check(Kflags::IS_INCOMING))
        {
            LOG(ERROR, PVT_FMT(tech_pvt->target(), "Channel is not connected"));
            return SWITCH_STATUS_FALSE;
        }

        tech_pvt->getSpecialVariables();

        tech_pvt->setVolume();

        tech_pvt->setCollectCall();

        if (!tech_pvt->call()->_flags.check(Kflags::CONNECTED))
        {
            /* we can unlock it now */
            lock.unlock();

            CommandRequest c_req(CommandRequest::COMMAND, CommandRequest::CMD_ANSWER, tech_pvt->target().object);

            Board::board(tech_pvt->target().device)->chanCommandHandler()->write(c_req);

        }
        else
        {
            DBG(FUNC, PVT_FMT(tech_pvt->target(), "channel is already connected"));
        }

        //Esperar o atendimento EV_CONNECT
        //if (!khomp_pvt::loop_while_flag_timed(pvt, c, kflags::REALLY_CONNECTED, timeout, &lock, false)) { return SWITCH_STATUS_FALSE; }


    }
    catch (K3LAPITraits::invalid_device & err)
    {
        LOG(ERROR, D("unable to get device: %d!") % err.device);
        return SWITCH_STATUS_FALSE;
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(tech_pvt->target(), "unable to lock: %s!") % err._msg.c_str());
        return SWITCH_STATUS_FALSE;

    }

    return SWITCH_STATUS_SUCCESS;
}


switch_status_t khompReceiveMessage(switch_core_session_t *session, switch_core_session_message_t *msg)
{
    Board::KhompPvt *tech_pvt;
    
    if(!session)
    {
        LOG(ERROR, D("session is NULL"));
        return SWITCH_STATUS_FALSE;
    }

    if(!msg)
    {
        LOG(ERROR, D("msg is NULL"));
        return SWITCH_STATUS_FALSE;
    }

    tech_pvt = (Board::KhompPvt *) switch_core_session_get_private(session);
    if(!tech_pvt)
    {
        LOG(ERROR, D("pvt is NULL"));
        return SWITCH_STATUS_FALSE;
    }
    
    switch (msg->message_id) {
        case SWITCH_MESSAGE_REDIRECT_AUDIO:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "SWITCH_MESSAGE_REDIRECT_AUDIO")); 
            break;
        case SWITCH_MESSAGE_TRANSMIT_TEXT:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "SWITCH_MESSAGE_TRANSMIT_TEXT")); 
            break;
        case SWITCH_MESSAGE_INDICATE_ANSWER:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "SWITCH_MESSAGE_INDICATE_ANSWER")); 
            return khompAnswer(session);
            break;
        case SWITCH_MESSAGE_INDICATE_PROGRESS:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "SWITCH_MESSAGE_INDICATE_PROGRESS")); 
            tech_pvt->indicateProgress();
            break;
        case SWITCH_MESSAGE_INDICATE_BRIDGE:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "SWITCH_MESSAGE_INDICATE_BRIDGE")); 
            break;
        case SWITCH_MESSAGE_INDICATE_UNBRIDGE:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "SWITCH_MESSAGE_INDICATE_UNBRIDGE")); 
            break;
        case SWITCH_MESSAGE_INDICATE_TRANSFER:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "SWITCH_MESSAGE_INDICATE_TRANSFER")); 
            break;
        case SWITCH_MESSAGE_INDICATE_RINGING:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "SWITCH_MESSAGE_INDICATE_RINGING")); 
            tech_pvt->indicateRinging();
            break;
        case SWITCH_MESSAGE_INDICATE_MEDIA:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "SWITCH_MESSAGE_INDICATE_MEDIA")); 
            break;
        case SWITCH_MESSAGE_INDICATE_NOMEDIA:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "SWITCH_MESSAGE_INDICATE_NOMEDIA")); 
            break;
        case SWITCH_MESSAGE_INDICATE_HOLD:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "SWITCH_MESSAGE_INDICATE_HOLD")); 
            break;
        case SWITCH_MESSAGE_INDICATE_UNHOLD:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "SWITCH_MESSAGE_INDICATE_UNHOLD")); 
            break;
        case SWITCH_MESSAGE_INDICATE_REDIRECT:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "SWITCH_MESSAGE_INDICATE_REDIRECT")); 
            break;
        case SWITCH_MESSAGE_INDICATE_RESPOND:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "SWITCH_MESSAGE_INDICATE_RESPOND")); 
            break;
        case SWITCH_MESSAGE_INDICATE_BROADCAST:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "SWITCH_MESSAGE_INDICATE_BROADCAST")); 
            break;
        case SWITCH_MESSAGE_INDICATE_MEDIA_REDIRECT:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "SWITCH_MESSAGE_INDICATE_MEDIA_REDIRECT")); 
            break;
        case SWITCH_MESSAGE_INDICATE_DEFLECT:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "SWITCH_MESSAGE_INDICATE_DEFLECT")); 
            break;
        case SWITCH_MESSAGE_INDICATE_VIDEO_REFRESH_REQ:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "SWITCH_MESSAGE_INDICATE_VIDEO_REFRESH_REQ")); 
            break;
        case SWITCH_MESSAGE_INDICATE_DISPLAY:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "SWITCH_MESSAGE_INDICATE_DISPLAY")); 
            break;
        case SWITCH_MESSAGE_INDICATE_TRANSCODING_NECESSARY:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "SWITCH_MESSAGE_INDICATE_TRANSCODING_NECESSARY")); 
            break;
        case SWITCH_MESSAGE_INDICATE_AUDIO_SYNC:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "SWITCH_MESSAGE_INDICATE_AUDIO_SYNC")); 
            break;
        case SWITCH_MESSAGE_INDICATE_REQUEST_IMAGE_MEDIA:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "SWITCH_MESSAGE_INDICATE_REQUEST_IMAGE_MEDIA")); 
            break;
        case SWITCH_MESSAGE_INDICATE_UUID_CHANGE:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "SWITCH_MESSAGE_INDICATE_UUID_CHANGE")); 
            break;
        case SWITCH_MESSAGE_INDICATE_SIMPLIFY:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "SWITCH_MESSAGE_INDICATE_SIMPLIFY")); 
            break;
        case SWITCH_MESSAGE_INDICATE_DEBUG_MEDIA:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "SWITCH_MESSAGE_INDICATE_DEBUG_MEDIA")); 
            break;
        case SWITCH_MESSAGE_INDICATE_PROXY_MEDIA:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "SWITCH_MESSAGE_INDICATE_PROXY_MEDIA")); 
            break;
        case SWITCH_MESSAGE_INDICATE_APPLICATION_EXEC:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "SWITCH_MESSAGE_INDICATE_APPLICATION_EXEC")); 
            break;
        case SWITCH_MESSAGE_INDICATE_APPLICATION_EXEC_COMPLETE:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "SWITCH_MESSAGE_INDICATE_APPLICATION_EXEC_COMPLETE")); 
            break;
        case SWITCH_MESSAGE_INVALID:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "SWITCH_MESSAGE_INVALID")); 
            break;
        default:
            DBG(FUNC, PVT_FMT(tech_pvt->_target, "unknown message received [%d].") % msg->message_id % (msg->from ? msg->from : 0)); 
            break;
    }

    return SWITCH_STATUS_SUCCESS;
}

/*!
  \brief Make sure when you have 2 sessions in the same scope that you pass
  the appropriate one to the routines that allocate memory or you will have
  1 channel with memory allocated from another channel's pool!
*/
switch_call_cause_t khompCall
        (switch_core_session_t *session,
         switch_event_t *var_event,
         switch_caller_profile_t *outbound_profile,
         switch_core_session_t **new_session,
         switch_memory_pool_t **pool,
         switch_originate_flag_t flags,
         switch_call_cause_t *cancel_cause)
{
    if (!outbound_profile)
    {
        LOG(ERROR, D("No caller profile"));
        //switch_core_session_destroy(new_session);
        return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
    }

    Board::KhompPvt *tech_pvt;
    int cause = (int)SWITCH_CAUSE_NONE;
    
    try
    {
        ScopedAllocLock alloc_lock;

        // got the pvt
        tech_pvt = processDialString(outbound_profile->destination_number,&cause);

        if(tech_pvt == NULL || cause != SWITCH_CAUSE_SUCCESS)
        {
            LOG(ERROR, D("unable to find free channel"));
            return (cause == SWITCH_CAUSE_SUCCESS) ? SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER : (switch_call_cause_t) cause;
        }

        ScopedPvtLock lock(tech_pvt);

        if(tech_pvt->justAlloc(false, pool) != SWITCH_STATUS_SUCCESS)
        {
            tech_pvt->cleanup(Board::KhompPvt::CLN_HARD);
            LOG(ERROR, PVT_FMT(tech_pvt->target(), "Initilization Error!"));
            return SWITCH_CAUSE_UNALLOCATED_NUMBER; 
        }

        tech_pvt->owner((session ? session : tech_pvt->session()));
        // get the session to FS
        *new_session = tech_pvt->session();

        if(tech_pvt->justStart(outbound_profile) != SWITCH_STATUS_SUCCESS)
        {
            *new_session = NULL;
            tech_pvt->cleanup(Board::KhompPvt::CLN_HARD);
            LOG(ERROR, PVT_FMT(tech_pvt->target(), "unable to justStart"));
            return SWITCH_CAUSE_UNALLOCATED_NUMBER;
        }

        alloc_lock.unlock();
        
        tech_pvt->getSpecialVariables();

        tech_pvt->setVolume();
        
        if(tech_pvt->makeCall() != ksSuccess)
        {
            *new_session = NULL;
            tech_pvt->cleanup(Board::KhompPvt::CLN_HARD);
            LOG(ERROR, PVT_FMT(tech_pvt->target(), "unable to makeCall"));
            return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
        }

        tech_pvt->setFSChannelVar("KCallAnswerInfo", "Unknown");
    }
    catch(ScopedLockFailed & err)
    {
        if(err._fail == ScopedLockFailed::ALLOC_FAILED)
        {
            LOG(ERROR, D("unable to global alloc lock: %s!") % err._msg.c_str());
        }
        else
        {
            tech_pvt->cleanup(Board::KhompPvt::CLN_HARD);
            LOG(ERROR, PVT_FMT(tech_pvt->target(), "unable to lock: %s!") % err._msg.c_str());
        }
        return SWITCH_CAUSE_INTERWORKING;
    }
    catch(Board::KhompPvt::InvalidSwitchChannel & err)
    {
        LOG(ERROR, PVT_FMT(tech_pvt->target(), "%s") % err._msg.c_str());
    }

    return SWITCH_CAUSE_SUCCESS;
}

switch_status_t khompReceiveEvent(switch_core_session_t *session, switch_event_t *event)
{
    struct Board::KhompPvt *tech_pvt = static_cast<Board::KhompPvt*>(switch_core_session_get_private(session));
    char *body = switch_event_get_body(event);
    //switch_assert(tech_pvt != NULL);

    if(!tech_pvt)
    {
        LOG(ERROR, D("pvt is NULL"));
        return SWITCH_STATUS_FALSE;
    }
    
    DBG(FUNC, PVT_FMT(tech_pvt->target(), "Receive Event id[%d] name[%s] body=[%s]") % event->event_id % event->headers->name % body); 

    if (!body) {
        body = (char *)"";
    }

    return SWITCH_STATUS_SUCCESS;
}

/* A simple idea of app */
SWITCH_STANDARD_APP(klogFunction)
{
    if (zstr(data)) 
    {
        LOG(ERROR, D("No data specified"));
    } 
    else 
    {
        LOG(MESSAGE, FMT("KLOG: %s") %  std::string(data));
    }
}

void setVariable(switch_core_session_t * session, const char * variable, const char * data)
{
    if(!session)
    {
        LOG(ERROR, FMT("Session is NULL in %s") % variable);
        return;
    }

    if (zstr(data)) 
    {
        LOG(ERROR, FMT("No %s specified") % variable);
        return;
    } 

    if(strcasecmp(data, "on") && strcasecmp(data, "off"))
    {
        LOG(ERROR, FMT("On %s specify on or off") % variable);
        return;
    }

    switch_channel_t *c = switch_core_session_get_channel(session);

    if(!c)
    {
        LOG(ERROR, FMT("Channel is NULL in %s") % variable);
        return;
    }

    switch_channel_set_variable(c, variable, (!strcasecmp(data, "on") ? "true" : "false" ));

    DBG(FUNC,FMT("%s set %s") % variable % data);
}

SWITCH_STANDARD_APP(kdtmfSuppressionFunction)
{
    setVariable(session, "KDTMFSuppression", data);
}

SWITCH_STANDARD_APP(kechoCancellerFunction)
{
    setVariable(session, "KEchoCanceller", data);
}

SWITCH_STANDARD_APP(kautoGainControlFunction)
{
    setVariable(session, "KAutoGainControl", data);
}

SWITCH_STANDARD_APP(kr2SendConditionFunction)
{
    setVariable(session, "KR2SendCondition", data);
}

SWITCH_STANDARD_APP(kadjustForFaxFunction)
{
    if(!session)
    {
        LOG(ERROR, D("Session is NULL"));
        return;
    }
    
    Board::KhompPvt *tech_pvt = static_cast<Board::KhompPvt*>(switch_core_session_get_private(session));

    if(!tech_pvt)
    {
        LOG(ERROR, D("pvt is NULL"));
        return;
    }

    DBG(FUNC, PVT_FMT(tech_pvt->target(), "Application KAdjustForFax"));

    try
    {
        ScopedPvtLock lock(tech_pvt);

        if(tech_pvt->application(FAX_ADJUST, session, data))
        {
            /* adjust the flag */
            tech_pvt->call()->_flags.set(Kflags::FAX_DETECTED);
        }
    }
    catch (ScopedLockFailed & err)
    {
        LOG(ERROR, PVT_FMT(tech_pvt->target(), "unable to lock: %s!") %  err._msg.c_str());
        return;
    }
}

SWITCH_STANDARD_APP(ksendFaxFunction)
{
    if(!session)
    {
        LOG(ERROR, D("Session is NULL"));
        return;
    }
    

    Board::KhompPvt *tech_pvt = static_cast<Board::KhompPvt*>(switch_core_session_get_private(session));

    if(!tech_pvt)
    {
        LOG(ERROR, D("pvt is NULL"));
        return;
    }

    DBG(FUNC, PVT_FMT(tech_pvt->target(), "Application KSendFax"));

    tech_pvt->application(FAX_SEND, session, data);
}

SWITCH_STANDARD_APP(kreceiveFaxFunction)
{
    if(!session)
    {
        LOG(ERROR, D("Session is NULL"));
        return;
    }
    

    Board::KhompPvt *tech_pvt = static_cast<Board::KhompPvt*>(switch_core_session_get_private(session));

    if(!tech_pvt)
    {
        LOG(ERROR, D("pvt is NULL"));
        return;
    }
    
    DBG(FUNC, PVT_FMT(tech_pvt->target(), "Application KReceiveFax"));

    tech_pvt->application(FAX_RECEIVE, session, data);
}

SWITCH_STANDARD_APP(kUserTransferFunction)
{
    if(!session)
    {
        LOG(ERROR, D("Session is NULL"));
        return;
    }
    
    if (zstr(data)) 
    {
        LOG(ERROR, D("No number specified"));
        return;
    } 

    Board::KhompPvt *tech_pvt = static_cast<Board::KhompPvt*>(switch_core_session_get_private(session));

    if(!tech_pvt)
    {
        DBG(FUNC, D("pvt is NULL"));
        return;
    }
    
    DBG(FUNC, PVT_FMT(tech_pvt->target(), "Application KUserTransfer"));

    tech_pvt->application(USER_TRANSFER, session, data);
}

SWITCH_STANDARD_APP(ksetVolumeFunction)
{
    if(!session)
    {
        LOG(ERROR, D("Session is NULL"));
        return;
    }

    if (zstr(data)) 
    {
        LOG(ERROR, D("No KSetVolume specified"));
        return;
    } 

    switch_channel_t *c = switch_core_session_get_channel(session);

    if(!c)
    {
        LOG(ERROR, D("Channel is NULL"));
        return;
    }

    switch_channel_set_variable(c, "KSetVolume", data);

    DBG(FUNC, FMT("KSetVolume set %s") % data);
}

SWITCH_STANDARD_APP(kSendSMSFunction)
{
    if(!session)
    {
        LOG(ERROR, D("Session is NULL"));
        return;
    }

    switch_channel_t *channel = switch_core_session_get_channel(session);

    if(!channel)
    {
        LOG(ERROR, D("Channel is NULL"));
        return;
    }

    switch_channel_set_variable(channel, "KSmsDelivered", "no");
    switch_channel_set_variable(channel, "KSmsErrorCode", "42");
    switch_channel_set_variable(channel, "KSmsErrorName", Verbose::gsmSmsCause((KGsmSmsCause)42).c_str());

    if (zstr(data))
    {
        LOG(ERROR, D("No data specified"));
        return;
    }

    int cause = (int)SWITCH_CAUSE_SUCCESS;

    try
    {
        ScopedAllocLock alloc_lock;

        Board::KhompPvt *tech_pvt = processSMSString(data, &cause);

        if(!tech_pvt)
        {
            DBG(FUNC, D("pvt is NULL, cause '%d'") % cause);
            return;
        }

        DBG(FUNC, PVT_FMT(tech_pvt->target(), "Application KSendSMS"));

        ScopedPvtLock lock(tech_pvt);

        alloc_lock.unlock();
        
        tech_pvt->application(SMS_SEND, session, data);

    }
    catch(ScopedLockFailed & err)
    {
        if(err._fail == ScopedLockFailed::ALLOC_FAILED)
        {
            LOG(ERROR, D("unable to global alloc lock"));
        }
        else
        {
            LOG(ERROR, D("unable to lock: %s!") % err._msg.c_str());
        }
    }

}

SWITCH_STANDARD_APP(kSelectSimCardFunction)
{
    if(!session)
    {
        LOG(ERROR, D("Session is NULL"));
        return;
    }

    if (zstr(data))
    {    
        LOG(ERROR, D("invalid number of arguments"));
        return;
    }    

    int dev = -1;
    int obj = -1;

    try
    {
        std::string datastr((const char*) data);
        Strings::trim(datastr);

        Strings::vector_type params;
        Strings::tokenize(datastr, params, "|,",3);

        std::string num("0");

        if (params.size() == 3)
        {    
            dev = Strings::tolong(params[0]);
            obj = Strings::tolong(params[1]);
            num = params[2];

            /* just check for validity */
            (void)Strings::tolong(params[2]);
        }    
        else
        {
            LOG(ERROR, D("Invalid number of arguments!"));
            return;
        }

        if (!Globals::k3lapi.valid_device(dev))
        {
            LOG(ERROR, D("ERROR: Invalid device number '%d'!") % dev);
            return;
        }

        if (!Globals::k3lapi.valid_channel(dev, obj))
        {
            LOG(ERROR, D("ERROR: Invalid channel number '%d' on device '%d'!") % obj % dev);
            return;
        }

        if(!Board::get(dev, obj)->application(SELECT_SIM_CARD, NULL, num.c_str()))
        {
            LOG(ERROR, D("ERROR: Unable to select sim card"));
        }
    }
    catch (Strings::invalid_value & e)
    {
        LOG(ERROR, D("invalid numeric value: %s") % e.value());
    }
    catch(...)
    {
        LOG(ERROR, D("ERROR: Unable to select sim card"));
    }
}

SWITCH_MODULE_LOAD_FUNCTION(mod_khomp_load)
{
    /* start log early */
    if(!K::Logger::start())
    {
        return SWITCH_STATUS_FALSE;
    }

    /* Two bodies can not occupy the same space at the same time */ 
    if(switch_loadable_module_exists("mod_kommuter") == SWITCH_STATUS_SUCCESS)
    {   
        LOG(ERROR, D("Kommuter module for FreeSWITCH is already loaded."));
        return SWITCH_STATUS_FALSE;
    } 

    //TODO: must put the autorevision variable here
    LOG(MESSAGE, "loading Khomp module");

    /* get a reference for the module pool */
    Globals::module_pool = pool;

    /* start config system! */
    Opt::initialize();

    if(!Board::initializeK3L())
    {
        return SWITCH_STATUS_TERM;
    }

    /* read configuration first */
    Opt::obtain();

    /*
       Spawn our k3l global var that will be used along the module
       for sending info to the boards
    */

    if(!Board::initialize())
    {
        LOG(ERROR, "Error while initialize Board struct");
        return SWITCH_STATUS_TERM;
    }

    /* apply global volume now */
    if(Opt::_options._input_volume() != 0 || Opt::_options._output_volume() != 0)
        Board::applyGlobalVolume();

    Opt::commit();

    /* create module interface, set state handlers and give a name */
    *module_interface = switch_loadable_module_create_module_interface(pool, "mod_khomp");
    Globals::khomp_endpoint_interface = static_cast<switch_endpoint_interface_t*>(switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE));
    Globals::khomp_endpoint_interface->interface_name = "khomp";
    Globals::khomp_endpoint_interface->io_routines = &khomp_io_routines;
    Globals::khomp_endpoint_interface->state_handler = &khomp_state_handlers;


    Globals::khomp_sms_endpoint_interface = static_cast<switch_endpoint_interface_t*>(switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE));
    Globals::khomp_sms_endpoint_interface->interface_name = "khomp_SMS";
    Globals::khomp_sms_endpoint_interface->io_routines = &khomp_sms_io_routines;
    Globals::khomp_sms_endpoint_interface->state_handler = &khomp_sms_state_handlers;
    
    Globals::khomp_pr_endpoint_interface = static_cast<switch_endpoint_interface_t*>(switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE));
    Globals::khomp_pr_endpoint_interface->interface_name = "khomp_PR";
    Globals::khomp_pr_endpoint_interface->io_routines = &khomp_pr_io_routines;
    Globals::khomp_pr_endpoint_interface->state_handler = &khomp_pr_state_handlers;

    /* Register cli commands */
    Cli::registerCommands(apiKhomp,module_interface);
    
    ESL::registerEvents();

    /* Add applications */
    SWITCH_ADD_APP(Globals::khomp_app_inteface, "KLog", "KLog", "KLog log every string to khomp log system", klogFunction, "<value>", SAF_SUPPORT_NOMEDIA);
    SWITCH_ADD_APP(Globals::khomp_app_inteface, "KDTMFSuppression", "KDTMFSuppression", "KDTMFSuppresion", kdtmfSuppressionFunction, "on|off", SAF_SUPPORT_NOMEDIA);
    SWITCH_ADD_APP(Globals::khomp_app_inteface, "KEchoCanceller", "KEchoCanceller", "KEchoCanceller", kechoCancellerFunction, "on|off", SAF_SUPPORT_NOMEDIA);
    SWITCH_ADD_APP(Globals::khomp_app_inteface, "KAutoGainControl", "KAutoGainControl", "KAutoGainControl", kautoGainControlFunction, "on|off", SAF_SUPPORT_NOMEDIA);
    SWITCH_ADD_APP(Globals::khomp_app_inteface, "KSetVolume", "KSetVolume", "KSetVolume", ksetVolumeFunction, "value|value", SAF_SUPPORT_NOMEDIA);
    SWITCH_ADD_APP(Globals::khomp_app_inteface, "KR2SendCondition", "KR2SendCondition", "KR2SendCondition", kr2SendConditionFunction, "value|value", SAF_SUPPORT_NOMEDIA);
    
    SWITCH_ADD_APP(Globals::khomp_app_inteface, "KAdjustForFax", "KAdjustForFax", "KAdjustForFax", kadjustForFaxFunction, "", SAF_SUPPORT_NOMEDIA);
    SWITCH_ADD_APP(Globals::khomp_app_inteface, "KSendFax", "KSendFax", "KSendFax", ksendFaxFunction, "<infilename>[:<infilename2>[:...]][|<faxid>]", SAF_SUPPORT_NOMEDIA);
    SWITCH_ADD_APP(Globals::khomp_app_inteface, "KReceiveFax", "KReceiveFax", "KReceiveFax", kreceiveFaxFunction, "<outfilename>[|<faxid>]", SAF_SUPPORT_NOMEDIA);
    
    SWITCH_ADD_APP(Globals::khomp_app_inteface, "KUserTransfer", "KUserTransfer", "KUserTransfer", kUserTransferFunction, "number", SAF_SUPPORT_NOMEDIA);
    
    SWITCH_ADD_APP(Globals::khomp_app_inteface, "KSendSMS", "KSendSMS", "KSendSMS", kSendSMSFunction, "resource|destination|message", SAF_SUPPORT_NOMEDIA);

    SWITCH_ADD_APP(Globals::khomp_app_inteface, "KSelectSimCard", "KSelectSimCard", "KSelectSimCard", kSelectSimCardFunction, "<board>|<channel>|<sim_card>", SAF_SUPPORT_NOMEDIA);

    Board::initializeHandlers();

    Board::kommuter.start();

    /* indicate that the module should continue to be loaded */
    return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_khomp_shutdown)
{
    Board::kommuter.stop();

    Cli::unregisterCommands();
    
    ESL::unregisterEvents();


    /* Finnish him! */
    DBG(FUNC, "Unloading mod_khomp...")

    Board::finalizeHandlers();

    Board::finalize();
    
    DBG(FUNC, "Successfully Unloaded mod_khomp")

    K::Logger::stop();

    return SWITCH_STATUS_SUCCESS;
}

/*!
   \brief khomp API definition
   TODO: Add as xml modifier
*/
SWITCH_STANDARD_API(apiKhomp)
{
    char *argv[10] = { 0 };
    int argc = 0;
    void *val;
    char *myarg = NULL;
    switch_status_t status = SWITCH_STATUS_SUCCESS;

    /* We should not ever get a session here */
    if (session) return SWITCH_STATUS_FALSE;

    /* Set the stream for Cli class */
    Cli::setStream(stream);

    if (zstr(cmd))
    {
        Cli::printKhompUsage();
        return status;
    }

    if (!(myarg = strdup(cmd))) return SWITCH_STATUS_MEMERR;


    if ((argc = switch_separate_string(myarg, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) < 1) {
        Cli::printKhompUsage();
        goto done;
    }

    /* Parse all commands */
    Cli::parseCommands(argc,argv);

done:
    switch_safe_free(myarg);
    return status;
}

extern "C" void Kstdcall khompAudioListener (int32 deviceid, int32 objectid, byte * read_buffer, int32 read_size)
{
    try
    {
    
    //If NULL get throws K3LAPITraits::invalid_channel exception.
    Board::KhompPvt * pvt = Board::get(deviceid, objectid);

    /* add listener audio to the read buffer */
    if (!pvt->_reader_frames.give((const char *)read_buffer, read_size))
    {
        DBG(STRM, PVT_FMT(pvt->target(), "Reader buffer full (read_size: %d)") % read_size);
    }

    if (!pvt->call()->_flags.check(Kflags::STREAM_UP))
    {
        DBG(STRM, PVT_FMT(pvt->target(), "Stream not enabled, skipping write..."));
        return;
    }
    
    /* will be used below for CM_ADD_STREAM_BUFFER */
    struct
    {
        const byte * buff;
        size_t       size;
    }
    write_packet = { (const byte *)0, 0 };
    
    /* push audio from the write buffer */
    switch_frame_t * fr = pvt->_writer_frames.pick();

    if (!fr)
    {
        //Writer buffer empty!
        DBG(STRM, PVT_FMT(pvt->target(), "Writer buffer empty (silence to board)"));

        write_packet.buff = (const byte *) Board::_cng_buffer;
        write_packet.size = (size_t)       Globals::boards_packet_size;

        pvt->command(KHOMP_LOG, CM_ADD_STREAM_BUFFER,
                (const char *)&write_packet);
        return;
    }


    /* what is the frame type? */
    switch (fr->flags)
    {
        case SFF_NONE:
        {
            write_packet.buff = (const byte *) fr->data;
            write_packet.size = (size_t)       fr->datalen;


            pvt->command(KHOMP_LOG, CM_ADD_STREAM_BUFFER,
                    (const char *)&write_packet);

            break;
        }

        case SFF_CNG:
        {
            write_packet.buff = (const byte *) Board::_cng_buffer;
            write_packet.size = (size_t)       Globals::boards_packet_size;

            pvt->command(KHOMP_LOG, CM_ADD_STREAM_BUFFER,
                    (const char *)&write_packet);

            break;
        }

        default:
            DBG(FUNC, PVT_FMT(pvt->target(), "DROPPING AUDIO..."));
            /* TODO: log something here... */
            break;
    }

    }
    catch (K3LAPITraits::invalid_channel & err)
    {
        LOG(ERROR, OBJ_FMT(deviceid, objectid, "Invalid channel..."));
        return;
    }
    catch (...)
    {
        LOG(ERROR, OBJ_FMT(deviceid, objectid, "Unexpected error..."));
        return;
    }
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet expandtab:
 */
