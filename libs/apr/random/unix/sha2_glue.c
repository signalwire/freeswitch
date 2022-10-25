#include <fspr.h>
#include <fspr_random.h>
#include <fspr_pools.h>
#include "sha2.h"

static void sha256_init(fspr_crypto_hash_t *h)
    {
    fspr__SHA256_Init(h->data);
    }

static void sha256_add(fspr_crypto_hash_t *h,const void *data,
			  fspr_size_t bytes)
    {
    fspr__SHA256_Update(h->data,data,bytes);
    }

static void sha256_finish(fspr_crypto_hash_t *h,unsigned char *result)
    {
    fspr__SHA256_Final(result,h->data);
    }

APR_DECLARE(fspr_crypto_hash_t *) fspr_crypto_sha256_new(fspr_pool_t *p)
    {
    fspr_crypto_hash_t *h=fspr_palloc(p,sizeof *h);

    h->data=fspr_palloc(p,sizeof(SHA256_CTX));
    h->init=sha256_init;
    h->add=sha256_add;
    h->finish=sha256_finish;
    h->size=256/8;

    return h;
    }
