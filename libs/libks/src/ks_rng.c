/*
 * Cross Platform random/uuid abstraction
 * Copyright(C) 2015 Michael Jerris
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so.
 *
 * This work is provided under this license on an "as is" basis, without warranty of any kind,
 * either expressed or implied, including, without limitation, warranties that the covered code
 * is free of defects, merchantable, fit for a particular purpose or non-infringing. The entire
 * risk as to the quality and performance of the covered code is with you. Should any covered
 * code prove defective in any respect, you (not the initial developer or any other contributor)
 * assume the cost of any necessary servicing, repair or correction. This disclaimer of warranty
 * constitutes an essential part of this license. No use of any covered code is authorized hereunder
 * except under this disclaimer.
 *
 */

#include "ks.h"
#include "sodium.h"
#include <aes.h>
#include <sha2.h>

static ks_bool_t initialized = KS_FALSE;
static ks_mutex_t *rng_mutex = NULL;
static sha512_ctx global_sha512;

#ifdef __WINDOWS__
#include <Wincrypt.h>
HCRYPTPROV crypt_provider;
#else
int fd = -1;
#endif

/*
 * memset_volatile is a volatile pointer to the memset function.
 * You can call (*memset_volatile)(buf, val, len) or even
 * memset_volatile(buf, val, len) just as you would call
 * memset(buf, val, len), but the use of a volatile pointer
 * guarantees that the compiler will not optimise the call away.
 */
//static void * (*volatile memset_volatile)(void *, int, size_t) = memset;

KS_DECLARE(uuid_t *) ks_uuid(uuid_t *uuid)
{
#ifdef __WINDOWS__
    UuidCreate ( uuid );
#else
    uuid_generate_random ( *uuid );
#endif
	return uuid;
}

KS_DECLARE(char *) ks_uuid_str(ks_pool_t *pool, uuid_t *uuid)
{
	char *uuidstr = ks_pool_alloc(pool, 37);
#ifdef __WINDOWS__
    unsigned char * str;
    UuidToStringA ( uuid, &str );
	uuidstr = ks_pstrdup(pool, str);
    RpcStringFreeA ( &str );
#else
    char str[37] = { 0 };
    uuid_unparse ( *uuid, str );
	uuidstr = ks_pstrdup(pool, str);
#endif
	return uuidstr;
}

KS_DECLARE(ks_status_t) ks_rng_init(void)
{
	if (!initialized) {
		if (sodium_init() == -1) {
			abort();
		}
		

		randombytes_random();
		ks_aes_init();
		ks_mutex_create(&rng_mutex, KS_MUTEX_FLAG_DEFAULT, ks_global_pool());
#ifdef __WINDOWS__
		if (!crypt_provider) {		
			if (CryptAcquireContext(&crypt_provider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT) == TRUE) {
				initialized = KS_TRUE;
			} else {
				initialized = KS_FALSE;
			}
		}
#else
		if (fd < 0) {
			fd = open("/dev/urandom", O_RDONLY);
			if (fd < 0) {
				fd = open("/dev/random", O_RDONLY);
			}
		}
		initialized = KS_TRUE;
#endif
	}

	sha512_begin(&global_sha512);

	if (initialized) {
		return KS_STATUS_SUCCESS;
	} else {
		return KS_STATUS_FAIL;
	}
}

KS_DECLARE(ks_status_t) ks_rng_shutdown(void)
{

	initialized = KS_FALSE;
#ifdef __WINDOWS__
    if (crypt_provider) {
        CryptReleaseContext(crypt_provider, 0);
        crypt_provider = 0;
    }
#else
	if (fd >= 0) {
		close(fd);
		fd = -1;
	}
#endif
	return KS_STATUS_SUCCESS;
}

KS_DECLARE(size_t) ks_rng_seed_data(uint8_t *seed, size_t length)
{
	size_t bytes = 0;

	if (!initialized && (ks_rng_init() != KS_STATUS_SUCCESS)) {
		return bytes;
	}
#ifdef __WINDOWS__
	if (crypt_provider) {
		if(!CryptGenRandom(crypt_provider, length, seed)) {
			return 0;
		}
		bytes = length;
	}
#else
	if (fd >= 0) {
		bytes = read(fd, seed, length);
	} else {
	}
#endif
	return bytes;
}

KS_DECLARE(size_t) ks_rng_add_entropy(const uint8_t *buffer, size_t length)
{

    uint8_t seed[64];
    size_t len = ks_rng_seed_data(seed, sizeof(seed));

    ks_mutex_lock(rng_mutex);

	if (!initialized) {
		ks_rng_init();
	}

    if (buffer && length) {
        sha512_hash(buffer, length, &global_sha512);
    }

    if (len > 0) {
        sha512_hash(seed, len, &global_sha512);
        length += len;
    }

    ks_mutex_unlock(rng_mutex);

    return length;
}

KS_DECLARE(size_t) ks_rng_get_data(uint8_t* buffer, size_t length) {
	randombytes_buf(buffer, length);
	return length;
}


#if 0

KS_DECLARE(size_t) ks_rng_get_data(uint8_t* buffer, size_t length) {

	aes_encrypt_ctx cx[1];
    sha512_ctx random_context;
    uint8_t    md[SHA512_DIGEST_SIZE];
    uint8_t    ctr[AES_BLOCK_SIZE];
    uint8_t    rdata[AES_BLOCK_SIZE];
    size_t     generated = length;

    /* Add entropy from system state. We will include whatever happens to be in the buffer, it can't hurt */
	ks_rng_add_entropy(buffer, length);

    ks_mutex_lock(rng_mutex);

    /* Copy the mainCtx and finalize it into the md buffer */
    memcpy(&random_context, &global_sha512, sizeof(sha512_ctx));
    sha512_end(md, &random_context);

    ks_mutex_lock(rng_mutex);

    /* Key an AES context from this buffer */
	aes_encrypt_key256(md, cx);

    /* Initialize counter, using excess from md if available */
    memset (ctr, 0, sizeof(ctr));
	uint32_t ctrbytes = AES_BLOCK_SIZE;
	memcpy(ctr + sizeof(ctr) - ctrbytes, md + 32, ctrbytes);

    /* Encrypt counter, copy to destination buffer, increment counter */
    while (length) {
        uint8_t *ctrptr;
        size_t copied;
		aes_encrypt(ctr, rdata, cx);
        copied = (sizeof(rdata) < length) ? sizeof(rdata) : length;
        memcpy (buffer, rdata, copied);
        buffer += copied;
        length -= copied;

        /* Increment counter */
        ctrptr = ctr + sizeof(ctr) - 1;
        while (ctrptr >= ctr) {
            if ((*ctrptr-- += 1) != 0) {
                break;
            }
        }
    }
    memset_volatile(&random_context, 0, sizeof(random_context));
    memset_volatile(md, 0, sizeof(md));
    memset_volatile(&cx, 0, sizeof(cx));
    memset_volatile(ctr, 0, sizeof(ctr));
    memset_volatile(rdata, 0, sizeof(rdata));

    return generated;
}

#endif

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
