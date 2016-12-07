/*
 * Copyright (c) 2016, Anthony Miiessaly II
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

#pragma GCC optimize ("O0")


#include "ks_dht_bucket.h"


/* change for testing */
#define KS_DHT_BUCKETSIZE 20
#define KS_DHTRT_INACTIVETIME  (5*60)    
#define KS_DHTRT_MAXPING  3

/* peer flags */
#define DHTPEER_ACTIVE  1
#define DHTPEER_SUSPECT 2
#define DHTPEER_EXPIRED 3

/* internal structures */
typedef struct ks_dhtrt_rw_lock_s {
   ks_pool_t* pool;   
   ks_mutex_t*  mutex;
   ks_cond_t*   rcond;
   volatile uint16_t     read_count;     
   ks_cond_t*   wcond;
   volatile uint16_t     write_count;    /* hopefully never more than 1 ! */
} ks_dhtrt_rw_lock;

typedef struct ks_dhtrt_bucket_entry_s {
    ks_time_t    tyme;
    unsigned char id[KS_DHT_IDSIZE];
    ks_dhtrt_node* gptr;                    /* ptr to peer */      
    uint8_t    inuse;
    uint8_t    outstanding_pings;
    uint8_t    flags;                     /* active, suspect, expired */
} ks_dhtrt_bucket_entry;

typedef struct ks_dhtrt_bucket_s {
    ks_dhtrt_bucket_entry  entries[KS_DHT_BUCKETSIZE];
    uint8_t       count;
    uint8_t       expired_count;
} ks_dhtrt_bucket; 


#define BHF_LEFT 0x80

typedef struct ks_dhtrt_bucket_header {
    struct ks_dhtrt_bucket_header* parent;
    struct ks_dhtrt_bucket_header* left;
    struct ks_dhtrt_bucket_header* right;
    ks_dhtrt_bucket*   bucket;
    ks_time_t        tyme;                 /* last processed time */
    unsigned char    mask[KS_DHT_IDSIZE];  /* node id mask        */
    unsigned char    flags;           
} ks_dhtrt_bucket_header;


typedef struct ks_dhtrt_internal_s {
   ks_dhtrt_bucket_header* buckets;       /* root bucketheader */
   /*   */
  
} ks_dhtrt_internal;

typedef struct ks_dhtrt_xort_s {
     unsigned int   ix;                   /* index of bucket array */	 
     unsigned char  xor[KS_DHT_IDSIZE];  /* corresponding xor value */
     unsigned int   nextix;   
} ks_dhtrt_xort;

typedef struct ks_dhtrt_sortedxors_s {
     ks_dhtrt_bucket_header* bheader;
     ks_dhtrt_xort    xort[KS_DHT_BUCKETSIZE];
     unsigned char  hixor[KS_DHT_IDSIZE];
	 unsigned int   startix;
     unsigned int   count;
     struct ks_dhtrt_sortedxors_s* next;	 
} ks_dhtrt_sortedxors;


/* --- static functions ---- */

static 
ks_dhtrt_bucket_header* ks_dhtrt_create_bucketheader(
                                    ks_pool_t *pool, 
                                    ks_dhtrt_bucket_header* parent, 
                                    unsigned char* mask);
static
ks_dhtrt_bucket* ks_dhtrt_create_bucket(ks_pool_t* pool);
static
ks_dhtrt_bucket_header* ks_dhtrt_find_bucketheader(ks_dhtrt_routetable* table, unsigned char* id);
static
ks_dhtrt_bucket_entry* ks_dhtrt_find_bucketentry(ks_dhtrt_bucket_header* header, ks_dhtrt_nodeid id);

static
void ks_dhtrt_split_bucket(ks_dhtrt_bucket_header* original, ks_dhtrt_bucket_header* left, ks_dhtrt_bucket_header* right);
static
ks_dht_node_t* ks_dhtrt_find_nodeid(ks_dhtrt_bucket* bucket, ks_dhtrt_nodeid nodeid);


static void 
ks_dhtrt_shiftright(unsigned char* id); 
static
void ks_dhtrt_shiftleft(unsigned char* id);
static int  
ks_dhtrt_xorcmp(const unsigned char *id1, const unsigned char *id2, const unsigned char *ref);
static void 
ks_dhtrt_xor(const unsigned char *id1, const unsigned char *id2, unsigned char *xor);
static int 
ks_dhtrt_ismasked(const unsigned char *id1, const unsigned char *mask);

static
ks_status_t ks_dhtrt_insert_node(ks_dhtrt_routetable* table, ks_dhtrt_node* node);
static
ks_status_t ks_dhtrt_insert_id(ks_dhtrt_bucket* bucket, ks_dhtrt_node* node);
static
void ks_dhtrt_delete_id(ks_dhtrt_bucket* bucket, ks_dhtrt_nodeid id);
static
char* ks_dhtrt_printableid(const unsigned char* id, char* buffer);
static
unsigned char ks_dhtrt_isactive(ks_dhtrt_bucket_entry* entry);
static
uint8_t ks_dhtrt_load_query(ks_dhtrt_querynodes* query, ks_dhtrt_sortedxors* xort);
static
uint8_t ks_dhtrt_findclosest_bucketnodes(unsigned char *nodeid,
                                            ks_dhtrt_bucket_header* header,
                                            ks_dhtrt_sortedxors* xors,
											unsigned char* hixor,
											unsigned int max);

static
void ks_dhtrt_ping(ks_dhtrt_bucket_entry* entry);

static
ks_status_t ks_dhtrt_initrwlock( ks_dhtrt_rw_lock* lock);
static
void ks_dhtrt_deinitrwlock( ks_dhtrt_rw_lock* lock);

static
void ks_dhtrt_getreadlock( ks_dhtrt_rw_lock* lock);
static
ks_status_t ks_dhtrt_tryreadlock( ks_dhtrt_rw_lock* lock);
static
void ks_dhtrt_releasereadlock( ks_dhtrt_rw_lock* lock);
static
void ks_dhtrt_getwritelock( ks_dhtrt_rw_lock* lock);
static
ks_status_t ks_dhtrt_trywritelock( ks_dhtrt_rw_lock* lock);
static
void ks_dhtrt_releasewritelock( ks_dhtrt_rw_lock* lock);



/* debugging */
#define KS_DHT_DEBUGPRINTF_



/*
    Public interface            
    ---------------
    ks_dhtrt_initroute
    ks_dhtrt_drinitroute

    ks_dhtrt_insertnode
 
*/

KS_DECLARE(ks_dhtrt_routetable*) ks_dhtrt_initroute( ks_pool_t *pool, ks_dhtrt_nodeid localid) 
{
    ks_log(KS_LOG_ERROR, "hello world\n");
    unsigned char initmask[KS_DHT_IDSIZE];
    memset(initmask, 0xff, sizeof(initmask));

    ks_dhtrt_routetable* table =   ks_pool_alloc(pool, sizeof(ks_dhtrt_routetable));
    memset(table, 0, sizeof(ks_dhtrt_routetable));

    ks_dhtrt_internal* internal =   ks_pool_alloc(pool, sizeof(ks_dhtrt_internal));
    memset(internal, 0, sizeof(ks_dhtrt_internal));
    table->internal = internal;

    /* initialize root bucket */
    ks_dhtrt_bucket_header* initial_header = ks_dhtrt_create_bucketheader(pool, 0, initmask);
    initial_header->flags = BHF_LEFT;    /* fake left to allow splitting */ 
    internal->buckets = initial_header;
    initial_header->bucket =  ks_dhtrt_create_bucket(pool);
    table->pool = pool;
    return table;
}

KS_DECLARE(void) ks_dhtrt_deinitroute(  ks_dhtrt_routetable* table ) 
{
    /*todo*/
    ks_pool_free(table->pool, table);
    return;
}

KS_DECLARE(ks_dhtrt_node*) ks_dhtrt_create_node(ks_dhtrt_routetable* table, ks_dhtrt_nodeid nodeid, ks_dht_node_t* node)
{
     /* first see if it exists */
     ks_dhtrt_node* peer = ks_dhtrt_find_node(table, nodeid);
     if (peer != 0)  {      /* humm not sure - this might be an error */
         return peer;
     }
     peer = ks_pool_alloc(table->pool, sizeof(ks_dhtrt_node));
     memset(peer, 0, sizeof(ks_dhtrt_node));
     memcpy(peer->id, nodeid, KS_DHT_IDSIZE);
     ks_status_t status = ks_dhtrt_insert_node(table, peer);
     if (status == KS_STATUS_FAIL) {
        ks_pool_free(table->pool, peer);
        return 0;
     }
     peer->handle = node;
     return peer;
}


KS_DECLARE(ks_status_t)   ks_dhtrt_delete_node(ks_dhtrt_routetable* table, ks_dhtrt_node* peer)
{
     ks_dhtrt_bucket_header* header = ks_dhtrt_find_bucketheader(table, peer->id);
     if (header != 0) {
        ks_dhtrt_bucket* bucket = header->bucket;
        if (bucket != 0) {           /* we were not able to find a bucket*/
            ks_dhtrt_delete_id(bucket, peer->id);
        }
     }
     ks_pool_free(table->pool, peer);
     return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_dhtrt_insert_node(ks_dhtrt_routetable* table, ks_dhtrt_node* peer)
{
    ks_dhtrt_bucket* bucket = 0;
    int insanity = 0;
 
    ks_dhtrt_bucket_header* header = ks_dhtrt_find_bucketheader(table, peer->id); 
	bucket = header->bucket;

    assert(bucket != 0);  /* we were not able to find a bucket*/
	
	while (bucket->count == KS_DHT_BUCKETSIZE) {
        if (insanity > 3200) assert(insanity < 3200);

	    /* first - seek a stale entry to eject */
        if (bucket->expired_count) {
           ks_status_t s = ks_dhtrt_insert_id(bucket, peer);
           if (s == KS_STATUS_SUCCESS) return KS_STATUS_SUCCESS;
        }

	    /* 
		   todo: attempting a ping at at this point would require us
		   to suspend this process ... tricky...assume right now we will go ahead and
		   eject.  Possibly add to a list to recheck
		*/

        if ( !(header->flags & BHF_LEFT) )  {   /* only the left handside node can be split */
#ifdef  KS_DHT_DEBUGPRINTF_
           char buffer[100];
           printf(" nodeid %s was not inserted\n",  ks_dhtrt_printableid(peer->id, buffer));
#endif
           return KS_STATUS_FAIL;
        }
        	
	    /* bucket must be split */
		/* work out new mask */
		unsigned char newmask[KS_DHT_IDSIZE];
		memcpy(newmask, header->mask, KS_DHT_IDSIZE);
        if (newmask[KS_DHT_IDSIZE-1] == 0) {  /* no more bits to shift - is this possible */
#ifdef  KS_DHT_DEBUGPRINTF_
           char buffer[100];
           printf(" nodeid %s was not inserted\n",  ks_dhtrt_printableid(peer->id, buffer));
#endif
           return KS_STATUS_FAIL;
        }

		/* shift right x bits : todo 1 bit for the moment */
	    ks_dhtrt_shiftright(newmask);

		/* create the new bucket structures */
        ks_dhtrt_bucket_header* newleft  = ks_dhtrt_create_bucketheader(table->pool, header, newmask);
        newleft->bucket = ks_dhtrt_create_bucket(table->pool); 
        newleft->flags = BHF_LEFT;                       /* flag as left hand side - therefore splitable */
	    ks_dhtrt_bucket_header* newright = ks_dhtrt_create_bucketheader(table->pool, header, header->mask);
		ks_dhtrt_split_bucket(header, newleft, newright);

		/* ok now we need to try again to see if the bucket has capacity */
		/* which bucket do care about */
		if (ks_dhtrt_ismasked(peer->id, newleft->mask)) {
            bucket = newleft->bucket;
            header = newleft;
        }
		else {
            bucket = newright->bucket;
            header = newright;
        }
        ++insanity;
	}

#ifdef  KS_DHT_DEBUGPRINTF_	
    char buffer[100];
    printf("inserting nodeid %s ",  ks_dhtrt_printableid(peer->id, buffer));
    printf("into bucket %s\n",  ks_dhtrt_printableid(header->mask, buffer));
#endif

    /* by this point we have a viable bucket */
    return ks_dhtrt_insert_id(bucket, peer);
}

KS_DECLARE(ks_dht_node_t*) ks_dhtrt_find_node(ks_dhtrt_routetable* table, ks_dhtrt_nodeid nodeid) {
     ks_dhtrt_bucket_header* header = ks_dhtrt_find_bucketheader(table, nodeid);
     if (header != 0) return 0;
     ks_dhtrt_bucket* bucket = header->bucket;
     if (bucket != 0) return 0;    /* probably a logic error ?*/
     return ks_dhtrt_find_nodeid(bucket, nodeid);
}

KS_DECLARE(ks_status_t) ks_dhtrt_touch_node(ks_dhtrt_routetable* table,  ks_dhtrt_nodeid nodeid) 
{
   ks_dhtrt_bucket_header* header = ks_dhtrt_find_bucketheader(table, nodeid);
   if (header == 0) return KS_STATUS_FAIL;
   if (header->bucket == 0)  return KS_STATUS_FAIL;
   ks_dhtrt_bucket_entry* e = ks_dhtrt_find_bucketentry(header, nodeid);
   if (e != 0) { 
      e->tyme = ks_time_now();
      e->outstanding_pings = 0;
      if (e->flags ==  DHTPEER_EXPIRED)  --header->bucket->expired_count;
      e->flags = DHTPEER_ACTIVE;
      return KS_STATUS_SUCCESS;
   }
   return KS_STATUS_FAIL;
}

KS_DECLARE(ks_status_t) ks_dhtrt_expire_node(ks_dhtrt_routetable* table,  ks_dhtrt_nodeid nodeid)
{
   ks_dhtrt_bucket_header* header = ks_dhtrt_find_bucketheader(table, nodeid);
   if (header == 0) return KS_STATUS_FAIL;
   ks_dhtrt_bucket_entry* e = ks_dhtrt_find_bucketentry(header, nodeid);
   if (e != 0) {
      e->flags = DHTPEER_EXPIRED;
      return KS_STATUS_SUCCESS;
   }
   return KS_STATUS_FAIL;
}

KS_DECLARE(uint8_t) ks_dhtrt_findclosest_nodes(ks_dhtrt_routetable* table, ks_dhtrt_querynodes* query) 
{
     query->count = 0;
     uint8_t max = query->max;
     uint8_t total = 0;
     uint8_t cnt;

     if (max == 0)  return 0;         /* sanity check */

     ks_dhtrt_bucket_header* header = ks_dhtrt_find_bucketheader(table, query->id);

#ifdef  KS_DHT_DEBUGPRINTF_
    char buffer[100];
    printf("finding %d closest nodes for  nodeid %s\n", max, ks_dhtrt_printableid(query->id, buffer));
    printf(" starting at mask: %s\n",  ks_dhtrt_printableid(header->mask, buffer));
#endif


     ks_dhtrt_sortedxors xort0;
     memset(&xort0, 0 , sizeof(xort0));
     ks_dhtrt_nodeid initid;
     memset(initid, 0xff, KS_DHT_IDSIZE);
     xort0.bheader = header;

     /* step 1 - look at immediate bucket */
     /* --------------------------------- */
     cnt = ks_dhtrt_findclosest_bucketnodes(query->id, header, &xort0, initid ,max);
     max -= cnt;
     total += cnt;

#ifdef  KS_DHT_DEBUGPRINTF_
    printf(" bucket header %s yielded %d nodes; total=%d\n",  buffer, cnt, total);
#endif

     if (total >= query->max) {   /* is query answered ?  */
         return ks_dhtrt_load_query(query, &xort0);
     }

     /* step2 - look at sibling */
     /* ----------------------- */
     ks_dhtrt_sortedxors xort1;
     xort0.next = &xort1;
     memset(&xort1, 0 , sizeof(xort1));
     memcpy(initid, &xort0.hixor, KS_DHT_IDSIZE);
     ks_dhtrt_bucket_header* parent = header->parent;
     if (header == parent->left) { 
        xort1.bheader = header = parent->right;
     }
     else {
        if (!parent->left->bucket) {   /* left hand might no have a bucket - if so choose left->right */ 
            xort1.bheader = header = parent->left->right;
        }  
        else {
            xort1.bheader = header = parent->left;
        }
     }

     cnt = ks_dhtrt_findclosest_bucketnodes(query->id, header, &xort1, initid ,max);
     max -= cnt;
     total += cnt;

#ifdef  KS_DHT_DEBUGPRINTF_
     printf(" stage2: sibling bucket header %s yielded %d nodes, total=%d\n",
                     ks_dhtrt_printableid(header->mask, buffer), cnt, total);
#endif

     if (total >= query->max) {   /* is query answered ?  */
         return ks_dhtrt_load_query(query, &xort0);
     }

     /* step3 and beyond ... work left and right until the count is satisfied */
     /* ---------------------------------------------------------------------- */
     memcpy(initid, &xort0.hixor, KS_DHT_IDSIZE);
 
     unsigned char leftid[KS_DHT_IDSIZE];
     unsigned char rightid[KS_DHT_IDSIZE];
     memcpy(leftid, xort0.bheader->mask, KS_DHT_IDSIZE);
     memcpy(rightid, xort1.bheader->mask, KS_DHT_IDSIZE);

     int insanity = 0;
     ks_dhtrt_bucket_header* lheader; 
     ks_dhtrt_bucket_header* rheader;
     ks_dhtrt_sortedxors* prev = &xort1;
     ks_dhtrt_sortedxors* tofree = 0;
     ks_dhtrt_sortedxors* xortn;
     ks_dhtrt_sortedxors* xortn1;

     do {
        lheader = 0;
        rheader = 0;
        xortn = 0;
        xortn1 = 0;
        if (leftid[0] != 0xff) { 
           ks_dhtrt_shiftleft(leftid);
           lheader = ks_dhtrt_find_bucketheader(table, leftid);
           if (lheader) {        
              xortn = ks_pool_alloc(table->pool, sizeof(ks_dhtrt_sortedxors));
              memset(xortn, 0, sizeof(ks_dhtrt_sortedxors));
              if (tofree == 0)   tofree = xortn;
              prev->next = xortn;
              prev = xortn;
              cnt += ks_dhtrt_findclosest_bucketnodes(query->id, lheader, xortn, leftid ,max);
              max -= cnt;
#ifdef  KS_DHT_DEBUGPRINTF_
              printf(" stage3: seaching left bucket header %s yielded %d nodes, total=%d\n",
                     ks_dhtrt_printableid(lheader->mask, buffer), cnt, total);
#endif
           }
        }

        if (max > 0 && rightid[KS_DHT_IDSIZE-1] != 0x00) {
           ks_dhtrt_shiftright(rightid);
           rheader = ks_dhtrt_find_bucketheader(table, rightid);
           if (rheader) {
              xortn1 = ks_pool_alloc(table->pool, sizeof(ks_dhtrt_sortedxors));
              memset(xortn1, 0, sizeof(ks_dhtrt_sortedxors));
              prev->next = xortn1;
              prev = xortn1;
              cnt = ks_dhtrt_findclosest_bucketnodes(query->id, rheader, xortn1, rightid , max);
              max -= cnt;
#ifdef  KS_DHT_DEBUGPRINTF_
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

     } while(max < query->count);


     ks_dhtrt_load_query(query, &xort0);
     /* free up the xort structs on heap */
     while(tofree) {
         ks_dhtrt_sortedxors* x = tofree->next;
         ks_pool_free(table->pool, tofree);
         tofree = x->next;
     }
     return query->count;
}

KS_DECLARE(void)  ks_dhtrt_process_table(ks_dhtrt_routetable* table)
{
    /* walk the table and update the status of all known knodes */
    /* anything that is suspect automatically becomes expired   */

    /* inactive for 15 minutes, a node becomes quesionable */
    /*  it should be pinged */

    /* if it has not been 'touched' since the last time */
    /*  give it one more try                            */

    /* inactive again it is considered inactive         */
    /*                                                  */

    ks_dhtrt_internal* internal = table->internal;
    ks_dhtrt_bucket_header* header = internal->buckets;
    ks_dhtrt_bucket_header* stack[KS_DHT_IDSIZE * 8];
    int stackix=0;
    ks_time_t t0 = ks_time_now(); 

    while(header) {
         stack[stackix++] = header;
         if (header->bucket) {
             ks_dhtrt_bucket* b = header->bucket;
             for (int ix=0; ix<KS_DHT_BUCKETSIZE; ++ix) {
                ks_dhtrt_bucket_entry* e =  &b->entries[ix];
                  if (e->inuse == 1) {
                  /* more than n pings outstanding? */
                  if (e->outstanding_pings >= KS_DHTRT_MAXPING) {
                     e->flags =  DHTPEER_EXPIRED; 
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
                }
             }   /* end for each bucket_entry */
         }
         header = header->left;
         if (header == 0 && stackix > 1) {
             stackix -= 2;
             header =  stack[stackix];
             header = header->right;
         }
    }
    return;
}


KS_DECLARE(void) ks_dhtrt_dump(ks_dhtrt_routetable* table, int level) {
     /* dump buffer headers */
     char buffer[100];
     memset(buffer, 0, 100);
     ks_dhtrt_internal* internal = table->internal;
     ks_dhtrt_bucket_header* header = internal->buckets;
     ks_dhtrt_bucket_header* stack[KS_DHT_IDSIZE * 8];
     int stackix = 0;


     while(header) {
         stack[stackix++] = header;
         /* walk and report left handsize */
         memset(buffer, 0, 100);
         /*ks_log*/ printf("bucket header: [%s]\n", ks_dhtrt_printableid(header->mask, buffer) );
         if (header->bucket) {
             ks_dhtrt_bucket* b = header->bucket;
             printf("   bucket holds %d entries\n", b->count);
             
             if (level == 7) {
                printf("   --------------------------\n");
                for(int ix=0; ix<KS_DHT_BUCKETSIZE; ++ix) {
                   memset(buffer, 0, 100);
                   if (b->entries[ix].inuse == 1) ks_dhtrt_printableid(b->entries[ix].id, buffer);
                   else strcpy(buffer, "<free>");
                   printf("       slot %d: %s\n", ix, buffer);
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
     return;
}





/* stupid routines to avoid unused warnings */
void colm() {
 ks_dhtrt_shiftright(0);
 ks_dhtrt_xor(0, 0, 0);
 ks_dhtrt_xorcmp(0, 0, 0);
 ks_dhtrt_split_bucket(0, 0, 0);
 ks_dhtrt_shiftleft(0);
 ks_dhtrt_initrwlock( 0);
 ks_dhtrt_deinitrwlock( 0);

 ks_dhtrt_getreadlock( 0);
 ks_dhtrt_getwritelock( 0);
 ks_dhtrt_tryreadlock( 0);
 ks_dhtrt_trywritelock( 0);
 ks_dhtrt_releasereadlock( 0);
 ks_dhtrt_releasewritelock( 0);

}



/* 
   internal functions 
*/

static
ks_dhtrt_bucket_header* ks_dhtrt_create_bucketheader(ks_pool_t *pool, ks_dhtrt_bucket_header* parent, unsigned char* mask) 
{
    ks_dhtrt_bucket_header* header = ks_pool_alloc(pool, sizeof(ks_dhtrt_bucket_header));
    memset(header, 0, sizeof(ks_dhtrt_bucket_header));
    memcpy(header->mask, mask, sizeof(header->mask));  
    header->parent = parent;   

#ifdef KS_DHT_DEBUGPRINTF_
    char buffer[100];
    printf("creating bucket header for mask: %s ",  ks_dhtrt_printableid(mask, buffer));
    if (parent) printf("from parent mask: %s ",  ks_dhtrt_printableid(parent->mask, buffer));
    printf("\n"); 
#endif
    return header;
}

static
ks_dhtrt_bucket* ks_dhtrt_create_bucket(ks_pool_t *pool)
{
    ks_dhtrt_bucket* bucket = ks_pool_alloc(pool, sizeof(ks_dhtrt_bucket));
    memset(bucket, 0, sizeof(ks_dhtrt_bucket));
    return bucket;
}

static
ks_dhtrt_bucket_header* ks_dhtrt_find_bucketheader(ks_dhtrt_routetable* table, unsigned char* id) 
{
    /* find the right bucket.  
	  if a bucket header has a bucket, it does not  children
      so it must be the bucket to use	  
	*/
    ks_dhtrt_internal* internal = table->internal;
    ks_dhtrt_bucket_header* header = internal->buckets;
    while(header) {
	   if ( header->bucket ) {
			return header;
	   }	
	   /* left hand side is more restrictive (closer) so should be tried first */
       if (header->left != 0 && (ks_dhtrt_ismasked(id, header->left->mask))) 
	        header = header->left;  
		else
            header = header->right;		
    } 
	return 0;
}

static
ks_dhtrt_bucket_entry* ks_dhtrt_find_bucketentry(ks_dhtrt_bucket_header* header, ks_dhtrt_nodeid nodeid) 
{
     ks_dhtrt_bucket* bucket = header->bucket;
     if (bucket == 0)  return 0;

     for (int ix=0; ix<KS_DHT_BUCKETSIZE; ++ix) {
#ifdef  KS_DHT_DEBUGPRINTF_
#endif
        if ( bucket->entries[ix].inuse == 1   &&
             (!memcmp(nodeid, bucket->entries[ix].id, KS_DHT_IDSIZE)) ) {
            return &(bucket->entries[ix]);
        }
     }
     return 0;
}

 
static
void ks_dhtrt_split_bucket(ks_dhtrt_bucket_header* original, ks_dhtrt_bucket_header* left, ks_dhtrt_bucket_header* right) 
{
    /* so split the bucket in two based on the masks in the new header */
    /* the existing bucket - with the remaining ids will be taken by the right hand side */

	ks_dhtrt_bucket* source = original->bucket;
	ks_dhtrt_bucket* dest   = left->bucket;
	
	int lix = 0;
	int rix = 0;
	
	for( ; rix<KS_DHT_BUCKETSIZE; ++rix) {
	    if (ks_dhtrt_ismasked(source->entries[rix].id, left->mask)) {
            /* move it to the left */
			memcpy(dest->entries[lix].id, source->entries[rix].id, KS_DHT_IDSIZE);
			dest->entries[lix].gptr = source->entries[rix].gptr;
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
#ifdef  KS_DHT_DEBUGPRINTF_
    char buffer[100];
    printf("\nsplitting bucket orginal: %s\n",  ks_dhtrt_printableid(original->mask, buffer));
    printf(" into (left) mask: %s size: %d\n",  ks_dhtrt_printableid(left->mask, buffer), left->bucket->count);
    printf(" and (right) mask: %s size: %d\n\n",  ks_dhtrt_printableid(right->mask, buffer), right->bucket->count);
#endif
	return;
}


/*
 *   buckets are implemented as static array 
 *   There does not seem to be any advantage in sorting/tree structures in terms of xor math
 *    so at least the static array does away with the need for locking.
 */
static 
ks_status_t ks_dhtrt_insert_id(ks_dhtrt_bucket* bucket, ks_dhtrt_node* peer)
{
    /* sanity checks */
	if (!bucket || bucket->count >= KS_DHT_BUCKETSIZE) {
	    assert(0);
	}
    uint8_t free = KS_DHT_BUCKETSIZE;
    uint8_t expiredix = KS_DHT_BUCKETSIZE;
	
	/* find free .. but also check that it is not already here! */
    uint8_t ix = 0;
	for(; ix<KS_DHT_BUCKETSIZE; ++ix)  {
 	    if (bucket->entries[ix].inuse == 0) {
            if (free == KS_DHT_BUCKETSIZE) {
               free = ix; /* use this one   */
            }
        }
        else if (free == KS_DHT_BUCKETSIZE && bucket->entries[ix].flags == DHTPEER_EXPIRED) {
            expiredix = ix;
        }
 		else if (!memcmp(bucket->entries[ix].id, peer->id, KS_DHT_IDSIZE)) {
#ifdef  KS_DHT_DEBUGPRINTF_
            char buffer[100];
            printf("duplicate peer %s found at %d ", ks_dhtrt_printableid(peer->id, buffer), ix);
#endif
            bucket->entries[ix].tyme = ks_time_now();
            bucket->entries[ix].flags &= DHTPEER_ACTIVE;
            return KS_STATUS_SUCCESS;  /* already exists */
        }
	}

    if (free == KS_DHT_BUCKETSIZE && expiredix<KS_DHT_BUCKETSIZE ) {
        /* bump this one - but only if we have no other option */
        free =  expiredix;
        --bucket->expired_count;
    }
	
	if ( free<KS_DHT_BUCKETSIZE ) {
		bucket->entries[free].inuse = 1;
		bucket->entries[free].gptr = peer;
        bucket->entries[free].tyme = ks_time_now();
        bucket->entries[free].flags &= DHTPEER_ACTIVE;
        ++bucket->count;
		memcpy(bucket->entries[free].id, peer->id, KS_DHT_IDSIZE);
#ifdef  KS_DHT_DEBUGPRINTF_
        char buffer[100];
        printf("inserting peer %s ",  ks_dhtrt_printableid(peer->id, buffer));
        printf("into bucket mask at index %d\n",  free);
#endif
        return KS_STATUS_SUCCESS;
	}
	
    return KS_STATUS_FAIL;
}
	 
static
ks_dht_node_t* ks_dhtrt_find_nodeid(ks_dhtrt_bucket* bucket, ks_dhtrt_nodeid nodeid) 
{
#ifdef  KS_DHT_DEBUGPRINTF_
    char buffer[100];
    printf("\nfind noeid for: %s\n",  ks_dhtrt_printableid(nodeid, buffer));
#endif

    
     for (int ix=0; ix<KS_DHT_BUCKETSIZE; ++ix) {
#ifdef  KS_DHT_DEBUGPRINTF_
        printf("\nbucket->entries[%d].id = %s inuse=%c\n", ix,
                   ks_dhtrt_printableid(bucket->entries[ix].id, buffer),
                   bucket->entries[ix].inuse  );
#endif    
        if ( bucket->entries[ix].inuse == 1   &&
             (!memcmp(nodeid, bucket->entries[ix].id, KS_DHT_IDSIZE)) ) {
           return bucket->entries[ix].gptr->handle;
        }
     }
     return 0;
}

static
void ks_dhtrt_delete_id(ks_dhtrt_bucket* bucket, ks_dhtrt_nodeid nodeid)
{
#ifdef  KS_DHT_DEBUGPRINTF_
    char buffer[100];
    printf("\ndeleting node for: %s\n",  ks_dhtrt_printableid(nodeid, buffer));
#endif


     for (int ix=0; ix<KS_DHT_BUCKETSIZE; ++ix) {
#ifdef  KS_DHT_DEBUGPRINTF_
        printf("\nbucket->entries[%d].id = %s inuse=%c\n", ix,
                   ks_dhtrt_printableid(bucket->entries[ix].id, buffer),
                   bucket->entries[ix].inuse  );
#endif
        if ( bucket->entries[ix].inuse == 1   &&
             (!memcmp(nodeid, bucket->entries[ix].id, KS_DHT_IDSIZE)) ) {
           bucket->entries[ix].inuse = 0;
           bucket->entries[ix].gptr = 0;
           bucket->entries[ix].flags = 0;
           return;
        }
     }
     return;
}


static
uint8_t ks_dhtrt_findclosest_bucketnodes(unsigned char *nodeid,
                                            ks_dhtrt_bucket_header* header,
                                            ks_dhtrt_sortedxors* xors,
											unsigned char* hixor,    /*todo: remove */
											unsigned int max) {
     
	 uint8_t count = 0;   /* count of nodes added this time */
	 xors->startix = KS_DHT_BUCKETSIZE;
     xors->count = 0;
     unsigned char xorvalue[KS_DHT_IDSIZE];
	 
	 /* just ugh! - there must be a better way to do this */
	 /* walk the entire bucket calculating the xor value on the way */
	 /* add valid & relevant entries to the xor values   */
	 ks_dhtrt_bucket* bucket = header->bucket;

     if (bucket == 0)  {        /* sanity */
#ifdef  KS_DHT_DEBUGPRINTF_
        char buf[100];
        printf("closestbucketnodes: intermediate tree node found %s\n", 
                   ks_dhtrt_printableid(header->mask, buf));
#endif
        
     }

	 for(uint8_t ix=0; ix<KS_DHT_BUCKETSIZE; ++ix) {
	    if ( bucket->entries[ix].inuse == 1   &&
             ks_dhtrt_isactive( &(bucket->entries[ix])) ) {
		  
		   /* calculate xor value */
		   ks_dhtrt_xor(nodeid, bucket->entries[ix].id, xorvalue );
		   
		   /* do we need to hold this one */
		   if ( count < max    ||                                 /* yes: we have not filled the quota yet */
		        (memcmp(xorvalue, hixor, KS_DHT_IDSIZE) < 0)) {   /* or is closer node than one already selected */
			   
				/* now sort the new xorvalue into the results structure  */
				/* this now becomes worst case O(n*2) logic - is there a better way */
				/* in practice the bucket size is fixed so actual behavior is proably 0(logn) */
				unsigned int xorix = xors->startix;       /* start of ordered list */
				unsigned int prev_xorix = KS_DHT_BUCKETSIZE;
                 
				for(int ix2=0; ix2<count; ++ix2) {
					if (memcmp(xorvalue, xors->xort[xorix].xor, KS_DHT_IDSIZE) > 0) {
                          
						break;            /* insert before xorix, after prev_xoris */
					}  
					prev_xorix = xorix;
					xorix = xors->xort[xorix].nextix;
				}
		   
				/* insert point found
				   count -> array slot to added newly identified node
				   insert_point -> the array slot before which we need to insert the newly identified node
				*/
				memcpy(xors->xort[count].xor, xorvalue, KS_DHT_IDSIZE);
				xors->xort[count].ix = ix;
				
				xors->xort[count].nextix = xorix;            /* correct forward chain */
				if (prev_xorix < KS_DHT_BUCKETSIZE) {        /* correct backward chain */
				   xors->xort[prev_xorix].nextix = count;
				}
				else {
				   xors->startix = count;
				}
				++count;
            }
		}
	 }
     xors->count = count; 
	 return count;   /* return count of added nodes */
}

static
uint8_t ks_dhtrt_load_query(ks_dhtrt_querynodes* query, ks_dhtrt_sortedxors* xort) 
{
	 ks_dhtrt_sortedxors* current = xort;
	 uint8_t loaded = 0;
 	 while(current) {
#ifdef  KS_DHT_DEBUGPRINTF_
         char buf[100];
         printf("  loadquery from bucket %s count %d\n",  
                   ks_dhtrt_printableid(current->bheader->mask,buf), current->count);
#endif
	     int xorix = current->startix; 
         for (uint8_t ix = 0; ix<= current->count && loaded < query->max; ++ix ) {
		     unsigned int z =  current->xort[xorix].ix;
		     query->nodes[ix] = current->bheader->bucket->entries[z].gptr->handle;
			 ++loaded;
		 }		    
		 if (loaded >= query->max) break;
	     current = current->next;
	 }
	 query->count = loaded;
	 return loaded;
}

void ks_dhtrt_ping(ks_dhtrt_bucket_entry* entry) {
    ++entry->outstanding_pings;
    /* @todo */
    /* set the appropriate command in the node and queue if for processing */
     /*ks_dht_node_t* node = entry->gptr; */
#ifdef  KS_DHT_DEBUGPRINTF_
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
void ks_dhtrt_shiftright(unsigned char* id) 
{
   unsigned char b0 = 0;
   unsigned char b1 = 0;

   for(int i = KS_DHT_IDSIZE-1; i >= 0; --i) {
     if (id[i] == 0) break;    /* beyond mask- we are done */
     b1 = id[i] & 0x01;
     id[i] >>= 1;
     if (i != (KS_DHT_IDSIZE-1)) {
        id[i+1] |= (b0 << 7);
     }
     b0 = b1;
   }
   return;
}

static
void ks_dhtrt_shiftleft(unsigned char* id) {

   for(int i = KS_DHT_IDSIZE-1; i >= 0; --i) {
      if (id[i] == 0xff) continue;
      id[i] <<= 1;
      id[i] |= 0x01;
      break;
   }
   return;
}

/* Determine whether id1 or id2 is closer to ref */
static int ks_dhtrt_xorcmp(const unsigned char *id1, const unsigned char *id2, const unsigned char *ref)
{
    int i;
    for (i = 0; i < KS_DHT_IDSIZE; i++) {
        unsigned char xor1, xor2;
        if (id1[i] == id2[i]) {
            continue;
        }
        xor1 = id1[i] ^ ref[i];
        xor2 = id2[i] ^ ref[i];
        if (xor1 < xor2) {
            return -1;          /* id1 is closer */
        }
        return 1;               /* id2 is closer */
    }
    return 0;     /* id2 and id2 are identical ! */
}

/* create an xor value from two ids */
static void ks_dhtrt_xor(const unsigned char *id1, const unsigned char *id2, unsigned char *xor)
{
    for (int i = 0; i < KS_DHT_IDSIZE; ++i) {
        if (id1[i] == id2[i]) {
            xor[i] = 0;
        }
        xor[i] = id1[i] ^ id2[i];
    }
    return;
}

/* is id masked by mask 1 => yes, 0=> no */
static int ks_dhtrt_ismasked(const unsigned char *id, const unsigned char *mask) 
{
    for (int i = 0; i < KS_DHT_IDSIZE; ++i) {
        if (mask[i] == 0 && id[i] != 0) return 0;
        else if (mask[i] == 0xff)       return 1;
        else if (id[i] > mask[i])       return 0;
    }
    return 1;
}

static
ks_status_t ks_dhtrt_initrwlock( ks_dhtrt_rw_lock* lock) 
{
    ks_status_t s  = ks_mutex_create(&lock->mutex, 0, lock->pool);
    if (s != KS_STATUS_SUCCESS) return s;
    s  = ks_cond_create_ex(&lock->rcond, lock->pool, lock->mutex);
    if (s != KS_STATUS_SUCCESS) return s;
    s  = ks_cond_create_ex(&lock->wcond, lock->pool, lock->mutex);
    return s;
}

static
void ks_dhtrt_deinitrwlock( ks_dhtrt_rw_lock* lock) 
{
    ks_cond_destroy(&lock->rcond);
    ks_cond_destroy(&lock->wcond);
    ks_mutex_destroy(&lock->mutex);
    memset(lock, 0, sizeof(ks_dhtrt_rw_lock));
}

static
void ks_dhtrt_getreadlock( ks_dhtrt_rw_lock* lock)
{
    ks_mutex_lock(lock->mutex);
    while (lock->write_count > 0) {
       ks_cond_wait(lock->rcond);
    }
    ++lock->read_count;
    ks_mutex_unlock(lock->mutex);
}

static
ks_status_t ks_dhtrt_tryreadlock( ks_dhtrt_rw_lock* lock)
{
     return KS_STATUS_FAIL;
}

static
void ks_dhtrt_releasereadlock( ks_dhtrt_rw_lock* lock) 
{
     ks_mutex_lock(lock->mutex);
     --lock->read_count;
     if (lock->read_count == 0)  
          ks_cond_signal(lock->wcond);
     ks_mutex_unlock(lock->mutex);
}

static
void ks_dhtrt_getwritelock( ks_dhtrt_rw_lock* lock) 
{
     ks_mutex_lock(lock->mutex);
     while (lock->read_count > 0) {
       ks_cond_wait(lock->wcond);
     }
     ++lock->write_count;
     ks_mutex_unlock(lock->mutex);
}

static
ks_status_t ks_dhtrt_trywritelock( ks_dhtrt_rw_lock* lock)
{
  return KS_STATUS_FAIL;
}

static
void ks_dhtrt_releasewritelock( ks_dhtrt_rw_lock* lock)
{
    ks_mutex_lock(lock->mutex);
    --lock->write_count;
    assert(lock->write_count==0);
    ks_cond_broadcast(lock->rcond);   
    ks_mutex_unlock(lock->mutex);
}


static char* ks_dhtrt_printableid(const unsigned char* id, char* buffer)
{
   char* t = buffer;
   memset(buffer, 0, KS_DHT_IDSIZE*2); 
   for (int i = 0; i < KS_DHT_IDSIZE; ++i, buffer+=2) {
      sprintf(buffer, "%02x", id[i]);
   }
   return t;
}

unsigned char ks_dhtrt_isactive(ks_dhtrt_bucket_entry* entry) 
{
     /* todo */
	 return 1;
}


