#include "switch_telnyx.h"
#include <string>
#include <vector>
#include <algorithm>

typedef std::vector<switch_telnyx_on_populate_core_heartbeat_func> heartbeat_callbacks;
typedef std::vector<switch_telnyx_on_populate_event_func> populate_event_callback;
typedef std::vector<switch_telnyx_on_populate_plain_status_func> plain_status_callback;
typedef std::vector<switch_telnyx_on_populate_json_status_func> json_status_callback;

static switch_telnyx_event_dispatch_t _event_dispatch;
static heartbeat_callbacks _heartbeat_callbacks;
static populate_event_callback _populate_event_callbacks;
static plain_status_callback _plain_status_callbacks;
static json_status_callback _json_status_callbacks;
static switch_thread_rwlock_t* _rwlock;

void switch_telnyx_init(switch_memory_pool_t *pool)
{
	memset(&_event_dispatch, 0, sizeof(_event_dispatch));
	switch_thread_rwlock_create(&_rwlock, pool);
}

void switch_telnyx_deinit()
{
	switch_thread_rwlock_wrlock(_rwlock);
	_populate_event_callbacks.clear();
	_heartbeat_callbacks.clear();
	switch_thread_rwlock_unlock(_rwlock);
	memset(&_event_dispatch, 0, sizeof(_event_dispatch));
}

switch_telnyx_event_dispatch_t* switch_telnyx_event_dispatch()
{
	return &_event_dispatch;
}

static bool get_channel_variable(switch_channel_t *channel, const char* name, std::string& value)
{
	if (!channel) {
		return false;
	}
	
	const char* val = switch_channel_get_variable_dup(channel, name, SWITCH_FALSE, -1);
	if (!val) {
		return false;
	}
	value = val;
	return true;
}

int switch_telnyx_hangup_cause_to_sip(switch_core_session_t *session, switch_call_cause_t cause)
{
	return (_event_dispatch.switch_telnyx_hangup_cause_to_sip ? _event_dispatch.switch_telnyx_hangup_cause_to_sip(session, cause) : -1);
}

switch_bool_t switch_telnyx_sofia_on_init(switch_core_session_t *session)
{
	if (_event_dispatch.switch_telnyx_sofia_on_init) {
		return _event_dispatch.switch_telnyx_sofia_on_init(session);
	}
	return SWITCH_TRUE;
}

void switch_telnyx_sofia_on_hangup(switch_core_session_t *session)
{
	if (_event_dispatch.switch_telnyx_sofia_on_hangup) {
		_event_dispatch.switch_telnyx_sofia_on_hangup(session);
	}
}

void switch_telnyx_channel_on_answer(switch_core_session_t *session)
{
	if (_event_dispatch.switch_telnyx_channel_on_answer) {
		_event_dispatch.switch_telnyx_channel_on_answer(session);
	}
}

void switch_telnyx_on_set_variable(switch_channel_t* channel, const char* name, const char* value)
{
	if (_event_dispatch.switch_telnyx_on_set_variable) {
		_event_dispatch.switch_telnyx_on_set_variable(channel, name, value);
	}
}

int switch_telnyx_call_recover(switch_core_session_t* session)
{
	if (_event_dispatch.switch_telnyx_call_recover) {
		return _event_dispatch.switch_telnyx_call_recover(session);
	}
	return 0;
}

void switch_telnyx_on_channel_recover(switch_channel_t* channel)
{
	if (_event_dispatch.switch_telnyx_on_channel_recover) {
		_event_dispatch.switch_telnyx_on_channel_recover(channel);
	}
}

void switch_telnyx_process_audio_stats(switch_core_session_t* session, switch_rtp_stats_t* stats)
{
	if (_event_dispatch.switch_telnyx_process_audio_stats) {
		_event_dispatch.switch_telnyx_process_audio_stats(session, stats);
	}
}

void switch_telnyx_process_audio_quality(switch_core_session_t* session, double r)
{
	if (_event_dispatch.switch_telnyx_process_audio_quality) {
		_event_dispatch.switch_telnyx_process_audio_quality(session, r);
	}
}

void switch_telnyx_on_populate_event(switch_event_t* event)
{
	switch_thread_rwlock_rdlock(_rwlock);
	for (auto& cb : _populate_event_callbacks)
	{
    	cb(event); 
	}
	switch_thread_rwlock_unlock(_rwlock);
}

void switch_telnyx_on_populate_core_heartbeat(switch_event_t* event)
{
	switch_thread_rwlock_rdlock(_rwlock);
	for (auto& cb : _heartbeat_callbacks)
	{
    	cb(event); 
	}
	switch_thread_rwlock_unlock(_rwlock);
}

void switch_telnyx_on_populate_api_plain_status(switch_stream_handle_t *stream)
{
	switch_thread_rwlock_rdlock(_rwlock);
	for (auto& cb : _plain_status_callbacks)
	{
    	cb(stream); 
	}
	switch_thread_rwlock_unlock(_rwlock);
}

void switch_telnyx_on_populate_api_json_status(cJSON* reply)
{
	switch_thread_rwlock_rdlock(_rwlock);
	for (auto& cb : _json_status_callbacks)
	{
    	cb(reply); 
	}
	switch_thread_rwlock_unlock(_rwlock);
}

void switch_telnyx_add_populate_event_callback(switch_telnyx_on_populate_event_func cb)
{
	switch_thread_rwlock_wrlock(_rwlock);
	_populate_event_callbacks.push_back(cb);
	switch_thread_rwlock_unlock(_rwlock);
}

void switch_telnyx_add_core_heartbeat_callback(switch_telnyx_on_populate_core_heartbeat_func cb)
{
	switch_thread_rwlock_wrlock(_rwlock);
	_heartbeat_callbacks.push_back(cb);
	switch_thread_rwlock_unlock(_rwlock);
}

void switch_telnyx_add_populate_api_plain_status_callback(switch_telnyx_on_populate_plain_status_func cb)
{
	switch_thread_rwlock_wrlock(_rwlock);
	_plain_status_callbacks.push_back(cb);
	switch_thread_rwlock_unlock(_rwlock);
}

void switch_telnyx_add_populate_api_json_status_callback(switch_telnyx_on_populate_json_status_func cb)
{
	switch_thread_rwlock_wrlock(_rwlock);
	_json_status_callbacks.push_back(cb);
	switch_thread_rwlock_unlock(_rwlock);
}

void switch_telnyx_publish_json_event(const char* json)
{
	if (_event_dispatch.switch_telnyx_publish_json_event) {
		_event_dispatch.switch_telnyx_publish_json_event(json);
	}
}

void switch_telnyx_process_flaws(switch_rtp_t* rtp_session, int penalty)
{
	if (_event_dispatch.switch_telnyx_process_flaws) {
		_event_dispatch.switch_telnyx_process_flaws(rtp_session, penalty);
	}
}

void switch_telnyx_process_packet_loss(switch_rtp_t* rtp_session, int loss)
{
	if (_event_dispatch.switch_telnyx_process_packet_loss) {
		_event_dispatch.switch_telnyx_process_packet_loss(rtp_session, loss);
	}
}

void switch_telnyx_set_current_trace_message(const char* msg)
{
	if (_event_dispatch.switch_telnyx_set_current_trace_message) {
		_event_dispatch.switch_telnyx_set_current_trace_message(msg);
	}
}

switch_call_cause_t switch_telnyx_recompute_cause_code(switch_channel_t* channel, int status, switch_call_cause_t cause)
{
	if (_event_dispatch.switch_telnyx_recompute_cause_code) {
		return _event_dispatch.switch_telnyx_recompute_cause_code(channel, status, cause);
	}
	return cause;
}

switch_bool_t switch_telnyx_sip_cause_to_q850(int status, switch_call_cause_t* cause)
{
	if (_event_dispatch.switch_telnyx_sip_cause_to_q850) {
		return _event_dispatch.switch_telnyx_sip_cause_to_q850(status, cause);
	}
	return SWITCH_FALSE;
}

switch_bool_t switch_telnyx_sip_on_media_timeout(switch_channel_t* channel, switch_rtp_t* rtp_session)
{
	if (_event_dispatch.switch_telnyx_on_media_timeout) {
		return _event_dispatch.switch_telnyx_on_media_timeout(channel, rtp_session);
	}
	return SWITCH_TRUE;
}