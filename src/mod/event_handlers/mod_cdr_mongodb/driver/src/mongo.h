/**
 * @file mongo.h
 * @brief Main MongoDB Declarations
 */

/*    Copyright 2009-2011 10gen Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef _MONGO_H_
#define _MONGO_H_

#include "bson.h"

MONGO_EXTERN_C_START

#define MONGO_MAJOR 0
#define MONGO_MINOR 4
#define MONGO_PATCH 0

#define MONGO_OK 0
#define MONGO_ERROR -1

#define MONGO_DEFAULT_PORT 27017

typedef enum mongo_error_t {
    MONGO_CONN_SUCCESS = 0,  /**< Connection success! */
    MONGO_CONN_NO_SOCKET,    /**< Could not create a socket. */
    MONGO_CONN_FAIL,         /**< An error occured while calling connect(). */
    MONGO_CONN_ADDR_FAIL,    /**< An error occured while calling getaddrinfo(). */
    MONGO_CONN_NOT_MASTER,   /**< Warning: connected to a non-master node (read-only). */
    MONGO_CONN_BAD_SET_NAME, /**< Given rs name doesn't match this replica set. */
    MONGO_CONN_NO_PRIMARY,   /**< Can't find primary in replica set. Connection closed. */

    MONGO_IO_ERROR,          /**< An error occurred while reading or writing on socket. */
    MONGO_READ_SIZE_ERROR,   /**< The response is not the expected length. */
    MONGO_COMMAND_FAILED,    /**< The command returned with 'ok' value of 0. */
    MONGO_CURSOR_EXHAUSTED,  /**< The cursor has no more results. */
    MONGO_CURSOR_INVALID,    /**< The cursor has timed out or is not recognized. */
    MONGO_CURSOR_PENDING,    /**< Tailable cursor still alive but no data. */
    MONGO_BSON_INVALID,      /**< BSON not valid for the specified op. */
    MONGO_BSON_NOT_FINISHED  /**< BSON object has not been finished. */
} mongo_error_t;

enum mongo_cursor_flags {
    MONGO_CURSOR_MUST_FREE = 1,      /**< mongo_cursor_destroy should free cursor. */
    MONGO_CURSOR_QUERY_SENT = ( 1<<1 ) /**< Initial query has been sent. */
};

enum mongo_index_opts {
    MONGO_INDEX_UNIQUE = ( 1<<0 ),
    MONGO_INDEX_DROP_DUPS = ( 1<<2 ),
    MONGO_INDEX_BACKGROUND = ( 1<<3 ),
    MONGO_INDEX_SPARSE = ( 1<<4 )
};

enum mongo_update_opts {
    MONGO_UPDATE_UPSERT = 0x1,
    MONGO_UPDATE_MULTI = 0x2,
    MONGO_UPDATE_BASIC = 0x4
};

enum mongo_cursor_opts {
    MONGO_TAILABLE = ( 1<<1 ),        /**< Create a tailable cursor. */
    MONGO_SLAVE_OK = ( 1<<2 ),        /**< Allow queries on a non-primary node. */
    MONGO_NO_CURSOR_TIMEOUT = ( 1<<4 ), /**< Disable cursor timeouts. */
    MONGO_AWAIT_DATA = ( 1<<5 ),      /**< Momentarily block for more data. */
    MONGO_EXHAUST = ( 1<<6 ),         /**< Stream in multiple 'more' packages. */
    MONGO_PARTIAL = ( 1<<7 )          /**< Allow reads even if a shard is down. */
};

enum mongo_operations {
    MONGO_OP_MSG = 1000,
    MONGO_OP_UPDATE = 2001,
    MONGO_OP_INSERT = 2002,
    MONGO_OP_QUERY = 2004,
    MONGO_OP_GET_MORE = 2005,
    MONGO_OP_DELETE = 2006,
    MONGO_OP_KILL_CURSORS = 2007
};

#pragma pack(1)
typedef struct {
    int len;
    int id;
    int responseTo;
    int op;
} mongo_header;

typedef struct {
    mongo_header head;
    char data;
} mongo_message;

typedef struct {
    int flag; /* FIX THIS COMMENT non-zero on failure */
    int64_t cursorID;
    int start;
    int num;
} mongo_reply_fields;

typedef struct {
    mongo_header head;
    mongo_reply_fields fields;
    char objs;
} mongo_reply;
#pragma pack()

typedef struct mongo_host_port {
    char host[255];
    int port;
    struct mongo_host_port *next;
} mongo_host_port;

typedef struct {
    mongo_host_port *seeds;        /**< List of seeds provided by the user. */
    mongo_host_port *hosts;        /**< List of host/ports given by the replica set */
    char *name;                    /**< Name of the replica set. */
    bson_bool_t primary_connected; /**< Primary node connection status. */
} mongo_replset;

typedef struct mongo {
    mongo_host_port *primary;  /**< Primary connection info. */
    mongo_replset *replset;    /**< replset object if connected to a replica set. */
    int sock;                  /**< Socket file descriptor. */
    int flags;                 /**< Flags on this connection object. */
    int conn_timeout_ms;       /**< Connection timeout in milliseconds. */
    int op_timeout_ms;         /**< Read and write timeout in milliseconds. */
    bson_bool_t connected;     /**< Connection status. */

    mongo_error_t err;         /**< Most recent driver error code. */
    char *errstr;              /**< String version of most recent driver error code. */
    int lasterrcode;           /**< getlasterror given by the server on calls. */
    char *lasterrstr;          /**< getlasterror string generated by server. */
} mongo;

typedef struct {
    mongo_reply *reply;  /**< reply is owned by cursor */
    mongo *conn;       /**< connection is *not* owned by cursor */
    const char *ns;    /**< owned by cursor */
    int flags;         /**< Flags used internally by this drivers. */
    int seen;          /**< Number returned so far. */
    bson current;      /**< This cursor's current bson object. */
    mongo_error_t err; /**< Errors on this cursor. */
    bson *query;       /**< Bitfield containing cursor options. */
    bson *fields;      /**< Bitfield containing cursor options. */
    int options;       /**< Bitfield containing cursor options. */
    int limit;         /**< Bitfield containing cursor options. */
    int skip;          /**< Bitfield containing cursor options. */
} mongo_cursor;

/* Connection API */

/** Initialize a new mongo connection object. If not created
 *  with mongo_new, you must initialize each mongo
 *  object using this function.
 *
 *  @note When finished, you must pass this object to
 *      mongo_destroy( ).
 *
 *  @param conn a mongo connection object allocated on the stack
 *      or heap.
 */
void mongo_init( mongo *conn );

/**
 * Connect to a single MongoDB server.
 *
 * @param conn a mongo object.
 * @param host a numerical network address or a network hostname.
 * @param port the port to connect to.
 *
 * @return MONGO_OK or MONGO_ERROR on failure. On failure, a constant of type
 *   mongo_conn_return_t will be set on the conn->err field.
 */
int mongo_connect( mongo *conn , const char *host, int port );

/**
 * Set up this connection object for connecting to a replica set.
 * To connect, pass the object to mongo_replset_connect().
 *
 * @param conn a mongo object.
 * @param name the name of the replica set to connect to.
 * */
void mongo_replset_init( mongo *conn, const char *name );

/**
 * Add a seed node to the replica set connection object.
 *
 * You must specify at least one seed node before connecting to a replica set.
 *
 * @param conn a mongo object.
 * @param host a numerical network address or a network hostname.
 * @param port the port to connect to.
 */
void mongo_replset_add_seed( mongo *conn, const char *host, int port );

/**
 * Utility function for converting a host-port string to a mongo_host_port.
 *
 * @param host_string a string containing either a host or a host and port separated
 *     by a colon.
 * @param host_port the mongo_host_port object to write the result to.
 */
void mongo_parse_host( const char *host_string, mongo_host_port *host_port );

/**
 * Connect to a replica set.
 *
 * Before passing a connection object to this function, you must already have called
 * mongo_set_replset and mongo_replset_add_seed.
 *
 * @param conn a mongo object.
 *
 * @return MONGO_OK or MONGO_ERROR on failure. On failure, a constant of type
 *   mongo_conn_return_t will be set on the conn->err field.
 */
int mongo_replset_connect( mongo *conn );

/** Set a timeout for operations on this connection. This
 *  is a platform-specific feature, and only work on *nix
 *  system. You must also compile for linux to support this.
 *
 *  @param conn a mongo object.
 *  @param millis timeout time in milliseconds.
 *
 *  @return MONGO_OK. On error, return MONGO_ERROR and
 *    set the conn->err field.
 */
int mongo_set_op_timeout( mongo *conn, int millis );

/**
 * Ensure that this connection is healthy by performing
 * a round-trip to the server.
 *
 * @param conn a mongo connection
 *
 * @return MONGO_OK if connected; otherwise, MONGO_ERROR.
 */
int mongo_check_connection( mongo *conn );

/**
 * Try reconnecting to the server using the existing connection settings.
 *
 * This function will disconnect the current socket. If you've authenticated,
 * you'll need to re-authenticate after calling this function.
 *
 * @param conn a mongo object.
 *
 * @return MONGO_OK or MONGO_ERROR and
 *   set the conn->err field.
 */
int mongo_reconnect( mongo *conn );

/**
 * Close the current connection to the server. After calling
 * this function, you may call mongo_reconnect with the same
 * connection object.
 *
 * @param conn a mongo object.
 */
void mongo_disconnect( mongo *conn );

/**
 * Close any existing connection to the server and free all allocated
 * memory associated with the conn object.
 *
 * You must always call this function when finished with the connection object.
 *
 * @param conn a mongo object.
 */
void mongo_destroy( mongo *conn );

/**
 * Insert a BSON document into a MongoDB server. This function
 * will fail if the supplied BSON struct is not UTF-8 or if
 * the keys are invalid for insert (contain '.' or start with '$').
 *
 * @param conn a mongo object.
 * @param ns the namespace.
 * @param data the bson data.
 *
 * @return MONGO_OK or MONGO_ERROR. If the conn->err
 *     field is MONGO_BSON_INVALID, check the err field
 *     on the bson struct for the reason.
 */
int mongo_insert( mongo *conn, const char *ns, bson *data );

/**
 * Insert a batch of BSON documents into a MongoDB server. This function
 * will fail if any of the documents to be inserted is invalid.
 *
 * @param conn a mongo object.
 * @param ns the namespace.
 * @param data the bson data.
 * @param num the number of documents in data.
 *
 * @return MONGO_OK or MONGO_ERROR.
 *
 */
int mongo_insert_batch( mongo *conn , const char *ns ,
                        bson **data , int num );

/**
 * Update a document in a MongoDB server.
 *
 * @param conn a mongo object.
 * @param ns the namespace.
 * @param cond the bson update query.
 * @param op the bson update data.
 * @param flags flags for the update.
 *
 * @return MONGO_OK or MONGO_ERROR with error stored in conn object.
 *
 */
int mongo_update( mongo *conn, const char *ns, const bson *cond,
                  const bson *op, int flags );

/**
 * Remove a document from a MongoDB server.
 *
 * @param conn a mongo object.
 * @param ns the namespace.
 * @param cond the bson query.
 *
 * @return MONGO_OK or MONGO_ERROR with error stored in conn object.
 */
int mongo_remove( mongo *conn, const char *ns, const bson *cond );

/**
 * Find documents in a MongoDB server.
 *
 * @param conn a mongo object.
 * @param ns the namespace.
 * @param query the bson query.
 * @param fields a bson document of fields to be returned.
 * @param limit the maximum number of documents to retrun.
 * @param skip the number of documents to skip.
 * @param options A bitfield containing cursor options.
 *
 * @return A cursor object allocated on the heap or NULL if
 *     an error has occurred. For finer-grained error checking,
 *     use the cursor builder API instead.
 */
mongo_cursor *mongo_find( mongo *conn, const char *ns, bson *query,
                          bson *fields, int limit, int skip, int options );

/**
 * Initalize a new cursor object.
 *
 * @param cursor
 * @param ns the namespace, represented as the the database
 *     name and collection name separated by a dot. e.g., "test.users"
 */
void mongo_cursor_init( mongo_cursor *cursor, mongo *conn, const char *ns );

/**
 * Set the bson object specifying this cursor's query spec. If
 * your query is the empty bson object "{}", then you need not
 * set this value.
 *
 * @param cursor
 * @param query a bson object representing the query spec. This may
 *   be either a simple query spec or a complex spec storing values for
 *   $query, $orderby, $hint, and/or $explain. See
 *   http://www.mongodb.org/display/DOCS/Mongo+Wire+Protocol for details.
 */
void mongo_cursor_set_query( mongo_cursor *cursor, bson *query );

/**
 * Set the fields to return for this cursor. If you want to return
 * all fields, you need not set this value.
 *
 * @param cursor
 * @param fields a bson object representing the fields to return.
 *   See http://www.mongodb.org/display/DOCS/Retrieving+a+Subset+of+Fields.
 */
void mongo_cursor_set_fields( mongo_cursor *cursor, bson *fields );

/**
 * Set the number of documents to skip.
 *
 * @param cursor
 * @param skip
 */
void mongo_cursor_set_skip( mongo_cursor *cursor, int skip );

/**
 * Set the number of documents to return.
 *
 * @param cursor
 * @param limit
 */
void mongo_cursor_set_limit( mongo_cursor *cursor, int limit );

/**
 * Set any of the available query options (e.g., MONGO_TAILABLE).
 *
 * @param cursor
 * @param options a bitfield storing query options. See
 *   mongo_cursor_bitfield_t for available constants.
 */
void mongo_cursor_set_options( mongo_cursor *cursor, int options );

/**
 * Return the current BSON object data as a const char*. This is useful
 * for creating bson iterators with bson_iterator_init.
 *
 * @param cursor
 */
const char *mongo_cursor_data( mongo_cursor *cursor );

/**
 * Return the current BSON object data as a const char*. This is useful
 * for creating bson iterators with bson_iterator_init.
 *
 * @param cursor
 */
const bson *mongo_cursor_bson( mongo_cursor *cursor );

/**
 * Iterate the cursor, returning the next item. When successful,
 *   the returned object will be stored in cursor->current;
 *
 * @param cursor
 *
 * @return MONGO_OK. On error, returns MONGO_ERROR and sets
 *   cursor->err with a value of mongo_error_t.
 */
int mongo_cursor_next( mongo_cursor *cursor );

/**
 * Destroy a cursor object. When finished with a cursor, you
 * must pass it to this function.
 *
 * @param cursor the cursor to destroy.
 *
 * @return MONGO_OK or an error code. On error, check cursor->conn->err
 *     for errors.
 */
int mongo_cursor_destroy( mongo_cursor *cursor );

/**
 * Find a single document in a MongoDB server.
 *
 * @param conn a mongo object.
 * @param ns the namespace.
 * @param query the bson query.
 * @param fields a bson document of the fields to be returned.
 * @param out a bson document in which to put the query result.
 *
 */
/* out can be NULL if you don't care about results. useful for commands */
bson_bool_t mongo_find_one( mongo *conn, const char *ns, bson *query,
                            bson *fields, bson *out );

/* MongoDB Helper Functions */

/**
 * Count the number of documents in a collection matching a query.
 *
 * @param conn a mongo object.
 * @param db the db name.
 * @param coll the collection name.
 * @param query the BSON query.
 *
 * @return the number of matching documents. If the command fails,
 *     MONGO_ERROR is returned.
 */
int64_t mongo_count( mongo *conn, const char *db, const char *coll,
                     bson *query );

/**
 * Create a compouned index.
 *
 * @param conn a mongo object.
 * @param ns the namespace.
 * @param data the bson index data.
 * @param options a bitfield for setting index options. Possibilities include
 *   MONGO_INDEX_UNIQUE, MONGO_INDEX_DROP_DUPS, MONGO_INDEX_BACKGROUND,
 *   and MONGO_INDEX_SPARSE.
 * @param out a bson document containing errors, if any.
 *
 * @return MONGO_OK if index is created successfully; otherwise, MONGO_ERROR.
 */
int mongo_create_index( mongo *conn, const char *ns, bson *key, int options, bson *out );

/**
 * Create an index with a single key.
 *
 * @param conn a mongo object.
 * @param ns the namespace.
 * @param field the index key.
 * @param options index options.
 * @param out a BSON document containing errors, if any.
 *
 * @return true if the index was created.
 */
bson_bool_t mongo_create_simple_index( mongo *conn, const char *ns, const char *field, int options, bson *out );

/* ----------------------------
   COMMANDS
   ------------------------------ */

/**
 * Run a command on a MongoDB server.
 *
 * @param conn a mongo object.
 * @param db the name of the database.
 * @param command the BSON command to run.
 * @param out the BSON result of the command.
 *
 * @return true if the command ran without error.
 */
bson_bool_t mongo_run_command( mongo *conn, const char *db, bson *command, bson *out );

/**
 * Run a command that accepts a simple string key and integer value.
 *
 * @param conn a mongo object.
 * @param db the name of the database.
 * @param cmd the command to run.
 * @param arg the integer argument to the command.
 * @param out the BSON result of the command.
 *
 * @return MONGO_OK or an error code.
 *
 */
int mongo_simple_int_command( mongo *conn, const char *db,
                              const char *cmd, int arg, bson *out );

/**
 * Run a command that accepts a simple string key and value.
 *
 * @param conn a mongo object.
 * @param db the name of the database.
 * @param cmd the command to run.
 * @param arg the string argument to the command.
 * @param out the BSON result of the command.
 *
 * @return true if the command ran without error.
 *
 */
bson_bool_t mongo_simple_str_command( mongo *conn, const char *db, const char *cmd, const char *arg, bson *out );

/**
 * Drop a database.
 *
 * @param conn a mongo object.
 * @param db the name of the database to drop.
 *
 * @return MONGO_OK or an error code.
 */
int mongo_cmd_drop_db( mongo *conn, const char *db );

/**
 * Drop a collection.
 *
 * @param conn a mongo object.
 * @param db the name of the database.
 * @param collection the name of the collection to drop.
 * @param out a BSON document containing the result of the command.
 *
 * @return true if the collection drop was successful.
 */
bson_bool_t mongo_cmd_drop_collection( mongo *conn, const char *db, const char *collection, bson *out );

/**
 * Add a database user.
 *
 * @param conn a mongo object.
 * @param db the database in which to add the user.
 * @param user the user name
 * @param pass the user password
 *
 * @return MONGO_OK or MONGO_ERROR.
  */
int mongo_cmd_add_user( mongo *conn, const char *db, const char *user, const char *pass );

/**
 * Authenticate a user.
 *
 * @param conn a mongo object.
 * @param db the database to authenticate against.
 * @param user the user name to authenticate.
 * @param pass the user's password.
 *
 * @return MONGO_OK on sucess and MONGO_ERROR on failure.
 */
int mongo_cmd_authenticate( mongo *conn, const char *db, const char *user, const char *pass );

/**
 * Check if the current server is a master.
 *
 * @param conn a mongo object.
 * @param out a BSON result of the command.
 *
 * @return true if the server is a master.
 */
/* return value is master status */
bson_bool_t mongo_cmd_ismaster( mongo *conn, bson *out );

/**
 * Get the error for the last command with the current connection.
 *
 * @param conn a mongo object.
 * @param db the name of the database.
 * @param out a BSON object containing the error details.
 *
 * @return MONGO_OK if no error and MONGO_ERROR on error. On error, check the values
 *     of conn->lasterrcode and conn->lasterrstr for the error status.
 */
int mongo_cmd_get_last_error( mongo *conn, const char *db, bson *out );

/**
 * Get the most recent error with the current connection.
 *
 * @param conn a mongo object.
 * @param db the name of the database.
 * @param out a BSON object containing the error details.
 *
 * @return MONGO_OK if no error and MONGO_ERROR on error. On error, check the values
 *     of conn->lasterrcode and conn->lasterrstr for the error status.
 */
int mongo_cmd_get_prev_error( mongo *conn, const char *db, bson *out );

/**
 * Reset the error state for the connection.
 *
 * @param conn a mongo object.
 * @param db the name of the database.
 */
void mongo_cmd_reset_error( mongo *conn, const char *db );

MONGO_EXTERN_C_END

#endif
