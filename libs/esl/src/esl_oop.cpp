#include <esl.h>
#include <esl_oop.h>

#define connection_construct_common() memset(&handle, 0, sizeof(handle)); last_event_obj = NULL
#define event_construct_common() event = NULL; serialized_string = NULL; mine = 0

void eslSetLogLevel(int level)
{
	esl_global_set_default_logger(level);
}

ESLconnection::ESLconnection(const char *host, const char *port, const char *password)
{
	connection_construct_common();
	int x_port = atoi(port);

	esl_connect(&handle, host, x_port, password);
}


ESLconnection::ESLconnection(int socket)
{
	connection_construct_common();
	memset(&handle, 0, sizeof(handle));
	esl_attach_handle(&handle, (esl_socket_t)socket, NULL);
}

ESLconnection::~ESLconnection()
{
	if (handle.connected) {
		esl_disconnect(&handle);
	}

}

int ESLconnection::connected()
{
	return handle.connected;
}

esl_status_t ESLconnection::send(const char *cmd)
{
	return esl_send(&handle, cmd);
}

ESLevent *ESLconnection::sendRecv(const char *cmd)
{
	if (esl_send_recv(&handle, cmd) == ESL_SUCCESS) {
		esl_event_t *event;
		esl_event_dup(&event, handle.last_sr_event);
		return new ESLevent(event, 1);
	}
	
	return NULL;
}

ESLevent *ESLconnection::getInfo()
{
	if (handle.connected && handle.info_event) {
		esl_event_t *event;
		esl_event_dup(&event, handle.info_event);
		return new ESLevent(event, 1);
	}
	
	return NULL;
}

int ESLconnection::setBlockingExecute(const char *val)
{
	if (val) {
		handle.blocking_execute = esl_true(val);
	}
	return handle.blocking_execute;
}

int ESLconnection::setEventLock(const char *val)
{
	if (val) {
		handle.event_lock = esl_true(val);
	}
	return handle.event_lock;
}

esl_status_t ESLconnection::execute(const char *app, const char *arg, const char *uuid)
{
	return esl_execute(&handle, app, arg, uuid);
}

esl_status_t ESLconnection::sendEvent(ESLevent *send_me)
{
	return esl_sendevent(&handle, send_me->event);
}

ESLevent *ESLconnection::recvEvent()
{
	if (last_event_obj) {
		delete last_event_obj;
	}
	
	if (esl_recv_event(&handle, NULL) == ESL_SUCCESS) {
		if (handle.last_ievent) {
			esl_event_t *event;
			esl_event_dup(&event, handle.last_ievent);
			last_event_obj = new ESLevent(event, 1);
			return last_event_obj;
		}
	}

	return NULL;
}

ESLevent *ESLconnection::recvEventTimed(int ms)
{
	if (last_event_obj) {
		delete last_event_obj;
		last_event_obj = NULL;
	}

	if (esl_recv_event_timed(&handle, ms, NULL) == ESL_SUCCESS) {
		if (handle.last_ievent) {
			esl_event_t *event;
			esl_event_dup(&event, handle.last_ievent);
			last_event_obj = new ESLevent(event, 1);
			return last_event_obj;
		}
    }
	
	return NULL;
}

esl_status_t ESLconnection::filter(const char *header, const char *value)
{
	return esl_filter(&handle, header, value);
}

esl_status_t ESLconnection::events(const char *etype, const char *value)
{
	esl_event_type_t type_id = ESL_EVENT_TYPE_PLAIN;

	if (!strcmp(etype, "xml")) {
		type_id = ESL_EVENT_TYPE_XML;
	}

	return esl_events(&handle, type_id, value);
}

// ESLevent
///////////////////////////////////////////////////////////////////////

ESLevent::ESLevent(const char *type, const char *subclass_name)
{
	esl_event_types_t event_id;
	
	event_construct_common();

	if (esl_name_event(type, &event_id) != ESL_SUCCESS) {
		event_id = ESL_EVENT_MESSAGE;
	}

	if (!esl_strlen_zero(subclass_name) && event_id != ESL_EVENT_CUSTOM) {
		esl_log(ESL_LOG_WARNING, "Changing event type to custom because you specified a subclass name!\n");
		event_id = ESL_EVENT_CUSTOM;
	}

	if (esl_event_create_subclass(&event, event_id, subclass_name) != ESL_SUCCESS) {
		esl_log(ESL_LOG_ERROR, "Failed to create event!\n");
		event = NULL;
	}

	serialized_string = NULL;
	mine = 1;
}

ESLevent::ESLevent(esl_event_t *wrap_me, int free_me)
{
	event_construct_common();
	event = wrap_me;
	mine = free_me;
	serialized_string = NULL;
}


ESLevent::ESLevent(ESLevent *me)
{
	/* workaround for silly php thing */
	event = me->event;
	mine = me->mine;
	serialized_string = NULL;
	me->event = NULL;
	me->mine = 0;
	esl_safe_free(me->serialized_string);
	delete me;
}

ESLevent::~ESLevent()
{
	
	if (serialized_string) {
		free(serialized_string);
	}

	if (event && mine) {
		esl_event_destroy(&event);
	}
}


const char *ESLevent::serialize(const char *format)
{
	int isxml = 0;

	this_check("");

	esl_safe_free(serialized_string);
	
	if (!event) {
		return "";
	}

	if (esl_event_serialize(event, &serialized_string, ESL_TRUE) == ESL_SUCCESS) {
		return serialized_string;
	}

	return "";

}

bool ESLevent::setPriority(esl_priority_t priority)
{
	this_check(false);

	if (event) {
        esl_event_set_priority(event, priority);
		return true;
	} else {
		esl_log(ESL_LOG_ERROR, "Trying to setPriority an event that does not exist!\n");
    }
	return false;
}

const char *ESLevent::getHeader(char *header_name)
{
	this_check("");

	if (event) {
		return esl_event_get_header(event, header_name);
	} else {
		esl_log(ESL_LOG_ERROR, "Trying to getHeader an event that does not exist!\n");
	}
	return NULL;
}

bool ESLevent::addHeader(const char *header_name, const char *value)
{
	this_check(false);

	if (event) {
		return esl_event_add_header_string(event, ESL_STACK_BOTTOM, header_name, value) == ESL_SUCCESS ? true : false;
	} else {
		esl_log(ESL_LOG_ERROR, "Trying to addHeader an event that does not exist!\n");
	}

	return false;
}

bool ESLevent::delHeader(const char *header_name)
{
	this_check(false);

	if (event) {
		return esl_event_del_header(event, header_name) == ESL_SUCCESS ? true : false;
	} else {
		esl_log(ESL_LOG_ERROR, "Trying to delHeader an event that does not exist!\n");
	}

	return false;
}


bool ESLevent::addBody(const char *value)
{
	this_check(false);

	if (event) {
		return esl_event_add_body(event, "%s", value) == ESL_SUCCESS ? true : false;
	} else {
		esl_log(ESL_LOG_ERROR, "Trying to addBody an event that does not exist!\n");
	}
	
	return false;
}

char *ESLevent::getBody(void)
{
	
	this_check((char *)"");

	if (event) {
		return esl_event_get_body(event);
	} else {
		esl_log(ESL_LOG_ERROR, "Trying to getBody an event that does not exist!\n");
	}
	
	return NULL;
}

const char *ESLevent::getType(void)
{
	this_check("");

	if (event) {
		return esl_event_name(event->event_id);
	} else {
		esl_log(ESL_LOG_ERROR, "Trying to getType an event that does not exist!\n");
	}
	
	return (char *) "invalid";
}
