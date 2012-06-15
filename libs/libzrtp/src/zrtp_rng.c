/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 */
 
#include "zrtp.h"

#define _ZTU_ "zrtp rng"

#define MD_DIGEST_LENGTH	SHA512_DIGEST_SIZE
#define	MD_CTX_init(a)
#define MD_Init(a)			sha512_begin(a)
#define MD_Final(a,b)		sha512_end(b,a)
#define	MD_Cleanup(a)		zrtp_memset(a,0,sizeof(*a));


#if (ZRTP_PLATFORM == ZP_WIN32) || (ZRTP_PLATFORM == ZP_WIN64) || (ZRTP_PLATFORM == ZP_WINCE)

#include <Wincrypt.h>

HCRYPTPROV   g_hCryptProv;

zrtp_status_t NtLmInitializeRNG(VOID)
{
    BOOL fSuccess;
	
    if (g_hCryptProv != 0) {		
        return zrtp_status_ok;
    }
	
    fSuccess = CryptAcquireContext( &g_hCryptProv,
								    NULL,
								    NULL,
								    PROV_RSA_FULL,
								    CRYPT_VERIFYCONTEXT);
	
    return (TRUE == fSuccess) ? zrtp_status_ok : zrtp_status_fail;
}

void  NtLmCleanupRNG(VOID)
{
    if (g_hCryptProv) {
        CryptReleaseContext(g_hCryptProv, 0);
        g_hCryptProv = 0;
    }
}

int zrtp_add_system_state(zrtp_global_t* zrtp, MD_CTX *ctx)
{
	uint8_t buffer[64];
	
	if(!CryptGenRandom(g_hCryptProv,  sizeof(buffer), buffer)) {
		ZRTP_LOG(1,(_ZTU_,"\tERROR! Error during CryptGenRandom.\n"));
		return 0;
	}
	
	MD_Update(ctx, buffer, sizeof(buffer));
	ZeroMemory((PVOID)buffer, sizeof(buffer));
	
	return sizeof(buffer);
}

#elif (ZRTP_PLATFORM == ZP_WIN32_KERNEL)

#include <Ndis.h>

/*----------------------------------------------------------------------------*/
int zrtp_add_system_state(zrtp_global_t* zrtp, MD_CTX *ctx)
{
    LARGE_INTEGER li1;
    LARGE_INTEGER li2;
    ULONG ul1;
    ULONG ul2;
    ULONGLONG ull;
    PKTHREAD thread;
    static int tsc_ok = 1;
	/* 
	 * WARNING! 
	 * Of course it's not a real size of entropy added to the context. It's very
	 * difficult to compute the size of real random data and estimate its quality.
	 * This value means: size of maximum possible random data which this function can provide.
	 */
	static int entropy_length = sizeof(LARGE_INTEGER)*2 + sizeof(PKTHREAD) +
								sizeof(ULONG)*2 + sizeof(LARGE_INTEGER)*2 + sizeof(ULONG)*2;

    li2 = KeQueryPerformanceCounter(&li1);
    MD_Update(ctx, &li1, sizeof(LARGE_INTEGER));
    MD_Update(ctx, &li2, sizeof(LARGE_INTEGER));

    ull = KeQueryInterruptTime();
    MD_Update(ctx, &ull, sizeof(ULONGLONG));

    thread = KeGetCurrentThread();
    MD_Update(ctx, &thread, sizeof(PKTHREAD));
    ul2 = KeQueryRuntimeThread(thread, &ul1);
    MD_Update(ctx, &ul1, sizeof(ULONG));
    MD_Update(ctx, &ul2, sizeof(ULONG));

    KeQuerySystemTime(&li1);
    MD_Update(ctx, &li1, sizeof(LARGE_INTEGER));

    KeQueryTickCount(&li1);
    MD_Update(ctx, &li1, sizeof(LARGE_INTEGER));

    if (tsc_ok) {
		__try {			
			ull = _RDTSC();
			MD_Update(ctx, &ull, sizeof(ULONGLONG));
		} __except(EXCEPTION_EXECUTE_HANDLER) {
			tsc_ok = 0;
		}
    }
    
    return entropy_length;
}

#elif ((ZRTP_PLATFORM == ZP_SYMBIAN))
/*
 * WARNING! 
 * This is just a stub to let you start with something little bit better then zero.
 * We have no possibility to implement entropy collection in this abstract cross-platform
 * application. This function MUST NOT be used as example in real applications. For more
 * information read \ref RNG in developers guide
 *
 * To add real entropy - capture random data from microphone and camera.
 */
extern uint32_t zrtp_symbian_kernel_random();
extern uint32_t zrtp_sum_of_pid_and_number_of_poccesses();
extern uint64_t zrtp_get_system_time_crazy();
extern unsigned int zrtp_get_pid();
extern uint32_t zrtp_get_availible_heap();


int zrtp_add_system_state(zrtp_global_t* zrtp, MD_CTX *ctx) {
	uint64_t sysdate;
	unsigned int	pid;
	uint32_t crazy_pid_sum;
	
	uint32_t heap_size;
	
	static int entropy_length = sizeof(sysdate) + sizeof(pid)
			+ sizeof(crazy_pid_sum) + sizeof(heap_size);
	sysdate = zrtp_get_system_time_crazy();
	MD_Update(ctx,&sysdate,sizeof(sysdate));
	
	pid = zrtp_get_pid();

	MD_Update(ctx,&pid,sizeof(pid));
	
	crazy_pid_sum = zrtp_sum_of_pid_and_number_of_poccesses();
	MD_Update(ctx,&crazy_pid_sum,sizeof(crazy_pid_sum));
	
	heap_size = zrtp_get_availible_heap();
	MD_Update(ctx,&heap_size,sizeof(heap_size));
	
	return entropy_length;
}

#elif ( (ZRTP_PLATFORM == ZP_LINUX) || (ZRTP_PLATFORM == ZP_DARWIN) || (ZRTP_PLATFORM == ZP_BSD) ||  (ZRTP_PLATFORM == ZP_ANDROID) )

#if ZRTP_HAVE_STDIO_H == 1
#	include <stdio.h>
#else
#	error "Used environment dosn't have <stdio.h> - zrtp_rng.c can't be build."
#endif

/*----------------------------------------------------------------------------*/
int zrtp_add_system_state(zrtp_global_t* zrtp, MD_CTX *ctx)
{
    uint8_t buffer[64];
    size_t bytes_read	= 0;
    static size_t length= sizeof(buffer);
    FILE *fp  			= NULL;
    
    fp = fopen("/dev/urandom", "rb");
    if (!fp) {
		ZRTP_LOG(1,(_ZTU_,"\tERROR! can't get access to /dev/urandom - trying /dev/random.\n"));
		fp = fopen("/dev/random", "rb");
    }
	
    if (fp) {
		int number_of_retries = 1024;
		while ((bytes_read < length) && (number_of_retries-- > 0)) {
			setbuf(fp, NULL); /* Otherwise fread() tries to read() 4096 bytes or other default value */
			bytes_read	+= fread(buffer+bytes_read, 1, length-bytes_read, fp);
		}

		if (0 != fclose(fp)) {
			ZRTP_LOG(1,(_ZTU_,"\tERROR! unable to cloas /dev/random\n"));
		}
    } else {
		ZRTP_LOG(1,(_ZTU_,"\tERROR! RNG Can't open /dev/random\n"));
    }    

    if (bytes_read < length) {
		ZRTP_LOG(1,(_ZTU_,"\tERROR! can't read random string! Current session have to be closed.\n"));
		return -1;
    }

    MD_Update(ctx, buffer, length);
	zrtp_memset(buffer, 0, sizeof(buffer));

    return bytes_read;
}

#endif

/*----------------------------------------------------------------------------*/
zrtp_status_t zrtp_init_rng(zrtp_global_t* zrtp)
{
	if (!zrtp->rand_initialized) {		
		zrtp_mutex_init(&zrtp->rng_protector);
		MD_Init(&zrtp->rand_ctx);
#if (ZRTP_PLATFORM == ZP_WIN32) || (ZRTP_PLATFORM == ZP_WIN64) || (ZRTP_PLATFORM == ZP_WINCE)
		if (zrtp_status_ok != NtLmInitializeRNG()) {
			ZRTP_LOG(1,(_ZTU_,"\tERROR! during CryptAcquireContext!\n"));
			return zrtp_status_fail;
		}
#endif		
		zrtp->rand_initialized = 1;
	}
	
	return zrtp_status_ok;
}

void zrtp_down_rng(zrtp_global_t* zrtp)
{
	if (zrtp->rand_initialized) {
		zrtp_mutex_destroy(zrtp->rng_protector);	
#if (ZRTP_PLATFORM == ZP_WIN32) || (ZRTP_PLATFORM == ZP_WIN64) || (ZRTP_PLATFORM == ZP_WINCE)
		NtLmCleanupRNG();
#endif
		zrtp->rand_initialized = 0;
	}
}


/*
 * Call this to add entropy to the system from the given buffer,
 * and also from the system state.  It's OK to pass a null buffer
 * with a length of zero, then we will just use the system entropy.
 */
/*----------------------------------------------------------------------------*/
int zrtp_entropy_add(zrtp_global_t* zrtp, const unsigned char *buffer, uint32_t length)
{
    if (buffer && length) {
		MD_Update(&zrtp->rand_ctx, buffer, length);
	}
	
	return zrtp_add_system_state(zrtp, &zrtp->rand_ctx);
}


/*
 * Random bits are produced as follows.
 * First stir new entropy into the random state (zrtp->rand_ctx).
 * Then make a copy of the random context and finalize it.
 * Use the digest to seed an AES-256 context and, if space remains, to
 * initialize a counter.
 * Then encrypt the counter with the AES-256 context, incrementing it
 * per block, until we have produced the desired quantity of data.
 */
/*----------------------------------------------------------------------------*/
int zrtp_randstr(zrtp_global_t* zrtp, unsigned char *buffer, uint32_t length)
{
	//TODO: replace bg_aes_xxx() with our own block cipher component.
	//TODO: Do the same with the hash functions.

    aes_encrypt_ctx	aes_ctx;
    MD_CTX			rand_ctx2;
    unsigned char	md[MD_DIGEST_LENGTH];
    unsigned char	ctr[AES_BLOCK_SIZE];
    unsigned char	rdata[AES_BLOCK_SIZE];
    uint32_t		generated = length;
	
	/*
	 * In few cases we need to gerate random value before initializing libzrtp engine.
	 * Following trick makes it possible.
	 */	
	if (!zrtp->rand_initialized) {
		if (zrtp_status_ok != zrtp_init_rng(zrtp)) {
			return -1;
		}
	}	

	zrtp_mutex_lock(zrtp->rng_protector);

    /*
     * Add entropy from system state
     * We will include whatever happens to be in the buffer, it can't hurt
     */
    if ( 0 > zrtp_entropy_add(zrtp, buffer, length) ) {		
		zrtp_mutex_unlock(zrtp->rng_protector);
        return -1;
    }

    /* Copy the zrtp->rand_ctx and finalize it into the md buffer */
    rand_ctx2 = zrtp->rand_ctx;
    MD_Final(&rand_ctx2, md);
    
    zrtp_mutex_unlock(zrtp->rng_protector);

    /* Key an AES context from this buffer */
    zrtp_bg_aes_encrypt_key256(md, &aes_ctx);

    /* Initialize counter, using excess from md if available */
    zrtp_memset (ctr, 0, sizeof(ctr));
    if (MD_DIGEST_LENGTH > (256/8)) {
		uint32_t ctrbytes = MD_DIGEST_LENGTH - (256/8);
		if (ctrbytes > AES_BLOCK_SIZE)
			ctrbytes = AES_BLOCK_SIZE;
		zrtp_memcpy(ctr + sizeof(ctr) - ctrbytes, md + (256/8), ctrbytes);
    }
	
    /* Encrypt counter, copy to destination buffer, increment counter */
    while (length)
    {
		unsigned char *ctrptr;
		uint32_t copied;
		zrtp_bg_aes_encrypt(ctr, rdata, &aes_ctx);
		copied = (sizeof(rdata) < length) ? sizeof(rdata) : length;
		zrtp_memcpy (buffer, rdata, copied);
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

    /* Done!  Cleanup and exit */
    MD_Cleanup (&rand_ctx2);
    MD_Cleanup (md);
    MD_Cleanup (&aes_ctx);
    MD_Cleanup (ctr);
    MD_Cleanup (rdata);
	
    return generated;
}

int zrtp_randstr2(unsigned char *buffer, uint32_t length) {
	zrtp_global_t zrtp;
	zrtp.rand_initialized = 0;
	return zrtp_randstr(&zrtp, buffer, length);
}
