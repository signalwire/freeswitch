/* gridfs.c */

/*    Copyright 2009-2012 10gen Inc.
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

#include "gridfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

MONGO_EXPORT gridfs* gridfs_create() {
    return (gridfs*)bson_malloc(sizeof(gridfs));
}

MONGO_EXPORT void gridfs_dispose(gridfs* gfs) {
    free(gfs);
}

MONGO_EXPORT gridfile* gridfile_create() {
    return (gridfile*)bson_malloc(sizeof(gridfile));
}

MONGO_EXPORT void gridfile_dispose(gridfile* gf) {
    free(gf);
}

MONGO_EXPORT void gridfile_get_descriptor(gridfile* gf, bson* out) {
    *out = *gf->meta;
}


static bson *chunk_new( bson_oid_t id, int chunkNumber,
                        const char *data, int len ) {
    bson *b = bson_malloc( sizeof( bson ) );

    bson_init( b );
    bson_append_oid( b, "files_id", &id );
    bson_append_int( b, "n", chunkNumber );
    bson_append_binary( b, "data", BSON_BIN_BINARY, data, len );
    bson_finish( b );
    return  b;
}

static void chunk_free( bson *oChunk ) {
    bson_destroy( oChunk );
    bson_free( oChunk );
}

int gridfs_init( mongo *client, const char *dbname, const char *prefix,
    gridfs *gfs ) {

    int options;
    bson b;
    bson_bool_t success;

    gfs->client = client;

    /* Allocate space to own the dbname */
    gfs->dbname = ( const char * )bson_malloc( strlen( dbname )+1 );
    strcpy( ( char * )gfs->dbname, dbname );

    /* Allocate space to own the prefix */
    if ( prefix == NULL ) prefix = "fs";
    gfs->prefix = ( const char * )bson_malloc( strlen( prefix )+1 );
    strcpy( ( char * )gfs->prefix, prefix );

    /* Allocate space to own files_ns */
    gfs->files_ns =
        ( const char * ) bson_malloc ( strlen( prefix )+strlen( dbname )+strlen( ".files" )+2 );
    strcpy( ( char * )gfs->files_ns, dbname );
    strcat( ( char * )gfs->files_ns, "." );
    strcat( ( char * )gfs->files_ns, prefix );
    strcat( ( char * )gfs->files_ns, ".files" );

    /* Allocate space to own chunks_ns */
    gfs->chunks_ns = ( const char * ) bson_malloc( strlen( prefix ) + strlen( dbname )
                     + strlen( ".chunks" ) + 2 );
    strcpy( ( char * )gfs->chunks_ns, dbname );
    strcat( ( char * )gfs->chunks_ns, "." );
    strcat( ( char * )gfs->chunks_ns, prefix );
    strcat( ( char * )gfs->chunks_ns, ".chunks" );

    bson_init( &b );
    bson_append_int( &b, "filename", 1 );
    bson_finish( &b );
    options = 0;
    success = ( mongo_create_index( gfs->client, gfs->files_ns, &b, options, NULL ) == MONGO_OK );
    bson_destroy( &b );
    if ( !success ) {
        bson_free( ( char * )gfs->dbname );
        bson_free( ( char * )gfs->prefix );
        bson_free( ( char * )gfs->files_ns );
        bson_free( ( char * )gfs->chunks_ns );
        return MONGO_ERROR;
    }

    bson_init( &b );
    bson_append_int( &b, "files_id", 1 );
    bson_append_int( &b, "n", 1 );
    bson_finish( &b );
    options = MONGO_INDEX_UNIQUE;
    success = ( mongo_create_index( gfs->client, gfs->chunks_ns, &b, options, NULL ) == MONGO_OK );
    bson_destroy( &b );
    if ( !success ) {
        bson_free( ( char * )gfs->dbname );
        bson_free( ( char * )gfs->prefix );
        bson_free( ( char * )gfs->files_ns );
        bson_free( ( char * )gfs->chunks_ns );
        return MONGO_ERROR;
    }

    return MONGO_OK;
}

MONGO_EXPORT void gridfs_destroy( gridfs *gfs ) {
    if ( gfs == NULL ) return;
    if ( gfs->dbname ) bson_free( ( char * )gfs->dbname );
    if ( gfs->prefix ) bson_free( ( char * )gfs->prefix );
    if ( gfs->files_ns ) bson_free( ( char * )gfs->files_ns );
    if ( gfs->chunks_ns ) bson_free( ( char * )gfs->chunks_ns );
}

static int gridfs_insert_file( gridfs *gfs, const char *name,
                                const bson_oid_t id, gridfs_offset length,
                                const char *contenttype ) {
    bson command;
    bson ret;
    bson res;
    bson_iterator it;
    int result;
    int64_t d;

    /* Check run md5 */
    bson_init( &command );
    bson_append_oid( &command, "filemd5", &id );
    bson_append_string( &command, "root", gfs->prefix );
    bson_finish( &command );
    result = mongo_run_command( gfs->client, gfs->dbname, &command, &res );
    bson_destroy( &command );
    if (result != MONGO_OK)
        return result;

    /* Create and insert BSON for file metadata */
    bson_init( &ret );
    bson_append_oid( &ret, "_id", &id );
    if ( name != NULL && *name != '\0' ) {
        bson_append_string( &ret, "filename", name );
    }
    bson_append_long( &ret, "length", length );
    bson_append_int( &ret, "chunkSize", DEFAULT_CHUNK_SIZE );
    d = ( bson_date_t )1000*time( NULL );
    bson_append_date( &ret, "uploadDate", d);
    bson_find( &it, &res, "md5" );
    bson_append_string( &ret, "md5", bson_iterator_string( &it ) );
    bson_destroy( &res );
    if ( contenttype != NULL && *contenttype != '\0' ) {
        bson_append_string( &ret, "contentType", contenttype );
    }
    bson_finish( &ret );
    result = mongo_insert( gfs->client, gfs->files_ns, &ret, NULL );
    bson_destroy( &ret );

    return result;
}

MONGO_EXPORT int gridfs_store_buffer( gridfs *gfs, const char *data,
                          gridfs_offset length, const char *remotename,
                          const char *contenttype ) {

    char const *end = data + length;
    const char *data_ptr = data;
    bson_oid_t id;
    int chunkNumber = 0;
    int chunkLen;
    bson *oChunk;

    /* Large files Assertion */
    /* assert( length <= 0xffffffff ); */

    /* Generate and append an oid*/
    bson_oid_gen( &id );

    /* Insert the file's data chunk by chunk */
    while ( data_ptr < end ) {
        chunkLen = DEFAULT_CHUNK_SIZE < ( unsigned int )( end - data_ptr ) ?
                   DEFAULT_CHUNK_SIZE : ( unsigned int )( end - data_ptr );
        oChunk = chunk_new( id, chunkNumber, data_ptr, chunkLen );
        mongo_insert( gfs->client, gfs->chunks_ns, oChunk, NULL );
        chunk_free( oChunk );
        chunkNumber++;
        data_ptr += chunkLen;
    }

    /* Inserts file's metadata */
    return gridfs_insert_file( gfs, remotename, id, length, contenttype );
}

MONGO_EXPORT void gridfile_writer_init( gridfile *gfile, gridfs *gfs,
                           const char *remote_name, const char *content_type ) {
    gfile->gfs = gfs;

    bson_oid_gen( &( gfile->id ) );
    gfile->chunk_num = 0;
    gfile->length = 0;
    gfile->pending_len = 0;
    gfile->pending_data = NULL;

    gfile->remote_name = ( char * )bson_malloc( strlen( remote_name ) + 1 );
    strcpy( ( char * )gfile->remote_name, remote_name );

    gfile->content_type = ( char * )bson_malloc( strlen( content_type ) + 1 );
    strcpy( ( char * )gfile->content_type, content_type );
}

MONGO_EXPORT void gridfile_write_buffer( gridfile *gfile, const char *data,
    gridfs_offset length ) {

    int bytes_left = 0;
    int data_partial_len = 0;
    int chunks_to_write = 0;
    char *buffer;
    bson *oChunk;
    gridfs_offset to_write = length + gfile->pending_len;

    if ( to_write < DEFAULT_CHUNK_SIZE ) { /* Less than one chunk to write */
        if( gfile->pending_data ) {
            gfile->pending_data = ( char * )bson_realloc( ( void * )gfile->pending_data, gfile->pending_len + to_write );
            memcpy( gfile->pending_data + gfile->pending_len, data, length );
        } else if ( to_write > 0 ) {
            gfile->pending_data = ( char * )bson_malloc( to_write );
            memcpy( gfile->pending_data, data, length );
        }
        gfile->pending_len += length;

    } else { /* At least one chunk of data to write */
        chunks_to_write = to_write / DEFAULT_CHUNK_SIZE;
        bytes_left = to_write % DEFAULT_CHUNK_SIZE;

        /* If there's a pending chunk to be written, we need to combine
         * the buffer provided up to DEFAULT_CHUNK_SIZE.
         */
        if ( gfile->pending_len > 0 ) {
            data_partial_len = DEFAULT_CHUNK_SIZE - gfile->pending_len;
            buffer = ( char * )bson_malloc( DEFAULT_CHUNK_SIZE );
            memcpy( buffer, gfile->pending_data, gfile->pending_len );
            memcpy( buffer + gfile->pending_len, data, data_partial_len );

            oChunk = chunk_new( gfile->id, gfile->chunk_num, buffer, DEFAULT_CHUNK_SIZE );
            mongo_insert( gfile->gfs->client, gfile->gfs->chunks_ns, oChunk, NULL );
            chunk_free( oChunk );
            gfile->chunk_num++;
            gfile->length += DEFAULT_CHUNK_SIZE;
            data += data_partial_len;

            chunks_to_write--;

            bson_free( buffer );
        }

        while( chunks_to_write > 0 ) {
            oChunk = chunk_new( gfile->id, gfile->chunk_num, data, DEFAULT_CHUNK_SIZE );
            mongo_insert( gfile->gfs->client, gfile->gfs->chunks_ns, oChunk, NULL );
            chunk_free( oChunk );
            gfile->chunk_num++;
            chunks_to_write--;
            gfile->length += DEFAULT_CHUNK_SIZE;
            data += DEFAULT_CHUNK_SIZE;
        }

        bson_free( gfile->pending_data );

        /* If there are any leftover bytes, store them as pending data. */
        if( bytes_left == 0 )
            gfile->pending_data = NULL;
        else {
            gfile->pending_data = ( char * )bson_malloc( bytes_left );
            memcpy( gfile->pending_data, data, bytes_left );
        }

        gfile->pending_len = bytes_left;
    }
}

MONGO_EXPORT int gridfile_writer_done( gridfile *gfile ) {

    /* write any remaining pending chunk data.
     * pending data will always take up less than one chunk */
    bson *oChunk;
    int response;
    if( gfile->pending_data ) {
        oChunk = chunk_new( gfile->id, gfile->chunk_num, gfile->pending_data, gfile->pending_len );
        mongo_insert( gfile->gfs->client, gfile->gfs->chunks_ns, oChunk, NULL );
        chunk_free( oChunk );
        bson_free( gfile->pending_data );
        gfile->length += gfile->pending_len;
    }

    /* insert into files collection */
    response = gridfs_insert_file( gfile->gfs, gfile->remote_name, gfile->id,
                                   gfile->length, gfile->content_type );

    bson_free( gfile->remote_name );
    bson_free( gfile->content_type );

    return response;
}

int gridfs_store_file( gridfs *gfs, const char *filename,
                        const char *remotename, const char *contenttype ) {

    char buffer[DEFAULT_CHUNK_SIZE];
    FILE *fd;
    bson_oid_t id;
    int chunkNumber = 0;
    gridfs_offset length = 0;
    gridfs_offset chunkLen = 0;
    bson *oChunk;

    /* Open the file and the correct stream */
    if ( strcmp( filename, "-" ) == 0 ) fd = stdin;
    else {
        fd = fopen( filename, "rb" );
        if (fd == NULL)
            return MONGO_ERROR;
    }

    /* Generate and append an oid*/
    bson_oid_gen( &id );

    /* Insert the file chunk by chunk */
    chunkLen = fread( buffer, 1, DEFAULT_CHUNK_SIZE, fd );
    do {
        oChunk = chunk_new( id, chunkNumber, buffer, chunkLen );
        mongo_insert( gfs->client, gfs->chunks_ns, oChunk, NULL );
        chunk_free( oChunk );
        length += chunkLen;
        chunkNumber++;
        chunkLen = fread( buffer, 1, DEFAULT_CHUNK_SIZE, fd );
    } while ( chunkLen != 0 );

    /* Close the file stream */
    if ( fd != stdin ) fclose( fd );

    /* Large files Assertion */
    /* assert(length <= 0xffffffff); */

    /* Optional Remote Name */
    if ( remotename == NULL || *remotename == '\0' ) {
        remotename = filename;
    }

    /* Inserts file's metadata */
    return gridfs_insert_file( gfs, remotename, id, length, contenttype );
}

MONGO_EXPORT void gridfs_remove_filename( gridfs *gfs, const char *filename ) {
    bson query;
    mongo_cursor *files;
    bson file;
    bson_iterator it;
    bson_oid_t id;
    bson b;

    bson_init( &query );
    bson_append_string( &query, "filename", filename );
    bson_finish( &query );
    files = mongo_find( gfs->client, gfs->files_ns, &query, NULL, 0, 0, 0 );
    bson_destroy( &query );

    /* Remove each file and it's chunks from files named filename */
    while ( mongo_cursor_next( files ) == MONGO_OK ) {
        file = files->current;
        bson_find( &it, &file, "_id" );
        id = *bson_iterator_oid( &it );

        /* Remove the file with the specified id */
        bson_init( &b );
        bson_append_oid( &b, "_id", &id );
        bson_finish( &b );
        mongo_remove( gfs->client, gfs->files_ns, &b, NULL );
        bson_destroy( &b );

        /* Remove all chunks from the file with the specified id */
        bson_init( &b );
        bson_append_oid( &b, "files_id", &id );
        bson_finish( &b );
        mongo_remove( gfs->client, gfs->chunks_ns, &b, NULL );
        bson_destroy( &b );
    }

    mongo_cursor_destroy( files );
}

int gridfs_find_query( gridfs *gfs, bson *query,
                       gridfile *gfile ) {

    bson uploadDate;
    bson finalQuery;
    bson out;
    int i;

    bson_init( &uploadDate );
    bson_append_int( &uploadDate, "uploadDate", -1 );
    bson_finish( &uploadDate );

    bson_init( &finalQuery );
    bson_append_bson( &finalQuery, "query", query );
    bson_append_bson( &finalQuery, "orderby", &uploadDate );
    bson_finish( &finalQuery );

    i = ( mongo_find_one( gfs->client, gfs->files_ns,
                          &finalQuery, NULL, &out ) == MONGO_OK );
    bson_destroy( &uploadDate );
    bson_destroy( &finalQuery );
    if ( !i )
        return MONGO_ERROR;
    else {
        gridfile_init( gfs, &out, gfile );
        bson_destroy( &out );
        return MONGO_OK;
    }
}

int gridfs_find_filename( gridfs *gfs, const char *filename,
                          gridfile *gfile )

{
    bson query;
    int i;

    bson_init( &query );
    bson_append_string( &query, "filename", filename );
    bson_finish( &query );
    i = gridfs_find_query( gfs, &query, gfile );
    bson_destroy( &query );
    return i;
}

int gridfile_init( gridfs *gfs, bson *meta, gridfile *gfile )

{
    gfile->gfs = gfs;
    gfile->pos = 0;
    gfile->meta = ( bson * )bson_malloc( sizeof( bson ) );
    if ( gfile->meta == NULL ) return MONGO_ERROR;
    bson_copy( gfile->meta, meta );
    return MONGO_OK;
}

MONGO_EXPORT void gridfile_destroy( gridfile *gfile )

{
    bson_destroy( gfile->meta );
    bson_free( gfile->meta );
}

bson_bool_t gridfile_exists( gridfile *gfile ) {
    return ( bson_bool_t )( gfile != NULL || gfile->meta == NULL );
}

MONGO_EXPORT const char *gridfile_get_filename( gridfile *gfile ) {
    bson_iterator it;

    bson_find( &it, gfile->meta, "filename" );
    return bson_iterator_string( &it );
}

MONGO_EXPORT int gridfile_get_chunksize( gridfile *gfile ) {
    bson_iterator it;

    bson_find( &it, gfile->meta, "chunkSize" );
    return bson_iterator_int( &it );
}

MONGO_EXPORT gridfs_offset gridfile_get_contentlength( gridfile *gfile ) {
    bson_iterator it;

    bson_find( &it, gfile->meta, "length" );

    if( bson_iterator_type( &it ) == BSON_INT )
        return ( gridfs_offset )bson_iterator_int( &it );
    else
        return ( gridfs_offset )bson_iterator_long( &it );
}

MONGO_EXPORT const char *gridfile_get_contenttype( gridfile *gfile ) {
    bson_iterator it;

    if ( bson_find( &it, gfile->meta, "contentType" ) )
        return bson_iterator_string( &it );
    else return NULL;
}

MONGO_EXPORT bson_date_t gridfile_get_uploaddate( gridfile *gfile ) {
    bson_iterator it;

    bson_find( &it, gfile->meta, "uploadDate" );
    return bson_iterator_date( &it );
}

MONGO_EXPORT const char *gridfile_get_md5( gridfile *gfile ) {
    bson_iterator it;

    bson_find( &it, gfile->meta, "md5" );
    return bson_iterator_string( &it );
}

const char *gridfile_get_field( gridfile *gfile, const char *name ) {
    bson_iterator it;

    bson_find( &it, gfile->meta, name );
    return bson_iterator_value( &it );
}

bson_bool_t gridfile_get_boolean( gridfile *gfile, const char *name ) {
    bson_iterator it;

    bson_find( &it, gfile->meta, name );
    return bson_iterator_bool( &it );
}

MONGO_EXPORT void gridfile_get_metadata( gridfile *gfile, bson* out ) {
    bson_iterator it;

    if ( bson_find( &it, gfile->meta, "metadata" ) )
        bson_iterator_subobject( &it, out );
    else
        bson_empty( out );
}

MONGO_EXPORT int gridfile_get_numchunks( gridfile *gfile ) {
    bson_iterator it;
    gridfs_offset length;
    gridfs_offset chunkSize;
    double numchunks;

    bson_find( &it, gfile->meta, "length" );

    if( bson_iterator_type( &it ) == BSON_INT )
        length = ( gridfs_offset )bson_iterator_int( &it );
    else
        length = ( gridfs_offset )bson_iterator_long( &it );

    bson_find( &it, gfile->meta, "chunkSize" );
    chunkSize = bson_iterator_int( &it );
    numchunks = ( ( double )length/( double )chunkSize );
    return ( numchunks - ( int )numchunks > 0 )
           ? ( int )( numchunks+1 )
           : ( int )( numchunks );
}

MONGO_EXPORT void gridfile_get_chunk( gridfile *gfile, int n, bson* out ) {
    bson query;
    
    bson_iterator it;
    bson_oid_t id;
    int result;

    bson_init( &query );
    bson_find( &it, gfile->meta, "_id" );
    id = *bson_iterator_oid( &it );
    bson_append_oid( &query, "files_id", &id );
    bson_append_int( &query, "n", n );
    bson_finish( &query );

    result = (mongo_find_one(gfile->gfs->client,
                             gfile->gfs->chunks_ns,
                             &query, NULL, out ) == MONGO_OK );
    bson_destroy( &query );
    if (!result) {
        bson empty;
        bson_empty(&empty);
        bson_copy(out, &empty);
    }
}

MONGO_EXPORT mongo_cursor *gridfile_get_chunks( gridfile *gfile, int start, int size ) {
    bson_iterator it;
    bson_oid_t id;
    bson gte;
    bson query;
    bson orderby;
    bson command;
    mongo_cursor *cursor;

    bson_find( &it, gfile->meta, "_id" );
    id = *bson_iterator_oid( &it );

    bson_init( &query );
    bson_append_oid( &query, "files_id", &id );
    if ( size == 1 ) {
        bson_append_int( &query, "n", start );
    } else {
        bson_init( &gte );
        bson_append_int( &gte, "$gte", start );
        bson_finish( &gte );
        bson_append_bson( &query, "n", &gte );
        bson_destroy( &gte );
    }
    bson_finish( &query );

    bson_init( &orderby );
    bson_append_int( &orderby, "n", 1 );
    bson_finish( &orderby );

    bson_init( &command );
    bson_append_bson( &command, "query", &query );
    bson_append_bson( &command, "orderby", &orderby );
    bson_finish( &command );

    cursor = mongo_find( gfile->gfs->client, gfile->gfs->chunks_ns,
                         &command, NULL, size, 0, 0 );

    bson_destroy( &command );
    bson_destroy( &query );
    bson_destroy( &orderby );

    return cursor;
}

gridfs_offset gridfile_write_file( gridfile *gfile, FILE *stream ) {
    int i;
    size_t len;
    bson chunk;
    bson_iterator it;
    const char *data;
    const int num = gridfile_get_numchunks( gfile );

    for ( i=0; i<num; i++ ) {
        gridfile_get_chunk( gfile, i, &chunk );
        bson_find( &it, &chunk, "data" );
        len = bson_iterator_bin_len( &it );
        data = bson_iterator_bin_data( &it );
        fwrite( data, sizeof( char ), len, stream );
        bson_destroy( &chunk );
    }

    return gridfile_get_contentlength( gfile );
}

MONGO_EXPORT gridfs_offset gridfile_read( gridfile *gfile, gridfs_offset size, char *buf ) {
    mongo_cursor *chunks;
    bson chunk;

    int first_chunk;
    int last_chunk;
    int total_chunks;
    gridfs_offset chunksize;
    gridfs_offset contentlength;
    gridfs_offset bytes_left;
    int i;
    bson_iterator it;
    gridfs_offset chunk_len;
    const char *chunk_data;

    contentlength = gridfile_get_contentlength( gfile );
    chunksize = gridfile_get_chunksize( gfile );
    size = ( contentlength - gfile->pos < size )
           ? contentlength - gfile->pos
           : size;
    bytes_left = size;

    first_chunk = ( gfile->pos )/chunksize;
    last_chunk = ( gfile->pos+size-1 )/chunksize;
    total_chunks = last_chunk - first_chunk + 1;
    chunks = gridfile_get_chunks( gfile, first_chunk, total_chunks );

    for ( i = 0; i < total_chunks; i++ ) {
        mongo_cursor_next( chunks );
        chunk = chunks->current;
        bson_find( &it, &chunk, "data" );
        chunk_len = bson_iterator_bin_len( &it );
        chunk_data = bson_iterator_bin_data( &it );
        if ( i == 0 ) {
            chunk_data += ( gfile->pos )%chunksize;
            chunk_len -= ( gfile->pos )%chunksize;
        }
        if ( bytes_left > chunk_len ) {
            memcpy( buf, chunk_data, chunk_len );
            bytes_left -= chunk_len;
            buf += chunk_len;
        } else {
            memcpy( buf, chunk_data, bytes_left );
        }
    }

    mongo_cursor_destroy( chunks );
    gfile->pos = gfile->pos + size;

    return size;
}

MONGO_EXPORT gridfs_offset gridfile_seek( gridfile *gfile, gridfs_offset offset ) {
    gridfs_offset length;

    length = gridfile_get_contentlength( gfile );
    gfile->pos = length < offset ? length : offset;
    return gfile->pos;
}
