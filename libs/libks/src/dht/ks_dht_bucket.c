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

#pragma GCC optimize ("O0")


#include "ks_dht.h"


/* change for testing */
#define KS_DHT_BUCKETSIZE 20
#define KS_DHTRT_INACTIVETIME  (5*60)	 
#define KS_DHTRT_MAXPING  3

/* peer flags */
#define DHTPEER_ACTIVE	1
#define DHTPEER_SUSPECT 2
#define DHTPEER_EXPIRED 3

typedef uint8_t ks_dhtrt_nodeid_t[KS_DHT_NODEID_SIZE];

/* internal structures */
typedef struct ks_dhtrt_bucket_entry_s {
	ks_time_t  tyme;
	uint8_t	   id[KS_DHT_NODEID_SIZE];
	ks_dht_node_t *gptr;					/* ptr to peer */	
    enum ks_dht_nodetype_t type; 
    enum ks_afflags_t  family;  
	uint8_t	   inuse;
	uint8_t	   outstanding_pings;
	uint8_t	   flags;					  /* active, suspect, expired */
} ks_dhtrt_bucket_entry_t;

typedef struct ks_dhtrt_bucket_s {
	ks_dhtrt_bucket_entry_t	 entries[KS_DHT_BUCKETSIZE];
	uint8_t		  count;
	uint8_t		  expired_count;
    ks_rwl_t     *lock;           /* lock for safe traversal of the entry array */    
} ks_dhtrt_bucket_t; 


#define BHF_LEFT 0x80

typedef struct ks_dhtrt_bucket_header_s {
	struct ks_dhtrt_bucket_header_s * parent;
	struct ks_dhtrt_bucket_header_s * left;
	struct ks_dhtrt_bucket_header_s * right;
	ks_dhtrt_bucket_t *	 bucket;
    ks_dhtrt_bucket_t *	 bucketv6;
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
	ks_rwl_t 			   *lock;  		    /* lock for safe traversal of the tree */
    ks_mutex_t             *deleted_node_lock;
    ks_dhtrt_deletednode_t *deleted_node;
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
void ks_dhtrt_xor(const uint8_t *id1, const uint8_t *id2, uint8_t *xor);
static  
int ks_dhtrt_ismasked(const uint8_t *id1, const uint8_t *mask);
static
void ks_dhtrt_queue_node_fordelete(ks_dhtrt_routetable_t *table, ks_dht_node_t* node);
static
void ks_dhtrt_process_deleted(ks_dhtrt_routetable_t *table); 

static
ks_status_t ks_dhtrt_insert_node(ks_dhtrt_routetable_t *table, ks_dht_node_t *node);
static
ks_status_t ks_dhtrt_insert_id(ks_dhtrt_bucket_t *bucket, ks_dht_node_t *node);
static
ks_status_t ks_dhtrt_delete_id(ks_dhtrt_bucket_t *bucket, ks_dhtrt_nodeid_t id);
static
char *ks_dhtrt_printableid(uint8_t *id, char *buffer);
static
unsigned char ks_dhtrt_isactive(ks_dhtrt_bucket_entry_t *entry);

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
void ks_dhtrt_ping(ks_dhtrt_bucket_entry_t *entry);



/* debugging */
#define KS_DHT_DEBUGPRINTF_
/* # define KS_DHT_DEBUGPRINTFX_  very verbose */


KS_DECLARE(ks_status_t) ks_dhtrt_initroute(ks_dhtrt_routetable_t **tableP, ks_pool_t *pool) 
{
	unsigned char initmask[KS_DHT_NODEID_SIZE];
	memset(initmask, 0xff, sizeof(initmask));

	ks_dhtrt_routetable_t *table =	 ks_pool_alloc(pool, sizeof(ks_dhtrt_routetable_t));

	ks_dhtrt_internal_t *internal =	  ks_pool_alloc(pool, sizeof(ks_dhtrt_internal_t));

	ks_rwl_create(&internal->lock, pool);
    ks_mutex_create(&internal->deleted_node_lock, KS_MUTEX_FLAG_DEFAULT, pool);
	table->internal = internal;

	/* initialize root bucket */
	ks_dhtrt_bucket_header_t *initial_header = ks_dhtrt_create_bucketheader(pool, 0, initmask);

	initial_header->flags = BHF_LEFT;	 /* fake left to allow splitting */ 
	internal->buckets = initial_header;
	initial_header->bucket =  ks_dhtrt_create_bucket(pool);
	table->pool = pool;

	*tableP = table;

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(void) ks_dhtrt_deinitroute(ks_dhtrt_routetable_t **table) 
{
	/* @todo*/

	ks_pool_t *pool = (*table)->pool;

	ks_pool_free(pool, *table);

	return;
}

KS_DECLARE(ks_status_t)	 ks_dhtrt_create_node( ks_dhtrt_routetable_t *table, 
											   ks_dht_nodeid_t nodeid,
											   enum ks_dht_nodetype_t type,	
											   char *ip,
											   unsigned short port,
											   ks_dht_node_t **node) 
{
    ks_dhtrt_internal_t* internal = table->internal;
    ks_rwl_read_lock(internal->lock);      /* grab write lock and insert */

    ks_dhtrt_bucket_header_t *header = ks_dhtrt_find_bucketheader(table, nodeid.id);
    assert(header != NULL);             /* should always find a header */

    ks_dhtrt_bucket_entry_t *bentry = ks_dhtrt_find_bucketentry(header, nodeid.id);
    if (bentry != 0) {
		bentry->type = ks_time_now_sec();
		(*node) = bentry->gptr;
		ks_rwl_read_unlock(internal->lock);
		return KS_STATUS_SUCCESS;
    }
    ks_rwl_read_unlock(internal->lock);

	/* @todo - replace with reusable memory pool */
    ks_dht_node_t *tnode = ks_pool_alloc(table->pool, sizeof(ks_dht_node_t));
	tnode->table = table;

	for (int i = 0; i < 5; ++i) {
		if (ip[i] == ':') { 
			tnode->family =	 AF_INET6; break;
		} else if (ip[i] == '.') { 
			tnode->family =	 AF_INET; break; 
		}
	}

    memcpy(tnode->nodeid.id, nodeid.id, KS_DHT_NODEID_SIZE);
    tnode->type = type;

    if (( ks_addr_set(&tnode->addr, ip, port, tnode->family) != KS_STATUS_SUCCESS) ||
        ( ks_rwl_create(&tnode->reflock, table->pool) !=  KS_STATUS_SUCCESS))       {
        ks_pool_free(table->pool, tnode);
        return KS_STATUS_FAIL;
    }

    ks_status_t s = ks_dhtrt_insert_node(table, tnode);
	
	(*node) = tnode;

	return s;
}

KS_DECLARE(ks_status_t) ks_dhtrt_delete_node(ks_dhtrt_routetable_t *table, ks_dht_node_t *node)
{
    ks_status_t s =  KS_STATUS_FAIL;
    ks_dhtrt_internal_t* internal = table->internal;
    ks_rwl_read_lock(internal->lock);      /* grab read lock */
	ks_dhtrt_bucket_header_t *header = ks_dhtrt_find_bucketheader(table, node->nodeid.id);

	if (header != 0) {
		ks_dhtrt_bucket_t *bucket = header->bucket;

		if (bucket != 0) {			 /* we found a bucket*/
			ks_rwl_write_lock(bucket->lock);
			s = ks_dhtrt_delete_id(bucket, node->nodeid.id);
			ks_rwl_write_unlock(bucket->lock);
		}

	}

    ks_rwl_read_unlock(internal->lock);   /* release write lock */
    /* at this point no subsequent find/query will return the node - so we can
       safely free it if we can grab the write lock
       Having held the write lock on the bucket we know no other thread 
       is awaiting a read/write lock on the node
    */                      

    if (ks_rwl_try_write_lock(node->reflock) ==  KS_STATUS_SUCCESS) {      /* grab exclusive lock on node */
		ks_rwl_destroy(&(node->reflock));
		ks_pool_free(table->pool, node);
    }
    else {
        ks_dhtrt_queue_node_fordelete(table, node);
	}
	return s;
}

static
ks_status_t ks_dhtrt_insert_node(ks_dhtrt_routetable_t *table, ks_dht_node_t *node)
{
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

	ks_rwl_write_lock(bucket->lock);
	
	while (bucket->count == KS_DHT_BUCKETSIZE) {
		if (insanity > 3200) assert(insanity < 3200);

		/* first - seek a stale entry to eject */
		if (bucket->expired_count) {
			ks_status_t s = ks_dhtrt_insert_id(bucket, node);
			if (s == KS_STATUS_SUCCESS) {
				 ks_rwl_write_unlock(bucket->lock);
                 ks_rwl_write_unlock(internal->lock);
				 return KS_STATUS_SUCCESS;
			}
		}

		/* 
		   todo: attempting a ping at at this point would require us
		   to suspend this process ... tricky...assume right now we will go ahead and
		   eject.  Possibly add to a list to recheck
		*/

		if ( !(header->flags & BHF_LEFT) )	{	/* only the left handside node can be split */
#ifdef	KS_DHT_DEBUGPRINTF_
			char buffer[100];
			printf(" nodeid %s was not inserted\n",	 ks_dhtrt_printableid(node->nodeid.id, buffer));
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
#ifdef	KS_DHT_DEBUGPRINTF_
			char buffer[100];
			printf(" nodeid %s was not inserted\n",	 ks_dhtrt_printableid(node->nodeid.id, buffer));
#endif
			ks_rwl_write_unlock(bucket->lock);
            ks_rwl_write_unlock(internal->lock);
			return KS_STATUS_FAIL;
		}

		/* shift right x bits : todo 1 bit for the moment */
		ks_dhtrt_shiftright(newmask);

		/* create the new bucket structures */
		ks_dhtrt_bucket_header_t *newleft  = ks_dhtrt_create_bucketheader(table->pool, header, newmask);

		newleft->bucket = ks_dhtrt_create_bucket(table->pool); 
		newleft->flags = BHF_LEFT;						 /* flag as left hand side - therefore splitable */

		ks_dhtrt_bucket_header_t *newright = ks_dhtrt_create_bucketheader(table->pool, header, header->mask);

		ks_dhtrt_split_bucket(header, newleft, newright);

		/* ok now we need to try again to see if the bucket has capacity */
		/* which bucket do care about */
		if (ks_dhtrt_ismasked(node->nodeid.id, newleft->mask)) {
			bucket = newleft->bucket;
            ks_rwl_write_lock(bucket->lock);                   /* lock new bucket */
            ks_rwl_write_unlock(header->right->bucket->lock);   /* unlock old bucket */
			header = newleft;
		} else {
			bucket = newright->bucket;
            /* note: we still hold a lock on the bucket */
			header = newright;
		}
		++insanity;
	}

#ifdef	KS_DHT_DEBUGPRINTF_	
	char buffer[100];
	printf("inserting nodeid %s ",	ks_dhtrt_printableid(node->nodeid.id, buffer));
	printf("into bucket %s\n",	ks_dhtrt_printableid(header->mask, buffer));
#endif

	/* by this point we have a viable & locked bucket
       so downgrade the internal lock to read.  safe as we hold the bucket write lock
       preventing it being sptlit under us.
    */
    ks_rwl_write_unlock(internal->lock);    
    ks_rwl_read_lock(internal->lock);

	ks_status_t s = ks_dhtrt_insert_id(bucket, node);
    ks_rwl_read_unlock(internal->lock);
    ks_rwl_write_unlock(bucket->lock);
    return s;
}

KS_DECLARE(ks_dht_node_t *) ks_dhtrt_find_node(ks_dhtrt_routetable_t *table, ks_dht_nodeid_t nodeid) 
{

	ks_dht_node_t* node = NULL;

    ks_dhtrt_internal_t* internal = table->internal;
    ks_rwl_read_lock(internal->lock);      /* grab read lock */

	ks_dhtrt_bucket_header_t *header = ks_dhtrt_find_bucketheader(table, nodeid.id);

	if (header != 0) {

		ks_dhtrt_bucket_t *bucket = header->bucket;

		if (bucket != 0) {			 /* probably a logic error ?*/

			ks_rwl_read_lock(bucket->lock);
			node = ks_dhtrt_find_nodeid(bucket, nodeid.id);
    
			if (node != NULL) {
				ks_rwl_read_lock(node->reflock);
			}

			ks_rwl_read_unlock(bucket->lock);
		}

	}

    ks_rwl_read_unlock(internal->lock);
	return node;  
}

KS_DECLARE(ks_status_t) ks_dhtrt_touch_node(ks_dhtrt_routetable_t *table,  ks_dht_nodeid_t nodeid) 
{
    ks_status_t s = KS_STATUS_FAIL;
	ks_dhtrt_internal_t* internal = table->internal;
	ks_rwl_read_lock(internal->lock);      /* grab read lock */

	ks_dhtrt_bucket_header_t *header = ks_dhtrt_find_bucketheader(table, nodeid.id);

	if (header != 0 && header->bucket != 0) {
		ks_rwl_write_lock(header->bucket->lock);
		ks_dhtrt_bucket_entry_t *e = ks_dhtrt_find_bucketentry(header, nodeid.id);

		if (e != 0) { 
			e->tyme = ks_time_now();
			e->outstanding_pings = 0;

			if (e->flags ==	 DHTPEER_EXPIRED) {
				--header->bucket->expired_count;
			}

			e->flags = DHTPEER_ACTIVE;
		    s = KS_STATUS_SUCCESS;
		}
		ks_rwl_write_unlock(header->bucket->lock);
	}
	ks_rwl_read_unlock(internal->lock);      /* release read lock */
	return s;
}

KS_DECLARE(ks_status_t) ks_dhtrt_expire_node(ks_dhtrt_routetable_t *table,	ks_dht_nodeid_t nodeid)
{
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
	uint8_t max = query->max;
	uint8_t total = 0;
	uint8_t cnt;

	if (max == 0) return 0;		    	/* sanity checks */
    if (max > KS_DHTRT_MAXQUERYSIZE) {  /* enforce the maximum */
		max = KS_DHTRT_MAXQUERYSIZE; 
        query->max = KS_DHTRT_MAXQUERYSIZE;
	}

	query->count = 0;

	ks_dhtrt_bucket_header_t *header = ks_dhtrt_find_bucketheader(table, query->nodeid.id);

#ifdef	KS_DHT_DEBUGPRINTF_
	char buffer[100];
	printf("finding %d closest nodes for  nodeid %s\n", max, ks_dhtrt_printableid(query->nodeid.id, buffer));
	printf(" starting at mask: %s\n",  ks_dhtrt_printableid(header->mask, buffer));
#endif

	ks_dhtrt_sortedxors_t xort0;
	memset(&xort0, 0 , sizeof(xort0));

	ks_dhtrt_nodeid_t initid;

	memset(initid, 0xff, KS_DHT_NODEID_SIZE);
	xort0.bheader = header;

	/* step 1 - look at immediate bucket */
	/* --------------------------------- */
	cnt = ks_dhtrt_findclosest_bucketnodes(query->nodeid.id, query->type, query->family, header, &xort0, initid ,max);
	max -= cnt;
	total += cnt;

#ifdef	KS_DHT_DEBUGPRINTF_
	printf(" bucket header %s yielded %d nodes; total=%d\n",  buffer, cnt, total);
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

	cnt = ks_dhtrt_findclosest_bucketnodes(query->nodeid.id, query->type, query->family, header, &xort1, initid ,max);
	max -= cnt;
	total += cnt;

#ifdef	KS_DHT_DEBUGPRINTF_
	printf(" stage2: sibling bucket header %s yielded %d nodes, total=%d\n",
		   ks_dhtrt_printableid(header->mask, buffer), cnt, total);
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
	ks_dhtrt_bucket_header_t *lheader; 
	ks_dhtrt_bucket_header_t *rheader;
	ks_dhtrt_sortedxors_t *prev = &xort1;
	ks_dhtrt_sortedxors_t *tofree = 0;
	ks_dhtrt_sortedxors_t *xortn;
	ks_dhtrt_sortedxors_t *xortn1;

	do {
		lheader = 0;
		rheader = 0;
		xortn = 0;
		xortn1 = 0;

		if (leftid[0] != 0xff) { 
			ks_dhtrt_shiftleft(leftid);
			lheader = ks_dhtrt_find_bucketheader(table, leftid);

			if (lheader) {		  
				xortn = ks_pool_alloc(table->pool, sizeof(ks_dhtrt_sortedxors_t));

				if (tofree == 0)   tofree = xortn;

				prev->next = xortn;
				prev = xortn;
				cnt += ks_dhtrt_findclosest_bucketnodes(query->nodeid.id, query->type, query->family, 
															lheader, xortn, leftid ,max);
				max -= cnt;
#ifdef	KS_DHT_DEBUGPRINTF_
				printf(" stage3: seaching left bucket header %s yielded %d nodes, total=%d\n",
					   ks_dhtrt_printableid(lheader->mask, buffer), cnt, total);
#endif
			}
		}

		if (max > 0 && rightid[KS_DHT_NODEID_SIZE-1] != 0x00) {
			ks_dhtrt_shiftright(rightid);
			rheader = ks_dhtrt_find_bucketheader(table, rightid);

			if (rheader) {
				xortn1 = ks_pool_alloc(table->pool, sizeof(ks_dhtrt_sortedxors_t));
				prev->next = xortn1;
				prev = xortn1;
				cnt = ks_dhtrt_findclosest_bucketnodes(query->nodeid.id, query->type, query->family,
															rheader, xortn1, rightid , max);
				max -= cnt;
#ifdef	KS_DHT_DEBUGPRINTF_
				printf(" stage3: seaching right bucket header %s yielded %d nodes, total=%d\n", 
					   ks_dhtrt_printableid(rheader->mask, buffer), cnt, total);
#endif
			}
		}
	   
		if (!lheader && !rheader) break;

		++insanity;

		if (insanity > 159) {
			assert(insanity <= 159);
		}

	} while (max < query->count);


	ks_dhtrt_load_query(query, &xort0);

	/* free up the xort structs on heap */
	while (tofree) {
		ks_dhtrt_sortedxors_t *x = tofree->next;

		ks_pool_free(table->pool, tofree);
		tofree = x;
	}

	return query->count;
}

KS_DECLARE(ks_status_t) ks_dhtrt_release_node(ks_dht_node_t* node)
{
    return ks_rwl_read_unlock(node->reflock);
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

	ks_dhtrt_internal_t *internal = table->internal;

	ks_rwl_read_lock(internal->lock);      /* grab read lock */

	ks_dhtrt_bucket_header_t *header = internal->buckets;
	ks_dhtrt_bucket_header_t *stack[KS_DHT_NODEID_SIZE * 8];
	int stackix=0;
	ks_time_t t0 = ks_time_now(); 

	while (header) {
		stack[stackix++] = header;

		if (header->bucket) {

			ks_dhtrt_bucket_t *b = header->bucket;

			if (ks_rwl_try_write_lock(b->lock) == KS_STATUS_SUCCESS) {

#ifdef  KS_DHT_DEBUGLOCKPRINTF_
        char buf[100];
        printf("process_table: LOCKING bucket %s\n",
               ks_dhtrt_printableid(header->mask, buf));
        fflush(stdout);
#endif


				for (int ix=0; ix<KS_DHT_BUCKETSIZE; ++ix) {
					ks_dhtrt_bucket_entry_t *e =  &b->entries[ix];

					if (e->inuse == 1) {

						if (e->gptr->type != KS_DHT_LOCAL) {   /* 'local' nodes do not get expired */

							/* more than n pings outstanding? */

							if (e->outstanding_pings >= KS_DHTRT_MAXPING) {
								e->flags =	DHTPEER_EXPIRED; 
								++b->expired_count;
								continue;
							}

							if (e->flags == DHTPEER_SUSPECT) {
								ks_dhtrt_ping(e); 
								continue;
							}

							ks_time_t tdiff = t0 - e->tyme;

							if (tdiff > KS_DHTRT_INACTIVETIME) {
								e->flags = DHTPEER_SUSPECT;
								ks_dhtrt_ping(e);
							}

						} /* end if not local */

					}  /* end if e->inuse */

				}	/* end for each bucket_entry */

#ifdef  KS_DHT_DEBUGLOCKPRINTF_
        char buf1[100];
        printf("process_table: UNLOCKING bucket %s\n",
               ks_dhtrt_printableid(header->mask, buf1));
        fflush(stdout);
#endif

				ks_rwl_write_unlock(b->lock);

			}   /* end of if trywrite_lock successful */
            else {
#ifdef  KS_DHT_DEBUGPRINTF_
        char buf2[100];
        printf("process_table: unble to LOCK bucket %s\n",
               ks_dhtrt_printableid(header->mask, buf2));
        fflush(stdout);
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

    ks_dhtrt_process_deleted(table);

	return;
}

void ks_dhtrt_process_deleted(ks_dhtrt_routetable_t *table)
{
	ks_dhtrt_internal_t* internal = table->internal;
	ks_mutex_lock(internal->deleted_node_lock);

	ks_dhtrt_deletednode_t *deleted = internal->deleted_node;
	ks_dhtrt_deletednode_t *prev = NULL, *temp=NULL;

	while(deleted) {
		ks_dht_node_t* node = deleted->node;

		if (ks_rwl_try_write_lock(node->reflock) == KS_STATUS_SUCCESS) {        
	        ks_rwl_destroy(&(node->reflock));
	        ks_pool_free(table->pool, node);
            temp = deleted;
            deleted = deleted->next;
            ks_pool_free(table->pool, temp);
			if (prev != NULL) {
				prev->next = deleted;
			}
            else {
                internal->deleted_node = deleted;
            }        
		}
        else {
            prev = deleted;
            deleted = prev->next;
        }
      
	}

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
		/*ks_log*/ printf("bucket header: [%s]\n", ks_dhtrt_printableid(header->mask, buffer) );

		if (header->bucket) {
			ks_dhtrt_bucket_t *b = header->bucket;
			printf("   bucket holds %d entries\n", b->count);
			 
			if (b->count > 0 && level == 7) {
				printf("   --------------------------\n");

				for (int ix=0; ix<KS_DHT_BUCKETSIZE; ++ix) {
					memset(buffer, 0, 100);
					if (b->entries[ix].inuse == 1) ks_dhtrt_printableid(b->entries[ix].id, buffer);
					else strcpy(buffer, "<free>");
					printf("	   slot %d: %s\n", ix, buffer);
				}

				printf("   --------------------------\n\n");
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
	printf("creating bucket header for mask: %s ",	ks_dhtrt_printableid(mask, buffer));
	if (parent) printf("from parent mask: %s ",	 ks_dhtrt_printableid(parent->mask, buffer));
	printf("\n"); 
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
ks_dhtrt_bucket_entry_t *ks_dhtrt_find_bucketentry(ks_dhtrt_bucket_header_t *header, ks_dhtrt_nodeid_t nodeid) 
{
	ks_dhtrt_bucket_t *bucket = header->bucket;

	if (bucket == 0)  return NULL;

	for (int ix=0; ix<KS_DHT_BUCKETSIZE; ++ix) {
#ifdef	KS_DHT_DEBUGPRINTF_
#endif
		if ( bucket->entries[ix].inuse == 1	  &&
			 (!memcmp(nodeid, bucket->entries[ix].id, KS_DHT_NODEID_SIZE)) ) {
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
		if (ks_dhtrt_ismasked(source->entries[rix].id, left->mask)) {
			/* move it to the left */
			memcpy(dest->entries[lix].id, source->entries[rix].id, KS_DHT_NODEID_SIZE);
			dest->entries[lix].gptr   = source->entries[rix].gptr;
			dest->entries[lix].family = source->entries[rix].family;
            dest->entries[lix].type   = source->entries[rix].type;
			dest->entries[lix].inuse = 1;
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
	printf("\nsplitting bucket orginal: %s\n",	ks_dhtrt_printableid(original->mask, buffer));
	printf(" into (left) mask: %s size: %d\n",	ks_dhtrt_printableid(left->mask, buffer), left->bucket->count);
	printf(" and (right) mask: %s size: %d\n\n",  ks_dhtrt_printableid(right->mask, buffer), right->bucket->count);
#endif
	return;
}


/*
 *	 buckets are implemented as static array 
 *	 There does not seem to be any advantage in sorting/tree structures in terms of xor math
 *	  so at least the static array does away with the need for locking.
 */
static 
ks_status_t ks_dhtrt_insert_id(ks_dhtrt_bucket_t *bucket, ks_dht_node_t *node)
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
		else if (!memcmp(bucket->entries[ix].id, node->nodeid.id, KS_DHT_NODEID_SIZE)) {
#ifdef	KS_DHT_DEBUGPRINTF_
			char buffer[100];
			printf("duplicate peer %s found at %d\n", ks_dhtrt_printableid(node->nodeid.id, buffer), ix);
#endif
			bucket->entries[ix].tyme = ks_time_now();
			bucket->entries[ix].flags &= DHTPEER_ACTIVE;
			return KS_STATUS_SUCCESS;  /* already exists */
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
        bucket->entries[free].type = node->type;
        bucket->entries[free].family = node->family;  
        bucket->entries[free].tyme = ks_time_now();
        bucket->entries[free].flags &= DHTPEER_ACTIVE;

		if (free !=  expiredix) {  /* are we are taking a free slot rather than replacing an expired node? */
			++bucket->count;       /* yes: increment total count */
		}

		memcpy(bucket->entries[free].id, node->nodeid.id, KS_DHT_NODEID_SIZE);
#ifdef	KS_DHT_DEBUGPRINTF_
		char buffer[100];
		printf("Inserting node %s at %d\n",  ks_dhtrt_printableid(node->nodeid.id, buffer), free);
#endif	
		return KS_STATUS_SUCCESS;
	}

	return KS_STATUS_FAIL;
}
	 
static
ks_dht_node_t *ks_dhtrt_find_nodeid(ks_dhtrt_bucket_t *bucket, ks_dhtrt_nodeid_t id) 
{
#ifdef	KS_DHT_DEBUGPRINTF_
	char buffer[100];
	printf("find nodeid for: %s\n",	 ks_dhtrt_printableid(id, buffer));
#endif

	
	for (int ix=0; ix<KS_DHT_BUCKETSIZE; ++ix) {
#ifdef	KS_DHT_DEBUGPRINTFX_
		char bufferx[100];
		if ( bucket->entries[ix].inuse == 1) {
			printf("\nbucket->entries[%d].id = %s inuse=%x\n", ix,
				   ks_dhtrt_printableid(bucket->entries[ix].id, bufferx),
				   bucket->entries[ix].inuse  );
		}
#endif	  
		if ( bucket->entries[ix].inuse == 1	  &&
			 (!memcmp(id, bucket->entries[ix].id, KS_DHT_NODEID_SIZE)) ) {
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
	printf("\ndeleting node for: %s\n",	 ks_dhtrt_printableid(id, buffer));
#endif

	for (int ix=0; ix<KS_DHT_BUCKETSIZE; ++ix) {
#ifdef	KS_DHT_DEBUGPRINTFX_
         char bufferx[100];_
		printf("\nbucket->entries[%d].id = %s inuse=%c\n", ix,
			   ks_dhtrt_printableid(bucket->entries[ix].id, bufferx),
			   bucket->entries[ix].inuse  );
#endif
		if ( bucket->entries[ix].inuse == 1	  &&
			 (!memcmp(id, bucket->entries[ix].id, KS_DHT_NODEID_SIZE)) ) {
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
		printf("closestbucketnodes: intermediate tree node found %s\n", 
			   ks_dhtrt_printableid(header->mask, buf));
#endif
		
	}

    ks_rwl_read_lock(bucket->lock);    /* get a read lock : released in load_query when the results are copied */
#ifdef  KS_DHT_DEBUGLOCKPRINTF_
        char buf[100];
        printf("closestbucketnodes: LOCKING bucket %s\n",
               ks_dhtrt_printableid(header->mask, buf));
        fflush(stdout);
#endif
    

	for (uint8_t ix=0; ix<KS_DHT_BUCKETSIZE; ++ix) {
	    if ( bucket->entries[ix].inuse == 1                              &&
             (family == ifboth || bucket->entries[ix].family == family)  &&
             (bucket->entries[ix].type & type)                           &&
             ks_dhtrt_isactive( &(bucket->entries[ix])) ) {
		  
			/* calculate xor value */
			ks_dhtrt_xor(bucket->entries[ix].id, id, xorvalue );
		   
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
		printf("  loadquery from bucket %s count %d\n",	 
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
        printf("load_query: UNLOCKING bucket %s\n",
               ks_dhtrt_printableid(current->bheader->mask, buf1));
        fflush(stdout);
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
    ks_dhtrt_deletednode_t* deleted = ks_pool_alloc(table->pool, sizeof(ks_dhtrt_deletednode_t));
    deleted->node = node; 
    ks_mutex_lock(internal->deleted_node_lock);
    deleted->next = internal->deleted_node;
    internal->deleted_node = deleted;
    ks_mutex_unlock(internal->deleted_node_lock);
}


void ks_dhtrt_ping(ks_dhtrt_bucket_entry_t *entry) {
	++entry->outstanding_pings;
	/* @todo */
	/* set the appropriate command in the node and queue if for processing */
	/*ks_dht_node_t *node = entry->gptr; */
#ifdef	KS_DHT_DEBUGPRINTF_
	char buf[100];
	printf("  ping queued for nodeid %s count %d\n",
		   ks_dhtrt_printableid(entry->id,buf), entry->outstanding_pings);
#endif
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

/* Determine whether id1 or id2 is closer to ref */

/*
  @todo: remove ?  simple memcpy seems to do the job ?

  static int
  ks_dhtrt_xorcmp(const uint8_t *id1, const uint8_t *id2, const uint8_t *ref);

  static int ks_dhtrt_xorcmp(const uint8_t *id1, const uint8_t *id2, const uint8_t *ref)
  {
  int i;
  for (i = 0; i < KS_DHT_NODEID_SIZE; i++) {
  uint8_t xor1, xor2;
  if (id1[i] == id2[i]) {
  continue;
  }
  xor1 = id1[i] ^ ref[i];
  xor2 = id2[i] ^ ref[i];
  if (xor1 < xor2) {
  return -1;		  / * id1 is closer * /
  }
  return 1;				  / * id2 is closer * /
  }
  return 0;		/ * id2 and id2 are identical ! * /
  }
*/


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

unsigned char ks_dhtrt_isactive(ks_dhtrt_bucket_entry_t *entry) 
{
	/* todo */
	return 1;
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

