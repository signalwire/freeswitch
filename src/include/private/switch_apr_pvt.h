/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2021, Anthony Minessale II <anthm@freeswitch.org>
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
 * Andrey Volk <andywolk@gmail.com>
 *
 *
 * switch_apr_pvt.h - APR
 *
 */

#ifndef __SWITCH_APR_PVT_H__
#define __SWITCH_APR_PVT_H__

/* for fspr_pool_create and fspr_pool_destroy */
/* functions only used in this file so not exposed */
#include <fspr_pools.h>

/* for fspr_hash_make, fspr_hash_pool_get, fspr_hash_set */
/* functions only used in this file so not exposed */
#include <fspr_hash.h>

/* for fspr_pvsprintf */
/* function only used in this file so not exposed */
#include <fspr_strings.h>

/* for fspr_initialize and fspr_terminate */
/* function only used in this file so not exposed */
#include <fspr_general.h>

#include <fspr_portable.h>

typedef struct switch_apr_queue_t switch_apr_queue_t;
fspr_status_t switch_apr_queue_create(switch_apr_queue_t **q, unsigned int queue_capacity, fspr_pool_t *a);
fspr_status_t switch_apr_queue_push(switch_apr_queue_t *queue, void *data);
fspr_status_t switch_apr_queue_trypush(switch_apr_queue_t *queue, void *data);
unsigned int switch_apr_queue_size(switch_apr_queue_t *queue);
fspr_status_t switch_apr_queue_pop(switch_apr_queue_t *queue, void **data);
fspr_status_t switch_apr_queue_pop_timeout(switch_apr_queue_t *queue, void **data, fspr_interval_time_t timeout);
fspr_status_t switch_apr_queue_trypop(switch_apr_queue_t *queue, void **data);
fspr_status_t switch_apr_queue_interrupt_all(switch_apr_queue_t *queue);
fspr_status_t switch_apr_queue_term(switch_apr_queue_t *queue);

#endif // __SWITCH_APR_PVT_H__

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
