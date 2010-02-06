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
 *
 * switch_scheduler.h -- Scheduler Engine
 *
 */

#ifndef SWITCH_SCHEDULER_H
#define SWITCH_SCHEDULER_H

#include <switch.h>

SWITCH_BEGIN_EXTERN_C
///\defgroup sched1 Scheduler
///\ingroup core1
///\{
	struct switch_scheduler_task {
	int64_t created;
	int64_t runtime;
	uint32_t cmd_id;
	char *group;
	void *cmd_arg;
	uint32_t task_id;
};


/*!
  \brief Schedule a task in the future
  \param task_runtime the time in epoch seconds to execute the task.
  \param func the callback function to execute when the task is executed.
  \param desc an arbitrary description of the task.
  \param group a group id tag to link multiple tasks to a single entity.
  \param cmd_id an arbitrary index number be used in the callback.
  \param cmd_arg user data to be passed to the callback.
  \param flags flags to alter behaviour 
  \return the id of the task
*/
SWITCH_DECLARE(uint32_t) switch_scheduler_add_task(time_t task_runtime,
												   switch_scheduler_func_t func,
												   const char *desc, const char *group, uint32_t cmd_id, void *cmd_arg, switch_scheduler_flag_t flags);

/*!
  \brief Delete a scheduled task
  \param task_id the id of the task
  \return the number of jobs deleted
*/
SWITCH_DECLARE(uint32_t) switch_scheduler_del_task_id(uint32_t task_id);

/*!
  \brief Delete a scheduled task based on the group name
  \param group the group name
  \return the number of jobs deleted
*/
SWITCH_DECLARE(uint32_t) switch_scheduler_del_task_group(const char *group);


/*!
  \brief Start the scheduler system
*/
SWITCH_DECLARE(void) switch_scheduler_task_thread_start(void);

/*!
  \brief Stop the scheduler system
*/
SWITCH_DECLARE(void) switch_scheduler_task_thread_stop(void);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
