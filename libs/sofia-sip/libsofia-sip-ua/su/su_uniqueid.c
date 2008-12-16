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
static int initialized = 0;

static void init(void);
static void init_node(void);

/* Constants */
static const unsigned version = 1;	/* Current version */
static const unsigned reserved = 128;	/* DCE variant */
#define granularity (10000000UL)
static const uint64_t mask60 = SU_U64_C(0xfffFFFFffffFFFF);
#define MAGIC (16384)

/* 100-nanosecond intervals between 15 October 1582 and 1 January 1900 */
static const uint64_t ntp_epoch =
(uint64_t)(141427) * (24 * 60 * 60L) * granularity;

/* State */
static uint64_t timestamp0 = 0;
static unsigned clock_sequence = MAGIC;
static unsigned char node[6];

FILE *urandom;

/*
 * Get current timestamp
 */
static uint64_t timestamp(void)
{
  uint64_t tl = su_ntp_now();
  uint64_t hi = su_ntp_hi(tl), lo = su_ntp_lo(tl);

  lo *= granularity;
  hi *= granularity;

  tl = hi + (lo >> 32) + ntp_epoch;

#ifdef TESTING
  printf("timestamp %08x-%08x\n", (unsigned)(tl >>32), (unsigned)tl);
#endif

  tl &= mask60;

  if (tl <= timestamp0)
    clock_sequence = (clock_sequence + 1) & (MAGIC - 1);

  timestamp0 = tl;

  return tl;
}

#if !HAVE_RANDOM
#define random() rand()
#define srandom(x) srand(x)
#endif

/*
 * Initialize clock_sequence and timestamp0
 */
static void init(void)
{
  int i;

  static uint32_t seed[32] = { 0 };
  su_time_t now;

  initialized = 1;

  /* Initialize our random number generator */
#if HAVE_DEV_URANDOM
  if (!urandom)
    urandom = fopen("/dev/urandom", "rb");
#endif	/* HAVE_DEV_URANDOM */

  if (urandom) {
    size_t len = fread(seed, sizeof seed, 1, urandom); (void)len;
  }
  else {
    for (i = 0; i < 16; i++) {
#if HAVE_CLOCK_GETTIME
      struct timespec ts;
      (void)clock_gettime(CLOCK_REALTIME, &ts);
      seed[2*i] ^= ts.tv_sec; seed[2*i+1] ^= ts.tv_nsec;
#endif
      su_time(&now);
      seed[2*i] ^= now.tv_sec; seed[2*i+1] ^= now.tv_sec;
    }

    seed[30] ^= getuid();
    seed[31] ^= getpid();
  }

#if HAVE_INITSTATE
  initstate(seed[0] ^ seed[1], (char *)&seed, sizeof(seed));
#else
  srand(seed[0] ^ seed[1]);
#endif

  clock_sequence = su_randint(0, MAGIC - 1);

  (void)timestamp();

  init_node();
}

#if HAVE_GETIFADDRS
#include <ifaddrs.h>
#if HAVE_NETPACKET_PACKET_H
#define HAVE_SOCKADDR_LL 1
#include <netpacket/packet.h>
#include <net/if_arp.h>
#endif
#endif

static
void init_node(void)
{
  size_t i;

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

      memcpy(node, sll->sll_addr, sizeof node);

      break;
#endif
    }

    freeifaddrs(results);

    if (ifa)
      return;			/* Success */
  }
#endif

  if (urandom) {
    size_t len = fread(node, sizeof node, 1, urandom); (void)len;
  }
  else for (i = 0; i < sizeof(node); i++) {
    unsigned r = random();
    node[i] = (r >> 24) ^ (r >> 16) ^ (r >> 8) ^ r;
  }

  node[0] |= 1;			/* "multicast" address */
}

size_t su_node_identifier(void *address, size_t addrlen)
{
  if (addrlen > sizeof node)
    addrlen = sizeof node;

  if (!initialized) init();

  memcpy(address, node, addrlen);

  return addrlen;
}

void su_guid_generate(su_guid_t *v)
{
  uint64_t time;
  unsigned clock;

  if (!initialized) init();

  time = timestamp();
  clock = clock_sequence;

  v->s.time_high_and_version =
    htons((unsigned short)(((time >> 48) & 0x0fff) | (version << 12)));
  v->s.time_mid = htons((unsigned short)((time >> 32) & 0xffff));
  v->s.time_low = htonl((unsigned long)(time & 0xffffffffUL));
  v->s.clock_seq_low = clock & 0xff;
  v->s.clock_seq_hi_and_reserved = (clock >> 8) | reserved;
  memcpy(v->s.node, node, sizeof(v->s.node));
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

/*
 * Generate random integer in range [lb, ub] (inclusive)
 */
int su_randint(int lb, int ub)
{
  unsigned rnd = 0;

  if (!initialized) init();

  if (urandom) {
    size_t len = fread(&rnd, 1, sizeof rnd, urandom); (void)len;
  }
  else
    rnd = random();

  if (ub - lb + 1 != 0)
    rnd %= (ub - lb + 1);

  return rnd + lb;
}

void *su_randmem(void *mem, size_t siz)
{
  size_t i;

  if (!initialized) init();

  if (urandom) {
    size_t len = fread(mem, 1, siz, urandom); (void)len;
  }
  else for (i = 0; i < siz; i++) {
    unsigned r = random();
    ((char *)mem)[i] = (r >> 24) ^ (r >> 16) ^ (r >> 8) ^ r;
  }

  return mem;
}

/** Get random number for RTP timestamps.
 *
 * This function returns a 32-bit random integer. It also initializes the
 * random number generator, if needed.
 */
uint32_t su_random(void)
{
  if (!initialized) init();

  if (urandom) {
    uint32_t rnd;
    size_t len = fread(&rnd, 1, sizeof rnd, urandom); (void)len;
    return rnd;
  }

  return (uint32_t)random();
}
