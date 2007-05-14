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
 * switch_core_event_hook.c Core Event Hooks
 *
 */
#include "switch.h"
#include "private/switch_core_pvt.h"

SWITCH_DECLARE(switch_status_t) switch_core_event_hook_add_outgoing(switch_core_session_t *session, switch_outgoing_channel_hook_t outgoing_channel)
{
	switch_io_event_hook_outgoing_channel_t *hook, *ptr;

	assert(outgoing_channel != NULL);
	if ((hook = switch_core_session_alloc(session, sizeof(*hook))) != 0) {
		hook->outgoing_channel = outgoing_channel;
		if (!session->event_hooks.outgoing_channel) {
			session->event_hooks.outgoing_channel = hook;
		} else {
			for (ptr = session->event_hooks.outgoing_channel; ptr && ptr->next; ptr = ptr->next);
			ptr->next = hook;

		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;
}

SWITCH_DECLARE(switch_status_t) switch_core_event_hook_add_state_change(switch_core_session_t *session, switch_state_change_hook_t state_change)
{
	switch_io_event_hook_state_change_t *hook, *ptr;

	assert(state_change != NULL);
	if ((hook = switch_core_session_alloc(session, sizeof(*hook))) != 0) {
		hook->state_change = state_change;
		if (!session->event_hooks.state_change) {
			session->event_hooks.state_change = hook;
		} else {
			for (ptr = session->event_hooks.state_change; ptr && ptr->next; ptr = ptr->next);
			ptr->next = hook;

		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;

}

SWITCH_DECLARE(switch_status_t) switch_core_event_hook_add_read_frame(switch_core_session_t *session, switch_read_frame_hook_t read_frame)
{
	switch_io_event_hook_read_frame_t *hook, *ptr;

	assert(read_frame != NULL);
	if ((hook = switch_core_session_alloc(session, sizeof(*hook))) != 0) {
		hook->read_frame = read_frame;
		if (!session->event_hooks.read_frame) {
			session->event_hooks.read_frame = hook;
		} else {
			for (ptr = session->event_hooks.read_frame; ptr && ptr->next; ptr = ptr->next);
			ptr->next = hook;

		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;

}

SWITCH_DECLARE(switch_status_t) switch_core_event_hook_add_write_frame(switch_core_session_t *session, switch_write_frame_hook_t write_frame)
{
	switch_io_event_hook_write_frame_t *hook, *ptr;

	assert(write_frame != NULL);
	if ((hook = switch_core_session_alloc(session, sizeof(*hook))) != 0) {
		hook->write_frame = write_frame;
		if (!session->event_hooks.write_frame) {
			session->event_hooks.write_frame = hook;
		} else {
			for (ptr = session->event_hooks.write_frame; ptr && ptr->next; ptr = ptr->next);
			ptr->next = hook;

		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;

}

SWITCH_DECLARE(switch_status_t) switch_core_event_hook_add_video_read_frame(switch_core_session_t *session, switch_video_read_frame_hook_t video_read_frame)
{
	switch_io_event_hook_video_read_frame_t *hook, *ptr;

	assert(video_read_frame != NULL);
	if ((hook = switch_core_session_alloc(session, sizeof(*hook))) != 0) {
		hook->video_read_frame = video_read_frame;
		if (!session->event_hooks.video_read_frame) {
			session->event_hooks.video_read_frame = hook;
		} else {
			for (ptr = session->event_hooks.video_read_frame; ptr && ptr->next; ptr = ptr->next);
			ptr->next = hook;

		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;

}

SWITCH_DECLARE(switch_status_t) switch_core_event_hook_add_video_write_frame(switch_core_session_t *session, switch_video_write_frame_hook_t video_write_frame)
{
	switch_io_event_hook_video_write_frame_t *hook, *ptr;

	assert(video_write_frame != NULL);
	if ((hook = switch_core_session_alloc(session, sizeof(*hook))) != 0) {
		hook->video_write_frame = video_write_frame;
		if (!session->event_hooks.video_write_frame) {
			session->event_hooks.video_write_frame = hook;
		} else {
			for (ptr = session->event_hooks.video_write_frame; ptr && ptr->next; ptr = ptr->next);
			ptr->next = hook;

		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;

}

SWITCH_DECLARE(switch_status_t) switch_core_event_hook_add_kill_channel(switch_core_session_t *session, switch_kill_channel_hook_t kill_channel)
{
	switch_io_event_hook_kill_channel_t *hook, *ptr;

	assert(kill_channel != NULL);
	if ((hook = switch_core_session_alloc(session, sizeof(*hook))) != 0) {
		hook->kill_channel = kill_channel;
		if (!session->event_hooks.kill_channel) {
			session->event_hooks.kill_channel = hook;
		} else {
			for (ptr = session->event_hooks.kill_channel; ptr && ptr->next; ptr = ptr->next);
			ptr->next = hook;

		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;

}

SWITCH_DECLARE(switch_status_t) switch_core_event_hook_add_waitfor_read(switch_core_session_t *session, switch_waitfor_read_hook_t waitfor_read)
{
	switch_io_event_hook_waitfor_read_t *hook, *ptr;

	assert(waitfor_read != NULL);
	if ((hook = switch_core_session_alloc(session, sizeof(*hook))) != 0) {
		hook->waitfor_read = waitfor_read;
		if (!session->event_hooks.waitfor_read) {
			session->event_hooks.waitfor_read = hook;
		} else {
			for (ptr = session->event_hooks.waitfor_read; ptr && ptr->next; ptr = ptr->next);
			ptr->next = hook;

		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;

}

SWITCH_DECLARE(switch_status_t) switch_core_event_hook_add_waitfor_write(switch_core_session_t *session, switch_waitfor_write_hook_t waitfor_write)
{
	switch_io_event_hook_waitfor_write_t *hook, *ptr;

	assert(waitfor_write != NULL);
	if ((hook = switch_core_session_alloc(session, sizeof(*hook))) != 0) {
		hook->waitfor_write = waitfor_write;
		if (!session->event_hooks.waitfor_write) {
			session->event_hooks.waitfor_write = hook;
		} else {
			for (ptr = session->event_hooks.waitfor_write; ptr && ptr->next; ptr = ptr->next);
			ptr->next = hook;

		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;

}


SWITCH_DECLARE(switch_status_t) switch_core_event_hook_add_send_dtmf(switch_core_session_t *session, switch_send_dtmf_hook_t send_dtmf)
{
	switch_io_event_hook_send_dtmf_t *hook, *ptr;

	assert(send_dtmf != NULL);
	if ((hook = switch_core_session_alloc(session, sizeof(*hook))) != 0) {
		hook->send_dtmf = send_dtmf;
		if (!session->event_hooks.send_dtmf) {
			session->event_hooks.send_dtmf = hook;
		} else {
			for (ptr = session->event_hooks.send_dtmf; ptr && ptr->next; ptr = ptr->next);
			ptr->next = hook;

		}

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;

}
