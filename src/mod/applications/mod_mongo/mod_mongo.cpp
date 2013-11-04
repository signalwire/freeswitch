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
 * mod_mongo.cpp -- API for MongoDB 
 *
 */

#include <switch.h>
#include "mod_mongo.h"

#define DELIMITER ';'
#define FIND_ONE_SYNTAX  "mongo_find_one ns; query; fields"
#define MAPREDUCE_SYNTAX "mongo_mapreduce ns; query"

static struct {
	mongo_connection_pool_t *conn_pool;
	char *map;
	char *reduce;
	char *finalize;
} globals;

SWITCH_STANDARD_API(mongo_mapreduce_function)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	DBClientBase *conn = NULL;
	char *ns = NULL, *json_query = NULL;

	ns = strdup(cmd);
	switch_assert(ns != NULL);

	if ((json_query = strchr(ns, DELIMITER))) {
		*json_query++ = '\0';
	}

	if (!zstr(ns) && !zstr(json_query)) {
		try {
			BSONObj query = fromjson(json_query);
			BSONObj out;
			BSONObjBuilder cmd;

			cmd.append("mapreduce", nsGetCollection(ns));
			if (!zstr(globals.map)) {
				cmd.appendCode("map", globals.map);
			}
			if (!zstr(globals.reduce)) {
				cmd.appendCode("reduce", globals.reduce);
			}
			if (!zstr(globals.finalize)) {
				cmd.appendCode("finalize", globals.finalize);
			}
			if(!query.isEmpty()) {
				cmd.append("query", query);
			}
			cmd.append("out", BSON("inline" << 1));

			conn = mongo_connection_pool_get(globals.conn_pool);
			if (conn) {
				conn->runCommand(nsGetDB(ns), cmd.done(), out);
				mongo_connection_pool_put(globals.conn_pool, conn, SWITCH_FALSE);

				stream->write_function(stream, "-OK\n%s\n", out.jsonString().c_str());
			} else {
				stream->write_function(stream, "-ERR\nNo connection\n");
			}
		} catch (DBException &e) {
			if (conn) {
				mongo_connection_pool_put(globals.conn_pool, conn, SWITCH_TRUE);
			}
			stream->write_function(stream, "-ERR\n%s\n", e.toString().c_str());
		}
	} else {
		stream->write_function(stream, "-ERR\n%s\n", MAPREDUCE_SYNTAX);	  
	}

	switch_safe_free(ns);

	return status;
}


SWITCH_STANDARD_API(mongo_find_one_function) 
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *ns = NULL, *json_query = NULL, *json_fields = NULL;

	ns = strdup(cmd);
	switch_assert(ns != NULL);

	if ((json_query = strchr(ns, DELIMITER))) {
		*json_query++ = '\0';
		if ((json_fields = strchr(json_query, DELIMITER))) {
			*json_fields++ = '\0';
		}
	}

	if (!zstr(ns) && !zstr(json_query) && !zstr(json_fields)) {

		DBClientBase *conn = NULL;

		try {
			BSONObj query = fromjson(json_query);
			BSONObj fields = fromjson(json_fields);

			conn = mongo_connection_pool_get(globals.conn_pool);
			if (conn) {
				BSONObj res = conn->findOne(ns, Query(query), &fields);
				mongo_connection_pool_put(globals.conn_pool, conn, SWITCH_FALSE);

				stream->write_function(stream, "-OK\n%s\n", res.jsonString().c_str());
			} else {
				stream->write_function(stream, "-ERR\nNo connection\n");
			}
		} catch (DBException &e) {
			if (conn) {
				mongo_connection_pool_put(globals.conn_pool, conn, SWITCH_TRUE);
			}
			stream->write_function(stream, "-ERR\n%s\n", e.toString().c_str());
		}

	} else {
	  stream->write_function(stream, "-ERR\n%s\n", FIND_ONE_SYNTAX);	  
	}

	switch_safe_free(ns);

	return status;
}

static switch_status_t config(void)
{
	const char *cf = "mongo.conf";
	switch_xml_t cfg, xml, settings, param;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	const char *conn_str = "127.0.0.1";
	switch_size_t min_connections = 1, max_connections = 1;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return SWITCH_STATUS_GENERR;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");
			int tmp;

			if (!strcmp(var, "host")) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "'host' is deprecated. use 'connection-string'\n"); 
				conn_str = val;
			} else if (!strcmp(var, "connection-string")) {
				conn_str = val;
			} else if (!strcmp(var, "min-connections")) {
				if ((tmp = atoi(val)) > 0) {
					min_connections = tmp;
				}
			} else if (!strcmp(var, "max-connections")) {
				if ((tmp = atoi(val)) > 0) {
					max_connections = tmp;
				}
			} else if (!strcmp(var, "map")) {
				globals.map = strdup(val);
			} else if (!strcmp(var, "reduce")) {
				globals.reduce = strdup(val);
			} else if (!strcmp(var, "finalize")) {
				globals.finalize = strdup(val);
			}
		}
	}

	if (mongo_connection_pool_create(&globals.conn_pool, min_connections, max_connections, conn_str) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Can't create connection pool\n");
		status = SWITCH_STATUS_GENERR;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Mongo connection pool created [%s %d/%d]\n", conn_str, (int)min_connections, (int)max_connections);
	}

	switch_xml_free(xml);

	return status;
}


SWITCH_BEGIN_EXTERN_C

SWITCH_MODULE_LOAD_FUNCTION(mod_mongo_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_mongo_shutdown);
SWITCH_MODULE_DEFINITION(mod_mongo, mod_mongo_load, mod_mongo_shutdown, NULL);


SWITCH_MODULE_LOAD_FUNCTION(mod_mongo_load)
{
	switch_api_interface_t *api_interface;
	switch_application_interface_t *app_interface;

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	memset(&globals, 0, sizeof(globals));

	if (config() != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_TERM;
	}

	SWITCH_ADD_API(api_interface, "mongo_find_one", "findOne", mongo_find_one_function, FIND_ONE_SYNTAX);
	SWITCH_ADD_API(api_interface, "mongo_mapreduce", "Map/Reduce", mongo_mapreduce_function, MAPREDUCE_SYNTAX);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_mongo_shutdown)
{
	mongo_connection_pool_destroy(&globals.conn_pool);
	switch_safe_free(globals.map);
	switch_safe_free(globals.reduce);
	switch_safe_free(globals.finalize);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_END_EXTERN_C

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

