/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2007 Nokia Corporation.
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

/**@CFILE sres_sip.c
 * @brief RFC3263 SIP Resolver
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @sa
 * @RFC3263
 */

#include "config.h"

struct sres_sip_tport;
struct srs_step;
struct srs_hint;
struct sres_sip_s;

#include <sofia-sip/su_string.h>
#include <sofia-sip/hostdomain.h>
#include <sofia-sip/url.h>
#include <sofia-sip/su_errno.h>
#include <sofia-sip/su.h>

#define SRES_CONTEXT_T    struct srs_step

#include <sofia-sip/su_uniqueid.h>
#include "sofia-sip/sres_sip.h"
#include "sofia-resolv/sres.h"
#include "sofia-resolv/sres_record.h"

#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#define SU_LOG sresolv_log

#include <sofia-sip/su_debug.h>

/**
 * For each transport, we have name used by tport module, SRV prefixes used
 * for resolving, and NAPTR service/conversion.
 */
struct sres_sip_tport {
  int stp_type;		/**< Default protocol for this URL type */
  uint16_t stp_number;	/**< Protocol number used by tport module */
  uint16_t stp_port;	/**< Default port number */
  char stp_name[6];	/**< Named used by tport module */
  char stp_prefix[14];	/**< Prefix for SRV domains */
  char stp_service[10];	/**< NAPTR service (empty if none) */
};

#define N_TRANSPORTS 20

static struct sres_sip_tport const sres_sip_tports[N_TRANSPORTS] = {
  { url_sip, TPPROTO_UDP, 5060, "udp", "_sip._udp.",  "SIP+D2U"  },
  { url_sip, TPPROTO_TCP, 5060, "tcp", "_sip._tcp.",  "SIP+D2T"  },
  { url_sips, TPPROTO_TLS, 5061, "tls", "_sips._tcp.", "SIPS+D2T" },
  { url_any, TPPROTO_SCTP, 5060, "sctp", "_sip._sctp.", "SIP+D2S" },
};

struct srs_step;
struct srs_hint;

struct sres_sip_s
{
  su_home_t             srs_home[1];
  sres_resolver_t      *srs_resolver;

  sres_sip_notify_f    *srs_callback;
  sres_sip_magic_t     *srs_magic;

  /* What we are resolving */
  url_t                *srs_url;

  /* Parsed */
  char const           *srs_target;
  short                 srs_maddr;
  short                 srs_transport;  /**< URL-specified transport protocol */
  uint16_t              srs_port;

  int                   srs_error;

  /** Resolving steps (to take) */
  struct srs_step *srs_head, **srs_queue; /**< List of subqueries */
  struct srs_step **srs_process;	  /**< Next result to process  */
  struct srs_step **srs_send;		  /**< Next query to make */

  /** Results */
  su_addrinfo_t *srs_results, **srs_next, **srs_tail;

  unsigned srs_try_naptr:1, srs_try_srv:1, srs_try_a:1;
  unsigned srs_canonname:1, srs_numeric:1;

  unsigned srs_blocking:1, srs_complete:1, :0;

  /** Hints for this request. */
  struct srs_hint {
    struct sres_sip_tport const *hint_stp;
    uint16_t hint_qtype;
    uint16_t hint_port;
  } srs_hints[2 * N_TRANSPORTS + 2];
  /* srs_hints[0] is ignored, srs_hints[2*N_TRANSPORTS + 1] is sentinel. */
};

/** Intermediate steps */
struct srs_step
{
  struct srs_step *sp_next;
  sres_sip_t *sp_srs;		/**< Backpointer */
  struct srs_step *sp_already;   /**< Step with identical query
				    - itself if first one */
  struct srs_step *sp_trace;	  /**< Step that resulted in this step */
  sres_record_t const *sp_origin; /**< DNS record that resulted in this step */

  char const *sp_target;
  sres_query_t *sp_query;

  /** Query status.
   *
   * STEP_NEW (-4) when created,
   * STEP_QUEUED (-3) when queued,
   * STEP_CACHED (-2) when found cached response
   * STEP_SENT (-1) when query has been sent
   * (positive) status from response otherwise
   */
  int sp_status;

  sres_record_t **sp_results;

  uint16_t sp_hint;	        /* Number of hint */

  uint16_t sp_port;		/* port number */
  uint16_t sp_type;		/* query type */
  uint16_t sp_prefer;		/* order */
  uint16_t sp_priority;		/* priority */
  uint16_t sp_weight;		/* weight */

  uint16_t sp_grayish;		/* candidate for graylisting */
};

#define SP_HINT(sp) ((sp)->sp_hint ? &(sp)->sp_srs[sp->sp_hint] : NULL)
#define SP_STP(sp) ((sp)->sp_hint ? (sp)->sp_srs[sp->sp_hint].hint_stp : NULL);

enum {
  STEP_NEW = -4,
  STEP_QUEUED = -3,
  STEP_CACHED = -2,
  STEP_SENT = -1,
  STEP_OK = 0
};

static void _sres_sip_destruct(void *_srs);
static sres_sip_t *sres_sip_fatal(sres_sip_t *srs, int error);
static void sres_sip_hint(sres_sip_t *srs, int qtype, int protocol);
static int sres_sip_url_transport(url_t const *uri);
static char const *sres_sip_transport_name(int number);
void sres_sip_graylist(sres_sip_t *srs, struct srs_step *step);

static int sres_sip_is_waiting(sres_sip_t const *srs);
static void sres_sip_return_results(sres_sip_t *srs, int final);

static void sres_sip_try_next_steps(sres_sip_t *srs);
static void sres_sip_try_naptr_steps(sres_sip_t *srs);
static void sres_sip_try_srv_steps(sres_sip_t *srs);
static void sres_sip_try_a_aaaa_steps(sres_sip_t *srs);

static struct srs_step *sres_sip_step_new(
  sres_sip_t *srs,
  int type,
  char const *prefix,
  char const *domain);

static void sres_sip_append_step(sres_sip_t *srs,
				 struct srs_step *step);

static void sres_sip_insert_step(sres_sip_t *srs,
				 struct srs_step *step);

static int sres_sip_send_steps(sres_sip_t *srs);
static void sres_sip_answer(struct srs_step *step,
			    sres_query_t *q,
			    sres_record_t *answers[]);
static int sres_sip_status_of_answers(sres_record_t *answers[], uint16_t type);
static int sres_sip_count_answers(sres_record_t *answers[], uint16_t type);
static void sres_sip_log_answers(sres_sip_t *srs,
				 struct srs_step *step,
				 sres_record_t *answers[]);

static int sres_sip_process(sres_sip_t *srs);

static void sres_sip_process_naptr(sres_sip_t *srs,
				   struct srs_step *nq,
				   sres_record_t *answers[]);
static void sres_sip_sort_naptr(sres_record_t *answers[]);

static void sres_sip_step_by_naptr(sres_sip_t *srs,
				  struct srs_step *,
				  uint16_t hint,
				  sres_naptr_record_t const *na);

static void sres_sip_process_srv(sres_sip_t *srs,
				 struct srs_step *nq,
				 sres_record_t *answers[]);

static void sres_sip_sort_srv(sres_record_t *answers[]);

#if SU_HAVE_IN6
static void sres_sip_process_aaaa(sres_sip_t *srs, struct srs_step *,
				  sres_record_t *answers[]);
#endif
static void sres_sip_process_a(sres_sip_t *srs, struct srs_step *,
			       sres_record_t *answers[]);

static void sres_sip_process_cname(sres_sip_t *srs,
				   struct srs_step *step0,
				   sres_record_t *answers[]);

static void sres_sip_process_numeric(sres_sip_t *srs);

static void sres_sip_append_result(sres_sip_t *srs,
				   su_addrinfo_t const *result);

/** Resolve a SIP uri.
 *
 */
sres_sip_t *
sres_sip_new(sres_resolver_t *sres,
	     url_string_t const *uri,
	     su_addrinfo_t const *hints,
	     int naptr, int srv,
	     sres_sip_notify_f *callback,
	     sres_sip_magic_t *magic)
{
  sres_sip_t *srs;
  url_t *u;
  char const *target, *port;
  su_addrinfo_t const hints0 = { 0 };
  int numeric;
  int transport;
  isize_t maddr;

  if (sres == NULL || uri == NULL)
    return su_seterrno(EFAULT), NULL;

  srs = su_home_new(sizeof *srs);
  if (srs == NULL)
    return NULL;

  srs->srs_queue = srs->srs_send = srs->srs_process = &srs->srs_head;
  srs->srs_next = srs->srs_tail = &srs->srs_results;

  su_home_destructor(srs->srs_home, _sres_sip_destruct);

  srs->srs_url = u = url_hdup(srs->srs_home, (url_t *)uri);
  if (u == NULL)
    return sres_sip_fatal(srs, SRES_SIP_ERR_BAD_URI);
  if (u->url_type != url_sip && u->url_type != url_sips)
    return sres_sip_fatal(srs, SRES_SIP_ERR_BAD_URI);

  /* RFC 3263:
   We define TARGET as the value of the maddr parameter of the URI, if
   present, otherwise, the host value of the hostport component of the
   URI.
  */
  maddr = url_param(u->url_params, "maddr=", NULL, 0);

  if (maddr) {
    target = su_alloc(srs->srs_home, maddr);
    url_param(u->url_params, "maddr=", (char *)target, maddr);
  }
  else {
    target = u->url_host;
  }

  if (!target)
    return sres_sip_fatal(srs, SRES_SIP_ERR_BAD_URI);

  srs->srs_target = target;
  srs->srs_maddr = maddr != 0;
  port = u->url_port;

  srs->srs_transport = transport = sres_sip_url_transport(u);
  if (transport == -1)
    return sres_sip_fatal(srs, SRES_SIP_ERR_NO_TPORT);

  if (transport && u->url_type == url_sips)
    /* <sips:host.domain;transport=tcp> */
    srs->srs_transport = transport | TPPROTO_SECURE;

  numeric = host_is_ip_address(target);

  if (numeric) {
    naptr = 0, srv = 0;
    if (!port || !strlen(port))
      port = url_port_default((enum url_type_e)u->url_type);
  }

  /* RFC 3263:
     If the TARGET was not a numeric IP address, but a port is present in
     the URI, the client performs an A or AAAA record lookup of the domain
     name.  The result will be a list of IP addresses, each of which can
     be contacted at the specific port from the URI and transport protocol
     determined previously.  The client SHOULD try the first record.  If
     an attempt should fail, based on the definition of failure in Section
     4.3, the next SHOULD be tried, and if that should fail, the next
     SHOULD be tried, and so on.

     This is a change from RFC 2543.  Previously, if the port was
     explicit, but with a value of 5060, SRV records were used.  Now, A
     or AAAA records will be used.
  */
  if (port && strlen(port)) {
    unsigned long number;
    naptr = 0, srv = 0;
    srs->srs_port = number = strtoul(port, 0, 10);
    if (number == 0 || number >= 65536)
      return sres_sip_fatal(srs, SRES_SIP_ERR_BAD_URI);
  }

  if (hints == NULL)
    hints = &hints0;

  srs->srs_canonname = (hints->ai_flags & AI_CANONNAME) != 0;
  srs->srs_numeric = (hints->ai_flags & AI_NUMERICHOST) != 0;

  srs->srs_resolver = sres_resolver_ref(sres);
  srs->srs_blocking = sres_is_blocking(sres);

  srs->srs_try_srv = srv;
  srs->srs_try_naptr = naptr;
  srs->srs_try_a = !numeric;

  for (;hints; hints = hints->ai_next) {
#if SU_HAVE_IN6
    if (hints->ai_family == 0 || hints->ai_family == AF_INET6) {
      sres_sip_hint(srs, sres_type_aaaa, hints->ai_protocol);
    }
#endif
    if (hints->ai_family == 0 || hints->ai_family == AF_INET) {
      sres_sip_hint(srs, sres_type_a, hints->ai_protocol);
    }
  }

  SU_DEBUG_5(("srs(%p): resolving <%s:%s%s%s%s%s%s%s>\n",
	      (void *)srs,
	      u->url_scheme, u->url_host,
	      u->url_port ? ":" : "", u->url_port ? u->url_port : "",
	      maddr ? ";maddr=" : "",
	      maddr ? target : "",
	      transport ? ";transport=" : "",
	      transport ? sres_sip_transport_name(transport) : ""));

  if (numeric)
    sres_sip_process_numeric(srs);
  else
    sres_sip_next_step(srs);

  srs->srs_callback = callback;
  srs->srs_magic = magic;

  return srs;
}

sres_sip_t *
sres_sip_ref(sres_sip_t *srs)
{
  return (sres_sip_t *)su_home_ref(srs->srs_home);
}

void
sres_sip_unref(sres_sip_t *srs)
{
  su_home_unref(srs->srs_home);
}

static void
_sres_sip_destruct(void *_srs)
{
  sres_sip_t *srs = _srs;
  sres_resolver_t *sres = srs->srs_resolver;
  struct srs_step *step;

  SU_DEBUG_5(("srs(%p): destroyed\n", (void *) (_srs)));

  srs->srs_resolver = NULL;

  for (step = srs->srs_head; step; step = step->sp_next) {
    if (step->sp_already == step)
      sres_free_answers(sres, step->sp_results);
    step->sp_results = NULL;
    sres_query_bind(step->sp_query, NULL, NULL), step->sp_query = NULL;
  }

  sres_resolver_unref(sres);
}

int
sres_sip_error(sres_sip_t *srs)
{
  return srs ? srs->srs_error : SRES_SIP_ERR_FAULT;
}

su_addrinfo_t const *
sres_sip_results(sres_sip_t *srs)
{
  return srs ? srs->srs_results : NULL;
}

su_addrinfo_t const *
sres_sip_next(sres_sip_t *srs)
{
  su_addrinfo_t *next = NULL;

  if (srs) {
    next = *srs->srs_next;
    if (next)
      srs->srs_next = &next->ai_next;
  }

  return next;
}

static sres_sip_t *
sres_sip_fatal(sres_sip_t *srs, int error)
{
  srs->srs_error = error;
  srs->srs_try_naptr = 0, srs->srs_try_srv = 0, srs->srs_try_a = 0;
  return srs;
}

static void
sres_sip_hint(sres_sip_t *srs, int qtype, int protocol)
{
  size_t i, j;
  int match = 0;
  uint16_t port = srs->srs_port;

  for (j = 0; sres_sip_tports[j].stp_number; j++) {
    struct sres_sip_tport const *stp = &sres_sip_tports[j];
    int already = 0;

    if (protocol && stp->stp_number != protocol)
      continue;

    /* Use only secure transports with SIPS */
    if (srs->srs_url->url_type == url_sips) {
      if (!(stp->stp_number & TPPROTO_SECURE))
	continue;
    }

    /* If transport is specified, use it. */
    if (srs->srs_transport && stp->stp_number != srs->srs_transport)
      continue;

    for (i = 1; srs->srs_hints[i].hint_stp; i++) {
      if (srs->srs_hints[i].hint_stp == stp &&
	  srs->srs_hints[i].hint_qtype == qtype) {
	match = 1, already = 1;
	break;
      }
      assert(i <= 2 * N_TRANSPORTS);
    }

    if (!already) {
      srs->srs_hints[i].hint_stp = stp;
      srs->srs_hints[i].hint_qtype = qtype;
      srs->srs_hints[i].hint_port = port ? port : stp->stp_port;
    }
  }

  if (!match) {
    /* XXX - hint did not match configured protocols */
  }
}

/** Return protocol number for transport parameter in URI */
static int
sres_sip_url_transport(url_t const *uri)
{
  char parameter[64];
  isize_t len = 0;
  size_t i;

  if (!uri)
    return -1;
  if (!uri->url_params)
    return 0;

  len = url_param(uri->url_params, "transport", parameter, (sizeof parameter));
  if (len == 0)
    return 0;
  if (len >= sizeof parameter)
    return -1;

  for (i = 0; *sres_sip_tports[i].stp_name; i++) {
    if (su_casematch(parameter, sres_sip_tports[i].stp_name))
      return sres_sip_tports[i].stp_number;
  }

  return -1;
}

static char const *
sres_sip_transport_name(int number)
{
  size_t i;

  for (i = 0; sres_sip_tports[i].stp_number; i++) {
    if (number == sres_sip_tports[i].stp_number)
      return sres_sip_tports[i].stp_name;
  }

  return NULL;
}

/** Cancel resolver query */
void
sres_sip_cancel_resolver(sres_sip_t *srs)
{
  struct srs_step *step;

  if (!srs)
    return;

  for (step = srs->srs_head; step; step = step->sp_next) {
    sres_query_bind(step->sp_query, NULL, NULL), step->sp_query = NULL;
  }
}

#if 0

/** Graylist SRV records */
static void
sres_sip_graylist(sres_sip_t *srs,
		  struct srs_step *step)
{
  char const *target = step->sp_target, *proto = step->sp_proto;
  unsigned prio = step->sp_priority, maxprio = prio;

  /* Don't know how to graylist but SRV records */
  if (step->sp_otype != sres_type_srv)
    return;

  SU_DEBUG_5(("srs(%p): graylisting %s:%u;transport=%s\n", (void *) target, step->sp_port, proto));

  for (step = srs->srs_send; step; step = step->sp_next)
    if (step->sp_otype == sres_type_srv && step->sp_priority > maxprio)
      maxprio = step->sp_priority;

  for (step = srs->srs_done; step; step = step->sp_next)
    if (step->sp_otype == sres_type_srv && step->sp_priority > maxprio)
      maxprio = step->sp_priority;

  for (step = srs->srs_done; step; step = step->sp_next) {
    int modified;

    if (step->sp_type != sres_type_srv || strcmp(proto, step->sp_proto))
      continue;

    /* modify the SRV record(s) corresponding to the latest A/AAAA record */
    modified = sres_set_cached_srv_priority(
      srs->orq_agent->sa_resolver,
      step->sp_target,
      target,
      step->sp_port,
      srs->orq_agent->sa_graylist,
      maxprio + 1);

    if (modified >= 0)
      SU_DEBUG_3(("srs(%p): reduced priority of %d %s SRV records (increase value to %u)\n",
		  (void *)srs,
		  modified, step->sp_target, maxprio + 1));
    else
      SU_DEBUG_3(("srs(%p): failed to reduce %s SRV priority\n",
		  (void *)srs,
		  step->sp_target));
  }
}
#endif

/** Take next step in resolving process.
 *
 * @retval 1 if there is more steps left
 * @retval 0 if resolving process is complete
 */
int
sres_sip_next_step(sres_sip_t *srs)
{
  if (srs == NULL)
    return 0;

  for (;;) {
    sres_sip_process(srs); /* Process answers */

    if (sres_sip_is_waiting(srs))
      return 1;

    sres_sip_try_next_steps(srs);

    if (!sres_sip_send_steps(srs))
      break;
  }

  if (sres_sip_is_waiting(srs))
    return 1;

  if (!srs->srs_complete) {
    SU_DEBUG_5(("srs(%p): complete\n", (void *)srs));
    srs->srs_complete = 1;
  }

  assert(*srs->srs_process == NULL);
  assert(!srs->srs_try_naptr && !srs->srs_try_srv && !srs->srs_try_a);

  return 0;
}

static int
sres_sip_is_waiting(sres_sip_t const *srs)
{
  return *srs->srs_process && (*srs->srs_process)->sp_query;
}

static void
sres_sip_try_next_steps(sres_sip_t *srs)
{
  if (*srs->srs_send == NULL) {
    if (srs->srs_try_naptr) {
      sres_sip_try_naptr_steps(srs);
    }
    else if (srs->srs_try_srv) {
      sres_sip_try_srv_steps(srs);
    }
    else if (srs->srs_try_a) {
      sres_sip_try_a_aaaa_steps(srs);
    }
  }
}

/** Try target NAPTR. */
static void
sres_sip_try_naptr_steps(sres_sip_t *srs)
{
  struct srs_step *step;

  srs->srs_try_naptr = 0;
  step = sres_sip_step_new(srs, sres_type_naptr, NULL, srs->srs_target);
  sres_sip_append_step(srs, step);
}

/** If no NAPTR is found, try target SRVs. */
static void
sres_sip_try_srv_steps(sres_sip_t *srs)
{
  int i;
  char const *target = srs->srs_target;

  srs->srs_try_srv = 0;

  for (i = 1; srs->srs_hints[i].hint_stp; i++) {
    struct sres_sip_tport const *stp;
    struct srs_step *step;

    stp = srs->srs_hints[i].hint_stp;
    step = sres_sip_step_new(srs, sres_type_srv, stp->stp_prefix, target);

    if (step) {
      step->sp_hint = i;
      step->sp_prefer = i + 1;
      step->sp_priority = 1;
      step->sp_weight = 1;
      sres_sip_append_step(srs, step);
    }
  }
}

/** If not NAPTR or SRV was found, try target A/AAAAs.  */
static void
sres_sip_try_a_aaaa_steps(sres_sip_t *srs)
{
  int i;

  srs->srs_try_a = 0;

  for (i = 1; srs->srs_hints[i].hint_stp; i++) {
    struct srs_hint *hint = srs->srs_hints + i;
    struct sres_sip_tport const *stp = hint->hint_stp;
    struct srs_step *step;

    /* Consider only transport from uri parameter */
    if (!srs->srs_transport &&
	/* ...or default transports for this URI type */
	stp->stp_type != srs->srs_url->url_type)
      continue;

    step = sres_sip_step_new(srs, hint->hint_qtype, NULL, srs->srs_target);

    if (step) {
      step->sp_hint = i;
      step->sp_prefer = 2;
      step->sp_priority = i;
      step->sp_port = srs->srs_port ? srs->srs_port : stp->stp_port;
      sres_sip_append_step(srs, step);
    }
  }
}

/** Send queries or lookup from cache.
 *
 * Return true if got data to process.
 */
static int
sres_sip_send_steps(sres_sip_t *srs)
{
  int process = 0;

  /* New queries to send */
  while (*srs->srs_send) {
    struct srs_step *step = *srs->srs_send, *step2;
    uint16_t type = step->sp_type;
    char const *domain = step->sp_target;
    sres_record_t **answers;
    int parallel = 0;

    srs->srs_send = &step->sp_next;

    if (step->sp_status != STEP_QUEUED) {
      assert(step->sp_already != step);
      continue;
    }

    assert(step->sp_already == step);

    answers = sres_cached_answers(srs->srs_resolver, type, domain);

    for (step2 = step; step2; step2 = step2->sp_next) {
      if (step2->sp_already == step)
	step2->sp_status = answers ? STEP_CACHED : STEP_SENT;
    }


    SU_DEBUG_5(("srs(%p): query %s %s%s\n",
		(void *)srs, sres_record_type(type, NULL), domain,
		answers ? " (cached)" : ""));

    if (answers)
      ;
    else if (srs->srs_blocking) {
      sres_blocking_query(srs->srs_resolver, type, domain, 0, &answers);
    }
    else {
      step->sp_query = sres_query(srs->srs_resolver,
				  sres_sip_answer, step,
				  type, domain);
      if (step->sp_query) {
	/* Query all self-generated SRV records at the same time */
	parallel = step->sp_trace == NULL && type == sres_type_srv;
	if (!parallel)
	  break;
	continue;
      }
    }

    sres_sip_answer(step, NULL, answers);
    return process = 1;
  }

  return process = 0;
}

static void
sres_sip_answer(struct srs_step *step,
		sres_query_t *q,
		sres_record_t *answers[])
{
  sres_sip_t *srs = step->sp_srs;
  struct srs_step *step2;
  int status = sres_sip_status_of_answers(answers, step->sp_type);

  SU_DEBUG_5(("srs(%p): %s %s (answers=%u) for %s %s\n",
	      (void *)srs,
	      step->sp_status == STEP_SENT ? "received" : "cached",
	      sres_record_status(status, NULL),
	      sres_sip_count_answers(answers, step->sp_type),
	      sres_record_type(step->sp_type, NULL), step->sp_target));

  sres_sip_log_answers(srs, step, answers);

  assert(step->sp_already == step);
  assert(step->sp_query == q); step->sp_query = NULL;
  assert(step->sp_status == STEP_SENT ||
	 step->sp_status == STEP_CACHED);

  step->sp_results = answers;
  step->sp_status = status;

  /* Check if answer is valid for another query, too */
  for (step2 = step->sp_next; step2; step2 = step2->sp_next) {
    if (step2->sp_already == step) {
      step2->sp_results = answers;
      step2->sp_status = status;
    }
  }

  if (!sres_sip_next_step(srs)) {
    sres_sip_return_results(srs, 1);
  }
  else if (q && *srs->srs_next) {
    sres_sip_return_results(srs, 0);
  }
}

static int
sres_sip_status_of_answers(sres_record_t *answers[], uint16_t type)
{
  int i;

  if (answers == NULL)
    return SRES_NETWORK_ERR;

  for (i = 0; answers[i]; i++) {
    if (answers[i]->sr_record->r_type == type) {
      return answers[i]->sr_record->r_status;
    }
  }

  return SRES_RECORD_ERR;
}

static int
sres_sip_count_answers(sres_record_t *answers[], uint16_t type)
{
  int n;

  for (n = 0; answers && answers[n]; n++) {
    if (type != answers[n]->sr_record->r_type ||
	answers[n]->sr_record->r_status != 0)
      break;
  }

  return n;
}

static void
sres_sip_log_answers(sres_sip_t *srs,
		     struct srs_step *step,
		     sres_record_t *answers[])
{
  char const *previous;
  int i;

  if (answers == NULL)
    return;
  if (SU_LOG->log_level < 5)
    return;

  previous = step->sp_target;

  for (i = 0; answers[i]; i++) {
    sres_record_t *sr = answers[i];
    int type = sr->sr_record->r_type;
    char const *domain = sr->sr_record->r_name;
    char addr[SU_ADDRSIZE];

    if (sr->sr_record->r_status)
      continue;

    if (su_casematch(previous, domain))
      domain = "";
    else
      previous = domain;

    if (type == sres_type_a) {
      sres_a_record_t const *a = sr->sr_a;
      su_inet_ntop(AF_INET, &a->a_addr, addr, sizeof(addr));
      SU_DEBUG_5(("srs(%p): %s IN A %s\n", (void *)srs, domain, addr));
    }
    else if (type == sres_type_cname) {
      char const *cname = sr->sr_cname->cn_cname;
      SU_DEBUG_5(("srs(%p): %s IN CNAME %s\n", (void *)srs, domain, cname));
    }
#if SU_HAVE_IN6
    else if (type == sres_type_cname) {
      sres_aaaa_record_t const *aaaa = sr->sr_aaaa;
      su_inet_ntop(AF_INET6, &aaaa->aaaa_addr, addr, sizeof(addr));
      SU_DEBUG_5(("srs(%p): %s IN AAAA %s\n", (void *)srs, domain, addr));
    }
#endif
    else if (type == sres_type_naptr) {
      sres_naptr_record_t const *na = sr->sr_naptr;
      SU_DEBUG_5(("srs(%p): %s IN NAPTR %u %u \"%s\" \"%s\" \"%s\" %s\n",
		  (void *)srs,
		  domain,
		  na->na_order, na->na_prefer,
		  na->na_flags, na->na_services,
		  na->na_regexp, na->na_replace));
    }
    else if (type == sres_type_srv) {
      sres_srv_record_t const *srv = sr->sr_srv;
      SU_DEBUG_5(("srs(%p): %s IN SRV %u %u %u %s\n",
		  (void *)srs,
		  domain,
		  (unsigned)srv->srv_priority, (unsigned)srv->srv_weight,
		  srv->srv_port, srv->srv_target));
    }
    else {
      /* Ignore */
    }
  }
}




/** Return error response */
static void
sres_sip_return_results(sres_sip_t *srs, int final)
{
  if (final && srs->srs_results == NULL && !srs->srs_error) {
    /* Get best error */
    struct srs_step *step;

    for (step = srs->srs_head; step; step = step->sp_next) {
      switch (step->sp_status) {
      case SRES_NAME_ERR:
	if (su_casematch(step->sp_target, srs->srs_target))
	  srs->srs_error = SRES_SIP_ERR_NO_NAME;
	break;
      case SRES_OK:
      case SRES_RECORD_ERR:
	/* Something was found */
	srs->srs_error = SRES_SIP_ERR_NO_DATA;
	break;
      case SRES_AUTH_ERR:
      case SRES_FORMAT_ERR:
	srs->srs_error = SRES_SIP_ERR_FAIL;
	break;
      case SRES_SERVER_ERR:
      case SRES_TIMEOUT_ERR:
      case SRES_NETWORK_ERR:
	srs->srs_error = SRES_SIP_ERR_AGAIN;
	break;
      }
      if (srs->srs_error)
	break;
    }

    if (!srs->srs_error)
      srs->srs_error = SRES_SIP_ERR_FAIL;
  }

  if (srs->srs_callback) {
    sres_sip_notify_f *callback = srs->srs_callback;
    sres_sip_magic_t *magic = srs->srs_magic;

    if (final)
      srs->srs_callback = NULL, srs->srs_magic = NULL;

    callback(magic, srs, final);
  }
}

static int
sres_sip_process(sres_sip_t *srs)
{
  while (*srs->srs_process) {
    struct srs_step *step = *srs->srs_process;

    if (step->sp_query)
      return 1;

    if (step->sp_status < 0)
      break;

    if (srs->srs_process != srs->srs_send)
      srs->srs_process = &step->sp_next;
    else
      srs->srs_send = srs->srs_process = &step->sp_next;

    if (step->sp_status == SRES_RECORD_ERR) {
      sres_sip_process_cname(srs, step, step->sp_results);
      continue;
    }

    if (step->sp_status)
      continue;

    SU_DEBUG_5(("srs(%p): process %s %s record%s\n",
		(void *)srs,
		sres_record_type(step->sp_type, NULL), step->sp_target,
		step->sp_already != step ? " (again)" : ""));

    switch (step->sp_type) {
    case sres_type_naptr:
      sres_sip_process_naptr(srs, step, step->sp_results);
      break;
    case sres_type_srv:
      sres_sip_process_srv(srs, step, step->sp_results);
      break;
#if SU_HAVE_IN6
    case sres_type_aaaa:
      sres_sip_process_aaaa(srs, step, step->sp_results);
      break;
#endif
    case sres_type_a:
      sres_sip_process_a(srs, step, step->sp_results);
      break;

    default:
      assert(!"unknown query type");
    }
  }

  return 0;
}


static struct srs_step *
sres_sip_step_new(sres_sip_t *srs,
		  int type,
		  char const *prefix,
		  char const *domain)
{
  struct srs_step *step, *step0;
  size_t plen = prefix ? strlen(prefix) : 0;
  size_t extra = 0;

  for (step0 = srs->srs_head; step0; step0 = step0->sp_next) {
    if (step0->sp_type == type &&
	su_casenmatch(prefix, step0->sp_target, plen) &&
	su_casematch(step0->sp_target + plen, domain))
      break;
  }

  if (prefix && !step0)
    extra = plen + strlen(domain) + 1;

  step = su_zalloc(srs->srs_home, (sizeof *step) + extra);

  if (step) {
    step->sp_srs = srs;
    step->sp_type = type;
    step->sp_status = STEP_NEW;

    if (step0) {
      step->sp_already = step0;
      step->sp_results = step0->sp_results;
      step->sp_target = step0->sp_target;
    }
    else {
      step->sp_already = step;

      if (prefix) {
	char *target = (char *)(step + 1);
	step->sp_target = memcpy(target, prefix, plen);
	strcpy(target + plen, domain);
      }
      else {
	step->sp_target = domain;
      }
    }
  }

  return step;
}


/** Append a step to queue */
static void
sres_sip_append_step(sres_sip_t *srs,
		     struct srs_step *step)
{
  if (step == NULL)
    return;

  assert(step->sp_status == STEP_NEW);

  *srs->srs_queue = step, srs->srs_queue = &step->sp_next;

  if (step->sp_already == step) {
    step->sp_status = STEP_QUEUED;
  }
  else {
    step->sp_status = step->sp_already->sp_status;
    step->sp_results = step->sp_already->sp_results;
  }
}

/** Insert a step to queue.
 *
 * Sort by 1) order 2) priority/preference 3) weight.
 */
static void
sres_sip_insert_step(sres_sip_t *srs,
		     struct srs_step *step)
{
  struct srs_step *already, *step2, **insert, **at, **next;
  struct srs_hint *hint = &srs->srs_hints[step->sp_hint];
  struct sres_sip_tport const *stp = hint->hint_stp;
  int weight = 0, N = 0, by;

  /* Currently, inserted steps are results from CNAME or SRV.
     They have hint at this point */
  assert(step->sp_hint);

  step->sp_srs = srs;

  insert = srs->srs_send;

  for (at = insert; *at; at = next) {
    next = &(*at)->sp_next;

    if (step->sp_prefer < (*at)->sp_prefer) {
      break;
    }
    if (step->sp_prefer > (*at)->sp_prefer) {
      insert = next, weight = 0, N = 0;
      continue;
    }
    if (step->sp_priority < (*at)->sp_priority) {
      break;
    }
    if (step->sp_priority > (*at)->sp_priority) {
      insert = next, weight = 0, N = 0;
      continue;
    }

    weight += (*at)->sp_weight, N++;
  }

  if (step->sp_weight)
    weight += step->sp_weight;
  else
    insert = at;

  if (insert != at)
    by = su_randint(0, weight - 1);
  else
    by = weight;

  SU_DEBUG_5(("srs(%p): %s %s query for %s;transport=%s (N=%u %d/%d)\n",
	      (void *)srs,
	      insert != at ? "inserting" : "appending",
	      sres_record_type(step->sp_type, NULL), step->sp_target,
	      stp->stp_name,
	      N, by, weight));

  if (insert != at) {
    for (;by > step->sp_weight; insert = &(*insert)->sp_next) {
      assert(*insert); assert((*insert)->sp_prefer == step->sp_prefer);
      assert((*insert)->sp_priority == step->sp_priority);
      by -= (*insert)->sp_weight;
    }
  }

  step->sp_next = *insert, *insert = step;
  if (insert == srs->srs_queue)
    srs->srs_queue = &step->sp_next;

  step->sp_status = STEP_QUEUED;

  if (step->sp_already == step)
    return;

  /* Check if sp_already is after step */
  for (already = step->sp_next; already; already = already->sp_next) {
    if (already == step->sp_already)
      break;
  }

  if (already) {
    assert(already->sp_status == STEP_QUEUED);
    step->sp_already = step;
    for (step2 = step->sp_next; step2; step2 = step2->sp_next) {
      if (step2->sp_already == already)
	step2->sp_already = step;
    }
  }
  else {
    step->sp_status = step->sp_already->sp_status;
    step->sp_results = step->sp_already->sp_results;
  }
}

/* Process NAPTR records */
static void
sres_sip_process_naptr(sres_sip_t *srs,
		       struct srs_step *step,
		       sres_record_t *answers[])
{
  int i, j, order = -1, found = 0;

  assert(answers);

  /* Sort NAPTR first by order then by preference */
  /* sres_sort_answers() sorts with flags etc. too */
  sres_sip_sort_naptr(answers);

  for (i = 0; answers[i]; i++) {
    sres_naptr_record_t const *na = answers[i]->sr_naptr;
    struct sres_sip_tport const *stp;
    int supported = 0;

    if (na->na_record->r_status)
      continue;
    if (na->na_record->r_type != sres_type_naptr)
      continue;

    /* RFC 2915 p 4:
     * Order
     *    A 16-bit unsigned integer specifying the order in which the
     *    NAPTR records MUST be processed to ensure the correct ordering
     *    of rules. Low numbers are processed before high numbers, and
     *    once a NAPTR is found whose rule "matches" the target, the
     *    client MUST NOT consider any NAPTRs with a higher value for
     *    order (except as noted below for the Flags field).
     */
    if (order >= 0 && order != na->na_order)
      break;

    /* RFC3263 p. 6:
       First, a client resolving a SIPS URI MUST discard any services that
       do not contain "SIPS" as the protocol in the service field.  The
       converse is not true, however.  A client resolving a SIP URI SHOULD
       retain records with "SIPS" as the protocol, if the client supports
       TLS.
    */
    if (su_casenmatch(na->na_services, "SIPS+", 5) ||
	su_casenmatch(na->na_services, "SIP+", 4))
      /* Use NAPTR results, don't try extra SRV/A/AAAA records */
      srs->srs_try_srv = 0, srs->srs_try_a = 0, found = 1;
    else
      continue; /* Not a SIP/SIPS service */

    /* Check if we have a transport matching with service */
    for (j = 1; (stp = srs->srs_hints[j].hint_stp); j++) {
      /*
       * Syntax of services is actually more complicated
       * but comparing the values in the transport list
       * match with those values that make any sense
       */
      if (su_casematch(na->na_services, stp->stp_service)) {
	/* We found matching NAPTR */
	order = na->na_order;
	supported = 1;
	sres_sip_step_by_naptr(srs, step, j, na);
      }
    }

    SU_DEBUG_5(("srs(%p): %s IN NAPTR %u %u \"%s\" \"%s\" \"%s\" %s%s\n",
		(void *)srs,
		na->na_record->r_name,
		na->na_order, na->na_prefer,
		na->na_flags, na->na_services,
		na->na_regexp, na->na_replace,
		supported ? "" : " (not supported)"));
  }

  if (found && order < 0)
    /* There was a SIP/SIPS natpr, but it has no matching hints */
    srs->srs_error = SRES_SIP_ERR_NO_TPORT;
}

static void
sres_sip_sort_naptr(sres_record_t *answers[])
{
  int i, j;

  for (i = 0; answers[i]; i++) {
    sres_naptr_record_t const *na = answers[i]->sr_naptr;

    if (na->na_record->r_type != sres_type_naptr)
      break;

    for (j = 0; j < i; j++) {
      sres_naptr_record_t const *nb = answers[j]->sr_naptr;

      if (na->na_order < nb->na_order)
	break;
      if (na->na_order > nb->na_order)
	continue;
      if (na->na_prefer < nb->na_prefer)
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
}

static void
sres_sip_step_by_naptr(sres_sip_t *srs,
		       struct srs_step *step0,
		       uint16_t hint,
		       sres_naptr_record_t const *na)
{
  struct srs_step *step;
  uint16_t qtype;
  uint16_t port = srs->srs_hints[hint].hint_stp->stp_port;
  uint16_t hint_qtype = srs->srs_hints[hint].hint_qtype;

  if (na->na_flags[0] == 's' || na->na_flags[0] == 'S')
    /* "S" flag means that the next lookup should be for SRV records */
    qtype = sres_type_srv; /* SRV */
  else if (na->na_flags[0] == 'a' || na->na_flags[0] == 'A')
    /* "A" means that the next lookup should be A/AAAA */
    qtype = hint_qtype;
  else
    return;

  step = sres_sip_step_new(srs, qtype, NULL, na->na_replace);

  if (step == NULL)
    return;

  step->sp_trace = step0;
  step->sp_origin = (sres_record_t const *)na;
  step->sp_prefer = na->na_prefer;
  step->sp_priority = hint;
  step->sp_weight = 1;
  step->sp_hint = hint;
  step->sp_port = port;

  sres_sip_append_step(srs, step);
}

/* Process SRV records */
static void
sres_sip_process_srv(sres_sip_t *srs,
		     struct srs_step *step0,
		     sres_record_t *answers[])
{
  int i;
  struct srs_hint *hint = &srs->srs_hints[step0->sp_hint];

  sres_sip_sort_srv(answers); /* Sort by priority (only) */

  for (i = 0; answers[i]; i++) {
    sres_srv_record_t const *srv = answers[i]->sr_srv;
    struct srs_step *step;

    if (srv->srv_record->r_status /* There was an error */ ||
        srv->srv_record->r_type != sres_type_srv)
      continue;

    srs->srs_try_a = 0;

    step = sres_sip_step_new(srs, hint->hint_qtype, NULL, srv->srv_target);
    if (step) {
      step->sp_hint = step0->sp_hint;
      step->sp_trace = step0;
      step->sp_origin = (sres_record_t const *)srv;
      step->sp_port = srv->srv_port;
      step->sp_prefer = step0->sp_prefer;
      step->sp_priority = srv->srv_priority;
      step->sp_weight = srv->srv_weight;

      /* Insert sorted by priority, randomly select by weigth */
      sres_sip_insert_step(srs, step);
    }
  }
}

static void
sres_sip_sort_srv(sres_record_t *answers[])
{
  int i, j;

  for (i = 0; answers[i]; i++) {
    sres_srv_record_t const *a = answers[i]->sr_srv;

    if (a->srv_record->r_type != sres_type_srv)
      break;

    for (j = 0; j < i; j++) {
      sres_srv_record_t const *b = answers[j]->sr_srv;

      if (a->srv_priority < b->srv_priority)
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
}

#if SU_HAVE_IN6
/* Process AAAA records */
static void
sres_sip_process_aaaa(sres_sip_t *srs,
		      struct srs_step *step,
		      sres_record_t *answers[])
{
  struct srs_hint *hint = &srs->srs_hints[step->sp_hint];
  struct sres_sip_tport const *stp = hint->hint_stp;
  su_addrinfo_t ai[1];
  su_sockaddr_t su[1];
  size_t i, j;

  for (i = j = 0; answers && answers[i]; i++) {
    sres_aaaa_record_t const *aaaa = answers[i]->sr_aaaa;

    if (aaaa->aaaa_record->r_status ||
        aaaa->aaaa_record->r_type != sres_type_aaaa)
      continue;			      /* There was an error */

    memset(ai, 0, (sizeof ai));
    ai->ai_protocol = stp->stp_number;
    ai->ai_addr = memset(su, 0, ai->ai_addrlen = (sizeof su->su_sin6));
    su->su_len = (sizeof su->su_sin6);
    su->su_family = ai->ai_family = AF_INET6;
    su->su_port = htons(step->sp_port);

    memcpy(&su->su_sin6.sin6_addr,
	   &aaaa->aaaa_addr,
	   (sizeof aaaa->aaaa_addr));

    ai->ai_canonname = aaaa->aaaa_record->r_name;

    sres_sip_append_result(srs, ai);
  }
}
#endif /* SU_HAVE_IN6 */

/* Process A records */
static void
sres_sip_process_a(sres_sip_t *srs,
		   struct srs_step *step,
		   sres_record_t *answers[])
{
  struct srs_hint *hint = &srs->srs_hints[step->sp_hint];
  struct sres_sip_tport const *stp = hint->hint_stp;
  su_addrinfo_t ai[1];
  su_sockaddr_t su[1];
  int i, j;

  for (i = j = 0; answers[i]; i++) {
    sres_a_record_t const *a = answers[i]->sr_a;

    if (a->a_record->r_status ||
	a->a_record->r_type != sres_type_a)
      continue;			      /* There was an error */

    memset(ai, 0, (sizeof ai));
    ai->ai_protocol = stp->stp_number;
    ai->ai_addr = memset(su, 0, ai->ai_addrlen = (sizeof su->su_sin));
    su->su_len = (sizeof su->su_sin);
    su->su_family = ai->ai_family = AF_INET;
    su->su_port = htons(step->sp_port);

    memcpy(&su->su_sin.sin_addr, &a->a_addr, (sizeof a->a_addr));

    ai->ai_canonname = a->a_record->r_name;

    sres_sip_append_result(srs, ai);
  }
}

static void
sres_sip_process_cname(sres_sip_t *srs,
		       struct srs_step *step0,
		       sres_record_t *answers[])
{
  struct srs_step *step;
  int i;

  if (answers == NULL)
    return;

  for (i = 0; answers[i]; i++) {
    sres_cname_record_t *cn = answers[i]->sr_cname;

    if (cn->cn_record->r_type != sres_type_cname ||
	cn->cn_record->r_status != SRES_OK)
      continue;

    step = sres_sip_step_new(srs, step0->sp_type, NULL, cn->cn_cname);
    if (!step)
      return;

    step->sp_trace = step0;
    step->sp_origin = answers[i];

    step->sp_hint = step0->sp_hint;
    step->sp_port = step0->sp_port;
    step->sp_prefer = step0->sp_prefer;
    step->sp_priority = step0->sp_priority;
    step->sp_weight = step0->sp_weight;

    sres_sip_insert_step(srs, step);

    return;
  }
}


/* Process URI with numeric host */
static void
sres_sip_process_numeric(sres_sip_t *srs)
{
  char const *target = srs->srs_target;
  su_addrinfo_t ai[1];
  su_sockaddr_t su[1];
  char buffer[64];
  int i;

  memset(ai, 0, (sizeof ai)); (void)buffer;

  if (host_is_ip4_address(target)) {
    ai->ai_addr = memset(su, 0, ai->ai_addrlen = (sizeof su->su_sin));
    su->su_len = (sizeof su->su_sin);
    if (su_inet_pton(su->su_family = ai->ai_family = AF_INET,
		     target, &su->su_sin.sin_addr) <= 0) {
      srs->srs_error = SRES_SIP_ERR_BAD_URI;
      return;
    }
  }
#if SU_HAVE_IN6
  else if (host_is_ip6_address(target)) {
    ai->ai_addr = memset(su, 0, ai->ai_addrlen = (sizeof su->su_sin6));
    su->su_len = (sizeof su->su_sin6);
    if (su_inet_pton(su->su_family = ai->ai_family = AF_INET6,
		     target, &su->su_sin6.sin6_addr) <= 0) {
      srs->srs_error = SRES_SIP_ERR_BAD_URI;
      return;
    }
  }
  else if (host_is_ip6_reference(target)) {
    size_t len = strlen(target) - 2;

    ai->ai_addr = memset(su, 0, ai->ai_addrlen = (sizeof su->su_sin6));
    su->su_len = (sizeof su->su_sin6);

    if (len >= sizeof buffer ||
	!memcpy(buffer, target + 1, len) ||
	(buffer[len] = 0) ||
	su_inet_pton(su->su_family = ai->ai_family = AF_INET6,
		     buffer, &su->su_sin6.sin6_addr) <= 0) {
      srs->srs_error = SRES_SIP_ERR_BAD_URI;
      return;
    }

    target = buffer;
  }
#endif
  else {
    srs->srs_error = SRES_SIP_ERR_BAD_URI;
    return;
  }

  ai->ai_canonname = (char *)target;

  for (i = 1; srs->srs_hints[i].hint_stp; i++) {
    struct srs_hint const *hint = srs->srs_hints + i;

    /* Consider only transport from uri parameter */
    if (!srs->srs_transport &&
	/* ...or default transports for this URI type */
	hint->hint_stp->stp_type != srs->srs_url->url_type)
      continue;

#if SU_HAVE_IN6
    if (ai->ai_family == AF_INET6 &&
	srs->srs_hints[i].hint_qtype != sres_type_aaaa)
      continue;
#endif
    ai->ai_protocol = srs->srs_hints[i].hint_stp->stp_number;
    sres_sip_append_result(srs, ai);
  }
}

/** Store A/AAAA query result */
static void
sres_sip_append_result(sres_sip_t *srs,
		       su_addrinfo_t const *result)
{
  su_addrinfo_t *ai, **tail;
  int duplicate = 0;
  char const *canonname = result->ai_canonname;
  char numeric[64];
  size_t clen = 0;

  for (ai = srs->srs_results; ai && !duplicate; ai = ai->ai_next) {
    duplicate =
      ai->ai_family == result->ai_family &&
      ai->ai_protocol == result->ai_protocol &&
      ai->ai_addrlen == result->ai_addrlen &&
      !memcmp(ai->ai_addr, result->ai_addr, result->ai_addrlen);
    /* Note - different canonnames are not considered */
    if (duplicate)
      break;
  }

  {
    unsigned port = 0;
    char const *lb = "", *rb = "";

    if (result->ai_family == AF_INET) {
      struct sockaddr_in const *sin = (struct sockaddr_in *)result->ai_addr;
      su_inet_ntop(AF_INET, &sin->sin_addr, numeric, sizeof numeric);
      port = ntohs(sin->sin_port);
    }
#if SU_HAVE_IN6
    else if (result->ai_family == AF_INET6) {
      struct sockaddr_in6 const *sin6 = (struct sockaddr_in6 *)result->ai_addr;
      su_inet_ntop(AF_INET6, &sin6->sin6_addr, numeric, (sizeof numeric));
      port = ntohs(sin6->sin6_port);
      lb = "[", rb = "]";
    }
#endif
    else {
      strcpy(numeric, "UNKNOWN");
    }

    SU_DEBUG_5(("srs(%p): %s result %s%s%s:%u;transport=%s\n",
				(void *)srs, duplicate ? "duplicate" : "returning",
		lb , numeric, rb, port,
		sres_sip_transport_name(result->ai_protocol)));

    if (srs->srs_numeric)
      canonname = numeric;
  }

  if (duplicate)
    return;

  if (!srs->srs_canonname)
    canonname = NULL;
  if (canonname) {
    clen = strlen(canonname);
    if (clen && canonname[clen - 1] == '.')
      /* Do not include final dot in canonname */;
    else
      clen++;
  }

  ai = su_zalloc(srs->srs_home, (sizeof *ai) + result->ai_addrlen + clen);

  if (ai == NULL)
    return;

  *ai = *result;
  ai->ai_next = NULL;
  ai->ai_addr = memcpy(ai + 1, ai->ai_addr, ai->ai_addrlen);
  if (canonname) {
    ai->ai_canonname = (char *)(ai->ai_addr) + ai->ai_addrlen;
    memcpy(ai->ai_canonname, canonname, clen - 1);
    ai->ai_canonname[clen - 1] = '\0';
  }
  else {
    ai->ai_canonname = NULL;
  }

  for (tail = srs->srs_next; *tail; tail = &(*tail)->ai_next)
    ;

  *tail = ai;

  srs->srs_error = 0;
}
