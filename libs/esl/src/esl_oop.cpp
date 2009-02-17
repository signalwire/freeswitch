#include <esl.h>
#include <esl_oop.h>

#define construct_common() memset(&handle, 0, sizeof(handle)); last_event_obj = NULL; last_event = NULL;

eslConnection::eslConnection(const char *host, const char *port, const char *password)
{
	construct_common();
	int x_port = atoi(port);

	esl_connect(&handle, host, x_port, password);
}


eslConnection::eslConnection(int socket)
{
	construct_common();
	memset(&handle, 0, sizeof(handle));
	esl_attach_handle(&handle, (esl_socket_t)socket, NULL);
}

eslConnection::~eslConnection()
{
	if (handle.connected) {
		esl_disconnect(&handle);
	}

	esl_event_safe_destroy(&last_event);
}

int eslConnection::connected()
{
	return handle.connected;
}

esl_status_t eslConnection::send(const char *cmd)
{
	return esl_send(&handle, cmd);
}

eslEvent *eslConnection::sendRecv(const char *cmd)
{
	if (esl_send_recv(&handle, cmd) == ESL_SUCCESS) {
		esl_event_t *event;
		esl_event_dup(&event, handle.last_sr_event);
		return new eslEvent(event, 1);
	}
	
	return NULL;
}

eslEvent *eslConnection::getInfo()
{
	if (handle.connected && handle.info_event) {
		esl_event_t *event;
		esl_event_dup(&event, handle.info_event);
		return new eslEvent(event, 1);
	}
	
	return NULL;
}

esl_status_t eslConnection::execute(const char *app, const char *arg, const char *uuid)
{
	return esl_execute(&handle, app, arg, uuid);
}

esl_status_t eslConnection::sendEvent(eslEvent *send_me)
{
	return esl_sendevent(&handle, send_me->event);
}

eslEvent *eslConnection::recvEvent()
{
	if (last_event_obj) {
		delete last_event_obj;
	}
	
	if (esl_recv_event(&handle, &last_event) == ESL_SUCCESS) {
		esl_event_t *event;
		esl_event_dup(&event, last_event);
		last_event_obj = new eslEvent(event, 1);
		return last_event_obj;
	}

	return NULL;
}

eslEvent *eslConnection::recvEventTimed(int ms)
{
	if (last_event_obj) {
		delete last_event_obj;
		last_event_obj = NULL;
	}

	if (esl_recv_event_timed(&handle, ms, &last_event) == ESL_SUCCESS) {
		esl_event_t *event;
		esl_event_dup(&event, last_event);
        last_event_obj = new eslEvent(event, 1);
        return last_event_obj;
    }
	
	return NULL;
}

esl_status_t eslConnection::filter(const char *header, const char *value)
{
	return esl_filter(&handle, header, value);
}

esl_status_t eslConnection::events(const char *etype, const char *value)
{
	esl_event_type_t type_id = ESL_EVENT_TYPE_PLAIN;

	if (!strcmp(etype, "xml")) {
		type_id = ESL_EVENT_TYPE_XML;
	}

	return esl_events(&handle, type_id, value);
}

// eslEvent
///////////////////////////////////////////////////////////////////////

eslEvent::eslEvent(const char *type, const char *subclass_name)
{
	esl_event_types_t event_id;
	
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

eslEvent::eslEvent(esl_event_t *wrap_me, int free_me)
{
	event = wrap_me;
	mine = free_me;
	serialized_string = NULL;
}

eslEvent::~eslEvent()
{

	if (serialized_string) {
		free(serialized_string);
	}

	if (event && mine) {
		esl_event_destroy(&event);
	}
}


const char *eslEvent::serialize(const char *format)
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

bool eslEvent::setPriority(esl_priority_t priority)
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

const char *eslEvent::getHeader(char *header_name)
{
	this_check("");

	if (event) {
		return esl_event_get_header(event, header_name);
	} else {
		esl_log(ESL_LOG_ERROR, "Trying to getHeader an event that does not exist!\n");
	}
	return NULL;
}

bool eslEvent::addHeader(const char *header_name, const char *value)
{
	this_check(false);

	if (event) {
		return esl_event_add_header_string(event, ESL_STACK_BOTTOM, header_name, value) == ESL_SUCCESS ? true : false;
	} else {
		esl_log(ESL_LOG_ERROR, "Trying to addHeader an event that does not exist!\n");
	}

	return false;
}

bool eslEvent::delHeader(const char *header_name)
{
	this_check(false);

	if (event) {
		return esl_event_del_header(event, header_name) == ESL_SUCCESS ? true : false;
	} else {
		esl_log(ESL_LOG_ERROR, "Trying to delHeader an event that does not exist!\n");
	}

	return false;
}


bool eslEvent::addBody(const char *value)
{
	this_check(false);

	if (event) {
		return esl_event_add_body(event, "%s", value) == ESL_SUCCESS ? true : false;
	} else {
		esl_log(ESL_LOG_ERROR, "Trying to addBody an event that does not exist!\n");
	}
	
	return false;
}

char *eslEvent::getBody(void)
{
	
	this_check((char *)"");

	if (event) {
		return esl_event_get_body(event);
	} else {
		esl_log(ESL_LOG_ERROR, "Trying to getBody an event that does not exist!\n");
	}
	
	return NULL;
}

const char *eslEvent::getType(void)
{
	this_check("");

	if (event) {
		return esl_event_name(event->event_id);
	} else {
		esl_log(ESL_LOG_ERROR, "Trying to getType an event that does not exist!\n");
	}
	
	return (char *) "invalid";
}
