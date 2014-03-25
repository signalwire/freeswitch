#include <esl.h>
#include <esl_oop.h>

#define connection_construct_common() memset(&handle, 0, sizeof(handle))
#define event_construct_common() event = NULL; serialized_string = NULL; mine = 0; hp = NULL

void eslSetLogLevel(int level)
{
	esl_global_set_default_logger(level);
}

ESLconnection::ESLconnection(const char *host, const int port, const char *password)
{
	connection_construct_common();

	esl_connect(&handle, host, port, NULL, password);
}

ESLconnection::ESLconnection(const char *host, const int port, const char *user, const char *password)
{
	connection_construct_common();

	esl_connect(&handle, host, port, user, password);
}

ESLconnection::ESLconnection(const char *host, const char *port, const char *password)
{
	connection_construct_common();
	if (port == NULL) return;
	int x_port = atoi(port);

	esl_connect(&handle, host, x_port, NULL, password);
}

ESLconnection::ESLconnection(const char *host, const char *port, const char *user, const char *password)
{
	connection_construct_common();
	if (port == NULL) return;
	int x_port = atoi(port);

	esl_connect(&handle, host, x_port, user, password);
}


ESLconnection::ESLconnection(int socket)
{
	connection_construct_common();
	esl_attach_handle(&handle, (esl_socket_t)socket, NULL);
}

ESLconnection::~ESLconnection()
{
	if (!handle.destroyed) {
		esl_disconnect(&handle);
	}
}

int ESLconnection::socketDescriptor()
{
	if (handle.connected) {
        return (int) handle.sock;
    }

	return -1;
}


int ESLconnection::disconnect()
{
	if (!handle.destroyed) {
        return esl_disconnect(&handle);
    }

	return 0;
}

int ESLconnection::connected()
{
	return handle.connected;
}

int ESLconnection::send(const char *cmd)
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

ESLevent *ESLconnection::api(const char *cmd, const char *arg)
{
	size_t len;
	char *cmd_buf;
	ESLevent *event;
	
	if (!cmd) {
		return NULL;
	}

	len = strlen(cmd) + (arg ? strlen(arg) : 0) + 10;

	cmd_buf = (char *) malloc(len + 1);
	assert(cmd_buf);

	snprintf(cmd_buf, len, "api %s %s", cmd, arg ? arg : "");
	*(cmd_buf + (len)) = '\0';


	event = sendRecv(cmd_buf);
	free(cmd_buf);

	return event;
}

ESLevent *ESLconnection::bgapi(const char *cmd, const char *arg, const char *job_uuid)
{
	size_t len;
	char *cmd_buf;
	ESLevent *event;
	
	if (!cmd) {
		return NULL;
	}

	len = strlen(cmd) + (arg ? strlen(arg) : 0) + (job_uuid ? strlen(job_uuid) + 12 : 0) + 10;

	cmd_buf = (char *) malloc(len + 1);
	assert(cmd_buf);
	
	if (job_uuid) {
		snprintf(cmd_buf, len, "bgapi %s%s%s\nJob-UUID: %s", cmd, arg ? " " : "", arg ? arg : "", job_uuid);
	} else {
		snprintf(cmd_buf, len, "bgapi %s%s%s", cmd, arg ? " " : "", arg ? arg : "");
	}

	*(cmd_buf + (len)) = '\0';

	event = sendRecv(cmd_buf);
	free(cmd_buf);
	
	return event;
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

int ESLconnection::setAsyncExecute(const char *val)
{
	if (val) {
		handle.async_execute = esl_true(val);
	}
	return handle.async_execute;
}

int ESLconnection::setEventLock(const char *val)
{
	if (val) {
		handle.event_lock = esl_true(val);
	}
	return handle.event_lock;
}

ESLevent *ESLconnection::execute(const char *app, const char *arg, const char *uuid)
{
	if (esl_execute(&handle, app, arg, uuid) == ESL_SUCCESS) {
		esl_event_t *event;
		esl_event_dup(&event, handle.last_sr_event);
		return new ESLevent(event, 1);
	}

	return NULL;
}


ESLevent *ESLconnection::executeAsync(const char *app, const char *arg, const char *uuid)
{
	int async = handle.async_execute;
	int r;

	handle.async_execute = 1;
	r = esl_execute(&handle, app, arg, uuid);
	handle.async_execute = async;

	if (r == ESL_SUCCESS) {
		esl_event_t *event;
		esl_event_dup(&event, handle.last_sr_event);
		return new ESLevent(event, 1);
	}

	return NULL;
}

ESLevent *ESLconnection::sendEvent(ESLevent *send_me)
{
	if (esl_sendevent(&handle, send_me->event) == ESL_SUCCESS) {
		esl_event_t *e = handle.last_ievent ? handle.last_ievent : handle.last_event;
		if (e) {
			esl_event_t *event;
			esl_event_dup(&event, e);
			return new ESLevent(event, 1);
		}
	}

	return new ESLevent("server_disconnected");
}

int ESLconnection::sendMSG(ESLevent *send_me, const char *uuid)
{
	if (esl_sendmsg(&handle, send_me->event, uuid) == ESL_SUCCESS) {
		return 0;
	}

	return 1;
}

ESLevent *ESLconnection::recvEvent()
{
	if (esl_recv_event(&handle, 1, NULL) == ESL_SUCCESS) {
		esl_event_t *e = handle.last_ievent ? handle.last_ievent : handle.last_event;
		if (e) {
			esl_event_t *event;
			esl_event_dup(&event, e);
			return new ESLevent(event, 1);
		}
	}

	return new ESLevent("server_disconnected");
}

ESLevent *ESLconnection::recvEventTimed(int ms)
{

	if (esl_recv_event_timed(&handle, ms, 1, NULL) == ESL_SUCCESS) {
		esl_event_t *e = handle.last_ievent ? handle.last_ievent : handle.last_event;
		if (e) {
			esl_event_t *event;
			esl_event_dup(&event, e);
			return new ESLevent(event, 1);
		}
    }
	
	return NULL;
}

ESLevent *ESLconnection::filter(const char *header, const char *value)
{
	esl_status_t status = esl_filter(&handle, header, value);

	if (status == ESL_SUCCESS && handle.last_sr_event) {
		esl_event_t *event;
		esl_event_dup(&event, handle.last_sr_event);
		return new ESLevent(event, 1);
	}

	return NULL;

}

int ESLconnection::events(const char *etype, const char *value)
{
	esl_event_type_t type_id = ESL_EVENT_TYPE_PLAIN;

	if (!strcmp(etype, "xml")) {
		type_id = ESL_EVENT_TYPE_XML;
	} else if (!strcmp(etype, "json")) {
        type_id = ESL_EVENT_TYPE_JSON;
	}

	return esl_events(&handle, type_id, value);
}

// ESLevent
///////////////////////////////////////////////////////////////////////

ESLevent::ESLevent(const char *type, const char *subclass_name)
{
	esl_event_types_t event_id;
	
	event_construct_common();

	if (!strcasecmp(type, "json") && !esl_strlen_zero(subclass_name)) {
		if (esl_event_create_json(&event, subclass_name) != ESL_SUCCESS) {
			return;
			
		}
		event_id = event->event_id;
	} else {

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

const char *ESLevent::nextHeader(void)
{
	const char *name = NULL;

	if (hp) {
		name = hp->name;
		hp = hp->next;
	}

	return name;
}

const char *ESLevent::firstHeader(void)
{
	if (event) {
		hp = event->headers;
	}

	return nextHeader();
}

const char *ESLevent::serialize(const char *format)
{
	this_check("");

	esl_safe_free(serialized_string);
	
	if (format == NULL) {
		format = "text";
	}
	
	if (!event) {
		return "";
	}

	if (format && !strcasecmp(format, "json")) {
		esl_event_serialize_json(event, &serialized_string);
		return serialized_string;
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

const char *ESLevent::getHeader(const char *header_name, int idx)
{
	this_check("");

	if (event) {
		return esl_event_get_header_idx(event, header_name, idx);
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

bool ESLevent::pushHeader(const char *header_name, const char *value)
{
	this_check(false);

	if (event) {
		return esl_event_add_header_string(event, ESL_STACK_PUSH, header_name, value) == ESL_SUCCESS ? true : false;
	} else {
		esl_log(ESL_LOG_ERROR, "Trying to addHeader an event that does not exist!\n");
	}

	return false;
}

bool ESLevent::unshiftHeader(const char *header_name, const char *value)
{
	this_check(false);

	if (event) {
		return esl_event_add_header_string(event, ESL_STACK_UNSHIFT, header_name, value) == ESL_SUCCESS ? true : false;
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

/* For Emacs:
 * Local Variables:
 * mode:c++
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
