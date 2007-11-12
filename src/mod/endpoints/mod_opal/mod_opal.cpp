
#include <switch.h>
#include <ptlib.h>
#include <ptlib/svcproc.h>
#include "mod_opal.h"
#include "fsmanager.h"
#include "fscon.h"


static switch_call_cause_t opalfs_outgoing_channel(switch_core_session_t *, switch_caller_profile_t *, switch_core_session_t **, switch_memory_pool_t **);
static switch_status_t opalfs_read_frame(switch_core_session_t *, switch_frame_t **, int, switch_io_flag_t, int);
static switch_status_t opalfs_write_frame(switch_core_session_t *, switch_frame_t *, int, switch_io_flag_t, int);
static switch_status_t opalfs_kill_channel(switch_core_session_t *, int);
static switch_status_t opalfs_waitfor_read(switch_core_session_t *, int, int);
static switch_status_t opalfs_waitfor_write(switch_core_session_t *, int, int);
static switch_status_t opalfs_send_dtmf(switch_core_session_t *, char *);
static switch_status_t opalfs_receive_message(switch_core_session_t *, switch_core_session_message_t *);
static switch_status_t opalfs_receive_event(switch_core_session_t *, switch_event_t *);
static switch_status_t opalfs_state_change(switch_core_session_t *);
static switch_status_t opalfs_read_video_frame(switch_core_session_t *, switch_frame_t **, int, switch_io_flag_t, int);
static switch_status_t opalfs_write_video_frame(switch_core_session_t *, switch_frame_t *, int, switch_io_flag_t, int);



static switch_status_t opalfs_on_init(switch_core_session_t *session);
static switch_status_t opalfs_on_ring(switch_core_session_t *session);
static switch_status_t opalfs_on_execute(switch_core_session_t *session);
static switch_status_t opalfs_on_hangup(switch_core_session_t *session);
static switch_status_t opalfs_on_loopback(switch_core_session_t *session);
static switch_status_t opalfs_on_transmit(switch_core_session_t *session);


static switch_memory_pool_t *opal_pool = NULL;
switch_endpoint_interface_t *opalfs_endpoint_interface = NULL;
static FSManager *opal_manager = NULL;


class _FSOpalProcess : public PProcess
{
   PCLASSINFO(_FSOpalProcess, PProcess)
   public:
       _FSOpalProcess(){PTrace::SetLevel(PSystemLog::Info);}; //just for fun and eyecandy ;)
       void Main() {};
} FSOpalProcess;



static switch_io_routines_t opalfs_io_routines = {
	/*.outgoing_channel */ opalfs_outgoing_channel,
	/*.read_frame */ opalfs_read_frame,
	/*.write_frame */ opalfs_write_frame,
	/*.kill_channel */ opalfs_kill_channel,
	/*.waitfor_read */ opalfs_waitfor_read,
	/*.waitfor_read */ opalfs_waitfor_write,
	/*.send_dtmf */ opalfs_send_dtmf,
	/*.receive_message */ opalfs_receive_message,
	/*.receive_event */ opalfs_receive_event,
	/*.state_change*/ opalfs_state_change,
	/*.read_video_frame*/ opalfs_read_video_frame,
	/*.write_video_frame*/ opalfs_write_video_frame
};

static switch_state_handler_table_t opalfs_event_handlers = {
	/*.on_init */ opalfs_on_init,
	/*.on_ring */ opalfs_on_ring,
	/*.on_execute */ opalfs_on_execute,
	/*.on_hangup */ opalfs_on_hangup,
	/*.on_loopback */ opalfs_on_loopback,
	/*.on_transmit */ opalfs_on_transmit
};


SWITCH_MODULE_LOAD_FUNCTION(mod_opal_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_opal_shutdown);
SWITCH_MODULE_DEFINITION(mod_opal, mod_opal_load, mod_opal_shutdown, NULL);

SWITCH_MODULE_LOAD_FUNCTION(mod_opal_load)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE,"Starting loading mod_opal\n");
    opal_pool = pool;

    *module_interface =NULL;
    *module_interface = switch_loadable_module_create_module_interface(pool, modname);
	
	if(!module_interface){
		return SWITCH_STATUS_MEMERR;
	}
	
	opalfs_endpoint_interface = (switch_endpoint_interface_t*)switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	opalfs_endpoint_interface->interface_name = "opalFS";
    opalfs_endpoint_interface->io_routines = &opalfs_io_routines;
    opalfs_endpoint_interface->state_handler = &opalfs_event_handlers;
	
	opal_manager = new FSManager();
	
	if(!opal_manager) {
		return SWITCH_STATUS_MEMERR;
	}
	
	if(!opal_manager->Initialize(pool)){
		delete opal_manager;
		return SWITCH_STATUS_FALSE;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Opal manager initilaized and running\n");
	
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_opal_shutdown)
{
    delete opal_manager;
    opal_manager = NULL;
    opal_pool = NULL;
    opalfs_endpoint_interface = NULL;
    return SWITCH_STATUS_SUCCESS;
}


static switch_call_cause_t opalfs_outgoing_channel(switch_core_session_t *session,
										switch_caller_profile_t *outbound_profile,
        								switch_core_session_t **new_session, 
        								switch_memory_pool_t **pool)
{
	fs_obj_t* tech_prv = (fs_obj_t*)switch_core_session_get_private(session);
	
	return SWITCH_CAUSE_SUCCESS;
	
}

static switch_status_t opalfs_read_frame(switch_core_session_t *session, switch_frame_t **frame, int timeout, switch_io_flag_t flags, int stream_id)
{
	fs_obj_t* tech_prv = (fs_obj_t*)switch_core_session_get_private(session);
	
    return tech_prv->Connection->io_read_frame(session, frame, timeout, flags, stream_id);
}

static switch_status_t opalfs_write_frame(switch_core_session_t *session, switch_frame_t *frame, int timeout, switch_io_flag_t flags, int stream_id)
{
	fs_obj_t* tech_prv = (fs_obj_t*)switch_core_session_get_private(session);
	
    return tech_prv->Connection->io_write_frame(session, frame, timeout, flags, stream_id);
}

static switch_status_t opalfs_kill_channel(switch_core_session_t *session, int sig)
{
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t opalfs_waitfor_read(switch_core_session_t *session, int ms, int stream_id)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Waitfor read!!!\n");
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t opalfs_waitfor_write(switch_core_session_t *session, int ms, int stream_id)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Waitfor write!!!\n");
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t opalfs_send_dtmf(switch_core_session_t *session, char *dtmf)
{
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t opalfs_receive_message(switch_core_session_t *session, switch_core_session_message_t *msg)
{
	fs_obj_t* tech_prv = (fs_obj_t*) switch_core_session_get_private(session);
	
	return tech_prv->Connection->io_receive_message(session, msg);
}

static switch_status_t opalfs_receive_event(switch_core_session_t *session, switch_event_t *event)
{
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t opalfs_state_change(switch_core_session_t *session)
{
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t opalfs_read_video_frame(switch_core_session_t *session, switch_frame_t **frame, int timeout, switch_io_flag_t flag, int stream_id)
{
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t opalfs_write_video_frame(switch_core_session_t *session, switch_frame_t *frame, int timeout, switch_io_flag_t flag, int stream_id)
{
    return SWITCH_STATUS_SUCCESS;
}


static switch_status_t opalfs_on_init(switch_core_session_t *session)
{
	fs_obj_t* fs_pvt = (fs_obj_t*)switch_core_session_get_private(session);
	return SWITCH_STATUS_SUCCESS;
	return fs_pvt->Connection->callback_on_init(session);
}

static switch_status_t opalfs_on_ring(switch_core_session_t *session)
{
	fs_obj_t* tech_prv = (fs_obj_t*)switch_core_session_get_private(session);
	
	return tech_prv->Connection->callback_on_ring(session);
}

static switch_status_t opalfs_on_execute(switch_core_session_t *session)
{
	fs_obj_t* tech_prv = (fs_obj_t*)switch_core_session_get_private(session);
	
    return tech_prv->Connection->callback_on_execute(session);
}

static switch_status_t opalfs_on_hangup(switch_core_session_t *session)
{
	fs_obj_t* tech_prv = (fs_obj_t*)switch_core_session_get_private(session);
	
	return tech_prv->Connection->callback_on_hangup(session);
}

static switch_status_t opalfs_on_loopback(switch_core_session_t *session)
{
	fs_obj_t* tech_prv = (fs_obj_t*)switch_core_session_get_private(session);
	
    return tech_prv->Connection->callback_on_loopback(session);;
}

static switch_status_t opalfs_on_transmit(switch_core_session_t *session)
{
    return SWITCH_STATUS_SUCCESS;
}






