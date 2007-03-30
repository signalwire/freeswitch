/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 * switch_core_event_hook.h Core Event Hooks
 *
 */
#ifndef SWITCH_EVENT_HOOKS_H
#define SWITCH_EVENT_HOOKS_H

#include <switch.h>
SWITCH_BEGIN_EXTERN_C typedef struct switch_io_event_hooks switch_io_event_hooks_t;

typedef struct switch_io_event_hook_outgoing_channel switch_io_event_hook_outgoing_channel_t;
typedef struct switch_io_event_hook_answer_channel switch_io_event_hook_answer_channel_t;
typedef struct switch_io_event_hook_receive_message switch_io_event_hook_receive_message_t;
typedef struct switch_io_event_hook_receive_event switch_io_event_hook_receive_event_t;
typedef struct switch_io_event_hook_read_frame switch_io_event_hook_read_frame_t;
typedef struct switch_io_event_hook_write_frame switch_io_event_hook_write_frame_t;
typedef struct switch_io_event_hook_kill_channel switch_io_event_hook_kill_channel_t;
typedef struct switch_io_event_hook_waitfor_read switch_io_event_hook_waitfor_read_t;
typedef struct switch_io_event_hook_waitfor_write switch_io_event_hook_waitfor_write_t;
typedef struct switch_io_event_hook_send_dtmf switch_io_event_hook_send_dtmf_t;
typedef struct switch_io_event_hook_state_change switch_io_event_hook_state_change_t;


typedef switch_status_t (*switch_outgoing_channel_hook_t) (switch_core_session_t *, switch_caller_profile_t *, switch_core_session_t *);
typedef switch_status_t (*switch_answer_channel_hook_t) (switch_core_session_t *);
typedef switch_status_t (*switch_receive_message_hook_t) (switch_core_session_t *, switch_core_session_message_t *);
typedef switch_status_t (*switch_receive_event_hook_t) (switch_core_session_t *, switch_event_t *);
typedef switch_status_t (*switch_read_frame_hook_t) (switch_core_session_t *, switch_frame_t **, int, switch_io_flag_t, int);
typedef switch_status_t (*switch_write_frame_hook_t) (switch_core_session_t *, switch_frame_t *, int, switch_io_flag_t, int);
typedef switch_status_t (*switch_kill_channel_hook_t) (switch_core_session_t *, int);
typedef switch_status_t (*switch_waitfor_read_hook_t) (switch_core_session_t *, int, int);
typedef switch_status_t (*switch_waitfor_write_hook_t) (switch_core_session_t *, int, int);
typedef switch_status_t (*switch_send_dtmf_hook_t) (switch_core_session_t *, char *);
typedef switch_status_t (*switch_state_change_hook_t) (switch_core_session_t *);


/*! \brief Node in which to store custom outgoing channel callback hooks */
struct switch_io_event_hook_outgoing_channel {
	/*! the outgoing channel callback hook */
	switch_outgoing_channel_hook_t outgoing_channel;
	struct switch_io_event_hook_outgoing_channel *next;
};

/*! \brief Node in which to store custom answer channel callback hooks */
struct switch_io_event_hook_answer_channel {
	/*! the answer channel callback hook */
	switch_answer_channel_hook_t answer_channel;
	struct switch_io_event_hook_answer_channel *next;
};

/*! \brief Node in which to store custom receive message callback hooks */
struct switch_io_event_hook_receive_message {
	/*! the answer channel callback hook */
	switch_receive_message_hook_t receive_message;
	struct switch_io_event_hook_receive_message *next;
};

/*! \brief Node in which to store custom receive message callback hooks */
struct switch_io_event_hook_receive_event {
	/*! the answer channel callback hook */
	switch_receive_event_hook_t receive_event;
	struct switch_io_event_hook_receive_event *next;
};

/*! \brief Node in which to store custom read frame channel callback hooks */
struct switch_io_event_hook_read_frame {
	/*! the read frame channel callback hook */
	switch_read_frame_hook_t read_frame;
	struct switch_io_event_hook_read_frame *next;
};

/*! \brief Node in which to store custom write_frame channel callback hooks */
struct switch_io_event_hook_write_frame {
	/*! the write_frame channel callback hook */
	switch_write_frame_hook_t write_frame;
	struct switch_io_event_hook_write_frame *next;
};

/*! \brief Node in which to store custom kill channel callback hooks */
struct switch_io_event_hook_kill_channel {
	/*! the kill channel callback hook */
	switch_kill_channel_hook_t kill_channel;
	struct switch_io_event_hook_kill_channel *next;
};

/*! \brief Node in which to store custom waitfor read channel callback hooks */
struct switch_io_event_hook_waitfor_read {
	/*! the waitfor read channel callback hook */
	switch_waitfor_read_hook_t waitfor_read;
	struct switch_io_event_hook_waitfor_read *next;
};

/*! \brief Node in which to store custom waitfor write channel callback hooks */
struct switch_io_event_hook_waitfor_write {
	/*! the waitfor write channel callback hook */
	switch_waitfor_write_hook_t waitfor_write;
	struct switch_io_event_hook_waitfor_write *next;
};

/*! \brief Node in which to store custom send dtmf channel callback hooks */
struct switch_io_event_hook_send_dtmf {
	/*! the send dtmf channel callback hook */
	switch_send_dtmf_hook_t send_dtmf;
	struct switch_io_event_hook_send_dtmf *next;
};

/*! \brief Node in which to store state change callback hooks */
struct switch_io_event_hook_state_change {
	/*! the send dtmf channel callback hook */
	switch_state_change_hook_t state_change;
	struct switch_io_event_hook_state_change *next;
};

/*! \brief A table of lists of io_event_hooks to store the event hooks associated with a session */
struct switch_io_event_hooks {
	/*! a list of outgoing channel hooks */
	switch_io_event_hook_outgoing_channel_t *outgoing_channel;
	/*! a list of answer channel hooks */
	switch_io_event_hook_answer_channel_t *answer_channel;
	/*! a list of receive message hooks */
	switch_io_event_hook_receive_message_t *receive_message;
	/*! a list of queue message hooks */
	switch_io_event_hook_receive_event_t *receive_event;
	/*! a list of read frame hooks */
	switch_io_event_hook_read_frame_t *read_frame;
	/*! a list of write frame hooks */
	switch_io_event_hook_write_frame_t *write_frame;
	/*! a list of kill channel hooks */
	switch_io_event_hook_kill_channel_t *kill_channel;
	/*! a list of wait for read hooks */
	switch_io_event_hook_waitfor_read_t *waitfor_read;
	/*! a list of wait for write hooks */
	switch_io_event_hook_waitfor_write_t *waitfor_write;
	/*! a list of send dtmf hooks */
	switch_io_event_hook_send_dtmf_t *send_dtmf;
	/*! a list of state change hooks */
	switch_io_event_hook_state_change_t *state_change;
};

extern switch_io_event_hooks_t switch_core_session_get_event_hooks(switch_core_session_t *session);



///\defgroup shooks Session Hook Callbacks
///\ingroup core1
///\{

/*! 
  \brief Add an event hook to be executed when a session requests an outgoing extension
  \param session session to bind hook to
  \param outgoing_channel hook to bind
  \return SWITCH_STATUS_SUCCESS on suceess
*/
SWITCH_DECLARE(switch_status_t) switch_core_event_hook_add_outgoing_channel(switch_core_session_t *session, switch_outgoing_channel_hook_t outgoing_channel);

/*! 
  \brief Add an event hook to be executed when a session answers a channel
  \param session session to bind hook to
  \param answer_channel hook to bind
  \return SWITCH_STATUS_SUCCESS on suceess
*/
SWITCH_DECLARE(switch_status_t) switch_core_event_hook_add_answer_channel(switch_core_session_t *session, switch_answer_channel_hook_t answer_channel);

/*! 
  \brief Add an event hook to be executed when a session sends a message
  \param session session to bind hook to
  \param receive_message hook to bind
  \return SWITCH_STATUS_SUCCESS on suceess
*/
SWITCH_DECLARE(switch_status_t) switch_core_event_hook_add_receive_message(switch_core_session_t *session, switch_receive_message_hook_t receive_message);

/*! 
  \brief Add an event hook to be executed when a session reads a frame
  \param session session to bind hook to
  \param  read_frame hook to bind
  \return SWITCH_STATUS_SUCCESS on suceess
*/
SWITCH_DECLARE(switch_status_t) switch_core_event_hook_add_read_frame(switch_core_session_t *session, switch_read_frame_hook_t read_frame);

/*! 
  \brief Add an event hook to be executed when a session writes a frame
  \param session session to bind hook to
  \param write_frame hook to bind
  \return SWITCH_STATUS_SUCCESS on suceess
*/
SWITCH_DECLARE(switch_status_t) switch_core_event_hook_add_write_frame(switch_core_session_t *session, switch_write_frame_hook_t write_frame);

/*! 
  \brief Add an event hook to be executed when a session kills a channel
  \param session session to bind hook to
  \param kill_channel hook to bind
  \return SWITCH_STATUS_SUCCESS on suceess
*/
SWITCH_DECLARE(switch_status_t) switch_core_event_hook_add_kill_channel(switch_core_session_t *session, switch_kill_channel_hook_t kill_channel);

/*! 
  \brief Add an event hook to be executed when a session waits for a read event
  \param session session to bind hook to
  \param waitfor_read hook to bind
  \return SWITCH_STATUS_SUCCESS on suceess
*/
SWITCH_DECLARE(switch_status_t) switch_core_event_hook_add_waitfor_read(switch_core_session_t *session, switch_waitfor_read_hook_t waitfor_read);

/*! 
  \brief Add an event hook to be executed when a session waits for a write event
  \param session session to bind hook to
  \param waitfor_write hook to bind
  \return SWITCH_STATUS_SUCCESS on suceess
*/
SWITCH_DECLARE(switch_status_t) switch_core_event_hook_add_waitfor_write(switch_core_session_t *session, switch_waitfor_write_hook_t waitfor_write);

/*! 
  \brief Add an event hook to be executed when a session sends dtmf
  \param session session to bind hook to
  \param send_dtmf hook to bind
  \return SWITCH_STATUS_SUCCESS on suceess
*/
SWITCH_DECLARE(switch_status_t) switch_core_event_hook_add_send_dtmf(switch_core_session_t *session, switch_send_dtmf_hook_t send_dtmf);

/*! 
  \brief Add an event hook to be executed when a session receives a state change signal
  \param session session to bind hook to
  \param state_change hook to bind
  \return SWITCH_STATUS_SUCCESS on suceess
*/
SWITCH_DECLARE(switch_status_t) switch_core_event_hook_add_state_change(switch_core_session_t *session, switch_answer_channel_hook_t state_change);
///\}

SWITCH_END_EXTERN_C
#endif
/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
