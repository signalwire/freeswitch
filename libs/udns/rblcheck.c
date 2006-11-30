/* $Id: rblcheck.c,v 1.9 2005/04/24 22:49:10 mjt Exp $
   dnsbl (rbl) checker application

   Copyright (C) 2005  Michael Tokarev <mjt@corpit.ru>
   This file is part of UDNS library, an async DNS stub resolver.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library, in file named COPYING.LGPL; if not,
   write to the Free Software Foundation, Inc., 59 Temple Place,
   Suite 330, Boston, MA  02111-1307  USA

 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include "udns.h"

static const char *version = "udns-rblcheck 0.1";
static char *progname;

struct rblookup {
  struct ipcheck *parent;
  struct in_addr key;
  const char *zone;
  struct dns_rr_a4  *addr;
  struct dns_rr_txt *txt;
};

struct ipcheck {
  const char *name;
  int naddr;
  int listed;
  struct rblookup *lookup;
};

#define notlisted ((void*)1)

static int nzones, nzalloc;
static const char **zones;

static int do_txt;
static int stopfirst;
static int verbose = 1;
/* verbosity level:
 * <0 - only bare As/TXTs
 * 0 - what RBL result
 * 1(default) - what is listed by RBL: result
 * 2          - what is[not ]listed by RBL: result, name lookups
 */

static int listed;
static int failures;

static void *ecalloc(int size, int cnt) {
  void *t = calloc(size, cnt);
  if (!t) {
    fprintf(stderr, "%s: out of memory\n", progname);
    exit(1);
  }
  return t;
}

static void addzone(const char *zone) {
  if (nzones >= nzalloc) {
    const char **zs = (const char**)ecalloc(sizeof(char*), (nzalloc += 16));
    if (zones) {
      memcpy(zs, zones, nzones * sizeof(char*));
      free(zones);
    }
    zones = zs;
  }
  zones[nzones++] = zone;
}

static int addzonefile(const char *fname) {
  FILE *f = fopen(fname, "r");
  char linebuf[2048];
  if (!f)
    return 0;
  while(fgets(linebuf, sizeof(linebuf), f)) {
    char *p = linebuf, *e;
    while(*p == ' ' || *p == '\t') ++p;
    if (*p == '#' || *p == '\n') continue;
    e = p;
    while(*e && *e != ' ' && *e != '\t' && *e != '\n')
      ++e;
    *e = '\0';
    addzone(p);
  }
  fclose(f);
  return 1;
}

static void dnserror(struct rblookup *ipl, const char *what) {
  fprintf(stderr, "%s: unable to %s for %s (%s): %s\n",
          progname, what, inet_ntoa(ipl->key), ipl->zone,
          dns_strerror(dns_status(0)));
  ++failures;
}

static void display_result(struct ipcheck *ipc) {
  int j;
  struct rblookup *l, *le;
  if (!ipc->naddr) return;
  for (l = ipc->lookup, le = l + nzones * ipc->naddr; l < le; ++l) {
    if (!l->addr) continue;
    if (verbose < 2 && l->addr == notlisted) continue;
    if (verbose >= 0) {
      if (ipc->name) printf("%s[%s]", ipc->name, inet_ntoa(l->key));
      else printf("%s", inet_ntoa(l->key));
    }
    if (l->addr == notlisted) {
      printf(" is NOT listed by %s\n", l->zone);
      continue;
    }
    else if (verbose >= 1)
      printf(" is listed by %s: ", l->zone);
    else if (verbose >= 0)
      printf(" %s ", l->zone);
    if (verbose >= 1 || !do_txt)
      for (j = 0; j < l->addr->dnsa4_nrr; ++j)
        printf("%s%s", j ? " " : "", inet_ntoa(l->addr->dnsa4_addr[j]));
    if (!do_txt) ;
    else if (l->txt) {
      for(j = 0; j < l->txt->dnstxt_nrr; ++j) {
        unsigned char *t = l->txt->dnstxt_txt[j].txt;
        unsigned char *e = t + l->txt->dnstxt_txt[j].len;
        printf("%s\"", verbose > 0 ? "\n\t" : j ? " " : "");
        while(t < e) {
          if (*t < ' ' || *t >= 127) printf("\\x%02x", *t);
          else if (*t == '\\' || *t == '"') printf("\\%c", *t);
          else putchar(*t);
          ++t;
        }
        putchar('"');
      }
      free(l->txt);
    }
    else
      printf("%s<no text available>", verbose > 0 ? "\n\t" : "");
    free(l->addr);
    putchar('\n');
  }
  free(ipc->lookup);
}

static void txtcb(struct dns_ctx *ctx, struct dns_rr_txt *r, void *data) {
  struct rblookup *ipl = data;
  if (r) {
    ipl->txt = r;
    ++ipl->parent->listed;
  }
  else if (dns_status(ctx) != DNS_E_NXDOMAIN)
    dnserror(ipl, "lookup DNSBL TXT record");
}

static void a4cb(struct dns_ctx *ctx, struct dns_rr_a4 *r, void *data) {
  struct rblookup *ipl = data;
  if (r) {
    ipl->addr = r;
    ++listed;
    if (do_txt) {
      if (dns_submit_a4dnsbl_txt(0, &ipl->key, ipl->zone, txtcb, ipl))
        return;
      dnserror(ipl, "submit DNSBL TXT record");
    }
    ++ipl->parent->listed;
  }
  else if (dns_status(ctx) != DNS_E_NXDOMAIN)
    dnserror(ipl, "lookup DNSBL A record");
  else
    ipl->addr = notlisted;
}

static int
submit_a_queries(struct ipcheck *ipc,
                 int naddr, const struct in_addr *addr) {
  int z, a;
  struct rblookup *rl = ecalloc(sizeof(*rl), nzones * naddr);
  ipc->lookup = rl;
  ipc->naddr = naddr;
  for(a = 0; a < naddr; ++a) {
    for(z = 0; z < nzones; ++z) {
      rl->key = addr[a];
      rl->zone = zones[z];
      rl->parent = ipc;
      if (!dns_submit_a4dnsbl(0, &rl->key, rl->zone, a4cb, rl))
        dnserror(rl, "submit DNSBL A query");
      ++rl;
    }
  }
  return 0;
}

static void namecb(struct dns_ctx *ctx, struct dns_rr_a4 *rr, void *data) {
  struct ipcheck *ipc = data;
  if (rr) {
    submit_a_queries(ipc, rr->dnsa4_nrr, rr->dnsa4_addr);
    free(rr);
  }
  else
    fprintf(stderr, "%s: unable to lookup %s: %s\n",
            progname, ipc->name, dns_strerror(dns_status(ctx)));
}

static int submit(struct ipcheck *ipc) {
  struct in_addr addr;
  if (inet_aton(ipc->name, &addr)) {
    submit_a_queries(ipc, 1, &addr);
    ipc->name = NULL;
  }
  else if (!dns_submit_a4(0, ipc->name, 0, namecb, ipc))
    fprintf(stderr, "%s: unable to submit name query for %s: %s\n",
            progname, ipc->name, dns_strerror(dns_status(0)));
  return 0;
}

static void waitdns(struct ipcheck *ipc) {
  struct timeval tv;
  fd_set fds;
  int c;
  int fd = dns_sock(NULL);
  time_t now = 0;
  FD_ZERO(&fds);
  while((c = dns_timeouts(NULL, -1, now)) > 0) {
    FD_SET(fd, &fds);
    tv.tv_sec = c;
    tv.tv_usec = 0;
    c = select(fd+1, &fds, NULL, NULL, &tv);
    now = time(NULL);
    if (c > 0)
      dns_ioevent(NULL, now);
    if (stopfirst && ipc->listed)
      break;
  }
}

int main(int argc, char **argv) {
  int c;
  struct ipcheck ipc;
  char *nameserver = NULL;
  int zgiven = 0;

  if (!(progname = strrchr(argv[0], '/'))) progname = argv[0];
  else argv[0] = ++progname;

  while((c = getopt(argc, argv, "hqtvms:S:cn:")) != EOF) switch(c) {
  case 's': ++zgiven; addzone(optarg); break;
  case 'S':
    ++zgiven;
    if (addzonefile(optarg)) break;
    fprintf(stderr, "%s: unable to read %s\n", progname, optarg);
    return 1;
  case 'c': ++zgiven; nzones = 0; break;
  case 'q': --verbose; break;
  case 'v': ++verbose; break;
  case 't': do_txt = 1; break;
  case 'n': nameserver = optarg; break;
  case 'm': ++stopfirst; break;
  case 'h':
    printf("%s: %s.\n", progname, version);
    printf("Usage is: %s [options] address..\n", progname);
    printf(
"Where options are:\n"
" -h - print this help and exit\n"
" -s service - add the service (DNSBL zone) to the serice list\n"
" -S service-file - add the DNSBL zone(s) read from the given file\n"
" -c - clear service list\n"
" -v - increase verbosity level (more -vs => more verbose)\n"
" -q - decrease verbosity level (opposite of -v)\n"
" -t - obtain and print TXT records if any\n"
" -m - stop checking after first address match in any list\n"
" -n ipaddr - use the given nameserver instead of the default\n"
"(if no -s or -S option is given, use $RBLCHECK_ZONES, ~/.rblcheckrc\n"
"or /etc/rblcheckrc in that order)\n"
    );
    return 0;
  default:
    fprintf(stderr, "%s: use `%s -h' for help\n", progname, progname);
    return 1;
  }

  if (!zgiven) {
    char *s = getenv("RBLCHECK_ZONES");
    if (s) {
      char *k;
      s = strdup(s);
      k = strtok(s, " \t");
      while(k) {
        addzone(k);
        k = strtok(NULL, " \t");
      }
      free(s);
    }
    else {
      char *path;
      char *home = getenv("HOME");
      if (!home) home = ".";
      path = malloc(strlen(home) + 1 + sizeof(".rblcheckrc"));
      sprintf(path, "%s/.rblcheckrc", home);
      if (!addzonefile(path))
        addzonefile("/etc/rblcheckrc");
      free(path);
    }
  }
  if (!nzones) {
    fprintf(stderr, "%s: no service (zone) list specified (-s or -S option)\n",
            progname);
    return 1;
  }

  argv += optind;
  argc -= optind;

  if (!argc)
    return 0;

  if (dns_init(0) < 0) {
    fprintf(stderr, "%s: unable to initialize DNS library: %s\n",
            progname, strerror(errno));
    return 1;
  }
  if (nameserver) {
    dns_add_serv(NULL, NULL);
    if (dns_add_serv(NULL, nameserver) < 0)
      fprintf(stderr, "%s: unable to use nameserver %s: %s\n",
              progname, nameserver, strerror(errno));
  }
  if (dns_open(NULL) < 0) {
    fprintf(stderr, "%s: unable to initialize DNS library: %s\n",
            progname, strerror(errno));
    return 1;
  }

  for (c = 0; c < argc; ++c) {
    if (c && (verbose > 1 || (verbose == 1 && do_txt))) putchar('\n');
    ipc.name = argv[c];
    submit(&ipc);
    waitdns(&ipc);
    display_result(&ipc);
    if (stopfirst > 1 && listed) break;
  }

  return listed ? 100 : failures ? 2 : 0;
}
