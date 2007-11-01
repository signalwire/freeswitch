/*
 * mod_opalh323.cpp
 *
 * Opal gluer for Freeswitch
 * This file implements fontend of Opal module functions
 * that is all functions that are used for communication
 * between FreeSWITCH core and this module
 *
 *
 * Copyright (c) 2007 Lukasz Zwierko (lzwierko@gmail.com)
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 *
 * The Initial Developer of the Original Code is Lukasz Zwierko (lzwierko@gmail.com)
 *
 * Contributor(s):
 *
 * $Log: mod_opal.cpp,v $
 *
 * Revision 1.00  2007/10/24 07:29:52  Lukasz Zwierko
 * Initial revision
 */

#include <ptlib.h>
#include <ptlib/svcproc.h>
#include "mod_opal.h"
#include "opal_backend.h"
#include <switch.h>
   
/*
 * IO routines handlers definitions for H323
 */
static switch_call_cause_t opalh323_outgoing_channel(switch_core_session_t *, switch_caller_profile_t *, switch_core_session_t **, switch_memory_pool_t **);
static switch_status_t opalh323_read_frame(switch_core_session_t *, switch_frame_t **, int, switch_io_flag_t, int);
static switch_status_t opalh323_write_frame(switch_core_session_t *, switch_frame_t *, int, switch_io_flag_t, int);
static switch_status_t opalh323_kill_channel(switch_core_session_t *, int);
static switch_status_t opalh323_waitfor_read(switch_core_session_t *, int, int);
static switch_status_t opalh323_waitfor_write(switch_core_session_t *, int, int);
static switch_status_t opalh323_send_dtmf(switch_core_session_t *, char *);
static switch_status_t opalh323_receive_message(switch_core_session_t *, switch_core_session_message_t *);
static switch_status_t opalh323_receive_event(switch_core_session_t *, switch_event_t *);
static switch_status_t opalh323_state_change(switch_core_session_t *);
static switch_status_t opalh323_read_video_frame(switch_core_session_t *, switch_frame_t **, int, switch_io_flag_t, int);
static switch_status_t opalh323_write_video_frame(switch_core_session_t *, switch_frame_t *, int, switch_io_flag_t, int);
/*
 * Event handlers declaration for H323
 */
static switch_status_t opalh323_on_init(switch_core_session_t *session);
static switch_status_t opalh323_on_ring(switch_core_session_t *session);
static switch_status_t opalh323_on_execute(switch_core_session_t *session);
static switch_status_t opalh323_on_hangup(switch_core_session_t *session);
static switch_status_t opalh323_on_loopback(switch_core_session_t *session);
static switch_status_t opalh323_on_transmit(switch_core_session_t *session);

/**
 * Declaration of private variables 
 */
static switch_memory_pool_t *opal_pool = NULL;
static switch_endpoint_interface_t *opalh323_endpoint_interface = NULL;
static FSOpalManager *opal_manager = NULL;


/*
 * This is Tuyan's Ozipek neat trick 
 * to initialize PProcess instance
 * which is needed by OpalManager
 */
class _FSOpalProcess : public PProcess
{
   PCLASSINFO(_FSOpalProcess, PProcess)
   public:
       _FSOpalProcess(){PTrace::SetLevel(PSystemLog::Info);}; //just for fun and eyecandy ;)
       void Main() {};
} FSOpalProcess;


/*
 * IO routines handlers set declaration 
 */
static switch_io_routines_t opalh323_io_routines = {
	/*.outgoing_channel */ opalh323_outgoing_channel,
	/*.read_frame */ opalh323_read_frame,
	/*.write_frame */ opalh323_write_frame,
	/*.kill_channel */ opalh323_kill_channel,
	/*.waitfor_read */ opalh323_waitfor_read,
	/*.waitfor_read */ opalh323_waitfor_write,
	/*.send_dtmf */ opalh323_send_dtmf,
	/*.receive_message */ opalh323_receive_message,
	/*.receive_event */ opalh323_receive_event,
	/*.state_change*/ opalh323_state_change,
	/*.read_video_frame*/ opalh323_read_video_frame,
	/*.write_video_frame*/ opalh323_write_video_frame
};

static switch_state_handler_table_t opalh323_event_handlers = {
	/*.on_init */ opalh323_on_init,
	/*.on_ring */ opalh323_on_ring,
	/*.on_execute */ opalh323_on_execute,
	/*.on_hangup */ opalh323_on_hangup,
	/*.on_loopback */ opalh323_on_loopback,
	/*.on_transmit */ opalh323_on_transmit
};

/* 
 * Loadable FreeSWITCH module functions declaration
 */
SWITCH_MODULE_LOAD_FUNCTION(mod_opal_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_opal_shutdown);
SWITCH_MODULE_DEFINITION(mod_opal, mod_opal_load, mod_opal_shutdown, NULL);

/*
 * This function is called on module load
 * It sets up frontend interface to FS
 *
 */
SWITCH_MODULE_LOAD_FUNCTION(mod_opal_load)
{
       
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE,"Starting loading mod_opal\n");
    opal_pool = pool;
    /* frontend initialization*/
    *module_interface =NULL;
    *module_interface = switch_loadable_module_create_module_interface(pool, modname);
    
    assert(*module_interface);
    if(!module_interface)
    {     
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE,"Can not create loadable module interfacer\n");
        return SWITCH_STATUS_MEMERR;
    }
    opalh323_endpoint_interface = (switch_endpoint_interface_t*)switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
    opalh323_endpoint_interface->interface_name = "opalH323";
    opalh323_endpoint_interface->io_routines = &opalh323_io_routines;
    opalh323_endpoint_interface->state_handler = &opalh323_event_handlers;
       
    
    /* backend initialization */
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Begin opal backend init\n");
    opal_manager = new FSOpalManager();
    assert(opal_manager);
    if(!opal_manager) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Can not create opal manger\n");
        return SWITCH_STATUS_MEMERR;
    }
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Opal manager created\n");
    
    if(!opal_manager->initialize(modname, opal_pool, opalh323_endpoint_interface)) {
        delete opal_manager;
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Can not initialize opal manger\n");
        return SWITCH_STATUS_FALSE; /* if can't initialize return general error */
    }
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Opal manager initilaized and running\n");
        
    
    return SWITCH_STATUS_SUCCESS;
}

/*
 * This functionis called on module teardown
 * It releases all internal resources, i.e. 
 * it dealocates OPAL core
 * 
 */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_opal_shutdown)
{
    /* deallocate OPAL manager */
    
    delete opal_manager;
    opal_manager = NULL;
    opal_pool = NULL;
    opalh323_endpoint_interface = NULL;
    return SWITCH_STATUS_SUCCESS;
}

/*
 * IO routines handlers definitions
 */
static switch_call_cause_t opalh323_outgoing_channel(
        switch_core_session_t *session,
	switch_caller_profile_t *outbound_profile,
        switch_core_session_t **new_session, 
        switch_memory_pool_t **pool)
{
     return  opal_manager->io_outgoing_channel(session,outbound_profile,new_session,pool);
}

static switch_status_t opalh323_read_frame(switch_core_session_t *session, switch_frame_t **frame, int timeout, switch_io_flag_t flags, int stream_id)
{
    return opal_manager->io_read_frame(session,frame,timeout,flags,stream_id);
}

static switch_status_t opalh323_write_frame(switch_core_session_t *session, switch_frame_t *frame, int timeout, switch_io_flag_t flags, int stream_id)
{
    return opal_manager->io_write_frame(session,frame,timeout,flags,stream_id);
}

static switch_status_t opalh323_kill_channel(switch_core_session_t *session, int sig)
{
    return opal_manager->io_kill_channel(session,sig);
}

static switch_status_t opalh323_waitfor_read(switch_core_session_t *session, int ms, int stream_id)
{
    return opal_manager->io_waitfor_read(session,ms,stream_id);
}

static switch_status_t opalh323_waitfor_write(switch_core_session_t *session, int ms, int stream_id)
{
    return opal_manager->io_waitfor_write(session,ms,stream_id);
}

static switch_status_t opalh323_send_dtmf(switch_core_session_t *session, char *dtmf)
{
    return opal_manager->io_send_dtmf(session,dtmf);
}

static switch_status_t opalh323_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg)
{
    return opal_manager->io_receive_message(session,msg);
}

static switch_status_t opalh323_receive_event(switch_core_session_t *session, switch_event_t *event)
{
    return opal_manager->io_receive_event(session,event);
}

static switch_status_t opalh323_state_change(switch_core_session_t *session)
{
    return opal_manager->io_state_change(session);
}

static switch_status_t opalh323_read_video_frame(switch_core_session_t *session, switch_frame_t **frame, int timeout, switch_io_flag_t flag, int stream_id)
{
    return opal_manager->io_read_video_frame(session,frame,timeout,flag,stream_id);
}

static switch_status_t opalh323_write_video_frame(switch_core_session_t *session, switch_frame_t *frame, int timeout, switch_io_flag_t flag, int stream_id)
{
    return opal_manager->io_write_video_frame(session,frame,timeout,flag,stream_id);
}

/*
 * Event handlers
 */

static switch_status_t opalh323_on_init(switch_core_session_t *session)
{
    return opal_manager->callback_on_init(session);
}

static switch_status_t opalh323_on_ring(switch_core_session_t *session)
{
    return opal_manager->callback_on_ring(session);
}

static switch_status_t opalh323_on_execute(switch_core_session_t *session)
{
    return opal_manager->callback_on_execute(session);
}

static switch_status_t opalh323_on_hangup(switch_core_session_t *session)
{
    return opal_manager->callback_on_hangup(session);
}

static switch_status_t opalh323_on_loopback(switch_core_session_t *session)
{
    return opal_manager->callback_on_loopback(session);
}

static switch_status_t opalh323_on_transmit(switch_core_session_t *session)
{
    return opal_manager->callback_on_transmit(session);
}
