/*
* FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
* Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
* Based on mod_skel by
* Anthony Minessale II <anthm@freeswitch.org>
*
* Contributor(s):
*
* Daniel Bryars <danb@aeriandi.com>
* Tim Brown <tim.brown@aeriandi.com>
* Anthony Minessale II <anthm@freeswitch.org>
* William King <william.king@quentustech.com>
* Mike Jerris <mike@jerris.com>
*
* kazoo.c -- Sends FreeSWITCH events to an AMQP broker
*
*/

#ifndef KAZOO_CONFIG_H
#define KAZOO_CONFIG_H

#include <switch.h>


struct kazoo_config_t {
	  switch_hash_t *hash;
	  switch_memory_pool_t *pool;
};

switch_status_t kazoo_config_loglevels(switch_memory_pool_t *pool, switch_xml_t cfg, kazoo_loglevels_ptr *ptr);
switch_status_t kazoo_config_filters(switch_memory_pool_t *pool, switch_xml_t cfg, kazoo_filter_ptr *ptr);
switch_status_t kazoo_config_fields(kazoo_config_ptr definitions, switch_memory_pool_t *pool, switch_xml_t cfg, kazoo_fields_ptr *ptr);

switch_status_t kazoo_config_event_handler(kazoo_config_ptr definitions, kazoo_config_ptr root, switch_xml_t cfg, kazoo_event_profile_ptr *ptr);
switch_status_t kazoo_config_fetch_handler(kazoo_config_ptr definitions, kazoo_config_ptr root, switch_xml_t cfg, kazoo_fetch_profile_ptr *ptr);
kazoo_config_ptr kazoo_config_event_handlers(kazoo_config_ptr definitions, switch_xml_t cfg);
kazoo_config_ptr kazoo_config_fetch_handlers(kazoo_config_ptr definitions, switch_xml_t cfg);
kazoo_config_ptr kazoo_config_definitions(switch_xml_t cfg);
void destroy_config(kazoo_config_ptr *ptr);

#endif /* KAZOO_CONFIG_H */

