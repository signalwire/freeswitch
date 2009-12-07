/* $Id: udns_resolver.c,v 1.57 2006/11/29 01:17:43 mjt Exp $
   resolver stuff (main module)

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

#ifdef WIN32
#ifdef _MSC_VER
#undef inline
#define inline __inline
#pragma warning(disable:4133)
#if (_MSC_VER >= 1400) // VC8+
#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif
#ifndef _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE
#endif
#endif // VC8+
int udns_inet_pton(int, const char *, void *);
#include "process.h"
#else
#define udns_inet_pton inet_pton
#endif
# include <winsock2.h>          /* includes <windows.h> */
# include <ws2tcpip.h>          /* needed for struct in6_addr */
# include <iphlpapi.h>		/* for dns server addresses etc */
# undef HAVE_POLL
#else
#define udns_inet_pton inet_pton
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>		/* for inet_pton() */
# include <unistd.h>
# include <fcntl.h>
# include <sys/time.h>
# ifdef HAVE_POLL
#  include <sys/poll.h>
# endif
# define closesocket(sock) close(sock)
#endif	/* !WIN32 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <stddef.h>
#include "udns.h"

#define DNS_QEXTRA	16	/* size of extra buffer space */
#define DNS_QBUF	DNS_HSIZE+DNS_MAXDN+DNS_QEXTRA

#if !defined(HAVE_INET6) && defined(AF_INET6)
# define HAVE_INET6 1
#endif
#ifdef NO_INET6
# undef HAVE_INET6
#endif

#ifndef EAFNOSUPPORT
# define EAFNOSUPPORT EINVAL
#endif

union usockaddr_ns {
  struct sockaddr sa;
  struct sockaddr_in sin;
#if HAVE_INET6
  struct sockaddr_in6 sin6;
#endif
};

struct dns_qlink {
  struct dns_query *next, *prev;
};

struct dns_query {
  struct dns_qlink dnsq_link;		/* list entry (should be first) */
  dnsc_t dnsq_buf[DNS_QBUF];		/* the query buffer */
  enum dns_class dnsq_cls;		/* requested RR class */
  enum dns_type  dnsq_typ;		/* requested RR type */
  unsigned dnsq_len;			/* length of the query packet */
  unsigned dnsq_origdnl;		/* original length of the dnsq_dn */
  unsigned dnsq_flags;			/* control flags for this query */
  unsigned dnsq_servi;			/* index of next server to try */
  unsigned dnsq_servwait;		/* bitmask: servers left to wait */
  unsigned dnsq_servskip;		/* bitmask: servers to skip */
  unsigned dnsq_try;			/* number of tries made so far */
  unsigned dnsq_srchi;			/* current search index */
  time_t dnsq_deadline;			/* when current try will expire */
  dns_parse_fn *dnsq_parse;		/* parse: raw => application */
  dns_query_fn *dnsq_cbck;		/* the callback to call when done */
  void *dnsq_cbdata;			/* user data for the callback */
#ifndef NDEBUG
  struct dns_ctx *dnsq_ctx;		/* the resolver context */
#endif
};

/* working with dns_query lists */

static inline void qlist_init(struct dns_qlink *list) {
  list->next = list->prev = (struct dns_query *)list;
}

static inline int qlist_empty(const struct dns_qlink *list) {
  return list->next == (const struct dns_query *)list ? 1 : 0;
}

static inline struct dns_query *qlist_first(struct dns_qlink *list) {
  return list->next == (struct dns_query *)list ? 0 : list->next;
}

static inline void qlist_remove(struct dns_query *q) {
  q->dnsq_link.next->dnsq_link.prev = q->dnsq_link.prev;
  q->dnsq_link.prev->dnsq_link.next = q->dnsq_link.next;
}

static inline struct dns_query *qlist_pop(struct dns_qlink *list) {
  struct dns_query *q = list->next;
  if (q == (struct dns_query *)list)
    return NULL;
  qlist_remove(q);
  return q;
}

/* insert q between prev and next */
static inline void
qlist_insert(struct dns_query *q,
             struct dns_query *prev, struct dns_query *next) {
  q->dnsq_link.next = next;
  q->dnsq_link.prev = prev;
  prev->dnsq_link.next = next->dnsq_link.prev = q;
}

static inline void
qlist_insert_after(struct dns_query *q, struct dns_query *prev) {
  qlist_insert(q, prev, prev->dnsq_link.next);
}

static inline void
qlist_insert_before(struct dns_query *q, struct dns_query *next) {
  qlist_insert(q, next->dnsq_link.prev, next);
}

static inline void
qlist_add_tail(struct dns_query *q, struct dns_qlink *top) {
  qlist_insert_before(q, (struct dns_query *)top);
}

static inline void
qlist_add_head(struct dns_query *q, struct dns_qlink *top) {
  qlist_insert_after(q, (struct dns_query *)top);
}

#define QLIST_FIRST(list, direction) ((list)->direction)
#define QLIST_ISLAST(list, q) ((q) == (struct dns_query*)(list))
#define QLIST_NEXT(q, direction) ((q)->dnsq_link.direction)

#define QLIST_FOR_EACH(list, q, direction) \
  for(q = QLIST_FIRST(list, direction); \
      !QLIST_ISLAST(list, q); q = QLIST_NEXT(q, direction))

struct dns_ctx {		/* resolver context */
  /* settings */
  unsigned dnsc_flags;			/* various flags */
  unsigned dnsc_timeout;		/* timeout (base value) for queries */
  unsigned dnsc_ntries;			/* number of retries */
  unsigned dnsc_ndots;			/* ndots to assume absolute name */
  unsigned dnsc_port;			/* default port (DNS_PORT) */
  unsigned dnsc_udpbuf;			/* size of UDP buffer */
  /* array of nameserver addresses */
  union usockaddr_ns dnsc_serv[DNS_MAXSERV];
  unsigned dnsc_nserv;			/* number of nameservers */
  unsigned dnsc_salen;			/* length of socket addresses */
  /* search list for unqualified names */
  dnsc_t dnsc_srch[DNS_MAXSRCH][DNS_MAXDN];
  unsigned dnsc_nsrch;			/* number of srch[] */

  dns_utm_fn *dnsc_utmfn;		/* register/cancel timer events */
  void *dnsc_utmctx;			/* user timer context for utmfn() */
  time_t dnsc_utmexp;			/* when user timer expires */

  dns_dbgfn *dnsc_udbgfn;		/* debugging function */

  /* dynamic data */
  unsigned short dnsc_nextid;		/* next queue ID to use */
  dns_socket dnsc_udpsock;			/* UDP socket */
  struct dns_qlink dnsc_qactive;	/* active list sorted by deadline */
  int dnsc_nactive;			/* number entries in dnsc_qactive */
  dnsc_t *dnsc_pbuf;			/* packet buffer (udpbuf size) */
  int dnsc_qstatus;			/* last query status value */
};

static const struct {
  const char *name;
  enum dns_opt opt;
  unsigned offset;
  unsigned min, max;
} dns_opts[] = {
#define opt(name,opt,field,min,max) \
	{name,opt,offsetof(struct dns_ctx,field),min,max}
  opt("retrans", DNS_OPT_TIMEOUT, dnsc_timeout, 1,300),
  opt("timeout", DNS_OPT_TIMEOUT, dnsc_timeout, 1,300),
  opt("retry",    DNS_OPT_NTRIES, dnsc_ntries, 1,50),
  opt("attempts", DNS_OPT_NTRIES, dnsc_ntries, 1,50),
  opt("ndots", DNS_OPT_NDOTS, dnsc_ndots, 0,1000),
  opt("port", DNS_OPT_PORT, dnsc_port, 1,0xffff),
  opt("udpbuf", DNS_OPT_UDPSIZE, dnsc_udpbuf, DNS_MAXPACKET,65536),
#undef opt
};
#define dns_ctxopt(ctx,offset) (*((unsigned*)(((char*)ctx)+offset)))

#define ISSPACE(x) (x == ' ' || x == '\t' || x == '\r' || x == '\n')

static const char space[] = " \t\r\n";

struct dns_ctx dns_defctx;

#define SETCTX(ctx) if (!ctx) ctx = &dns_defctx
#define SETCTXINITED(ctx) SETCTX(ctx); assert(CTXINITED(ctx))
#define CTXINITED(ctx) (ctx->dnsc_flags & DNS_INITED)
#define SETCTXFRESH(ctx) SETCTXINITED(ctx); assert(!CTXOPEN(ctx))
#define SETCTXINACTIVE(ctx) SETCTXINITED(ctx); assert(qlist_empty(&ctx->dnsc_qactive))
#define SETCTXOPEN(ctx) SETCTXINITED(ctx); assert(CTXOPEN(ctx))
#ifdef WIN32
#define CTXOPEN(ctx) (ctx->dnsc_udpsock != INVALID_SOCKET )
#else
#define CTXOPEN(ctx) (ctx->dnsc_udpsock >= 0)
#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif
#endif

#if defined(NDEBUG) || !defined(DEBUG)
#define dns_assert_ctx(ctx)
#else
static void dns_assert_ctx(const struct dns_ctx *ctx) {
  int nactive = 0;
  const struct dns_query *q;
  QLIST_FOR_EACH(&ctx->dnsc_qactive, q, next) {
    assert(q->dnsq_ctx == ctx);
    assert(q->dnsq_link.next->dnsq_link.prev == q);
    assert(q->dnsq_link.prev->dnsq_link.next == q);
    ++nactive;
  }
  assert(nactive == ctx->dnsc_nactive);
}
#endif

enum {
  DNS_INTERNAL		= 0xffff, /* internal flags mask */
  DNS_INITED		= 0x0001, /* the context is initialized */
  DNS_ASIS_DONE		= 0x0002, /* search: skip the last as-is query */
  DNS_SEEN_NODATA	= 0x0004, /* search: NODATA has been received */
  DNS_SEEN_FAIL		= 0x0008, /* search: SERVFAIL has been received */
  DNS_SEEN_WRONG	= 0x0010, /* search: something wrong happened */
};

static int dns_add_serv_internal(struct dns_ctx *ctx, const char *serv) {
  union usockaddr_ns *sns;
  if (!serv)
    return (ctx->dnsc_nserv = 0);
  if (ctx->dnsc_nserv >= DNS_MAXSERV)
    return errno = ENFILE, -1;
  sns = &ctx->dnsc_serv[ctx->dnsc_nserv];
  memset(sns, 0, sizeof(*sns));
#if HAVE_INET6
  { struct in_addr addr;
    struct in6_addr addr6;
    if (udns_inet_pton(AF_INET, serv, &addr) > 0) {
      sns->sin.sin_family = AF_INET;
      sns->sin.sin_addr = addr;
      return ++ctx->dnsc_nserv;
    }
    if (udns_inet_pton(AF_INET6, serv, &addr6) > 0) {
      sns->sin6.sin6_family = AF_INET6;
      sns->sin6.sin6_addr = addr6;
      return ++ctx->dnsc_nserv;
    }
  }
#else
  { struct in_addr addr;
    if (inet_aton(serv, &addr) > 0) {
      sns->sin.sin_family = AF_INET;
      sns->sin.sin_addr = addr;
      return ++ctx->dnsc_nserv;
    }
  }
#endif
  errno = EINVAL;
  return -1;
}

int dns_add_serv(struct dns_ctx *ctx, const char *serv) {
  SETCTXFRESH(ctx);
  return dns_add_serv_internal(ctx, serv);
}

static void dns_set_serv_internal(struct dns_ctx *ctx, char *serv) {
  ctx->dnsc_nserv = 0;
  for(serv = strtok(serv, space); serv; serv = strtok(NULL, space))
    dns_add_serv_internal(ctx, serv);
}

static int
dns_add_serv_s_internal(struct dns_ctx *ctx, const struct sockaddr *sa) {
  if (!sa)
    return (ctx->dnsc_nserv = 0);
  if (ctx->dnsc_nserv >= DNS_MAXSERV)
    return errno = ENFILE, -1;
#if HAVE_INET6
  else if (sa->sa_family == AF_INET6)
    ctx->dnsc_serv[ctx->dnsc_nserv].sin6 = *(struct sockaddr_in6*)sa;
#endif
  else if (sa->sa_family == AF_INET)
    ctx->dnsc_serv[ctx->dnsc_nserv].sin = *(struct sockaddr_in*)sa;
  else
    return errno = EAFNOSUPPORT, -1;
  return ++ctx->dnsc_nserv;
}

int dns_add_serv_s(struct dns_ctx *ctx, const struct sockaddr *sa) {
  SETCTXFRESH(ctx);
  return dns_add_serv_s_internal(ctx, sa);
}

static void dns_set_opts_internal(struct dns_ctx *ctx, const char *opts) {
  size_t i, v;
  for(;;) {
    while(ISSPACE(*opts)) ++opts;
    if (!*opts) break;
    for(i = 0; i < sizeof(dns_opts)/sizeof(dns_opts[0]); ++i) {
      v = strlen(dns_opts[i].name);
      if (strncmp(dns_opts[i].name, opts, v) != 0 ||
          (opts[v] != ':' && opts[v] != '='))
        continue;
      opts += v + 1;
      v = 0;
      if (*opts < '0' || *opts > '9') break;
      do v = v * 10 + (*opts++ - '0');
      while (*opts >= '0' && *opts <= '9');
      if (dns_opts[i].min && v < dns_opts[i].min) v = dns_opts[i].min;
      else if (v > dns_opts[i].max) v = dns_opts[i].max;
      dns_ctxopt(ctx, dns_opts[i].offset) = (unsigned)v;
      break;
    }
    while(*opts && !ISSPACE(*opts)) ++opts;
  }
}

int dns_set_opts(struct dns_ctx *ctx, const char *opts) {
  SETCTXINACTIVE(ctx);
  dns_set_opts_internal(ctx, opts);
  return 0;
}

int dns_set_opt(struct dns_ctx *ctx, enum dns_opt opt, int val) {
  int prev;
  unsigned i;
  SETCTXINACTIVE(ctx);
  for(i = 0; i < sizeof(dns_opts)/sizeof(dns_opts[0]); ++i) {
    if (dns_opts[i].opt != opt) continue;
    prev = dns_ctxopt(ctx, dns_opts[i].offset);
    if (val >= 0) {
      unsigned v = val;
      if (v < dns_opts[i].min || v > dns_opts[i].max) {
        errno = EINVAL;
        return -1;
      }
      dns_ctxopt(ctx, dns_opts[i].offset) = v;
    }
    return prev;
  }
  if (opt == DNS_OPT_FLAGS) {
    prev = ctx->dnsc_flags & ~DNS_INTERNAL;
    if (val >= 0)
      ctx->dnsc_flags =
        (ctx->dnsc_flags & DNS_INTERNAL) | (val & ~DNS_INTERNAL);
    return prev;
  }
  errno = ENOSYS;
  return -1;
}

static int dns_add_srch_internal(struct dns_ctx *ctx, const char *srch) {
  if (!srch)
    return (ctx->dnsc_nsrch = 0);
  else if (ctx->dnsc_nsrch >= DNS_MAXSRCH)
    return errno = ENFILE, -1;
  else if (dns_sptodn(srch, ctx->dnsc_srch[ctx->dnsc_nsrch], DNS_MAXDN) <= 0)
    return errno = EINVAL, -1;
  else
    return ++ctx->dnsc_nsrch;
}

int dns_add_srch(struct dns_ctx *ctx, const char *srch) {
  SETCTXINACTIVE(ctx);
  return dns_add_srch_internal(ctx, srch);
}

static void dns_set_srch_internal(struct dns_ctx *ctx, char *srch) {
  ctx->dnsc_nsrch = 0;
  for(srch = strtok(srch, space); srch; srch = strtok(NULL, space))
    dns_add_srch_internal(ctx, srch);
}

static void dns_drop_utm(struct dns_ctx *ctx) {
  if (ctx->dnsc_utmfn)
    ctx->dnsc_utmfn(NULL, -1, ctx->dnsc_utmctx);
  ctx->dnsc_utmctx = NULL;
  ctx->dnsc_utmexp = -1;
}

static void
dns_request_utm(struct dns_ctx *ctx, time_t now) {
  struct dns_query *q;
  time_t deadline;
  int timeout;
  if (!ctx->dnsc_utmfn)
    return;
  q = QLIST_FIRST(&ctx->dnsc_qactive, next);
  if (QLIST_ISLAST(&ctx->dnsc_qactive, q))
    deadline = -1, timeout = -1;
  else if (!now || q->dnsq_deadline <= now)
    deadline = 0, timeout = 0;
  else
    deadline = q->dnsq_deadline, timeout = (int)(deadline - now);
  if (ctx->dnsc_utmexp == deadline)
    return;
  ctx->dnsc_utmfn(ctx, timeout, ctx->dnsc_utmctx);
  ctx->dnsc_utmexp = deadline;
}

void dns_set_dbgfn(struct dns_ctx *ctx, dns_dbgfn *dbgfn) {
  SETCTXINITED(ctx);
  ctx->dnsc_udbgfn = dbgfn;
}

void
dns_set_tmcbck(struct dns_ctx *ctx, dns_utm_fn *fn, void *data) {
  SETCTXINITED(ctx);
  dns_drop_utm(ctx);
  ctx->dnsc_utmfn = fn;
  ctx->dnsc_utmctx = data;
}

#ifdef WIN32

typedef DWORD (WINAPI *GetAdaptersAddressesFunc)(
  ULONG Family, DWORD Flags, PVOID Reserved,
  PIP_ADAPTER_ADDRESSES pAdapterAddresses,
  PULONG pOutBufLen);

static int dns_initns_iphlpapi(struct dns_ctx *ctx) {
  HANDLE h_iphlpapi;
  GetAdaptersAddressesFunc pfnGetAdAddrs;
  PIP_ADAPTER_ADDRESSES pAddr, pAddrBuf;
  PIP_ADAPTER_DNS_SERVER_ADDRESS pDnsAddr;
  ULONG ulOutBufLen;
  DWORD dwRetVal;
  int ret = -1;

  h_iphlpapi = LoadLibrary("iphlpapi.dll");
  if (!h_iphlpapi)
    return -1;
  pfnGetAdAddrs = (GetAdaptersAddressesFunc)
    GetProcAddress(h_iphlpapi, "GetAdaptersAddresses");
  if (!pfnGetAdAddrs) goto freelib;
  ulOutBufLen = 0;
  dwRetVal = pfnGetAdAddrs(AF_UNSPEC, 0, NULL, NULL, &ulOutBufLen);
  if (dwRetVal != ERROR_BUFFER_OVERFLOW) goto freelib;
  pAddrBuf = malloc(ulOutBufLen);
  if (!pAddrBuf) goto freelib;
  dwRetVal = pfnGetAdAddrs(AF_UNSPEC, 0, NULL, pAddrBuf, &ulOutBufLen);
  if (dwRetVal != ERROR_SUCCESS) goto freemem;
  for (pAddr = pAddrBuf;
       pAddr && ctx->dnsc_nserv <= DNS_MAXSERV;
       pAddr = pAddr->Next)
    for (pDnsAddr = pAddr->FirstDnsServerAddress;
         pDnsAddr && ctx->dnsc_nserv <= DNS_MAXSERV;
         pDnsAddr = pDnsAddr->Next)
      dns_add_serv_s_internal(ctx, pDnsAddr->Address.lpSockaddr);
  ret = 0;
freemem:
  free(pAddrBuf);
freelib:
  FreeLibrary(h_iphlpapi);
  return ret;
}

static int dns_initns_registry(struct dns_ctx *ctx) {
  LONG res;
  HKEY hk;
  DWORD type = REG_EXPAND_SZ | REG_SZ;
  DWORD len;
  char valBuf[1024];

#define REGKEY_WINNT "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters"
#define REGKEY_WIN9x "SYSTEM\\CurrentControlSet\\Services\\VxD\\MSTCP"
  res = RegOpenKeyEx(HKEY_LOCAL_MACHINE, REGKEY_WINNT, 0, KEY_QUERY_VALUE, &hk);
  if (res != ERROR_SUCCESS)
    res = RegOpenKeyEx(HKEY_LOCAL_MACHINE, REGKEY_WIN9x,
                       0, KEY_QUERY_VALUE, &hk);
  if (res != ERROR_SUCCESS)
    return -1;
  len = sizeof(valBuf) - 1;
  res = RegQueryValueEx(hk, "NameServer", NULL, &type, valBuf, &len);
  if (res != ERROR_SUCCESS || !len || !valBuf[0]) {
    len = sizeof(valBuf) - 1;
    res = RegQueryValueEx(hk, "DhcpNameServer", NULL, &type, valBuf, &len);
  }
  RegCloseKey(hk);
  if (res != ERROR_SUCCESS || !len || !valBuf[0])
    return -1;
  valBuf[len] = '\0';
  /* nameservers are stored as a whitespace-seperate list:
   * "192.168.1.1 123.21.32.12" */
  dns_set_serv_internal(ctx, valBuf);
  return 0;
}

static int dns_init_internal(struct dns_ctx *ctx) {
  if (dns_initns_iphlpapi(ctx) != 0)
    dns_initns_registry(ctx);
  /*XXX WIN32: probably good to get default domain and search list too...
   * And options.  Something is in registry. */
  /*XXX WIN32: maybe environment variables are also useful? */
  return 0;
}

#ifdef _MSC_VER
#pragma warning(disable:4100)

#include "windows.h"

void gettimeofday(struct timeval *tv, void *tz)
{
    long int l = GetTickCount();
 
    tv->tv_sec = l / 1000;
    tv->tv_usec = (l % 1000) * 1000;
    return;
} 
#endif
#else /* !WIN32 */

static int dns_init_internal(struct dns_ctx *ctx) {
  char *v;
  char buf[2049];	/* this buffer is used to hold /etc/resolv.conf */

  /* read resolv.conf... */
  { int fd = open("/etc/resolv.conf", O_RDONLY);
    if (fd >= 0) {
      int l = read(fd, buf, sizeof(buf) - 1);
      close(fd);
      buf[l < 0 ? 0 : l] = '\0';
    }
    else
      buf[0] = '\0';
  }
  if (buf[0]) {	/* ...and parse it */
    char *line, *nextline;
    line = buf;
    do {
      nextline = strchr(line, '\n');
      if (nextline) *nextline++ = '\0';
      v = line;
      while(*v && !ISSPACE(*v)) ++v;
      if (!*v) continue;
      *v++ = '\0';
      while(ISSPACE(*v)) ++v;
      if (!*v) continue;
      if (strcmp(line, "domain") == 0)
        dns_set_srch_internal(ctx, strtok(v, space));
      else if (strcmp(line, "search") == 0)
        dns_set_srch_internal(ctx, v);
      else if (strcmp(line, "nameserver") == 0)
        dns_add_serv_internal(ctx, strtok(v, space));
      else if (strcmp(line, "options") == 0)
        dns_set_opts_internal(ctx, v);
    } while((line = nextline) != NULL);
  }

  buf[sizeof(buf)-1] = '\0';

  /* get list of nameservers from env. vars. */
  if ((v = getenv("NSCACHEIP")) != NULL ||
      (v = getenv("NAMESERVERS")) != NULL) {
    strncpy(buf, v, sizeof(buf) - 1);
    dns_set_serv_internal(ctx, buf);
  }
  /* if $LOCALDOMAIN is set, use it for search list */
  if ((v = getenv("LOCALDOMAIN")) != NULL) {
    strncpy(buf, v, sizeof(buf) - 1);
    dns_set_srch_internal(ctx, buf);
  }
  if ((v = getenv("RES_OPTIONS")) != NULL)
    dns_set_opts_internal(ctx, v);

  /* if still no search list, use local domain name */
  if (!ctx->dnsc_nsrch &&
      gethostname(buf, sizeof(buf) - 1) == 0 &&
      (v = strchr(buf, '.')) != NULL &&
      *++v != '\0')
    dns_add_srch_internal(ctx, v);

  return 0;
}

#endif /* dns_init_internal() for !WIN32 */

static void dns_firstid(struct dns_ctx *ctx) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  ctx->dnsc_nextid = (unsigned short)((tv.tv_usec ^ getpid()) & 0xffff);
}

dns_socket dns_init(int do_open) {
  struct dns_ctx *ctx = &dns_defctx;
  assert(!CTXINITED(ctx));
  memset(ctx, 0, sizeof(*ctx));
  ctx->dnsc_timeout = 4;
  ctx->dnsc_ntries = 3;
  ctx->dnsc_ndots = 1;
  ctx->dnsc_udpbuf = DNS_EDNS0PACKET;
  ctx->dnsc_port = DNS_PORT;
  ctx->dnsc_udpsock = INVALID_SOCKET;
  qlist_init(&ctx->dnsc_qactive);
  if (dns_init_internal(ctx) != 0)
    return -1;
  dns_firstid(ctx);
  ctx->dnsc_flags |= DNS_INITED;
  return do_open ? dns_open(ctx) : 0;
}

struct dns_ctx *dns_new(const struct dns_ctx *ctx) {
  struct dns_ctx *n;
  SETCTXINITED(ctx);
  dns_assert_ctx(ctx);
  n = malloc(sizeof(*n));
  if (!n)
    return NULL;
  *n = *ctx;
  n->dnsc_udpsock = INVALID_SOCKET;
  qlist_init(&n->dnsc_qactive);
  n->dnsc_nactive = 0;
  n->dnsc_pbuf = NULL;
  n->dnsc_qstatus = 0;
  n->dnsc_utmfn = NULL;
  n->dnsc_utmctx = NULL;
  dns_firstid(n);
  return n;
}

void dns_free(struct dns_ctx *ctx) {
  struct dns_query *q;
  SETCTXINITED(ctx);
  dns_assert_ctx(ctx);
  dns_drop_utm(ctx);
  if (ctx->dnsc_udpsock >= 0)
    closesocket(ctx->dnsc_udpsock);
  if (ctx->dnsc_pbuf)
    free(ctx->dnsc_pbuf);
  while((q = qlist_pop(&ctx->dnsc_qactive)))
    free(q);
  if (ctx != &dns_defctx)
    free(ctx);
  else
    memset(ctx, 0, sizeof(*ctx));
}

dns_socket dns_open(struct dns_ctx *ctx) {
  dns_socket sock;
  unsigned i;
  int port;
  union usockaddr_ns *sns;
#if HAVE_INET6
  unsigned have_inet6 = 0;
#endif

  SETCTXINITED(ctx);
  assert(!CTXOPEN(ctx));

  port = htons(ctx->dnsc_port);
  /* ensure we have at least one server */
  if (!ctx->dnsc_nserv) {
    sns = ctx->dnsc_serv;
    sns->sin.sin_family = AF_INET;
    sns->sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ctx->dnsc_nserv = 1;
  }

  for (i = 0; i < ctx->dnsc_nserv; ++i) {
    sns = &ctx->dnsc_serv[i];
    /* set port for each sockaddr */
#if HAVE_INET6
    if (sns->sa.sa_family == AF_INET6) {
      if (!sns->sin6.sin6_port) sns->sin6.sin6_port = port;
      ++have_inet6;
    }
    else
#endif
    {
      assert(sns->sa.sa_family == AF_INET);
      if (!sns->sin.sin_port) sns->sin.sin_port = port;
    }
  }

#if !HAVE_INET6
  ctx->dnsc_salen = sizeof(struct sockaddr_in);
#else
  if (have_inet6 && have_inet6 < ctx->dnsc_nserv) {
    /* convert all IPv4 addresses to IPv6 V4MAPPED */
    struct sockaddr_in6 sin6;
    memset(&sin6, 0, sizeof(sin6));
    sin6.sin6_family = AF_INET6;
    /* V4MAPPED: ::ffff:1.2.3.4 */
    sin6.sin6_addr.s6_addr[10] = 0xff;
    sin6.sin6_addr.s6_addr[11] = 0xff;
    for(i = 0; i < ctx->dnsc_nserv; ++i) {
      sns = &ctx->dnsc_serv[i];
      if (sns->sa.sa_family == AF_INET) {
        sin6.sin6_port = sns->sin.sin_port;
        memcpy(&sin6.sin6_addr.s6_addr[12], &sns->sin.sin_addr, sizeof(struct in_addr));
        sns->sin6 = sin6;
      }
    }
  }

  ctx->dnsc_salen = have_inet6 ?
    sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);

  if (have_inet6)
    sock = socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP);
  else
#endif /* HAVE_INET6 */
    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if (sock < 0) {
    ctx->dnsc_qstatus = DNS_E_TEMPFAIL;
    return -1;
  }
#ifdef WIN32
  { unsigned long on = 1;
    if (ioctlsocket(sock, FIONBIO, &on) == SOCKET_ERROR) {
      closesocket(sock);
      ctx->dnsc_qstatus = DNS_E_TEMPFAIL;
      return -1;
    }
  }
#else	/* !WIN32 */
  if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK) < 0 ||
      fcntl(sock, F_SETFD, FD_CLOEXEC) < 0) {
    closesocket(sock);
    ctx->dnsc_qstatus = DNS_E_TEMPFAIL;
    return -1;
  }
#endif	/* WIN32 */
  /* allocate the packet buffer */
  if (!(ctx->dnsc_pbuf = malloc(ctx->dnsc_udpbuf))) {
    closesocket(sock);
    ctx->dnsc_qstatus = DNS_E_NOMEM;
    errno = ENOMEM;
    return -1;
  }

  ctx->dnsc_udpsock = sock;
  return sock;
}

void dns_close(struct dns_ctx *ctx) {
  SETCTXINITED(ctx);
  if (ctx->dnsc_udpsock < 0) return;
  closesocket(ctx->dnsc_udpsock);
  ctx->dnsc_udpsock = INVALID_SOCKET;
  free(ctx->dnsc_pbuf);
  ctx->dnsc_pbuf = NULL;
}

dns_socket dns_sock(const struct dns_ctx *ctx) {
  SETCTXINITED(ctx);
  return ctx->dnsc_udpsock;
}

int dns_active(const struct dns_ctx *ctx) {
  SETCTXINITED(ctx);
  dns_assert_ctx(ctx);
  return ctx->dnsc_nactive;
}

int dns_status(const struct dns_ctx *ctx) {
  SETCTX(ctx);
  return ctx->dnsc_qstatus;
}
void dns_setstatus(struct dns_ctx *ctx, int status) {
  SETCTX(ctx);
  ctx->dnsc_qstatus = status;
}

/* End the query and return the result to the caller.
 */
static void
dns_end_query(struct dns_ctx *ctx, struct dns_query *q,
              int status, void *result) {
  dns_query_fn *cbck = q->dnsq_cbck;
  void *cbdata = q->dnsq_cbdata;
  ctx->dnsc_qstatus = status;
  assert((status < 0 && result == 0) || (status >= 0 && result != 0));
  assert(cbck != 0);	/*XXX callback may be NULL */
  assert(ctx->dnsc_nactive > 0);
  --ctx->dnsc_nactive;
  /* force the query to be unconnected */
  /*memset(q, 0, sizeof(*q));*/
#ifndef NDEBUG
  q->dnsq_ctx = NULL;
#endif
  free(q);
  cbck(ctx, result, cbdata);
}

#define DNS_DBG(ctx, code, sa, slen, pkt, plen) \
  do { \
    if (ctx->dnsc_udbgfn) \
      ctx->dnsc_udbgfn(code, (sa), slen, pkt, plen, 0, 0); \
  } while(0)
#define DNS_DBGQ(ctx, q, code, sa, slen, pkt, plen) \
  do { \
    if (ctx->dnsc_udbgfn) \
      ctx->dnsc_udbgfn(code, (sa), slen, pkt, plen, q, q->dnsq_cbdata); \
  } while(0)

/* Try next search, filling in qDN in query.
 * Return new qDN len or 0 if no more to search.
 * Caller should fill up the rest of the query.
 */
static unsigned dns_next_srch(const struct dns_ctx *ctx, struct dns_query *q) {
  unsigned ol = q->dnsq_origdnl - 1;	/* origdnl is at least 1 */
  dnsc_t *p = dns_payload(q->dnsq_buf) + ol;
  dnscc_t *dn;
  int n;
  while (q->dnsq_srchi < ctx->dnsc_nsrch) {
    dn = ctx->dnsc_srch[q->dnsq_srchi++];
    if (!*dn) {			/* root dn */
      if (!(q->dnsq_flags & DNS_ASIS_DONE))
        break;
    }
    else if ((n = dns_dntodn(dn, p, DNS_MAXDN - ol)) > 0)
      return n + ol;
  }
  if (q->dnsq_flags & DNS_ASIS_DONE)
    return 0;
  q->dnsq_flags |= DNS_ASIS_DONE;
  *p = '\0';
  return ol + 1;
}

/* find the next server which isn't skipped starting from current.
 * return 0 if ok, >0 if ok but we started next cycle, or <0 if
 * number of tries exceeded or no more servers.
 */
static int dns_find_serv(const struct dns_ctx *ctx, struct dns_query *q) {
  int cycle;
  if (q->dnsq_try < ctx->dnsc_ntries) for(cycle = 0;;) {
    if (q->dnsq_servi < ctx->dnsc_nserv) {
      if (!(q->dnsq_servskip & (1 << q->dnsq_servi)))
        return cycle;
      ++q->dnsq_servi;
    }
    else if (cycle || ++q->dnsq_try >= ctx->dnsc_ntries)
      break;
    else {
      cycle = 1;
      q->dnsq_servi = 0;
    }
  }
  return -1;
}

/* send the query out and add it to the active list. */
static void dns_send(struct dns_ctx *ctx, struct dns_query *q, time_t now) {
  int n;
  struct dns_query *p;

  /* if we can't send the query, return TEMPFAIL even when searching:
   * we can't be sure whenever the name we tried to search exists or not,
   * so don't continue searching, or we may find the wrong name. */

  /* if there's no more servers, fail the query */
  n = dns_find_serv(ctx, q);
  if (n < 0) {
    dns_end_query(ctx, q, DNS_E_TEMPFAIL, 0);
    return;
  }

  /* send the query */
  n = 10;
  while (sendto(ctx->dnsc_udpsock, q->dnsq_buf, q->dnsq_len, 0,
                &ctx->dnsc_serv[q->dnsq_servi].sa, ctx->dnsc_salen) < 0) {
    /*XXX just ignore the sendto() error for now and try again.
     * In the future, it may be possible to retrieve the error code
     * and find which operation/query failed.
     *XXX try the next server too?
     */
    if (--n) continue;
    /* if we can't send the query, fail it. */
    dns_end_query(ctx, q, DNS_E_TEMPFAIL, 0);
    return;
  }
  DNS_DBGQ(ctx, q, 1,
           &ctx->dnsc_serv[q->dnsq_servi].sa, sizeof(union usockaddr_ns),
           q->dnsq_buf, q->dnsq_len);
  q->dnsq_servwait |= 1 << q->dnsq_servi;	/* expect reply from this ns */

  /* advance to the next server, and choose a timeout.
   * we will try next server in 1 secound, but start next
   * cycle waiting for proper timeout. */
  ++q->dnsq_servi;
  n = dns_find_serv(ctx, q) ? ctx->dnsc_timeout << (q->dnsq_try - 1) : 1;

  q->dnsq_deadline = now = now + n;

  /* insert the query to the tail of the list */
  QLIST_FOR_EACH(&ctx->dnsc_qactive, p, prev)
    if (p->dnsq_deadline <= now)
      break;
  qlist_insert_after(q, p);

}

static void dns_dummy_cb(struct dns_ctx *ctx, void *result, void *data) {
  if (result) free(result);
  data = ctx = 0;	/* used */
}

struct dns_query *
dns_submit_dn(struct dns_ctx *ctx,
              dnscc_t *dn, int qcls, int qtyp, int flags,
              dns_parse_fn *parse, dns_query_fn *cbck, void *data) {
  dnsc_t *p;
  unsigned dnl;
  struct dns_query *q;
  SETCTXOPEN(ctx);
  dns_assert_ctx(ctx);

  q = calloc(sizeof(*q), 1);
  if (!q) {
    ctx->dnsc_qstatus = DNS_E_NOMEM;
    return NULL;
  }

#ifndef NDEBUG
  q->dnsq_ctx = ctx;
#endif
  q->dnsq_parse = parse;
  q->dnsq_cbck = cbck ? cbck : dns_dummy_cb;
  q->dnsq_cbdata = data;

  flags = (flags | ctx->dnsc_flags) & ~DNS_INTERNAL;
  if (!ctx->dnsc_nsrch) q->dnsq_flags |= DNS_NOSRCH;
  if (!(flags & DNS_NORD)) q->dnsq_buf[DNS_H_F1] |= DNS_HF1_RD;
  if (flags & DNS_AAONLY) q->dnsq_buf[DNS_H_F1] |= DNS_HF1_AA;
  q->dnsq_buf[DNS_H_QDCNT2] = 1;
  dns_put16(q->dnsq_buf + DNS_H_QID, ctx->dnsc_nextid++);

  q->dnsq_origdnl = dns_dnlen(dn);
  assert(q->dnsq_origdnl > 0 && q->dnsq_origdnl <= DNS_MAXDN);
  memcpy(dns_payload(q->dnsq_buf), dn, q->dnsq_origdnl);
  p = dns_payload(q->dnsq_buf) + q->dnsq_origdnl;
  if (flags & DNS_NOSRCH || dns_dnlabels(dn) > ctx->dnsc_ndots)
    flags |= DNS_ASIS_DONE;
  else if ((dnl = dns_next_srch(ctx, q)) > 0)
    p = dns_payload(q->dnsq_buf) + dnl;
  else
    p[-1] = '\0';
  q->dnsq_flags = flags;
  q->dnsq_typ = qtyp;
  p = dns_put16(p, qtyp);
  q->dnsq_cls = qcls;
  p = dns_put16(p, qcls);
  if (ctx->dnsc_udpbuf > DNS_MAXPACKET) {
    p++;			/* empty (root) DN */
    p = dns_put16(p, DNS_T_OPT);
    p = dns_put16(p, ctx->dnsc_udpbuf);
    p += 2;		/* EDNS0 RCODE & VERSION */
    p += 2;		/* rest of the TTL field */
    p += 2;		/* RDLEN */
    q->dnsq_buf[DNS_H_ARCNT2] = 1;
  }
  assert(p <= q->dnsq_buf + DNS_QBUF);
  q->dnsq_len = (unsigned)(p - q->dnsq_buf);

  qlist_add_head(q, &ctx->dnsc_qactive);
  ++ctx->dnsc_nactive;
  dns_request_utm(ctx, 0);

  return q;
}

struct dns_query *
dns_submit_p(struct dns_ctx *ctx,
             const char *name, int qcls, int qtyp, int flags,
             dns_parse_fn *parse, dns_query_fn *cbck, void *data) {
  int isabs;
  SETCTXOPEN(ctx);
  if (dns_ptodn(name, 0, ctx->dnsc_pbuf, DNS_MAXDN, &isabs) <= 0) {
    ctx->dnsc_qstatus = DNS_E_BADQUERY;
    return NULL;
  }
  if (isabs)
    flags |= DNS_NOSRCH;
  return
    dns_submit_dn(ctx, ctx->dnsc_pbuf, qcls, qtyp, flags, parse, cbck, data);
}

/* process readable fd condition.
 * To be usable in edge-triggered environment, the routine
 * should consume all input so it should loop over.
 * Note it isn't really necessary to loop here, because
 * an application may perform the loop just fine by it's own,
 * but in this case we should return some sensitive result,
 * to indicate when to stop calling and error conditions.
 * Note also we may encounter all sorts of recvfrom()
 * errors which aren't fatal, and at the same time we may
 * loop forever if an error IS fatal.
 * Current loop/goto looks just terrible... */
void dns_ioevent(struct dns_ctx *ctx, time_t now) {
  int r;
  unsigned servi, l;
  struct dns_query *q;
  dnsc_t *pbuf;
  dnscc_t *pend, *pcur;
  void *result;
  union usockaddr_ns sns;
  socklen_t slen;

  SETCTX(ctx);
  if (!CTXOPEN(ctx))
    return;
  dns_assert_ctx(ctx);
  pbuf = ctx->dnsc_pbuf;

  if (!now) now = time(NULL);

again:

  for(;;) { /* receive the reply */
    dnsc_t dn[DNS_MAXDN];

    slen = sizeof(sns);
    r = recvfrom(ctx->dnsc_udpsock, pbuf, ctx->dnsc_udpbuf, 0, &sns.sa, &slen);
    if (r < 0) {
      /*XXX just ignore recvfrom() errors for now.
       * in the future it may be possible to determine which
       * query failed and requeue it.
       * Note there may be various error conditions, triggered
       * by both local problems and remote problems.  It isn't
       * quite trivial to determine whenever an error is local
       * or remote.  On local errors, we should stop, while
       * remote errors should be ignored (for now anyway).
       */
#ifdef WIN32
      if (WSAGetLastError() == WSAEWOULDBLOCK)
#else
      if (errno == EAGAIN)
#endif
      {
        dns_request_utm(ctx, now);
	return;
      }
      continue;
    }
    /* ignore replies from wrong server */
#if HAVE_INET6
    if (sns.sa.sa_family == AF_INET6 && slen >= sizeof(sns.sin6)) {
      for (servi = 0; servi < ctx->dnsc_nserv; ++servi)
        if (ctx->dnsc_serv[servi].sin6.sin6_port == sns.sin6.sin6_port &&
            memcmp(&ctx->dnsc_serv[servi].sin6.sin6_addr,
                   &sns.sin6.sin6_addr, sizeof(sns.sin6.sin6_addr)) == 0)
          break;
    }
    else
#endif
    if (sns.sa.sa_family == AF_INET && slen >= sizeof(sns.sin)) {
      for (servi = 0; servi < ctx->dnsc_nserv; ++servi)
        if (ctx->dnsc_serv[servi].sin.sin_addr.s_addr == sns.sin.sin_addr.s_addr &&
            ctx->dnsc_serv[servi].sin.sin_port == sns.sin.sin_port)
          break;
    }
    else {
      DNS_DBG(ctx, -1, &sns.sa, slen, pbuf, r);
      continue;
    }
    if (servi >= ctx->dnsc_nserv) {
      DNS_DBG(ctx, -2, &sns.sa, slen, pbuf, r);
      continue;
    }

    pend = pbuf + r;
    pcur = dns_payload(pbuf);
    if (pcur >= pend || dns_numqd(pbuf) != 1 || dns_opcode(pbuf) != 0 ||
        dns_getdn(pbuf, &pcur, pend, dn, sizeof(dn)) < 0 ||
        pcur + 4 > pend) {
      /*XXX ignore non-query replies and replies with numqd!=1? */
      DNS_DBG(ctx, -3, &sns.sa, slen, pbuf, r);
      continue;
    }

    /* truncation bit (TC).  Ooh, we don't handle TCP (yet?),
     * but we do handle larger UDP sizes.
     * Note that e.g. djbdns will only send header if resp.
     * does not fit, not whatever is fit in 512 bytes. */
    if (dns_tc(pbuf)) {
      DNS_DBG(ctx, -4, &sns.sa, slen, pbuf, r);
      continue;	/* just ignore response for now.. any hope? */
    }

    /* find the request for this reply in active queue
     * Note we pick any request, even queued for another
     * server - in case first server replies a bit later
     * than we expected. */
    for (q = QLIST_FIRST(&ctx->dnsc_qactive, next);; q = QLIST_NEXT(q, next)) {
      if (QLIST_ISLAST(&ctx->dnsc_qactive, q)) {
        /* no more requests: old reply? */
        DNS_DBG(ctx, -5, &sns.sa, slen, pbuf, r);
        goto again;
      }
      /* ignore replies that has not been sent to this server.
       * Note dnsq_servi is the *next* server to try. */
      if (!q->dnsq_try && q->dnsq_servi <= servi)
        continue;
      /*XXX ignore replies from servers we're ignoring? o/
      if (q->dnsq_servskip & (1 << servi))
        continue; */
      /* check qID */
      if (q->dnsq_buf[DNS_H_QID1] != pbuf[DNS_H_QID1] ||
          q->dnsq_buf[DNS_H_QID2] != pbuf[DNS_H_QID2])
        continue;
      /* check qDN, qCLS and qTYP */
      if (!(l = dns_dnequal(dn, dns_payload(q->dnsq_buf))) ||
          memcmp(pcur, dns_payload(q->dnsq_buf) + l, 4) != 0)
        continue;
      /* ok, this is expected reply with matching query. */
      break;
    }

    break;

  }

  DNS_DBGQ(ctx, q, 0, &sns.sa, slen, pbuf, r);

  /* we got a reply for our query */
  q->dnsq_servwait &= ~(1 << servi);	/* don't expect reply from this serv */

  /* process the RCODE */
  switch(dns_rcode(pbuf)) {

  case DNS_R_NOERROR:
    qlist_remove(q);
    if (!dns_numan(pbuf)) {	/* no data of requested type */
      q->dnsq_flags |= DNS_SEEN_NODATA;
      r = DNS_E_NODATA;
      break;
    }
    /* the only case where we may succeed */
    if (q->dnsq_parse) {
      r = q->dnsq_parse(dns_payload(q->dnsq_buf), pbuf, pcur, pend, &result);
      if (r < 0)
        result = NULL;
    }
    else if ((result = malloc(r)) != NULL)
      memcpy(result, pbuf, r);
    else
      r = DNS_E_NOMEM;
    /* (maybe) successeful answer (modulo nomem and parsing probs) */
    /* note we pass DNS_E_NODATA here */
    dns_end_query(ctx, q, r, result);
    goto again;

  case DNS_R_NXDOMAIN:
    qlist_remove(q);
    r = DNS_E_NXDOMAIN;
    break;

  case DNS_R_SERVFAIL:
    q->dnsq_flags |= DNS_SEEN_FAIL;
  case DNS_R_NOTIMPL:
  case DNS_R_REFUSED:
    /* for these rcodes, advance this request
     * to the next server and reschedule */
  default: /* unknown rcode? hmmm... */
    /* try next server */
    q->dnsq_servskip |= 1 << servi;	/* don't retry this server */
    if (!q->dnsq_servwait) {
      qlist_remove(q);
      dns_send(ctx, q, now);
    }
    else {
      /* else this is the only place where q will be left unconnected
       * if we will move qlist_remove() before the switch{}. */
    }
    goto again;

  }

  /* here we have either NODATA or NXDOMAIN */
  if (!(q->dnsq_flags & DNS_NOSRCH)) {
    /* try next element from search list */
    unsigned sl;

    l = dns_dnlen(dns_payload(q->dnsq_buf)) + DNS_HSIZE;	/* past qDN */
    /* save qcls, qtyp and EDNS0 stuff (of len sl) in pbuf */
    sl = q->dnsq_len - l;
    memcpy(pbuf, q->dnsq_buf + l, sl);
    /* try next search list */
    l = dns_next_srch(ctx, q);
    if (l) {	/* something else to try, of len l */
      l += DNS_HSIZE;
      memcpy(q->dnsq_buf + l, pbuf, sl);
      q->dnsq_len = l + sl;
      q->dnsq_try = 0; q->dnsq_servi = 0;
      q->dnsq_servwait = q->dnsq_servskip = 0;
      dns_send(ctx, q, now);
      goto again;
    }
    /* else we have nothing more to search, end the query. */
    if (q->dnsq_flags & DNS_SEEN_FAIL)
      /* at least one server/query failed, fail the query */
      r = DNS_E_TEMPFAIL;
    else if (q->dnsq_flags & DNS_SEEN_NODATA)
      /* for one domain we have seen NODATA, return it */
      r = DNS_E_NODATA;
    else /* else all should be NXDOMAINs */
      r = DNS_E_NXDOMAIN;
  }

  dns_end_query(ctx, q, r, 0);
  goto again;
}

/* handle all timeouts */
int dns_timeouts(struct dns_ctx *ctx, int maxwait, time_t now) {
  struct dns_query *q;
  int w;
  SETCTX(ctx);
  dns_assert_ctx(ctx);
  if (!now) now = time(NULL);
  while((q = qlist_first(&ctx->dnsc_qactive)) && q->dnsq_deadline <= now) {
    qlist_remove(q);
    dns_send(ctx, q, now);
  }
  dns_request_utm(ctx, now);
  if (!q)
    return maxwait;
  w = (int)(q->dnsq_deadline - now);
  return maxwait < 0 || maxwait > w ? w : maxwait;
}

struct dns_resolve_data {
  int   dnsrd_done;
  void *dnsrd_result;
};

static void dns_resolve_cb(struct dns_ctx *ctx, void *result, void *data) {
  struct dns_resolve_data *d = data;
  d->dnsrd_result = result;
  d->dnsrd_done = 1;
  ctx = ctx;
}

void *dns_resolve(struct dns_ctx *ctx, struct dns_query *q) {
  time_t now;
#ifdef HAVE_POLL
  struct pollfd pfd;
#else
  fd_set rfd;
  struct timeval tv;
#endif
  struct dns_resolve_data d;
  int n;
  SETCTXOPEN(ctx);

  if (!q)
    return NULL;

  assert(ctx == q->dnsq_ctx);
  dns_assert_ctx(ctx);
  /* do not allow re-resolving syncronous queries */
  assert(q->dnsq_cbck != dns_resolve_cb && "can't resolve syncronous query");
  if (q->dnsq_cbck == dns_resolve_cb) {
    ctx->dnsc_qstatus = DNS_E_BADQUERY;
    return NULL;
  }
  q->dnsq_cbck = dns_resolve_cb;
  q->dnsq_cbdata = &d;
  d.dnsrd_done = 0;

#ifdef HAVE_POLL
  pfd.fd = ctx->dnsc_udpsock;
  pfd.events = POLLIN;
#else
  FD_ZERO(&rfd);
#endif

  now = time(NULL);
  while(!d.dnsrd_done && (n = dns_timeouts(ctx, -1, now)) >= 0) {
#ifdef HAVE_POLL
    n = poll(&pfd, 1, n * 1000);
#else
    tv.tv_sec = n;
    tv.tv_usec = 0;
    FD_SET(ctx->dnsc_udpsock, &rfd);
    n = select((int)(ctx->dnsc_udpsock + 1), &rfd, NULL, NULL, &tv);
#endif
    now = time(NULL);
    if (n > 0)
      dns_ioevent(ctx, now);
  }

  return d.dnsrd_result;
}

void *dns_resolve_dn(struct dns_ctx *ctx,
                     dnscc_t *dn, int qcls, int qtyp, int flags,
                     dns_parse_fn *parse) {
  return
    dns_resolve(ctx,
      dns_submit_dn(ctx, dn, qcls, qtyp, flags, parse, NULL, NULL));
}

void *dns_resolve_p(struct dns_ctx *ctx,
                    const char *name, int qcls, int qtyp, int flags,
                    dns_parse_fn *parse) {
  return
    dns_resolve(ctx,
      dns_submit_p(ctx, name, qcls, qtyp, flags, parse, NULL, NULL));
}

int dns_cancel(struct dns_ctx *ctx, struct dns_query *q) {
  SETCTX(ctx);
  dns_assert_ctx(ctx);
  assert(q->dnsq_ctx == ctx);
  /* do not allow cancelling syncronous queries */
  assert(q->dnsq_cbck != dns_resolve_cb && "can't cancel syncronous query");
  if (q->dnsq_cbck == dns_resolve_cb)
    return (ctx->dnsc_qstatus = DNS_E_BADQUERY);
  qlist_remove(q);
  --ctx->dnsc_nactive;
  dns_request_utm(ctx, 0);
  return 0;
}

