/*
 * mod_opalh323.cpp
 *
 * Opal-H323 gluer for Freeswitch
 * This file implements fontend of OpalH323 module functions
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
 * $Log: mod_opalh323.cpp,v $
 *
 * Revision 1.00  2007/10/24 07:29:52  lzwierko
 * Initial revision
 */


#include "mod_opalh323.h"
#inlcude "opalh323_backend.h"
#include <switch.h>
   
/*
 * IO routines handlers definitions 
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
 * Event handlers declaration
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
static FSOpalManager *opalh323_manager = NULL;
static switch_memory_pool_t *opalh323_pool = NULL;
static switch_endpoint_interface_t *opalh323_endpoint_interface;

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
SWITCH_MODULE_LOAD_FUNCTION(mod_opalh323_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_opalh323_shutdown);
SWITCH_MODULE_DEFINITION(mod_opalh323, mod_opalh323_load, mod_opalh323_shutdown, NULL);
    
/*
 * Pointer to endpoint interface descriptor for this module 
 */
switch_endpoint_interface_t *opalh323_endpoint_interface;


/*
 * This function is called on module load
 * It sets up: 
 * 1. frontend - interface to FreeSWITCH core
 * 2. backend - inerface to OPAL core
 *
 */
SWITCH_MODULE_LOAD_FUNCTION(mod_opalh323_load)
{
       
    /* frontend initialization*/
    *module_interface =NULL;
    *module_interface = switch_loadable_module_create_module_interface(pool, modname);
    
    assert(*module_interface);
    if(!module_interface)
    {     
        return SWITCH_STATUS_MEMERR;
    }
    opalh323_endpoint_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
    opalh323_endpoint_interface->interface_name = "OPALH323";
    opalh323_endpoint_interface->io_routines = &opalh323_io_routines;
    opalh323_endpoint_interface->state_handler = &opalh323_event_handlers;
   
    /* backend initialization */
    opalh323_manager = new FSOpalManager();
    assert(opalh323_manager);
    if(!opalh323_manager)
    {
        return SWITCH_STATUS_MEMERR;
    }
    
    if(!opalh323_manager->initialize(pool))
    {
        delete opalh323_manager;
        return SWITCH_STATUS_FALSE; /* if can't initialize return general error */
    }      
    
    /* indicate that the module should continue to be loaded */
    return SWITCH_STATUS_SUCCESS;
}

/*
 * This functionis called on module teardown
 * It releases all internal resources, i.e. 
 * it dealocates OPAL core
 * 
 */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_opalh323_shutdown)
{
    
    /* deallocate OPAL manager */
    delete opalh323_manager;
    
    return SWITCH_STATUS_SUCCESS;
}


/*
 * IO routines handlers definitions
 */
static switch_call_cause_t opalh323_outgoing_channel(switch_core_session_t *, switch_caller_profile_t *, switch_core_session_t **, switch_memory_pool_t **)
{
    return 0;
}

static switch_status_t opalh323_read_frame(switch_core_session_t *, switch_frame_t **, int, switch_io_flag_t, int)
{
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t opalh323_write_frame(switch_core_session_t *, switch_frame_t *, int, switch_io_flag_t, int)
{
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t opalh323_kill_channel(switch_core_session_t *, int)
{
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t opalh323_waitfor_read(switch_core_session_t *, int, int)
{
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t opalh323_waitfor_write(switch_core_session_t *, int, int)
{
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t opalh323_send_dtmf(switch_core_session_t *, char *)
{
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t opalh323_receive_message(switch_core_session_t *, switch_core_session_message_t *)
{
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t opalh323_receive_event(switch_core_session_t *, switch_event_t *)
{
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t opalh323_state_change(switch_core_session_t *)
{
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t opalh323_read_video_frame(switch_core_session_t *, switch_frame_t **, int, switch_io_flag_t, int)
{
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t opalh323_write_video_frame(switch_core_session_t *, switch_frame_t *, int, switch_io_flag_t, int)
{
    return SWITCH_STATUS_SUCCESS;
}

/*
 * Event handlers
 */

static switch_status_t opalh323_on_init(switch_core_session_t *session)
{
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t opalh323_on_ring(switch_core_session_t *session)
{
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t opalh323_on_execute(switch_core_session_t *session)
{
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t opalh323_on_hangup(switch_core_session_t *session)
{
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t opalh323_on_loopback(switch_core_session_t *session)
{
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t opalh323_on_transmit(switch_core_session_t *session)
{
    return SWITCH_STATUS_SUCCESS;
}
