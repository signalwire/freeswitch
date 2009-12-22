/*
**  OSSP uuid - Universally Unique Identifier
**  Copyright (c) 2004-2008 Ralf S. Engelschall <rse@engelschall.com>
**  Copyright (c) 2004-2008 The OSSP Project <http://www.ossp.org/>
**
**  This file is part of OSSP uuid, a library for the generation
**  of UUIDs which can found at http://www.ossp.org/pkg/lib/uuid/
**
**  Permission to use, copy, modify, and distribute this software for
**  any purpose with or without fee is hereby granted, provided that
**  the above copyright notice and this permission notice appear in all
**  copies.
**
**  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
**  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
**  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
**  IN NO EVENT SHALL THE AUTHORS AND COPYRIGHT HOLDERS AND THEIR
**  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
**  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
**  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
**  USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
**  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
**  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
**  OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
**  SUCH DAMAGE.
**
**  uuid_prng.c: PRNG API implementation
*/

/* own headers (part 1/2) */
#include "uuid_ac.h"

/* system headers */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#if defined(WIN32)
#define WINVER 0x0500
#include <windows.h>
#include <wincrypt.h>
#endif

/* own headers (part 2/2) */
#include "uuid_time.h"
#include "uuid_prng.h"
#include "uuid_md5.h"

struct prng_st {
    int    dev; /* system PRNG device */
    md5_t *md5; /* local MD5 PRNG engine */
    long   cnt; /* time resolution compensation counter */
};

prng_rc_t prng_create(prng_t **prng)
{
#if !defined(WIN32)
    int fd = -1;
#endif
    struct timeval tv;
    pid_t pid;
    unsigned int i;

    /* sanity check argument(s) */
    if (prng == NULL)
        return PRNG_RC_ARG;

    /* allocate object */
    if ((*prng = (prng_t *)malloc(sizeof(prng_t))) == NULL)
        return PRNG_RC_MEM;

    /* try to open the system PRNG device */
    (*prng)->dev = -1;
#if !defined(WIN32)
    if ((fd = open("/dev/urandom", O_RDONLY)) == -1)
        fd = open("/dev/random", O_RDONLY|O_NONBLOCK);
    if (fd != -1) {
        (void)fcntl(fd, F_SETFD, FD_CLOEXEC);
        (*prng)->dev = fd;
    }
#endif

    /* initialize MD5 engine */
    if (md5_create(&((*prng)->md5)) != MD5_RC_OK) {
        free(*prng);
        return PRNG_RC_INT;
    }

    /* initialize time resolution compensation counter */
    (*prng)->cnt = 0;

    /* seed the C library PRNG once */
    (void)time_gettimeofday(&tv);
    pid = getpid();
    srand((unsigned int)(
        ((unsigned int)pid << 16)
        ^ (unsigned int)pid
        ^ (unsigned int)tv.tv_sec
        ^ (unsigned int)tv.tv_usec));
    for (i = (unsigned int)((tv.tv_sec ^ tv.tv_usec) & 0x1F); i > 0; i--)
        (void)rand();

    return PRNG_RC_OK;
}

prng_rc_t prng_data(prng_t *prng, void *data_ptr, size_t data_len)
{
    size_t n;
    unsigned char *p;
    struct {
        struct timeval tv;
        long cnt;
        int rnd;
    } entropy;
    unsigned char md5_buf[MD5_LEN_BIN];
    unsigned char *md5_ptr;
    size_t md5_len;
    int retries;
    int i;
#if defined(WIN32)
    HCRYPTPROV hProv;
#endif

    /* sanity check argument(s) */
    if (prng == NULL || data_len == 0)
        return PRNG_RC_ARG;

    /* prepare for generation */
    p = (unsigned char *)data_ptr;
    n = data_len;

    /* approach 1: try to gather data via stronger system PRNG device */
    if (prng->dev != -1) {
        retries = 0;
        while (n > 0) {
            i = (int)read(prng->dev, (void *)p, n);
            if (i <= 0) {
                if (retries++ > 16)
                    break;
                continue;
            }
            retries = 0;
            n -= (unsigned int)i;
            p += (unsigned int)i;
        }
    }
#if defined(WIN32)
    else {
        if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, 0))
            CryptGenRandom(hProv, n, p);
    }
#endif

    /* approach 2: try to gather data via weaker libc PRNG API. */
    while (n > 0) {
        /* gather new entropy */
        (void)time_gettimeofday(&(entropy.tv));  /* source: libc time */
        entropy.rnd = rand();                    /* source: libc PRNG */
        entropy.cnt = prng->cnt++;               /* source: local counter */

        /* pass entropy into MD5 engine */
        if (md5_update(prng->md5, (void *)&entropy, sizeof(entropy)) != MD5_RC_OK)
            return PRNG_RC_INT;

        /* store MD5 engine state as PRN output */
        md5_ptr = md5_buf;
        md5_len = sizeof(md5_buf);
        if (md5_store(prng->md5, (void **)(void *)&md5_ptr, &md5_len) != MD5_RC_OK)
            return PRNG_RC_INT;
        for (i = 0; i < MD5_LEN_BIN && n > 0; i++, n--)
            *p++ ^= md5_buf[i]; /* intentionally no assignment because arbitrary
                                   caller buffer content is leveraged, too */
    }

    return PRNG_RC_OK;
}

prng_rc_t prng_destroy(prng_t *prng)
{
    /* sanity check argument(s) */
    if (prng == NULL)
        return PRNG_RC_ARG;

    /* close PRNG device */
    if (prng->dev != -1)
        (void)close(prng->dev);

    /* destroy MD5 engine */
    (void)md5_destroy(prng->md5);

    /* free object */
    free(prng);

    return PRNG_RC_OK;
}

