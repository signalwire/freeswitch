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

/**@file resolve_sip.c Use sresolv library to resolve a SIP or SIPS domain.
 *
 * This is an example program for @b sresolv library.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @par Created: Tue Jul 16 18:50:14 2002 ppessi
 *
 *
 */

#include <stdint.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Typesafe */
#define SRES_CONTEXT_T struct context

#include "sofia-sip/sresolv.h"
#include "sofia-sip/su_string.h"

char const name[] = "sip_resolve";

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <poll.h>

enum progress {
  querying_naptr,
  querying_srv,
  querying_cname,
  querying_a6,
  querying_aaaa,
  querying_a
};

/* Application context */
struct context
{
  sres_resolver_t     *sres;
  int                  sr_exitcode;
  int                  sr_ready;

  char const          *sr_canon;
  int                  sr_sips;
  char const          *sr_tport;

  sres_query_t        *sr_query;
  unsigned short       sr_port;

  int                  sr_n_sockets;
  sres_socket_t       *sr_sockets;
  struct pollfd       *sr_pollfds;

#if 0
  char const          *sr_domain;

  enum progress        sr_progress;
  sres_naptr_record_t *sr_naptr;
  sres_srv_record_t   *sr_srv;
  sres_cname_record_t *sr_cname;
  sres_aaaa_record_t  *sr_aaaa;
  sres_a6_record_t    *sr_a6;
  sres_a_record_t     *sr_a;
#endif
};

static int query_srv(struct context *sr, char const *domain);
static int query_a(struct context *sr, char const *domain);

/* Process NAPTR records */
static
void answer_to_naptr_query(sres_context_t *sr,
			   sres_query_t *q,
			   sres_record_t *answers[])
{
  int i;

  sr->sr_query = NULL;

  /* Sort NAPTR records by the order. */
  sres_sort_answers(sr->sres, answers);

  for (i = 0; answers && answers[i]; i++) {
    sres_naptr_record_t const *na = answers[i]->sr_naptr;

    if (na->na_record->r_status)
      /* There was an error */
      continue;

    printf("naptr: %s\n\t%d IN NAPTR %u %u \"%s\" \"%s\" \"%s\" %s\n",
	   na->na_record->r_name, na->na_record->r_ttl,
	   na->na_order, na->na_prefer,
	   na->na_flags, na->na_services,
	   na->na_regexp, na->na_replace);

    switch (na->na_flags[0]) {
    case 's': /* srv */
      if (!su_casenmatch("SIP+", na->na_services, 4))
	/* Something else but SIP */
	break;
      query_srv(sr, na->na_replace);
      sres_free_answers(sr->sres, answers);
      return;

    case 'a':
      if (!su_casenmatch("SIP+", na->na_services, 4))
	/* Something else but SIP */
	break;
      query_a(sr, na->na_replace);
      sres_free_answers(sr->sres, answers);
      return;
    }
  }

  query_a(sr, /* sr->sr_uri->url_host */ sr->sr_canon);

  sres_free_answers(sr->sres, answers);
}

static
int query_naptr(struct context *sr, char const *domain)
{
  sres_record_t **answers;

  answers = sres_cached_answers(sr->sres, sres_type_naptr, domain);

  if (answers) {
    answer_to_naptr_query(sr, NULL, answers);
    return 0;
  }
  else {
    sr->sr_query = sres_query_make(sr->sres, answer_to_naptr_query, sr,
				   sr->sr_sockets[0], sres_type_naptr, domain);
    return sr->sr_query ? 0 : -1;
  }
}

/* Process SRV records */
static
void answer_to_srv_query(sres_context_t *sr, sres_query_t *q,
			 sres_record_t *answers[])
{
  int i;

  sr->sr_query = NULL;

  sres_sort_answers(sr->sres, answers);  /* Sort SRV records by the priority. */

  for (i = 0; answers && answers[i]; i++) {
    sres_srv_record_t const *srv = answers[i]->sr_srv;
    if (srv->srv_record->r_status)
      /* There was an error */
      continue;
    sr->sr_port = srv->srv_port;
    query_a(sr, srv->srv_target);
    return;
  }

  query_a(sr, /* sr->sr_uri->url_host */ sr->sr_canon);

  sres_free_answers(sr->sres, answers);
}

static
int query_srv(struct context *sr, char const *domain)
{
  sres_record_t **answers;

  answers = sres_cached_answers(sr->sres, sres_type_srv, domain);

  if (answers) {
    answer_to_srv_query(sr, NULL, answers);
    return 0;
  }
  else {
    sr->sr_query = sres_query_make(sr->sres, answer_to_srv_query, sr,
				   sr->sr_sockets[0], sres_type_srv, domain);
    return sr->sr_query ? 0 : -1;
  }
}

/* Process A records */
static
void answer_to_a_query(sres_context_t *sr, sres_query_t *q,
		       sres_record_t *answers[])
{
  int i;

  sr->sr_query = NULL;

  for (i = 0; answers && answers[i]; i++) {
    char addr[64];
    sres_a_record_t const *a = answers[i]->sr_a;

    if (a->a_record->r_status)
      continue;			      /* There was an error */

    su_inet_ntop(AF_INET, &a->a_addr, addr, sizeof(addr));
    printf("%s@%s:%u\n", sr->sr_tport, addr, sr->sr_port);
    sr->sr_exitcode = 0;
  }

  sres_free_answers(sr->sres, answers);

  sr->sr_ready = 1;
}

static
int query_a(struct context *sr, char const *domain)
{
  sres_record_t **answers;

  answers = sres_cached_answers(sr->sres, sres_type_a, domain);

  if (answers) {
    answer_to_a_query(sr, NULL, answers);
    return 0;
  }
  else {
    sr->sr_query = sres_query_make(sr->sres, answer_to_a_query, sr,
				   sr->sr_sockets[0], sres_type_a, domain);
    return sr->sr_query ? 0 : -1;
  }
}

void usage(void)
{
  fprintf(stderr, "usage: resolve_sip [-s] [@dnsserver] domain\n");
  exit(1);
}

int prepare_run(struct context *sr)
{
  sr->sr_n_sockets = 1;
  sr->sr_sockets = calloc(1, sizeof(*sr->sr_sockets));
  sr->sr_pollfds = calloc(1, sizeof(*sr->sr_pollfds));

  if (!sr->sr_sockets || !sr->sr_pollfds ||
      (sres_resolver_sockets(sr->sres, sr->sr_sockets, 1) == -1))
    return 0;

  sr->sr_pollfds[0].fd = sr->sr_sockets[0];
  sr->sr_pollfds[0].events = POLLIN | POLLERR;

  return 1;
}

void run(struct context *sr)
{
  int i, n, events;

  n = sr->sr_n_sockets;

  while (!sr->sr_ready) {
    events = poll(sr->sr_pollfds, n, 500);

    if (events)
      for (i = 0; i < n; i++) {
	if (sr->sr_pollfds[i].revents)
	  sres_resolver_receive(sr->sres, sr->sr_pollfds[i].fd);
      }

    /* No harm is done (except wasted CPU) if timer is called more often */
    sres_resolver_timer(sr->sres, sr->sr_sockets[0]);
  }
}

int main(int argc, char *argv[])
{
  struct context sr[1] = {{ 0 }};
  char const *dnsserver = NULL;

  sr->sr_exitcode = 1;
  sr->sr_tport = "*";

  if (argv[1] && strcmp(argv[1], "-s") == 0)
    sr->sr_sips = 1, argv++;

  if (argv[1] && argv[1][0] == '@')
    dnsserver = argv++[1] + 1;

  if (argv[1] == NULL)
    usage();

  sr->sres = sres_resolver_new(getenv("SRESOLV_CONF"));

  if (sr->sres)
    if (prepare_run(sr))
      if (query_naptr(sr, sr->sr_canon = argv[1]) == 0)
	run(sr);

  sres_resolver_unref(sr->sres), sr->sres = NULL;

  return sr->sr_exitcode;
}
