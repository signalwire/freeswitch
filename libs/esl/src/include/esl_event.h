/*
 * Copyright (c) 2007-2012, Anthony Minessale II
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ESL_EVENT_H
#define ESL_EVENT_H

#include <esl.h>

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

typedef enum {
	ESL_STACK_BOTTOM,
	ESL_STACK_TOP,
	ESL_STACK_PUSH,
	ESL_STACK_UNSHIFT
} esl_stack_t;

typedef enum {
	ESL_EVENT_CUSTOM,
	ESL_EVENT_CLONE,
	ESL_EVENT_CHANNEL_CREATE,
	ESL_EVENT_CHANNEL_DESTROY,
	ESL_EVENT_CHANNEL_STATE,
	ESL_EVENT_CHANNEL_CALLSTATE,
	ESL_EVENT_CHANNEL_ANSWER,
	ESL_EVENT_CHANNEL_HANGUP,
	ESL_EVENT_CHANNEL_HANGUP_COMPLETE,
	ESL_EVENT_CHANNEL_EXECUTE,
	ESL_EVENT_CHANNEL_EXECUTE_COMPLETE,
	ESL_EVENT_CHANNEL_HOLD,
	ESL_EVENT_CHANNEL_UNHOLD,
	ESL_EVENT_CHANNEL_BRIDGE,
	ESL_EVENT_CHANNEL_UNBRIDGE,
	ESL_EVENT_CHANNEL_PROGRESS,
	ESL_EVENT_CHANNEL_PROGRESS_MEDIA,
	ESL_EVENT_CHANNEL_OUTGOING,
	ESL_EVENT_CHANNEL_PARK,
	ESL_EVENT_CHANNEL_UNPARK,
	ESL_EVENT_CHANNEL_APPLICATION,
	ESL_EVENT_CHANNEL_ORIGINATE,
	ESL_EVENT_CHANNEL_UUID,
	ESL_EVENT_API,
	ESL_EVENT_LOG,
	ESL_EVENT_INBOUND_CHAN,
	ESL_EVENT_OUTBOUND_CHAN,
	ESL_EVENT_STARTUP,
	ESL_EVENT_SHUTDOWN,
	ESL_EVENT_PUBLISH,
	ESL_EVENT_UNPUBLISH,
	ESL_EVENT_TALK,
	ESL_EVENT_NOTALK,
	ESL_EVENT_SESSION_CRASH,
	ESL_EVENT_MODULE_LOAD,
	ESL_EVENT_MODULE_UNLOAD,
	ESL_EVENT_DTMF,
	ESL_EVENT_MESSAGE,
	ESL_EVENT_PRESENCE_IN,
	ESL_EVENT_NOTIFY_IN,
	ESL_EVENT_PRESENCE_OUT,
	ESL_EVENT_PRESENCE_PROBE,
	ESL_EVENT_MESSAGE_WAITING,
	ESL_EVENT_MESSAGE_QUERY,
	ESL_EVENT_ROSTER,
	ESL_EVENT_CODEC,
	ESL_EVENT_BACKGROUND_JOB,
	ESL_EVENT_DETECTED_SPEECH,
	ESL_EVENT_DETECTED_TONE,
	ESL_EVENT_PRIVATE_COMMAND,
	ESL_EVENT_HEARTBEAT,
	ESL_EVENT_TRAP,
	ESL_EVENT_ADD_SCHEDULE,
	ESL_EVENT_DEL_SCHEDULE,
	ESL_EVENT_EXE_SCHEDULE,
	ESL_EVENT_RE_SCHEDULE,
	ESL_EVENT_RELOADXML,
	ESL_EVENT_NOTIFY,
	ESL_EVENT_PHONE_FEATURE,
	ESL_EVENT_PHONE_FEATURE_SUBSCRIBE,
	ESL_EVENT_SEND_MESSAGE,
	ESL_EVENT_RECV_MESSAGE,
	ESL_EVENT_REQUEST_PARAMS,
	ESL_EVENT_CHANNEL_DATA,
	ESL_EVENT_GENERAL,
	ESL_EVENT_COMMAND,
	ESL_EVENT_SESSION_HEARTBEAT,
	ESL_EVENT_CLIENT_DISCONNECTED,
	ESL_EVENT_SERVER_DISCONNECTED,
	ESL_EVENT_SEND_INFO,
	ESL_EVENT_RECV_INFO,
	ESL_EVENT_RECV_RTCP_MESSAGE,
	ESL_EVENT_CALL_SECURE,
	ESL_EVENT_NAT,
	ESL_EVENT_RECORD_START,
	ESL_EVENT_RECORD_STOP,
	ESL_EVENT_PLAYBACK_START,
	ESL_EVENT_PLAYBACK_STOP,
	ESL_EVENT_CALL_UPDATE,
	ESL_EVENT_FAILURE,
	ESL_EVENT_SOCKET_DATA,
	ESL_EVENT_MEDIA_BUG_START,
	ESL_EVENT_MEDIA_BUG_STOP,
	ESL_EVENT_CONFERENCE_DATA_QUERY,
	ESL_EVENT_CONFERENCE_DATA,
	ESL_EVENT_CALL_SETUP_REQ,
	ESL_EVENT_CALL_SETUP_RESULT,
	ESL_EVENT_CALL_DETAIL,
	ESL_EVENT_DEVICE_STATE,
	ESL_EVENT_ALL
} esl_event_types_t;

typedef enum {
	ESL_PRIORITY_NORMAL,
	ESL_PRIORITY_LOW,
	ESL_PRIORITY_HIGH
} esl_priority_t;

/*! \brief An event Header */
	struct esl_event_header {
	/*! the header name */
	char *name;
	/*! the header value */
	char *value;
	/*! array space */
	char **array;
	/*! array index */
	int idx;
	/*! hash of the header name */
	unsigned long hash;
	struct esl_event_header *next;
};


/*! \brief Representation of an event */
struct esl_event {
	/*! the event id (descriptor) */
	esl_event_types_t event_id;
	/*! the priority of the event */
	esl_priority_t priority;
	/*! the owner of the event */
	char *owner;
	/*! the subclass of the event */
	char *subclass_name;
	/*! the event headers */
	esl_event_header_t *headers;
	/*! the event headers tail pointer */
	esl_event_header_t *last_header;
	/*! the body of the event */
	char *body;
	/*! user data from the subclass provider */
	void *bind_user_data;
	/*! user data from the event sender */
	void *event_user_data;
	/*! unique key */
	unsigned long key;
	struct esl_event *next;
	int flags;
};

typedef enum {
	ESL_EF_UNIQ_HEADERS = (1 << 0)
} esl_event_flag_t;


#define ESL_EVENT_SUBCLASS_ANY NULL

/*!
  \brief Create an event
  \param event a NULL pointer on which to create the event
  \param event_id the event id enumeration of the desired event
  \param subclass_name the subclass name for custom event (only valid when event_id is ESL_EVENT_CUSTOM)
  \return ESL_SUCCESS on success
*/
ESL_DECLARE(esl_status_t) esl_event_create_subclass(esl_event_t **event, esl_event_types_t event_id, const char *subclass_name);

/*!
  \brief Set the priority of an event
  \param event the event to set the priority on
  \param priority the event priority
  \return ESL_SUCCESS
*/
ESL_DECLARE(esl_status_t) esl_event_set_priority(esl_event_t *event, esl_priority_t priority);

/*!
  \brief Retrieve a header value from an event
  \param event the event to read the header from
  \param header_name the name of the header to read
  \return the value of the requested header
*/


ESL_DECLARE(esl_event_header_t *) esl_event_get_header_ptr(esl_event_t *event, const char *header_name);
ESL_DECLARE(char *) esl_event_get_header_idx(esl_event_t *event, const char *header_name, int idx);
#define esl_event_get_header(_e, _h) esl_event_get_header_idx(_e, _h, -1)

/*!
  \brief Retrieve the body value from an event
  \param event the event to read the body from
  \return the value of the body or NULL
*/
ESL_DECLARE(char *)esl_event_get_body(esl_event_t *event);

/*!
  \brief Add a header to an event
  \param event the event to add the header to
  \param stack the stack sense (stack it on the top or on the bottom)
  \param header_name the name of the header to add
  \param fmt the value of the header (varargs see standard sprintf family)
  \return ESL_SUCCESS if the header was added
*/
ESL_DECLARE(esl_status_t) esl_event_add_header(esl_event_t *event, esl_stack_t stack,
											   const char *header_name, const char *fmt, ...); //PRINTF_FUNCTION(4, 5);

ESL_DECLARE(int) esl_event_add_array(esl_event_t *event, const char *var, const char *val);

/*!
  \brief Add a string header to an event
  \param event the event to add the header to
  \param stack the stack sense (stack it on the top or on the bottom)
  \param header_name the name of the header to add
  \param data the value of the header
  \return ESL_SUCCESS if the header was added
*/
ESL_DECLARE(esl_status_t) esl_event_add_header_string(esl_event_t *event, esl_stack_t stack, const char *header_name, const char *data);

ESL_DECLARE(esl_status_t) esl_event_del_header_val(esl_event_t *event, const char *header_name, const char *var);
#define esl_event_del_header(_e, _h) esl_event_del_header_val(_e, _h, NULL)

/*!
  \brief Destroy an event
  \param event pointer to the pointer to event to destroy
*/
ESL_DECLARE(void) esl_event_destroy(esl_event_t **event);
#define esl_event_safe_destroy(_event) if (_event) esl_event_destroy(_event)

/*!
  \brief Duplicate an event
  \param event a NULL pointer on which to duplicate the event
  \param todup an event to duplicate
  \return ESL_SUCCESS if the event was duplicated
*/
ESL_DECLARE(esl_status_t) esl_event_dup(esl_event_t **event, esl_event_t *todup);
ESL_DECLARE(void) esl_event_merge(esl_event_t *event, esl_event_t *tomerge);

/*!
  \brief Render the name of an event id enumeration
  \param event the event id to render the name of
  \return the rendered name
*/
ESL_DECLARE(const char *)esl_event_name(esl_event_types_t event);

/*!
  \brief return the event id that matches a given event name
  \param name the name of the event
  \param type the event id to return
  \return ESL_SUCCESS if there was a match
*/
ESL_DECLARE(esl_status_t) esl_name_event(const char *name, esl_event_types_t *type);

/*!
  \brief Render a string representation of an event sutable for printing or network transport 
  \param event the event to render
  \param str a string pointer to point at the allocated data
  \param encode url encode the headers
  \return ESL_SUCCESS if the operation was successful
  \note you must free the resulting string when you are finished with it
*/
ESL_DECLARE(esl_status_t) esl_event_serialize(esl_event_t *event, char **str, esl_bool_t encode);
ESL_DECLARE(esl_status_t) esl_event_serialize_json(esl_event_t *event, char **str);
ESL_DECLARE(esl_status_t) esl_event_create_json(esl_event_t **event, const char *json);
/*!
  \brief Add a body to an event
  \param event the event to add to body to
  \param fmt optional body of the event (varargs see standard sprintf family)
  \return ESL_SUCCESS if the body was added to the event
  \note the body parameter can be shadowed by the esl_event_reserve_subclass_detailed function
*/
ESL_DECLARE(esl_status_t) esl_event_add_body(esl_event_t *event, const char *fmt, ...);
ESL_DECLARE(esl_status_t) esl_event_set_body(esl_event_t *event, const char *body);

/*!
  \brief Create a new event assuming it will not be custom event and therefore hiding the unused parameters
  \param event a NULL pointer on which to create the event
  \param id the event id enumeration of the desired event
  \return ESL_SUCCESS on success
*/
#define esl_event_create(event, id) esl_event_create_subclass(event, id, ESL_EVENT_SUBCLASS_ANY)

ESL_DECLARE(const char *)esl_priority_name(esl_priority_t priority);

///\}

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */

#endif /* defined(ESL_EVENT_H) */

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
