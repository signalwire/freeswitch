/*
 * Copyright (c) 2016, FreeSWITCH Solutions LLC
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
 * A PARTICULAR PURPOSE ARE DISCLAIMED.	 IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* # pragma GCC optimize ("O0") */


#include "ks_dht.h"


/* change for testing */
#define KS_DHT_BUCKETSIZE 20
#define KS_DHTRT_INACTIVETIME  (10*60)	
#define KS_DHTRT_EXPIREDTIME   (15*60)
#define KS_DHTRT_MAXPING  3
#define KS_DHTRT_PROCESSTABLE_INTERVAL (5*60)  
#define KS_DHTRT_PROCESSTABLE_SHORTINTERVAL120 (2*60)
#define KS_DHTRT_PROCESSTABLE_SHORTINTERVAL60 (1*60)
#define KS_DHTRT_RECYCLE_NODE_THRESHOLD 100

/* peer flags */
#define DHTPEER_DUBIOUS 0
#define DHTPEER_EXPIRED 1
#define DHTPEER_ACTIVE  2


typedef uint8_t ks_dhtrt_nodeid_t[KS_DHT_NODEID_SIZE];

/* internal structures */
typedef struct ks_dhtrt_bucket_entry_s {
	ks_time_t  tyme;
    ks_time_t  ping_tyme;
//	uint8_t	   id[KS_DHT_NODEID_SIZE];
	ks_dht_node_t *gptr;					/* ptr to peer */	
	uint8_t	   inuse;
	uint8_t	   outstanding_pings;
	uint8_t	   flags;					  /* active, suspect, expired */
	uint8_t    touched;                   /* did we ever get a touch */
} ks_dhtrt_bucket_entry_t;

typedef struct ks_dhtrt_bucket_s {
	ks_dhtrt_bucket_entry_t	 entries[KS_DHT_BUCKETSIZE];
	uint8_t		  count;
    ks_time_t     findtyme;       /* last time a find was generated */
	uint8_t		  expired_count;
	ks_rwl_t     *lock;           /* lock for safe traversal of the entry array */    
} ks_dhtrt_bucket_t; 


#define BHF_LEFT 0x80

typedef struct ks_dhtrt_bucket_header_s {
	struct ks_dhtrt_bucket_header_s * parent;
	struct ks_dhtrt_bucket_header_s * left;
	struct ks_dhtrt_bucket_header_s * right;
	struct ks_dhtrt_bucket_header_s * left1bit;
	struct ks_dhtrt_bucket_header_s * right1bit;
	ks_dhtrt_bucket_t *	 bucket;
	ks_time_t		 tyme;				   /* last processed time */
	unsigned char	 mask[KS_DHT_NODEID_SIZE];	/* node id mask		   */
	unsigned char	 flags;			  
} ks_dhtrt_bucket_header_t;

typedef struct ks_dhtrt_deletednode_s {
	ks_dht_node_t*  node;
	struct ks_dhtrt_deletednode_s *next;
} ks_dhtrt_deletednode_t;

typedef struct ks_dhtrt_internal_s {
	uint8_t	 localid[KS_DHT_NODEID_SIZE];
	ks_dhtrt_bucket_header_t *buckets;		/* root bucketheader */
	ks_dht_t               *dht; 
	ks_rwl_t 			   *lock;  		    /* lock for safe traversal of the tree */
	ks_time_t              last_process_table;
	ks_time_t              next_process_table_delta;
	ks_mutex_t             *deleted_node_lock;
	ks_dhtrt_deletednode_t *deleted_node;
	ks_dhtrt_deletednode_t *free_node_ex;
	uint32_t               deleted_count;
    uint32_t               bucket_count;
    uint32_t               header_count;
} ks_dhtrt_internal_t;

typedef struct ks_dhtrt_xort_s {
	unsigned int	ix;					  /* index of bucket array */	 
	unsigned char	xor[KS_DHT_NODEID_SIZE];  /* corresponding xor value */
	unsigned int	nextix;	  
} ks_dhtrt_xort_t;

typedef struct ks_dhtrt_sortedxors_s {
	ks_dhtrt_bucket_header_t *bheader;
	ks_dhtrt_xort_t	xort[KS_DHT_BUCKETSIZE];
	unsigned char	hixor[KS_DHT_NODEID_SIZE];
	unsigned int	startix;
	unsigned int	count;
	struct ks_dhtrt_sortedxors_s* next;	 
} ks_dhtrt_sortedxors_t;


/* --- static functions ---- */

static 
ks_dhtrt_bucket_header_t *ks_dhtrt_create_bucketheader(
													   ks_pool_t *pool, 
													   ks_dhtrt_bucket_header_t *parent, 
													   unsigned char *mask);
static
ks_dhtrt_bucket_t *ks_dhtrt_create_bucket(ks_pool_t *pool);
static
ks_dhtrt_bucket_header_t *ks_dhtrt_find_bucketheader(ks_dhtrt_routetable_t *table, ks_dhtrt_nodeid_t id);
static
ks_dhtrt_bucket_header_t *ks_dhtrt_find_relatedbucketheader(ks_dhtrt_bucket_header_t *header, ks_dhtrt_nodeid_t id);
static
ks_dhtrt_bucket_entry_t *ks_dhtrt_find_bucketentry(ks_dhtrt_bucket_header_t *header, ks_dhtrt_nodeid_t id);

static
void ks_dhtrt_split_bucket(ks_dhtrt_bucket_header_t *original, ks_dhtrt_bucket_header_t *left, ks_dhtrt_bucket_header_t *right);
static
ks_dht_node_t *ks_dhtrt_find_nodeid(ks_dhtrt_bucket_t *bucket, ks_dhtrt_nodeid_t nodeid);


static  
void ks_dhtrt_shiftright(uint8_t *id); 
static
void ks_dhtrt_shiftleft(uint8_t *id);
static
void ks_dhtrt_midmask(uint8_t *leftid, uint8_t *rightid, uint8_t *midpt);

static  
void ks_dhtrt_xor(const uint8_t *id1, const uint8_t *id2, uint8_t *xor);
static  
int ks_dhtrt_ismasked(const uint8_t *id1, const uint8_t *mask);
static
void ks_dhtrt_queue_node_fordelete(ks_dhtrt_routetable_t *table, ks_dht_node_t* node);
static
void ks_dhtrt_process_deleted(ks_dhtrt_routetable_t *table, int8_t all); 

static
ks_dht_node_t *ks_dhtrt_make_node(ks_dhtrt_routetable_t *table);
static
ks_status_t ks_dhtrt_insert_node(ks_dhtrt_routetable_t *table, ks_dht_node_t *node, enum ks_create_node_flags_t flags);
static
ks_dhtrt_bucket_entry_t* ks_dhtrt_insert_id(ks_dhtrt_bucket_t *bucket, ks_dht_node_t *node);
static
ks_status_t ks_dhtrt_delete_id(ks_dhtrt_bucket_t *bucket, ks_dhtrt_nodeid_t id);
static
char *ks_dhtrt_printableid(uint8_t *id, char *buffer);

static
uint8_t ks_dhtrt_findclosest_locked_nodes(ks_dhtrt_routetable_t *table, ks_dhtrt_querynodes_t *query);
static
uint8_t ks_dhtrt_load_query(ks_dhtrt_querynodes_t *query, ks_dhtrt_sortedxors_t *xort);
static
uint8_t ks_dhtrt_findclosest_bucketnodes(unsigned char *nodeid,
                                         enum ks_dht_nodetype_t type,
                                         enum ks_afflags_t family,
										 ks_dhtrt_bucket_header_t *header,
										 ks_dhtrt_sortedxors_t *xors,
										 unsigned char *hixor,
										 unsigned int max);

static
void ks_dhtrt_ping(ks_dhtrt_internal_t *internal, ks_dhtrt_bucket_entry_t *entry);
static
void ks_dhtrt_find(ks_dhtrt_routetable_t *table, ks_dhtrt_internal_t *internal, ks_dht_nodeid_t *nodeid);


/* debugging */
#define KS_DHT_DEBUGPRINTF_
/* very verbose                   */
/* # define KS_DHT_DEBUGPRINTFX_  */
/* debug locking                  */
/* #define KS_DHT_DEBUGLOCKPRINTF_ */ 

KS_DECLARE(ks_status_t) ks_dhtrt_initroute(ks_dhtrt_routetable_t **tableP,
											ks_dht_t *dht,
											ks_pool_t *pool) 
{

	unsigned char initmask[KS_DHT_NODEID_SIZE];
	memset(initmask, 0xff, sizeof(initmask));

	ks_dhtrt_routetable_t *table =	 ks_pool_alloc(pool, sizeof(ks_dhtrt_routetable_t));

	ks_dhtrt_internal_t *internal =	  ks_pool_alloc(pool, sizeof(ks_dhtrt_internal_t));

	ks_rwl_create(&internal->lock, pool);
	internal->dht   = dht;  
    internal->next_process_table_delta = KS_DHTRT_PROCESSTABLE_INTERVAL; 
    ks_mutex_create(&internal->deleted_node_lock, KS_MUTEX_FLAG_DEFAULT, pool);
	table->internal = internal;

	/* initialize root bucket */
	ks_dhtrt_bucket_header_t *initial_header = ks_dhtrt_create_bucketheader(pool, 0, initmask);

	initial_header->flags = BHF_LEFT;	 /* fake left to allow splitting */ 
	internal->buckets = initial_header;
	initial_header->bucket =  ks_dhtrt_create_bucket(pool);
	internal->header_count = 1;
	internal->bucket_count = 1;
	table->pool = pool;

	*tableP = table;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(void) ks_dhtrt_deinitroute(ks_dhtrt_routetable_t **tableP) 
{
	/* @todo*/
    ks_dhtrt_routetable_t* table = *tableP;

    if (!table || !table->internal) {
        return;
    }

    ks_dhtrt_internal_t* internal = table->internal;
    ks_rwl_write_lock(internal->lock);      /* grab write lock */ 
    ks_dhtrt_process_deleted(table, 1);
    table->internal = NULL;                 /* make sure no other threads get in */
	ks_pool_t *pool = table->pool;


    ks_dhtrt_bucket_header_t *header = internal->buckets;
    ks_dhtrt_bucket_header_t *last_header;
    ks_dhtrt_bucket_header_t *stack[KS_DHT_NODEID_SIZE * 8];
    int stackix=0;

    while (header) {
        stack[stackix++] = header;

        if (header->bucket) {
            ks_pool_free(pool, &header->bucket);
            header->bucket = NULL;
        }
		last_header = header; 
        header = header->left;

        if (header == 0 && stackix > 1) {
            stackix -= 2;
            header =  stack[stackix];
            header = header->right;
#ifdef  KS_DHT_DEBUGLOCKPRINTFX_
            char buf[100];
            ks_log(KS_LOG_DEBUG,"deinit: freeing bucket header %s\n", ks_dhtrt_printableid(last_header->mask, buf));
#endif  
			ks_pool_free(pool, &last_header);          
        }
    }

    ks_pool_free(pool, &internal);
	ks_pool_free(pool, &table);
    *tableP = NULL;

	return;
}



KS_DECLARE(ks_status_t)	 ks_dhtrt_create_node( ks_dhtrt_routetable_t *table, 
											   ks_dht_nodeid_t nodeid,
											   enum ks_dht_nodetype_t type,	
											   char *ip,
											   unsigned short port,
											   enum ks_create_node_flags_t flags,
											   ks_dht_node_t **node) 
{
    if (!table || !table->internal) {
        return KS_STATUS_FAIL;
    }

    ks_dht_node_t *tnode;
    ks_dhtrt_internal_t* internal = table->internal;

    ks_rwl_read_lock(internal->lock);      /* grab write lock and insert */

    ks_dhtrt_bucket_header_t *header = ks_dhtrt_find_bucketheader(table, nodeid.id);
    assert(header != NULL);             /* should always find a header */

    ks_dhtrt_bucket_entry_t *bentry = ks_dhtrt_find_bucketentry(header, nodeid.id);

    if (bentry != 0) { 

        ks_rwl_read_lock(header->bucket->lock);

		bentry->tyme = ks_time_now_sec();

		/* touch */
		if (flags == KS_DHTRT_CREATE_TOUCH) {
			bentry->outstanding_pings = 0;
			bentry->touched = 1;	
			if (bentry->flags == DHTPEER_EXPIRED) {
				--header->bucket->expired_count;
			}		
		}
        
        if (bentry->touched) {
            bentry->flags = DHTPEER_ACTIVE;
        }

		/* ping */
        if (!bentry->touched && !bentry->outstanding_pings) {
			ks_dhtrt_ping(internal, bentry);	
		}

		tnode = bentry->gptr;

		ks_rwl_read_unlock(header->bucket->lock);
        ks_rwl_read_lock( tnode->reflock);
		ks_rwl_read_unlock(internal->lock);

        (*node) = tnode;
		return KS_STATUS_SUCCESS;
    }
    ks_rwl_read_unlock(internal->lock);

    tnode = ks_dhtrt_make_node(table);
	tnode->table = table;

    enum ks_afflags_t family =  AF_INET;

	for (int i = 0; i < 5; ++i) {
		if (ip[i] == ':') { 
			family =	 AF_INET6; 
			break;
		} else if (ip[i] == '.') { 
			family =	 AF_INET; 
			break; 
		}
	}

    memcpy(tnode->nodeid.id, nodeid.id, KS_DHT_NODEID_SIZE);
    tnode->type = type;

    if (( ks_addr_set(&tnode->addr, ip, port, family) != KS_STATUS_SUCCESS) ||
        ( ks_rwl_create(&tnode->reflock, table->pool) !=  KS_STATUS_SUCCESS))       {
        ks_pool_free(table->pool, &tnode);
		ks_rwl_read_unlock(internal->lock);
        return KS_STATUS_FAIL;
    }

    ks_status_t s = ks_dhtrt_insert_node(table, tnode, flags);

    if (tnode && s == KS_STATUS_SUCCESS) {
		ks_rwl_read_lock( tnode->reflock);
	}

	(*node) = tnode;

	return s;
}

KS_DECLARE(ks_status_t) ks_dhtrt_delete_node(ks_dhtrt_routetable_t *table, ks_dht_node_t *node)
{
    char buf[51];
    if (!table || !table->internal) {
        return KS_STATUS_FAIL;
    }

    ks_status_t s =  KS_STATUS_FAIL;
    ks_dhtrt_internal_t* internal = table->internal;

    ks_rwl_read_lock(internal->lock);      /* grab read lock */
	ks_dhtrt_bucket_header_t *header = ks_dhtrt_find_bucketheader(table, node->nodeid.id);

	if (header != 0) {
		ks_dhtrt_bucket_t *bucket = header->bucket;

		if (bucket != 0) {			 /* we found a bucket*/
#ifdef  KS_DHT_DEBUGLOCKPRINTF_
            ks_log(KS_LOG_DEBUG, "Delete node: LOCKING bucket %s\n",  ks_dhtrt_printableid(header->mask, buf));
#endif
			ks_rwl_write_lock(bucket->lock);
			s = ks_dhtrt_delete_id(bucket, node->nodeid.id);
			ks_log(KS_LOG_DEBUG, "Delete node: UNLOCKING bucket %s\n", ks_dhtrt_printableid(header->mask, buf));

			ks_rwl_write_unlock(bucket->lock);
		}
	}

	ks_rwl_read_unlock(internal->lock);   /* release write lock */
	/* at this point no subsequent find/query will return the node */

	if (s == KS_STATUS_FAIL) {
		ks_log(KS_LOG_DEBUG, "Delete node: node not found %s\n", ks_dhtrt_printableid(header->mask, buf));
		return KS_STATUS_FAIL;  /* cannot delete what we cannot find */
	}

	ks_dhtrt_queue_node_fordelete(table, node);
	return s;
}

static
ks_status_t ks_dhtrt_insert_node(ks_dhtrt_routetable_t *table, ks_dht_node_t *node, enum ks_create_node_flags_t flags)
{
    char buf[51];
	if (!table || !table->internal) {
		return KS_STATUS_FAIL;
	}

	ks_dhtrt_internal_t* internal = table->internal;
	ks_dhtrt_bucket_t *bucket = 0;

	int insanity = 0;

	ks_rwl_write_lock(internal->lock);
	ks_dhtrt_bucket_header_t *header = ks_dhtrt_find_bucketheader(table, node->nodeid.id); 
	assert(header != NULL);             /* should always find a header */ 

	bucket = header->bucket;

	if (bucket == 0) {
		ks_rwl_write_unlock(internal->lock);       
		return  KS_STATUS_FAIL;  /* we were not able to find a bucket*/
	}
#ifdef  KS_DHT_DEBUGLOCKPRINTF_
	ks_log(KS_LOG_DEBUG, "Insert node: LOCKING bucket %s\n", ks_dhtrt_printableid(header->mask, buf));
#endif

	ks_rwl_write_lock(bucket->lock);
	
	while (bucket->count == KS_DHT_BUCKETSIZE) {
		if (insanity > 3200) assert(insanity < 3200);

		/* first - seek a stale entry to eject */
		if (bucket->expired_count) {
			ks_dhtrt_bucket_entry_t* entry = ks_dhtrt_insert_id(bucket, node);

			if (entry != NULL) {
#ifdef  KS_DHT_DEBUGLOCKPRINTF_
				 ks_log(KS_LOG_DEBUG, "insert node: UNLOCKING bucket %s\n", ks_dhtrt_printableid(header->mask, buf));
#endif
				 ks_rwl_write_unlock(bucket->lock);
                 ks_rwl_write_unlock(internal->lock);
				 return KS_STATUS_SUCCESS;
			}
		}

		if ( !(header->flags & BHF_LEFT) )	{	/* only the left handside node can be split */
			ks_log(KS_LOG_DEBUG, "nodeid %s was not inserted\n", ks_dhtrt_printableid(node->nodeid.id, buf));
#ifdef  KS_DHT_DEBUGLOCKPRINTF_
			ks_log(KS_LOG_DEBUG, "Insert node: UNLOCKING bucket %s\n",  ks_dhtrt_printableid(header->mask, buf));
#endif
			ks_rwl_write_unlock(bucket->lock);
			ks_rwl_write_unlock(internal->lock);
			return KS_STATUS_FAIL;
		}
			
		/* bucket must be split */
		/* work out new mask */
		unsigned char newmask[KS_DHT_NODEID_SIZE];
		memcpy(newmask, header->mask, KS_DHT_NODEID_SIZE);

		if (newmask[KS_DHT_NODEID_SIZE-1] == 0) {  /* no more bits to shift - is this possible */
			ks_log(KS_LOG_DEBUG,"Nodeid %s was not inserted\n",	 ks_dhtrt_printableid(node->nodeid.id, buf));
#ifdef  KS_DHT_DEBUGLOCKPRINTF_
			ks_log(KS_LOG_DEBUG, "Insert node: UNLOCKING bucket %s\n", ks_dhtrt_printableid(header->mask, buf));
#endif
			ks_rwl_write_unlock(bucket->lock);
			ks_rwl_write_unlock(internal->lock);
			return KS_STATUS_FAIL;
		}

		/* shift right 1 bit */
		ks_dhtrt_shiftright(newmask);

		/* create the new bucket structures */
		ks_dhtrt_bucket_header_t *newleft  = ks_dhtrt_create_bucketheader(table->pool, header, newmask);

		newleft->bucket = ks_dhtrt_create_bucket(table->pool); 
		newleft->flags = BHF_LEFT;						 /* flag as left hand side - therefore splitable */

		ks_dhtrt_bucket_header_t *newright = ks_dhtrt_create_bucketheader(table->pool, header, header->mask);

        newright->right1bit = newleft;
        newleft->left1bit = newright;

		ks_dhtrt_split_bucket(header, newleft, newright);
		internal->header_count += 2;
		++internal->bucket_count;

		/* ok now we need to try again to see if the bucket has capacity */
		/* which bucket do care about */
		if (ks_dhtrt_ismasked(node->nodeid.id, newleft->mask)) {
			bucket = newleft->bucket;
#ifdef  KS_DHT_DEBUGLOCKPRINTF_
			ks_log(KS_LOG_DEBUG, "Insert node: UNLOCKING bucket %s\n", ks_dhtrt_printableid(header->right->mask, buf));
			ks_log(KS_LOG_DEBUG, "Insert node: LOCKING bucket %s\n", ks_dhtrt_printableid(newleft->mask, buf));
#endif

			ks_rwl_write_lock(bucket->lock);                   /* lock new bucket */
			ks_rwl_write_unlock(header->right->bucket->lock);   /* unlock old bucket */
			header = newleft;
		}
		else {
			bucket = newright->bucket;
			/* note: we still hold a lock on the bucket */
			header = newright;
		}
		++insanity;
	}

	ks_log(KS_LOG_DEBUG, "Inserting nodeid %s\n", ks_dhtrt_printableid(node->nodeid.id, buf));
	ks_log(KS_LOG_DEBUG, "  ...into bucket %s\n", buf);

	ks_status_t s = KS_STATUS_FAIL;

	ks_dhtrt_bucket_entry_t* entry = ks_dhtrt_insert_id(bucket, node);
    if (entry != NULL) {
        s = KS_STATUS_SUCCESS;
		/* touch */
		if (flags == KS_DHTRT_CREATE_TOUCH) {
			 entry->flags = DHTPEER_ACTIVE;
			 entry->touched = 1;
		}
		else if (flags == KS_DHTRT_CREATE_PING ) {
			 ks_dhtrt_ping(internal, entry);
		}
	}
	ks_rwl_write_unlock(internal->lock);
#ifdef  KS_DHT_DEBUGLOCKPRINTF_
	ks_log(KS_LOG_DEBUG, "Insert node: UNLOCKING bucket %s\n",
							ks_dhtrt_printableid(header->mask, buf));
#endif
	ks_rwl_write_unlock(bucket->lock);
	return s;
}

KS_DECLARE(ks_dht_node_t *) ks_dhtrt_find_node(ks_dhtrt_routetable_t *table, ks_dht_nodeid_t nodeid) 
{
	if (!table || !table->internal) {
		return NULL;
	}

	ks_dht_node_t* node = NULL;
	ks_dhtrt_internal_t* internal = table->internal;

	ks_rwl_read_lock(internal->lock);      /* grab read lock */

	ks_dhtrt_bucket_header_t *header = ks_dhtrt_find_bucketheader(table, nodeid.id);

	if (header != 0) {

		ks_dhtrt_bucket_t *bucket = header->bucket;

		if (bucket != 0) {			 /* probably a logic error ?*/

#ifdef  KS_DHT_DEBUGLOCKPRINTF_
			char bufx[51];
			ks_log(KS_LOG_DEBUG, "Find node: read LOCKING bucket %s\n",  ks_dhtrt_printableid(header->mask, bufx));
#endif

			ks_rwl_read_lock(bucket->lock);
			node = ks_dhtrt_find_nodeid(bucket, nodeid.id);
    
			if (node != NULL) {
				ks_rwl_read_lock(node->reflock);
			}
#ifdef  KS_DHT_DEBUGLOCKPRINTF_
			ks_log(KS_LOG_DEBUG, "Find node: read UNLOCKING bucket %s\n",  ks_dhtrt_printableid(header->mask, bufx));
#endif
			ks_rwl_read_unlock(bucket->lock);
		}

	}

	ks_rwl_read_unlock(internal->lock);
	return node;  
}

KS_DECLARE(ks_status_t) ks_dhtrt_touch_node(ks_dhtrt_routetable_t *table,  ks_dht_nodeid_t nodeid) 
{
	if (!table || !table->internal) {
		return KS_STATUS_FAIL;
	}

	ks_status_t s = KS_STATUS_FAIL;
	ks_dhtrt_internal_t* internal = table->internal;

	ks_rwl_read_lock(internal->lock);      /* grab read lock */

	ks_dhtrt_bucket_header_t *header = ks_dhtrt_find_bucketheader(table, nodeid.id);

	if (header != 0 && header->bucket != 0) {
		ks_rwl_write_lock(header->bucket->lock);
#ifdef  KS_DHT_DEBUGLOCKPRINTF_
	    char bufx[51];
		ks_log(KS_LOG_DEBUG, "Touch node: write bucket %s\n",  ks_dhtrt_printableid(header->mask, bufx));
#endif

		ks_dhtrt_bucket_entry_t *e = ks_dhtrt_find_bucketentry(header, nodeid.id);

		if (e != 0) { 
			e->tyme = ks_time_now_sec();
			e->outstanding_pings = 0;
			e->touched = 1;

			if (e->flags ==	DHTPEER_EXPIRED) {
				--header->bucket->expired_count;
			}

			e->flags = DHTPEER_ACTIVE;
			s = KS_STATUS_SUCCESS;
		}
#ifdef  KS_DHT_DEBUGLOCKPRINTF_
		ks_log(KS_LOG_DEBUG, "Touch node: UNLOCKING bucket %s\n",  ks_dhtrt_printableid(header->mask, bufx));
#endif
		ks_rwl_write_unlock(header->bucket->lock);
	}
	ks_rwl_read_unlock(internal->lock);      /* release read lock */
	return s;
}

KS_DECLARE(ks_status_t) ks_dhtrt_expire_node(ks_dhtrt_routetable_t *table,	ks_dht_nodeid_t nodeid)
{
	if (!table || !table->internal) {
		return KS_STATUS_FAIL;
	}

	ks_status_t s = KS_STATUS_FAIL;
	ks_dhtrt_internal_t *internal = table->internal;

	ks_rwl_read_lock(internal->lock);      /* grab read lock */
	ks_dhtrt_bucket_header_t *header = ks_dhtrt_find_bucketheader(table, nodeid.id);

	if (header != 0 && header->bucket != 0) {
		ks_rwl_write_lock(header->bucket->lock);   
		ks_dhtrt_bucket_entry_t *e = ks_dhtrt_find_bucketentry(header, nodeid.id);

		if (e != 0) {
			e->flags = DHTPEER_EXPIRED;
			s = KS_STATUS_SUCCESS;
		}
		ks_rwl_write_unlock(header->bucket->lock);
	}
	ks_rwl_read_unlock(internal->lock);      /* release read lock */
	return s;
}

KS_DECLARE(uint8_t) ks_dhtrt_findclosest_nodes(ks_dhtrt_routetable_t *table, ks_dhtrt_querynodes_t *query)
{
	if (!table || !table->internal) {
		return KS_STATUS_FAIL;
		query->count = 0; 
	}

	uint8_t count = 0;
	ks_dhtrt_internal_t *internal = table->internal;

	ks_rwl_read_lock(internal->lock);      /* grab read lock */
	count = ks_dhtrt_findclosest_locked_nodes(table, query);
	ks_rwl_read_unlock(internal->lock);      /* release read lock */
	return count;
}

static
uint8_t ks_dhtrt_findclosest_locked_nodes(ks_dhtrt_routetable_t *table, ks_dhtrt_querynodes_t *query) 
{
	char buf[51];
	uint8_t total = 0;
	uint8_t cnt;

	if (query->max == 0) return 0;		    	/* sanity checks */
	if (query->max > KS_DHTRT_MAXQUERYSIZE) {  /* enforce the maximum */
		query->max = KS_DHTRT_MAXQUERYSIZE;
	}

	query->count = 0;

	ks_dhtrt_bucket_header_t *header = ks_dhtrt_find_bucketheader(table, query->nodeid.id);

	ks_log(KS_LOG_DEBUG, "Finding %d closest nodes for  nodeid %s\n", 
							query->max, 
							ks_dhtrt_printableid(query->nodeid.id, buf));
	ks_log(KS_LOG_DEBUG, "   ...starting at mask: %s\n",  buf);

	ks_dhtrt_sortedxors_t xort0;
	memset(&xort0, 0 , sizeof(xort0));

	ks_dhtrt_nodeid_t initid;

	memset(initid, 0xff, KS_DHT_NODEID_SIZE);
	xort0.bheader = header;

	/* step 1 - look at immediate bucket */
	/* --------------------------------- */
	int max = query->max;
	cnt = ks_dhtrt_findclosest_bucketnodes(query->nodeid.id, query->type, query->family, header, &xort0, initid ,max);
	total += cnt;

#ifdef	KS_DHT_DEBUGPRINTF_
	ks_log(KS_LOG_DEBUG, "Bucket %s yielded %d nodes; total=%d\n",  buf, cnt, total);
#endif

	if (total >= query->max  ||
		!header->parent       ) {	 /* is query answered ?	 */
		return ks_dhtrt_load_query(query, &xort0);
	}

	/* step2 - look at sibling */
	/* ----------------------- */
	ks_dhtrt_sortedxors_t xort1;

	xort0.next = &xort1;
	memset(&xort1, 0 , sizeof(xort1));
	memcpy(initid, &xort0.hixor, KS_DHT_NODEID_SIZE);

	ks_dhtrt_bucket_header_t *parent = header->parent;

	if (header == parent->left) { 
		xort1.bheader = header = parent->right;
	} else {
		if (!parent->left->bucket) {   /* left hand might no have a bucket - if so choose left->right */ 
			xort1.bheader = header = parent->left->right;
		} else {
			xort1.bheader = header = parent->left;
		}
	}

	max = query->count - total;
	cnt = ks_dhtrt_findclosest_bucketnodes(query->nodeid.id, query->type, query->family, header, &xort1, initid ,max);
	total += cnt;

#ifdef	KS_DHT_DEBUGPRINTF_
	ks_log(KS_LOG_DEBUG," stage2: sibling bucket header %s yielded %d nodes, total=%d\n",
		   ks_dhtrt_printableid(header->mask, buf), cnt, total);
#endif

	if (total >= query->max) {	 /* is query answered ?	 */
		return ks_dhtrt_load_query(query, &xort0);
	}

	/* step3 and beyond ... work left and right until the count is satisfied */
	/* ---------------------------------------------------------------------- */
	memcpy(initid, &xort0.hixor, KS_DHT_NODEID_SIZE);
 
	unsigned char leftid[KS_DHT_NODEID_SIZE];
	unsigned char rightid[KS_DHT_NODEID_SIZE];

	memcpy(leftid, xort0.bheader->mask, KS_DHT_NODEID_SIZE);
	memcpy(rightid, xort1.bheader->mask, KS_DHT_NODEID_SIZE);

	int insanity = 0;
	ks_dhtrt_bucket_header_t *lheader = 0; 
	ks_dhtrt_bucket_header_t *rheader = 0;
	ks_dhtrt_bucket_header_t *last_rheader = 0;
	ks_dhtrt_bucket_header_t *last_lheader = 0;
	ks_dhtrt_sortedxors_t *prev = &xort1;
	ks_dhtrt_sortedxors_t *tofree = 0;
	ks_dhtrt_sortedxors_t *xortn;
	ks_dhtrt_sortedxors_t *xortn1;

	do {
		last_lheader = lheader;
		lheader = 0;
		last_rheader = rheader;
		rheader = 0;
		xortn = 0;
		xortn1 = 0;

		if (leftid[0] != 0xff) { 

			ks_dhtrt_shiftleft(leftid);

			if (last_lheader && last_lheader->left1bit) {
				lheader = last_lheader->left1bit = ks_dhtrt_find_relatedbucketheader(last_lheader->left1bit, leftid);
			}
			else {
				lheader = ks_dhtrt_find_bucketheader(table, leftid);
				if (last_lheader) {
					last_lheader->left1bit = lheader;    /* remember so we can take a shortcut next query */
				} 
			}

			if (lheader) {		  
				xortn = ks_pool_alloc(table->pool, sizeof(ks_dhtrt_sortedxors_t));

				if (tofree == 0) {
					tofree = xortn;
				}

				prev->next = xortn;
				prev = xortn;
			    max = query->max - total;
				cnt = ks_dhtrt_findclosest_bucketnodes(query->nodeid.id, query->type, query->family, 
															lheader, xortn, leftid ,max);
				total += cnt;
#ifdef	KS_DHT_DEBUGPRINTF_
				ks_log(KS_LOG_DEBUG," stage3: seaching left bucket header %s yielded %d nodes, total=%d\n",
					   ks_dhtrt_printableid(lheader->mask, buf), cnt, total);
#endif
			}
#ifdef  KS_DHT_DEBUGPRINTF_
			else {
				ks_log(KS_LOG_DEBUG," stage3: failed to find left header %s\n",
						ks_dhtrt_printableid(leftid, buf));
			}
#endif

		}

		if (rightid[KS_DHT_NODEID_SIZE-1] != 0x00) {

			ks_dhtrt_shiftright(rightid);

			if (last_rheader && last_rheader->right1bit) {
				rheader = last_rheader->right1bit = ks_dhtrt_find_relatedbucketheader(last_rheader->right1bit, rightid);
			}
			else {
				rheader = ks_dhtrt_find_bucketheader(table, rightid);
				if (rheader == last_rheader) {    /* did we get the same bucket header returned */
					rheader = 0;                  /* yes: we are done on the left hand branch   */
				}
				else {
					if (last_rheader) {
						last_rheader->left1bit = rheader;    /* remember so we can take a shortcut next query */
					}
				}
			}

			if (rheader) {
				xortn1 = ks_pool_alloc(table->pool, sizeof(ks_dhtrt_sortedxors_t));

				if (tofree == 0) {
					tofree = xortn1;
				}

				prev->next = xortn1;
				prev = xortn1;
				max = query->max - total;
				cnt = ks_dhtrt_findclosest_bucketnodes(query->nodeid.id, query->type, query->family,
															rheader, xortn1, rightid , max);
				total += cnt;
#ifdef	KS_DHT_DEBUGPRINTF_
				ks_log(KS_LOG_DEBUG," stage3: seaching right bucket header %s yielded %d nodes, total=%d\n", 
					   ks_dhtrt_printableid(rheader->mask, buf), cnt, total);
#endif
			}
#ifdef  KS_DHT_DEBUGPRINTF_
			else {
				ks_log(KS_LOG_DEBUG," stage3: failed to find right header %s\n",
					ks_dhtrt_printableid(rightid, buf));
			}
#endif

		}
	   
		if (!lheader && !rheader) {
			break;
		}
	
		++insanity;

		if (insanity > 159) {
			assert(insanity <= 159);
		}
	} while (total < query->max);


	ks_dhtrt_load_query(query, &xort0);

	/* free up the xort structs on heap */
	while (tofree) {
		ks_dhtrt_sortedxors_t *x = tofree->next;

		ks_pool_free(table->pool, &tofree);
		tofree = x;
	}

	return query->count;
}

KS_DECLARE(ks_status_t) ks_dhtrt_release_node(ks_dht_node_t* node)
{
	assert(node);
	return ks_rwl_read_unlock(node->reflock);
}

KS_DECLARE(ks_status_t) ks_dhtrt_sharelock_node(ks_dht_node_t* node)
{
	assert(node);
	return ks_rwl_read_lock(node->reflock);
}

KS_DECLARE(ks_status_t) ks_dhtrt_release_querynodes(ks_dhtrt_querynodes_t *query)
{
	for(int ix=0; ix<query->count; ++ix) {
		ks_rwl_read_unlock(query->nodes[ix]->reflock);
	}
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(void)  ks_dhtrt_process_table(ks_dhtrt_routetable_t *table)
{
	/* walk the table and update the status of all known knodes */
	/* anything that is suspect automatically becomes expired	*/

	/* inactive for 15 minutes, a node becomes quesionable */
	/*	it should be pinged */

	/* if it has not been 'touched' since the last time */
	/*	give it one more try							*/

	/* inactive again it is considered inactive			*/
	/*													*/

	char buf[51];

	if (!table || !table->internal) {
		return;
	}

	ks_dhtrt_internal_t *internal = table->internal;
	int ping_count  = 0;
    int ping2_count = 0;

	ks_time_t t0 = ks_time_now_sec();

	if (t0 - internal->last_process_table < internal->next_process_table_delta) {
        /*printf("process table: next scan not scheduled\n");*/
		return;  
	} 

	internal->last_process_table = t0;

	ks_log(KS_LOG_DEBUG,"process_table in progress\n");

	ks_rwl_read_lock(internal->lock);      /* grab read lock */

	ks_dhtrt_bucket_header_t *header = internal->buckets;
	ks_dhtrt_bucket_header_t *stack[KS_DHT_NODEID_SIZE * 8];
	int stackix=0;

	while (header) {
		stack[stackix++] = header;

		if (header->bucket) {

			ks_dhtrt_bucket_t *b = header->bucket;

			if (ks_rwl_try_write_lock(b->lock) == KS_STATUS_SUCCESS) {

#ifdef  KS_DHT_DEBUGLOCKPRINTF_
				ks_log(KS_LOG_DEBUG,"process_table: LOCKING bucket %s\n", ks_dhtrt_printableid(header->mask, buf));
#endif


				if (b->count == 0) {

				    if (t0 - b->findtyme >= (ks_time_t)KS_DHTRT_EXPIREDTIME) { /* bucket has been empty for a while */

                        ks_dht_nodeid_t targetid;

						if (header->right1bit) {
                            ks_dhtrt_midmask(header->mask, header->right1bit->mask, targetid.id);
						}
						else {
							ks_dhtrt_nodeid_t rightid;
							memcpy(rightid, header->mask, KS_DHT_NODEID_SIZE);                          
							ks_dhtrt_shiftright(rightid);
							ks_dhtrt_midmask(header->mask, rightid, targetid.id);
						}

                        ks_dhtrt_find(table, internal, &targetid);
                        b->findtyme = t0;
					}
				}
				else {
					for (int ix=0; ix<KS_DHT_BUCKETSIZE; ++ix) {
						ks_dhtrt_bucket_entry_t *e =  &b->entries[ix];

						if (e->inuse == 1) {

                            ks_time_t tdiff = t0 - e->tyme;  

							if (e->gptr->type != KS_DHT_LOCAL) {   /* 'local' nodes do not get expired */

								/* more than n pings outstanding? */

								if (e->flags == DHTPEER_DUBIOUS) {
									continue;                     /* nothin' to see here */
								}

                                /* refresh empty buckets */
								if ( e->flags != DHTPEER_EXPIRED             && 
                                     tdiff >= KS_DHTRT_EXPIREDTIME           &&        /* beyond expired time */ 
									 e->outstanding_pings >= KS_DHTRT_MAXPING ) {      /* has been retried    */ 
									ks_log(KS_LOG_DEBUG,"process_table: expiring node %s\n", 
															ks_dhtrt_printableid(e->gptr->nodeid.id, buf));
									e->flags =	DHTPEER_EXPIRED; 
									++b->expired_count;
									e->outstanding_pings = 0;     /* extinguish all hope: do not retry again */ 
									continue;
								}

								/* re ping in-doubt nodes */
								if ( e->outstanding_pings > 0) {
									ks_time_t tping = t0 - e->ping_tyme;         /* time since we last pinged */

                                    if (e->outstanding_pings == KS_DHTRT_MAXPING - 1) {  /* final ping */ 
										ks_dhtrt_ping(internal, e);
										e->ping_tyme = t0;
										++ping2_count;
									}
									else if (tping >=  KS_DHTRT_PROCESSTABLE_SHORTINTERVAL120) {
										ks_dhtrt_ping(internal, e);
										e->ping_tyme = t0;
										++ping_count;
									}
                                    continue;
								} 

								/* look for newly expired nodes */
								if (tdiff > KS_DHTRT_EXPIREDTIME) {       
									e->flags = DHTPEER_DUBIOUS;               /* mark as dubious          */
									ks_dhtrt_ping(internal, e);               /* final effort to activate */
									e->ping_tyme = t0;
									continue;                                 
								}

								if (tdiff > KS_DHTRT_INACTIVETIME) {          /* inactive for suspicious length */
									ks_dhtrt_ping(internal, e);               /* kick                           */
									e->ping_tyme = t0;
									++ping_count;                            
									continue;
								}

							} /* end if not local */

						}  /* end if e->inuse */

					}	/* end for each bucket_entry */
				}    /* if bucket->count == 0 .... else */            

#ifdef  KS_DHT_DEBUGLOCKPRINTF_
				char buf[100];
				ks_log(KS_LOG_DEBUG,"process_table: UNLOCKING bucket %s\n", ks_dhtrt_printableid(header->mask, buf));
#endif

				ks_rwl_write_unlock(b->lock);

			}   /* end of if trywrite_lock successful */
			else {
#ifdef  KS_DHT_DEBUGPRINTF_
				char buf1[100];
				ks_log(KS_LOG_DEBUG,"process_table: unble to LOCK bucket %s\n", ks_dhtrt_printableid(header->mask, buf1));
#endif
			}
		}

		header = header->left;

		if (header == 0 && stackix > 1) {
			stackix -= 2;
			header =  stack[stackix];
			header = header->right;
		}
	}
	ks_rwl_read_unlock(internal->lock);      /* release read lock */

	ks_dhtrt_process_deleted(table, 0);

	if (ping2_count > 0) {
		internal->next_process_table_delta = KS_DHTRT_PROCESSTABLE_SHORTINTERVAL60;
	}
    if (ping_count > 0) {
        internal->next_process_table_delta = KS_DHTRT_PROCESSTABLE_SHORTINTERVAL120;
    }
	else {
		internal->next_process_table_delta = KS_DHTRT_PROCESSTABLE_INTERVAL;
	}
	ks_log(KS_LOG_DEBUG,"process_table complete\n");

	return;
}

void ks_dhtrt_process_deleted(ks_dhtrt_routetable_t *table, int8_t all)
{
	ks_dhtrt_internal_t* internal = table->internal;
	ks_mutex_lock(internal->deleted_node_lock);

	ks_dhtrt_deletednode_t *deleted = internal->deleted_node;
	ks_dhtrt_deletednode_t *prev = NULL, *temp=NULL;

#ifdef  KS_DHT_DEBUGPRINTF_
	ks_log(KS_LOG_DEBUG, "ALLOC process_deleted entry: internal->deleted_count %d\n", internal->deleted_count);
#endif


	/* reclaim excess memory */
	uint32_t threshold = KS_DHTRT_RECYCLE_NODE_THRESHOLD;

	if (all) {
		 threshold = 0;
	}

	while(internal->deleted_count > threshold  && deleted) {
		ks_dht_node_t* node = deleted->node;

#ifdef  KS_DHT_DEBUGPRINTFX_
		ks_log(KS_LOG_DEBUG, "ALLOC process_deleted : try write lock\n");
#endif

		if (ks_rwl_try_write_lock(node->reflock) == KS_STATUS_SUCCESS) {        
			ks_rwl_destroy(&(node->reflock));
			ks_pool_free(table->pool, &node);
			temp = deleted;
			deleted = deleted->next;
			ks_pool_free(table->pool, &temp);
			--internal->deleted_count;
#ifdef  KS_DHT_DEBUGPRINTFX__
			ks_log(KS_LOG_DEBUG, "ALLOC process_deleted: internal->deleted_count reduced to %d\n", internal->deleted_count);
#endif
			if (prev != NULL) {
				prev->next = deleted;
			}
			else {
				internal->deleted_node = deleted;
			}   
     
		}
		else {
#ifdef  KS_DHT_DEBUGPRINTF_
			ks_log(KS_LOG_DEBUG, "ALLOC process_deleted : try write lock failed\n");
#endif
			prev = deleted;
			deleted = prev->next;
		}
	}

#ifdef  KS_DHT_DEBUGPRINTF_
	ks_log(KS_LOG_DEBUG, "ALLOC process_deleted exit: internal->deleted_count %d\n", internal->deleted_count);
#endif

	ks_mutex_unlock(internal->deleted_node_lock);
}


KS_DECLARE(void) ks_dhtrt_dump(ks_dhtrt_routetable_t *table, int level) {
	/* dump buffer headers */
	char buffer[100];
	memset(buffer, 0, 100);
	ks_dhtrt_internal_t *internal = table->internal;
	ks_dhtrt_bucket_header_t *header = internal->buckets;
	ks_dhtrt_bucket_header_t *stack[KS_DHT_NODEID_SIZE * 8];
	int stackix = 0;

	ks_rwl_read_lock(internal->lock);      /* grab read lock */
	while (header) {
		stack[stackix++] = header;
		/* walk and report left handsize */
		memset(buffer, 0, 100);
		ks_log(KS_LOG_DEBUG, "bucket header: [%s]\n", ks_dhtrt_printableid(header->mask, buffer) );

		if (header->bucket) {
			ks_dhtrt_bucket_t *b = header->bucket;
			ks_log(KS_LOG_DEBUG, " bucket holds %d entries\n", b->count);
			 
			if (b->count > 0 && level == 7) {
				ks_log(KS_LOG_DEBUG, "   --------------------------\n");

				for (int ix=0; ix<KS_DHT_BUCKETSIZE; ++ix) {
					memset(buffer, 0, 100);
					if (b->entries[ix].inuse == 1) {
						ks_dhtrt_printableid(b->entries[ix].gptr->nodeid.id, buffer);
						ks_dht_node_t *n = b->entries[ix].gptr;
						ks_log(KS_LOG_DEBUG, "     slot %d: flags:%d %d type:%d family:%d %s\n", ix,
												b->entries[ix].flags,
												b->entries[ix].outstanding_pings,
												n->type,
												n->addr.family,
												buffer);
					}
					else {
							ks_log(KS_LOG_DEBUG, "	   slot %d: <free>\n", ix); 
					}
				}

				ks_log(KS_LOG_DEBUG, "   --------------------------\n\n");
			}
	
		}	

		header = header->left;

		if (header == 0 && stackix > 1) {
			stackix -= 2; 
			header =  stack[stackix];
			header = header->right;
		}
	}			 
	ks_rwl_read_unlock(internal->lock);      /* release read lock */
	return;
}

/* 
   internal functions 
*/

static
ks_dhtrt_bucket_header_t *ks_dhtrt_create_bucketheader(ks_pool_t *pool, ks_dhtrt_bucket_header_t *parent, uint8_t *mask) 
{
	ks_dhtrt_bucket_header_t *header = ks_pool_alloc(pool, sizeof(ks_dhtrt_bucket_header_t));

	memcpy(header->mask, mask, sizeof(header->mask));  
	header->parent = parent;   

#ifdef KS_DHT_DEBUGPRINTF_
	char buffer[100];
	ks_log(KS_LOG_DEBUG, "creating bucket header for mask: %s\n", ks_dhtrt_printableid(mask, buffer));
	if (parent) {
		 ks_log(KS_LOG_DEBUG, "  ... from parent mask: %s\n",  ks_dhtrt_printableid(parent->mask, buffer));
	}
#endif
	return header;
}

static
ks_dhtrt_bucket_t *ks_dhtrt_create_bucket(ks_pool_t *pool)
{
	ks_dhtrt_bucket_t *bucket = ks_pool_alloc(pool, sizeof(ks_dhtrt_bucket_t));
	ks_rwl_create(&bucket->lock, pool);
	return bucket;
}

static
ks_dhtrt_bucket_header_t *ks_dhtrt_find_bucketheader(ks_dhtrt_routetable_t *table, ks_dhtrt_nodeid_t id) 
{
	/* find the right bucket.  
	   if a bucket header has a bucket, it does not	 children
	   so it must be the bucket to use	  
	*/
	ks_dhtrt_internal_t *internal = table->internal;
	ks_dhtrt_bucket_header_t *header = internal->buckets;

	while (header) {
		if ( header->bucket ) {
			return header;
		}	

		/* left hand side is more restrictive (closer) so should be tried first */
		if (header->left != 0 && (ks_dhtrt_ismasked(id, header->left->mask))) {
			header = header->left;	
		} else {
			header = header->right;
		}
	} 

	return NULL;
}

static
ks_dhtrt_bucket_header_t *ks_dhtrt_find_relatedbucketheader(ks_dhtrt_bucket_header_t *header, ks_dhtrt_nodeid_t id)
{
	/*
		using the passed bucket header as a starting point find the right bucket.
		This is a shortcut used in query to shorten the search path for queries extending beyond a single bucket.
	*/

	while (header) {
		if ( header->bucket ) {
			return header;
		}

		/* left hand side is more restrictive (closer) so should be tried first */
		if (header->left != 0 && (ks_dhtrt_ismasked(id, header->left->mask))) {
			header = header->left;
		} else {
			header = header->right;
		}
	}
	return NULL;
}



static
ks_dhtrt_bucket_entry_t *ks_dhtrt_find_bucketentry(ks_dhtrt_bucket_header_t *header, ks_dhtrt_nodeid_t nodeid) 
{
	ks_dhtrt_bucket_t *bucket = header->bucket;

	if (bucket == 0)  return NULL;

	for (int ix=0; ix<KS_DHT_BUCKETSIZE; ++ix) {

		if ( bucket->entries[ix].inuse == 1	  &&
			 (!memcmp(nodeid, bucket->entries[ix].gptr->nodeid.id, KS_DHT_NODEID_SIZE)) ) {
			return &(bucket->entries[ix]);
		}
	}

	return NULL;
}

 
static
void ks_dhtrt_split_bucket(ks_dhtrt_bucket_header_t *original,
						   ks_dhtrt_bucket_header_t *left, 
						   ks_dhtrt_bucket_header_t *right) 
{
	/* so split the bucket in two based on the masks in the new header */
	/* the existing bucket - with the remaining ids will be taken by the right hand side */

	ks_dhtrt_bucket_t *source = original->bucket;
	ks_dhtrt_bucket_t *dest	  = left->bucket;
	
	int lix = 0;
	int rix = 0;

	for ( ; rix<KS_DHT_BUCKETSIZE; ++rix) {

		if (ks_dhtrt_ismasked(source->entries[rix].gptr->nodeid.id, left->mask)) {

			/* move it to the left */
			memcpy(&dest->entries[lix], &source->entries[rix], sizeof(ks_dhtrt_bucket_entry_t));
			++lix;
			++dest->count;
			
			/* now remove it from the original bucket */		
			source->entries[rix].inuse = 0;
			--source->count;
		}
	}

	/* give original bucket to the new left hand side header */
	right->bucket = source;
	original->bucket = 0;
	original->left = left;
	original->right = right;
#ifdef	KS_DHT_DEBUGPRINTF_
	char buffer[100];
	ks_log(KS_LOG_DEBUG, "\nsplitting bucket orginal: %s\n", ks_dhtrt_printableid(original->mask, buffer));
	ks_log(KS_LOG_DEBUG, " into (left) mask: %s size: %d\n", ks_dhtrt_printableid(left->mask, buffer), left->bucket->count);
	ks_log(KS_LOG_DEBUG, " and (right) mask: %s size: %d\n", ks_dhtrt_printableid(right->mask, buffer), right->bucket->count);
#endif
	return;
}


/*
 *	 buckets are implemented as static array 
 *	 There does not seem to be any advantage in sorting/tree structures in terms of xor math
 *	  so at least the static array does away with the need for locking.
 */
static 
ks_dhtrt_bucket_entry_t* ks_dhtrt_insert_id(ks_dhtrt_bucket_t *bucket, ks_dht_node_t *node)
{
	/* sanity checks */
	if (!bucket || bucket->count > KS_DHT_BUCKETSIZE) {
		assert(0);
	}

	uint8_t free = KS_DHT_BUCKETSIZE;
	uint8_t expiredix = KS_DHT_BUCKETSIZE;
	
	/* find free .. but also check that it is not already here! */
	uint8_t ix = 0;

	for (; ix<KS_DHT_BUCKETSIZE; ++ix)	{

		if (bucket->entries[ix].inuse == 0) {

			if (free == KS_DHT_BUCKETSIZE) {
				free = ix; /* use this one	 */
			}

		}
		else if (free == KS_DHT_BUCKETSIZE && bucket->entries[ix].flags == DHTPEER_EXPIRED) {
			expiredix = ix;
		}

		else if (!memcmp(bucket->entries[ix].gptr->nodeid.id, node->nodeid.id, KS_DHT_NODEID_SIZE)) {
#ifdef	KS_DHT_DEBUGPRINTF_
			char buffer[100];
			ks_log(KS_LOG_DEBUG, "duplicate peer %s found at %d\n", ks_dhtrt_printableid(node->nodeid.id, buffer), ix);
#endif
			bucket->entries[ix].tyme = ks_time_now_sec();
			return &bucket->entries[ix];  /* already exists : leave flags unchanged */
		}
	}

	if (free == KS_DHT_BUCKETSIZE && expiredix<KS_DHT_BUCKETSIZE ) {
		/* bump this one - but only if we have no other option */
		free =	expiredix;
		--bucket->expired_count;
	}
	
	if ( free<KS_DHT_BUCKETSIZE ) {
		bucket->entries[free].inuse = 1;
		bucket->entries[free].gptr = node;
		bucket->entries[free].tyme = ks_time_now_sec();
		bucket->entries[free].flags = DHTPEER_DUBIOUS;

		if (free != expiredix) {  /* are we are taking a free slot rather than replacing an expired node? */
			++bucket->count;       /* yes: increment total count */
		}

		memcpy(bucket->entries[free].gptr->nodeid.id, node->nodeid.id, KS_DHT_NODEID_SIZE);
#ifdef	KS_DHT_DEBUGPRINTF_
		char buffer[100];
		ks_log(KS_LOG_DEBUG, "Inserting node %s at %d\n",  ks_dhtrt_printableid(node->nodeid.id, buffer), free);
#endif	
		return &bucket->entries[free];
	}

	return NULL;
}
	 
static
ks_dht_node_t *ks_dhtrt_find_nodeid(ks_dhtrt_bucket_t *bucket, ks_dhtrt_nodeid_t id) 
{
#ifdef	KS_DHT_DEBUGPRINTF_
	char buffer[100];
	ks_log(KS_LOG_DEBUG, "Find nodeid for: %s\n",	 ks_dhtrt_printableid(id, buffer));
#endif

	
	for (int ix=0; ix<KS_DHT_BUCKETSIZE; ++ix) {
#ifdef	KS_DHT_DEBUGPRINTFX_
		char bufferx[100];
		if ( bucket->entries[ix].inuse == 1 && bucket->entries[ix].flags == DHTPEER_ACTIVE ) {
			ks_log(KS_LOG_DEBUG, "bucket->entries[%d].id = %s inuse=%x\n", ix,
						ks_dhtrt_printableid(bucket->entries[ix].id, bufferx),
						bucket->entries[ix].inuse  );
		}
#endif	  
		if ( bucket->entries[ix].inuse == 1              	&& 
             bucket->entries[ix].flags == DHTPEER_ACTIVE    &&
			 (!memcmp(id, bucket->entries[ix].gptr->nodeid.id, KS_DHT_NODEID_SIZE)) ) {
			return bucket->entries[ix].gptr;
		}
	}
	return NULL;
}

static
ks_status_t ks_dhtrt_delete_id(ks_dhtrt_bucket_t *bucket, ks_dhtrt_nodeid_t id)
{
#ifdef	KS_DHT_DEBUGPRINTF_
	char buffer[100];
	ks_log(KS_LOG_DEBUG, "deleting node for: %s\n",	 ks_dhtrt_printableid(id, buffer));
#endif

	for (int ix=0; ix<KS_DHT_BUCKETSIZE; ++ix) {
#ifdef	KS_DHT_DEBUGPRINTFX_
		char bufferx[100];
		ks_log(KS_LOG_DEBUG, "bucket->entries[%d].id = %s inuse=%c\n", ix,
						ks_dhtrt_printableid(bucket->entries[ix].gptr->nodeid.id, bufferx),
						bucket->entries[ix].inuse  );
#endif
		if ( bucket->entries[ix].inuse == 1	  &&
			 (!memcmp(id, bucket->entries[ix].gptr->nodeid.id, KS_DHT_NODEID_SIZE)) ) {
			bucket->entries[ix].inuse = 0;
			bucket->entries[ix].gptr  = 0;
			bucket->entries[ix].flags = 0;
			--bucket->count;
			return KS_STATUS_SUCCESS;
		}
	}
	return KS_STATUS_FAIL;
}


static
uint8_t ks_dhtrt_findclosest_bucketnodes(ks_dhtrt_nodeid_t id,
											enum ks_dht_nodetype_t type,
											enum ks_afflags_t family,
											ks_dhtrt_bucket_header_t *header,
											ks_dhtrt_sortedxors_t *xors,
											unsigned char *hixor,	  /*todo: remove */
											unsigned int max) {
	 
	uint8_t count = 0;	 /* count of nodes added this time */
	xors->startix = KS_DHT_BUCKETSIZE;
	xors->count = 0;
	xors->bheader = header;
	unsigned char xorvalue[KS_DHT_NODEID_SIZE];
	 
	/* just ugh! - there must be a better way to do this */
	/* walk the entire bucket calculating the xor value on the way */
	/* add valid & relevant entries to the xor values	*/
	ks_dhtrt_bucket_t *bucket = header->bucket;

	if (bucket == 0)  {		   /* sanity */
#ifdef	KS_DHT_DEBUGPRINTF_
		char buf[100];
		ks_log(KS_LOG_DEBUG, "closestbucketnodes: intermediate tree node found %s\n", 
					ks_dhtrt_printableid(header->mask, buf));
#endif
		
	}

	ks_rwl_read_lock(bucket->lock);    /* get a read lock : released in load_query when the results are copied */
#ifdef  KS_DHT_DEBUGLOCKPRINTF_
	char buf[100];
	ks_log(KS_LOG_DEBUG, "closestbucketnodes: LOCKING bucket %s\n",
					ks_dhtrt_printableid(header->mask, buf));
#endif
    

	for (uint8_t ix=0; ix<KS_DHT_BUCKETSIZE; ++ix) {

		if ( bucket->entries[ix].inuse == 1                                       &&    /* in use      */
			bucket->entries[ix].flags == DHTPEER_ACTIVE                           &&    /* not dubious or expired */
			(family == ifboth || bucket->entries[ix].gptr->addr.family == family) &&    /* match if family */
			(bucket->entries[ix].gptr->type & type) )                             {     /* match type   */
		  
			/* calculate xor value */
			ks_dhtrt_xor(bucket->entries[ix].gptr->nodeid.id, id, xorvalue );
		   
			/* do we need to hold this one */
			if ( count < max	||								   /* yes: we have not filled the quota yet */
				(memcmp(xorvalue, hixor, KS_DHT_NODEID_SIZE) < 0)) {	/* or is closer node than one already selected */
			   
				/* now sort the new xorvalue into the results structure	 */
				/* this now becomes worst case O(n*2) logic - is there a better way */
				/* in practice the bucket size is fixed so actual behavior is proably 0(logn) */
				unsigned int xorix = xors->startix;		  /* start of ordered list */
				unsigned int prev_xorix = KS_DHT_BUCKETSIZE;
				 
				for (int ix2=0; ix2<count; ++ix2) {

					if (memcmp(xorvalue, xors->xort[xorix].xor, KS_DHT_NODEID_SIZE) > 0) {
						break;			  /* insert before xorix, after prev_xoris */
					}
  
					prev_xorix = xorix;
					xorix = xors->xort[xorix].nextix;
				}
		   
				/* insert point found
				   count -> array slot to added newly identified node
				   insert_point -> the array slot before which we need to insert the newly identified node
				*/
				memcpy(xors->xort[count].xor, xorvalue, KS_DHT_NODEID_SIZE);
				xors->xort[count].ix = ix;
				
				xors->xort[count].nextix = xorix;			 /* correct forward chain */

				if (prev_xorix < KS_DHT_BUCKETSIZE) {		 /* correct backward chain */
					xors->xort[prev_xorix].nextix = count;
				} else {
					xors->startix = count;
				}
				++count;
			}
		}
	}

	xors->count = count; 
	return count;	/* return count of added nodes */
}

static
uint8_t ks_dhtrt_load_query(ks_dhtrt_querynodes_t *query, ks_dhtrt_sortedxors_t *xort) 
{
	ks_dhtrt_sortedxors_t *current = xort;
	uint8_t loaded = 0;

	while (current) {
#ifdef	KS_DHT_DEBUGPRINTF_
		char buf[100];
		ks_log(KS_LOG_DEBUG, "  loadquery from bucket %s count %d\n",	 
					ks_dhtrt_printableid(current->bheader->mask,buf), current->count);
#endif
		int xorix = current->startix; 

		for (uint8_t ix = 0;
				ix< current->count && loaded < query->max && xorix !=  KS_DHT_BUCKETSIZE;
				++ix )	 {
			unsigned int z =  current->xort[xorix].ix;
			query->nodes[ix] = current->bheader->bucket->entries[z].gptr;
			xorix =  current->xort[xorix].nextix;
			++loaded;
		}

#ifdef  KS_DHT_DEBUGLOCKPRINTF_
		char buf1[100];
		ks_log(KS_LOG_DEBUG, "load_query: UNLOCKING bucket %s\n",
						ks_dhtrt_printableid(current->bheader->mask, buf1));
#endif
		ks_rwl_read_unlock(current->bheader->bucket->lock); /* release the read lock from findclosest_bucketnodes */
			
		if (loaded >= query->max) break;
		current = current->next;
	}
	query->count = loaded;

	return loaded;
}

void ks_dhtrt_queue_node_fordelete(ks_dhtrt_routetable_t* table, ks_dht_node_t* node)
{
	ks_dhtrt_internal_t* internal = table->internal;
	ks_mutex_lock(internal->deleted_node_lock);
	ks_dhtrt_deletednode_t* deleted = internal->free_node_ex;   /* grab a free stub */

	if (deleted) {
		internal->free_node_ex = deleted->next;    
	}
	else {
		deleted = ks_pool_alloc(table->pool, sizeof(ks_dhtrt_deletednode_t));
	}

	deleted->node = node;
	deleted->next = internal->deleted_node;  
	internal->deleted_node = deleted;                         /* add to deleted queue */
	++internal->deleted_count;
#ifdef  KS_DHT_DEBUGPRINTF_
	ks_log(KS_LOG_DEBUG, "ALLOC: Queue for delete %d\n", internal->deleted_count);
#endif
	ks_mutex_unlock(internal->deleted_node_lock);
}

ks_dht_node_t* ks_dhtrt_make_node(ks_dhtrt_routetable_t* table)
{
	ks_dht_node_t *node = NULL;
	ks_dhtrt_internal_t *internal = table->internal;
	ks_mutex_lock(internal->deleted_node_lock);

	/* to to reuse a deleted node */
	if (internal->deleted_count) {
		ks_dhtrt_deletednode_t *deleted =  internal->deleted_node;
		node = deleted->node;                        /* take the node */
		memset(node, 0, sizeof(ks_dht_node_t));
		deleted->node = 0;                           /* avoid accidents */     
		internal->deleted_node = deleted->next; 
		deleted->next =  internal->free_node_ex;     /* save the stub for reuse */
		--internal->deleted_count; 
#ifdef  KS_DHT_DEBUGPRINTFX_
		ks_log(KS_LOG_DEBUG, "ALLOC: Reusing a node struct %d\n", internal->deleted_count);
#endif     
	}
	ks_mutex_unlock(internal->deleted_node_lock);

	if (!node) {
		node = ks_pool_alloc(table->pool, sizeof(ks_dht_node_t));
	}

	return node;
}

void ks_dhtrt_ping(ks_dhtrt_internal_t *internal, ks_dhtrt_bucket_entry_t *entry) {
	++entry->outstanding_pings;

#ifdef	KS_DHT_DEBUGPRINTF_
	char buf[100];
	ks_log(KS_LOG_DEBUG, "Ping queued for nodeid %s count %d\n",
					ks_dhtrt_printableid(entry->gptr->nodeid.id,buf), entry->outstanding_pings);
    /*printf("ping:  %s\n", buf); fflush(stdout);*/
#endif
	ks_dht_node_t* node = entry->gptr;
	ks_log(KS_LOG_DEBUG, "Node addr %s %d\n", node->addr.host, node->addr.port);
	ks_dht_ping(internal->dht, &node->addr, NULL, NULL);

	return;
}

static
void ks_dhtrt_find(ks_dhtrt_routetable_t *table, ks_dhtrt_internal_t *internal, ks_dht_nodeid_t *target) {

	char buf[100];
	ks_log(KS_LOG_DEBUG, "Find queued for target %s\n", ks_dhtrt_printableid(target->id, buf));
	ks_dht_search(internal->dht, NULL, NULL, table, target);
    return;
}

/*
  strictly for shifting the bucketheader mask 
  so format must be a right filled mask (hex: ..ffffffff)
*/
static
void ks_dhtrt_shiftright(uint8_t *id) 
{
	unsigned char b0 = 0;
	unsigned char b1 = 0;

	for (int i = KS_DHT_NODEID_SIZE-1; i >= 0; --i) {
		if (id[i] == 0) break;	  /* beyond mask- we are done */
		b1 = id[i] & 0x01;
		id[i] >>= 1;
		if (i != (KS_DHT_NODEID_SIZE-1)) {
			id[i+1] |= (b0 << 7);
		}
		b0 = b1;
	}
	return;
}

static
void ks_dhtrt_shiftleft(uint8_t *id) {

	for (int i = KS_DHT_NODEID_SIZE-1; i >= 0; --i) {
		if (id[i] == 0xff) continue;
		id[i] <<= 1;
		id[i] |= 0x01;
		break;
	}
	return;
}

static
void ks_dhtrt_midmask(uint8_t *leftid, uint8_t *rightid, uint8_t *midpt) {

    uint8_t i = 0;

    memset(midpt, 0, sizeof KS_DHT_NODEID_SIZE);

    for ( ; i < KS_DHT_NODEID_SIZE; ++i) {

        if (leftid[i] == 0 && rightid[i] == 0) {
            continue;
        }
        else if (leftid[i] == 0 || rightid[i] == 0) {
            midpt[i] = leftid[i] | rightid[i];
            continue;
        }
        else {
            if (leftid[i] == rightid[i]) {
				midpt[i] = leftid[i] >> 1;
                i++;
			}
            else { 
				uint16_t x = leftid[i] + rightid[i];
				x >>= 1;
				midpt[i++] = (uint8_t)x;
			}
			break;
		}
    }

    if (i == KS_DHT_NODEID_SIZE) {
		return;
	}

	if ( i < KS_DHT_NODEID_SIZE ) {
		memcpy(&midpt[i], &rightid[i], KS_DHT_NODEID_SIZE-i);
	}

    return;
}

/* create an xor value from two ids */
static void ks_dhtrt_xor(const uint8_t *id1, const uint8_t *id2, uint8_t *xor)
{
	for (int i = 0; i < KS_DHT_NODEID_SIZE; ++i) {
		if (id1[i] == id2[i]) {
			xor[i] = 0;
		}
		xor[i] = id1[i] ^ id2[i];
	}
	return;
}

/* is id masked by mask 1 => yes, 0=> no */
static int ks_dhtrt_ismasked(const uint8_t *id, const unsigned char *mask) 
{
	for (int i = 0; i < KS_DHT_NODEID_SIZE; ++i) {
		if (mask[i] == 0 && id[i] != 0) return 0;
		else if (mask[i] == 0xff)		return 1;
		else if (id[i] > mask[i])		return 0;
	}
	return 1;
}

static char *ks_dhtrt_printableid(uint8_t *id, char *buffer)
{
	char *t = buffer;
	memset(buffer, 0, KS_DHT_NODEID_SIZE*2); 
	for (int i = 0; i < KS_DHT_NODEID_SIZE; ++i, buffer+=2) {
		sprintf(buffer, "%02x", id[i]);
	}
	return t;
}


/*
 *
 * serialization and deserialization
 * ---------------------------------
*/

typedef struct ks_dhtrt_serialized_bucket_s 
{
	uint16_t            count;
	char               eye[4];
	ks_dhtrt_nodeid_t  id;
} ks_dhtrt_serialized_bucket_t;

typedef struct ks_dhtrt_serialized_routetable_s
{
	uint32_t           size;
	uint8_t            version;
	uint8_t            count;
	char               eye[4];
} ks_dhtrt_serialized_routetable_t;

#define DHTRT_SERIALIZATION_VERSION 1

static void ks_dhtrt_serialize_node(ks_dht_node_t *source, ks_dht_node_t *dest) 
{
	memcpy(dest, source, sizeof(ks_dht_node_t));
	memset(&dest->table, 0, sizeof(void*));
	memset(&dest->reflock, 0, sizeof(void*));      
}

static void ks_dhtrt_serialize_bucket(ks_dhtrt_routetable_t *table, 
										ks_dhtrt_serialized_routetable_t *stable,
										ks_dhtrt_bucket_header_t* header, 
										unsigned char* buffer) 
{
	uint8_t tzero = 0;
	ks_dhtrt_serialized_bucket_t *s = (ks_dhtrt_serialized_bucket_t*)buffer;

	memcpy(s->eye, "HEAD", 4);
	memcpy(s->id, header->mask, KS_DHT_NODEID_SIZE);
	buffer += sizeof(ks_dhtrt_serialized_bucket_t);
	stable->size  += sizeof(ks_dhtrt_serialized_bucket_t);

	if (header->bucket != 0) {
		ks_dhtrt_bucket_t* bucket = header->bucket;

		memcpy(&s->count, &bucket->count, sizeof(uint8_t));

		for (int i=0; i< KS_DHT_BUCKETSIZE; ++i) {
			if (bucket->entries[i].inuse == 1) {
				ks_dhtrt_serialize_node(bucket->entries[i].gptr, (ks_dht_node_t*)buffer);
				buffer += sizeof(ks_dht_node_t);
				stable->size  += sizeof(ks_dht_node_t);
			}
		}
	}
    else {
		memcpy(&s->count, &tzero, sizeof(uint8_t));
	}
}

static void ks_dhtrt_serialize_table(ks_dhtrt_routetable_t *table, 
										ks_dhtrt_serialized_routetable_t *stable,  
										unsigned char *buffer)
{
	ks_dhtrt_bucket_header_t *stack[KS_DHT_NODEID_SIZE * 8];
	int stackix=0;

	ks_dhtrt_internal_t *internal = table->internal;
	ks_dhtrt_bucket_header_t *header = internal->buckets;

	while (header) {
		stack[stackix++] = header;

		++stable->count;

		ks_dhtrt_serialize_bucket(table, stable, header, buffer);
        buffer = (unsigned char*)stable + stable->size;       
 
		header = header->left;

		if (header == 0 && stackix > 1) {
			stackix -= 2;
			header =  stack[stackix];
			header = header->right;
		}
	}
    return;
}

KS_DECLARE(uint32_t) ks_dhtrt_serialize(ks_dhtrt_routetable_t *table, void **ptr)
{
	ks_dhtrt_internal_t *internal = table->internal;    
	ks_rwl_write_lock(internal->lock);      /* grab write lock */

	uint32_t buffer_size = 3200 * sizeof(ks_dht_node_t);
	buffer_size +=  internal->header_count * sizeof(ks_dhtrt_serialized_bucket_t);
	buffer_size +=   sizeof(ks_dhtrt_serialized_routetable_t);
	unsigned char *buffer = (*ptr) = ks_pool_alloc(table->pool, buffer_size);

	ks_dhtrt_serialized_routetable_t *stable = (ks_dhtrt_serialized_routetable_t*)buffer;
	stable->size = sizeof(ks_dhtrt_serialized_routetable_t);
	stable->version = DHTRT_SERIALIZATION_VERSION;
	memcpy(stable->eye, "DHRT", 4);
    
	buffer +=  sizeof(ks_dhtrt_serialized_routetable_t);

	ks_dhtrt_serialize_table(table, stable, buffer); 
	ks_rwl_write_unlock(internal->lock);      /* write unlock */
	return stable->size;  
}


static void ks_dhtrt_deserialize_node(ks_dhtrt_routetable_t *table, 
										ks_dht_node_t *source, 
										ks_dht_node_t *dest) 
{
	memcpy(dest, source, sizeof(ks_dht_node_t));
	dest->table = table;
	ks_rwl_create(&dest->reflock, table->pool);
}


KS_DECLARE(ks_status_t) ks_dhtrt_deserialize(ks_dhtrt_routetable_t *table, void* buffer)
{
	ks_dhtrt_internal_t *internal = table->internal;
	ks_rwl_write_lock(internal->lock);      /* grab write lock */
	unsigned char *ptr = (unsigned char*)buffer; 

	ks_dhtrt_serialized_routetable_t *stable = (ks_dhtrt_serialized_routetable_t*)buffer;
	ptr += sizeof(ks_dhtrt_serialized_routetable_t);

	/* unpack and chain the buckets */
	for (int i=0; i<stable->count; ++i) {

		ks_dhtrt_serialized_bucket_t *s = (ks_dhtrt_serialized_bucket_t*)ptr;

		if (memcmp(s->eye, "HEAD", 4)) {
			assert(0);
			ks_rwl_write_unlock(internal->lock);      /* write unlock */
			return KS_STATUS_FAIL;
		}

		ptr += sizeof(ks_dhtrt_serialized_bucket_t);

		/* currently adding the nodes individually   
         * need a better way to do this that is compatible with the pending
		 * changes for supernode support
        */

		char buf[51];
		ks_log(KS_LOG_DEBUG, "deserialize bucket [%s] count %d\n", ks_dhtrt_printableid(s->id, buf), s->count);

		int mid = s->count >>1;    
		ks_dht_node_t *fnode = NULL;
		ks_dht_node_t *node = NULL;

		for(int i0=0; i0<s->count; ++i0) {
			/* recreate the node */
			ks_dht_node_t *node = ks_pool_alloc(table->pool, sizeof(ks_dht_node_t));  
			if (i0 == mid) fnode = node;
			ks_dhtrt_deserialize_node(table, (ks_dht_node_t*)ptr, node);
			ptr +=  sizeof(ks_dht_node_t);
			ks_dhtrt_insert_node(table, node, 0); 
		}

		/* 
		*	now the bucket is complete - now trigger a find.
		*	This staggers the series of finds.  We only do this for populated tables here. 
		*	Once the table is loaded, process_table will as normal start the ping/find process to
		*	update and populate the table.
		*/

		if (s->count > 0) {
			if (fnode) {
				ks_dhtrt_find(table, internal, &fnode->nodeid);
			}
			else if (node) {
				ks_dhtrt_find(table, internal, &node->nodeid);
			}
		} 
	}

	ks_rwl_write_unlock(internal->lock);      /* write unlock */
	return KS_STATUS_SUCCESS;
}


/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */

