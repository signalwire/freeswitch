/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2006 Nokia Corporation.
 * Copyright (C) 2006 Dimitri E. Prado.
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

/**@CFILE sres.c
 * @brief Sofia DNS Resolver implementation.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Teemu Jalava <Teemu.Jalava@nokia.com>
 * @author Mikko Haataja
 * @author Kai Vehmanen <kai.vehmanen@nokia.com>
 *         (work on the win32 nameserver discovery)
 * @author Dimitri E. Prado
 *         (initial version of win32 nameserver discovery)
 *
 * @todo The resolver should allow handling arbitrary records, too.
 */

#include "config.h"

#if HAVE_STDINT_H
#include <stdint.h>
#elif HAVE_INTTYPES_H
#include <inttypes.h>
#else
#if defined(HAVE_WIN32)
typedef _int8 int8_t;
typedef unsigned _int8 uint8_t;
typedef unsigned _int16 uint16_t;
typedef unsigned _int32 uint32_t;
#endif
#endif

#if HAVE_NETINET_IN_H
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#if HAVE_WINSOCK2_H
#include <winsock2.h>
#include <ws2tcpip.h>
#ifndef IPPROTO_IPV6		/* socklen_t is used with @RFC2133 API */
typedef int socklen_t;
#endif
#endif

#if HAVE_IPHLPAPI_H
#include <iphlpapi.h>
#endif

#if HAVE_IP_RECVERR || HAVE_IPV6_RECVERR
#include <linux/types.h>
#include <linux/errqueue.h>
#include <sys/uio.h>
#endif

#include <time.h>

#include "sofia-resolv/sres.h"
#include "sofia-resolv/sres_cache.h"
#include "sofia-resolv/sres_record.h"
#include "sofia-resolv/sres_async.h"

#include <sofia-sip/su_alloc.h>
#include <sofia-sip/su_strlst.h>
#include <sofia-sip/su_string.h>
#include <sofia-sip/su_errno.h>

#include "sofia-sip/htable.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <limits.h>

#include <assert.h>

#if HAVE_WINSOCK2_H
/* Posix send() */
su_inline
ssize_t sres_send(sres_socket_t s, void *b, size_t length, int flags)
{
  if (length > INT_MAX)
    length = INT_MAX;
  return (ssize_t)send(s, b, (int)length, flags);
}

/* Posix recvfrom() */
su_inline
ssize_t sres_recvfrom(sres_socket_t s, void *buffer, size_t length, int flags,
		      struct sockaddr *from, socklen_t *fromlen)
{
  int retval, ilen;

  if (fromlen)
    ilen = *fromlen;

  if (length > INT_MAX)
    length = INT_MAX;

  retval = recvfrom(s, buffer, (int)length, flags,
		    (void *)from, fromlen ? &ilen : NULL);

  if (fromlen)
    *fromlen = ilen;

  return (ssize_t)retval;
}

su_inline
int sres_close(sres_socket_t s)
{
  return closesocket(s);
}

#if !defined(IPPROTO_IPV6) && (_WIN32_WINNT < 0x0600)
#if HAVE_SIN6
#include <tpipv6.h>
#else
#if !defined(__MINGW32__)
struct sockaddr_storage {
    short ss_family;
    char ss_pad[126];
};
#endif
#endif
#endif
#else

#define sres_send(s,b,len,flags) send((s),(b),(len),(flags))
#define sres_recvfrom(s,b,len,flags,a,alen) \
  recvfrom((s),(b),(len),(flags),(a),(alen))
#define sres_close(s) close((s))
#define SOCKET_ERROR   (-1)
#define INVALID_SOCKET ((sres_socket_t)-1)
#endif

#define SRES_TIME_MAX ((time_t)LONG_MAX)

#if !HAVE_INET_PTON
int su_inet_pton(int af, char const *src, void *dst);
#else
#define su_inet_pton inet_pton
#endif
#if !HAVE_INET_NTOP
const char *su_inet_ntop(int af, void const *src, char *dst, size_t size);
#else
#define su_inet_ntop inet_ntop
#endif

#if defined(va_copy)
#elif defined(__va_copy)
#define va_copy(dst, src) __va_copy((dst), (src))
#else
#define va_copy(dst, src) (memcpy(&(dst), &(src), sizeof (va_list)))
#endif

/*
 * 3571 is a prime =>
 * we hash successive id values to different parts of hash tables
 */
#define Q_PRIME 3571
#define SRES_QUERY_HASH(q) ((q)->q_hash)

/**
 * How often to recheck nameserver information (seconds).
 */
#ifndef HAVE_WIN32
#define SRES_UPDATE_INTERVAL_SECS        5
#else
#define SRES_UPDATE_INTERVAL_SECS        180
#endif
void sres_cache_clean(sres_cache_t *cache, time_t now);

typedef struct sres_message    sres_message_t;
typedef struct sres_config     sres_config_t;
typedef struct sres_server     sres_server_t;
typedef struct sres_nameserver sres_nameserver_t;

/** Default path to resolv.conf */
static char const sres_conf_file_path[] = "/etc/resolv.conf";

/** EDNS0 support. @internal */
enum edns {
  edns_not_tried = -1,
  edns_not_supported = 0,
  edns0_configured = 1,
  edns0_supported = 2,
};

struct sres_server {
  sres_socket_t           dns_socket;

  char                    dns_name[48];     /**< Server name */
  struct sockaddr_storage dns_addr[1];  /**< Server node address */
  ssize_t                 dns_addrlen;  /**< Size of address */

  enum edns               dns_edns;	/**< Server supports edns. */

  /** ICMP/temporary error received, zero when successful. */
  time_t                  dns_icmp;
  /** Persistent error, zero when successful or timeout.
   *
   * Never selected if dns_error is SRES_TIME_MAX.
   */
  time_t                  dns_error;
};

HTABLE_DECLARE_WITH(sres_qtable, qt, sres_query_t, unsigned, size_t);

struct sres_resolver_s {
  su_home_t           res_home[1];

  void               *res_userdata;
  sres_cache_t       *res_cache;

  time_t              res_now;
  sres_qtable_t       res_queries[1];   /**< Table of active queries */

  char const         *res_cnffile;      /**< Configuration file name */
  char const        **res_options;      /**< Option strings */

  sres_config_t const *res_config;
  time_t              res_checked;

  unsigned long       res_updated;
  sres_update_f      *res_updcb;
  sres_async_t       *res_async;
  sres_schedule_f    *res_schedulecb;
  short               res_update_all;

  uint16_t            res_id;
  short               res_i_server;  /**< Current server to try
					(when doing round-robin) */
  short               res_n_servers; /**< Number of servers */
  sres_server_t     **res_servers;
};

/* Parsed configuration. @internal */
struct sres_config {
  su_home_t c_home[1];

  time_t c_modified;
  char const *c_filename;

  /* domain and search */
  char const *c_search[SRES_MAX_SEARCH + 1];

  /* nameserver */
  struct sres_nameserver {
    struct sockaddr_storage ns_addr[1];
    ssize_t ns_addrlen;
  } *c_nameservers[SRES_MAX_NAMESERVERS + 1];

  /* sortlist */
  struct sres_sortlist {
    struct sockaddr_storage addr[1];
    ssize_t addrlen;
    char const *name;
  } *c_sortlist[SRES_MAX_SORTLIST + 1];

  uint16_t    c_port;	     /**< Server port to use */

  /* options */
  struct sres_options {
    uint16_t timeout;
    uint16_t attempts;
    uint16_t ndots;
    enum edns edns;
    unsigned debug:1;
    unsigned rotate:1;
    unsigned check_names:1;
    unsigned inet6:1;
    unsigned ip6int:1;
    unsigned ip6bytestring:1;
  } c_opt;
};

struct sres_query_s {
  unsigned        q_hash;
  sres_resolver_t*q_res;
  sres_answer_f  *q_callback;
  sres_context_t *q_context;
  char           *q_name;
  time_t          q_timestamp;
  uint16_t        q_type;
  uint16_t        q_class;
  uint16_t        q_id;			/**< If nonzero, not answered */
  uint16_t        q_retry_count;
  uint8_t         q_n_servers;
  uint8_t         q_i_server;
  int8_t          q_edns;
  uint8_t         q_n_subs;
  sres_query_t   *q_subqueries[1 + SRES_MAX_SEARCH];
  sres_record_t **q_subanswers[1 + SRES_MAX_SEARCH];
};


struct sres_message {
  uint16_t m_offset;
  uint16_t m_size;
  char const *m_error;
  union {
    struct {
      /* Header defined in RFC 1035 section 4.1.1 (page 26) */
      uint16_t mh_id;		/* Query ID */
      uint16_t mh_flags;	/* Flags */
      uint16_t mh_qdcount;	/* Question record count */
      uint16_t mh_ancount;	/* Answer record count */
      uint16_t mh_nscount;	/* Authority records count */
      uint16_t mh_arcount;	/* Additional records count */
    } mp_header;
    uint8_t mp_data[1500 - 40];	/**< IPv6 datagram */
  } m_packet;
#define m_id      m_packet.mp_header.mh_id
#define m_flags   m_packet.mp_header.mh_flags
#define m_qdcount m_packet.mp_header.mh_qdcount
#define m_ancount m_packet.mp_header.mh_ancount
#define m_nscount m_packet.mp_header.mh_nscount
#define m_arcount m_packet.mp_header.mh_arcount
#define m_data    m_packet.mp_data
};

#define sr_refcount sr_record->r_refcount
#define sr_name     sr_record->r_name
#define sr_status   sr_record->r_status
#define sr_size     sr_record->r_size
#define sr_type     sr_record->r_type
#define sr_class    sr_record->r_class
#define sr_ttl      sr_record->r_ttl
#define sr_rdlen    sr_record->r_rdlen
#define sr_parsed   sr_record->r_parsed
#define sr_rdata    sr_generic->g_data

enum {
  SRES_HDR_QR = (1 << 15),
  SRES_HDR_QUERY = (0 << 11),
  SRES_HDR_IQUERY = (1 << 11),
  SRES_HDR_STATUS = (2 << 11),
  SRES_HDR_OPCODE = (15 << 11),	/* mask */
  SRES_HDR_AA = (1 << 10),
  SRES_HDR_TC = (1 << 9),
  SRES_HDR_RD = (1 << 8),
  SRES_HDR_RA = (1 << 7),
  SRES_HDR_RCODE = (15 << 0)	/* mask of return code */
};

HTABLE_PROTOS_WITH(sres_qtable, qt, sres_query_t, unsigned, size_t);

#define CHOME(cache) ((su_home_t *)(cache))

/** Get address from sockaddr storage. */
#if HAVE_SIN6
#define SS_ADDR(ss) \
  ((ss)->ss_family == AF_INET ? \
   (void *)&((struct sockaddr_in *)ss)->sin_addr : \
  ((ss)->ss_family == AF_INET6 ? \
   (void *)&((struct sockaddr_in6 *)ss)->sin6_addr : \
   (void *)&((struct sockaddr *)ss)->sa_data))
#else
#define SS_ADDR(ss) \
  ((ss)->ss_family == AF_INET ? \
   (void *)&((struct sockaddr_in *)ss)->sin_addr : \
   (void *)&((struct sockaddr *)ss)->sa_data)
#endif

static int sres_config_changed_servers(sres_config_t const *new_c,
				       sres_config_t const *old_c);
static sres_server_t **sres_servers_new(sres_resolver_t *res,
					sres_config_t const *c);
static sres_answer_f sres_resolving_cname;

/** Generate new 16-bit identifier for DNS query. */
static void
sres_gen_id(sres_resolver_t *res, sres_query_t *query)
{
  if (res->res_id == 0) {
    res->res_id = 1;
  }
  query->q_id = res->res_id++;
  query->q_hash = query->q_id * Q_PRIME;
}

/** Return true if we have a search list or a local domain name. */
static int
sres_has_search_domain(sres_resolver_t *res)
{
  return res->res_config->c_search[0] != NULL;
}

static void sres_resolver_destructor(void *);

sres_resolver_t *
sres_resolver_new_with_cache_va(char const *conf_file_path,
				sres_cache_t *cache,
				char const *options,
				va_list va);
static
sres_resolver_t *
sres_resolver_new_internal(sres_cache_t *cache,
			   sres_config_t const *config,
			   char const *conf_file_path,
			   char const **options);

static void sres_servers_close(sres_resolver_t *res,
			       sres_server_t **servers);

static int sres_servers_count(sres_server_t * const *servers);

static sres_socket_t sres_server_socket(sres_resolver_t *res,
					sres_server_t *dns);

static sres_query_t * sres_query_alloc(sres_resolver_t *res,
				       sres_answer_f *callback,
				       sres_context_t *context,
				       uint16_t type,
				       char const * domain);

static void sres_free_query(sres_resolver_t *res, sres_query_t *q);

static
int sres_sockaddr2string(sres_resolver_t *,
			 char name[], size_t namelen,
			 struct sockaddr const *);

static
sres_config_t *sres_parse_resolv_conf(sres_resolver_t *res,
				      char const **options);

static
sres_server_t *sres_next_server(sres_resolver_t *res,
				uint8_t *in_out_i,
				int always);

static
int sres_send_dns_query(sres_resolver_t *res, sres_query_t *q);

static
void sres_answer_subquery(sres_context_t *context,
			  sres_query_t *query,
			  sres_record_t **answers);

static
sres_record_t **
sres_combine_results(sres_resolver_t *res,
		     sres_record_t **search_results[SRES_MAX_SEARCH + 1]);

static
void sres_query_report_error(sres_query_t *q,
			     sres_record_t **answers);

static void
sres_resend_dns_query(sres_resolver_t *res, sres_query_t *q, int timeout);

static
sres_server_t *sres_server_by_socket(sres_resolver_t const *ts,
				     sres_socket_t socket);

static
int sres_resolver_report_error(sres_resolver_t *res,
			       sres_socket_t socket,
			       int errcode,
			       struct sockaddr_storage *remote,
			       socklen_t remotelen,
			       char const *info);

static
void sres_log_response(sres_resolver_t const *res,
		       sres_message_t const *m,
		       struct sockaddr_storage const *from,
		       sres_query_t const *query,
		       sres_record_t * const *reply);

static int sres_decode_msg(sres_resolver_t *res,
			   sres_message_t *m,
			   sres_query_t **,
			   sres_record_t ***aanswers);

static char const *sres_toplevel(char buf[], size_t bsize, char const *domain);

static sres_record_t *sres_create_record(sres_resolver_t *,
					 sres_message_t *m,
					 int nth);

static sres_record_t *sres_init_rr_soa(sres_cache_t *cache,
				       sres_soa_record_t *,
				       sres_message_t *m);
static sres_record_t *sres_init_rr_a(sres_cache_t *cache,
				     sres_a_record_t *,
				     sres_message_t *m);
static sres_record_t *sres_init_rr_a6(sres_cache_t *cache,
				      sres_a6_record_t *,
				      sres_message_t *m);
static sres_record_t *sres_init_rr_aaaa(sres_cache_t *cache,
					sres_aaaa_record_t *,
					sres_message_t *m);
static sres_record_t *sres_init_rr_cname(sres_cache_t *cache,
					 sres_cname_record_t *,
					 sres_message_t *m);
static sres_record_t *sres_init_rr_ptr(sres_cache_t *cache,
				       sres_ptr_record_t *,
				       sres_message_t *m);
static sres_record_t *sres_init_rr_srv(sres_cache_t *cache,
				       sres_srv_record_t *,
				       sres_message_t *m);
static sres_record_t *sres_init_rr_naptr(sres_cache_t *cache,
					 sres_naptr_record_t *,
					 sres_message_t *m);
static sres_record_t *sres_init_rr_unknown(sres_cache_t *cache,
					   sres_common_t *r,
					   sres_message_t *m);

static sres_record_t *sres_create_error_rr(sres_cache_t *cache,
                                           sres_query_t const *q,
                                           uint16_t errcode);

static void m_put_uint16(sres_message_t *m, uint16_t h);
static void m_put_uint32(sres_message_t *m, uint32_t w);

static uint16_t m_put_domain(sres_message_t *m,
                             char const *domain,
                             uint16_t top,
                             char const *topdomain);

static uint32_t m_get_uint32(sres_message_t *m);
static uint16_t m_get_uint16(sres_message_t *m);
static uint8_t m_get_uint8(sres_message_t *m);

static unsigned m_get_string(char *d, unsigned n, sres_message_t *m, uint16_t offset);
static unsigned m_get_domain(char *d, unsigned n, sres_message_t *m, uint16_t offset);

/* ---------------------------------------------------------------------- */

#define SU_LOG sresolv_log

#include <sofia-sip/su_debug.h>

#ifdef HAVE_WIN32
#include <winreg.h>
#endif

/**@ingroup sresolv_env
 *
 * Environment variable determining the debug log level for @b sresolv
 * module.
 *
 * The SRESOLV_DEBUG environment variable is used to determine the debug
 * logging level for @b sresolv module. The default level is 3.
 *
 * @sa <sofia-sip/su_debug.h>, sresolv_log, SOFIA_DEBUG
 */
#ifdef DOXYGEN
extern char const SRESOLV_DEBUG[]; /* dummy declaration for Doxygen */
#endif

#ifndef SU_DEBUG
#define SU_DEBUG 3
#endif

/**Debug log for @b sresolv module.
 *
 * The sresolv_log is the log object used by @b sresolv module. The level of
 * #sresolv_log is set using #SRESOLV_DEBUG environment variable.
 */
su_log_t sresolv_log[] = { SU_LOG_INIT("sresolv", "SRESOLV_DEBUG", SU_DEBUG) };

/** Internal errors */
enum {
  SRES_EDNS0_ERR = 255		/**< Server did not support EDNS. */
};

/* ---------------------------------------------------------------------- */

/**Create a resolver.
 *
 * Allocate and initialize a new sres resolver object. The resolver object
 * contains the parsed resolv.conf file, a cache object containing past
 * answers from DNS, and a list of active queries. The default resolv.conf
 * file can be overriden by giving the name of the configuration file as @a
 * conf_file_path.
 *
 * @param conf_file_path name of the resolv.conf configuration file
 *
 * @return A pointer to a newly created sres resolver object, or NULL upon
 * an error.
 */
sres_resolver_t *
sres_resolver_new(char const *conf_file_path)
{
  return sres_resolver_new_internal(NULL, NULL, conf_file_path, NULL);
}

/** Copy a resolver.
 *
 * Make a copy of resolver sharing the configuration and cache with old
 * resolver.
 */
sres_resolver_t *sres_resolver_copy(sres_resolver_t *res)
{
  char const *cnffile;
  sres_config_t *config;
  sres_cache_t *cache;
  char const **options;

  if (!res)
    return NULL;

  cnffile = res->res_cnffile;
  config = su_home_ref(res->res_config->c_home);
  cache = res->res_cache;
  options = res->res_options;

  return sres_resolver_new_internal(cache, config, cnffile, options);
}

/**New resolver object.
 *
 * Allocate and initialize a new sres resolver object. The resolver object
 * contains the parsed resolv.conf file, a cache object containing past
 * answers from DNS, and a list of active queries. The default resolv.conf
 * file can be overriden by giving the name of the configuration file as @a
 * conf_file_path.
 *
 * It is also possible to override the values in the resolv.conf and
 * RES_OPTIONS by giving the directives in the NULL-terminated list.
 *
 * @param conf_file_path name of the resolv.conf configuration file
 * @param cache          optional pointer to a resolver cache (may be NULL)
 * @param option, ...    list of resolv.conf options directives
 *                       (overriding options in conf_file)
 *
 * @par Environment Variables
 * - #LOCALDOMAIN overrides @c domain or @c search directives
 * - #RES_OPTIONS overrides values of @a options in resolv.conf
 * - #SRES_OPTIONS overrides values of @a options in resolv.conf, #RES_OPTIONS,
 *   and @a options, ... list given as argument for this function
 *
 * @return A pointer to a newly created sres resolver object, or NULL upon
 * an error.
 */
sres_resolver_t *
sres_resolver_new_with_cache(char const *conf_file_path,
			     sres_cache_t *cache,
			     char const *option, ...)
{
  sres_resolver_t *retval;
  va_list va;
  va_start(va, option);
  retval = sres_resolver_new_with_cache_va(conf_file_path, cache, option, va);
  va_end(va);
  return retval;
}

/**Create a resolver.
 *
 * Allocate and initialize a new sres resolver object.
 *
 * This is a stdarg version of sres_resolver_new_with_cache().
 */
sres_resolver_t *
sres_resolver_new_with_cache_va(char const *conf_file_path,
				sres_cache_t *cache,
				char const *option,
				va_list va)
{
  va_list va0;
  size_t i;
  char const *o, *oarray[16], **olist = oarray;
  sres_resolver_t *res;

  va_copy(va0, va);

  for (i = 0, o = option; o; o = va_arg(va0, char const *)) {
    if (i < 16)
      olist[i] = o;
    i++;
  }

  if (i >= 16) {
    olist = malloc((i + 1) * sizeof *olist);
    if (!olist)
      return NULL;
    for (i = 0, o = option; o; o = va_arg(va, char const *)) {
      olist[i++] = o;
      i++;
    }
  }
  olist[i] = NULL;
  res = sres_resolver_new_internal(cache, NULL, conf_file_path, olist);
  if (olist != oarray)
    free(olist);

  va_end(va0);

  return res;
}

sres_resolver_t *
sres_resolver_new_internal(sres_cache_t *cache,
			   sres_config_t const *config,
			   char const *conf_file_path,
			   char const **options)
{
  sres_resolver_t *res;
  size_t i, n, len;
  char **array, *o, *end;

  for (n = 0, len = 0; options && options[n]; n++)
    len += strlen(options[n]) + 1;

  res = su_home_new(sizeof(*res) + (n + 1) * (sizeof *options) + len);

  if (res == NULL)
    return NULL;

  array = (void *)(res + 1);
  o = (void *)(array + n + 1);
  end = o + len;

  for (i = 0; options && options[i]; i++)
    o = memccpy(array[i] = o, options[i], '\0', len - (end - o));
  assert(o == end);

  su_home_destructor(res->res_home, sres_resolver_destructor);

  while (res->res_id == 0) {
#if HAVE_DEV_URANDOM
    int fd;
    if ((fd = open("/dev/urandom", O_RDONLY, 0)) != -1) {
      size_t len = read(fd, &res->res_id, (sizeof res->res_id)); (void)len;
      close(fd);
    }
    else
#endif
    res->res_id = time(NULL);
  }

  time(&res->res_now);

  if (cache)
    res->res_cache = sres_cache_ref(cache);
  else
    res->res_cache = sres_cache_new(0);

  res->res_config = config;

  if (conf_file_path && conf_file_path != sres_conf_file_path)
    res->res_cnffile = su_strdup(res->res_home, conf_file_path);
  else
    res->res_cnffile = conf_file_path = sres_conf_file_path;

  if (!res->res_cache || !res->res_cnffile) {
    perror("sres: malloc");
  }
  else if (sres_qtable_resize(res->res_home, res->res_queries, 0) < 0) {
    perror("sres: res_qtable_resize");
  }
  else if (sres_resolver_update(res, config == NULL) < 0) {
    perror("sres: sres_resolver_update");
  }
  else {
    return res;
  }

  sres_resolver_unref(res);

  return NULL;
}

/** Increase reference count on a resolver object. */
sres_resolver_t *
sres_resolver_ref(sres_resolver_t *res)
{
  return su_home_ref(res->res_home);
}

/** Decrease the reference count on a resolver object.  */
void
sres_resolver_unref(sres_resolver_t *res)
{
  su_home_unref(res->res_home);
}

/** Set userdata pointer.
 *
 * @return New userdata pointer.
 *
 * @ERRORS
 * @ERROR EFAULT @a res points outside the address space
 */
void *
sres_resolver_set_userdata(sres_resolver_t *res,
			   void *userdata)
{
  void *old;

  if (!res)
    return su_seterrno(EFAULT), (void *)NULL;

  old = res->res_userdata, res->res_userdata = userdata;

  return old;
}

/**Get userdata pointer.
 *
 * @return Userdata pointer.
 *
 * @ERRORS
 * @ERROR EFAULT @a res points outside the address space
 */
void *
sres_resolver_get_userdata(sres_resolver_t const *res)
{
  if (res == NULL)
    return su_seterrno(EFAULT), (void *)NULL;
  else
    return res->res_userdata;
}

/** Set async object.
 *
 * @return Set async object.
 *
 * @ERRORS
 * @ERROR EFAULT @a res points outside the address space
 * @ERROR EALREADY different async callback already set
 */
sres_async_t *
sres_resolver_set_async(sres_resolver_t *res,
			sres_update_f *callback,
			sres_async_t *async,
			int update_all)
{
  if (!res)
    return su_seterrno(EFAULT), (void *)NULL;

  if (res->res_updcb && res->res_updcb != callback)
    return su_seterrno(EALREADY), (void *)NULL;

  res->res_async = async;
  res->res_updcb = callback;
  res->res_update_all = callback && update_all != 0;

  return async;
}

/** Get async object */
sres_async_t *
sres_resolver_get_async(sres_resolver_t const *res,
			sres_update_f *callback)
{
  if (res == NULL)
    return su_seterrno(EFAULT), (void *)NULL;
  else if (callback == NULL)
    return res->res_async ? (sres_async_t *)-1 : 0;
  else if (res->res_updcb != callback)
    return NULL;
  else
    return res->res_async;
}

/** Register resolver timer callback. */
int sres_resolver_set_timer_cb(sres_resolver_t *res,
			       sres_schedule_f *callback,
			       sres_async_t *async)
{
  if (res == NULL)
    return su_seterrno(EFAULT);
  if (res->res_async != async)
    return su_seterrno(EALREADY);

  res->res_schedulecb = callback;
  return 0;
}

/**Send a DNS query.
 *
 * Sends a DNS query with specified @a type and @a domain to the DNS server.
 * When an answer is received, the @a callback function is called with
 * @a context and returned records as arguments.
 *
 * The sres resolver takes care of retransmitting the query if a root object
 * is associate with the resolver or if sres_resolver_timer() is called in
 * regular intervals. It generates an error record with nonzero status if no
 * response is received.
 *
 * @param res pointer to resolver
 * @param callback function called when query is answered or times out
 * @param context pointer given as an extra argument to @a callback function
 * @param type record type to query (see #sres_qtypes)
 * @param domain name to query
 *
 * Query types also indicate the record type of the result.
 * Any record can be queried with #sres_qtype_any.
 * Well-known query types understood and decoded by @b sres include
 * #sres_type_a,
 * #sres_type_aaaa,
 * #sres_type_cname,
 * #sres_type_ptr
 * #sres_type_soa,
 * #sres_type_aaaa,
 * #sres_type_srv, and
 * #sres_type_naptr.
 *
 * Deprecated query type #sres_type_a6 is also decoded.
 *
 * @note The domain name is @b not concatenated with the domains from seach
 * path or with the local domain. Use sres_search() in order to try domains
 * in search path.
 *
 * @sa sres_search(), sres_blocking_query(), sres_cached_answers(),
 * sres_query_sockaddr()
 *
 * @ERRORS
 * @ERROR EFAULT @a res or @a domain point outside the address space
 * @ERROR ENAMETOOLONG @a domain is longer than SRES_MAXDNAME
 * @ERROR ENETDOWN no DNS servers configured
 * @ERROR ENOMEM memory exhausted
 */
sres_query_t *
sres_query(sres_resolver_t *res,
	   sres_answer_f *callback,
	   sres_context_t *context,
	   uint16_t type,
	   char const *domain)
{
  sres_query_t *query = NULL;
  size_t dlen;

  char b[8];
  SU_DEBUG_9(("sres_query(%p, %p, %s, \"%s\") called\n",
			  (void *)res, (void *)context, sres_record_type(type, b), domain));

  if (res == NULL || domain == NULL)
    return su_seterrno(EFAULT), (void *)NULL;

  dlen = strlen(domain);
  if (dlen > SRES_MAXDNAME ||
      (dlen == SRES_MAXDNAME && domain[dlen - 1] != '.')) {
    su_seterrno(ENAMETOOLONG);
    return NULL;
  }

  /* Reread resolv.conf if needed */
  sres_resolver_update(res, 0);

  if (res->res_n_servers == 0)
    return (void)su_seterrno(ENETDOWN), (sres_query_t *)NULL;

  query = sres_query_alloc(res, callback, context, type, domain);

  if (query && sres_send_dns_query(res, query) != 0)
    sres_free_query(res, query), query = NULL;

  return query;
}

/**Search DNS.
 *
 * Sends DNS queries with specified @a type and @a name to the DNS server.
 * If the @a name does not contain enought dots, the search domains are
 * appended to the name and resulting domain name are also queried. When
 * answer to all the search domains is received, the @a callback function
 * is called with @a context and combined records from answers as arguments.
 *
 * The sres resolver takes care of retransmitting the queries if a root
 * object is associate with the resolver or if sres_resolver_timer() is
 * called in regular intervals. It generates an error record with nonzero
 * status if no response is received.
 *
 * @param res pointer to resolver object
 * @param callback pointer to completion function
 * @param context argument given to the completion function
 * @param type record type to search (or sres_qtype_any for any record)
 * @param name host or domain name to search from DNS
 *
 * @ERRORS
 * @ERROR EFAULT @a res or @a domain point outside the address space
 * @ERROR ENAMETOOLONG @a domain is longer than SRES_MAXDNAME
 * @ERROR ENETDOWN no DNS servers configured
 * @ERROR ENOMEM memory exhausted
 *
 * @sa sres_query(), sres_blocking_search(), sres_search_cached_answers().
 */
sres_query_t *
sres_search(sres_resolver_t *res,
	    sres_answer_f *callback,
	    sres_context_t *context,
	    uint16_t type,
	    char const *name)
{
  char const *domain = name;
  sres_query_t *query = NULL;
  size_t dlen;
  unsigned dots; char const *dot;
  char b[8];

  SU_DEBUG_9(("sres_search(%p, %p, %s, \"%s\") called\n",
			  (void *)res, (void *)context, sres_record_type(type, b), domain));

  if (res == NULL || domain == NULL)
    return su_seterrno(EFAULT), (void *)NULL;

  dlen = strlen(domain);
  if (dlen > SRES_MAXDNAME ||
      (dlen == SRES_MAXDNAME && domain[dlen - 1] != '.')) {
    su_seterrno(ENAMETOOLONG);
    return NULL;
  }

  sres_resolver_update(res, 0);

  if (res->res_n_servers == 0)
    return (void)su_seterrno(ENETDOWN), (sres_query_t *)NULL;

  if (domain[dlen - 1] == '.')
    /* Domain ends with dot - do not search */
    dots = res->res_config->c_opt.ndots;
  else if (sres_has_search_domain(res))
    for (dots = 0, dot = strchr(domain, '.');
	 dots < res->res_config->c_opt.ndots && dot;
	 dots++, dot = strchr(dot + 1, '.'))
      ;
  else
    dots = 0;

  query = sres_query_alloc(res, callback, context, type, domain);

  if (query) {
    /* Create sub-query for each search domain */
    if (dots < res->res_config->c_opt.ndots) {
      sres_query_t *sub;
      int i, subs;
      size_t len;
      char const *const *domains = res->res_config->c_search;
      char search[SRES_MAXDNAME + 1];

      assert(dlen < SRES_MAXDNAME);

      memcpy(search, domain, dlen);
      search[dlen++] = '.';
      search[dlen] = '\0';

      for (i = 0, subs = 0; i <= SRES_MAX_SEARCH; i++) {
	if (domains[i]) {
	  len = strlen(domains[i]);

	  if (dlen + len + 1 > SRES_MAXDNAME)
	    continue;

	  memcpy(search + dlen, domains[i], len);
	  search[dlen + len] = '.';
	  search[dlen + len + 1] = '\0';
	  sub = sres_query_alloc(res, sres_answer_subquery, (void *)query,
				 type, search);

	  if (sub == NULL) {
	  }
	  else if (sres_send_dns_query(res, sub) == 0) {
	    query->q_subqueries[i] = sub;
	  }
	  else {
	    sres_free_query(res, sub), sub = NULL;
	  }
	  subs += sub != NULL;
	}
      }

      query->q_n_subs = subs;
    }

    if (sres_send_dns_query(res, query) != 0) {
      if (!query->q_n_subs)
	sres_free_query(res, query), query = NULL;
      else
	query->q_id = 0;
    }
  }

  return query;
}

/** Make a reverse DNS query.
 *
 * Send a query to DNS server with specified @a type and domain name formed
 * from the socket address @a addr. The sres resolver takes care of
 * retransmitting the query if a root object is associate with the resolver or
 * if sres_resolver_timer() is called in regular intervals. It generates an
 * error record with nonzero status if no response is received.
 *
 * @param res pointer to resolver
 * @param callback function called when query is answered or times out
 * @param context pointer given as an extra argument to @a callback function
 * @param type record type to query (or sres_qtype_any for any record)
 * @param addr socket address structure
 *
 * The @a type should be #sres_type_ptr. The @a addr should contain either
 * IPv4 (AF_INET) or IPv6 (AF_INET6) address.
 *
 * If the #SRES_OPTIONS environment variable, #RES_OPTIONS environment
 * variable, or an "options" entry in resolv.conf file contains an option
 * "ip6-dotint", the IPv6 addresses are resolved using suffix ".ip6.int"
 * instead of the standard ".ip6.arpa" suffix.
 *
 * @ERRORS
 * @ERROR EAFNOSUPPORT address family specified in @a addr is not supported
 * @ERROR ENETDOWN no DNS servers configured
 * @ERROR EFAULT @a res or @a addr point outside the address space
 * @ERROR ENOMEM memory exhausted
 *
 * @sa sres_query(), sres_blocking_query_sockaddr(),
 * sres_cached_answers_sockaddr()
 *
 */
sres_query_t *
sres_query_sockaddr(sres_resolver_t *res,
		    sres_answer_f *callback,
		    sres_context_t *context,
		    uint16_t type,
		    struct sockaddr const *addr)
{
  char name[80];

  if (!res || !addr)
    return su_seterrno(EFAULT), (void *)NULL;

  if (!sres_sockaddr2string(res, name, sizeof(name), addr))
    return NULL;

  return sres_query(res, callback, context, type, name);
}


/** Make a DNS query.
 *
 * @deprecated Use sres_query() instead.
 */
sres_query_t *
sres_query_make(sres_resolver_t *res,
		sres_answer_f *callback,
		sres_context_t *context,
		int dummy,
		uint16_t type,
		char const *domain)
{
  return sres_query(res, callback, context, type, domain);
}

/** Make a reverse DNS query.
 *
 * @deprecated Use sres_query_sockaddr() instead.
 */
sres_query_t *
sres_query_make_sockaddr(sres_resolver_t *res,
			 sres_answer_f *callback,
			 sres_context_t *context,
			 int dummy,
			 uint16_t type,
			 struct sockaddr const *addr)
{
  char name[80];

  if (!res || !addr)
    return su_seterrno(EFAULT), (void *)NULL;

  if (!sres_sockaddr2string(res, name, sizeof(name), addr))
    return NULL;

  return sres_query_make(res, callback, context, dummy, type, name);
}


/** Bind a query with another callback and context pointer.
 *
 * @param query pointer to a query object to bind
 * @param callback pointer to new callback function (may be NULL)
 * @param context pointer to callback context (may be NULL)
*/
void sres_query_bind(sres_query_t *query,
                     sres_answer_f *callback,
                     sres_context_t *context)
{
  if (query) {
    query->q_callback = callback;
    query->q_context = context;
  }
}

/**Get a list of matching (type/domain) records from cache.
 *
 * @return
 * pointer to an array of pointers to cached records, or
 * NULL if no entry was found.
 *
 * @ERRORS
 * @ERROR ENAMETOOLONG @a domain is longer than SRES_MAXDNAME
 * @ERROR ENOENT no cached records were found
 * @ERROR EFAULT @a res or @a domain point outside the address space
 * @ERROR ENOMEM memory exhausted
 */
sres_record_t **
sres_cached_answers(sres_resolver_t *res,
		    uint16_t type,
		    char const *domain)
{
  sres_record_t **result;
  char rooted_domain[SRES_MAXDNAME];

  if (!res)
    return su_seterrno(EFAULT), (void *)NULL;

  domain = sres_toplevel(rooted_domain, sizeof rooted_domain, domain);

  if (!domain)
    return NULL;

  if (!sres_cache_get(res->res_cache, type, domain, &result))
    return su_seterrno(ENOENT), (void *)NULL;

  return result;
}

/**Search for a list of matching (type/name) records from cache.
 *
 * @return
 * pointer to an array of pointers to cached records, or
 * NULL if no entry was found.
 *
 * @ERRORS
 * @ERROR ENAMETOOLONG @a name or resulting domain is longer than SRES_MAXDNAME
 * @ERROR ENOENT no cached records were found
 * @ERROR EFAULT @a res or @a domain point outside the address space
 * @ERROR ENOMEM memory exhausted
 *
 * @sa sres_search(), sres_cached_answers()
 */
sres_record_t **
sres_search_cached_answers(sres_resolver_t *res,
			   uint16_t type,
			   char const *name)
{
  char const *domain = name;
  sres_record_t **search_results[SRES_MAX_SEARCH + 1] = { NULL };
  char rooted_domain[SRES_MAXDNAME];
  unsigned dots; char const *dot;
  size_t found = 0;
  int i;

  SU_DEBUG_9(("sres_search_cached_answers(%p, %s, \"%s\") called\n",
	      (void *)res, sres_record_type(type, rooted_domain), domain));

  if (!res || !name)
    return su_seterrno(EFAULT), (void *)NULL;

  if (sres_has_search_domain(res))
    for (dots = 0, dot = strchr(domain, '.');
	 dots < res->res_config->c_opt.ndots && dot;
	 dots++, dot = strchr(dot + 1, '.'))
      ;
  else
    dots = 0;

  domain = sres_toplevel(rooted_domain, sizeof rooted_domain, domain);

  if (!domain)
    return NULL;

  if (sres_cache_get(res->res_cache, type, domain, &search_results[0]))
    found = 1;

  if (dots < res->res_config->c_opt.ndots) {
    char const *const *domains = res->res_config->c_search;
    size_t dlen = strlen(domain);

    for (i = 0; domains[i] && i < SRES_MAX_SEARCH; i++) {
      size_t len = strlen(domains[i]);
      if (dlen + len + 1 >= SRES_MAXDNAME)
	continue;
      if (domain != rooted_domain)
	domain = memcpy(rooted_domain, domain, dlen);
      memcpy(rooted_domain + dlen, domains[i], len);
      strcpy(rooted_domain + dlen + len, ".");
      if (sres_cache_get(res->res_cache, type, domain, search_results + i + 1))
	found++;
    }
  }

  if (found == 0)
    return su_seterrno(ENOENT), (void *)NULL;

  if (found == 1) {
    for (i = 0; i <= SRES_MAX_SEARCH; i++)
      if (search_results[i])
	return search_results[i];
  }

  return sres_combine_results(res, search_results);
}

/**Get a list of matching (type/domain) reverse records from cache.
 *
 * @param res pointer to resolver
 * @param type record type to query (or sres_qtype_any for any record)
 * @param addr socket address structure
 *
 * The @a type should be #sres_type_ptr. The @a addr should contain either
 * IPv4 (AF_INET) or IPv6 (AF_INET6) address.
 *
 * If the #SRES_OPTIONS environment variable, #RES_OPTIONS environment
 * variable or an "options" entry in resolv.conf file contains an option
 * "ip6-dotint", the IPv6 addresses are resolved using suffix ".ip6.int"
 * instead of default ".ip6.arpa".
 *
 * @retval
 * pointer to an array of pointers to cached records, or
 * NULL if no entry was found.
 *
 * @ERRORS
 * @ERROR EAFNOSUPPORT address family specified in @a addr is not supported
 * @ERROR ENOENT no cached records were found
 * @ERROR EFAULT @a res or @a addr point outside the address space
 * @ERROR ENOMEM memory exhausted
 */
sres_record_t **
sres_cached_answers_sockaddr(sres_resolver_t *res,
			     uint16_t type,
			     struct sockaddr const *addr)
{
  sres_record_t **result;
  char name[80];

  if (!res || !addr)
    return su_seterrno(EFAULT), (void *)NULL;

  if (!sres_sockaddr2string(res, name, sizeof name, addr))
    return NULL;

  if (!sres_cache_get(res->res_cache, type, name, &result))
    su_seterrno(ENOENT), (void *)NULL;

  return result;
}

/** Set the priority of the matching cached SRV record.
 *
 * The SRV records with the domain name, target and port are matched and
 * their priority value is adjusted. This function is used to implement
 * greylisting of SIP servers.
 *
 * @param res      pointer to resolver
 * @param domain   domain name of the SRV record(s) to modify
 * @param target   SRV target of the SRV record(s) to modify
 * @param port     port number of SRV record(s) to modify
 *                 (in host byte order)
 * @param ttl      new ttl for SRV records of the domain
 * @param priority new priority value (0=highest, 65535=lowest)
 *
 * @sa sres_cache_set_srv_priority()
 *
 * @NEW_1_12_8
 */
int sres_set_cached_srv_priority(sres_resolver_t *res,
				 char const *domain,
				 char const *target,
				 uint16_t port,
				 uint32_t ttl,
				 uint16_t priority)
{
  char rooted_domain[SRES_MAXDNAME];

  if (res == NULL || res->res_cache == NULL)
    return su_seterrno(EFAULT);

  domain = sres_toplevel(rooted_domain, sizeof rooted_domain, domain);

  if (!domain)
    return -1;

  return sres_cache_set_srv_priority(res->res_cache,
				     domain, target, port,
				     ttl, priority);
}


/** Sort answers. */
int
sres_sort_answers(sres_resolver_t *res, sres_record_t **answers)
{
  int i, j;

  if (res == NULL || answers == NULL)
    return su_seterrno(EFAULT);

  if (answers[0] == NULL || answers[1] == NULL)
    return 0;

  /* Simple insertion sorting */
  /*
   * We do not use qsort because we want later extend this to sort
   * local A records first etc.
   */
  for (i = 1; answers[i]; i++) {
    for (j = 0; j < i; j++) {
      if (sres_record_compare(answers[i], answers[j]) < 0)
	break;
    }
    if (j < i) {
      sres_record_t *r = answers[i];
      for (; j < i; i--) {
	answers[i] = answers[i - 1];
      }
      answers[j] = r;
    }
  }

  return 0;
}

/** Sort and filter query results */
int
sres_filter_answers(sres_resolver_t *res,
		    sres_record_t **answers,
		    uint16_t type)
{
  int i, n;

  if (res == NULL || answers == NULL)
    return su_seterrno(EFAULT);

  for (n = 0, i = 0; answers[i]; i++) {
    if (answers[i]->sr_record->r_status ||
	answers[i]->sr_record->r_class != sres_class_in ||
	(type != 0 && answers[i]->sr_record->r_type != type)) {
      sres_free_answer(res, answers[i]);
      continue;
    }
    answers[n++] = answers[i];
  }
  answers[n] = NULL;

  sres_sort_answers(res, answers);

  return n;
}


/** Free and zero one record. */
void sres_free_answer(sres_resolver_t *res, sres_record_t *answer)
{
  if (res && answer)
    sres_cache_free_one(res->res_cache, answer);
}

/** Free and zero an array of records.
 *
 * The array of records can be returned by sres_cached_answers() or
 * given by callback function.
 */
void
sres_free_answers(sres_resolver_t *res,
		  sres_record_t **answers)
{
  if (res && answers)
    sres_cache_free_answers(res->res_cache, answers);
}

/** Convert type to its name. */
char const *sres_record_type(int type, char buffer[8])
{
  switch (type) {
  case sres_type_a: return "A";
  case sres_type_ns: return "NS";
  case sres_type_mf: return "MF";
  case sres_type_cname: return "CNAME";
  case sres_type_soa: return "SOA";
  case sres_type_mb: return "MB";
  case sres_type_mg: return "MG";
  case sres_type_mr: return "MR";
  case sres_type_null: return "NULL";
  case sres_type_wks: return "WKS";
  case sres_type_ptr: return "PTR";
  case sres_type_hinfo: return "HINFO";
  case sres_type_minfo: return "MINFO";
  case sres_type_mx: return "MX";
  case sres_type_txt: return "TXT";
  case sres_type_rp: return "RP";
  case sres_type_afsdb: return "AFSDB";
  case sres_type_x25: return "X25";
  case sres_type_isdn: return "ISDN";
  case sres_type_rt: return "RT";
  case sres_type_nsap: return "NSAP";
  case sres_type_nsap_ptr: return "NSAP_PTR";
  case sres_type_sig: return "SIG";
  case sres_type_key: return "KEY";
  case sres_type_px: return "PX";
  case sres_type_gpos: return "GPOS";
  case sres_type_aaaa: return "AAAA";
  case sres_type_loc: return "LOC";
  case sres_type_nxt: return "NXT";
  case sres_type_eid: return "EID";
  case sres_type_nimloc: return "NIMLOC";
  case sres_type_srv: return "SRV";
  case sres_type_atma: return "ATMA";
  case sres_type_naptr: return "NAPTR";
  case sres_type_kx: return "KX";
  case sres_type_cert: return "CERT";
  case sres_type_a6: return "A6";
  case sres_type_dname: return "DNAME";
  case sres_type_sink: return "SINK";
  case sres_type_opt: return "OPT";

  case sres_qtype_tsig: return "TSIG";
  case sres_qtype_ixfr: return "IXFR";
  case sres_qtype_axfr: return "AXFR";
  case sres_qtype_mailb: return "MAILB";
  case sres_qtype_maila: return "MAILA";
  case sres_qtype_any: return "ANY";

  default:
    if (buffer)
      sprintf(buffer, "%u?", type & 65535);
    return buffer;
  }
}

/** Convert record status to its name */
char const *sres_record_status(int status, char buffer[8])
{
  switch (status) {
  case SRES_OK: return "OK";
  case SRES_FORMAT_ERR: return "FORMAT_ERR";
  case SRES_SERVER_ERR: return "SERVER_ERR";
  case SRES_NAME_ERR: return "NAME_ERR";
  case SRES_UNIMPL_ERR: return "UNIMPL_ERR";
  case SRES_AUTH_ERR: return "AUTH_ERR";

  /* Errors generated by sresolv */
  case SRES_TIMEOUT_ERR: return "TIMEOUT_ERR";
  case SRES_RECORD_ERR: return "RECORD_ERR";
  case SRES_INTERNAL_ERR: return "INTERNAL_ERR";
  case SRES_NETWORK_ERR: return "NETWORK_ERR";

  default:
    if (buffer)
      sprintf(buffer, "%u?", status & 255);
    return buffer;
  }
}


/** Convert class to its name. */
static char const *
sres_record_class(int rclass, char buffer[8])
{
  switch (rclass) {
  case 1: return "IN";
  case 2: return "2?";
  case 3: return "CHAOS";
  case 4: return "HS";
  case 254: return "NONE";
  case 255: return "ANY";

  default:
    sprintf(buffer, "%u?", rclass & 65535);
    return buffer;
  }
}

/** Compare two records. */
int
sres_record_compare(sres_record_t const *aa, sres_record_t const *bb)
{
  int D;
  sres_common_t const *a = aa->sr_record, *b = bb->sr_record;

  D = a->r_status - b->r_status; if (D) return D;
  D = a->r_class - b->r_class; if (D) return D;
  D = a->r_type - b->r_type; if (D) return D;

  if (a->r_status)
    return 0;

  switch (a->r_type) {
  case sres_type_soa:
    {
      sres_soa_record_t const *A = aa->sr_soa, *B = bb->sr_soa;
      D = A->soa_serial - B->soa_serial; if (D) return D;
      D = su_strcasecmp(A->soa_mname, B->soa_mname); if (D) return D;
      D = su_strcasecmp(A->soa_rname, B->soa_rname); if (D) return D;
      D = A->soa_refresh - B->soa_refresh; if (D) return D;
      D = A->soa_retry - B->soa_retry; if (D) return D;
      D = A->soa_expire - B->soa_expire; if (D) return D;
      D = A->soa_minimum - B->soa_minimum; if (D) return D;
      return 0;
    }
  case sres_type_a:
    {
      sres_a_record_t const *A = aa->sr_a, *B = bb->sr_a;
      return memcmp(&A->a_addr, &B->a_addr, sizeof A->a_addr);
    }
  case sres_type_a6:
    {
      sres_a6_record_t const *A = aa->sr_a6, *B = bb->sr_a6;
      D = A->a6_prelen - B->a6_prelen; if (D) return D;
      D = !A->a6_prename - !B->a6_prename;
      if (D == 0 && A->a6_prename && B->a6_prename)
	D = su_strcasecmp(A->a6_prename, B->a6_prename); if (D) return D;
      return memcmp(&A->a6_suffix, &B->a6_suffix, sizeof A->a6_suffix);
    }
  case sres_type_aaaa:
    {
      sres_aaaa_record_t const *A = aa->sr_aaaa, *B = bb->sr_aaaa;
      return memcmp(&A->aaaa_addr, &B->aaaa_addr, sizeof A->aaaa_addr);
    }
  case sres_type_cname:
    {
      sres_cname_record_t const *A = aa->sr_cname, *B = bb->sr_cname;
      return strcmp(A->cn_cname, B->cn_cname);
    }
  case sres_type_ptr:
    {
      sres_ptr_record_t const *A = aa->sr_ptr, *B = bb->sr_ptr;
      return strcmp(A->ptr_domain, B->ptr_domain);
    }
  case sres_type_srv:
    {
      sres_srv_record_t const *A = aa->sr_srv, *B = bb->sr_srv;
      D = A->srv_priority - B->srv_priority; if (D) return D;
      /* Record with larger weight first */
      D = B->srv_weight - A->srv_weight; if (D) return D;
      D = strcmp(A->srv_target, B->srv_target); if (D) return D;
      return A->srv_port - B->srv_port;
    }
  case sres_type_naptr:
    {
      sres_naptr_record_t const *A = aa->sr_naptr, *B = bb->sr_naptr;
      D = A->na_order - B->na_order; if (D) return D;
      D = A->na_prefer - B->na_prefer; if (D) return D;
      D = strcmp(A->na_flags, B->na_flags); if (D) return D;
      D = strcmp(A->na_services, B->na_services); if (D) return D;
      D = strcmp(A->na_regexp, B->na_regexp); if (D) return D;
      return strcmp(A->na_replace, B->na_replace);
    }
  default:
    return 0;
  }
}

/* ---------------------------------------------------------------------- */
/* Private functions */

/** Destruct */
static
void
sres_resolver_destructor(void *arg)
{
  sres_resolver_t *res = arg;

  assert(res);
  sres_cache_unref(res->res_cache);
  res->res_cache = NULL;

  sres_servers_close(res, res->res_servers);

  if (res->res_config)
    su_home_unref((su_home_t *)res->res_config->c_home);

  if (res->res_updcb)
    res->res_updcb(res->res_async, INVALID_SOCKET, INVALID_SOCKET);
}

HTABLE_BODIES_WITH(sres_qtable, qt, sres_query_t, SRES_QUERY_HASH,
		   unsigned, size_t);

/** Allocate a query structure */
static
sres_query_t *
sres_query_alloc(sres_resolver_t *res,
		 sres_answer_f *callback,
		 sres_context_t *context,
		 uint16_t type,
		 char const *domain)
{
  sres_query_t *query;
  size_t dlen = strlen(domain);

  if (sres_qtable_is_full(res->res_queries))
    if (sres_qtable_resize(res->res_home, res->res_queries, 0) < 0)
      return NULL;

  query = su_alloc(res->res_home, sizeof(*query) + dlen + 1);

  if (query) {
    memset(query, 0, sizeof *query);
    query->q_res = res;
    query->q_callback = callback;
    query->q_context = context;
    query->q_type = type;
    query->q_class = sres_class_in;
    query->q_timestamp = res->res_now;
    query->q_name = strcpy((char *)(query + 1), domain);

    sres_gen_id(res, query);
    assert(query->q_id);

    query->q_i_server = res->res_i_server;
    query->q_n_servers = res->res_n_servers;

    sres_qtable_append(res->res_queries, query);

    if (res->res_schedulecb && res->res_queries->qt_used == 1)
      res->res_schedulecb(res->res_async, 2 * SRES_RETRANSMIT_INTERVAL);
  }

  return query;
}

su_inline
void
sres_remove_query(sres_resolver_t *res, sres_query_t *q, int all)
{
  int i;

  if (q->q_hash) {
    sres_qtable_remove(res->res_queries, q), q->q_hash = 0;

    if (all)
      for (i = 0; i <= SRES_MAX_SEARCH; i++) {
	if (q->q_subqueries[i] && q->q_subqueries[i]->q_hash) {
	  sres_qtable_remove(res->res_queries, q->q_subqueries[i]);
	  q->q_subqueries[i]->q_hash = 0;
	}
      }
  }
}

/** Remove a query from hash table and free it. */
static
void sres_free_query(sres_resolver_t *res, sres_query_t *q)
{
  int i;

  if (q == NULL)
    return;

  if (q->q_hash)
    sres_qtable_remove(res->res_queries, q), q->q_hash = 0;

  for (i = 0; i <= SRES_MAX_SEARCH; i++) {
    sres_query_t *sq;

    sq = q->q_subqueries[i];
    q->q_subqueries[i] = NULL;
    if (sq)
      sres_free_query(res, sq);
    if (q->q_subanswers[i])
      sres_cache_free_answers(res->res_cache, q->q_subanswers[i]);
    q->q_subanswers[i] = NULL;
  }

  su_free(res->res_home, q);
}

static
sres_record_t **
sres_combine_results(sres_resolver_t *res,
		     sres_record_t **search_results[SRES_MAX_SEARCH + 1])
{
  sres_record_t **combined_result;
  int i, j, found;

  /* Combine the results into a single list. */
  for (i = 0, found = 0; i <= SRES_MAX_SEARCH; i++)
    if (search_results[i])
      for (j = 0; search_results[i][j]; j++)
	found++;

  combined_result = su_alloc((su_home_t *)res->res_cache,
			     (found + 1) * (sizeof combined_result[0]));
  if (combined_result) {
    for (i = 0, found = 0; i <= SRES_MAX_SEARCH; i++)
      if (search_results[i])
	for (j = 0; search_results[i][j]; j++) {
	  combined_result[found++] = search_results[i][j];
	  search_results[i][j] = NULL;
	}

    combined_result[found] = NULL;
    sres_sort_answers(res, combined_result);
  }

  for (i = 0; i <= SRES_MAX_SEARCH; i++)
    if (search_results[i])
      sres_free_answers(res, search_results[i]), search_results[i] = NULL;

  return combined_result;
}

static
int
sres_sockaddr2string(sres_resolver_t *res,
		     char name[],
		     size_t namelen,
		     struct sockaddr const *addr)
{
  name[0] = '\0';

  if (addr->sa_family == AF_INET) {
    struct sockaddr_in const *sin = (struct sockaddr_in *)addr;
    uint8_t const *in_addr = (uint8_t*)&sin->sin_addr;
    return snprintf(name, namelen, "%u.%u.%u.%u.in-addr.arpa.",
		    in_addr[3], in_addr[2], in_addr[1], in_addr[0]);
  }
#if HAVE_SIN6
  else if (addr->sa_family == AF_INET6) {
    struct sockaddr_in6 const *sin6 = (struct sockaddr_in6 *)addr;
    size_t addrsize = sizeof(sin6->sin6_addr.s6_addr);
    char *postfix;
    size_t required;
    size_t i;

    if (res->res_config->c_opt.ip6int)
      postfix = "ip6.int.";
    else
      postfix = "ip6.arpa.";

    required = addrsize * 4 + strlen(postfix);

    if (namelen <= required)
      return (int)required;

    for (i = 0; i < addrsize; i++) {
      uint8_t byte = sin6->sin6_addr.s6_addr[addrsize - i - 1];
      uint8_t hex;

      hex = byte & 0xf;
      name[4 * i] = hex > 9 ? hex + 'a' - 10 : hex + '0';
      name[4 * i + 1] = '.';
      hex = (byte >> 4) & 0xf;
      name[4 * i + 2] = hex > 9 ? hex + 'a' - 10 : hex + '0';
      name[4 * i + 3] = '.';
    }

    strcpy(name + 4 * i, postfix);

    return (int)required;
  }
#endif /* HAVE_SIN6 */
  else {
    su_seterrno(EAFNOSUPPORT);
    SU_DEBUG_3(("%s: %s\n", "sres_sockaddr2string",
                su_strerror(EAFNOSUPPORT)));
    return 0;
  }
}

/** Make a domain name a top level domain name.
 *
 * The function sres_toplevel() returns a copies string @a domain and
 * terminates it with a dot if it is not already terminated.
 */
static
char const *
sres_toplevel(char buf[], size_t blen, char const *domain)
{
  size_t len;
  int already;

  if (!domain)
    return su_seterrno(EFAULT), (void *)NULL;

  len = strlen(domain);

  if (len >= blen)
    return su_seterrno(ENAMETOOLONG), (void *)NULL;

  already = len > 0 && domain[len - 1] == '.';

  if (already)
    return domain;

  if (len + 1 >= blen)
    return su_seterrno(ENAMETOOLONG), (void *)NULL;

  strcpy(buf, domain);
  buf[len] = '.'; buf[len + 1] = '\0';

  return buf;
}

/* ---------------------------------------------------------------------- */

static int sres_update_config(sres_resolver_t *res, int always, time_t now);
static int sres_parse_config(sres_config_t *, FILE *);
static int sres_parse_options(sres_config_t *c, char const *value);
static int sres_parse_nameserver(sres_config_t *c, char const *server);
static time_t sres_config_timestamp(sres_config_t const *c);

/** Update configuration
 *
 * @retval 0 when successful
 * @retval -1 upon an error
 */
int sres_resolver_update(sres_resolver_t *res, int always)
{
  sres_server_t **servers, **old_servers;
  int updated;

  updated = sres_update_config(res, always, time(&res->res_now));
  if (updated < 0)
    return -1;

  if (!res->res_servers || always || updated) {
    servers = sres_servers_new(res, res->res_config);
    old_servers = res->res_servers;

    res->res_i_server = 0;
    res->res_n_servers = sres_servers_count(servers);
    res->res_servers = servers;

    sres_servers_close(res, old_servers);
    su_free(res->res_home, old_servers);

    if (!servers)
      return -1;
  }

  return 0;
}

/** Update config file.
 *
 * @retval 1 if DNS server list is different from old one.
 * @retval 0 when otherwise successful
 * @retval -1 upon an error
 */
static
int sres_update_config(sres_resolver_t *res, int always, time_t now)
{
  sres_config_t *c = NULL;
  sres_config_t const *previous;
  int retval;

  previous = res->res_config;

  if (!always && previous && now < res->res_checked)
    return 0;
  /* Try avoid checking for changes too often. */
  res->res_checked = now + SRES_UPDATE_INTERVAL_SECS;

  if (!always && previous &&
      sres_config_timestamp(previous) == previous->c_modified)
    return 0;

  c = sres_parse_resolv_conf(res, res->res_options);
  if (!c)
    return -1;

  res->res_config = c;

  retval = sres_config_changed_servers(c, previous);

  su_home_unref((su_home_t *)previous->c_home);

  return retval;
}

#if HAVE_WIN32

/** Number of octets to read from a registry key at a time */
#define QUERY_DATALEN         1024
#define MAX_DATALEN           65535

/**
 * Uses IP Helper IP to get DNS servers list.
 */
static int sres_parse_win32_ip(sres_config_t *c)
{
  int ret = -1;

#if HAVE_IPHLPAPI_H
  DWORD dw;
  su_home_t *home = c->c_home;
  ULONG size = sizeof(FIXED_INFO);

  do {
    FIXED_INFO *info = (FIXED_INFO *)su_alloc(home, size);
    dw = GetNetworkParams(info, &size);
    if (dw == ERROR_SUCCESS) {
      IP_ADDR_STRING* addr = &info->DnsServerList;
      for (; addr; addr = addr->Next) {
       SU_DEBUG_3(("Adding nameserver: %s\n", addr->IpAddress.String));
       sres_parse_nameserver(c, addr->IpAddress.String);
      }
      ret = 0;
    }
    su_free(home, info);
  } while (dw == ERROR_BUFFER_OVERFLOW);
#endif

  return ret;
}

/**
 * Parses name servers listed in registry key 'key+lpValueName'. The
 * key is expected to contain a whitespace separate list of
 * name server IP addresses.
 *
 * @return number of server addresses added
 */
static int sres_parse_win32_reg_parse_dnsserver(sres_config_t *c, HKEY key, LPCTSTR lpValueName)
{
  su_home_t *home = c->c_home;
  su_strlst_t *reg_dns_list;
  BYTE *name_servers = su_alloc(home, QUERY_DATALEN);
  DWORD name_servers_length = QUERY_DATALEN;
  int ret, servers_added = 0;

  /* get name servers and ... */
  while((ret = RegQueryValueEx(key,
			       lpValueName,
			       NULL, NULL,
			       name_servers,
			       &name_servers_length)) == ERROR_MORE_DATA) {
    name_servers_length += QUERY_DATALEN;

    /* sanity check, upper limit for memallocs */
    if (name_servers_length > MAX_DATALEN) break;

    name_servers = su_realloc(home, name_servers, name_servers_length);
    if (name_servers == NULL) {
      ret = ERROR_BUFFER_OVERFLOW;
      break;
    }
  }

  /* if reading the key was succesful, continue */
  if (ret == ERROR_SUCCESS) {
    if (name_servers[0]){
      int i;

      /* add to list */
      reg_dns_list = su_strlst_split(home, (char *)name_servers, " ");

      for(i = 0 ; i < su_strlst_len(reg_dns_list); i++) {
	const char *item = su_strlst_item(reg_dns_list, i);
	SU_DEBUG_3(("Adding nameserver: %s (key=%s)\n", item, (char*)lpValueName));
	sres_parse_nameserver(c, item);
	++servers_added;
      }

      su_strlst_destroy(reg_dns_list);

    }
  }

  su_free(home, name_servers);

  return servers_added;
}

/**
 * Discover system nameservers from Windows registry.
 *
 * Refs:
 *  - http://msdn.microsoft.com/library/default.asp?url=/library/en-us/sysinfo/base/regqueryvalueex.asp
 *  - http://support.microsoft.com/default.aspx?scid=kb;en-us;120642
 *  - http://support.microsoft.com/kb/314053/EN-US/
 *  - IP Helper API (possibly better way than current registry-based impl.)
 *    http://msdn.microsoft.com/library/default.asp?url=/library/en-us/iphlp/iphlp/ip_helper_start_page.asp
 */
static int sres_parse_win32_reg(sres_config_t *c)
{
  int ret = -1;

#define MAX_KEY_LEN           255
#define MAX_VALUE_NAME_LEN    16383

  su_home_t *home = c->c_home;
  HKEY key_handle;
#if 0
  HKEY interface_key_handle;
  FILETIME ftime;
  int index, i;
#endif
  int found = 0;
  char *interface_guid = su_alloc(home, MAX_VALUE_NAME_LEN);

#if 0
#if __MINGW32__
  DWORD guid_size = QUERY_DATALEN;
#else
  int guid_size = MAX_VALUE_NAME_LEN;
#endif

  /* step: find interface specific nameservers
   * - this is currently disabled 2006/Jun (the current check might insert
   *   multiple unnecessary nameservers to the search list)
   */
  /* open the 'Interfaces' registry Key */
  if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
		   "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces",
		   0, KEY_READ, &key_handle)) {
    SU_DEBUG_2(("RegOpenKeyEx failed\n"));
  } else {
    index = 0;
    /* for each interface listed ... */
    while (RegEnumKeyEx(key_handle, index,
			interface_guid, &guid_size,
			NULL,NULL,0,&ftime) == ERROR_SUCCESS){
      if (RegOpenKeyEx(key_handle, interface_guid,
		       0, KEY_READ,
		       &interface_key_handle) == ERROR_SUCCESS) {

	/* note: 'NameServer' is preferred over 'DhcpNameServer' */
	found += sres_parse_win32_reg_parse_dnsserver(c, interface_key_handle, "NameServer");
	if (found == 0)
	  found += sres_parse_win32_reg_parse_dnsserver(c, interface_key_handle, "DhcpNameServer");

	RegCloseKey(interface_key_handle);
      } else{
	SU_DEBUG_2(("interface RegOpenKeyEx failed\n"));
      }
      index++;
      guid_size = 64;
    }
    RegCloseKey(key_handle);
  }
#endif /* #if 0: interface-specific nameservers */

  /* step: if no interface-specific nameservers are found,
   *       check for system-wide nameservers */
  if (found == 0) {
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
		     "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters",
		     0, KEY_READ, &key_handle)) {
      SU_DEBUG_2(("RegOpenKeyEx failed (2)\n"));
    } else {
      found += sres_parse_win32_reg_parse_dnsserver(c, key_handle, "NameServer");
      if (found == 0)
	found += sres_parse_win32_reg_parse_dnsserver(c, key_handle, "DhcpNameServer");
      RegCloseKey(key_handle);
    }
  }

  SU_DEBUG_3(("Total of %d name servers found from win32 registry.\n", found));

  /* return success if servers found */
  if (found) ret = 0;

  su_free(home, interface_guid);

  return ret;
}

#endif /* HAVE_WIN32 */

/** Parse /etc/resolv.conf file.
 *
 * @retval #sres_config_t structure when successful
 * @retval NULL upon an error
 *
 * @todo The resolv.conf directives @b sortlist and most of the options
 *       are currently ignored.
 */
static
sres_config_t *sres_parse_resolv_conf(sres_resolver_t *res,
				      char const **options)
{
  sres_config_t *c = su_home_new(sizeof *c);

  if (c) {
    FILE *f;
    int i;

    f = fopen(c->c_filename = res->res_cnffile, "r");

    sres_parse_config(c, f);

    if (f)
      fclose(f);

#if HAVE_WIN32
    /* note: no 127.0.0.1 on win32 systems */
    /* on win32, query the registry for nameservers */
    if (sres_parse_win32_ip(c) == 0 || sres_parse_win32_reg(c) == 0)
      /* success */;
    else
      /* now what? */;
#else
    /* Use local nameserver by default */
    if (c->c_nameservers[0] == NULL)
      sres_parse_nameserver(c, "127.0.0.1");
#endif

    for (i = 0; c->c_nameservers[i] && i < SRES_MAX_NAMESERVERS; i++) {
      struct sockaddr_in *sin = (void *)c->c_nameservers[i]->ns_addr;
      sin->sin_port = htons(c->c_port);
    }

    sres_parse_options(c, getenv("RES_OPTIONS"));

    if (options)
      for (i = 0; options[i]; i++)
	sres_parse_options(c, options[i]);

    sres_parse_options(c, getenv("SRES_OPTIONS"));

    su_home_threadsafe(c->c_home);
  }

  return c;
}

uint16_t _sres_default_port = 53;

/** Parse config file.
 *
 * @return Number of search domains, if successful.
 * @retval -1 upon an error (never happens).
 */
static
int sres_parse_config(sres_config_t *c, FILE *f)
{
  su_home_t *home = c->c_home;
  int line;
  char const *localdomain;
  char *search = NULL, *domain = NULL;
  char buf[1025];
  int i = 0;

  localdomain = getenv("LOCALDOMAIN");

  /* Default values */
  c->c_opt.ndots = 1;
  c->c_opt.check_names = 1;
  c->c_opt.timeout = SRES_RETRY_INTERVAL;
  c->c_opt.attempts = SRES_MAX_RETRY_COUNT;
  c->c_port = _sres_default_port;

  if (f != NULL) {
    for (line = 1; fgets(buf, sizeof(buf), f); line++) {
      size_t len;
      char *value, *b;

      /* Skip whitespace at the beginning ...*/
      b = buf + strspn(buf, " \t");

      /* ... and comments + whitespace at the end */
      for (len = strcspn(b, "#;"); len > 0 && strchr(" \t\r\n", b[len - 1]); len--)
	;

      if (len == 0) 	/* Empty line or comment */
	continue;

      b[len] = '\0';

      len = strcspn(b, " \t");
      value = b + len; value += strspn(value, " \t");

#define MATCH(token) (len == strlen(token) && su_casenmatch(token, b, len))

      if (MATCH("nameserver")) {
	if (sres_parse_nameserver(c, value) < 0)
	  return -1;
      }
      else if (MATCH("domain")) {
	if (localdomain)	/* LOCALDOMAIN overrides */
	  continue;
	if (search)
	  su_free(home, search), search = NULL;
	if (domain)
	  su_free(home, domain), domain = NULL;
	domain = su_strdup(home, value);
	if (!domain)
	  return -1;
      }
      else if (MATCH("search")) {
	if (localdomain)	/* LOCALDOMAIN overrides */
	  continue;
	if (search) su_free(home, search), search = NULL;
	if (domain) su_free(home, domain), domain = NULL;
	search = su_strdup(home, value);
	if (!search)
	  return -1;
      }
      else if (MATCH("port")) {
	unsigned long port = strtoul(value, NULL, 10);
	if (port < 65536)
	  c->c_port = port;
      }
      else if (MATCH("options")) {
	sres_parse_options(c, value);
      }
    }
  }

  if (f)
    c->c_modified = sres_config_timestamp(c);

  if (localdomain)
    c->c_search[0] = localdomain;
  else if (domain)
    c->c_search[0] = domain;
  else if (search) {
    for (i = 0; search[0] && i < SRES_MAX_SEARCH; i++) {
      c->c_search[i] = search;
      search += strcspn(search, " \t");
      if (*search) {
	*search++ = '\0';
	search += strspn(search, " \t");
      }
    }
  }

  return i;
}

#if DOXYGEN_ONLY
/**@ingroup sresolv_env
 *
 * Environment variable containing options for Sofia resolver. The options
 * recognized by Sofia resolver are as follows:
 * - @b debug           turn on debugging (no effect)
 * - @b ndots:<i>n</i>  when searching, try first to query name as absolute
 *                      domain if it contains at least <i>n</i> dots
 * - @b timeout:<i>secs</i> timeout in seconds
 * - @b attempts:<i>n</i> fail after <i>n</i> retries
 * - @b rotate          use round robin selection of nameservers
 * - @b no-check-names  do not check names for invalid characters
 * - @b inet6           (no effect)
 * - @b ip6-dotint      IPv6 addresses are resolved using suffix ".ip6.int"
 *                      instead of the standard ".ip6.arpa" suffix
 * - @b ip6-bytestring  (no effect)
 * The following option is a Sofia-specific extension:
 * - @b no-edns0        do not try to use EDNS0 extension (@RFC2671)
 *
 * The same options can be listed in @b options directive in resolv.conf, or
 * in #RES_OPTIONS environment variable. Note that options given in
 * #SRES_OPTIONS override those specified in #RES_OPTIONS which in turn
 * override options specified in the @b options directive of resolve.conf.
 *
 * The meaning of an option can be reversed with prefix "no-".
 *
 * @sa Manual page for resolv.conf, #RES_OPTIONS.
 */
extern SRES_OPTIONS;

/**@ingroup sresolv_env
 *
 * Environment variable containing resolver options. This environment
 * variable is also used by standard BIND resolver.
 *
 * @sa Manual page for resolv.conf, #SRES_OPTIONS.
 */
extern RES_OPTIONS;

/**@ingroup sresolv_env
 *
 * Environment variable containing search domain. This environment
 * variable is also used by standard BIND resolver.
 *
 * @sa Manual page for resolv.conf, #RES_OPTIONS, #SRES_OPTIONS.
 */
extern LOCALDOMAIN;
#endif

/* Parse options line or #SRES_OPTIONS or #RES_OPTIONS environment variable. */
static int
sres_parse_options(sres_config_t *c, char const *value)
{
  if (!value)
    return -1;

  while (value[0]) {
    char const *b;
    size_t len, extra = 0;
    unsigned long n = 0;

    b = value; len = strcspn(value, " \t:");
    value += len;

    if (value[0] == ':') {
      len++;
      n = strtoul(++value, NULL, 10);
      value += extra = strcspn(value, " \t");
    }

    if (*value)
      value += strspn(value, " \t");

    if (n > 65536) {
      SU_DEBUG_3(("sres: %s: invalid %*.0s\n", c->c_filename,
		  (int)(len + extra), b));
      continue;
    }

    /* Documented by BIND9 resolv.conf */
    if (MATCH("no-debug")) c->c_opt.debug = 0;
    else if (MATCH("debug")) c->c_opt.debug = 1;
    else if (MATCH("ndots:")) c->c_opt.ndots = n;
    else if (MATCH("timeout:")) c->c_opt.timeout = n;
    else if (MATCH("attempts:")) c->c_opt.attempts = n;
    else if (MATCH("no-rotate")) c->c_opt.rotate = 0;
    else if (MATCH("rotate")) c->c_opt.rotate = 1;
    else if (MATCH("no-check-names")) c->c_opt.check_names = 0;
    else if (MATCH("check-names")) c->c_opt.check_names = 1;
    else if (MATCH("no-inet6")) c->c_opt.ip6int = 0;
    else if (MATCH("inet6")) c->c_opt.inet6 = 1;
    else if (MATCH("no-ip6-dotint")) c->c_opt.ip6int = 0;
    else if (MATCH("ip6-dotint")) c->c_opt.ip6int = 1;
    else if (MATCH("no-ip6-bytestring")) c->c_opt.ip6bytestring = 0;
    else if (MATCH("ip6-bytestring")) c->c_opt.ip6bytestring = 1;
    /* Sofia-specific extensions: */
    else if (MATCH("no-edns0")) c->c_opt.edns = edns_not_supported;
    else if (MATCH("edns0")) c->c_opt.edns = edns0_configured;
    else {
      SU_DEBUG_3(("sres: %s: unknown option %*.0s\n",
		  c->c_filename, (int)(len + extra), b));
    }
  }

  return 0;
}

static
int sres_parse_nameserver(sres_config_t *c, char const *server)
{
  sres_nameserver_t *ns;
  struct sockaddr *sa;
  int err, i;

  for (i = 0; i < SRES_MAX_NAMESERVERS; i++)
    if (c->c_nameservers[i] == NULL)
      break;

  if (i >= SRES_MAX_NAMESERVERS)
    return 0 /* Silently discard extra nameservers */;

  ns = su_zalloc(c->c_home, (sizeof *ns) + strlen(server) + 1);
  if (!ns)
    return -1;

  sa = (void *)ns->ns_addr;

#if HAVE_SIN6
  if (strchr(server, ':')) {
    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;
    memset(sa, 0, ns->ns_addrlen = sizeof *sin6);
    err = su_inet_pton(sa->sa_family = AF_INET6, server, &sin6->sin6_addr);
  }
  else
#endif
    {
      struct sockaddr_in *sin = (struct sockaddr_in *)sa;
      memset(sa, 0, ns->ns_addrlen = sizeof *sin);
      err = su_inet_pton(sa->sa_family = AF_INET, server, &sin->sin_addr);
    }

  if (err <= 0) {
    SU_DEBUG_3(("sres: nameserver %s: invalid address\n", server));
    su_free(c->c_home, ns);
    return 0;
  }

#if HAVE_SA_LEN
  sa->sa_len = ns->ns_addrlen;
#endif

  c->c_nameservers[i] = ns;

  return 1;
}

/** Get current timestamp of resolv.conf file */
static
time_t sres_config_timestamp(sres_config_t const *c)
{
#ifndef HAVE_WIN32
  struct stat st;

  if (stat(c->c_filename, &st) == 0)
    return st.st_mtime;

  /** @return If the resolv.conf file does not exists, return old timestamp. */
  return c->c_modified;
#else
  /** On WIN32, return always different timestamp */
  return c->c_modified + SRES_UPDATE_INTERVAL_SECS;
#endif
}


/* ---------------------------------------------------------------------- */

/** Check if the new configuration has different servers than the old */
static
int sres_config_changed_servers(sres_config_t const *new_c,
				sres_config_t const *old_c)
{
  int i;
  sres_nameserver_t const *new_ns, *old_ns;

  if (old_c == NULL)
    return 1;

  for (i = 0; i < SRES_MAX_NAMESERVERS; i++) {
    new_ns = new_c->c_nameservers[i];
    old_ns = old_c->c_nameservers[i];

    if (!new_ns != !old_ns)
      return 1;
    if (!new_ns)
      return 0;
    if (new_ns->ns_addrlen != old_ns->ns_addrlen)
      return 1;
    if (memcmp(new_ns->ns_addr, old_ns->ns_addr, new_ns->ns_addrlen))
      return 1;
  }

  return 0;
}

/** Allocate new servers structure */
static
sres_server_t **sres_servers_new(sres_resolver_t *res,
				 sres_config_t const *c)
{
  sres_server_t **servers, *dns;
  sres_nameserver_t *ns;
  int N, i;
  size_t size;

  for (N = 0; c->c_nameservers[N] && N < SRES_MAX_NAMESERVERS; N++)
    ;

  size = (N + 1) * (sizeof *servers) + N * (sizeof **servers);

  servers = su_zalloc(res->res_home, size); if (!servers) return servers;
  dns = (void *)(servers + N + 1);
  for (i = 0; i < N; i++) {
    dns->dns_socket = INVALID_SOCKET;
    ns = c->c_nameservers[i];
    memcpy(dns->dns_addr, ns->ns_addr, dns->dns_addrlen = ns->ns_addrlen);
    su_inet_ntop(dns->dns_addr->ss_family, SS_ADDR(dns->dns_addr),
	      dns->dns_name, sizeof dns->dns_name);
    dns->dns_edns = c->c_opt.edns;
    servers[i] = dns++;
  }

  return servers;
}

static
void sres_servers_close(sres_resolver_t *res,
			sres_server_t **servers)
{
  int i;

  if (res == NULL || servers == NULL)
    return;

  for (i = 0; i < SRES_MAX_NAMESERVERS; i++) {
    if (!servers[i])
      break;

    if (servers[i]->dns_socket != INVALID_SOCKET) {
      if (res->res_updcb)
	res->res_updcb(res->res_async, INVALID_SOCKET, servers[i]->dns_socket);
      sres_close(servers[i]->dns_socket);
    }
  }
}

static
int sres_servers_count(sres_server_t *const *servers)
{
  int i;

  if (!servers)
    return 0;

  for (i = 0; i < SRES_MAX_NAMESERVERS; i++) {
    if (!servers[i])
      break;
  }

  return i;
}

static
sres_socket_t sres_server_socket(sres_resolver_t *res, sres_server_t *dns)
{
  int family = dns->dns_addr->ss_family;
  sres_socket_t s;

  if (dns->dns_socket != INVALID_SOCKET)
    return dns->dns_socket;

  s = socket(family, SOCK_DGRAM, IPPROTO_UDP);
  if (s == -1) {
    SU_DEBUG_1(("%s: %s: %s\n", "sres_server_socket", "socket",
		su_strerror(su_errno())));
    return s;
  }

#if HAVE_IP_RECVERR
  if (family == AF_INET || family == AF_INET6) {
    int const one = 1;
    if (setsockopt(s, SOL_IP, IP_RECVERR, &one, sizeof(one)) < 0) {
      if (family == AF_INET)
	SU_DEBUG_3(("setsockopt(IPVRECVERR): %s\n", su_strerror(su_errno())));
    }
  }
#endif
#if HAVE_IPV6_RECVERR
  if (family == AF_INET6) {
    int const one = 1;
    if (setsockopt(s, SOL_IPV6, IPV6_RECVERR, &one, sizeof(one)) < 0)
      SU_DEBUG_3(("setsockopt(IPV6_RECVERR): %s\n", su_strerror(su_errno())));
  }
#endif

  if (connect(s, (void *)dns->dns_addr, dns->dns_addrlen) < 0) {
    char ipaddr[64];
    char const *lb = "", *rb = "";

    if (family == AF_INET) {
      void *addr = &((struct sockaddr_in *)dns->dns_addr)->sin_addr;
      su_inet_ntop(family, addr, ipaddr, sizeof ipaddr);
    }
#if HAVE_SIN6
    else if (family == AF_INET6) {
      void *addr = &((struct sockaddr_in6 *)dns->dns_addr)->sin6_addr;
      su_inet_ntop(family, addr, ipaddr, sizeof ipaddr);
      lb = "[", rb = "]";
    }
#endif
    else
      snprintf(ipaddr, sizeof ipaddr, "<af=%u>", family);

    SU_DEBUG_1(("%s: %s: %s: %s%s%s:%u\n", "sres_server_socket", "connect",
		su_strerror(su_errno()), lb, ipaddr, rb,
		ntohs(((struct sockaddr_in *)dns->dns_addr)->sin_port)));
    sres_close(s);
    return INVALID_SOCKET;
  }

  if (res->res_updcb) {
    if (res->res_updcb(res->res_async, s, INVALID_SOCKET) < 0) {
      SU_DEBUG_1(("%s: %s: %s\n", "sres_server_socket", "update callback",
		  su_strerror(su_errno())));
      sres_close(s);
      return INVALID_SOCKET;
    }
  }

  dns->dns_socket = s;

  return s;
}

/* ---------------------------------------------------------------------- */

/** Send a query packet */
static
int
sres_send_dns_query(sres_resolver_t *res,
		    sres_query_t *q)
{
  sres_message_t m[1];
  uint8_t i, i0, N = res->res_n_servers;
  sres_socket_t s;
  int error = 0;
  ssize_t size, no_edns_size, edns_size;
  uint16_t id = q->q_id;
  uint16_t type = q->q_type;
  char const *domain = q->q_name;
  time_t now = res->res_now;
  sres_server_t **servers = res->res_servers, *dns;
  char b[8];

  if (now == 0) time(&now);

  SU_DEBUG_9(("sres_send_dns_query(%p, %p) called\n", (void *)res, (void *)q));

  if (domain == NULL)
    return -1;
  if (servers == NULL)
    return -1;
  if (N == 0)
    return -1;

  memset(m, 0, offsetof(sres_message_t, m_data[sizeof m->m_packet.mp_header]));

  /* Create a DNS message */
  size = sizeof(m->m_packet.mp_header);
  m->m_size = (uint16_t)sizeof(m->m_data);
  m->m_offset = (uint16_t)size;

  m->m_id = id;
  m->m_flags = htons(SRES_HDR_QUERY | SRES_HDR_RD);

  /* Query record */
  m->m_qdcount = htons(1);
  m_put_domain(m, domain, 0, NULL);
  m_put_uint16(m, type);
  m_put_uint16(m, sres_class_in);

  no_edns_size = m->m_offset;

  /* EDNS0 record (optional) */
  m_put_domain(m, ".", 0, NULL);
  m_put_uint16(m, sres_type_opt);
  m_put_uint16(m, sizeof(m->m_packet)); /* Class: our UDP payload size */
  m_put_uint32(m, 0);		/* TTL: extended RCODE & flags */
  m_put_uint16(m, 0);

  edns_size = m->m_offset;

  if (m->m_error) {
    SU_DEBUG_3(("%s(): encoding: %s\n", "sres_send_dns_query", m->m_error));
    su_seterrno(EIO);
    return -1;
  }

  i0 = q->q_i_server;
  if (i0 > N) i0 = 0; /* Number of DNS servers reduced */
  dns = servers[i = i0];

  error = EIO;

  if (res->res_config->c_opt.rotate || dns->dns_error || dns->dns_icmp)
    dns = sres_next_server(res, &q->q_i_server, 1), i = q->q_i_server;

  for (; dns; dns = sres_next_server(res, &i, 1)) {
    /* If server supports EDNS, include EDNS0 record */
    q->q_edns = dns->dns_edns;
    /* 0 (no EDNS) or 1 (EDNS supported) additional data records */
    m->m_arcount = htons(q->q_edns != 0);
    /* Size with or without EDNS record */
    size = q->q_edns ? edns_size : no_edns_size;

    s = sres_server_socket(res, dns);

    if (s == INVALID_SOCKET) {
      dns->dns_icmp = now;
      dns->dns_error = SRES_TIME_MAX;
      continue;
    }

    /* Send the DNS message via the UDP socket */
    if (sres_send(s, m->m_data, size, 0) == size)
      break;
    error = su_errno();

    dns->dns_icmp = now;
    dns->dns_error = now;	/* Mark as a bad destination */
  }

  if (!dns) {
    /* All servers have reported errors */
    SU_DEBUG_5(("%s(): sendto: %s\n", "sres_send_dns_query",
		su_strerror(error)));
    return su_seterrno(error);
  }

  q->q_i_server = i;

  SU_DEBUG_5(("%s(%p, %p) id=%u %s %s (to [%s]:%u)\n",
	      "sres_send_dns_query",
	      (void *)res, (void *)q, id, sres_record_type(type, b), domain,
	      dns->dns_name,
	      htons(((struct sockaddr_in *)dns->dns_addr)->sin_port)));

  return 0;
}

/** Retry time after ICMP error */
#define DNS_ICMP_TIMEOUT 60

/** Retry time after immediate error */
#define DNS_ERROR_TIMEOUT 10

/** Select next server.
 *
 * @param res resolver object
 * @param[in,out] in_out_i index to DNS server table
 * @param always return always a server
 */
static
sres_server_t *sres_next_server(sres_resolver_t *res,
				uint8_t *in_out_i,
				int always)
{
  int i, j, N;
  sres_server_t *dns, **servers;
  time_t now = res->res_now;

  N = res->res_n_servers;
  servers = res->res_servers;
  i = *in_out_i;

  assert(res->res_servers && res->res_servers[i]);

  for (j=0; j < N; j++) {
    dns = servers[j]; if (!dns) continue;
    if (dns->dns_icmp + DNS_ICMP_TIMEOUT < now)
      dns->dns_icmp = 0;
    if (dns->dns_error + DNS_ERROR_TIMEOUT < now &&
	dns->dns_error != SRES_TIME_MAX)
      dns->dns_error = 0;
  }

  /* Retry using another server? */
  for (j = (i + 1) % N; (j != i); j = (j + 1) % N) {
    dns = servers[j]; if (!dns) continue;
    if (dns->dns_icmp == 0) {
      return *in_out_i = j, dns;
    }
  }

  for (j = (i + 1) % N; (j != i); j = (j + 1) % N) {
    dns = servers[j]; if (!dns) continue;
    if (dns->dns_error == 0) {
      return *in_out_i = j, dns;
    }
  }

  if (!always)
    return NULL;

  dns = servers[i];
  if (dns && dns->dns_error < now && dns->dns_error != SRES_TIME_MAX)
    return dns;

  for (j = (i + 1) % N; j != i; j = (j + 1) % N) {
    dns = servers[j]; if (!dns) continue;
    if (dns->dns_error < now && dns->dns_error != SRES_TIME_MAX)
      return *in_out_i = j, dns;
  }

  return NULL;
}

/**
 * Callback function for subqueries
 */
static
void sres_answer_subquery(sres_context_t *context,
			  sres_query_t *query,
			  sres_record_t **answers)
{
  sres_resolver_t *res;
  sres_query_t *top = (sres_query_t *)context;
  int i;
  assert(top); assert(top->q_n_subs > 0); assert(query);

  res = query->q_res;

  for (i = 0; i <= SRES_MAX_SEARCH; i++) {
    if (top->q_subqueries[i] == query)
      break;
  }
  assert(i <= SRES_MAX_SEARCH);
  if (i > SRES_MAX_SEARCH || top->q_n_subs == 0) {
    sres_free_answers(res, answers);
    return;
  }

  if (answers) {
    int j, k;
    for (j = 0, k = 0; answers[j]; j++) {
      if (answers[j]->sr_status)
	sres_free_answer(query->q_res, answers[j]);
      else
	answers[k++] = answers[j];
    }
    answers[k] = NULL;
    if (!answers[0])
      sres_free_answers(query->q_res, answers), answers = NULL;
  }

  top->q_subqueries[i] = NULL;
  top->q_subanswers[i] = answers;
  top->q_n_subs--;

  if (answers && top->q_callback) {
    sres_answer_f *callback = top->q_callback;

    top->q_callback = NULL;
    sres_remove_query(top->q_res, top, 1);
    callback(top->q_context, top, answers);
  }
  else if (top->q_n_subs == 0 && top->q_id == 0) {
    sres_query_report_error(top, NULL);
  };
}

/** Report sres error */
static void
sres_query_report_error(sres_query_t *q,
			sres_record_t **answers)
{
  int i;

  if (q->q_callback) {
    char sbuf[8], tbuf[8];
    int status = 0;

    for (i = 0; i <= SRES_MAX_SEARCH; i++) {
      if (q->q_subqueries[i])	/* a pending query... */
	return;

      if (q->q_subanswers[i]) {
	answers = q->q_subanswers[i];
	q->q_subanswers[i] = NULL;
	break;
      }
    }

    if (answers == NULL) {
      sres_cache_t *cache = q->q_res->res_cache;

      status = q->q_retry_count ? SRES_TIMEOUT_ERR : SRES_NETWORK_ERR;

      answers = su_zalloc(CHOME(cache), 2 * sizeof *answers);
      if (answers)
	answers[0] = sres_create_error_rr(cache, q, status);
    }
    else {
      for (i = 0; answers[i]; i++) {
	status = answers[i]->sr_record->r_status;
	if (status)
	  break;
      }
    }

    SU_DEBUG_5(("sres(q=%p): reporting error %s for %s %s\n",
		(void *)q,
		sres_record_status(status, sbuf),
		sres_record_type(q->q_type, tbuf), q->q_name));

    sres_remove_query(q->q_res, q, 1);
    (q->q_callback)(q->q_context, q, answers);
  }

  sres_free_query(q->q_res, q);
}

/** Resolver timer function.
 *
 * The function sresolver_timer() should be called in regular intervals. We
 * recommend calling it in 500 ms intervals.
 *
 * @param res pointer to resolver object
 * @param dummy argument for compatibility
 */
void sres_resolver_timer(sres_resolver_t *res, int dummy)
{
  size_t i;
  sres_query_t *q;
  time_t now, retry_time;

  if (res == NULL)
    return;

  now = time(&res->res_now);

  if (res->res_queries->qt_used) {
    SU_DEBUG_9(("sres_resolver_timer() called at %lu\n", (long) now));

    /** Every time it is called it goes through all query structures, and
     * retransmits all the query messages, which have not been answered yet.
     */
    for (i = 0; i < res->res_queries->qt_size; i++) {
      q = res->res_queries->qt_table[i];

      if (!q)
	continue;

      /* Exponential backoff */
      retry_time = q->q_timestamp + ((time_t)1 << q->q_retry_count);

      if (now < retry_time)
	continue;

      sres_resend_dns_query(res, q, 1);

      if (q != res->res_queries->qt_table[i])
	i--;
    }

    if (res->res_schedulecb && res->res_queries->qt_used)
      res->res_schedulecb(res->res_async, SRES_RETRANSMIT_INTERVAL);
  }

  sres_cache_clean(res->res_cache, res->res_now);
}

/** Resend DNS query, report error if cannot resend any more.
 *
 * @param res  resolver object
 * @param q    query object
 * @param timeout  true if resent because of timeout
 *                (false if because icmp error report)
 */
static void
sres_resend_dns_query(sres_resolver_t *res, sres_query_t *q, int timeout)
{
  uint8_t i, N;
  sres_server_t *dns;

  SU_DEBUG_9(("sres_resend_dns_query(%p, %p, %s) called\n",
	      (void *)res, (void *)q, timeout ? "timeout" : "error"));

  N = res->res_n_servers;

  if (N > 0 && q->q_retry_count < SRES_MAX_RETRY_COUNT) {
    i = q->q_i_server;
    dns = sres_next_server(res, &i, timeout);

    if (dns) {
      res->res_i_server = q->q_i_server = i;

      if (q->q_retry_count > res->res_n_servers + 1 &&
	  dns->dns_edns == edns_not_tried)
	q->q_edns = edns_not_supported;

      sres_send_dns_query(res, q);

      if (timeout)
	q->q_retry_count++;

      return;
    }
  }

  /* report timeout/network error */
  q->q_id = 0;

  if (q->q_n_subs)
    return;			/* let subqueries also timeout */

  sres_query_report_error(q, NULL);
}

static void
sres_resolve_cname(sres_resolver_t *res,
                   sres_query_t *orig_query,
                   char const *cname)
{
  sres_query_t *query;

  query = sres_query_alloc(res,
			   sres_resolving_cname,
			   (sres_context_t *)orig_query,
			   orig_query->q_type,
			   cname);

  if (query)
    sres_send_dns_query(res, query);
  else
    sres_query_report_error(orig_query, NULL);
}

static void
sres_resolving_cname(sres_context_t *original_query,
		    sres_query_t *query,
		    sres_record_t **answers)
{
  sres_query_t *orig = (sres_query_t *)original_query;

  /* Notify the listener */
  if (orig->q_callback != NULL)
    (orig->q_callback)(orig->q_context, orig, answers);

  sres_free_query(orig->q_res, orig);
}

/** Get a server by socket */
static
sres_server_t *
sres_server_by_socket(sres_resolver_t const *res, sres_socket_t socket)
{
  int i;

  if (socket == -1)
    return NULL;

  for (i = 0; i < res->res_n_servers; i++) {
    if (socket == res->res_servers[i]->dns_socket)
      return res->res_servers[i];
  }

  return NULL;
}

static
void
sres_canonize_sockaddr(struct sockaddr_storage *from, socklen_t *fromlen)
{
#if HAVE_SIN6
  struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)from;

  size_t sin6_addrsize =
    offsetof(struct sockaddr_in6, sin6_addr) +
    (sizeof sin6->sin6_addr);

  if (from->ss_family == AF_INET6) {
    struct in6_addr const *ip6 = &sin6->sin6_addr;

    if (IN6_IS_ADDR_V4MAPPED(ip6) || IN6_IS_ADDR_V4COMPAT(ip6)) {
      /* Convert to a IPv4 address */
      struct sockaddr_in *sin = (struct sockaddr_in *)from;
      memcpy(&sin->sin_addr, ip6->s6_addr + 12, sizeof sin->sin_addr);
      sin->sin_family = AF_INET;
      *fromlen = sizeof (*sin);
#if HAVE_SA_LEN
      sin->sin_len = sizeof (*sin);
#endif
    }
    else if (sin6_addrsize < *fromlen) {
      /* Zero extra sin6 members like sin6_flowinfo or sin6_scope_id */
      memset((char *)from + sin6_addrsize, 0, *fromlen - sin6_addrsize);
    }
  }
#endif

  if (from->ss_family == AF_INET) {
    struct sockaddr_in *sin = (struct sockaddr_in *)from;
    memset(sin->sin_zero, 0, sizeof (sin->sin_zero));
  }
}

static
int sres_no_update(sres_async_t *async,
		   sres_socket_t new_socket,
		   sres_socket_t old_socket)
{
  return 0;
}

/** Create connected sockets for resolver.
 */
int sres_resolver_sockets(sres_resolver_t *res,
			  sres_socket_t *return_sockets,
			  int n)
{
  sres_socket_t s = INVALID_SOCKET;
  int i, retval;

  if (!sres_resolver_set_async(res, sres_no_update,
			       (sres_async_t *)-1, 1))
    return -1;

  retval = res->res_n_servers; assert(retval <= SRES_MAX_NAMESERVERS);

  if (!return_sockets || n == 0)
    return retval;

  for (i = 0; i < retval && i < n;) {
    sres_server_t *dns = res->res_servers[i];

    s = sres_server_socket(res, dns);

    if (s == INVALID_SOCKET) {	/* Mark as a bad destination */
      dns->dns_icmp = SRES_TIME_MAX;
      dns->dns_error = SRES_TIME_MAX;
    }

    return_sockets[i++] = s;
  }

  return retval;
}

#if 0
/** Get a server by socket address */
static
sres_server_t *
sres_server_by_sockaddr(sres_resolver_t const *res,
			void const *from, socklen_t fromlen)
{
  int i;

  for (i = 0; i < res->res_n_servers; i++) {
    sres_server_t *dns = res->res_servers[i];
    if (dns->dns_addrlen == fromlen &&
	memcmp(dns->dns_addr, from, fromlen) == 0)
      return dns;
  }

  return NULL;
}
#endif

/** Receive error message from socket. */
#if HAVE_IP_RECVERR || HAVE_IPV6_RECVERR
int sres_resolver_error(sres_resolver_t *res, int socket)
{
  int errcode = 0;
  struct cmsghdr *c;
  struct sock_extended_err *ee;
  struct sockaddr_storage *from;
  char control[512];
  char errmsg[64 + 768];
  struct iovec iov[1];
  struct msghdr msg[1] = {{ 0 }};
  struct sockaddr_storage name[1] = {{ 0 }};
  int n;
  char info[128] = "";

  SU_DEBUG_9(("%s(%p, %u) called\n", "sres_resolver_error",
	      (void *)res, socket));

  msg->msg_name = name, msg->msg_namelen = sizeof(name);
  msg->msg_iov = iov, msg->msg_iovlen = 1;
  iov->iov_base = errmsg, iov->iov_len = sizeof(errmsg);
  msg->msg_control = control, msg->msg_controllen = sizeof(control);

  n = recvmsg(socket, msg, MSG_ERRQUEUE);

  if (n < 0) {
    int error = su_errno();
    if (error != EAGAIN && error != EWOULDBLOCK)
      SU_DEBUG_1(("%s: recvmsg: %s\n", __func__, su_strerror(error)));
    return n;
  }

  if ((msg->msg_flags & MSG_ERRQUEUE) != MSG_ERRQUEUE) {
    SU_DEBUG_1(("%s: recvmsg: no errqueue\n", __func__));
    return su_seterrno(EIO);
  }

  if (msg->msg_flags & MSG_CTRUNC) {
    SU_DEBUG_1(("%s: extended error was truncated\n", __func__));
    return su_seterrno(EIO);
  }

  if (msg->msg_flags & MSG_TRUNC) {
    /* ICMP message may contain original message... */
    SU_DEBUG_5(("%s: icmp(6) message was truncated (at %d)\n", __func__, n));
  }

  /* Go through the ancillary data */
  for (c = CMSG_FIRSTHDR(msg); c; c = CMSG_NXTHDR(msg, c)) {
    if (0
#if HAVE_IP_RECVERR
	|| (c->cmsg_level == SOL_IP && c->cmsg_type == IP_RECVERR)
#endif
#if HAVE_IPV6_RECVERR
	|| (c->cmsg_level == SOL_IPV6 && c->cmsg_type == IPV6_RECVERR)
#endif
	) {
      char const *origin;

      ee = (struct sock_extended_err *)CMSG_DATA(c);
      from = (void *)SO_EE_OFFENDER(ee);
      info[0] = '\0';

      switch (ee->ee_origin) {
      case SO_EE_ORIGIN_LOCAL:
	strcpy(info, origin = "local");
	break;
      case SO_EE_ORIGIN_ICMP:
	snprintf(info, sizeof(info), "%s type=%u code=%u",
		 origin = "icmp", ee->ee_type, ee->ee_code);
	break;
      case SO_EE_ORIGIN_ICMP6:
	snprintf(info, sizeof(info), "%s type=%u code=%u",
		 origin = "icmp6", ee->ee_type, ee->ee_code);
	break;
      case SO_EE_ORIGIN_NONE:
	strcpy(info, origin = "none");
	break;
      default:
	strcpy(info, origin = "unknown");
	break;
      }

      if (ee->ee_info)
	snprintf(info + strlen(info), sizeof(info) - strlen(info),
		 " info=%08x", ee->ee_info);
      errcode = ee->ee_errno;

      if (from->ss_family != AF_UNSPEC) {
	socklen_t fromlen = ((char *)c + c->cmsg_len) - (char *)from;

	sres_canonize_sockaddr(from, &fromlen);

	snprintf(info + strlen(info), sizeof(info) - strlen(info),
		 " reported by ");
	su_inet_ntop(from->ss_family, SS_ADDR(from),
		  info + strlen(info), sizeof(info) - strlen(info));
      }

      if (msg->msg_namelen <= 0)
	break;

      {
	int error;
	socklen_t errorlen = sizeof error;
	/* Get error, if any */
	getsockopt(socket, SOL_SOCKET, SO_ERROR, (void *)&error, &errorlen);
      }

      if (sres_resolver_report_error(res, socket, errcode,
				     msg->msg_name, msg->msg_namelen,
				     info))
	return errcode;
      break;
    }
  }

  if (errcode)
    sres_resolver_report_error(res, socket, errcode, NULL, 0, info);

  return errcode;
}

#else
int sres_resolver_error(sres_resolver_t *res, int socket)
{
  int errcode = 0;
  socklen_t errorlen = sizeof(errcode);

  SU_DEBUG_9(("%s(%p, %u) called\n", "sres_resolver_error",
	      (void *)res, socket));

  getsockopt(socket, SOL_SOCKET, SO_ERROR, (void *)&errcode, &errorlen);

  return sres_resolver_report_error(res, socket, errcode, NULL, 0, "");
}
#endif


/** Report error */
static
int
sres_resolver_report_error(sres_resolver_t *res,
			   sres_socket_t socket,
			   int errcode,
			   struct sockaddr_storage *remote,
			   socklen_t remotelen,
			   char const *info)
{
  char buf[80];

  buf[0] = '\0';

  if (remote) {
    sres_canonize_sockaddr(remote, &remotelen);

    if (remote->ss_family == AF_INET) {
      struct sockaddr_in const *sin = (struct sockaddr_in *)remote;
      uint8_t const *in_addr = (uint8_t*)&sin->sin_addr;
      su_inet_ntop(AF_INET, in_addr, buf, sizeof(buf));
    }
#if HAVE_SIN6
    else if (remote->ss_family == AF_INET6) {
      struct sockaddr_in6 const *sin6 = (struct sockaddr_in6 *)remote;
      uint8_t const *in_addr = (uint8_t*)&sin6->sin6_addr;
      su_inet_ntop(AF_INET6, in_addr, buf, sizeof(buf));
    }
#endif
  }

  SU_DEBUG_5(("sres: network error %u (%s)%s%s%s%s\n",
	      errcode, su_strerror(errcode),
	      buf[0] ? " from " : "", buf,
	      info ? " by " : "",
	      info ? info : ""));

  if (res->res_queries->qt_used) {
    /* Report error to queries */
    sres_server_t *dns;
    sres_query_t *q;
    size_t i;

    dns = sres_server_by_socket(res, socket);

    if (dns) {
      time(&res->res_now);
      dns->dns_icmp = res->res_now;

      for (i = 0; i < res->res_queries->qt_size; i++) {
	q = res->res_queries->qt_table[i];

	if (!q || dns != res->res_servers[q->q_i_server])
	  continue;

	/* Resend query/report error to application */
	sres_resend_dns_query(res, q, 0);

	if (q != res->res_queries->qt_table[i])
	  i--;
      }
    }
  }

  return 1;
}


/** Receive a response packet from socket. */
int
sres_resolver_receive(sres_resolver_t *res, int socket)
{
  ssize_t num_bytes;
  int error;
  sres_message_t m[1];

  sres_query_t *query = NULL;
  sres_record_t **reply;
  sres_server_t *dns;

  struct sockaddr_storage from[1];
  socklen_t fromlen = sizeof from;

  SU_DEBUG_9(("%s(%p, %u) called\n", "sres_resolver_receive",
	      (void *)res, socket));

  memset(m, 0, offsetof(sres_message_t, m_data));

  num_bytes = sres_recvfrom(socket, m->m_data, sizeof (m->m_data), 0,
			    (void *)from, &fromlen);

  if (num_bytes <= 0) {
    SU_DEBUG_5(("%s: %s\n", "sres_resolver_receive", su_strerror(su_errno())));
    return 0;
  }

  if (num_bytes > 65535)
    num_bytes = 65535;

  dns = sres_server_by_socket(res, socket);
  if (!dns)
    return 0;

  m->m_size = (uint16_t)num_bytes;

  /* Decode the received message and get the matching query object */
  error = sres_decode_msg(res, m, &query, &reply);

  sres_log_response(res, m, from, query, reply);

  if (query == NULL)
    ;
  else if (error == SRES_EDNS0_ERR) {
    dns->dns_edns = edns_not_supported;
    assert(query->q_id);
    sres_remove_query(res, query, 0);
    sres_gen_id(res, query);
    sres_qtable_append(res->res_queries, query);
    sres_send_dns_query(res, query);
    query->q_retry_count++;
  }
  else if (!error && reply) {
    /* Remove the query from the pending list */
    sres_remove_query(res, query, 1);

    /* Resolve the CNAME alias, if necessary */
    if (query->q_type != sres_type_cname && query->q_type != sres_qtype_any &&
        reply[0] && reply[0]->sr_type == sres_type_cname) {
      const char *alias = reply[0]->sr_cname[0].cn_cname;
      sres_record_t **cached = NULL;

      /* Check for the aliased results in the cache */
      if (sres_cache_get(res->res_cache, query->q_type, alias, &cached)
          > 0) {
        reply = cached;
      }
      else {
        /* Submit a query with the aliased name, dropping this result */
        sres_resolve_cname(res, query, alias);
        return 1;
      }
    }

    /* Notify the listener */
    if (query->q_callback != NULL)
      (query->q_callback)(query->q_context, query, reply);

    sres_free_query(res, query);
  }
  else {
    sres_query_report_error(query, reply);
  }

  return 1;
}

static
void sres_log_response(sres_resolver_t const *res,
		       sres_message_t const *m,
		       struct sockaddr_storage const *from,
		       sres_query_t const *query,
		       sres_record_t * const *reply)
{
  if (SU_LOG->log_level >= 5) {
#ifndef ADDRSIZE
#define ADDRSIZE 48
#endif
    char host[ADDRSIZE] = "*";
    uint16_t port = 0;

    if (from == NULL)
      ;
    else if (from->ss_family == AF_INET) {
      struct sockaddr_in sin;
      memcpy(&sin, from, sizeof sin);
      su_inet_ntop(AF_INET, &sin.sin_addr, host, sizeof host);
      port = sin.sin_port;
    }
#if HAVE_SIN6
    else if (from->ss_family == AF_INET6) {
      struct sockaddr_in6 sin6;
      memcpy(&sin6, from, sizeof sin6);
      su_inet_ntop(AF_INET6, &sin6.sin6_addr, host, sizeof host);
      port = sin6.sin6_port;
    }
#endif

    SU_DEBUG_5(("sres_resolver_receive(%p, %p) id=%u (from [%s]:%u)\n",
		(void *)res, (void *)query, m->m_id,
		host, ntohs(port)));
  }
}

/** Decode DNS message.
 *
 *
 * @retval 0 if successful
 * @retval >0 if message indicated error
 * @retval -1 if decoding error
 */
static
int
sres_decode_msg(sres_resolver_t *res,
		sres_message_t *m,
		sres_query_t **qq,
		sres_record_t ***return_answers)
{
  sres_record_t *rr = NULL, **answers = NULL, *error = NULL;
  sres_query_t *query = NULL, **hq;
  su_home_t *chome;
  hash_value_t hash;
  int err;
  unsigned i, total, errorcount = 0;

  assert(res && m && return_answers);

  time(&res->res_now);
  chome = CHOME(res->res_cache);

  *qq = NULL;
  *return_answers = NULL;

  m->m_offset = sizeof(m->m_packet.mp_header);

  if (m->m_size < m->m_offset) {
    SU_DEBUG_5(("sres_decode_msg: truncated message\n"));
    return -1;
  }

  m->m_flags   = ntohs(m->m_flags);
  m->m_qdcount = ntohs(m->m_qdcount);
  m->m_ancount = ntohs(m->m_ancount);
  m->m_nscount = ntohs(m->m_nscount);
  m->m_arcount = ntohs(m->m_arcount);

  hash = Q_PRIME * m->m_id;

  /* Search for query with this ID */
  for (hq = sres_qtable_hash(res->res_queries, hash);
       *hq;
       hq = sres_qtable_next(res->res_queries, hq))
    if (hash == (*hq)->q_hash)
      break;

  *qq = query = *hq;

  if (!query) {
    SU_DEBUG_5(("sres_decode_msg: matching query for id=%u\n", m->m_id));
    return -1;
  }

  assert(query && m->m_id == query->q_id);

  if ((m->m_flags & 15) == SRES_FORMAT_ERR && query->q_edns)
    return SRES_EDNS0_ERR;

  /* Scan question section.
   * XXX: never mind the useless result values, this is done
   * for the side effects in m */
  for (i = 0; i < m->m_qdcount; i++) {
    char name[1024];
    uint16_t qtype, qclass;
    m_get_domain(name, sizeof(name), m, 0); /* Query domain */
    qtype = m_get_uint16(m);  /* Query type */
    qclass = m_get_uint16(m); /* Query class */
  }

  if (m->m_error) {
    SU_DEBUG_5(("sres_decode_msg: %s\n", m->m_error));
    return -1;
  }

  err = m->m_flags & SRES_HDR_RCODE;

  if (m->m_ancount == 0 && err == 0)
    err = SRES_RECORD_ERR;

  if (err == SRES_RECORD_ERR ||
      err == SRES_NAME_ERR ||
      err == SRES_UNIMPL_ERR)
    errorcount = 1;

  total = errorcount + m->m_ancount + m->m_nscount + m->m_arcount;

  answers = su_zalloc(chome, (total + 2) * sizeof answers[0]);
  if (!answers)
    return -1;

  /* Scan resource records */
  for (i = 0; i < total; i++) {
    if (i < errorcount)
      rr = error = sres_create_error_rr(res->res_cache, query, err);
    else
      rr = sres_create_record(res, m, i - errorcount);

    if (!rr) {
      SU_DEBUG_5(("sres_create_record: %s\n", m->m_error));
      break;
    }

    if (error && rr->sr_type == sres_type_soa) {
      sres_soa_record_t *soa = (sres_soa_record_t *)rr;
      if (error->sr_ttl > soa->soa_minimum && soa->soa_minimum > 10)
	  error->sr_ttl = soa->soa_minimum;
    }

    answers[i] = rr;
  }

  if (i < total) {
    SU_DEBUG_5(("sres_decode_msg: got %u but expected "
		"errors=%u an=%u ar=%u ns=%u\n", i, errorcount,
		m->m_ancount, m->m_arcount, m->m_nscount));
    for (i = 0; i < total; i++)
      sres_cache_free_record(res->res_cache, answers[i]);
    su_free(chome, answers);
    return -1;
  }

  if (m->m_ancount > 0 && errorcount == 0 && query->q_type < sres_qtype_tsig
      && (query->q_callback == sres_resolving_cname ||
	  answers[0]->sr_type != sres_type_cname)) {

    for (i = 0; i < m->m_ancount; i++) {
      if (query->q_type == answers[i]->sr_type)
	break;
    }

    if (i == m->m_ancount) {
      char b0[8], b1[8];
      /* The queried request was not found */
      SU_DEBUG_5(("sres_decode_msg: sent query %s, got %s\n",
		  sres_record_type(query->q_type, b0),
		  sres_record_type(answers[0]->sr_type, b1)));
      rr = sres_create_error_rr(res->res_cache, query, err = SRES_RECORD_ERR);
      memmove(answers + 1, answers, (sizeof answers[0]) * total++);
      answers[0] = rr;
      errorcount = 1;
    }
  }

  for (i = 0; i < total; i++) {
    rr = answers[i];

    if (i < m->m_ancount + errorcount)
      /* Increase reference count of entry passed in answers */
      rr->sr_refcount++;
    else
      /* Do not pass extra records to user */
      answers[i] = NULL;

    sres_cache_store(res->res_cache, rr, res->res_now);
  }

  *return_answers = answers;

  return err;
}

static
sres_record_t *
sres_create_record(sres_resolver_t *res, sres_message_t *m, int nth)
{
  sres_cache_t *cache = res->res_cache;
  sres_record_t *sr, sr0[1];

  uint16_t m_size;
  char name[1025];
  unsigned len;
  char btype[8], bclass[8];

  sr = memset(sr0, 0, sizeof sr0);

  len = m_get_domain(sr->sr_name = name, sizeof(name) - 1, m, 0); /* Name */
  sr->sr_type = m_get_uint16(m);  /* Type */
  sr->sr_class = m_get_uint16(m); /* Class */
  sr->sr_ttl = m_get_uint32(m);   /* TTL */
  sr->sr_rdlen = m_get_uint16(m); /* rdlength */
  sr->sr_parsed = 1;
  if (m->m_error)
    goto error;
  if (len >= (sizeof name)) {
    m->m_error = "too long domain name in record";
    goto error;
  }
  name[len] = 0;

  SU_DEBUG_9(("%s RR received %s %s %s %d rdlen=%d\n",
	      nth < m->m_ancount ? "ANSWER" :
	      nth < m->m_ancount + m->m_nscount ? "AUTHORITY" :
	      "ADDITIONAL",
	      name,
	      sres_record_type(sr->sr_type, btype),
	      sres_record_class(sr->sr_class, bclass),
	      sr->sr_ttl, sr->sr_rdlen));

  if (m->m_offset + sr->sr_rdlen > m->m_size) {
    m->m_error = "truncated message";
    goto error;
  }

  m_size = m->m_size;
  /* limit m_size to indicated rdlen, check whether record is truncated */
  m->m_size = m->m_offset + sr->sr_rdlen;

  switch (sr->sr_type) {
  case sres_type_soa:
    sr = sres_init_rr_soa(cache, sr->sr_soa, m);
    break;
  case sres_type_a:
    sr = sres_init_rr_a(cache, sr->sr_a, m);
    break;
  case sres_type_a6:
    sr = sres_init_rr_a6(cache, sr->sr_a6, m);
    break;
  case sres_type_aaaa:
    sr = sres_init_rr_aaaa(cache, sr->sr_aaaa, m);
    break;
  case sres_type_cname:
    sr = sres_init_rr_cname(cache, sr->sr_cname, m);
    break;
  case sres_type_ptr:
    sr = sres_init_rr_ptr(cache, sr->sr_ptr, m);
    break;
  case sres_type_srv:
    sr = sres_init_rr_srv(cache, sr->sr_srv, m);
    break;
  case sres_type_naptr:
    sr = sres_init_rr_naptr(cache, sr->sr_naptr, m);
    break;
  default:
    sr = sres_init_rr_unknown(cache, sr->sr_record, m);
    break;
  }

  if (m->m_error)
    goto error;

  if (sr == sr0)
    sr = sres_cache_alloc_record(cache, sr, 0);

  if (sr == NULL) {
    m->m_error = "memory exhausted";
    goto error;
  }

  /* Fill in the common fields */
  m->m_size = m_size;

  return sr;

 error:
  if (sr && sr != sr0)
    sres_cache_free_record(cache, sr);
  SU_DEBUG_5(("%s: %s\n", "sres_create_record", m->m_error));
  return NULL;
}

/** Decode SOA record */
static sres_record_t *sres_init_rr_soa(sres_cache_t *cache,
				       sres_soa_record_t *soa,
				       sres_message_t *m)
{
  uint16_t moffset, roffset;
  unsigned mnamelen, rnamelen;

  soa->soa_record->r_size = sizeof *soa;

  moffset = m->m_offset, mnamelen = m_get_domain(NULL, 0, m, 0) + 1;
  roffset = m->m_offset, rnamelen = m_get_domain(NULL, 0, m, 0) + 1;

  soa->soa_serial = m_get_uint32(m);
  soa->soa_refresh = m_get_uint32(m);
  soa->soa_retry = m_get_uint32(m);
  soa->soa_expire = m_get_uint32(m);
  soa->soa_minimum = m_get_uint32(m);

  if (m->m_error)
    return NULL;

  soa = (void *)sres_cache_alloc_record(cache, (void *)soa,
					mnamelen + rnamelen);

  if (soa) {
    char *mname, *rname;

    assert(moffset > 0 && roffset > 0 && mnamelen > 1 && rnamelen > 1);

    m_get_domain(mname = (char *)(soa + 1), mnamelen, m, moffset);
    soa->soa_mname = mname;

    m_get_domain(rname = mname + mnamelen, rnamelen, m, roffset);
    soa->soa_rname = rname;
  }

  return (sres_record_t *)soa;
}

/** Decode A record */
static sres_record_t *sres_init_rr_a(sres_cache_t *cache,
				     sres_a_record_t *a,
				     sres_message_t *m)
{
  a->a_record->r_size = sizeof *a;

  a->a_addr.s_addr = htonl(m_get_uint32(m));

  return (sres_record_t *)a;
}

/** Decode A6 record. See @RFC2874 */
static sres_record_t *sres_init_rr_a6(sres_cache_t *cache,
				      sres_a6_record_t *a6,
				      sres_message_t *m)
{

  unsigned suffixlen = 0, i;
  unsigned prefixlen = 0;
  uint16_t offset;

  a6->a6_record->r_size = sizeof *a6;

  a6->a6_prelen = m_get_uint8(m);

  if (a6->a6_prelen > 128) {
    m->m_error = "Invalid prefix length in A6 record";
    return NULL;
  }

  suffixlen = (128 + 7 - a6->a6_prelen) / 8;
  for (i = 16 - suffixlen; i < 16; i++)
    a6->a6_suffix.u6_addr[i] = m_get_uint8(m);

  if (a6->a6_prelen > 0) {
    if (suffixlen > 0)
      /* Zero pad bits */
      a6->a6_suffix.u6_addr[16 - suffixlen] &= 0xff >> (a6->a6_prelen & 7);

    offset = m->m_offset, prefixlen = m_get_domain(NULL, 0, m, 0) + 1;

    if (m->m_error)
      return NULL;

    a6 = (void *)sres_cache_alloc_record(cache, (void *)a6, prefixlen);
    if (a6)
      m_get_domain(a6->a6_prename = (char *)(a6 + 1), prefixlen, m, offset);
  }

  return (sres_record_t *)a6;
}

/** Decode AAAA record */
static sres_record_t *sres_init_rr_aaaa(sres_cache_t *cache,
					sres_aaaa_record_t *aaaa,
					sres_message_t *m)
{
  aaaa->aaaa_record->r_size = sizeof *aaaa;

  if (m->m_offset + sizeof(aaaa->aaaa_addr) <= m->m_size) {
    memcpy(&aaaa->aaaa_addr, m->m_data + m->m_offset, sizeof(aaaa->aaaa_addr));
    m->m_offset += sizeof(aaaa->aaaa_addr);
  }
  else
    m->m_error = "truncated AAAA record";

  return (sres_record_t *)aaaa;
}

/** Decode CNAME record */
static sres_record_t *sres_init_rr_cname(sres_cache_t *cache,
					 sres_cname_record_t *cn,
					 sres_message_t *m)
{
  uint16_t offset;
  unsigned dlen;

  cn->cn_record->r_size = sizeof *cn;

  offset = m->m_offset, dlen = m_get_domain(NULL, 0, m, 0) + 1;

  if (m->m_error)
    return NULL;

  cn = (void *)sres_cache_alloc_record(cache, (void *)cn, dlen);
  if (cn)
    m_get_domain(cn->cn_cname = (char *)(cn + 1), dlen, m, offset);

  return (sres_record_t *)cn;
}

/** Decode PTR record */
static sres_record_t *sres_init_rr_ptr(sres_cache_t *cache,
				       sres_ptr_record_t *ptr,
				       sres_message_t *m)
{
  uint16_t offset;
  unsigned dlen;

  ptr->ptr_record->r_size = sizeof *ptr;

  offset = m->m_offset, dlen = m_get_domain(NULL, 0, m, 0) + 1;

  if (m->m_error)
    return NULL;

  ptr = (void *)sres_cache_alloc_record(cache, (void *)ptr, dlen);
  if (ptr)
    m_get_domain(ptr->ptr_domain = (char *)(ptr + 1), dlen, m, offset);

  return (sres_record_t *)ptr;
}

/** Decode SRV record */
static sres_record_t *sres_init_rr_srv(sres_cache_t *cache,
				       sres_srv_record_t *srv,
				       sres_message_t *m)
{
  uint16_t offset;
  unsigned dlen;

  srv->srv_record->r_size = sizeof *srv;

  srv->srv_priority = m_get_uint16(m);
  srv->srv_weight = m_get_uint16(m);
  srv->srv_port = m_get_uint16(m);
  offset = m->m_offset, dlen = m_get_domain(NULL, 0, m, 0) + 1;
  if (m->m_error)
    return NULL;

  srv = (void *)sres_cache_alloc_record(cache, (void *)srv, dlen);
  if (srv)
    m_get_domain(srv->srv_target = (char *)(srv + 1), dlen, m, offset);

  return (sres_record_t *)srv;
}

/** Decode NAPTR record */
static sres_record_t *sres_init_rr_naptr(sres_cache_t *cache,
					 sres_naptr_record_t *na,
					 sres_message_t *m)
{
  uint16_t offset[4];
  unsigned len[4];

  na->na_record->r_size = sizeof *na;

  na->na_order = m_get_uint16(m);
  na->na_prefer = m_get_uint16(m);

  offset[0] = m->m_offset, len[0] = m_get_string(NULL, 0, m, 0) + 1;
  offset[1] = m->m_offset, len[1] = m_get_string(NULL, 0, m, 0) + 1;
  offset[2] = m->m_offset, len[2] = m_get_string(NULL, 0, m, 0) + 1;
  offset[3] = m->m_offset, len[3] = m_get_domain(NULL, 0, m, 0) + 1;

  if (m->m_error)
    return NULL;

  na = (void *)sres_cache_alloc_record(cache, (void *)na,
				       len[0] + len[1] + len[2] + len[3]);
  if (na) {
    char *s = (char *)(na + 1);
    m_get_string(na->na_flags = s, len[0], m, offset[0]), s += len[0];
    m_get_string(na->na_services = s, len[1], m, offset[1]), s += len[1];
    m_get_string(na->na_regexp = s, len[2], m, offset[2]), s += len[2];
    m_get_domain(na->na_replace = s, len[3], m, offset[3]), s += len[3];
  }

  return (sres_record_t *)na;
}

/** Decode unknown record */
static sres_record_t *sres_init_rr_unknown(sres_cache_t *cache,
					   sres_common_t *r,
					   sres_message_t *m)
{
  if (m->m_offset + r->r_rdlen > m->m_size)
    m->m_error = "truncated record";

  if (m->m_error)
    return NULL;

  r->r_size = sizeof *r;

  r = (void *)sres_cache_alloc_record(cache, (void *)r, r->r_rdlen + 1);
  if (r) {
    char *data = (char *)(r + 1);

    r->r_parsed = 0;

    memcpy(data, m->m_data + m->m_offset, r->r_rdlen);
    m->m_offset += r->r_rdlen;
    data[r->r_rdlen] = 0;
  }

  return (sres_record_t *)r;
}

static
sres_record_t *sres_create_error_rr(sres_cache_t *cache,
				    sres_query_t const *q,
				    uint16_t errcode)
{
  sres_record_t *sr, r[1];
  char buf[SRES_MAXDNAME];

  sr = memset(r, 0, sizeof *sr);

  sr->sr_name = (char *)sres_toplevel(buf, sizeof buf, q->q_name);
  sr->sr_size = sizeof *sr;
  sr->sr_status = errcode;
  sr->sr_type = q->q_type;
  sr->sr_class = q->q_class;
  sr->sr_ttl = 10 * 60;

  return sres_cache_alloc_record(cache, sr, 0);
}

/* Message processing primitives */

static
void
m_put_uint16(sres_message_t *m,
	     uint16_t h)
{
  uint8_t *p;

  if (m->m_error)
    return;

  p = m->m_data + m->m_offset;
  m->m_offset += sizeof h;

  if (m->m_offset > m->m_size) {
    m->m_error = "message size overflow";
    return;
  }

  p[0] = h >> 8; p[1] = h;
}

static
void
m_put_uint32(sres_message_t *m,
	     uint32_t w)
{
  uint8_t *p;

  if (m->m_error)
    return;

  p = m->m_data + m->m_offset;
  m->m_offset += sizeof w;

  if (m->m_offset > m->m_size) {
    m->m_error = "message size overflow";
    return;
  }

  p[0] = w >> 24; p[1] = w >> 16; p[2] = w >> 8; p[3] = w;
}

/*
 * Put domain into query
 */
static
uint16_t
m_put_domain(sres_message_t *m,
	     char const *domain,
	     uint16_t top,
	     char const *topdomain)
{
  char const *label;
  size_t llen;

  if (m->m_error)
    return top;

  /* Copy domain into query label at a time */
  for (label = domain; label && label[0]; label += llen) {
    if (label[0] == '.' && label[1] != '\0') {
      m->m_error = "empty label";
      return 0;
    }

    llen = strcspn(label, ".");

    if (llen >= 64) {
      m->m_error = "too long label";
      return 0;
    }
    if (m->m_offset + llen + 1 > m->m_size) {
      m->m_error = "message size overflow";
      return 0;
    }

    m->m_data[m->m_offset++] = (uint8_t)llen;
    memcpy(m->m_data + m->m_offset, label, llen);
    m->m_offset += (uint8_t)llen;

    if (label[llen] == '\0')
      break;
    if (llen == 0)
      return top;
    if (label[llen + 1])
      llen++;
  }

  if (top) {
    m_put_uint16(m, 0xc000 | top);
    return top;
  }
  else if (topdomain) {
    uint16_t retval = m->m_offset;
    m_put_domain(m, topdomain, 0, NULL);
    return retval;
  }
  else if (m->m_offset < m->m_size)
    m->m_data[m->m_offset++] = '\0';
  else
    m->m_error = "message size overflow";

  return 0;
}

static
uint32_t
m_get_uint32(sres_message_t *m)
{
  uint8_t const *p = m->m_data + m->m_offset;

  if (m->m_error)
    return 0;

  m->m_offset += 4;

  if (m->m_offset > m->m_size) {
    m->m_error = "truncated message";
    return 0;
  }

  return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static
uint16_t
m_get_uint16(sres_message_t *m)
{
  uint8_t const *p = m->m_data + m->m_offset;

  if (m->m_error)
    return 0;

  m->m_offset += 2;

  if (m->m_offset > m->m_size) {
    m->m_error = "truncated message";
    return 0;
  }

  return (p[0] << 8) | p[1];
}

static
uint8_t
m_get_uint8(sres_message_t *m)
{
  uint8_t const *p = m->m_data + m->m_offset;

  if (m->m_error)
    return 0;

  m->m_offset += 1;

  if (m->m_offset > m->m_size) {
    m->m_error = "truncated message";
    return 0;
  }

  return p[0];
}

/**
 * Get a string.
 */
static unsigned
m_get_string(char *d,
	     unsigned n,
	     sres_message_t *m,
	     uint16_t offset)
{
  uint8_t size;
  uint8_t *p = m->m_data;
  int save_offset;

  if (m->m_error)
    return 0;

  if (offset == 0)
    offset = m->m_offset, save_offset = 1;
  else
    save_offset = 0;

  size = p[offset++];

  if (size + offset >= m->m_size) {
    m->m_error = "truncated message";
    return size;
  }

  offset += size;

  if (save_offset)
    m->m_offset = offset;

  if (n == 0 || d == NULL)
    return size;		/* Just return the size (without NUL). */

  memcpy(d, p + offset - size, size < n ? size : n);

  if (size < n)
    d[size] = '\0';		/* NUL terminate */

  return size;
}

/**
 * Uncompress a domain.
 *
 * @param offset start uncompression from this point in message
 */
static unsigned
m_get_domain(char *d,
	     unsigned n,
	     sres_message_t *m,
	     uint16_t offset)
{
  uint8_t cnt;
  unsigned i = 0;
  uint8_t *p = m->m_data;
  uint16_t new_offset;
  int save_offset;

  if (m->m_error)
    return 0;

  if (d == NULL)
    n = 0;

  if (offset == 0)
    offset = m->m_offset, save_offset = 1;
  else
    save_offset = 0;

  while ((cnt = p[offset++])) {
    if (cnt >= 0xc0) {
      if (offset >= m->m_size) {
        m->m_error = "truncated message";
        return 0;
      }

      new_offset = ((cnt & 0x3F) << 8) + p[offset++];

      if (save_offset)
        m->m_offset = offset;

      if (new_offset <= 0 || new_offset >= m->m_size) {
        m->m_error = "invalid domain compression";
        return 0;
      }

      offset = new_offset;
      save_offset = 0;
    }
    else {
      if (offset + cnt >= m->m_size) {
        m->m_error = "truncated message";
        return 0;
      }
      if (i + cnt + 1 < n) {
        memcpy(d + i, p + offset, cnt);
        d[i + cnt] = '.';
      }

      i += cnt + 1;
      offset += cnt;
    }
  }

  if (i == 0) {
    if (i < n)
      d[i] = '.';
    i++;
  }

  if (i < n)
    d[i] = '\0';

  if (save_offset)
    m->m_offset = offset;

  return i;
}
