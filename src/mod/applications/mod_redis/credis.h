/* credis.h -- a C client library for Redis, public API.
 *
 * Copyright (c) 2009-2010, Jonas Romfelt <jonas at romfelt dot se>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __CREDIS_H
#define __CREDIS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Functions below should map quite nicely to Redis 1.02 command set. 
 * Refer to the official Redis documentation for further explanation of 
 * each command. See credis examples that show how functions can be used.
 * Here is a brief example that connects to a Redis server and sets value 
 * of key `fruit' to `banana': 
 *
 *    REDIS rh = credis_connect("localhost", 6789, 2000);
 *    credis_set(rh, "fruit", "banana");
 *    credis_close(rh);
 *
 * In general, functions return 0 on success or a negative value on
 * error. Refer to CREDIS_ERR_* codes. The return code -1 is typically 
 * used when for instance a key is not found. 
 *
 * IMPORTANT! Memory buffers are allocated, used and managed by credis 
 * internally. Subsequent calls to credis functions _will_ destroy the 
 * data to which returned values reference to. If for instance the 
 * returned value by a call to credis_get() is to be used later in the 
 * program, a strdup() is highly recommended. However, each `REDIS' 
 * handle has its own state and manages its own memory buffer 
 * independently. That means that one of two handles can be destroyed
 * while the other keeps its connection and data.
 * 
 * TODO
 *  - Currently only support for zero-terminated strings, not for storing 
 *    abritary binary data as bulk data. Basically an API issue since it
 *    is partially supported internally.
 *  - Support for Redis >= 1.1 protocol
 */

/* handle to a Redis server connection */
typedef struct _cr_redis* REDIS;

#define CREDIS_OK 0
#define CREDIS_ERR -90
#define CREDIS_ERR_NOMEM -91
#define CREDIS_ERR_RESOLVE -92
#define CREDIS_ERR_CONNECT -93
#define CREDIS_ERR_SEND -94
#define CREDIS_ERR_RECV -95
#define CREDIS_ERR_TIMEOUT -96
#define CREDIS_ERR_PROTOCOL -97

#define CREDIS_TYPE_NONE 1
#define CREDIS_TYPE_STRING 2
#define CREDIS_TYPE_LIST 3
#define CREDIS_TYPE_SET 4

#define CREDIS_SERVER_MASTER 1
#define CREDIS_SERVER_SLAVE 2

#define CREDIS_VERSION_STRING_SIZE 32

typedef struct _cr_info {
  char redis_version[CREDIS_VERSION_STRING_SIZE];
  int bgsave_in_progress;
  int connected_clients;
  int connected_slaves;
  unsigned int used_memory;
  long long changes_since_last_save;
  int last_save_time;
  long long total_connections_received;
  long long total_commands_processed;
  int uptime_in_seconds;
  int uptime_in_days;
  int role;
} REDIS_INFO;


/*
 * Connection handling
 */

/* setting host to NULL will use "localhost". setting port to 0 will use 
 * default port 6379 */
REDIS credis_connect(const char *host, int port, int timeout);

void credis_close(REDIS rhnd);

void credis_quit(REDIS rhnd);

int credis_auth(REDIS rhnd, const char *password);

int credis_ping(REDIS rhnd);

/* 
 * Commands operating on string values 
 */

int credis_set(REDIS rhnd, const char *key, const char *val);

/* returns -1 if the key doesn't exists */
int credis_get(REDIS rhnd, const char *key, char **val);

/* returns -1 if the key doesn't exists */
int credis_getset(REDIS rhnd, const char *key, const char *set_val, char **get_val);

/* returns number of values returned in vector `valv'. `keyc' is the number of
 * keys stored in `keyv'. */
int credis_mget(REDIS rhnd, int keyc, const char **keyv, char ***valv);

/* returns -1 if the key already exists and hence not set */
int credis_setnx(REDIS rhnd, const char *key, const char *val);

int credis_incr(REDIS rhnd, const char *key, int *new_val);

int credis_incrby(REDIS rhnd, const char *key, int incr_val, int *new_val);

int credis_decr(REDIS rhnd, const char *key, int *new_val);

int credis_decrby(REDIS rhnd, const char *key, int decr_val, int *new_val);

/* returns -1 if the key doesn't exists and 0 if it does */
int credis_exists(REDIS rhnd, const char *key);

/* returns -1 if the key doesn't exists and 0 if it was removed */
int credis_del(REDIS rhnd, const char *key);

/* returns type, refer to CREDIS_TYPE_* defines */
int credis_type(REDIS rhnd, const char *key);

/* TODO for Redis >= 1.1 
 * MSET key1 value1 key2 value2 ... keyN valueN set a multiple keys to multiple values in a single atomic operation
 * MSETNX key1 value1 key2 value2 ... keyN valueN set a multiple keys to multiple values in a single atomic operation if none of
 * DEL key1 key2 ... keyN remove multiple keys 
 */

/*
 * Commands operating on key space 
 */

int credis_keys(REDIS rhnd, const char *pattern, char **keyv, int len);

int credis_randomkey(REDIS rhnd, char **key);

int credis_rename(REDIS rhnd, const char *key, const char *new_key_name);

/* returns -1 if the key already exists */
int credis_renamenx(REDIS rhnd, const char *key, const char *new_key_name);

/* returns size of db */
int credis_dbsize(REDIS rhnd);

/* returns -1 if the timeout was not set; either due to key already has 
   an associated timeout or key does not exist */
int credis_expire(REDIS rhnd, const char *key, int secs);

/* returns time to live seconds or -1 if key does not exists or does not 
 * have expire set */
int credis_ttl(REDIS rhnd, const char *key);

/*
 * Commands operating on lists 
 */

int credis_rpush(REDIS rhnd, const char *key, const char *element);

int credis_lpush(REDIS rhnd, const char *key, const char *element);

/* returns length of list */
int credis_llen(REDIS rhnd, const char *key);

/* returns number of elements returned in vector `elementv' */
int credis_lrange(REDIS rhnd, const char *key, int start, int range, char ***elementv);

int credis_ltrim(REDIS rhnd, const char *key, int start, int end);

/* returns -1 if the key doesn't exists */
int credis_lindex(REDIS rhnd, const char *key, int index, char **element);

int credis_lset(REDIS rhnd, const char *key, int index, const char *element);

/* returns number of elements removed */
int credis_lrem(REDIS rhnd, const char *key, int count, const char *element);

/* returns -1 if the key doesn't exists */
int credis_lpop(REDIS rhnd, const char *key, char **val);

/* returns -1 if the key doesn't exists */
int credis_rpop(REDIS rhnd, const char *key, char **val);

/* TODO for Redis >= 1.1 
 * RPOPLPUSH srckey dstkey 
 *
 * TODO for Redis >= 1.3.1
 * BLPOP key1 key2 ... keyN timeout
 * BRPOP key1 key2 ... keyN timeout
 */

/*
 * Commands operating on sets 
 */

/* returns -1 if the given member was already a member of the set */
int credis_sadd(REDIS rhnd, const char *key, const char *member);

/* returns -1 if the given member is not a member of the set */
int credis_srem(REDIS rhnd, const char *key, const char *member);

/* returns -1 if the key doesn't exists and 0 if it does */
int credis_sismember(REDIS rhnd, const char *key, const char *member);

/* returns -1 if the given key doesn't exists else value is returned in `member' */
int credis_spop(REDIS rhnd, const char *key, char **member);

/* returns -1 if the member doesn't exists in the source set */
int credis_smove(REDIS rhnd, const char *sourcekey, const char *destkey, 
                 const char *member);

/* returns cardinality (number of members) or 0 if the given key doesn't exists */
int credis_scard(REDIS rhnd, const char *key);

/* returns number of members returned in vector `members'. `keyc' is the number of
 * keys stored in `keyv'. */
int credis_sinter(REDIS rhnd, int keyc, const char **keyv, char ***members);

/* `keyc' is the number of keys stored in `keyv' */
int credis_sinterstore(REDIS rhnd, const char *destkey, int keyc, const char **keyv);

/* returns number of members returned in vector `members'. `keyc' is the number of
 * keys stored in `keyv'. */
int credis_sunion(REDIS rhnd, int keyc, const char **keyv, char ***members);

/* `keyc' is the number of keys stored in `keyv' */
int credis_sunionstore(REDIS rhnd, const char *destkey, int keyc, const char **keyv);

/* returns number of members returned in vector `members'. `keyc' is the number of
 * keys stored in `keyv'. */
int credis_sdiff(REDIS rhnd, int keyc, const char **keyv, char ***members);

/* `keyc' is the number of keys stored in `keyv' */
int credis_sdiffstore(REDIS rhnd, const char *destkey, int keyc, const char **keyv);

/* returns number of members returned in vector `members' */
int credis_smembers(REDIS rhnd, const char *key, char ***members);

/* TODO Redis >= 1.1
 * SRANDMEMBER key Return a random member of the Set value at key
 */

/*
 * Multiple databases handling commands 
 */

int credis_select(REDIS rhnd, int index);

/* returns -1 if the key was not moved; already present at target 
 * or not found on current db */
int credis_move(REDIS rhnd, const char *key, int index);

int credis_flushdb(REDIS rhnd);

int credis_flushall(REDIS rhnd);

/*
 * Sorting 
 */

/* returns number of elements returned in vector `elementv' */
int credis_sort(REDIS rhnd, const char *query, char ***elementv);

/* 
 * Persistence control commands 
 */

int credis_save(REDIS rhnd);

int credis_bgsave(REDIS rhnd);

/* returns UNIX time stamp of last successfull save to disk */
int credis_lastsave(REDIS rhnd);

int credis_shutdown(REDIS rhnd);

/*
 * Remote server control commands 
 */

int credis_info(REDIS rhnd, REDIS_INFO *info);

int credis_monitor(REDIS rhnd);

/* setting host to NULL and/or port to 0 will turn off replication */
int credis_slaveof(REDIS rhnd, const char *host, int port);

#ifdef __cplusplus
}
#endif

#endif /* __CREDIS_H */
