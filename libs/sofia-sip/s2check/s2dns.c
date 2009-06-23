/* ---------------------------------------------------------------------- */
/* S2 DNS server */

#include <sofia-sip/sresolv.h>
#include <sofia-resolv/sres_record.h>
#include <sofia-sip/url.h>

#include "s2dns.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>

extern uint16_t _sres_default_port; /* Ugly hack */

static struct s2dns {
  su_root_t *root;
  su_socket_t socket;
  su_wait_t wait[1];
  int reg;
  int (*filter)(void *data, size_t len, void *userdata);
  void *userdata;
} s2dns;

static
struct s2_dns_response {
  struct s2_dns_response *next;
  uint16_t qlen, dlen;
  struct m_header {
    /* Header defined in RFC 1035 section 4.1.1 (page 26) */
    uint16_t mh_id;		/* Query ID */
    uint16_t mh_flags;		/* Flags */
    uint16_t mh_qdcount;	/* Question record count */
    uint16_t mh_ancount;	/* Answer record count */
    uint16_t mh_nscount;	/* Authority records count */
    uint16_t mh_arcount;	/* Additional records count */
  } header[1];
  uint8_t data[1500];
} *zonedata;

enum {
  FLAGS_QR = (1 << 15),
  FLAGS_QUERY = (0 << 11),
  FLAGS_IQUERY = (1 << 11),
  FLAGS_STATUS = (2 << 11),
  FLAGS_OPCODE = (15 << 11),	/* mask */
  FLAGS_AA = (1 << 10),		/*  */
  FLAGS_TC = (1 << 9),
  FLAGS_RD = (1 << 8),
  FLAGS_RA = (1 << 7),

  FLAGS_RCODE = (15 << 0),	/* mask of return code */

  FLAGS_OK = 0,			/* No error condition. */
  FLAGS_FORMAT_ERR = 1,		/* Server could not interpret query. */
  FLAGS_SERVER_ERR = 2,		/* Server error. */
  FLAGS_NAME_ERR = 3,		/* No domain name. */
  FLAGS_UNIMPL_ERR = 4,		/* Not implemented. */
  FLAGS_AUTH_ERR = 5,		/* Refused */
};

uint32_t s2_dns_ttl = 3600;

static int s2_dns_query(su_root_magic_t *magic,
			su_wait_t *w,
			su_wakeup_arg_t *arg);

void s2_dns_setup(su_root_t *root)
{
  int n;
  su_socket_t socket;
  su_wait_t *wait;
  su_sockaddr_t su[1];
  socklen_t sulen = sizeof su->su_sin;

  assert(root);

  memset(su, 0, sulen);
  su->su_len = sulen;
  su->su_family = AF_INET;

  /* su->su_port = htons(1053); */

  socket = su_socket(su->su_family, SOCK_DGRAM, 0);

  n = bind(socket, &su->su_sa, sulen); assert(n == 0);
  n = getsockname(socket, &su->su_sa, &sulen); assert(n == 0);

  _sres_default_port = ntohs(su->su_port);

  s2dns.root = root;
  wait = s2dns.wait;
  n = su_wait_create(wait, socket, SU_WAIT_IN); assert(n == 0);
  s2dns.reg = su_root_register(root, wait, s2_dns_query, NULL, 0);
  assert(s2dns.reg > 0);
  s2dns.socket = socket;
}

/* Set filter function */
void
s2_dns_set_filter(int (*filter)(void *data, size_t len, void *userdata),
		       void *userdata)
{
  s2dns.filter = filter;
  s2dns.userdata = userdata;
}

void
s2_dns_teardown(void)
{
  struct s2_dns_response *r, *next;
  su_root_deregister(s2dns.root, s2dns.reg), s2dns.reg = -1;
  su_close(s2dns.socket), s2dns.socket = -1;
  s2dns.root = NULL;

  for (r = zonedata, zonedata = NULL; r; r = next) {
    next = r->next;
    free(r);
  }
}

static int
s2_dns_query(su_root_magic_t *magic,
	     su_wait_t *w,
	     su_wakeup_arg_t *arg)
{
  union {
    struct m_header header[1];
    uint8_t buffer[1500];
  } request;
  ssize_t len;

  su_socket_t socket;
  su_sockaddr_t su[1];
  socklen_t sulen = sizeof su;
  uint16_t flags;
  struct s2_dns_response *r;
  size_t const hlen = sizeof r->header;

  (void)arg;

  socket = s2dns.socket;

  len = su_recvfrom(socket, request.buffer, sizeof request.buffer, 0,
		    &su->su_sa, &sulen);

  flags = ntohs(request.header->mh_flags);

  if (len < (ssize_t)hlen)
    return 0;
  if ((flags & FLAGS_QR) == FLAGS_QR)
    return 0;
  if ((flags & FLAGS_RCODE) != FLAGS_OK)
    return 0;

  if ((flags & FLAGS_OPCODE) != FLAGS_QUERY
      || ntohs(request.header->mh_qdcount) != 1) {
    flags |= FLAGS_QR | FLAGS_UNIMPL_ERR;
    request.header->mh_flags = htons(flags);
    if (!s2dns.filter || s2dns.filter(request.buffer, len, s2dns.userdata))
      su_sendto(socket, request.buffer, len, 0, &su->su_sa, sulen);
    return 0;
  }

  for (r = zonedata; r; r = r->next) {
    if (memcmp(r->data, request.buffer + hlen, r->qlen) == 0)
      break;
  }

  if (r) {
    flags |= FLAGS_QR | FLAGS_AA | FLAGS_OK;
    request.header->mh_flags = htons(flags);
    request.header->mh_ancount = htons(r->header->mh_ancount);
    request.header->mh_nscount = htons(r->header->mh_nscount);
    request.header->mh_arcount = htons(r->header->mh_arcount);
    memcpy(request.buffer + hlen + r->qlen,
	   r->data + r->qlen,
	   r->dlen - r->qlen);
    len = hlen + r->dlen;
  }
  else {
    flags |= FLAGS_QR | FLAGS_AA | FLAGS_NAME_ERR;
  }

  request.header->mh_flags = htons(flags);
  if (!s2dns.filter || s2dns.filter(request.buffer, len, s2dns.userdata))
    su_sendto(socket, request.buffer, len, 0, &su->su_sa, sulen);
  return 0;
}

static char const *default_domain;

/** Set default domain suffix used with s2_dns_record() */
char const *
s2_dns_default(char const *domain)
{
  assert(domain == NULL || strlen(domain));
  assert(domain == NULL || domain[strlen(domain) - 1] == '.');
  return default_domain = domain;
}

static void put_uint16(struct s2_dns_response *m, uint16_t h)
{
  uint8_t *p = m->data + m->dlen;

  assert(m->dlen + (sizeof h) < sizeof m->data);
  p[0] = h >> 8; p[1] = h;
  m->dlen += (sizeof h);
}

static void put_uint32(struct s2_dns_response *m, uint32_t w)
{
  uint8_t *p = m->data + m->dlen;

  assert(m->dlen + (sizeof w) < sizeof m->data);
  p[0] = w >> 24; p[1] = w >> 16; p[2] = w >> 8; p[3] = w;
  m->dlen += (sizeof w);
}

static void put_domain(struct s2_dns_response *m, char const *domain)
{
  char const *label;
  size_t llen;

  if (domain && domain[0] == 0)
    domain = default_domain;

  /* Copy domain into query label at a time */
  for (label = domain; label && label[0]; label += llen) {
    assert(!(label[0] == '.' && label[1] != '\0'));
    llen = strcspn(label, ".");
    assert(llen < 64);
    assert(m->dlen + llen + 1 < sizeof m->data);
    m->data[m->dlen++] = (uint8_t)llen;
    if (llen == 0)
      return;

    memcpy(m->data + m->dlen, label, llen);
    m->dlen += (uint16_t)llen;

    if (label[llen] == '\0') {
      if (default_domain) {
	label = default_domain, llen = 0;
	continue;
      }
      break;
    }
    if (label[llen + 1])
      llen++;
  }

  assert(m->dlen < sizeof m->data);
  m->data[m->dlen++] = '\0';
}

static void put_string(struct s2_dns_response *m, char const *string)
{
  uint8_t *p = m->data + m->dlen;
  size_t len = strlen(string);

  assert(len <= 255);
  assert(m->dlen + len + 1 < sizeof m->data);

  *p++ = (uint8_t)len;
  memcpy(p, string, len);
  m->dlen += len + 1;
}

static uint16_t put_len_at(struct s2_dns_response *m)
{
  uint16_t at = m->dlen;
  assert(m->dlen + sizeof(at) < sizeof m->data);
  memset(m->data + m->dlen, 0, sizeof(at));
  m->dlen += sizeof(at);
  return at;
}

static void put_len(struct s2_dns_response *m, uint16_t start)
{
  uint8_t *p = m->data + start;
  uint16_t len = m->dlen - (start + 2);
  p[0] = len >> 8; p[1] = len;
}

static void put_data(struct s2_dns_response *m, void const *data, uint16_t len)
{
  assert(m->dlen + len < sizeof m->data);
  memcpy(m->data + m->dlen, data, len);
  m->dlen += len;
}

static void put_query(struct s2_dns_response *m, char const *domain,
		      uint16_t qtype)
{
  assert(m->header->mh_qdcount == 0);
  put_domain(m, domain), put_uint16(m, qtype), put_uint16(m, sres_class_in);
  m->header->mh_qdcount++;
  m->qlen = m->dlen;
}

static void put_a_record(struct s2_dns_response *m,
			 char const *domain,
			 struct in_addr addr)
{
  uint16_t start;

  put_domain(m, domain);
  put_uint16(m, sres_type_a);
  put_uint16(m, sres_class_in);
  put_uint32(m, s2_dns_ttl);
  start = put_len_at(m);

  put_data(m, &addr, sizeof addr);
  put_len(m, start);
}

static void put_aaaa_record(struct s2_dns_response *m,
			    char const *domain,
			    struct in6_addr addr)
{
  uint16_t start;

  put_domain(m, domain);
  put_uint16(m, sres_type_aaaa);
  put_uint16(m, sres_class_in);
  put_uint32(m, s2_dns_ttl);
  start = put_len_at(m);

  put_data(m, &addr, sizeof addr);
  put_len(m, start);
}

static void put_cname_record(struct s2_dns_response *m,
			     char const *domain,
			     char const *cname)
{
  uint16_t start;

  put_domain(m, domain);
  put_uint16(m, sres_type_cname);
  put_uint16(m, sres_class_in);
  put_uint32(m, s2_dns_ttl);
  start = put_len_at(m);

  put_domain(m, cname);
  put_len(m, start);
}

static void put_srv_record(struct s2_dns_response *m,
			   char const *domain,
			   uint16_t prio, uint16_t weight,
			   uint16_t port, char const *target)
{
  uint16_t start;
  put_domain(m, domain);
  put_uint16(m, sres_type_srv);
  put_uint16(m, sres_class_in);
  put_uint32(m, s2_dns_ttl);
  start = put_len_at(m);

  put_uint16(m, prio);
  put_uint16(m, weight);
  put_uint16(m, port);
  put_domain(m, target);
  put_len(m, start);
}

static void put_naptr_record(struct s2_dns_response *m,
			     char const *domain,
			     uint16_t order, uint16_t preference,
			     char const *flags,
			     char const *services,
			     char const *regexp,
			     char const *replace)
{
  uint16_t start;
  put_domain(m, domain);
  put_uint16(m, sres_type_naptr);
  put_uint16(m, sres_class_in);
  put_uint32(m, s2_dns_ttl);
  start = put_len_at(m);

  put_uint16(m, order);
  put_uint16(m, preference);
  put_string(m, flags);
  put_string(m, services);
  put_string(m, regexp);
  put_domain(m, replace);
  put_len(m, start);
}

static void put_srv_record_from_uri(struct s2_dns_response *m,
				    char const *base,
				    uint16_t prio, uint16_t weight,
				    url_t const *uri, char const *server)
{
  char domain[1024] = "none";
  char const *service = url_port(uri);
  uint16_t port;

  if (uri->url_type == url_sips) {
    strcpy(domain, "_sips._tcp.");
  }
  else if (uri->url_type == url_sip) {
    if (url_has_param(uri, "transport=udp")) {
      strcpy(domain, "_sip._udp.");
    }
    else if (url_has_param(uri, "transport=tcp")) {
      strcpy(domain, "_sip._tcp.");
    }
  }

  assert(strcmp(domain, "none"));

  strcat(domain, base);

  if (m->header->mh_qdcount == 0)
    put_query(m, domain, sres_type_srv);

  port = (uint16_t)strtoul(service, NULL, 10);

  put_srv_record(m, domain, prio, weight, port, server);
}

static
void s2_add_to_zone(struct s2_dns_response *_r)
{
  size_t size = offsetof(struct s2_dns_response, data[_r->dlen]);
  struct s2_dns_response *r = malloc(size); assert(r);

  memcpy(r, _r, size);
  r->next = zonedata;
  zonedata = r;
}


static void make_server(char *server, char const *prefix, char const *domain)
{
  strcpy(server, prefix);

  if (strlen(server) == 0 || server[strlen(server) - 1] != '.') {
    strcat(server, ".");
    strcat(server, domain);
  }
}

/** Set up records for SIP server */
void s2_dns_domain(char const *domain, int use_naptr,
		   /* char *prefix, int priority, url_t const *uri, */
		   ...)
{
  struct s2_dns_response m[1];

  char server[1024], target[1024];

  va_list va0, va;
  char const *prefix; int priority; url_t const *uri;
  struct in_addr localhost;

  assert(s2dns.reg != 0);

  su_inet_pton(AF_INET, "127.0.0.1", &localhost);

  va_start(va0, use_naptr);

  if (use_naptr) {
    memset(m, 0, sizeof m);
    put_query(m, domain, sres_type_naptr);

    va_copy(va, va0);

    for (;(prefix = va_arg(va, char *));) {
      char *services = NULL;

      priority = va_arg(va, int);
      uri = va_arg(va, url_t *);
      if (uri == NULL)
	continue;

      if (uri->url_type == url_sips) {
	services = "SIPS+D2T";
	strcpy(target, "_sips._tcp.");
      }
      else if (uri->url_type == url_sip) {
	if (url_has_param(uri, "transport=udp")) {
	  services = "SIP+D2U";
	  strcpy(target, "_sip._udp.");
	}
	else if (url_has_param(uri, "transport=tcp")) {
	  services = "SIP+D2T";
	  strcpy(target, "_sip._tcp.");
	}
      }

      strcat(target, domain);
      assert(services);
      put_naptr_record(m, domain, 1, priority, "s", services, "", target);
      m->header->mh_ancount++;
    }

    va_end(va);
    va_copy(va, va0);

    for (;(prefix = va_arg(va, char *));) {
      priority = va_arg(va, int);
      uri = va_arg(va, url_t *);
      if (uri == NULL)
	continue;

      make_server(server, prefix, domain);

      put_srv_record_from_uri(m, domain, priority, 10, uri, server);
      m->header->mh_arcount++;

      put_a_record(m, server, localhost);
      m->header->mh_arcount++;
    }
    va_end(va);

    s2_add_to_zone(m);
  }

  /* Add SRV records */
  va_copy(va, va0);
  for (;(prefix = va_arg(va, char *));) {
    priority = va_arg(va, int);
    uri = va_arg(va, url_t *);
    if (uri == NULL)
      continue;

    make_server(server, prefix, domain);

    memset(m, 0, sizeof m);
    put_srv_record_from_uri(m, domain, priority, 10, uri, server);
    m->header->mh_ancount++;

    strcpy(server, prefix); strcat(server, domain);

    put_a_record(m, server, localhost);
    m->header->mh_arcount++;

    s2_add_to_zone(m);
  }
  va_end(va);

  /* Add A records */
  va_copy(va, va0);
  for (;(prefix = va_arg(va, char *));) {
    (void)va_arg(va, int);
    if (va_arg(va, url_t *) == NULL)
      continue;

    memset(m, 0, sizeof m);
    make_server(server, prefix, domain);

    put_query(m, server, sres_type_a);
    put_a_record(m, server, localhost);
    m->header->mh_ancount++;

    s2_add_to_zone(m);
  }
  va_end(va);

  va_end(va0);
}

/** Insert DNS response.

  s2_dns_record("example.com", sres_type_naptr,
		// order priority flags services regexp target
		"", sres_type_naptr, 20, 50, "a", "SIP+D2U", "", "sip00",
		"", sres_type_naptr, 20, 50, "a", "SIP+D2T", "", "sip00",
		"", sres_type_naptr, 20, 50, "a", "SIPS+D2T", "", "sip00",
		"sip00", sres_type_a, "12.13.14.15",
		NULL);
*/
void s2_dns_record(
  char const *qdomain, unsigned qtype,
  /* unsigned atype, domain, */
  ...)
{
  struct s2_dns_response m[1];
  char const *domain;
  va_list va;
  unsigned atype;
  unsigned ancount = 0, arcount = 0;

  memset(m, 0, (sizeof m));

  va_start(va, qtype);

  put_query(m, qdomain, qtype);

  for (domain = va_arg(va, char const *); domain; domain = va_arg(va, char const *)) {
    if (!domain[0])
      domain = qdomain;

    if (strcmp(domain, "@") == 0)
      domain = default_domain;

    atype = va_arg(va, unsigned);

    if (arcount == 0 &&
	(atype == qtype || atype == sres_type_cname) &&
	strcmp(qdomain, domain) == 0)
      ancount++;
    else
      arcount++;

    switch(atype) {
    case sres_type_naptr:
      {
	unsigned order = va_arg(va, unsigned);
	unsigned priority = va_arg(va, unsigned);
	char const *flags = va_arg(va, char const *);
	char const *services = va_arg(va, char const *);
	char const *regexp = va_arg(va, char const *);
	char const *target = va_arg(va, char const *);

	put_naptr_record(m, domain, order, priority,
			 flags, services, regexp, target);
      }
      break;
    case sres_type_srv:
      {
	unsigned priority = va_arg(va, unsigned);
	unsigned weight = va_arg(va, unsigned);
	unsigned port = va_arg(va, unsigned);
	char const *target = va_arg(va, char const *);

	put_srv_record(m, domain, priority, weight, port, target);
      }
      break;
    case sres_type_aaaa:
#if SU_HAVE_IN6
      {
	char const *address = va_arg(va, char const *); /* target */
	struct in6_addr aaaa;

	inet_pton(AF_INET6, address, &aaaa);

	put_aaaa_record(m, domain, aaaa);
      }
      break;
#endif
    case sres_type_a:
      {
	char const *address = va_arg(va, char const *); /* target */
	struct in_addr a;

	inet_pton(AF_INET, address, &a);

	put_a_record(m, domain, a);
      }
      break;
    case sres_type_cname:
      put_cname_record(m, domain, va_arg(va, char const *));
    }
  }

  m->header->mh_ancount = ancount;
  m->header->mh_arcount = arcount;

  s2_add_to_zone(m);

  va_end(va);
}
