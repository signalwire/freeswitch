/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2013, Anthony Minessale II <anthm@freeswitch.org>
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
 * Tamas Cseke <cstomi.levlist@gmail.com>
 *
 * mod_mongo.h -- API for MongoDB 
 *
 */

#ifndef MOD_MONGO_H
#define MOD_MONGO_H

#include <mongo/client/dbclient.h>

using namespace mongo;

typedef struct {
	char *conn_str;

	switch_size_t min_connections;
	switch_size_t max_connections;
	switch_size_t size;  
	switch_queue_t *connections;
	switch_mutex_t *mutex;
	switch_memory_pool_t *pool;

} mongo_connection_pool_t;


switch_status_t mongo_connection_create(DBClientBase **connection, const char *conn_str);
void mongo_connection_destroy(DBClientBase **conn);

switch_status_t mongo_connection_pool_create(mongo_connection_pool_t **conn_pool, switch_size_t min_connections, switch_size_t max_connections,
											const char *conn_str);
void mongo_connection_pool_destroy(mongo_connection_pool_t **conn_pool);


DBClientBase *mongo_connection_pool_get(mongo_connection_pool_t *conn_pool);
switch_status_t mongo_connection_pool_put(mongo_connection_pool_t *conn_pool, DBClientBase *conn, switch_bool_t destroy);


#endif

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
