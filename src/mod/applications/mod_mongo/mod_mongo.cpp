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
 * Christopher Rienzo <crienzo@grasshopper.com>
 *
 * mod_mongo.cpp -- API for MongoDB 
 *
 */

#include <switch.h>
#include <mongo/client/dbclient.h>

using namespace mongo;

#define DELIMITER ';'
#define FIND_ONE_SYNTAX  "mongo_find_one ns; query; fields; options"
#define MAPREDUCE_SYNTAX "mongo_mapreduce ns; query"

static struct {
	const char *map;
	const char *reduce;
	const char *finalize;
	const char *conn_str;
	double socket_timeout;
} globals;

static int parse_query_options(char *query_options_str)
{
	int query_options = 0;
	if (strstr(query_options_str, "cursorTailable")) {
		query_options |= QueryOption_CursorTailable;
	}
	if (strstr(query_options_str, "slaveOk")) {
		query_options |= QueryOption_SlaveOk;
	}
	if (strstr(query_options_str, "oplogReplay")) {
		query_options |= QueryOption_OplogReplay;
	}
	if (strstr(query_options_str, "noCursorTimeout")) {
		query_options |= QueryOption_NoCursorTimeout;
	}
	if (strstr(query_options_str, "awaitData")) {
		query_options |= QueryOption_AwaitData;
	}
	if (strstr(query_options_str, "exhaust")) {
		query_options |= QueryOption_Exhaust;
	}
	if (strstr(query_options_str, "partialResults")) {
		query_options |= QueryOption_PartialResults;
	}
	return query_options;
}

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
			scoped_ptr<ScopedDbConnection> conn(ScopedDbConnection::getScopedDbConnection(string(globals.conn_str, globals.socket_timeout)));
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

			try {
				conn->get()->runCommand(nsGetDB(ns), cmd.done(), out);
				stream->write_function(stream, "-OK\n%s\n", out.jsonString().c_str());
			} catch (DBException &e) {
				stream->write_function(stream, "-ERR\n%s\n", e.toString().c_str());
			} catch (...) {
				stream->write_function(stream, "-ERR\nUnknown exception!\n");
			}
			conn->done();
		} catch (DBException &e) {
			stream->write_function(stream, "-ERR\n%s\n", e.toString().c_str());
		} catch (...) {
			stream->write_function(stream, "-ERR\nUnknown exception!\n");
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
	char *ns = NULL, *json_query = NULL, *json_fields = NULL, *query_options_str = NULL;
	int query_options = 0;
	
	ns = strdup(cmd);
	switch_assert(ns != NULL);

	if ((json_query = strchr(ns, DELIMITER))) {
		*json_query++ = '\0';
		if ((json_fields = strchr(json_query, DELIMITER))) {
			*json_fields++ = '\0';
			if ((query_options_str = strchr(json_fields, DELIMITER))) {
				*query_options_str++ = '\0';
				if (!zstr(query_options_str)) {
					query_options = parse_query_options(query_options_str);
				}
			}
		}
	}

	if (!zstr(ns) && !zstr(json_query) && !zstr(json_fields)) {
		try {
			scoped_ptr<ScopedDbConnection> conn(ScopedDbConnection::getScopedDbConnection(string(globals.conn_str), globals.socket_timeout));
			BSONObj query = fromjson(json_query);
			BSONObj fields = fromjson(json_fields);
			try {
				BSONObj res = conn->get()->findOne(ns, Query(query), &fields, query_options);
				stream->write_function(stream, "-OK\n%s\n", res.jsonString().c_str());
			} catch (DBException &e) {
				stream->write_function(stream, "-ERR\n%s\n", e.toString().c_str());
			} catch (...) {
				stream->write_function(stream, "-ERR\nUnknown exception!\n");
			}
			conn->done();
		} catch (DBException &e) {
			stream->write_function(stream, "-ERR\n%s\n", e.toString().c_str());
		} catch (...) {
			stream->write_function(stream, "-ERR\nUnknown exception!\n");
		}
	} else {
	  stream->write_function(stream, "-ERR\n%s\n", FIND_ONE_SYNTAX);	  
	}

	switch_safe_free(ns);

	return status;
}

static switch_status_t config(switch_memory_pool_t *pool)
{
	const char *cf = "mongo.conf";
	switch_xml_t cfg, xml, settings, param;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	/* set defaults */
	globals.map = "";
	globals.reduce = "";
	globals.finalize = "";
	globals.conn_str = "";
	globals.socket_timeout = 0.0;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return SWITCH_STATUS_GENERR;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcmp(var, "connection-string")) {
				if (zstr(val)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "missing connection-string value\n");
					status = SWITCH_STATUS_GENERR;
				} else {
					try {
						string errmsg;
						ConnectionString cs = ConnectionString::parse(string(val), errmsg);
						if (!cs.isValid()) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "connection-string \"%s\" is not valid: %s\n", val, errmsg.c_str());
							status = SWITCH_STATUS_GENERR;
						} else {
							globals.conn_str = switch_core_strdup(pool, val);
						}
					} catch (DBException &e) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "connection-string \"%s\" is not valid: %s\n", val, e.toString().c_str());
						status = SWITCH_STATUS_GENERR;
					} catch (...) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "connection-string \"%s\" is not valid\n", val);
						status = SWITCH_STATUS_GENERR;
					}
				}
			} else if (!strcmp(var, "map")) {
				if (!zstr(val)) {
					globals.map = switch_core_strdup(pool, val);
				}
			} else if (!strcmp(var, "reduce")) {
				if (!zstr(val)) {
					globals.reduce = switch_core_strdup(pool, val);
				}
			} else if (!strcmp(var, "finalize")) {
				if (!zstr(val)) {
					globals.finalize = switch_core_strdup(pool, val);
				}
			} else if (!strcmp(var, "socket-timeout")) {
				if (!zstr(val)) {
					if (switch_is_number(val)) {
						double timeout = atof(val);
						if (timeout >= 0.0) {
							globals.socket_timeout = timeout;
						} else {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "socket-timeout \"%s\" is not valid\n", val);
						}
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "socket-timeout \"%s\" is not valid\n", val);
					}
				}
			} else if (!strcmp(var, "max-connections")) {
				if (!zstr(val)) {
					if (switch_is_number(val)) {
						int max_connections = atoi(val);
						if (max_connections > 0) {
							PoolForHost::setMaxPerHost(max_connections);
						} else {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "max-connections \"%s\" is not valid\n", val);
						}
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "max-connections \"%s\" is not valid\n", val);
					}
				}
			}
		}
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

	if (config(pool) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_TERM;
	}

	SWITCH_ADD_API(api_interface, "mongo_find_one", "findOne", mongo_find_one_function, FIND_ONE_SYNTAX);
	SWITCH_ADD_API(api_interface, "mongo_mapreduce", "Map/Reduce", mongo_mapreduce_function, MAPREDUCE_SYNTAX);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_mongo_shutdown)
{
	ScopedDbConnection::clearPool();
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

