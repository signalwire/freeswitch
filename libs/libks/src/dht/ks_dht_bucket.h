/*
 * Copyright (c) 2016 Colm Quinn
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _KS_DHT_BUCKETS_H_
#define _KS_DHT_BUCKETS_H_

#ifdef __cplusplus
#define KS_BEGIN_EXTERN_C       extern "C" {
#define KS_END_EXTERN_C         }
#else
#define KS_BEGIN_EXTERN_C
#define KS_END_EXTERN_C
#endif

#include "ks.h"

KS_BEGIN_EXTERN_C 

/* @todo: temporary - replace with real definiton when available */
typedef void  ks_dht_node_t;


enum ks_dhtrt_nodestate_t {DHTRT_UNKNOWN, DHTRT_ACTIVE, DHTRT_SUSPECT, DHTRT_EXPIRED};
 
typedef ks_status_t (*ks_dhtrt_callback)(ks_dht_node_t*, enum ks_dhtrt_nodestate_t);  


/* for testing */
#define KS_DHT_BUCKETSIZE 20
#define KS_DHT_IDSIZE     20  


typedef struct ks_dhtrt_node_s {
    unsigned char  id[KS_DHT_IDSIZE];
    ks_dht_node_t* handle;
} ks_dhtrt_node;

typedef struct ks_dhtrt_routetable_s {
    void*       internal;                       /* ks_dhtrt_internal */
    ks_pool_t*  pool;                           /*  */
    ks_logger_t logger;
} ks_dhtrt_routetable;

typedef struct ks_dhtrt_querynodes_s {
    unsigned char  id[KS_DHT_IDSIZE]; /* in: id to query */
	uint8_t        max;               /* in: maximum to return */
	uint8_t        count;             /* out: number returned */
	ks_dht_node_t* nodes[KS_DHT_BUCKETSIZE];  /* out: array of peers (ks_dht_node_t* peer[incount]) */
}  ks_dhtrt_querynodes;


typedef unsigned char ks_dhtrt_nodeid[KS_DHT_IDSIZE];

/* methods */

ks_dhtrt_routetable* ks_dhtrt_initroute( ks_pool_t *pool, ks_dhtrt_nodeid localid);
ks_status_t        ks_dhtrt_registercallback(ks_dhtrt_callback, enum ks_dhtrt_nodestate_t);
void               ks_dhtrt_deinitroute(ks_dhtrt_routetable* table ); 
 
ks_dhtrt_node*     ks_dhtrt_create_node(ks_dhtrt_routetable* table, ks_dhtrt_nodeid nodeid, ks_dht_node_t* node);
ks_status_t        ks_dhtrt_delete_node(ks_dhtrt_routetable* table, ks_dhtrt_node* node);

ks_status_t        ks_dhtrt_touch_node(ks_dhtrt_routetable* table,  ks_dhtrt_nodeid nodeid);
ks_status_t        ks_dhtrt_expire_node(ks_dhtrt_routetable* table,  ks_dhtrt_nodeid nodeid);

uint8_t            ks_dhtrt_findclosest_nodes(ks_dhtrt_routetable* table, ks_dhtrt_querynodes* query);
ks_dht_node_t*     ks_dhtrt_find_node(ks_dhtrt_routetable* table, ks_dhtrt_nodeid id);


/* debugging aids */
void               ks_dhtrt_dump(ks_dhtrt_routetable* table, int level);
void               ks_dhtrt_process_table(ks_dhtrt_routetable* table);     


KS_END_EXTERN_C

#endif

