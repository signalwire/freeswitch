/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2015, Anthony Minessale II <anthm@freeswitch.org>
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
#define FIND_N_SYNTAX "mongo_find_n ns; query; fields; options; n"
#define MAPREDUCE_SYNTAX "mongo_mapreduce ns; query"

SWITCH_MODULE_LOAD_FUNCTION(mod_mongo_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_mongo_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_mongo_runtime);
SWITCH_MODULE_DEFINITION(mod_mongo, mod_mongo_load, mod_mongo_shutdown, mod_mongo_runtime);

static struct {
	int shutdown;
	const char *map;
	const char *reduce;
	const char *finalize;
	const char *conn_str;
	mongoc_client_pool_t *client_pool;
	const char *limit_database;
	const char *limit_collection;
	const char *limit_conn_str;
	int limit_cleanup_interval_sec;
	mongoc_client_pool_t *limit_client_pool;
	switch_mutex_t *mod_mongo_private_mutex;
	switch_thread_rwlock_t *limit_database_rwlock;
	switch_thread_rwlock_t *shutdown_rwlock;
} globals;

/**
 * resources acquired by this session
 */
struct mod_mongo_private {
	switch_hash_t *resources;
	switch_mutex_t *mutex;
};

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
static mongoc_client_t *get_connection(mongoc_client_pool_t *client_pool, const char *conn_str)
{
	mongoc_client_t *client = mongoc_client_pool_pop(client_pool);
	if (!client) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to get connection to: %s\n", conn_str);
		return NULL;
	}
	/* TODO auth */
	return client;
}

/**
 * Mark connection as finished
 */
static void connection_done(mongoc_client_pool_t *client_pool, mongoc_client_t *conn)
{
	mongoc_client_pool_push(client_pool, conn);
}

SWITCH_STANDARD_API(mod_mongo_mapreduce_function)
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
		mongoc_client_t *conn = get_connection(globals.client_pool, globals.conn_str);
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
			connection_done(globals.client_pool, conn);
		} else {
			stream->write_function(stream, "-ERR\nfailed to get connection!\n");
		}
	} else {
		stream->write_function(stream, "-ERR\n%s\n", MAPREDUCE_SYNTAX);
	}

	switch_safe_free(db);

	return status;
}

SWITCH_STANDARD_API(mod_mongo_find_n_function)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *db = NULL, *collection = NULL, *json_query = NULL, *json_fields = NULL, *query_options_str = NULL;
	int query_options = 0;
	int n = 1;

	db = strdup(cmd);
	switch_assert(db != NULL);

	if ((collection = strchr(db, '.'))) {
		*collection++ = '\0';
		if ((json_query = strchr(collection, DELIMITER))) {
			*json_query++ = '\0';
			if ((json_fields = strchr(json_query, DELIMITER))) {
				*json_fields++ = '\0';
				if ((query_options_str = strchr(json_fields, DELIMITER))) {
					char *n_str;
					*query_options_str++ = '\0';
					if (!zstr(query_options_str)) {
						query_options = parse_query_options(query_options_str);
					}
					if ((n_str = strchr(query_options_str, DELIMITER))) {
						*n_str++ = '\0';
						if (switch_is_number(n_str)) {
							n = atoi(n_str);
							if (n < 1) {
								n = 1;
							}
						}
					}
				}
			}
		}
	}

	if (!zstr(db) && !zstr(collection) && !zstr(json_query) && !zstr(json_fields)) {
		bson_error_t error;
		mongoc_client_t *conn = get_connection(globals.client_pool, globals.conn_str);
		if (conn) {
			mongoc_collection_t *col = mongoc_client_get_collection(conn, db, collection);
			if (col) {
				bson_t *query = bson_new_from_json((uint8_t *)json_query, strlen(json_query), &error);
				bson_t *fields = bson_new_from_json((uint8_t *)json_fields, strlen(json_fields), &error);
				if (query && fields) {
					/* send query */
					mongoc_cursor_t *cursor = mongoc_collection_find(col, query_options, 0, n, 0, query, fields, NULL);
					if (cursor && !mongoc_cursor_error(cursor, &error)) {
						/* get results from cursor */
						const bson_t *result;
						stream->write_function(stream, "-OK\n[");
						if (mongoc_cursor_more(cursor) && mongoc_cursor_next(cursor, &result)) {
							char *json_result;
							json_result = bson_as_json(result, NULL);
							stream->write_function(stream, "%s", json_result);
							bson_free(json_result);
						}
						while (mongoc_cursor_more(cursor) && mongoc_cursor_next(cursor, &result)) {
							char *json_result;
							json_result = bson_as_json(result, NULL);
							stream->write_function(stream, ",%s", json_result);
							bson_free(json_result);
						}
						stream->write_function(stream, "]\n");
					} else {
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
			connection_done(globals.client_pool, conn);
		} else {
			stream->write_function(stream, "-ERR\nfailed to get connection!\n");
		}
	} else {
		stream->write_function(stream, "-ERR\n%s\n", FIND_N_SYNTAX);
	}

	switch_safe_free(db);

	return status;
}

SWITCH_STANDARD_API(mod_mongo_find_one_function)
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
		mongoc_client_t *conn = get_connection(globals.client_pool, globals.conn_str);
		if (conn) {
			mongoc_collection_t *col = mongoc_client_get_collection(conn, db, collection);
			if (col) {
				bson_t *query = bson_new_from_json((uint8_t *)json_query, strlen(json_query), &error);
				bson_t *fields = bson_new_from_json((uint8_t *)json_fields, strlen(json_fields), &error);
				if (query && fields) {
					/* send query */
					mongoc_cursor_t *cursor = mongoc_collection_find(col, query_options, 0, 1, 0, query, fields, NULL);
					if (cursor && !mongoc_cursor_error(cursor, &error)) {
						/* get result from cursor */
						const bson_t *result;
						if (mongoc_cursor_more(cursor) && mongoc_cursor_next(cursor, &result)) {
							char *json_result;
							json_result = bson_as_json(result, NULL);
							stream->write_function(stream, "-OK\n%s\n", json_result);
							bson_free(json_result);
						} else {
							/* empty set */
							stream->write_function(stream, "-OK\n{}\n");
						}
					} else {
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
			connection_done(globals.client_pool, conn);
		} else {
			stream->write_function(stream, "-ERR\nfailed to get connection!\n");
		}
	} else {
		stream->write_function(stream, "-ERR\n%s\n", FIND_ONE_SYNTAX);
	}

	switch_safe_free(db);

	return status;
}

/**
 * Calculate resource count from BSON document
 */
static switch_status_t mod_mongo_get_count(switch_core_session_t *session, const char *key, const bson_t *b, int *new_val_ret, char **resource_ret)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	bson_iter_t iter;
	if (new_val_ret) {
		if (bson_iter_init_find(&iter, b, key) && BSON_ITER_HOLDS_INT32(&iter)) {
			*new_val_ret = bson_iter_int32(&iter);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Failed to get resource count\n");
			status = SWITCH_STATUS_GENERR;
		}
	}
	if (resource_ret) {
		if (bson_iter_init_find(&iter, b, "_id") && BSON_ITER_HOLDS_UTF8(&iter)) {
			uint32_t len;
			const char *resource = bson_iter_utf8(&iter, &len);
			if (!zstr(resource)) {
				if (bson_utf8_validate(resource, len, 0)) {
					*resource_ret = strdup(resource);
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Resource name is not valid utf8\n");
					status = SWITCH_STATUS_GENERR;
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Resource name is empty string");
				status = SWITCH_STATUS_GENERR;
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Resource name not found\n");
			status = SWITCH_STATUS_GENERR;
		}
	}
	return status;
}

/**
 * Increment a resource by val
 * @param session
 * @param resource name of resource being incremented
 * @param val number to increment resource by
 * @param max maximum value of resource allowed
 * @param new_val_ret new value of resource after increment completed
 */
static switch_status_t mod_mongo_increment(switch_core_session_t *session, const char *resource, int val, int max, int *new_val_ret)
{
	switch_status_t status = SWITCH_STATUS_GENERR;
	mongoc_client_t *conn = get_connection(globals.limit_client_pool, globals.limit_conn_str);
	if (conn) {
		mongoc_collection_t *col = mongoc_client_get_collection(conn, globals.limit_database, globals.limit_collection);
		if (col) {
			int upsert;
			bson_t *query, *update, reply;
			bson_error_t error;

			/* construct update query - the counts are stored as:
			{ _id: "realm_resource", total: 29, "fs-1": 5, "fs-2": 10, "fs-3": 3, "fs-4": 11 }
			*/
			if (val >= 0) {
				if (max > 0) {
					/* increment if < max */
					query = BCON_NEW("_id", resource,
						"total", "{", "$lt", BCON_INT32(max), "}");
					upsert = 1; /* will fail with duplicate index key error if total condition is not satisfied */
				} else {
					/* increment, no restrictions */
					query = BCON_NEW("_id", resource);
					upsert = 1;
				}
			} else {
				/* don't allow decrement below 0, don't add fields that don't exist */
				query = BCON_NEW("_id", resource,
					"total", "{", "$gte", BCON_INT32(-val), "}",
					switch_core_get_switchname(), "{", "$gte", BCON_INT32(-val), "}");
				upsert = 0;
			}
			update = BCON_NEW("$inc", "{", "total", BCON_INT32(val), switch_core_get_switchname(), BCON_INT32(val), "}");

			if (!mongoc_collection_find_and_modify(col, query, NULL, update, NULL, false, upsert, true, &reply, &error)) {
				if (max > 0 && error.code == 11000) {
					/* duplicate key index error - limit exceeded  */
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Usage for %s exceeds maximum rate of %d\n", resource, max);
					status = SWITCH_STATUS_FALSE;
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Increment %s by %d failed: %s\n", resource, val, error.message);
					status = SWITCH_STATUS_GENERR;
				}
			} else if (new_val_ret) {
				status = mod_mongo_get_count(session, "total", &reply, new_val_ret, NULL);
			} else {
				status = SWITCH_STATUS_SUCCESS;
			}

			bson_destroy(query);
			bson_destroy(update);
			bson_destroy(&reply);
			mongoc_collection_destroy(col);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Increment %s by %d failed: unable to get collection %s from database %s\n", resource, val, globals.limit_collection, globals.limit_database);
		}
		connection_done(globals.limit_client_pool, conn);
	}
	return status;
}

/**
 * Get resource usage
 */
static switch_status_t mod_mongo_get_usage(const char *resource, int *usage)
{
	switch_status_t status = SWITCH_STATUS_GENERR;
	mongoc_client_t *conn = get_connection(globals.limit_client_pool, globals.limit_conn_str);
	if (conn) {
		mongoc_collection_t *col = mongoc_client_get_collection(conn, globals.limit_database, globals.limit_collection);
		if (col) {
			bson_t *query = BCON_NEW("_id", resource);
			bson_t *fields = BCON_NEW("total", BCON_INT32(1));
			bson_error_t error;
			mongoc_cursor_t *cursor = mongoc_collection_find(col, 0, 0, 1, 0, query, fields, NULL);
			if (cursor) {
				if (!mongoc_cursor_error(cursor, &error)) {
					/* get result from cursor */
					const bson_t *result;
					if (mongoc_cursor_more(cursor) && mongoc_cursor_next(cursor, &result)) {
						status = mod_mongo_get_count(NULL, "total", result, usage, NULL);
					}
				}
				mongoc_cursor_destroy(cursor);
			}
			bson_destroy(query);
			bson_destroy(fields);
			mongoc_collection_destroy(col);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Get usage failed: unable to get collection %s from database %s\n", globals.limit_collection, globals.limit_database);
		}
		connection_done(globals.limit_client_pool, conn);
	}
	return status;
}

/**
 * Clear all limits on this server
 */
static switch_status_t mod_mongo_reset(void)
{
	switch_status_t status = SWITCH_STATUS_GENERR;
	mongoc_client_t *conn = get_connection(globals.limit_client_pool, globals.limit_conn_str);
	if (conn) {
		mongoc_collection_t *col = mongoc_client_get_collection(conn, globals.limit_database, globals.limit_collection);
		if (col) {
			bson_t *query;
			//bson_t *fields;
			mongoc_cursor_t *cursor;
			bson_error_t error;
			query = BCON_NEW(switch_core_get_switchname(), "{", "$gt", BCON_INT32(0), "}");
			//fields = BCON_NEW(switch_core_get_switchname(), "1");

			/* find all docs w/ this server and clear its counts */
			switch_thread_rwlock_wrlock(globals.limit_database_rwlock); /* prevent increments on this server */
			cursor = mongoc_collection_find(col, 0, 0, 0, 0, query, NULL, NULL);
			if (cursor) {
				if (!mongoc_cursor_error(cursor, &error)) {
					/* get result from cursor */
					const bson_t *result;
					char *resource = NULL;
					while (mongoc_cursor_more(cursor) && mongoc_cursor_next(cursor, &result)) {
						int count = 0;
						if ((status = mod_mongo_get_count(NULL, switch_core_get_switchname(), result, &count, &resource)) == SWITCH_STATUS_SUCCESS) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Reset %s, -%d\n", resource, count);
							if (count > 0 && !zstr(resource)) {
								/* decrement server counts from mongo */
								if ((status = mod_mongo_increment(NULL, resource, -count, 0, NULL)) == SWITCH_STATUS_GENERR) {
									switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Reset done - increment error\n");
									break;
								}
							}
							switch_safe_free(resource);
							resource = NULL;
						} else {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Reset failed - get count\n");
							break;
						}
					}
					switch_safe_free(resource);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Reset failed: %s\n", error.message);
				}
				mongoc_cursor_destroy(cursor);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Reset failed: NULL cursor returned\n");
			}
			switch_thread_rwlock_unlock(globals.limit_database_rwlock);

			bson_destroy(query);
			//bson_destroy(fields);
			mongoc_collection_destroy(col);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Reset failed: unable to get collection %s from database %s\n", globals.limit_collection, globals.limit_database);
		}
		connection_done(globals.limit_client_pool, conn);
	}
	return status;
}

/**
 * Clean up all entries w/ resource count of 0
 */
static switch_status_t mod_mongo_cleanup(void)
{
	switch_status_t status = SWITCH_STATUS_GENERR;
	mongoc_client_t *conn = get_connection(globals.limit_client_pool, globals.limit_conn_str);
	if (conn) {
		mongoc_collection_t *col = mongoc_client_get_collection(conn, globals.limit_database, globals.limit_collection);
		if (col) {
			bson_t *selector = BCON_NEW("total", BCON_INT32(0));
			bson_error_t error;
			if (mongoc_collection_remove(col, 0, selector, NULL, &error)) {
				status = SWITCH_STATUS_SUCCESS;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Cleanup failed: %s\n", error.message);
			}
			bson_destroy(selector);
			mongoc_collection_destroy(col);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Cleanup failed: unable to get collection %s from database %s\n", globals.limit_collection, globals.limit_database);
		}
		connection_done(globals.limit_client_pool, conn);
	}
	return status;
}

/**
 * @brief Enforces limit_mongo restrictions
 * @param session current session
 * @param realm limit realm
 * @param id limit id
 * @param max maximum count
 * @param interval interval for rate limiting
 * @return SWITCH_TRUE if the access is allowed, SWITCH_FALSE if it isn't
 */
SWITCH_LIMIT_INCR(mod_mongo_limit_incr)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *limit_id = switch_core_session_sprintf(session, "%s_%s", realm, resource);

	/* get session's resource tracking information */
	struct mod_mongo_private *pvt = switch_channel_get_private(channel, "limit_mongo");
	if (!pvt) {
		switch_mutex_lock(globals.mod_mongo_private_mutex); /* prevents concurrent alloc of mod_mongo_private */
		pvt = switch_channel_get_private(channel, "limit_mongo");
		if (!pvt) {
			pvt = (struct mod_mongo_private *) switch_core_session_alloc(session, sizeof(*pvt));
			switch_core_hash_init(&pvt->resources);
			switch_mutex_init(&pvt->mutex, SWITCH_MUTEX_UNNESTED, switch_core_session_get_pool(session));
			switch_channel_set_private(channel, "limit_mongo", pvt);
		}
		switch_mutex_unlock(globals.mod_mongo_private_mutex);
	}

	switch_mutex_lock(pvt->mutex); /* prevents concurrent increment in session */
	switch_thread_rwlock_rdlock(globals.limit_database_rwlock); /* prevent reset operation on this server */

	/* check if resource is already incremented on this session */
	if (!switch_core_hash_find(pvt->resources, limit_id)) {
		/* increment resource usage */
		if ((status = mod_mongo_increment(session, limit_id, 1, max, NULL)) == SWITCH_STATUS_SUCCESS) {
			/* remember this resource was incremented */
			switch_core_hash_insert(pvt->resources, limit_id, limit_id);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "%s already acquired\n", limit_id);
	}

	switch_thread_rwlock_unlock(globals.limit_database_rwlock);
	switch_mutex_unlock(pvt->mutex);

	return status;
}

/**
 * @brief Releases usage of a limit_mongo-controlled resource
 */
SWITCH_LIMIT_RELEASE(mod_mongo_limit_release)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	struct mod_mongo_private *pvt = switch_channel_get_private(channel, "limit_mongo");
	int status = SWITCH_STATUS_SUCCESS;

	if (!pvt) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "No limit tracking data for channel\n");
		return SWITCH_STATUS_SUCCESS;
	}

	switch_mutex_lock(pvt->mutex); /* prevents concurrent decrement in session */
	switch_thread_rwlock_rdlock(globals.limit_database_rwlock); /* prevent reset operation on this server */

	/* no realm / resource = clear all resources */
	if (realm == NULL && resource == NULL) {
		/* clear all resources */
		switch_hash_index_t *hi = NULL;
		while ((hi = switch_core_hash_first_iter(pvt->resources, hi))) {
			void *p_val = NULL;
			const void *p_key;
			switch_ssize_t keylen;
			switch_core_hash_this(hi, &p_key, &keylen, &p_val);
			if (mod_mongo_increment(session, (const char *)p_key, -1, 0, NULL) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Couldn't decrement %s\n", (const char *)p_key);
				status = SWITCH_STATUS_FALSE;
				break;
			} else {
				switch_core_hash_delete(pvt->resources, (const char *) p_key);
			}
		}
	} else if (!zstr(realm) && !zstr(resource)) {
		/* clear specific resource */
		const char *limit_id = switch_core_session_sprintf(session, "%s_%s", realm, resource);
		if (switch_core_hash_find(pvt->resources, limit_id)) {
			if (mod_mongo_increment(session, limit_id, -1, 0, NULL) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Couldn't decrement %s\n", limit_id);
			} else {
				switch_core_hash_delete(pvt->resources, limit_id);
			}
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Missing either realm or resource to release\n");
	}

	switch_thread_rwlock_unlock(globals.limit_database_rwlock);
	switch_mutex_unlock(pvt->mutex);

	return status;
}

SWITCH_LIMIT_USAGE(mod_mongo_limit_usage)
{
	char *limit_id = switch_mprintf("%s_%s", realm, resource);
	int usage = 0;
	mod_mongo_get_usage(limit_id, &usage);
	switch_safe_free(limit_id);
	return usage;
}

SWITCH_LIMIT_RESET(mod_mongo_limit_reset)
{
	return mod_mongo_reset();
}

SWITCH_LIMIT_STATUS(mod_mongo_limit_status)
{
	return strdup("-ERR not supported");
}

static switch_status_t do_config(switch_memory_pool_t *pool)
{
	const char *cf = "mongo.conf";
	switch_xml_t cfg, xml, settings, param;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	/* set defaults */
	globals.map = "";
	globals.reduce = "";
	globals.finalize = "";
	globals.conn_str = "";
	globals.client_pool = NULL;
	globals.limit_database = "limit";
	globals.limit_collection = "mod_mongo";
	globals.limit_conn_str = "";
	globals.limit_client_pool = NULL;
	globals.limit_cleanup_interval_sec = 300;

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
					mongoc_uri_t *uri;
					globals.conn_str = switch_core_strdup(pool, val);
					uri = mongoc_uri_new(globals.conn_str);
					if (uri) {
						globals.client_pool = mongoc_client_pool_new(uri);
						mongoc_uri_destroy(uri);
						if (!globals.client_pool) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to pool for connection-string: %s\n", globals.conn_str);
							status = SWITCH_STATUS_GENERR;
							goto done;
						}
						if (!globals.limit_client_pool) {
							/* use connection-string for limit backend unless overriden by limit-connection-string */
							globals.limit_client_pool = globals.client_pool;
							globals.limit_conn_str = globals.conn_str;
						}
					} else {
						mongoc_uri_destroy(uri);
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid connection-string: %s\n", globals.conn_str);
						status = SWITCH_STATUS_GENERR;
						goto done;
					}
				}
			} else if (!strcmp(var, "limit-connection-string")) {
				if (zstr(val)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "missing limit-connection-string - using connection-string instead\n");
					continue;
				} else {
					mongoc_uri_t *uri;
					globals.limit_conn_str = switch_core_strdup(pool, val);
					uri = mongoc_uri_new(globals.limit_conn_str);
					if (uri) {
						globals.limit_client_pool = mongoc_client_pool_new(uri);
						mongoc_uri_destroy(uri);
						if (!globals.limit_client_pool) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to pool for limit-connection-string: %s\n", globals.limit_conn_str);
							status = SWITCH_STATUS_GENERR;
							goto done;
						}
					} else {
						mongoc_uri_destroy(uri);
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid limit-connection-string: %s\n", globals.limit_conn_str);
						status = SWITCH_STATUS_GENERR;
						goto done;
					}
				}
			} else if (!strcmp(var, "limit-database")) {
				if (zstr(val)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "missing limit-database - using '%s'\n", globals.limit_database);
				} else {
					globals.limit_database = switch_core_strdup(pool, val);
				}
			} else if (!strcmp(var, "limit-collection")) {
				if (zstr(val)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "missing limit-collection - using '%s'\n", globals.limit_collection);
				} else {
					globals.limit_collection = switch_core_strdup(pool, val);
				}
			} else if (!strcmp(var, "limit-cleanup-interval-sec")) {
				if (zstr(val) || !switch_is_number(val)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "bad value of limit-cleanup-interval-sec\n");
				} else {
					int new_interval = atoi(val);
					if (new_interval >= 0) {
						globals.limit_cleanup_interval_sec = new_interval;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "limit-cleanup-interval-sec must be >= 0\n");
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
	switch_api_interface_t *api_interface = NULL;
	switch_limit_interface_t *limit_interface = NULL;

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	memset(&globals, 0, sizeof(globals));

	if (do_config(pool) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_TERM;
	}

	switch_mutex_init(&globals.mod_mongo_private_mutex, SWITCH_MUTEX_UNNESTED, pool);
	switch_thread_rwlock_create(&globals.limit_database_rwlock, pool);
	switch_thread_rwlock_create(&globals.shutdown_rwlock, pool);

	/* clear all entries */
	mod_mongo_reset();

	SWITCH_ADD_API(api_interface, "mongo_find_one", "findOne", mod_mongo_find_one_function, FIND_ONE_SYNTAX);
	SWITCH_ADD_API(api_interface, "mongo_find_n", "find", mod_mongo_find_n_function, FIND_N_SYNTAX);
	SWITCH_ADD_API(api_interface, "mongo_mapreduce", "Map/Reduce", mod_mongo_mapreduce_function, MAPREDUCE_SYNTAX);

	SWITCH_ADD_LIMIT(limit_interface, "mongo", mod_mongo_limit_incr, mod_mongo_limit_release, mod_mongo_limit_usage, mod_mongo_limit_reset, mod_mongo_limit_status, NULL);

	return SWITCH_STATUS_SUCCESS;
}

/**
 * Periodically cleanup mongo limit counters
 */
SWITCH_MODULE_RUNTIME_FUNCTION(mod_mongo_runtime)
{
	switch_interval_time_t cleanup_time = switch_micro_time_now() + (globals.limit_cleanup_interval_sec * 1000 * 1000);
	switch_thread_rwlock_rdlock(globals.shutdown_rwlock);
	while(!globals.shutdown && globals.limit_cleanup_interval_sec) {
		switch_micro_sleep(1 * 1000 * 1000);
		if (!globals.shutdown && switch_micro_time_now() > cleanup_time) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Cleanup\n");
			mod_mongo_cleanup();
			cleanup_time = switch_micro_time_now() + (globals.limit_cleanup_interval_sec * 1000 * 1000);
		}
	}
	switch_thread_rwlock_unlock(globals.shutdown_rwlock);
	return SWITCH_STATUS_TERM;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_mongo_shutdown)
{
	globals.shutdown = 1;
	switch_thread_rwlock_wrlock(globals.shutdown_rwlock);
	switch_thread_rwlock_unlock(globals.shutdown_rwlock);
	if (globals.limit_client_pool && globals.limit_client_pool != globals.client_pool) {
		mongoc_client_pool_destroy(globals.limit_client_pool);
		globals.limit_client_pool = NULL;
	}
	if (globals.client_pool) {
		mongoc_client_pool_destroy(globals.client_pool);
		globals.client_pool = NULL;
	}
	if (globals.mod_mongo_private_mutex) {
		switch_mutex_destroy(globals.mod_mongo_private_mutex);
		globals.mod_mongo_private_mutex = NULL;
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

