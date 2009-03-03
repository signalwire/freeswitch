/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2006 Nokia Corporation.
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

/**@CFILE tport_test.c
 *
 * Test functions for transports
 *
 * @internal
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Apr  3 11:25:13 2002 ppessi
 */

/* always assert()s */
#undef NDEBUG

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <assert.h>

typedef struct tp_test_s tp_test_t;

#define TP_STACK_T tp_test_t
#define TP_CLIENT_T struct called

#include <sofia-sip/su_wait.h>
#include <sofia-sip/su_md5.h>

#include "tport_internal.h"	/* Get SU_DEBUG_*() */

#include "test_class.h"
#include "test_protos.h"
#include "sofia-sip/msg.h"
#include "sofia-sip/msg_mclass.h"
#include "sofia-sip/msg_addr.h"

#if HAVE_SIGCOMP
#include <sigcomp.h>
#endif

#include <sofia-sip/base64.h>

#include <sofia-sip/su_log.h>

#include "sofia-sip/tport.h"

struct tp_test_s {
  su_home_t  tt_home[1];
  int        tt_flags;
  su_root_t *tt_root;
  msg_mclass_t *tt_mclass;
  tport_t   *tt_srv_tports;
  tport_t   *tt_tports;

  tport_t   *tt_rtport;

  tp_name_t tt_udp_name[1];
  tp_name_t tt_udp_comp[1];

  tp_name_t tt_tcp_name[1];
  tp_name_t tt_tcp_comp[1];

  tp_name_t tt_sctp_name[1];
  tp_name_t tt_sctp_comp[1];

  tp_name_t tt_tls_name[1];
  tp_name_t tt_tls_comp[1];

#if HAVE_SIGCOMP
  struct sigcomp_state_handler *state_handler;
  struct sigcomp_algorithm const *algorithm;
  struct sigcomp_compartment *master_cc;

#define IF_SIGCOMP_TPTAG_COMPARTMENT(cc) TAG_IF(cc, TPTAG_COMPARTMENT(cc)),
#else
#define IF_SIGCOMP_TPTAG_COMPARTMENT(cc)
#endif

  int        tt_status;
  int        tt_received;
  msg_t     *tt_rmsg;
  uint8_t    tt_digest[SU_MD5_DIGEST_SIZE];

  su_addrinfo_t const *tt_tcp_addr;
  tport_t   *tt_tcp;
};

int tstflags;
#define TSTFLAGS tstflags

#include <sofia-sip/tstdef.h>

char const name[] = "tport_test";

SOFIAPUBVAR su_log_t tport_log[];

static int name_test(tp_test_t *tt)
{
  tp_name_t tpn[1];

  su_home_t home[1] = { SU_HOME_INIT(home) };

  su_sockaddr_t su[1];

  BEGIN();

  memset(su, 0, sizeof su);

  su->su_port = htons(5060);
  su->su_family = AF_INET;

  TEST(tport_convert_addr(home, tpn, "tcp", "localhost", su), 0);

  su->su_family = AF_INET;

  TEST(tport_convert_addr(home, tpn, "tcp", "localhost", su), 0);

#if SU_HAVE_IN6
  su->su_family = AF_INET6;
  TEST(tport_convert_addr(home, tpn, "tcp", "localhost", su), 0);
#endif

  END();
}

/* Count number of transports in chain */
static
int count_tports(tport_t *tp)
{
  int n = 0;

  for (tp = tport_primaries(tp); tp; tp = tport_next(tp))
    n++;

  return n;
}

static int check_msg(tp_test_t *tt, msg_t *msg, char const *ident)
{
  msg_test_t *tst;
  msg_payload_t *pl;
  usize_t i, len;

  BEGIN();

  TEST_1(tst = msg_test_public(msg));
  TEST_1(pl = tst->msg_payload);

  if (ident) {
    if (!tst->msg_content_location ||
	strcmp(ident, tst->msg_content_location->g_string))
      return 1;
  }

  len = pl->pl_len;

  for (i = 0; i < len; i++) {
    if (pl->pl_data[i] != (char) (i % 240))
      break;
  }

  if (pl)
  return i != len;

  END();
}

static int test_create_md5(tp_test_t *tt, msg_t *msg)
{
  msg_test_t *tst;
  msg_payload_t *pl;
  su_md5_t md5[1];

  BEGIN();

  TEST_1(tst = msg_test_public(msg));
  TEST_1(pl = tst->msg_payload);

  su_md5_init(md5);
  su_md5_update(md5, pl->pl_data, pl->pl_len);
  su_md5_digest(md5, tt->tt_digest);

  END();
}

static int test_check_md5(tp_test_t *tt, msg_t *msg)
{
  msg_test_t *tst;
  msg_payload_t *pl;
  su_md5_t md5[1];
  uint8_t digest[SU_MD5_DIGEST_SIZE];

  BEGIN();

  TEST_1(tst = msg_test_public(msg));
  TEST_1(pl = tst->msg_payload);

  su_md5_init(md5);
  su_md5_update(md5, pl->pl_data, pl->pl_len);
  su_md5_digest(md5, digest);

  TEST(memcmp(digest, tt->tt_digest, sizeof digest), 0);

  END();
}

static int test_msg_md5(tp_test_t *tt, msg_t *msg)
{
  msg_test_t *tst;

  BEGIN();

  TEST_1(tst = msg_test_public(msg));

  if (tst->msg_content_md5) {
    su_md5_t md5sum[1];
    uint8_t digest[SU_MD5_DIGEST_SIZE];
    char b64[BASE64_SIZE(SU_MD5_DIGEST_SIZE) + 1];

    msg_payload_t *pl =tst->msg_payload;

    su_md5_init(md5sum);
    su_md5_update(md5sum, pl->pl_data, pl->pl_len);
    su_md5_digest(md5sum, digest);

    base64_e(b64, sizeof(b64), digest, sizeof(digest));

    if (strcmp(b64, tst->msg_content_md5->g_string)) {
      ;
    }

    TEST_S(b64, tst->msg_content_md5->g_string);
  } else {
    TEST_1(tst->msg_content_md5);
  }

  END();
}

#define TPORT_TEST_VERSION MSG_TEST_VERSION_CURRENT

static int new_test_msg(tp_test_t *tt, msg_t **retval,
			char const *ident,
			int N, int len)
{
  msg_t *msg;
  msg_test_t *tst;
  su_home_t *home;
  msg_request_t *rq;
  msg_unknown_t *u;
  msg_content_location_t *cl;
  msg_content_md5_t *md5;
  msg_content_length_t *l;
  msg_separator_t *sep;
  msg_payload_t payload[1];
  msg_header_t *h;
  int i;

  su_md5_t md5sum[1];
  uint8_t digest[SU_MD5_DIGEST_SIZE];
  char b64[BASE64_SIZE(SU_MD5_DIGEST_SIZE) + 1];

  BEGIN();

  TEST_1(msg = msg_create(tt->tt_mclass, 0));
  TEST_1(tst = msg_test_public(msg));
  TEST_1(home = msg_home(msg));

  TEST_SIZE(msg_maxsize(msg, 1024 + N * len), 0);

  TEST_1(rq = msg_request_make(home, "DO im:foo@faa " TPORT_TEST_VERSION));
  TEST(msg_header_insert(msg, (void *)tst, (msg_header_t *)rq), 0);

  TEST_1(u = msg_unknown_make(home, "Foo: faa"));
  TEST(msg_header_insert(msg, (void *)tst, (msg_header_t *)u), 0);

  TEST_1(u = msg_unknown_make(home, "Foo: faa"));
  TEST(msg_header_insert(msg, (void *)tst, (msg_header_t *)u), 0);

  if (ident) {
    TEST_1(cl = msg_content_location_make(home, ident));
    TEST(msg_header_insert(msg, (void *)tst, (msg_header_t *)cl), 0);
  }

  msg_payload_init(payload);

  payload->pl_len = len;
  TEST_1(payload->pl_data = su_zalloc(home, payload->pl_len));

  for (i = 0; i < len; i++) {
    payload->pl_data[i] = (char) (i % 240);
  }

  su_md5_init(md5sum);

  for (i = 0; i < N; i++) {
    h = msg_header_dup(home, (msg_header_t*)payload);
    TEST_1(h);
    TEST(msg_header_insert(msg, (void *)tst, h), 0);
    su_md5_update(md5sum, payload->pl_data, payload->pl_len);
  }

  TEST_1(l = msg_content_length_format(home, MOD_ZU, (size_t)(N * payload->pl_len)));
  TEST(msg_header_insert(msg, (void *)tst, (msg_header_t *)l), 0);

  su_md5_digest(md5sum, digest);

  base64_e(b64, sizeof(b64), digest, sizeof(digest));

  TEST_1(md5 = msg_content_md5_make(home, b64));
  TEST(msg_header_insert(msg, (void *)tst, (msg_header_t *)md5), 0);

  TEST_1(sep = msg_separator_create(home));
  TEST(msg_header_insert(msg, (void *)tst, (msg_header_t *)sep), 0);

  TEST(msg_serialize(msg, (void *)tst), 0);

  *retval = msg;

  END();
}

static
struct sigcomp_compartment *
test_sigcomp_compartment(tp_test_t *tt, tport_t *tp, tp_name_t const *tpn);

static void tp_test_recv(tp_test_t *tt,
			 tport_t *tp,
			 msg_t *msg,
			 tp_magic_t *magic,
			 su_time_t now)
{
  tp_name_t frm[1];

  if (tport_delivered_from(tp, msg, frm) != -1 && frm->tpn_comp) {
    struct sigcomp_compartment *cc = test_sigcomp_compartment(tt, tp, frm);

    tport_sigcomp_accept(tp, cc, msg);
  }

  tt->tt_status = 1;
  tt->tt_received++;

  if (msg_has_error(msg)) {
    tt->tt_status = -1;
    tt->tt_rtport = tp;
  }
  else if (test_msg_md5(tt, msg))
    msg_destroy(msg);
  else if (tt->tt_rmsg)
    msg_destroy(msg);
  else {
    tt->tt_rmsg = msg;
    tt->tt_rtport = tp;
  }
}

static void tp_test_error(tp_test_t *tt,
			  tport_t *tp,
			  int errcode,
			  char const *remote)
{
  tt->tt_status = -1;
  fprintf(stderr, "tp_test_error(%p): error %d (%s) from %s\n",
	  (void *)tp, errcode, su_strerror(errcode),
	  remote ? remote : "<unknown destination>");
}

msg_t *tp_test_msg(tp_test_t *tt, int flags,
		   char const data[], usize_t size,
		   tport_t const *tp,
		   tp_client_t *tpc)
{
  msg_t *msg = msg_create(tt->tt_mclass, flags);

  msg_maxsize(msg, 2 * 1024 * 1024);

  return msg;
}


static
struct sigcomp_compartment *
test_sigcomp_compartment(tp_test_t *tt,
			 tport_t *tp,
			 tp_name_t const *tpn)
{
  struct sigcomp_compartment *cc = NULL;
#if HAVE_SIGCOMP
  char name[256];
  int namesize;

  namesize = snprintf(name, sizeof name, "TEST_%s/%s:%s",
		     tpn->tpn_proto,
		     tpn->tpn_host,
		     tpn->tpn_port);

  if (namesize <= 0 || namesize >= sizeof name)
    return NULL;

  cc = sigcomp_compartment_access(tt->state_handler,
				  0, name, namesize, NULL, 0);

  if (cc == NULL) {
    cc = sigcomp_compartment_create(tt->algorithm, tt->state_handler,
				    0, name, namesize, NULL, 0);

    sigcomp_compartment_option(cc, "dms=32768");
  }
#endif

  return cc;
}

/* Accept/reject early SigComp message */
int test_sigcomp_accept(tp_stack_t *tt, tport_t *tp, msg_t *msg)
{
  struct sigcomp_compartment *cc = NULL;

  cc = test_sigcomp_compartment(tt, tp, tport_name(tp));

  if (cc)
    tport_sigcomp_assign(tp, cc);

  return tport_sigcomp_accept(tp, cc, msg);
}


tp_stack_class_t const tp_test_class[1] =
  {{
      /* tpac_size */ sizeof(tp_test_class),
      /* tpac_recv */  tp_test_recv,
      /* tpac_error */ tp_test_error,
      /* tpac_alloc */ tp_test_msg,
  }};

static int init_test(tp_test_t *tt)
{
  tp_name_t myname[1] = {{ "*", "*", "*", "*", "sigcomp" }};
#if HAVE_SCTP
  char const * transports[] = { "udp", "tcp", "sctp", NULL };
#else
  char const * transports[] = { "udp", "tcp", NULL };
#endif
  tp_name_t const *tpn;
  tport_t *tp;
  unsigned idle;
  int logging = -1;

  BEGIN();

  int mask = AI_PASSIVE | AI_CANONNAME | AI_NUMERICHOST;

#ifdef AI_ALL
  mask |= AI_ALL;
#endif
#ifdef AI_V4MAPPED_CFG
  mask |= AI_V4MAPPED_CFG;
#endif
#ifdef AI_ADDRCONFIG
  mask |= AI_ADDRCONFIG;
#endif
#ifdef AI_V4MAPPED
  mask |= AI_V4MAPPED;
#endif

  /* Test that we have no common flags with underlying getaddrinfo() */
  TEST(mask & TP_AI_MASK, 0);

  TEST_1(tt->tt_root = su_root_create(NULL));

  myname->tpn_host = "127.0.0.1";
  myname->tpn_ident = "client";

  /* Create message class */
  TEST_1(tt->tt_mclass = msg_mclass_clone(msg_test_mclass, 0, 0));

  /* Try to insert Content-Length header (expecting failure) */
  TEST(msg_mclass_insert(tt->tt_mclass, msg_content_length_href), -1);

#if HAVE_SIGCOMP
  TEST_1(tt->state_handler = sigcomp_state_handler_create());
  TEST_1(tt->algorithm =
	 sigcomp_algorithm_by_name(getenv("SIGCOMP_ALGORITHM")));
  TEST_1(tt->master_cc =
	 sigcomp_compartment_create(tt->algorithm, tt->state_handler,
				    0, "", 0, NULL, 0));
  TEST(sigcomp_compartment_option(tt->master_cc, "stateless"), 1);
#endif

  /* Create client transport */
  TEST_1(tt->tt_tports =
	 tport_tcreate(tt, tp_test_class, tt->tt_root,
		       IF_SIGCOMP_TPTAG_COMPARTMENT(tt->master_cc)
		       TAG_END()));

  /* Bind client transports */
  TEST(tport_tbind(tt->tt_tports, myname, transports,
		   TPTAG_SERVER(0), TAG_END()),
       0);

  if (getenv("TPORT_TEST_HOST"))
    myname->tpn_host = getenv("TPORT_TEST_HOST");
  else
    myname->tpn_host = "*";

  if (getenv("TPORT_TEST_PORT"))
    myname->tpn_port = getenv("TPORT_TEST_PORT");

  myname->tpn_ident = "server";

  /* Create server transport */
  TEST_1(tt->tt_srv_tports =
	 tport_tcreate(tt, tp_test_class, tt->tt_root,
		       IF_SIGCOMP_TPTAG_COMPARTMENT(tt->master_cc)
		       TAG_END()));

  /* Bind server transports */
  TEST(tport_tbind(tt->tt_srv_tports, myname, transports,
		   TPTAG_SERVER(1),
		   TAG_END()),
       0);

  /* Check that the master transport has idle parameter */
  TEST(tport_get_params(tt->tt_srv_tports,
			TPTAG_IDLE_REF(idle),
			TAG_END()), 1);

  /* Check that logging tag works */
  TEST(tport_get_params(tt->tt_srv_tports,
			TPTAG_LOG_REF(logging),
			TAG_END()), 1);
  TEST(tport_set_params(tt->tt_srv_tports,
			TPTAG_LOG(logging),
			TAG_END()), 1);


  for (tp = tport_primaries(tt->tt_srv_tports); tp; tp = tport_next(tp))
    TEST_S(tport_name(tp)->tpn_ident, "server");

  {
    su_sockaddr_t su[1];
    socklen_t sulen;
    int s;
    int i, before, after;
    char port[8];

    tp_name_t rname[1];

    *rname = *myname;

    /* Check that we cannot bind to an already used socket */

    memset(su, 0, sulen = sizeof(su->su_sin));
    s = su_socket(su->su_family = AF_INET, SOCK_STREAM, 0); TEST_1(s != -1);
    TEST_1(bind(s, &su->su_sa, sulen) != -1);
    TEST_1(listen(s, 5) != -1);
    TEST_1(getsockname(s, &su->su_sa, &sulen) != -1);

    sprintf(port, "%u", ntohs(su->su_port));

    rname->tpn_port = port;
    rname->tpn_ident = "failure";

    before = count_tports(tt->tt_srv_tports);

    /* Bind server transports to an reserved port - this should fail */
    TEST(tport_tbind(tt->tt_srv_tports, rname, transports,
		     TPTAG_SERVER(1),
		     TAG_END()),
	 -1);

    after = count_tports(tt->tt_srv_tports);

    /* Check that no new primary transports has been added by failed call */
    TEST(before, after);

    /* Add new transports to an ephemeral port with new identity */

    for (tp = tport_primaries(tt->tt_srv_tports); tp; tp = tport_next(tp))
      TEST_S(tport_name(tp)->tpn_ident, "server");

    rname->tpn_port = "*";
    rname->tpn_ident = "server2";

    /* Bind server transports to another port */
    TEST(tport_tbind(tt->tt_srv_tports, rname, transports,
		     TPTAG_SERVER(1),
		     TAG_END()),
	 0);

    /* Check that new transports are after old ones. */
    for (i = 0, tp = tport_primaries(tt->tt_srv_tports);
	 i < before;
	 i++, tp = tport_next(tp))
      TEST_S(tport_name(tp)->tpn_ident, "server");

    for (; tp; tp = tport_next(tp))
      TEST_S(tport_name(tp)->tpn_ident, "server2");
  }

#if HAVE_TLS
  {
    tp_name_t tlsname[1] = {{ "tls", "*", "*", "*", NULL }};
    char const * transports[] = { "tls", NULL };

    char const *srcdir = getenv("srcdir");

    if (srcdir == NULL)
      srcdir = ".";

    tlsname->tpn_host = myname->tpn_host;
    tlsname->tpn_ident = "server";

    /* Bind client transports */
    TEST(tport_tbind(tt->tt_tports, tlsname, transports,
		     TPTAG_SERVER(0),
		     TPTAG_CERTIFICATE(srcdir),
		     TAG_END()),
	 0);

    /* Bind tls server transport */
    TEST(tport_tbind(tt->tt_srv_tports, tlsname, transports,
		     TPTAG_SERVER(1),
		     TPTAG_CERTIFICATE(srcdir),
		     TAG_END()),
	 0);
  }
#endif

  for (tp = tport_primaries(tt->tt_srv_tports); tp; tp = tport_next(tp)) {
    TEST_1(tpn = tport_name(tp));

    if (tt->tt_flags & tst_verbatim) {
      char const *host = tpn->tpn_host != tpn->tpn_canon ? tpn->tpn_host : "";
      printf("bound transport to %s/%s:%s%s%s%s%s\n",
	     tpn->tpn_proto, tpn->tpn_canon, tpn->tpn_port,
	     host[0] ? ";maddr=" : "", host,
	     tpn->tpn_comp ? ";comp=" : "",
	     tpn->tpn_comp ? tpn->tpn_comp : "");
    }

    /* Ignore server2 tports for now */
    if (strcmp(tpn->tpn_ident, "server"))
      continue;

    if (strcmp(tpn->tpn_proto, "udp") == 0) {
      *tt->tt_udp_name = *tpn;
      tt->tt_udp_name->tpn_comp = NULL;
      tt->tt_udp_name->tpn_ident = NULL;
      *tt->tt_udp_comp = *tpn;
      tt->tt_udp_comp->tpn_ident = NULL;
    }
    else if (strcmp(tpn->tpn_proto, "tcp") == 0) {
      *tt->tt_tcp_name = *tpn;
      tt->tt_tcp_name->tpn_comp = NULL;
      tt->tt_tcp_name->tpn_ident = NULL;
      *tt->tt_tcp_comp = *tpn;
      tt->tt_tcp_comp->tpn_ident = NULL;

      if (tt->tt_tcp_addr == NULL) {
	tt->tt_tcp_addr = tport_get_address(tp);
	tt->tt_tcp = tp;
      }
    }
    else if (strcmp(tpn->tpn_proto, "sctp") == 0) {
      *tt->tt_sctp_name = *tpn;
      tt->tt_sctp_name->tpn_ident = NULL;
    }
    else if (strcmp(tpn->tpn_proto, "tls") == 0) {
      *tt->tt_tls_name = *tpn;
      tt->tt_tls_name->tpn_ident = NULL;
    }
  }

  END();
}

char const payload[] =
"Some data\n"
"More data\n";

#include <time.h>

int
tport_test_run(tp_test_t *tt, unsigned timeout)
{
  time_t now = time(NULL);

  tt->tt_status = 0;

  msg_destroy(tt->tt_rmsg), tt->tt_rmsg = NULL;
  tt->tt_rtport = NULL;

  while (!tt->tt_status) {
    if (tt->tt_flags & tst_verbatim) {
      fputs(".", stdout); fflush(stdout);
    }
    su_root_step(tt->tt_root, 500L);

    if (!getenv("TPORT_TEST_DEBUG") &&
	time(NULL) > (time_t)(now + timeout))
      return 0;
  }

  return tt->tt_status;
}

static int udp_test(tp_test_t *tt)
{
  tport_t *tp;
  msg_t *msg;
  msg_test_t *tst;
  su_home_t *home;
  msg_request_t *rq;
  msg_unknown_t *u;
  msg_content_length_t *l;
  msg_content_md5_t *md5;
  msg_separator_t *sep;
  msg_payload_t *pl;

  BEGIN();

  TEST_1(msg = msg_create(tt->tt_mclass, 0));
  TEST_1(tst = msg_test_public(msg));
  TEST_1(home = msg_home(msg));

  TEST_1(rq = msg_request_make(home, "DO im:foo@faa " TPORT_TEST_VERSION));
  TEST(msg_header_insert(msg, (void *)tst, (msg_header_t *)rq), 0);

  TEST_1(u = msg_unknown_make(home, "Foo: faa"));
  TEST(msg_header_insert(msg, (void *)tst, (msg_header_t *)u), 0);

  TEST_1(pl = msg_payload_make(home, payload));
  TEST(msg_header_insert(msg, (void *)tst, (msg_header_t *)pl), 0);

  TEST_1(l = msg_content_length_format(home, MOD_ZU, (size_t)pl->pl_len));
  TEST(msg_header_insert(msg, (void *)tst, (msg_header_t *)l), 0);

  TEST_1(md5 = msg_content_md5_make(home, "R6nitdrtJFpxYzrPaSXfrA=="));
  TEST(msg_header_insert(msg, (void *)tst, (msg_header_t *)md5), 0);

  TEST_1(sep = msg_separator_create(home));
  TEST(msg_header_insert(msg, (void *)tst, (msg_header_t *)sep), 0);

  TEST(msg_serialize(msg, (void *)tst), 0);

  TEST_1(tp = tport_tsend(tt->tt_tports, msg, tt->tt_udp_name, TAG_END()));

  TEST_S(tport_name(tp)->tpn_ident, "client");

  TEST(tport_test_run(tt, 5), 1);

  msg_destroy(msg);

#if 0
  tp_name_t tpn[1] = {{ NULL }};

  TEST_1(msg = tt->tt_rmsg); tt->tt_rmsg = NULL;

  TEST_1(home = msg_home(msg));

  TEST_1(tport_convert_addr(home, tpn, "udp", NULL, msg_addr(msg)) == 0);

  tpn->tpn_comp = tport_name(tt->tt_rtport)->tpn_comp;

  /* reply */
  TEST_1(tport_tsend(tt->tt_rtport, msg, tpn, TAG_END()) != NULL);

  msg_destroy(msg);

  TEST(tport_test_run(tt, 5), 1);

  msg_destroy(msg);
#endif

  msg_destroy(tt->tt_rmsg), tt->tt_rmsg = NULL;

  END();
}

int pending_server_close, pending_client_close;

void server_closed_callback(tp_stack_t *tt, tp_client_t *client,
			    tport_t *tp, msg_t *msg, int error)
{
  assert(msg == NULL);
  assert(client == NULL);
  if (msg == NULL) {
    tport_release(tp, pending_server_close, NULL, NULL, client, 0);
    pending_server_close = 0;
  }
}

void client_closed_callback(tp_stack_t *tt, tp_client_t *client,
			    tport_t *tp, msg_t *msg, int error)
{
  assert(msg == NULL);
  assert(client == NULL);
  if (msg == NULL) {
    tport_release(tp, pending_client_close, NULL, NULL, client, 0);
    pending_client_close = 0;
  }
}

static int tcp_test(tp_test_t *tt)
{
  BEGIN();

  msg_t *msg = NULL;
  int i, N;
  tport_t *tp, *tp0;
  char ident[16];
  su_time_t started;

  /* Send a single message */
  TEST_1(!new_test_msg(tt, &msg, "tcp-first", 1, 1024));
  TEST_1(tp = tport_tsend(tt->tt_tports, msg, tt->tt_tcp_name, TAG_END()));
  TEST_S(tport_name(tp)->tpn_ident, "client");
  tp0 = tport_incref(tp);
  msg_destroy(msg);

  tport_set_params(tp,
       	    TPTAG_KEEPALIVE(100),
       	    TPTAG_PINGPONG(500),
       	    TPTAG_IDLE(500),
       	    TAG_END());

  TEST(tport_test_run(tt, 5), 1);
  TEST_1(!check_msg(tt, tt->tt_rmsg, "tcp-first"));
  msg_destroy(tt->tt_rmsg), tt->tt_rmsg = NULL;

  /* Ask for notification upon close */
  pending_client_close = tport_pend(tp0, NULL, client_closed_callback, NULL);
  TEST_1(pending_client_close > 0);
  tp = tt->tt_rtport;
  pending_server_close = tport_pend(tp, NULL, server_closed_callback, NULL);
  TEST_1(pending_server_close > 0);

  N = 0; tt->tt_received = 0;

#ifndef WIN32			/* Windows seems to be buffering too much */

  /* Create a large message, just to force queueing in sending end */
  TEST(new_test_msg(tt, &msg, "tcp-0", 1, 16 * 64 * 1024), 0);
  test_create_md5(tt, msg);
  TEST_1(tp = tport_tsend(tt->tt_tports, msg, tt->tt_tcp_name, TAG_END()));
  N++;
  TEST_S(tport_name(tp)->tpn_ident, "client");
  TEST_P(tport_incref(tp), tp0); tport_decref(&tp);
  msg_destroy(msg);

  /* Fill up the queue */
  for (i = 1; i < TPORT_QUEUESIZE; i++) {
    snprintf(ident, sizeof ident, "tcp-%u", i);

    TEST(new_test_msg(tt, &msg, ident, 1, 64 * 1024), 0);
    TEST_1(tp = tport_tsend(tt->tt_tports, msg, tt->tt_tcp_name, TAG_END()));
    N++;
    TEST_S(tport_name(tp)->tpn_ident, "client");
    TEST_P(tport_incref(tp), tp0); tport_decref(&tp);
    msg_destroy(msg);
  }

  /* This overflows the queue */
  TEST(new_test_msg(tt, &msg, "tcp-overflow", 1, 1024), 0);
  TEST_1(!tport_tsend(tt->tt_tports, msg, tt->tt_tcp_name, TAG_END()));
  msg_destroy(msg);

  TEST(tport_test_run(tt, 60), 1);
  TEST_1(!check_msg(tt, tt->tt_rmsg, "tcp-0"));
  test_check_md5(tt, tt->tt_rmsg);
  msg_destroy(tt->tt_rmsg), tt->tt_rmsg = NULL;

  if (tt->tt_received < TPORT_QUEUESIZE) { /* We have not received it all */
    snprintf(ident, sizeof ident, "tcp-%u", tt->tt_received);
    TEST(tport_test_run(tt, 5), 1);
    TEST_1(!check_msg(tt, tt->tt_rmsg, ident));
    msg_destroy(tt->tt_rmsg), tt->tt_rmsg = NULL;
  }
#else
 (void)i; (void)ident;
#endif

  /* This uses a new connection */
  TEST_1(!new_test_msg(tt, &msg, "tcp-no-reuse", 1, 1024));
  TEST_1(tp = tport_tsend(tt->tt_tports, msg, tt->tt_tcp_name,
       		   TPTAG_REUSE(0), TAG_END()));
  N++;
  TEST_S(tport_name(tp)->tpn_ident, "client");
  TEST_1(tport_incref(tp) != tp0); tport_decref(&tp);
  msg_destroy(msg);

  /* This uses the old connection */
  TEST_1(!new_test_msg(tt, &msg, "tcp-reuse", 1, 1024));
  TEST_1(tp = tport_tsend(tt->tt_tports, msg, tt->tt_tcp_name,
       		   TPTAG_REUSE(1), TAG_END()));
  N++;
  TEST_S(tport_name(tp)->tpn_ident, "client");
  TEST_1(tport_incref(tp) == tp0); tport_decref(&tp);
  msg_destroy(msg);

  /* Receive every message from queue */
  while (tt->tt_received < N) {
    TEST(tport_test_run(tt, 5), 1);
    /* Validate message */
    TEST_1(!check_msg(tt, tt->tt_rmsg, NULL));
    msg_destroy(tt->tt_rmsg), tt->tt_rmsg = NULL;
  }

  /* Try to send a single message */
  TEST_1(!new_test_msg(tt, &msg, "tcp-last", 1, 1024));
  TEST_1(tp = tport_tsend(tt->tt_tports, msg, tt->tt_tcp_name, TAG_END()));
  TEST_S(tport_name(tp)->tpn_ident, "client");
  TEST_P(tport_incref(tp), tp0); tport_decref(&tp);
  msg_destroy(msg);

  TEST(tport_test_run(tt, 5), 1);

  TEST_1(!check_msg(tt, tt->tt_rmsg, "tcp-last"));
  msg_destroy(tt->tt_rmsg), tt->tt_rmsg = NULL;

  TEST_1(pending_server_close && pending_client_close);
  SU_DEBUG_3(("tport_test(%p): waiting for PONG timeout\n", (void *)tp0));

  /* Wait until notifications -
     client closes when no pong is received and notifys pending,
     then server closes and notifys pending */
  while (pending_server_close || pending_client_close)
    su_root_step(tt->tt_root, 50);

  tport_decref(&tp0);

  /* Again a single message */
  TEST_1(!new_test_msg(tt, &msg, "tcp-pingpong", 1, 512));
  TEST_1(tp = tport_tsend(tt->tt_tports, msg, tt->tt_tcp_name, TAG_END()));
  TEST_S(tport_name(tp)->tpn_ident, "client");
  tp0 = tport_incref(tp);
  msg_destroy(msg);

  tport_set_params(tp0,
       	    TPTAG_KEEPALIVE(250),
       	    TPTAG_PINGPONG(200),
       	    TAG_END());

  TEST(tport_test_run(tt, 5), 1);
  TEST_1(!check_msg(tt, tt->tt_rmsg, "tcp-pingpong"));
  msg_destroy(tt->tt_rmsg), tt->tt_rmsg = NULL;

  /* Ask for notifications upon close */
  pending_client_close = tport_pend(tp0, NULL, client_closed_callback, NULL);
  TEST_1(pending_client_close > 0);

  tp = tt->tt_rtport;
  pending_server_close = tport_pend(tp, NULL, server_closed_callback, NULL);
  TEST_1(pending_server_close > 0);

  /* Now server responds with pong ... */
  TEST(tport_set_params(tp, TPTAG_PONG2PING(1), TAG_END()), 1);

  started = su_now();

  while (pending_server_close && pending_client_close) {
    su_root_step(tt->tt_root, 50);
    if (su_duration(su_now(), started) > 1000)
      break;
  }

  /* ... and we are still pending after a second */
  TEST_1(pending_client_close && pending_server_close);
  TEST_1(su_duration(su_now(), started) > 1000);

  tport_shutdown(tp0, 2);
  tport_unref(tp0);

  while (pending_server_close || pending_client_close)
    su_root_step(tt->tt_root, 50);

  END();
}

static int test_incomplete(tp_test_t *tt)
{
  BEGIN();

  su_addrinfo_t const *ai = tt->tt_tcp_addr;
  su_socket_t s;
  int connected;

  TEST_1(ai != NULL);

  TEST(tport_set_params(tt->tt_tcp, TPTAG_TIMEOUT(500), TAG_END()), 1);

  s = su_socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
  TEST_1(s != SOCKET_ERROR);

  su_setblocking(s, 1);
  connected = connect(s, ai->ai_addr, (socklen_t)ai->ai_addrlen);

  su_root_step(tt->tt_root, 50);

  TEST(su_send(s, "F", 1, 0), 1);
  su_root_step(tt->tt_root, 50);
  TEST(su_send(s, "O", 1, 0), 1);
  su_root_step(tt->tt_root, 50);
  TEST(su_send(s, "O", 1, 0), 1);
  su_root_step(tt->tt_root, 50);
  TEST(su_send(s, " ", 1, 0), 1);
  su_root_step(tt->tt_root, 50);

  tt->tt_received = 0;
  TEST(tport_test_run(tt, 5), -1);
  TEST(tt->tt_received, 1);
  TEST_P(tt->tt_rmsg, NULL);

  su_close(s);

  END();
}

static int reuse_test(tp_test_t *tt)
{
  msg_t *msg = NULL;
  int i, reuse = -1;
  tport_t *tp, *tp0, *tp1;
  tp_name_t tpn[1];

  BEGIN();

  /* Flush existing connections */
  *tpn = *tt->tt_tcp_name;
  tpn->tpn_port = "*";
  TEST_1(tp = tport_by_name(tt->tt_tports, tpn));
  TEST_1(tport_is_primary(tp));
  TEST(tport_flush(tp), 0);

  for (i = 0; i < 10; i++)
    su_root_step(tt->tt_root, 10L);

  TEST(tport_set_params(tp, TPTAG_REUSE(0), TAG_END()), 1);

  /* Send two messages */
  TEST(new_test_msg(tt, &msg, "reuse-1", 1, 1024), 0);
  TEST_1(tp = tport_tsend(tt->tt_tports, msg, tt->tt_tcp_name, TAG_END()));
  TEST_S(tport_name(tp)->tpn_ident, "client");
  TEST_1(tp0 = tport_incref(tp));
  TEST(tport_get_params(tp, TPTAG_REUSE_REF(reuse), TAG_END()), 1);
  TEST(reuse, 0);
  msg_destroy(msg), msg = NULL;

  TEST(new_test_msg(tt, &msg, "reuse-2", 1, 1024), 0);
  TEST_1(tp = tport_tsend(tt->tt_tports, msg, tt->tt_tcp_name, TAG_END()));
  TEST_S(tport_name(tp)->tpn_ident, "client");
  TEST_1(tp1 = tport_incref(tp)); TEST_1(tp0 != tp1);
  TEST(tport_get_params(tp, TPTAG_REUSE_REF(reuse), TAG_END()), 1);
  TEST(reuse, 0);
  msg_destroy(msg), msg = NULL;

  /* Receive every message from queue */
  for (tt->tt_received = 0;
       tt->tt_received < 2;) {
    TEST(tport_test_run(tt, 5), 1);
    msg_destroy(tt->tt_rmsg), tt->tt_rmsg = NULL;
  }

  /* Enable reuse on single connection */
  TEST(tport_set_params(tp1, TPTAG_REUSE(1), TAG_END()), 1);
  TEST(new_test_msg(tt, &msg, "reuse-3", 1, 1024), 0);
  TEST_1(tp = tport_tsend(tt->tt_tports, msg, tt->tt_tcp_name,
			  TPTAG_REUSE(1),
			  TAG_END()));
  TEST_S(tport_name(tp)->tpn_ident, "client");
  TEST_1(tp1 == tp);
  TEST(tport_get_params(tp, TPTAG_REUSE_REF(reuse), TAG_END()), 1);
  TEST(reuse, 1);
  msg_destroy(msg), msg = NULL;

  TEST(tport_test_run(tt, 5), 1);
  msg_destroy(tt->tt_rmsg), tt->tt_rmsg = NULL;

  TEST_1(tp = tport_by_name(tt->tt_tports, tpn));
  TEST_1(tport_is_primary(tp));
  TEST(tport_set_params(tp, TPTAG_REUSE(1), TAG_END()), 1);

  /* Send a single message with different connection */
  TEST_1(!new_test_msg(tt, &msg, "fresh-1", 1, 1024));
  TEST_1(tp = tport_tsend(tt->tt_tports, msg, tt->tt_tcp_name,
			  TPTAG_FRESH(1),
			  TPTAG_REUSE(1),
			  TAG_END()));
  TEST_S(tport_name(tp)->tpn_ident, "client");
  TEST_1(tport_incref(tp) != tp1);  tport_decref(&tp);
  msg_destroy(msg);

  TEST(tport_test_run(tt, 5), 1);

  TEST_1(!check_msg(tt, tt->tt_rmsg, "fresh-1"));
  msg_destroy(tt->tt_rmsg), tt->tt_rmsg = NULL;

  TEST_1(tport_shutdown(tp0, 2) >= 0);
  TEST_1(tport_shutdown(tp1, 2) >= 0);
  TEST_1(tport_shutdown(tp0, 1) >= 0);

  TEST(tport_shutdown(NULL, 0), -1);

  tport_decref(&tp0);
  tport_decref(&tp1);

  END();
}

static int sctp_test(tp_test_t *tt)
{
  BEGIN();

  msg_t *msg = NULL;
  int i, n;
  tport_t *tp, *tp0;
  char buffer[32];

  if (!tt->tt_sctp_name->tpn_proto)
    return 0;

  /* Just a small and nice message first */
  TEST_1(!new_test_msg(tt, &msg, "cid:sctp-first", 1, 1024));
  test_create_md5(tt, msg);
  TEST_1(tp = tport_tsend(tt->tt_tports, msg, tt->tt_sctp_name, TAG_END()));
  TEST_S(tport_name(tp)->tpn_ident, "client");
  msg_destroy(msg);

  tport_set_params(tp, TPTAG_KEEPALIVE(100), TPTAG_IDLE(500), TAG_END());

  TEST(tport_test_run(tt, 5), 1);

  TEST_1(!check_msg(tt, tt->tt_rmsg, NULL));
  test_check_md5(tt, tt->tt_rmsg);
  msg_destroy(tt->tt_rmsg), tt->tt_rmsg = NULL;

  tp0 = tport_ref(tp);

  pending_server_close = pending_client_close = 0;

  /* Ask for notification upon close */
  pending_client_close = tport_pend(tp, NULL, client_closed_callback, NULL);
  TEST_1(pending_client_close > 0);
  tp = tt->tt_rtport;
  pending_server_close = tport_pend(tp, NULL, server_closed_callback, NULL);
  TEST_1(pending_server_close > 0);

  if (0) { /* SCTP does not work reliably. Really. */

  tt->tt_received = 0;

  /* Create large messages, just to force queueing in sending end */
  for (n = 0; !tport_queuelen(tp); n++) {
    snprintf(buffer, sizeof buffer, "cid:sctp-%u", n);
    TEST_1(!new_test_msg(tt, &msg, buffer, 1, 32000));
    test_create_md5(tt, msg);
    TEST_1(tp = tport_tsend(tp0, msg, tt->tt_sctp_name, TAG_END()));
    TEST_S(tport_name(tp)->tpn_ident, "client");
    msg_destroy(msg);
  }

  /* Fill up the queue */
  for (i = 1; i < TPORT_QUEUESIZE; i++) {
    snprintf(buffer, sizeof buffer, "cid:sctp-%u", n + i);
    TEST_1(!new_test_msg(tt, &msg, buffer, 1, 1024));
    TEST_1(tp = tport_tsend(tp0, msg, tt->tt_sctp_name, TAG_END()));
    msg_destroy(msg);
  }

  /* Try to overflow the queue */
  snprintf(buffer, sizeof buffer, "cid:sctp-%u", n + i);
  TEST_1(!new_test_msg(tt, &msg, buffer, 1, 1024));
  TEST_1(!tport_tsend(tt->tt_tports, msg, tt->tt_sctp_name, TAG_END()));
  msg_destroy(msg);

  TEST(tport_test_run(tt, 5), 1);

  TEST_1(!check_msg(tt, tt->tt_rmsg, NULL));
  test_check_md5(tt, tt->tt_rmsg);
  msg_destroy(tt->tt_rmsg), tt->tt_rmsg = NULL;

  /* This uses a new connection */
  TEST_1(!new_test_msg(tt, &msg, "cid:sctp-new", 1, 1024));
  TEST_1(tport_tsend(tt->tt_tports, msg, tt->tt_sctp_name,
		     TPTAG_REUSE(0), TAG_END()));
  msg_destroy(msg);

  /* Receive every message from queue */
  for (; tt->tt_received < n + TPORT_QUEUESIZE - 1;) {
    TEST(tport_test_run(tt, 10), 1);
    /* Validate message */
    TEST_1(!check_msg(tt, tt->tt_rmsg, NULL));
    msg_destroy(tt->tt_rmsg), tt->tt_rmsg = NULL;
  }

  /* Try to send a single message */
  TEST_1(!new_test_msg(tt, &msg, "cid:sctp-final", 1, 512));
  TEST_1(tport_tsend(tt->tt_tports, msg, tt->tt_sctp_name, TAG_END()));
  msg_destroy(msg);

  TEST(tport_test_run(tt, 10), 1);

  TEST_1(!check_msg(tt, tt->tt_rmsg, NULL));
  msg_destroy(tt->tt_rmsg), tt->tt_rmsg = NULL;
  }

  tport_unref(tp0);

  /* Wait until notifications -
     client closes when idle and notifys pending,
     then server closes and notifys pending */
  while (pending_server_close || pending_client_close)
    su_root_step(tt->tt_root, 50);

  END();
}

struct called {
  int n, error, pending, released;
};

static
void tls_error_callback(tp_stack_t *tt, tp_client_t *client,
			tport_t *tp, msg_t *msg, int error)
{
  struct called *called = (struct called *)client;

  tt->tt_status = -1;

  called->n++, called->error = error;

  if (called->pending) {
    called->released = tport_release(tp, called->pending, msg, NULL, client, 0);
    called->pending = 0;
  }
}

static int tls_test(tp_test_t *tt)
{
  BEGIN();

#if HAVE_TLS
  tp_name_t const *dst = tt->tt_tls_name;
  msg_t *msg = NULL;
  int i;
  char ident[16];
  tport_t *tp, *tp0;
  struct called called[1] = {{ 0, 0, 0, 0 }};

  TEST_S(dst->tpn_proto, "tls");

  /* Send a single message */
  TEST_1(!new_test_msg(tt, &msg, "tls-first", 1, 1024));
  TEST_1(tp = tport_tsend(tt->tt_tports, msg, dst, TAG_END()));
  TEST_1(tp0 = tport_ref(tp));
  TEST_1(called->pending = tport_pend(tp, msg, tls_error_callback, (tp_client_t *)called));

  i = tport_test_run(tt, 5);
  msg_destroy(msg);

  if (i < 0) {
    if (called->n) {
      TEST(called->released, 0);
      puts("test_tport: skipping TLS tests");
      tport_unref(tp0);
      return 0;
    }
  }

  TEST(i, 1);

  TEST_1(!check_msg(tt, tt->tt_rmsg, "tls-first"));
  msg_destroy(tt->tt_rmsg), tt->tt_rmsg = NULL;

  tport_set_params(tp, TPTAG_KEEPALIVE(100), TPTAG_IDLE(500), TAG_END());

  /* Ask for notification upon close */
  pending_client_close = tport_pend(tp0, NULL, client_closed_callback, NULL);
  TEST_1(pending_client_close > 0);
  tp = tt->tt_rtport;
  pending_server_close = tport_pend(tp, NULL, server_closed_callback, NULL);

  TEST_1(pending_server_close > 0);

  /* Send a largish message */
  TEST_1(!new_test_msg(tt, &msg, "tls-0", 16, 16 * 1024));
  test_create_md5(tt, msg);
  TEST_1(tp = tport_tsend(tt->tt_tports, msg, dst, TAG_END()));
  TEST_1(tp == tp0);
  msg_destroy(msg);

  /* Fill up the queue */
  for (i = 1; i < TPORT_QUEUESIZE; i++) {
    snprintf(ident, sizeof ident, "tls-%u", i);

    TEST_1(!new_test_msg(tt, &msg, ident, 2, 512));
    TEST_1(tp = tport_tsend(tt->tt_tports, msg, dst, TAG_END()));
    TEST_1(tp == tp0);
    msg_destroy(msg);
  }

  /* This uses a new connection */
  TEST_1(!new_test_msg(tt, &msg, "tls-no-reuse", 1, 1024));
  TEST_1(tp = tport_tsend(tt->tt_tports, msg, dst,
		     TPTAG_REUSE(0), TAG_END()));
  TEST_1(tp != tp0);
  msg_destroy(msg);

  tt->tt_received = 0;

  /* Receive every message from queue */
  while (tt->tt_received < TPORT_QUEUESIZE + 1) {
    TEST(tport_test_run(tt, 5), 1);
    /* Validate message */
    msg_destroy(tt->tt_rmsg), tt->tt_rmsg = NULL;
  }

  /* Try to send a single message */
  TEST_1(!new_test_msg(tt, &msg, "tls-last", 1, 1024));
  TEST_1(tport_tsend(tt->tt_tports, msg, dst, TAG_END()));
  msg_destroy(msg);

  TEST(tport_test_run(tt, 5), 1);

  TEST_1(!check_msg(tt, tt->tt_rmsg, "tls-last"));
  msg_destroy(tt->tt_rmsg), tt->tt_rmsg = NULL;

  tport_decref(&tp0);

  /* Wait until notifications -
     client closes when idle and notifys pending,
     then server closes and notifys pending */
  while (pending_server_close || pending_client_close)
    su_root_step(tt->tt_root, 50);

#endif

  END();
}

static int sigcomp_test(tp_test_t *tt)
{
  BEGIN();

#if HAVE_SIGCOMP
  su_home_t *home;
  tp_name_t tpn[1] = {{ NULL }};
  struct sigcomp_compartment *cc;

  if (tt->tt_udp_comp->tpn_comp) {
    msg_t *msg = NULL;

    TEST_1(cc = test_sigcomp_compartment(tt, tt->tt_tports, tt->tt_udp_comp));

    TEST_1(!new_test_msg(tt, &msg, "udp-sigcomp", 1, 1200));
    test_create_md5(tt, msg);
    TEST_1(tport_tsend(tt->tt_tports,
		       msg,
		       tt->tt_udp_comp,
		       TPTAG_COMPARTMENT(cc),
		       TAG_END()));
    msg_destroy(msg);

    TEST(tport_test_run(tt, 5), 1);

    TEST_1(!check_msg(tt, tt->tt_rmsg, NULL));

    test_check_md5(tt, tt->tt_rmsg);

    TEST_1(msg = tt->tt_rmsg); tt->tt_rmsg = NULL;

    TEST_1(home = msg_home(msg));

    TEST_1(tport_convert_addr(home, tpn, "udp", NULL, msg_addr(msg)) == 0);

    tpn->tpn_comp = tport_name(tt->tt_rtport)->tpn_comp;

    /* reply */
    TEST_1(cc = test_sigcomp_compartment(tt, tt->tt_tports, tpn));
    TEST_1(tport_tsend(tt->tt_rtport, msg, tpn,
		       TPTAG_COMPARTMENT(cc),
		       TAG_END()) != NULL);

    msg_destroy(msg);

    TEST(tport_test_run(tt, 5), 1);

    TEST_1(!check_msg(tt, tt->tt_rmsg, NULL));
    test_check_md5(tt, tt->tt_rmsg);
    msg_destroy(tt->tt_rmsg), tt->tt_rmsg = NULL;
  }

  if (tt->tt_tcp_comp->tpn_comp) {
    tport_t *tp;
    msg_t *msg = NULL;

    *tpn = *tt->tt_tcp_comp;

    TEST_1(!new_test_msg(tt, &msg, "tcp-sigcomp", 1, 1500));
    test_create_md5(tt, msg);

    tport_log->log_level = 9;

    TEST_1(cc = test_sigcomp_compartment(tt, tt->tt_tports, tpn));
    TEST_1(tp = tport_tsend(tt->tt_tports,
			    msg,
			    tpn,
			    TPTAG_COMPARTMENT(cc),
			    TAG_END()));
    TEST_1(tport_incref(tp)); tport_decref(&tp);
    msg_destroy(msg);

    TEST(tport_test_run(tt, 5), 1);

    TEST_1(!check_msg(tt, tt->tt_rmsg, "tcp-sigcomp"));
    test_check_md5(tt, tt->tt_rmsg);
    msg_destroy(tt->tt_rmsg), tt->tt_rmsg = NULL;

    TEST_1(!new_test_msg(tt, &msg, "tcp-sigcomp-2", 1, 3000));
    test_create_md5(tt, msg);
    TEST_1(tp = tport_tsend(tt->tt_tports,
			    msg,
			    tt->tt_tcp_comp,
			    TPTAG_COMPARTMENT(cc),
			    TAG_END()));
    TEST_1(tport_incref(tp)); tport_decref(&tp);
    msg_destroy(msg);

    TEST(tport_test_run(tt, 5), 1);

    TEST_1(!check_msg(tt, tt->tt_rmsg, "tcp-sigcomp-2"));
    test_check_md5(tt, tt->tt_rmsg);

    msg_destroy(tt->tt_rmsg), tt->tt_rmsg = NULL;

    TEST_1(!new_test_msg(tt, &msg, "tcp-sigcomp-3", 1, 45500));
    test_create_md5(tt, msg);
    TEST_1(tp = tport_tsend(tt->tt_tports,
			    msg,
			    tt->tt_tcp_comp,
			    TPTAG_COMPARTMENT(cc),
			    TAG_END()));
    TEST_1(tport_incref(tp));
    msg_destroy(msg);

    TEST(tport_test_run(tt, 5), 1);

    tport_decref(&tp);
    TEST_1(!check_msg(tt, tt->tt_rmsg, "tcp-sigcomp-3"));
    test_check_md5(tt, tt->tt_rmsg);
    msg_destroy(tt->tt_rmsg), tt->tt_rmsg = NULL;

    {
      tp_name_t tpn[1];
      tport_t *ctp, *rtp;

      *tpn = *tt->tt_tcp_comp; tpn->tpn_comp = NULL;

      TEST_1(!new_test_msg(tt, &msg, "tcp-sigcomp-4", 1, 1000));
      test_create_md5(tt, msg);
      TEST_1(tp = tport_tsend(tt->tt_tports,
			      msg,
			      tpn,
			      TPTAG_COMPARTMENT(cc),
			      TPTAG_FRESH(1),
			      TAG_END()));
      TEST_1(ctp = tport_incref(tp));
      msg_destroy(msg);

      TEST(tport_test_run(tt, 5), 1);

      TEST_1(!check_msg(tt, tt->tt_rmsg, "tcp-sigcomp-4"));
      test_check_md5(tt, tt->tt_rmsg);
      TEST_1((msg_addrinfo(tt->tt_rmsg)->ai_flags & TP_AI_COMPRESSED) == 0);
      msg_destroy(tt->tt_rmsg), tt->tt_rmsg = NULL;
      TEST_1(rtp = tport_incref(tt->tt_rtport));

      TEST_1(!new_test_msg(tt, &msg, "tcp-sigcomp-5", 1, 1000));
      test_create_md5(tt, msg);
      {
	/* Mess with internal data structures in order to
	   force tport to use SigComp on this connection */
	tp_name_t *tpn = (tp_name_t *)tport_name(rtp);
	tpn->tpn_comp = "sigcomp";
      }
      TEST_1(tp = tport_tsend(rtp,
			      msg,
			      tt->tt_tcp_comp,
			      TPTAG_COMPARTMENT(cc),
			      TAG_END()));
      TEST_1(tport_incref(tp));
      msg_destroy(msg);

      TEST(tp, rtp);

      TEST(tport_test_run(tt, 5), 1);

      tport_decref(&tp);
      TEST_1(!check_msg(tt, tt->tt_rmsg, "tcp-sigcomp-5"));
      test_check_md5(tt, tt->tt_rmsg);
      TEST_1((msg_addrinfo(tt->tt_rmsg)->ai_flags & TP_AI_COMPRESSED) != 0);
      msg_destroy(tt->tt_rmsg), tt->tt_rmsg = NULL;
      TEST(ctp, tt->tt_rtport);
      tport_decref(&ctp);
    }
  }
#endif

  END();
}

#if HAVE_SOFIA_STUN

#include <sofia-sip/stun_tag.h>

static int stun_test(tp_test_t *tt)
{
  BEGIN();

  tport_t *mr;
  tp_name_t tpn[1] = {{ "*", "*", "*", "*", NULL }};
#if HAVE_NETINET_SCTP_H
  char const * transports[] = { "udp", "tcp", "sctp", NULL };
#else
  char const * transports[] = { "udp", "tcp", NULL };
#endif

  TEST_1(mr = tport_tcreate(tt, tp_test_class, tt->tt_root, TAG_END()));

  TEST(tport_tbind(tt->tt_tports, tpn, transports, TPTAG_SERVER(1),
		   STUNTAG_SERVER("999.999.999.999"),
		   TAG_END()), -1);

  tport_destroy(mr);

  END();
}
#else
static int stun_test(tp_test_t *tt)
{
  return 0;
}
#endif

static int deinit_test(tp_test_t *tt)
{
  BEGIN();

  /* Destroy client transports */
  tport_destroy(tt->tt_tports), tt->tt_tports = NULL;

  /* Destroy server transports */
  tport_destroy(tt->tt_srv_tports), tt->tt_srv_tports = NULL;

#if HAVE_SIGCOMP
  sigcomp_state_handler_free(tt->state_handler); tt->state_handler = NULL;
#endif

  END();
}

/* Test tport_tags filter */
static int filter_test(tp_test_t *tt)
{
  tagi_t *lst, *result;

  su_home_t home[1] = { SU_HOME_INIT(home) };

  BEGIN();

  lst = tl_list(TSTTAG_HEADER_STR("X: Y"),
		TAG_SKIP(2),
		TPTAG_IDENT("foo"),
		TSTTAG_HEADER_STR("X: Y"),
		TPTAG_IDENT("bar"),
		TAG_NULL());

  TEST_1(lst);

  result = tl_afilter(home, tport_tags, lst);

  TEST_1(result);
  TEST_P(result[0].t_tag, tptag_ident);
  TEST_P(result[1].t_tag, tptag_ident);

  free(lst);
  su_home_deinit(home);

  END();
}

#if HAVE_ALARM
#include <signal.h>

static RETSIGTYPE sig_alarm(int s)
{
  fprintf(stderr, "%s: FAIL! test timeout!\n", name);
  exit(1);
}

char const alarm_option[] = " [--no-alarm]";

#else
char const alarm_option[] = "";
#endif

void usage(int exitcode)
{
  fprintf(stderr, "usage: %s [-v] [-a]%s\n", name, alarm_option);
  exit(exitcode);
}

int main(int argc, char *argv[])
{
  int flags = 0;	/* XXX */
  int retval = 0;
  int no_alarm = 0;
  int i;

  tp_test_t tt[1] = {{{ SU_HOME_INIT(tt) }}};

  for (i = 1; argv[i]; i++) {
    if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbatim") == 0)
      tstflags |= tst_verbatim;
    else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--abort") == 0)
      tstflags |= tst_abort;
    else if (strcmp(argv[i], "--no-alarm") == 0)
      no_alarm = 1;
    else
      usage(1);
  }

#if HAVE_OPEN_C
  tstflags |= tst_verbatim;
#endif

#if HAVE_ALARM
  if (!no_alarm) {
    signal(SIGALRM, sig_alarm);
    alarm(120);
  }
#endif

  /* Use log */
  if (flags & tst_verbatim)
    tport_log->log_default = 9;
  else
    tport_log->log_default = 1;

  su_init();

  retval |= name_test(tt); fflush(stdout);
  retval |= filter_test(tt); fflush(stdout);

  retval |= init_test(tt); fflush(stdout);
  if (retval == 0) {
    retval |= sigcomp_test(tt); fflush(stdout);
    retval |= sctp_test(tt); fflush(stdout);
    retval |= udp_test(tt); fflush(stdout);
    retval |= tcp_test(tt); fflush(stdout);
    retval |= test_incomplete(tt); fflush(stdout);
    retval |= reuse_test(tt); fflush(stdout);
    retval |= tls_test(tt); fflush(stdout);
    if (0)			/* Not yet working... */
      retval |= stun_test(tt); fflush(stdout);
    retval |= deinit_test(tt); fflush(stdout);
  }

  su_deinit();

#if HAVE_OPEN_C
  sleep(10);
#endif

  return retval;
}

