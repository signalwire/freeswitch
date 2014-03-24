/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * mod_mongo.c -- API for MongoDB 
 *
 */
#include <switch.h>

#ifndef MAX
/* libbson will define MIN/MAX in a way that won't compile in FS */
#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))
#include <mongoc.h>
#undef MAX
#undef MIN
#else
#include <mongoc.h>
#endif

#define DELIMITER ';'
#define FIND_ONE_SYNTAX  "mongo_find_one ns; query; fields; options"
#define MAPREDUCE_SYNTAX "mongo_mapreduce ns; query"

SWITCH_MODULE_LOAD_FUNCTION(mod_mongo_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_mongo_shutdown);
SWITCH_MODULE_DEFINITION(mod_mongo, mod_mongo_load, mod_mongo_shutdown, NULL);

static struct {
	const char *map;
	const char *reduce;
	const char *finalize;
	const char *conn_str;
	mongoc_uri_t *uri;
	mongoc_client_pool_t *client_pool;
} globals;

/**
 * @param query_options_str
 * @return query options
 */
static int parse_query_options(char *query_options_str)
{
	int query_options = MONGOC_QUERY_NONE;
	if (strstr(query_options_str, "cursorTailable")) {
		query_options |= MONGOC_QUERY_TAILABLE_CURSOR;
	}
	if (strstr(query_options_str, "slaveOk")) {
		query_options |= MONGOC_QUERY_SLAVE_OK;
	}
	if (strstr(query_options_str, "oplogReplay")) {
		query_options |= MONGOC_QUERY_OPLOG_REPLAY;
	}
	if (strstr(query_options_str, "noCursorTimeout")) {
		query_options |= MONGOC_QUERY_NO_CURSOR_TIMEOUT;
	}
	if (strstr(query_options_str, "awaitData")) {
		query_options |= MONGOC_QUERY_AWAIT_DATA;
	}
	if (strstr(query_options_str, "exhaust")) {
		query_options |= MONGOC_QUERY_EXHAUST;
	}
	if (strstr(query_options_str, "partialResults")) {
		query_options |= MONGOC_QUERY_PARTIAL;
	}
	return query_options;
}

/**
 * @return a new connection to mongodb or NULL if error
 */
static mongoc_client_t *get_connection(void)
{
	mongoc_client_t *client = mongoc_client_pool_pop(globals.client_pool);
	if (!client) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to get connection to: %s\n", globals.conn_str);
		return NULL;
	}
	/* TODO auth */
	return client;
}

/**
 * Mark connection as finished
 */
static void connection_done(mongoc_client_t *conn)
{
	mongoc_client_pool_push(globals.client_pool, conn);
}

SWITCH_STANDARD_API(mongo_mapreduce_function)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *db = NULL, *collection = NULL, *json_query = NULL;

	db = strdup(cmd);
	switch_assert(db != NULL);

	if ((collection = strchr(db, '.'))) {
		*collection++ = '\0';
		if ((json_query = strchr(collection, DELIMITER))) {
			*json_query++ = '\0';
		}
	}

	if (!zstr(db) && !zstr(collection) && !zstr(json_query)) {
		mongoc_client_t *conn = get_connection();
		if (conn) {
			bson_error_t error;
			bson_t *query = bson_new_from_json((uint8_t *)json_query, strlen(json_query), &error);
			if (query) {
				bson_t out;
				bson_t cmd;
				bson_t child;

				/* build command to send to mongodb */
				bson_init(&cmd);
				BSON_APPEND_UTF8(&cmd, "mapreduce", collection);
				if (!zstr(globals.map)) {
					BSON_APPEND_CODE(&cmd, "map", globals.map);
				}
				if (!zstr(globals.reduce)) {
					BSON_APPEND_CODE(&cmd, "reduce", globals.reduce);
				}
				if (!zstr(globals.finalize)) {
					BSON_APPEND_CODE(&cmd, "finalize", globals.finalize);
				}
				if (!bson_empty(query)) {
					BSON_APPEND_DOCUMENT(&cmd, "query", query);
				}
				bson_append_document_begin(&cmd, "out", strlen("out"), &child);
				BSON_APPEND_INT32(&child, "inline", 1);
				bson_append_document_end(&cmd, &child);

				/* send command and get result */
				if (mongoc_client_command_simple(conn, db, &cmd, NULL /* read prefs */, &out, &error)) {
					char *json_result = bson_as_json(&out, NULL);
					stream->write_function(stream, "-OK\n%s\n", json_result);
					bson_free(json_result);
				} else {
					stream->write_function(stream, "-ERR\nmongo_run_command failed!\n");
				}

				bson_destroy(query);
				bson_destroy(&cmd);
				bson_destroy(&out);
			} else {
				stream->write_function(stream, "-ERR\nfailed to parse query!\n");
			}
			connection_done(conn);
		} else {
			stream->write_function(stream, "-ERR\nfailed to get connection!\n");
		}
	} else {
		stream->write_function(stream, "-ERR\n%s\n", MAPREDUCE_SYNTAX);
	}

	switch_safe_free(db);

	return status;
}

SWITCH_STANDARD_API(mongo_find_one_function)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *db = NULL, *collection = NULL, *json_query = NULL, *json_fields = NULL, *query_options_str = NULL;
	int query_options = 0;

	db = strdup(cmd);
	switch_assert(db != NULL);

	if ((collection = strchr(db, '.'))) {
		*collection++ = '\0';
		if ((json_query = strchr(collection, DELIMITER))) {
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
	}

	if (!zstr(db) && !zstr(collection) && !zstr(json_query) && !zstr(json_fields)) {
		bson_error_t error;
		mongoc_client_t *conn = get_connection();
		if (conn) {
			mongoc_collection_t *col = mongoc_client_get_collection(conn, db, collection);
			if (col) {
				bson_t *query = bson_new_from_json((uint8_t *)json_query, strlen(json_query), &error);
				bson_t *fields = bson_new_from_json((uint8_t *)json_fields, strlen(json_fields), &error);
				if (query && fields) {
					int ok = 0;
					/* send query */
					mongoc_cursor_t *cursor = mongoc_collection_find(col, query_options, 0, 1, 0, query, fields, NULL);
					if (cursor && mongoc_cursor_more(cursor) && !mongoc_cursor_error(cursor, &error)) {
						/* get result from cursor */
						const bson_t *result;
						if (mongoc_cursor_next(cursor, &result)) {
							char *json_result;
							json_result = bson_as_json(result, NULL);
							stream->write_function(stream, "-OK\n%s\n", json_result);
							bson_free(json_result);
							ok = 1;
						}
					}
					if (!ok) {
						stream->write_function(stream, "-ERR\nquery failed!\n");
					}
					if (cursor) {
						mongoc_cursor_destroy(cursor);
					}
				} else {
					stream->write_function(stream, "-ERR\nmissing query or fields!\n%s\n", FIND_ONE_SYNTAX);
				}
				if (query) {
					bson_destroy(query);
				}
				if (fields) {
					bson_destroy(fields);
				}
				mongoc_collection_destroy(col);
			} else {
				stream->write_function(stream, "-ERR\nunknown collection: %s\n", collection);
			}
			connection_done(conn);
		} else {
			stream->write_function(stream, "-ERR\nfailed to get connection!\n");
		}
	} else {
		stream->write_function(stream, "-ERR\n%s\n", FIND_ONE_SYNTAX);	  
	}

	switch_safe_free(db);

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
	globals.uri = NULL;
	globals.client_pool = NULL;

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
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "missing connection-string\n");
					status = SWITCH_STATUS_GENERR;
					goto done;
				} else {
					globals.conn_str = switch_core_strdup(pool, val);
					globals.uri = mongoc_uri_new(globals.conn_str);
					if (globals.uri) {
						globals.client_pool = mongoc_client_pool_new(globals.uri);
						if (!globals.client_pool) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to pool for connection-string: %s\n", globals.conn_str);
							status = SWITCH_STATUS_GENERR;
							goto done;
						}
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid connection-string: %s\n", globals.conn_str);
						status = SWITCH_STATUS_GENERR;
						goto done;
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
			}
		}
	}

	if (!globals.client_pool) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "No mongodb connection pool configured!  Make sure connection-string is set\n");
		status = SWITCH_STATUS_GENERR;
	}

done:
	switch_xml_free(xml);

	return status;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_mongo_load)
{
	switch_api_interface_t *api_interface;

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
	if (globals.client_pool) {
		mongoc_client_pool_destroy(globals.client_pool);
		globals.client_pool = NULL;
	}
	return SWITCH_STATUS_SUCCESS;
}

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

