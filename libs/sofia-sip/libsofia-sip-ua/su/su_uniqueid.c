/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Contact: Pekka Pessi <pekka.pessi@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

/**@defgroup su_uniqueid GloballyUniqueIDs
 *
 * Globally unique IDs and random integers.
 *
 * GloballyUniqueID or #su_guid_t is a 128-bit identifier based on current
 * time and MAC address of the node generating the ID. A new ID is generated
 * each time su_guid_generate() is called. Please note that such IDs are @b
 * not unique if multiple processes are run on the same node.
 *
 * Use su_guid_sprintf() to convert #su_guid_t to printable format.
 *
 * The random integers can be generated with functions
 * - su_randint(),
 * - su_randmem(), or
 * - su_random().
 */

/**@ingroup su_uniqueid
 *
 * @CFILE su_uniqueid.c Construct a GloballyUniqueID as per H.225.0 v2.
 *
 * @author Pekka Pessi <pessi@research.nokia.com>
 *
 * @date Created: Tue Apr 15 06:31:41 1997 pessi
 */

#include "config.h"

#if defined(_WIN32)
int _getpid(void);
#define getpid _getpid
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#if HAVE_UNISTD_H
#include <sys/types.h>
#include <unistd.h>
#endif

#include "sofia-sip/su.h"
#include "sofia-sip/su_time.h"
#include "sofia-sip/su_uniqueid.h"

/* For random number generator */
static FILE *urandom;

union state {
  uint64_t u64;
};

#if SU_HAVE_PTHREADS

#include <pthread.h>

#if __sun
#undef PTHREAD_ONCE_INIT
#define PTHREAD_ONCE_INIT {{ 0, 0, 0, PTHREAD_ONCE_NOTDONE }}
#endif

static pthread_once_t once = PTHREAD_ONCE_INIT;
static int done_once = 1;
static pthread_key_t state_key;

static void
init_once(void)
{
  pthread_key_create(&state_key, free);
#if HAVE_DEV_URANDOM
  urandom = fopen("/dev/urandom", "rb");
#endif	/* HAVE_DEV_URANDOM */
  done_once = 1;
}

#else
static int initialized;
#endif

static union state *
get_state(void)
{
  static union state state0[1];
  union state *retval;

#if SU_HAVE_PTHREADS

  pthread_once(&once, init_once);

  if (urandom)
    return NULL;

  retval = pthread_getspecific(state_key);
  if (retval) {
    return retval;
  }

  retval = calloc(1, sizeof *retval);
  if (retval != NULL)
    pthread_setspecific(state_key, retval);
  else
    retval = state0;

#else  /* !SU_HAVE_PTHREADS */

  if (urandom == NULL) {
#if HAVE_DEV_URANDOM
    urandom = fopen("/dev/urandom", "rb");
#endif	/* HAVE_DEV_URANDOM */
  }

  if (urandom)
    return NULL;

  retval = state0;

  if (initialized)
    return retval;
#endif

  {
    uint32_t seed[32];
    int i;
    union {
      uint32_t u32;
      pthread_t tid;
    } tid32 = { 0 };

    tid32.tid = pthread_self();

    memset(seed, 0, sizeof seed); /* Make valgrind happy */

    for (i = 0; i < 32; i += 2) {
#if HAVE_CLOCK_GETTIME
      struct timespec ts;
      (void)clock_gettime(CLOCK_REALTIME, &ts);
      seed[i] ^= ts.tv_sec; seed[i + 1] ^= ts.tv_nsec;
#else
      su_time_t now;
      su_time(&now);
      seed[i] ^= now.tv_sec; seed[i + 1] ^= now.tv_sec;
#endif
    }

    seed[0] ^= getuid();
    seed[1] ^= getpid();
    seed[2] ^= tid32.u32;
    seed[3] ^= (uint32_t)(intptr_t)retval;

    for (i = 0; i < 32; i+= 4) {
      retval->u64 += ((uint64_t)seed[i] << 32) | seed[i + 1];
      retval->u64 *= ((uint64_t)seed[i + 3] << 32) | seed[i + 2];
    }

    retval->u64 += (uint64_t)su_nanotime(NULL);
  }

  return retval;
}

#if !defined(WIN32) && !defined(WIN64)
void sofia_su_uniqueid_destructor(void)
  __attribute__((destructor));
#endif

void
sofia_su_uniqueid_destructor(void)
{
#if HAVE_DEV_URANDOM
	if (urandom) {
		fclose(urandom);
		urandom=NULL;
	}
#endif	/* HAVE_DEV_URANDOM */

#if SU_HAVE_PTHREADS
  if (done_once) {
    pthread_key_delete(state_key);
    done_once = 0;
  }
#endif
}

#if HAVE_GETIFADDRS
#include <ifaddrs.h>
#if HAVE_NETPACKET_PACKET_H
#define HAVE_SOCKADDR_LL 1
#include <netpacket/packet.h>
#include <net/if_arp.h>
#endif
#endif

#define SIZEOF_NODE 6
static
void init_node(uint8_t node[SIZEOF_NODE])
{
#if HAVE_GETIFADDRS && HAVE_SOCKADDR_LL
  struct ifaddrs *ifa, *results;

  if (getifaddrs(&results) == 0) {
    for (ifa = results; ifa; ifa = ifa->ifa_next) {
#if HAVE_SOCKADDR_LL
      struct sockaddr_ll const *sll = (void *)ifa->ifa_addr;

      if (sll == NULL || sll->sll_family != AF_PACKET)
	continue;
      switch (sll->sll_hatype) {
      case ARPHRD_ETHER:
      case ARPHRD_EETHER:
      case ARPHRD_IEEE802:
	break;
      default:
	continue;
      }

      memcpy(node, sll->sll_addr, SIZEOF_NODE);

      break;
#endif
    }

    freeifaddrs(results);

    if (ifa)
      return;			/* Success */
  }
#endif

  su_randmem(node, SIZEOF_NODE);
  node[0] |= 1;			/* "multicast" address */
}

static unsigned char node[SIZEOF_NODE];

size_t su_node_identifier(void *address, size_t addrlen)
{
  if (addrlen > SIZEOF_NODE)
    addrlen = SIZEOF_NODE;

  su_guid_generate(NULL);
  memcpy(address, node, addrlen);

  return addrlen;
}

void su_guid_generate(su_guid_t *v)
{
  /* Constants */
  static const unsigned version = 1;	/* Current version */
  static const unsigned reserved = 128;	/* DCE variant */
#define granularity (10000000UL)
  static const uint64_t mask60 = SU_U64_C(0xfffFFFFffffFFFF);
#define MAGIC (16384)

  /* 100-nanosecond intervals between 15 October 1582 and 1 January 1900 */
  static const uint64_t ntp_epoch =
    (uint64_t)(141427) * (24 * 60 * 60L) * granularity;

  static uint64_t timestamp0 = 0;
  static unsigned clock_sequence = MAGIC;

#if SU_HAVE_PTHREADS
  static pthread_mutex_t update = PTHREAD_MUTEX_INITIALIZER;
#endif

  uint64_t tl = su_ntp_now();
  uint64_t hi = su_ntp_hi(tl), lo = su_ntp_lo(tl);

  lo *= granularity;
  hi *= granularity;

  tl = hi + (lo >> 32) + ntp_epoch;

#ifdef TESTING
  printf("timestamp %08x-%08x\n", (unsigned)(tl >>32), (unsigned)tl);
#endif

  tl &= mask60;
  if (tl == 0) tl++;

#if SU_HAVE_PTHREADS
  pthread_mutex_lock(&update);
#endif

  if (timestamp0 == 0) {
    clock_sequence = su_randint(0, MAGIC - 1);
    init_node(node);
  }
  else if (tl <= timestamp0) {
    clock_sequence = (clock_sequence + 1) & (MAGIC - 1);
  }

  timestamp0 = tl;

#if SU_HAVE_PTHREADS
  pthread_mutex_unlock(&update);
#endif

  if (v) {
    v->s.time_high_and_version =
      htons((unsigned short)(((tl >> 48) & 0x0fff) | (version << 12)));
    v->s.time_mid = htons((unsigned short)((tl >> 32) & 0xffff));
    v->s.time_low = htonl((unsigned long)(tl & 0xffffffffUL));
    v->s.clock_seq_low = clock_sequence & 0xff;
    v->s.clock_seq_hi_and_reserved = (clock_sequence >> 8) | reserved;
    memcpy(v->s.node, node, sizeof(v->s.node));
  }
}

/*
 * Human-readable form of GloballyUniqueID
 */
isize_t su_guid_sprintf(char* buf, size_t len, su_guid_t const *v)
{
  char mybuf[su_guid_strlen + 1];
  sprintf(mybuf, "%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
	  (unsigned long)ntohl(v->s.time_low),
	  ntohs(v->s.time_mid),
	  ntohs(v->s.time_high_and_version),
	  v->s.clock_seq_low,
	  v->s.clock_seq_hi_and_reserved,
	  v->s.node[0], v->s.node[1], v->s.node[2],
	  v->s.node[3], v->s.node[4], v->s.node[5]);
  memcpy(buf, mybuf, len > sizeof(mybuf) ? sizeof(mybuf) : len);
  return su_guid_strlen;
}

uint64_t su_random64(void)
{
  union state *state = get_state();

  if (state) {
    /* Simple rand64 from AoCP */
    return state->u64 = state->u64 * 0X5851F42D4C957F2DULL + 1ULL;
  }
  else {
    uint64_t retval;
    size_t len = fread(&retval, 1, sizeof retval, urandom); (void)len;
    return retval;
  }
}

void *su_randmem(void *mem, size_t siz)
{
  union state *state = get_state();

  if (state) {
    size_t i;
    uint64_t r64;
    uint32_t r32;

    for (i = 0; i < siz; i += 4) {
      /* Simple rand64 from AoCP */
      state->u64 = r64 = state->u64 * 0X5851F42D4C957F2DULL + 1ULL;
      r32 = (uint32_t) (r64 >> 32) ^ (uint32_t)r64;
      if (siz - i >= 4)
	memcpy((char *)mem + i, &r32, 4);
      else
	memcpy((char *)mem + i, &r32, siz - i);
    }
  }
  else {
    size_t len = fread(mem, 1, siz, urandom); (void)len;
  }

  return mem;
}

/**
 * Generate random integer in range [lb, ub] (inclusive)
 */
int su_randint(int lb, int ub)
{
  uint64_t rnd;
  unsigned modulo = (unsigned)(ub - lb + 1);

  if (modulo != 0) {
    do {
      rnd = su_random64();
    } while (rnd / modulo == 0xffffFFFFffffFFFFULL / modulo);

    rnd %= modulo;
  }
  else {
    rnd = su_random64();
  }

  return (int)rnd + lb;
}

/** Get random 32-bit unsigned number.
 *
 */
uint32_t su_random(void)
{
  return (uint32_t)(su_random64() >> 16);
}
