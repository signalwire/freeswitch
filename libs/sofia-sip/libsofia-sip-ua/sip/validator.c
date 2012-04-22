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

/**@internal @IFILE validator.c
 *
 * SIP parser tester. This uses output from tport dump where messages are
 * separated with Control-K ('\v') from each other.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Wed Mar 21 19:12:13 2001 ppessi
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/mman.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <sofia-sip/su_types.h>
#include <sofia-sip/su_alloc_stat.h>

#include <sofia-sip/su_time.h>

#include <sofia-sip/su_tag.h>
#include <sofia-sip/su_tag_class.h>
#include <sofia-sip/su_tag_io.h>

#include <sofia-sip/sip_tag.h>
#include <sofia-sip/url_tag.h>

#include <sofia-sip/sip.h>
#include <sofia-sip/sip_header.h>

#include <sofia-sip/msg_buffer.h>

char const *name = "validator";

typedef struct {
  unsigned o_verbose : 1;	/**< Be verbose */
  unsigned o_very_verbose : 1;	/**< Be very verbose */
  unsigned o_requests : 1;	/**< Only requests */
  unsigned o_responses : 1;	/**< Only responses */
  unsigned o_decode : 1;	/**< Only try to decode,
				   print error if unknown headers */
  unsigned o_print : 1;		/**< Print whole message */
  unsigned o_times : 1;		/**< Generate timing information */
  unsigned o_memstats : 1;	/**< Generate memory statistics */
  unsigned o_histogram : 1;	/**< Generate histograms */
  unsigned o_sipstats : 1;	/**< Generate header statistics  */
  unsigned o_vsipstats : 1;	/**< Generate verbatim header statistics */
  unsigned : 0;
  unsigned o_flags;		/**< Message flags */
} options_t;

typedef struct {
  size_t N;
  uint32_t bsize;
  double buckets[32768];
} histogram_t;

static
histogram_t *histogram_create(uint64_t max, uint32_t bsize)
{
  size_t N = (max + bsize - 1) / bsize;
  histogram_t *h = calloc(1, offsetof(histogram_t, buckets[N + 1]));
  if (!h) { perror("calloc"); exit(1); }
  h->N = N, h->bsize = bsize;
  return h;
}

static
double *histogram_update(histogram_t *h, uint32_t n)
{
  if (h->bsize > 1)
    n /= h->bsize;

  if (n < h->N)
    return &h->buckets[n];
  else
    return &h->buckets[h->N];
}

static void
histogram_div(histogram_t *h, histogram_t const *n)
{
  size_t i;
  assert(h->N == n->N); assert(h->bsize == n->bsize);

  for (i = 0; i <= h->N; i++) {
    if (n->buckets[i]) {
      h->buckets[i] /= n->buckets[i];
    }
    else {
      assert(h->buckets[i] == 0);
    }
  }
}

typedef struct {
  uint64_t number;
  uint64_t headers;
  uint64_t payloads;
  uint64_t pl_bytes;
} sipstat_t;

typedef struct {
  sipstat_t req, resp;
  histogram_t *hist_headers;
} sipstats_t;

typedef struct {
  char const *name;
  char const *sep;
  uint64_t  messages;
  uint64_t  bytes;
  uint64_t  errors;
  uint32_t  files;
  double    time;
  options_t options[1];

  /* Statistics */
  histogram_t *hist_msgsize;
  histogram_t *hist_mallocs;
  histogram_t *hist_memsize;
  histogram_t *hist_nheaders;
  sipstats_t   sipstats[1];
  su_home_stat_t hs[1];

  uint64_t     est_fail, est_succ, est_slack;
} context_t;

void usage(void)
{
  fprintf(stderr,
	  "usage: %s [-vdp]\n",
	  name);
  exit(2);
}

char *lastpart(char *path)
{
  char *p = strrchr(path, '/');

  if (p)
    return p + 1;
  else
    return path;
}

msg_mclass_t const *mclass = NULL;

int validate_file(int fd, char const *name, context_t *ctx);
int validate_dump(char *, off_t, context_t *ctx);
int report(context_t const *ctx);
static void memstats(msg_t *, uint32_t msize, context_t *ctx);
static void sipstats(msg_t *, uint32_t msize, sipstats_t *ss, context_t *ctx);

int main(int argc, char *argv[])
{
  context_t ctx[1] =  {{ 0 }};
  options_t *o = ctx->options;

  name = lastpart(argv[0]);  /* Set our name */

  for (; argv[1]; argv++) {
    if (argv[1][0] == 0)
      usage();
    else if (argv[1][0] != '-')
      break;
    else if (argv[1][1] == 0) {
      argv++; break;
    }
    else if (strcmp(argv[1], "-v") == 0)
      o->o_very_verbose = o->o_verbose, o->o_verbose = 1;
    else if (strcmp(argv[1], "-d") == 0)
      o->o_decode = 1;		/* Decode only */
    else if (strcmp(argv[1], "-p") == 0)
      o->o_print = 1;
    else if (strcmp(argv[1], "-q") == 0)
      o->o_requests = 1;
    else if (strcmp(argv[1], "-Q") == 0)
      o->o_responses = 1;
    else if (strcmp(argv[1], "-t") == 0)
      o->o_times = 1;
    else if (strcmp(argv[1], "-m") == 0)
      o->o_memstats = 1;
    else if (strcmp(argv[1], "-s") == 0)
      o->o_vsipstats = o->o_sipstats, o->o_sipstats = 1;
    else if (strcmp(argv[1], "-h") == 0)
      o->o_histogram = 1;
    else
      usage();
  }

  if (o->o_requests && o->o_responses)
    usage();

  if (!mclass)
    mclass = sip_default_mclass();

  if (argv[1]) {
    for (; argv[1]; argv++) {
      int fd = open(argv[1], O_RDONLY, 000);
      if (fd == -1)
	perror(argv[1]), exit(1);
      if (validate_file(fd, argv[1], ctx))
	exit(1);
      close(fd);
    }
  }
  else
    validate_file(0, "", ctx);

  report(ctx);

  exit(0);
}


int validate_file(int fd, char const *name, context_t *ctx)
{
  void *p;
  off_t size;
  int retval;

  ctx->name = name;
  if (strlen(name))
    ctx->sep = ": ";
  else
    ctx->sep = "";

  ctx->files++;

  size = lseek(fd, 0, SEEK_END);

  if (size < 1)
    return 0;
  if (size > INT_MAX) {
    fprintf(stderr, "%s%stoo large file to map\n", ctx->name, ctx->sep);
    return -1;
  }

#ifndef _WIN32
  p = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0L);
  if (p == NULL) {
    perror("mmap");
    return -1;
  }

  retval = validate_dump(p, size, ctx);
  munmap(p, size);
  return retval;

#else
  errno = EINVAL;
  perror("mmap not implemented");
  return -1;
#endif

}

su_inline
void nul_terminate(char *b, off_t size)
{
  char *end;

  /* NUL-terminate */
  for (end = b + size - 1; end != b; end--)
    if (*end == '\v')
      break;

  *end = '\0';
}

su_inline
int search_msg(char **bb, char const *protocol)
{
  int linelen, plen = strlen(protocol);
  char *b = *bb;

  for (;;) {
    if (!b[0])
      return 0;

    if (strncmp(b, protocol, plen) == 0 && b[plen] == ' ')
      return 1;			/* status */

    linelen = strcspn(b, "\r\n");

    if (linelen > plen + 1 &&
	b[linelen - plen - 1] == ' ' &&
	strncmp(b + linelen - plen, protocol, plen) == 0)
      return 1;			/* request */

    b += linelen + strspn(b + linelen, "\r\n");
    *bb = b;
  }
}

int validate_dump(char *b, off_t size, context_t *ctx)
{
  size_t n = 0, N = 0;
  struct message {
    char *b;
    int   size;
  } *msgs = NULL;
  uint64_t time0, time1;
  options_t *o = ctx->options;
  int maxsize = 0;

  nul_terminate(b, size);

  /* Split dump file to messages */
  while (search_msg(&b, SIP_VERSION_CURRENT)) {
    int msize = strcspn(b, "\v");
    int linelen = strcspn(b, "\r\n");

    if (o->o_responses &&
	memcmp(b, SIP_VERSION_CURRENT, strlen(SIP_VERSION_CURRENT)) != 0)
      ;
    else if (o->o_requests &&
	memcmp(b, SIP_VERSION_CURRENT, strlen(SIP_VERSION_CURRENT)) == 0)
      ;
    else {
      if (o->o_very_verbose)
	printf("message "MOD_ZU": %.*s\n", n, linelen, b);

      if (n == N) {
	N *= 2; if (n == N) N = 16;

	msgs = realloc(msgs, sizeof(*msgs) * N);
	if (msgs == NULL) {
	  perror("realloc");
	  exit(1);
	}
      }
      msgs[n].b = b; msgs[n].size = msize;
      n++;

      ctx->bytes += msize;

      if (msize > maxsize)
	maxsize = msize;
    }

    b += msize; if (*b) *b++ = '\0';
  }

  ctx->messages += N = n;

  if (o->o_histogram) {
    ctx->hist_msgsize = histogram_create(maxsize, 64);

    if (o->o_memstats) {
      ctx->hist_mallocs = histogram_create(maxsize, 64);
      ctx->hist_memsize = histogram_create(maxsize, 64);
    }

    if (o->o_sipstats) {
      ctx->sipstats->hist_headers = histogram_create(64, 1);
      ctx->hist_nheaders = histogram_create(maxsize, 64);
    }
  }

  time0 = su_nanocounter();

  for (n = 0; n < N; n++) {
    msg_t *msg = msg_create(mclass, o->o_flags);
    int m;

    if (msg == NULL) {
      perror("msg_create"); exit(1);
    }

    if (o->o_memstats)
      su_home_init_stats(msg_home(msg));

    msg_buf_set(msg, msgs[n].b, msgs[n].size + 1);
    msg_buf_commit(msg, msgs[n].size, 1);

    su_home_preload(msg_home(msg), 1, msgs[n].size + 384);

    m = msg_extract(msg);

    if (m < 0) {
      fprintf(stderr, "%s%sparsing error in message "MOD_ZU"\n",
	      ctx->name, ctx->sep, n);
      ctx->errors++;
    }
    else {
      if (ctx->hist_msgsize)
	*histogram_update(ctx->hist_msgsize, msgs[n].size) += 1;

      if (o->o_sipstats)
	sipstats(msg, msgs[n].size, ctx->sipstats, ctx);

      if (o->o_memstats)
	memstats(msg, msgs[n].size, ctx);
    }

    msg_destroy(msg);
  }

  time1 = su_nanocounter();

  if (o->o_times) {
    double dur = (time1 - time0) * 1E-9;

    ctx->time += dur;

    printf("%s%s"MOD_ZU" messages in %g seconds (%g msg/sec)\n"
	   "      parse speed %.1f Mb/s (on Ethernet wire %.1f Mb/s)\n",
	   ctx->name, ctx->sep, N, dur, (double)N / dur,
	   (double)ctx->bytes * 8 / ctx->time / 1e6,
	   ((double)ctx->bytes + N * (16 + 20 + 8)) * 8 / ctx->time / 1e6);
  }

  free(msgs);

  return 0;
}

typedef unsigned longlong ull;

static
void report_memstats(char const *title, su_home_stat_t const hs[1])
{
  printf("%s%smemory statistics\n", title, strlen(title) ? " " : "");
  if (hs->hs_allocs.hsa_number)
    printf("\t"LLU" allocs, "LLU" bytes, "LLU" rounded,"
	   " "LLU" max\n",
	   (ull)hs->hs_allocs.hsa_number, (ull)hs->hs_allocs.hsa_bytes,
	   (ull)hs->hs_allocs.hsa_rbytes, (ull)hs->hs_allocs.hsa_maxrbytes);
  if (hs->hs_frees.hsf_number)
    printf("\t"LLU" frees, "LLU" bytes, rounded to "LLU" bytes\n",
	   (ull)hs->hs_frees.hsf_number, (ull)hs->hs_frees.hsf_bytes,
	   (ull)hs->hs_frees.hsf_rbytes);
  if (hs->hs_rehash || hs->hs_clones)
    printf("\t"LLU" rehashes, "LLU" clones\n",
	   (ull)hs->hs_rehash, (ull)hs->hs_clones);
}

void memstats(msg_t *msg, uint32_t msize, context_t *ctx)
{
  options_t *o = ctx->options;
  su_home_stat_t hs[1];

  su_home_get_stats(msg_home(msg), 1, hs, sizeof(hs));
  su_home_stat_add(ctx->hs, hs);

  if (o->o_histogram) {
    *histogram_update(ctx->hist_mallocs, msize) += hs->hs_allocs.hsa_number;
    *histogram_update(ctx->hist_memsize, msize) += hs->hs_allocs.hsa_maxrbytes;
  }

  {
    int estimate = msize + 384;
    int slack = estimate - hs->hs_allocs.hsa_maxrbytes;

    if (slack < 0)
      ctx->est_fail++;
    else {
      ctx->est_succ++;
      ctx->est_slack += slack;
    }
  }

  if (o->o_very_verbose)
    report_memstats(ctx->name, hs);
}

void report_sipstat(char const *what, sipstat_t const *sss)
{
  printf("%s: "LLU" with %.1f headers (total "LLU")\n",
	 what, (ull)sss->number, (double)sss->headers / sss->number,
	 (ull)sss->headers);
  if (sss->payloads)
    printf("\t"LLU" with body of %.1f bytes (total "LLU")\n",
	   (ull)sss->payloads, (double)sss->pl_bytes / sss->payloads,
	   (ull)sss->payloads);
}

void sipstats(msg_t *msg, uint32_t msize, sipstats_t *ss, context_t *ctx)
{
  options_t *o = ctx->options;
  msg_pub_t *m = msg_object(msg);
  sip_t const *sip = sip_object(msg);
  msg_header_t *h;
  sipstat_t *sss;
  size_t n, bytes;

  if (!sip)
    return;

  if (m->msg_request) {
    sss = &ss->req;
    h = m->msg_request;
  }
  else if (m->msg_status) {
    sss = &ss->resp;
    h = m->msg_status;
  }
  else {
    return;
  }

  sss->number++;

  /* Count headers */
  for (n = 0, h = h->sh_succ; h && !sip_is_separator((sip_header_t *)h); h = h->sh_succ)
    n++;

  sss->headers += n;

  bytes = sip->sip_payload ? (size_t)sip->sip_payload->pl_len : 0;

  if (bytes) {
    sss->payloads++;
    sss->pl_bytes += bytes;
  }

  if (ctx->hist_nheaders) {
    *histogram_update(ctx->hist_nheaders, msize) += n;
    *histogram_update(ss->hist_headers, n) += 1;
  }

  if (o->o_very_verbose)
    printf("%s%s"MOD_ZU" headers, "MOD_ZU" bytes in payload\n",
	   ctx->name, ctx->sep, n, bytes);
}

void report_histogram(char const *title, histogram_t const *h)
{
  size_t i, min_i, max_i;

  for (i = 0; i < h->N && h->buckets[i] == 0.0; i++)
    ;
  min_i = i;

  for (i = h->N - 1; i >= 0 && h->buckets[i] == 0.0; i--)
    ;
  max_i = i;

  if (min_i >= max_i)
    return;

  printf("%s histogram\n", title);
  for (i = min_i; i < max_i; i++)
    printf("\t"MOD_ZU".."MOD_ZU": %.1f\n", i * h->bsize, (i + 1) * h->bsize, h->buckets[i]);

  if (h->buckets[h->N])
    printf("\t"MOD_ZU"..: %.1f\n", h->N * h->bsize, h->buckets[h->N]);
}

int report(context_t const *ctx)
{
  const options_t *o = ctx->options;
  uint64_t n = ctx->messages;

  if (!n)
    return -1;

  printf("total "LLU" messages with "LLU" bytes (mean size "LLU")\n",
	 (ull)n, (ull)ctx->bytes, (ull)(ctx->bytes / n));

  if (ctx->hist_msgsize)
    report_histogram("Message size", ctx->hist_msgsize);

  if (o->o_times && ctx->files > 1)
    printf("total "LLU" messages in %g seconds (%g msg/sec)\n",
	   (ull)n, ctx->time, (double)n / ctx->time);

  if (o->o_sipstats) {
    const sipstats_t *ss = ctx->sipstats;
    report_sipstat("requests", &ss->req);
    report_sipstat("responses", &ss->resp);

    if (ctx->hist_nheaders) {
      histogram_div(ctx->hist_nheaders, ctx->hist_msgsize);
      report_histogram("Number of headers", ctx->hist_nheaders);
    }
  }

  if (o->o_memstats) {
    su_home_stat_t hs[1];

    *hs = *ctx->hs;
    report_memstats("total", hs);

    /* Calculate mean */
    hs->hs_clones /= n; hs->hs_rehash /= n;
    hs->hs_allocs.hsa_number /= n; hs->hs_allocs.hsa_bytes /= n;
    hs->hs_allocs.hsa_rbytes /= n; hs->hs_allocs.hsa_maxrbytes /= n;
    hs->hs_frees.hsf_number /= n; hs->hs_frees.hsf_bytes /= n;
    hs->hs_frees.hsf_rbytes /= n;
    hs->hs_blocks.hsb_number /= n; hs->hs_blocks.hsb_bytes /= n;
    hs->hs_blocks.hsb_rbytes /= n;

    report_memstats("mean", hs);

    printf("\testimator fails %.1f%% times (mean slack %.0f bytes)\n",
	   100 * (double)ctx->est_fail / (ctx->est_fail + ctx->est_succ),
	   (double)ctx->est_slack / ctx->est_succ);

    if (ctx->hist_memsize) {
      histogram_div(ctx->hist_memsize, ctx->hist_msgsize);
      report_histogram("Allocated memory", ctx->hist_memsize);
    }
  }

  return 0;
}
