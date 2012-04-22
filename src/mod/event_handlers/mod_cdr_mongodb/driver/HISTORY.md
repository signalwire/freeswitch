# MongoDB C Driver History

## 0.4

THIS RELEASE INCLUDES NUMEROUS BACKWARD-BREAKING CHANGES.
These changes have been made for extensibility, consistency,
and ease of use. Please read the following release notes
carefully, and study the updated tutorial.

API Principles:

1. Present a consistent interface for all objects: connections,
   cursors, bson objects, and bson iterators.
2. Require no knowledge of an object's implementation to use the API.
3. Allow users to allocate objects on the stack or on the heap.
4. Integrate API with new error reporting strategy.
5. Be concise, except where it impairs clarity.

Changes:

* mongo_replset_init_conn has been renamed to mongo_replset_init.
* bson_buffer has been removed. All functions for building bson
  objects now take objects of type bson. The new pattern looks like this:

  Example:

    bson b[1];
    bson_init( b );
    bson_append_int( b, "foo", 1 );
    bson_finish( b );
    /* The object is ready to use.
       When finished, destroy it. */
    bson_destroy( b );

* mongo_connection has been renamed to mongo.

  Example:

    mongo conn[1];
    mongo_connect( conn, '127.0.0.1', 27017 );
    /* Connection is ready. Destroy when down. */
    mongo_destroy( conn );

* New cursor builder API for clearer code:

  Example:

    mongo_cursor cursor[1];
    mongo_cursor_init( cursor, conn, "test.foo" );

    bson query[1];

    bson_init( query );
    bson_append_int( query, "bar", 1 );
    bson_finish( query );

    bson fields[1];

    bson_init( fields );
    bson_append_int( fields, "baz", 1 );
    bson_finish( fields );

    mongo_cursor_set_query( cursor, query );
    mongo_cursor_set_fields( cursor, fields );
    mongo_cursor_set_limit( cursor, 10 );
    mongo_cursor_set_skip( cursor, 10 );

    while( mongo_cursor_next( cursor ) == MONGO_OK )
        bson_print( mongo_cursor_bson( cursor ) );

* bson_iterator_init now takes a (bson*) instead of a (const char*). This is consistent
  with bson_find, which also takes a (bson*). If you want to initiate a bson iterator
  with a buffer, use the new function bson_iterator_from_buffer.
* With the addition of the mongo_cursor_bson function, it's now no
  longer necessary to know how bson and mongo_cursor objects are implemented.

  Example:

    bson b[1];
    bson_iterator i[1];

    bson_iterator_init( i, b );

    /* With a cursor */
    bson_iterator_init( i, mongo_cursor_bson( cursor ) );

* Added mongo_cursor_data and bson_data functions, which return the
  raw bson buffer as a (const char *).
* All constants that were once lower case are now
  upper case. These include: MONGO_OP_MSG, MONGO_OP_UPDATE, MONGO_OP_INSERT,
  MONGO_OP_QUERY, MONGO_OP_GET_MORE, MONGO_OP_DELETE, MONGO_OP_KILL_CURSORS
  BSON_EOO, BSON_DOUBLE, BSON_STRING, BSON_OBJECT, BSON_ARRAY, BSON_BINDATA,
  BSON_UNDEFINED, BSON_OID, BSON_BOOL, BSON_DATE, BSON_NULL, BSON_REGEX, BSON_DBREF,
  BSON_CODE, BSON_SYMBOL, BSON_CODEWSCOPE, BSON_INT, BSON_TIMESTAMP, BSON_LONG,
  MONGO_CONN_SUCCESS, MONGO_CONN_BAD_ARG, MONGO_CONN_NO_SOCKET, MONGO_CONN_FAIL,
  MONGO_CONN_NOT_MASTER, MONGO_CONN_BAD_SET_NAME, MONGO_CONN_CANNOT_FIND_PRIMARY 
  If your programs use any of these constants, you must convert them to their
  upper case forms, or you will see compile errors.
* The error handling strategy has been changed. Exceptions are not longer being used.
* Functions taking a mongo_connection object now return either MONGO_OK or MONGO_ERROR.
  In case of an error, an error code of type mongo_error_t will be indicated on the
  mongo_connection->err field.
* Functions taking a bson object now return either BSON_OK or BSON_ERROR.
  In case of an error, an error code of type bson_validity_t will be indicated on the
  bson->err or bson_buffer->err field.
* Calls to mongo_cmd_get_last_error store the error status on the
  mongo->lasterrcode and mongo->lasterrstr fields.
* bson_print now prints all types.
* Users may now set custom malloc, realloc, free, printf, sprintf, and fprintf fields.
* Groundwork for modules for supporting platform-specific features (e.g., socket timeouts).
* Added mongo_set_op_timeout for setting socket timeout. To take advantage of this, you must
  compile with --use-platform=LINUX. The compiles with platform/linux/net.h instead of the
  top-level net.h.
* Fixed tailable cursors.
* GridFS API is now in-line with the new driver API. In particular, all of the
  following functions now return MONGO_OK or MONGO_ERROR: gridfs_init,
  gridfile_init, gridfile_writer_done, gridfs_store_buffer, gridfs_store_file,
  and gridfs_find_query.
* Fixed a few memory leaks.

## 0.3
2011-4-14

* Support replica sets.
* Better standard connection API.
* GridFS write buffers iteratively.
* Fixes for working with large GridFS files (> 3GB)
* bson_append_string_n and family (Gergely Nagy)

## 0.2
2011-2-11

* GridFS support (Chris Triolo).
* BSON Timestamp type support.

## 0.1
2009-11-30

* Initial release.
