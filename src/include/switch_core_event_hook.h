/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 * switch_core_event_hook.h Core Event Hooks
 *
 */
#ifndef SWITCH_EVENT_HOOKS_H
#define SWITCH_EVENT_HOOKS_H

#include <switch.h>
SWITCH_BEGIN_EXTERN_C typedef struct switch_io_event_hooks switch_io_event_hooks_t;

typedef struct switch_io_event_hook_outgoing_channel switch_io_event_hook_outgoing_channel_t;
typedef struct switch_io_event_hook_receive_message switch_io_event_hook_receive_message_t;
typedef struct switch_io_event_hook_receive_event switch_io_event_hook_receive_event_t;
typedef struct switch_io_event_hook_read_frame switch_io_event_hook_read_frame_t;
typedef struct switch_io_event_hook_video_read_frame switch_io_event_hook_video_read_frame_t;
typedef struct switch_io_event_hook_write_frame switch_io_event_hook_write_frame_t;
typedef struct switch_io_event_hook_video_write_frame switch_io_event_hook_video_write_frame_t;
typedef struct switch_io_event_hook_kill_channel switch_io_event_hook_kill_channel_t;
typedef struct switch_io_event_hook_send_dtmf switch_io_event_hook_send_dtmf_t;
typedef struct switch_io_event_hook_recv_dtmf switch_io_event_hook_recv_dtmf_t;
typedef struct switch_io_event_hook_state_change switch_io_event_hook_state_change_t;
typedef struct switch_io_event_hook_resurrect_session switch_io_event_hook_resurrect_session_t;
typedef switch_status_t (*switch_outgoing_channel_hook_t)
                (switch_core_session_t *, switch_event_t *, switch_caller_profile_t *, switch_core_session_t *, switch_originate_flag_t);
typedef switch_status_t (*switch_receive_message_hook_t) (switch_core_session_t *, switch_core_session_message_t *);
typedef switch_status_t (*switch_receive_event_hook_t) (switch_core_session_t *, switch_event_t *);
typedef switch_status_t (*switch_read_frame_hook_t) (switch_core_session_t *, switch_frame_t **, switch_io_flag_t, int);
typedef switch_status_t (*switch_video_read_frame_hook_t) (switch_core_session_t *, switch_frame_t **, switch_io_flag_t, int);
typedef switch_status_t (*switch_write_frame_hook_t) (switch_core_session_t *, switch_frame_t *, switch_io_flag_t, int);
typedef switch_status_t (*switch_video_write_frame_hook_t) (switch_core_session_t *, switch_frame_t *, switch_io_flag_t, int);
typedef switch_status_t (*switch_kill_channel_hook_t) (switch_core_session_t *, int);
typedef switch_status_t (*switch_send_dtmf_hook_t) (switch_core_session_t *, const switch_dtmf_t *, switch_dtmf_direction_t direction);
typedef switch_status_t (*switch_recv_dtmf_hook_t) (switch_core_session_t *, const switch_dtmf_t *, switch_dtmf_direction_t direction);
typedef switch_status_t (*switch_state_change_hook_t) (switch_core_session_t *);
typedef switch_call_cause_t (*switch_resurrect_session_hook_t) (switch_core_session_t **, switch_memory_pool_t **, void *);

/*! \brief Node in which to store custom receive message callback hooks */
struct switch_io_event_hook_outgoing_channel {
	switch_outgoing_channel_hook_t outgoing_channel;
	struct switch_io_event_hook_outgoing_channel *next;
};

/*! \brief Node in which to store custom receive message callback hooks */
struct switch_io_event_hook_receive_message {
	switch_receive_message_hook_t receive_message;
	struct switch_io_event_hook_receive_message *next;
};


/*! \brief Node in which to store custom receive message callback hooks */
struct switch_io_event_hook_receive_event {
	/*! the event callback hook */
	switch_receive_event_hook_t receive_event;
	struct switch_io_event_hook_receive_event *next;
};

/*! \brief Node in which to store custom read frame channel callback hooks */
struct switch_io_event_hook_read_frame {
	/*! the read frame channel callback hook */
	switch_read_frame_hook_t read_frame;
	struct switch_io_event_hook_read_frame *next;
};

/*! \brief Node in which to store custom read frame channel callback hooks */
struct switch_io_event_hook_video_read_frame {
	/*! the read frame channel callback hook */
	switch_read_frame_hook_t video_read_frame;
	struct switch_io_event_hook_video_read_frame *next;
};

/*! \brief Node in which to store custom write_frame channel callback hooks */
struct switch_io_event_hook_write_frame {
	/*! the write_frame channel callback hook */
	switch_write_frame_hook_t write_frame;
	struct switch_io_event_hook_write_frame *next;
};

/*! \brief Node in which to store custom video_write_frame channel callback hooks */
struct switch_io_event_hook_video_write_frame {
	/*! the video_write_frame channel callback hook */
	switch_video_write_frame_hook_t video_write_frame;
	struct switch_io_event_hook_video_write_frame *next;
};

/*! \brief Node in which to store custom kill channel callback hooks */
struct switch_io_event_hook_kill_channel {
	/*! the kill channel callback hook */
	switch_kill_channel_hook_t kill_channel;
	struct switch_io_event_hook_kill_channel *next;
};

/*! \brief Node in which to store custom send dtmf channel callback hooks */
struct switch_io_event_hook_send_dtmf {
	/*! the send dtmf channel callback hook */
	switch_send_dtmf_hook_t send_dtmf;
	struct switch_io_event_hook_send_dtmf *next;
};

/*! \brief Node in which to store custom recv dtmf channel callback hooks */
struct switch_io_event_hook_recv_dtmf {
	/*! the recv dtmf channel callback hook */
	switch_recv_dtmf_hook_t recv_dtmf;
	struct switch_io_event_hook_recv_dtmf *next;
};

/*! \brief Node in which to store state change callback hooks */
struct switch_io_event_hook_state_change {
	/*! the state change channel callback hook */
	switch_state_change_hook_t state_change;
	struct switch_io_event_hook_state_change *next;
};


struct switch_io_event_hook_resurrect_session {
	switch_resurrect_session_hook_t resurrect_session;
	struct switch_io_event_hook_resurrect_session *next;
};

/*! \brief A table of lists of io_event_hooks to store the event hooks associated with a session */
struct switch_io_event_hooks {
	/*! a list of outgoing channel hooks */
	switch_io_event_hook_outgoing_channel_t *outgoing_channel;
	/*! a list of receive message hooks */
	switch_io_event_hook_receive_message_t *receive_message;
	/*! a list of queue message hooks */
	switch_io_event_hook_receive_event_t *receive_event;
	/*! a list of read frame hooks */
	switch_io_event_hook_read_frame_t *read_frame;
	/*! a list of video read frame hooks */
	switch_io_event_hook_video_read_frame_t *video_read_frame;
	/*! a list of write frame hooks */
	switch_io_event_hook_write_frame_t *write_frame;
	/*! a list of video write frame hooks */
	switch_io_event_hook_video_write_frame_t *video_write_frame;
	/*! a list of kill channel hooks */
	switch_io_event_hook_kill_channel_t *kill_channel;
	/*! a list of send dtmf hooks */
	switch_io_event_hook_send_dtmf_t *send_dtmf;
	/*! a list of recv dtmf hooks */
	switch_io_event_hook_recv_dtmf_t *recv_dtmf;
	/*! a list of state change hooks */
	switch_io_event_hook_state_change_t *state_change;
	switch_io_event_hook_resurrect_session_t *resurrect_session;
};

extern switch_io_event_hooks_t switch_core_session_get_event_hooks(switch_core_session_t *session);

#define NEW_HOOK_DECL_ADD_P(_NAME) SWITCH_DECLARE(switch_status_t) switch_core_event_hook_add_##_NAME \
															   (switch_core_session_t *session, switch_##_NAME##_hook_t _NAME)

#define NEW_HOOK_DECL_REM_P(_NAME) SWITCH_DECLARE(switch_status_t) switch_core_event_hook_remove_##_NAME \
																   (switch_core_session_t *session, switch_##_NAME##_hook_t _NAME)

#define NEW_HOOK_DECL(_NAME) NEW_HOOK_DECL_ADD_P(_NAME)					\
	{																	\
		switch_io_event_hook_##_NAME##_t *hook, *ptr;					\
		assert(_NAME != NULL);											\
		for (ptr = session->event_hooks._NAME; ptr && ptr->next; ptr = ptr->next) \
			if (ptr->_NAME == _NAME) return SWITCH_STATUS_FALSE;		\
		if (ptr && ptr->_NAME == _NAME) return SWITCH_STATUS_FALSE;		\
		if ((hook = switch_core_session_alloc(session, sizeof(*hook))) != 0) { \
			hook->_NAME = _NAME ;										\
			if (! session->event_hooks._NAME ) {						\
				session->event_hooks._NAME = hook;						\
			} else {													\
				ptr->next = hook;										\
			}															\
			return SWITCH_STATUS_SUCCESS;								\
		}																\
		return SWITCH_STATUS_MEMERR;									\
	}																	\
	NEW_HOOK_DECL_REM_P(_NAME)											\
	{																	\
		switch_io_event_hook_##_NAME##_t *ptr, *last = NULL;			\
		assert(_NAME != NULL);											\
		for (ptr = session->event_hooks._NAME; ptr; ptr = ptr->next) {	\
			if (ptr->_NAME == _NAME) {									\
				if (last) {												\
					last->next = ptr->next;								\
				} else {												\
					session->event_hooks._NAME = ptr->next;				\
				}														\
				return SWITCH_STATUS_SUCCESS;							\
			}															\
			last = ptr;													\
		}																\
		return SWITCH_STATUS_FALSE;										\
	}


NEW_HOOK_DECL_ADD_P(outgoing_channel);
NEW_HOOK_DECL_ADD_P(receive_message);
NEW_HOOK_DECL_ADD_P(receive_event);
NEW_HOOK_DECL_ADD_P(state_change);
NEW_HOOK_DECL_ADD_P(read_frame);
NEW_HOOK_DECL_ADD_P(write_frame);
NEW_HOOK_DECL_ADD_P(video_read_frame);
NEW_HOOK_DECL_ADD_P(video_write_frame);
NEW_HOOK_DECL_ADD_P(kill_channel);
NEW_HOOK_DECL_ADD_P(send_dtmf);
NEW_HOOK_DECL_ADD_P(recv_dtmf);
NEW_HOOK_DECL_ADD_P(resurrect_session);

NEW_HOOK_DECL_REM_P(outgoing_channel);
NEW_HOOK_DECL_REM_P(receive_message);
NEW_HOOK_DECL_REM_P(receive_event);
NEW_HOOK_DECL_REM_P(state_change);
NEW_HOOK_DECL_REM_P(read_frame);
NEW_HOOK_DECL_REM_P(write_frame);
NEW_HOOK_DECL_REM_P(video_read_frame);
NEW_HOOK_DECL_REM_P(video_write_frame);
NEW_HOOK_DECL_REM_P(kill_channel);
NEW_HOOK_DECL_REM_P(send_dtmf);
NEW_HOOK_DECL_REM_P(recv_dtmf);
NEW_HOOK_DECL_REM_P(resurrect_session);


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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
