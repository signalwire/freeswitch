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

/**@SIP_PARSER
 *
 * @file test_sip_msg.c  Simple SIP message parser/printer tester.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Created: Fri Feb 18 10:25:08 2000 ppessi
 */

#include "config.h"

/* Avoid casting sip_t to msg_pub_t and sip_header_t to msg_header_t */
#define MSG_PUB_T       struct sip_s
#define MSG_HDR_T       union sip_header_u

#include "sofia-sip/sip_parser.h"
#include "sofia-sip/msg_mclass.h"
#include "sofia-sip/msg_mclass_hash.h"
#include <sofia-sip/sip_header.h>
#include <sofia-sip/sip_util.h>
#include <sofia-sip/msg_addr.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

int diff(const char *olds, const char *news, int *linep, int *pos)
{
  const char *o, *n;

  *linep = 0;

  for (o = olds, n = news; *o && *n && *o == *n ; o++, n++) {
    if (*o == '\n') ++*linep;
  }

  *pos = o - olds;

  return *o != *n;
}

int test_msg_class(msg_mclass_t const *mc)
{
  int i, j, N;

  N = mc->mc_hash_size;

  /* Check parser table sanity */
  for (i = 0; i < N; i++) {
    /* Verify each header entry */
    msg_hclass_t *hc = mc->mc_hash[i].hr_class;

    if (hc == NULL)
      continue;

    /* Short form */
    if (hc->hc_short[0])
      assert(mc->mc_short[hc->hc_short[0] - 'a'].hr_class == hc);

    /* Long form */
    for (j = MC_HASH(hc->hc_name, N); j != i; j = (j + 1) % N)
      assert(mc->mc_hash[j].hr_class);
  }

  return 0;
}

char * url_print(url_t *url, char buf[1024])
{
  url_e(buf, 1024, url);

  return buf;
}

void print_contact(FILE *f, sip_contact_t *m)
{
  char const * const *p;
  char buf[1024];
  const char *sep = "\tContact: ";

  for (;m; m = m->m_next) {
    int quoted_url = 0;
    fputs(sep, f); sep = ", ";

    if (m->m_display) {
      quoted_url = 1;
      fprintf(f, "\"%s\" <",  m->m_display);
    }
    url_print(m->m_url, buf);
    if (!quoted_url && strpbrk(buf, ",;?")) {
      fputs("<", f);
    }
    fputs(buf, f);
    if (quoted_url) fputs(">", f);

    if (m->m_params)
      for (p = m->m_params; *p; p++)
	fprintf(f, " ;%s", *p);

    if (m->m_comment)
      fprintf(f, " (%s)", m->m_comment);
  }

  fputs("\n", f);
}

void print_via(FILE *f, sip_via_t *v)
{
  char const * const *p;
  char const * sep = "\tVia: ";

  for (;v; v = v->v_next) {
    fputs(sep, f); sep = ", ";

    fprintf(f, "%s %s", v->v_protocol, v->v_host);
    if (v->v_port)
      fprintf(f, ":%s", v->v_port);

    if (v->v_params)
      for (p = v->v_params; *p; p++)
	fprintf(f, " ;%s", *p);
    if (v->v_comment)
      fprintf(f, " (%s)", v->v_comment);
  }

  fputs("\n", f);
}

int main(int argc, char *argv[])
{
  char urlbuf[1024];
  size_t n;
  int m, tcp;
  sip_t *sip;
  int exitcode = 0;
  msg_mclass_t const *sip_mclass = sip_default_mclass();
  msg_t *msg = msg_create(sip_mclass, MSG_FLG_EXTRACT_COPY);
  msg_iovec_t iovec[1];

  tcp = argv[1] && strcmp(argv[1], "-t") == 0;

  test_msg_class(sip_mclass);

  for (n = 0, m = 0;;) {
    if (msg_recv_iovec(msg, iovec, 1, 1, 0) < 0) {
      perror("msg_recv_iovec");
      exit(1);
    }
    assert(iovec->mv_len >= 1);

    n = read(0, iovec->mv_base, 1);

    if (n < 0) {
      perror("test_sip_msg read");
      exit(1);
    }

    msg_recv_commit(msg, n, n == 0);

    if (tcp)
      m = msg_extract(msg);

    if (n == 0 || m < 0)
      break;
  }

  if (!tcp)
    m = msg_extract(msg);

  sip = msg_object(msg);
  if (sip)
    fprintf(stdout, "sip flags = %x\n", sip->sip_flags);

  if (m < 0) {
    fprintf(stderr, "test_sip_msg: parsing error ("MOD_ZD")\n", n);
    exit(1);
  }

  if (sip->sip_flags & MSG_FLG_TRUNC) {
    fprintf(stderr, "test_sip_msg: message truncated\n");
    exit(1);
  }

  if (msg_next(msg)) {
    fprintf(stderr, "test_sip_msg: stuff after message\n");
    exit(1);
  }

#if 0
  fprintf(stderr, "test_sip_msg: %d headers (%d short ones), %d unknown\n",
	  msg->mh_n_headers, msg->mh_n_short, msg->mh_n_unknown);

  if (msg->mh_payload) {
    fprintf(stderr, "\twith payload of %d bytes\n",
	    msg->mh_payload->pl_len);
  }
#endif

  if (MSG_HAS_ERROR(sip->sip_flags) || sip->sip_error) {
    fprintf(stderr, "test_sip_msg: parsing error\n");
    exit(1);
  }
  else if (sip_sanity_check(sip) < 0) {
    fprintf(stderr, "test_sip_msg: message failed sanity check\n");
    exit(1);
  }

  if (sip->sip_request) {
    fprintf(stdout, "\trequest %s (%d) %s %s\n",
	    sip->sip_request->rq_method_name,
	    sip->sip_request->rq_method,
	    url_print(sip->sip_request->rq_url, urlbuf),
	    sip->sip_request->rq_version);
    if (sip->sip_request->rq_url->url_type == url_unknown) {
      exitcode = 1;
      fprintf(stderr, "test_sip_msg: invalid request URI\n");
    }
  }

  if (sip->sip_status)
    fprintf(stdout, "\tstatus %s %03d %s\n",
	    sip->sip_status->st_version,
	    sip->sip_status->st_status,
	    sip->sip_status->st_phrase);

  if (sip->sip_cseq)
    fprintf(stdout, "\tCSeq: %u %s (%d)\n",
	    sip->sip_cseq->cs_seq,
	    sip->sip_cseq->cs_method_name,
	    sip->sip_cseq->cs_method);

  if (sip->sip_call_id)
    fprintf(stdout, "\tCall-ID: %s (%x)\n",
	    sip->sip_call_id->i_id,
	    sip->sip_call_id->i_hash);

  if (sip->sip_from)
    fprintf(stdout, "\tFrom: %s@%s%s%s\n",
	    sip->sip_from->a_user ? sip->sip_from->a_user : "[nobody]",
	    sip->sip_from->a_host ? sip->sip_from->a_host : "[nowhere]",
	    sip->sip_from->a_tag ? " ;tag=" : "",
	    sip->sip_from->a_tag ? sip->sip_from->a_tag : "");

  if (sip->sip_to)
    fprintf(stdout, "\tTo: %s@%s%s%s\n",
	    sip->sip_to->a_user ? sip->sip_to->a_user : "[nobody]",
	    sip->sip_to->a_host ? sip->sip_to->a_host : "[nowhere]",
	    sip->sip_to->a_tag ? " ;tag=" : "",
	    sip->sip_to->a_tag ? sip->sip_to->a_tag : "");

  if (sip->sip_contact)
    print_contact(stdout, sip->sip_contact);
  if (sip->sip_via)
    print_via(stdout, sip->sip_via);

  if (sip->sip_content_length) {
    fprintf(stdout, "\tcontent length %u\n",
	    sip->sip_content_length->l_length);
  }

  if (msg_next(msg)) {
    fprintf(stderr, "test_sip_msg: extra stuff after valid message\n");
    exit(1);
  }

  return exitcode;
}
