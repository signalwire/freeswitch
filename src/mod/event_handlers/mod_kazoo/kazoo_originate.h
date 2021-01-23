#pragma once

#include <switch.h>

SWITCH_DECLARE(switch_status_t) kz_switch_ivr_enterprise_originate(switch_core_session_t *session,
																switch_core_session_t **bleg,
																switch_call_cause_t *cause,
																const char *bridgeto,
																uint32_t timelimit_sec,
																const switch_state_handler_table_t *table,
																const char *cid_name_override,
																const char *cid_num_override,
																switch_caller_profile_t *caller_profile_override,
																switch_event_t *ovars, switch_originate_flag_t flags,
																switch_call_cause_t *cancel_cause);

SWITCH_DECLARE(switch_status_t) kz_switch_ivr_originate(switch_core_session_t *session,
													 switch_core_session_t **bleg,
													 switch_call_cause_t *cause,
													 const char *bridgeto,
													 uint32_t timelimit_sec,
													 const switch_state_handler_table_t *table,
													 const char *cid_name_override,
													 const char *cid_num_override,
													 switch_caller_profile_t *caller_profile_override,
													 switch_event_t *ovars, switch_originate_flag_t flags,
													 switch_call_cause_t *cancel_cause,
													 switch_dial_handle_t *dh);
