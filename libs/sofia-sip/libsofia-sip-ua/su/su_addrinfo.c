/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include <sofia-sip/su.h>
#include <sofia-sip/su_addrinfo.h>

#ifndef IN_LOOPBACKNET
#define IN_LOOPBACKNET          127
#endif

#ifndef IN_EXPERIMENTAL
#define IN_EXPERIMENTAL(a)      ((((long int) (a)) & 0xf0000000) == 0xf0000000)
#endif

#if !HAVE_GETADDRINFO

#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <ctype.h>

#ifndef EAI_NODATA
#define EAI_NODATA 7
#endif

/*
 * "#ifdef FAITH" part is local hack for supporting IPv4-v6 translator.
 *
 * Issues to be discussed:
 * - Thread safe-ness must be checked.
 * - Return values.  There are nonstandard return values defined and used
 *   in the source code.  This is because RFC2133 is silent about which error
 *   code must be returned for which situation.
 * - PF_UNSPEC case would be handled in getipnodebyname() with the AI_ALL flag.
 */

#if defined(__KAME__) && defined(INET6)
# define FAITH
#endif

#define SUCCESS 0
#define GAI_ANY 0
#define YES 1
#define NO  0

#undef SU_HAVE_IN6

#ifdef FAITH
static int translate = NO;
static struct in6_addr faith_prefix = IN6ADDR_GAI_ANY_INIT;
#endif

static const char in_addrany[] = { 0, 0, 0, 0 };
static const char in6_addrany[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0

};
static const char in_loopback[] = { 127, 0, 0, 1 };
static const char in6_loopback[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1
};

struct sockinet {
	u_char	si_len;
	u_char	si_family;
	u_short	si_port;
};

static struct gai_afd {
	int a_af;
	int a_addrlen;
	int a_socklen;
	int a_off;
	const char *a_addrany;
	const char *a_loopback;
} gai_afdl [] = {
#if SU_HAVE_IN6
#define N_INET6 0
	{PF_INET6, sizeof(struct in6_addr),
	 sizeof(struct sockaddr_in6),
	 offsetof(struct sockaddr_in6, sin6_addr),
	 in6_addrany, in6_loopback},
#define N_INET  1
#else
#define N_INET  0
#endif
	{PF_INET, sizeof(struct in_addr),
	 sizeof(struct sockaddr_in),
	 offsetof(struct sockaddr_in, sin_addr),
	 in_addrany, in_loopback},
	{0, 0, 0, 0, NULL, NULL},
};

#if SU_HAVE_IN6
#define PTON_MAX	16
#else
#define PTON_MAX	4
#endif

#if SU_HAVE_IN6 && !defined(s6_addr8)
#  define s6_addr8 s6_addr
#endif

#if !SU_HAVE_IN6
#if defined(_WIN32) || defined(__CYGWIN__) || defined(__MINGW32__)
#else
extern int h_errno;
#endif
#endif

static int get_name(const char *, struct gai_afd *,
		    struct addrinfo **, char *, struct addrinfo *,
		    int);
static int get_addr(const char *, int, struct addrinfo **,
		    struct addrinfo *, int);
static int str_isnumber(const char *);

#define GET_CANONNAME(ai, str) \
if (pai->ai_flags & AI_CANONNAME) {\
	if (((ai)->ai_canonname = (char *)malloc(strlen(str) + 1)) != NULL) {\
		strcpy((ai)->ai_canonname, (str));\
	} else {\
		error = EAI_MEMORY;\
		goto free;\
	}\
}

#if SU_HAVE_SOCKADDR_SA_LEN
#define GET_AI(ai, gai_afd, addr, port) {\
	char *p;\
	if (((ai) = (struct addrinfo *)malloc(sizeof(struct addrinfo) +\
					      ((gai_afd)->a_socklen)))\
	    == NULL) goto free;\
	memcpy(ai, pai, sizeof(struct addrinfo));\
	(ai)->ai_addr = (struct sockaddr *)((ai) + 1);\
	memset((ai)->ai_addr, 0, (gai_afd)->a_socklen);\
	(ai)->ai_addr->sa_len = (ai)->ai_addrlen = (gai_afd)->a_socklen;\
	(ai)->ai_addr->sa_family = (ai)->ai_family = (gai_afd)->a_af;\
	((struct sockinet *)(ai)->ai_addr)->si_port = port;\
	p = (char *)((ai)->ai_addr);\
	memcpy(p + (gai_afd)->a_off, (addr), (gai_afd)->a_addrlen);\
}
#else
#define GET_AI(ai, gai_afd, addr, port) {\
	char *p;\
	if (((ai) = (struct addrinfo *)malloc(sizeof(struct addrinfo) +\
					      ((gai_afd)->a_socklen)))\
	    == NULL) goto free;\
	memcpy(ai, pai, sizeof(struct addrinfo));\
	(ai)->ai_addr = (struct sockaddr *)((ai) + 1);\
	memset((ai)->ai_addr, 0, (gai_afd)->a_socklen);\
	(ai)->ai_addrlen = (gai_afd)->a_socklen; \
	(ai)->ai_addr->sa_family = (ai)->ai_family = (gai_afd)->a_af;\
	((struct sockinet *)(ai)->ai_addr)->si_port = port;\
	p = (char *)((ai)->ai_addr);\
	memcpy(p + (gai_afd)->a_off, (addr), (gai_afd)->a_addrlen);\
}
#endif

#define ERR(err) { error = (err); goto bad; }

static int
str_isnumber(p)
	const char *p;
{
	char *q = (char *)p;
	while (*q) {
		if (! isdigit(*q))
			return NO;
		q++;
	}
	return YES;
}

static
int
getaddrinfo(hostname, servname, hints, res)
	const char *hostname, *servname;
	const struct addrinfo *hints;
	struct addrinfo **res;
{
	struct addrinfo sentinel;
	struct addrinfo *top = NULL;
	struct addrinfo *cur;
	int i, error = 0;
	char pton[PTON_MAX];
	struct addrinfo ai;
	struct addrinfo *pai;
	u_short port;

#ifdef FAITH
	static int firsttime = 1;

	if (firsttime) {
		/* translator hack */
		{
			char *q = getenv("GAI");
			if (q && su_inet_pton(AF_INET6, q, &faith_prefix) == 1)
				translate = YES;
		}
		firsttime = 0;
	}
#endif

	/* initialize file static vars */
	sentinel.ai_next = NULL;
	cur = &sentinel;
	pai = &ai;
	pai->ai_flags = 0;
	pai->ai_family = PF_UNSPEC;
	pai->ai_socktype = GAI_ANY;
	pai->ai_protocol = GAI_ANY;
	pai->ai_addrlen = 0;
	pai->ai_canonname = NULL;
	pai->ai_addr = NULL;
	pai->ai_next = NULL;
	port = GAI_ANY;

	if (hostname == NULL && servname == NULL)
		return EAI_NONAME;
	if (hints) {
		/* error check for hints */
		if (hints->ai_addrlen || hints->ai_canonname ||
		    hints->ai_addr || hints->ai_next)
			ERR(EAI_BADHINTS); /* xxx */
		if (hints->ai_flags & ~AI_MASK)
			ERR(EAI_BADFLAGS);
		switch (hints->ai_family) {
		case PF_UNSPEC:
		case PF_INET:
#if SU_HAVE_IN6
		case PF_INET6:
#endif
			break;
		default:
			ERR(EAI_FAMILY);
		}
		memcpy(pai, hints, sizeof(*pai));
		switch (pai->ai_socktype) {
		case GAI_ANY:
			switch (pai->ai_protocol) {
			case GAI_ANY:
				break;
			case IPPROTO_UDP:
				pai->ai_socktype = SOCK_DGRAM;
				break;
			case IPPROTO_TCP:
				pai->ai_socktype = SOCK_STREAM;
				break;
#if HAVE_SCTP
			case IPPROTO_SCTP:
				pai->ai_socktype = SOCK_STREAM;
				break;
#endif
			default:
				pai->ai_socktype = SOCK_RAW;
				break;
			}
			break;
		case SOCK_RAW:
			break;
		case SOCK_DGRAM:
			if (pai->ai_protocol != IPPROTO_UDP &&
#if HAVE_SCTP
			    pai->ai_protocol != IPPROTO_SCTP &&
#endif
			    pai->ai_protocol != GAI_ANY)
				ERR(EAI_BADHINTS);	/*xxx*/
			if (pai->ai_protocol == GAI_ANY)
				pai->ai_protocol = IPPROTO_UDP;
			break;
		case SOCK_STREAM:
			if (pai->ai_protocol != IPPROTO_TCP &&
#if HAVE_SCTP
			    pai->ai_protocol != IPPROTO_SCTP &&
#endif
			    pai->ai_protocol != GAI_ANY)
				ERR(EAI_BADHINTS);	/*xxx*/
			if (pai->ai_protocol == GAI_ANY)
				pai->ai_protocol = IPPROTO_TCP;
			break;
#if HAVE_SCTP
		case SOCK_SEQPACKET:
			if (pai->ai_protocol != IPPROTO_SCTP &&
			    pai->ai_protocol != GAI_ANY)
				ERR(EAI_BADHINTS);	/*xxx*/

			if (pai->ai_protocol == GAI_ANY)
				pai->ai_protocol = IPPROTO_SCTP;
			break;
#endif
		default:
			ERR(EAI_SOCKTYPE);
			break;
		}
	}

	/*
	 * service port
	 */
	if (servname) {
		if (str_isnumber(servname)) {
			if (pai->ai_socktype == GAI_ANY) {
				/* caller accept *GAI_ANY* socktype */
				pai->ai_socktype = SOCK_DGRAM;
				pai->ai_protocol = IPPROTO_UDP;
			}
			port = htons(atoi(servname));
		} else {
			struct servent *sp;
			char *proto;

			proto = NULL;
			switch (pai->ai_socktype) {
			case GAI_ANY:
				proto = NULL;
				break;
			case SOCK_DGRAM:
				proto = "udp";
				break;
			case SOCK_STREAM:
				proto = "tcp";
				break;
			default:
				fprintf(stderr, "panic!\n");
				break;
			}
			if ((sp = getservbyname(servname, proto)) == NULL)
				ERR(EAI_SERVICE);
			port = sp->s_port;
			if (pai->ai_socktype == GAI_ANY) {
				if (strcmp(sp->s_proto, "udp") == 0) {
					pai->ai_socktype = SOCK_DGRAM;
					pai->ai_protocol = IPPROTO_UDP;
				} else if (strcmp(sp->s_proto, "tcp") == 0) {
					pai->ai_socktype = SOCK_STREAM;
					pai->ai_protocol = IPPROTO_TCP;
#if HAVE_SCTP
				} else if (strcmp(sp->s_proto, "sctp") == 0) {
					pai->ai_socktype = SOCK_STREAM;
					pai->ai_protocol = IPPROTO_SCTP;
#endif
				} else
					ERR(EAI_PROTOCOL);	/*xxx*/
			}
		}
	}

	/*
	 * hostname == NULL.
	 * passive socket -> anyaddr (0.0.0.0 or ::)
	 * non-passive socket -> localhost (127.0.0.1 or ::1)
	 */
	if (hostname == NULL) {
		struct gai_afd *gai_afd;

		for (gai_afd = &gai_afdl[0]; gai_afd->a_af; gai_afd++) {
			if (!(pai->ai_family == PF_UNSPEC
			   || pai->ai_family == gai_afd->a_af)) {
				continue;
			}

			if (pai->ai_flags & AI_PASSIVE) {
				GET_AI(cur->ai_next, gai_afd, gai_afd->a_addrany, port);
				/* xxx meaningless?
				 * GET_CANONNAME(cur->ai_next, "anyaddr");
				 */
			} else {
				GET_AI(cur->ai_next, gai_afd, gai_afd->a_loopback,
					port);
				/* xxx meaningless?
				 * GET_CANONNAME(cur->ai_next, "localhost");
				 */
			}
			cur = cur->ai_next;
		}
		top = sentinel.ai_next;
		if (top)
			goto good;
		else
			ERR(EAI_FAMILY);
	}

	/* hostname as numeric name */
	for (i = 0; gai_afdl[i].a_af; i++) {
		if (su_inet_pton(gai_afdl[i].a_af, hostname, pton)) {
			u_long v4a;
			u_char pfx;

			switch (gai_afdl[i].a_af) {
			case AF_INET:
				v4a = ((struct in_addr *)pton)->s_addr;
				if (IN_MULTICAST(v4a) || IN_EXPERIMENTAL(v4a))
					pai->ai_flags &= ~AI_CANONNAME;
				v4a >>= IN_CLASSA_NSHIFT;
				if (v4a == 0 || v4a == IN_LOOPBACKNET)
					pai->ai_flags &= ~AI_CANONNAME;
				break;
#if SU_HAVE_IN6
			case AF_INET6:
				pfx = ((struct in6_addr *)pton)->s6_addr8[0];
				if (pfx == 0 || pfx == 0xfe || pfx == 0xff)
					pai->ai_flags &= ~AI_CANONNAME;
				break;
#endif
			}

			if (pai->ai_family == gai_afdl[i].a_af ||
			    pai->ai_family == PF_UNSPEC) {
				if (! (pai->ai_flags & AI_CANONNAME)) {
					GET_AI(top, &gai_afdl[i], pton, port);
					goto good;
				}
				/*
				 * if AI_CANONNAME and if reverse lookup
				 * fail, return ai anyway to pacify
				 * calling application.
				 *
				 * XXX getaddrinfo() is a name->address
				 * translation function, and it looks strange
				 * that we do addr->name translation here.
				 */
				get_name(pton, &gai_afdl[i], &top, pton, pai, port);
				goto good;
			} else
				ERR(EAI_FAMILY);	/*xxx*/
		}
	}

	if (pai->ai_flags & AI_NUMERICHOST)
		ERR(EAI_NONAME);

	/* hostname as alphabetical name */
	error = get_addr(hostname, pai->ai_family, &top, pai, port);
	if (error == 0) {
		if (top) {
 good:
			*res = top;
			return SUCCESS;
		} else
			error = EAI_FAIL;
	}
 free:
	if (top)
		freeaddrinfo(top);
 bad:
	*res = NULL;
	return error;
}

static int
get_name(addr, gai_afd, res, numaddr, pai, port0)
	const char *addr;
	struct gai_afd *gai_afd;
	struct addrinfo **res;
	char *numaddr;
	struct addrinfo *pai;
	int port0;
{
	u_short port = port0 & 0xffff;
	struct hostent *hp;
	struct addrinfo *cur;
	int error = 0, h_error;

#if SU_HAVE_IN6
	hp = getipnodebyaddr(addr, gai_afd->a_addrlen, gai_afd->a_af, &h_error);
#else
	hp = gethostbyaddr(addr, gai_afd->a_addrlen, AF_INET);
#endif
	if (hp && hp->h_name && hp->h_name[0] && hp->h_addr_list[0]) {
		GET_AI(cur, gai_afd, hp->h_addr_list[0], port);
		GET_CANONNAME(cur, hp->h_name);
	} else
		GET_AI(cur, gai_afd, numaddr, port);

#if SU_HAVE_IN6
	if (hp)
		freehostent(hp);
#endif
	*res = cur;
	return SUCCESS;
 free:
	if (cur)
		freeaddrinfo(cur);
#if SU_HAVE_IN6
	if (hp)
		freehostent(hp);
#endif
 /* bad: */
	*res = NULL;
	return error;
}

static int
get_addr(hostname, af, res, pai, port0)
	const char *hostname;
	int af;
	struct addrinfo **res;
	struct addrinfo *pai;
	int port0;
{
	u_short port = port0 & 0xffff;
	struct addrinfo sentinel;
	struct hostent *hp;
	struct addrinfo *top, *cur;
	struct gai_afd *gai_afd;
	int i, error = 0, h_error;
	char *ap;

	top = NULL;
	sentinel.ai_next = NULL;
	cur = &sentinel;
#if SU_HAVE_IN6
	if (af == AF_UNSPEC) {
		hp = getipnodebyname(hostname, AF_INET6,
				AI_ADDRCONFIG|AI_ALL|AI_V4MAPPED, &h_error);
	} else
		hp = getipnodebyname(hostname, af, AI_ADDRCONFIG, &h_error);
#else
	hp = gethostbyname(hostname);
	h_error = h_errno;
#endif
	if (hp == NULL) {
		switch (h_error) {
		case HOST_NOT_FOUND:
		case NO_DATA:
			error = EAI_NODATA;
			break;
		case TRY_AGAIN:
			error = EAI_AGAIN;
			break;
		case NO_RECOVERY:
		default:
			error = EAI_FAIL;
			break;
		}
		goto bad;
	}

	if ((hp->h_name == NULL) || (hp->h_name[0] == 0) ||
	    (hp->h_addr_list[0] == NULL))
		ERR(EAI_FAIL);

	for (i = 0; (ap = hp->h_addr_list[i]) != NULL; i++) {
		switch (af) {
#if SU_HAVE_IN6
		case AF_INET6:
			gai_afd = &gai_afdl[N_INET6];
			break;
#endif
#ifndef INET6
		default:	/* AF_UNSPEC */
#endif
		case AF_INET:
			gai_afd = &gai_afdl[N_INET];
			break;
#if SU_HAVE_IN6
		default:	/* AF_UNSPEC */
			if (IN6_IS_ADDR_V4MAPPED((struct in6_addr *)ap)) {
				ap += sizeof(struct in6_addr) -
					sizeof(struct in_addr);
				gai_afd = &gai_afdl[N_INET];
			} else
				gai_afd = &gai_afdl[N_INET6];
			break;
#endif
		}
#ifdef FAITH
		if (translate && gai_afd->a_af == AF_INET) {
			struct in6_addr *in6;

			GET_AI(cur->ai_next, &gai_afdl[N_INET6], ap, port);
			in6 = &((struct sockaddr_in6 *)cur->ai_next->ai_addr)->sin6_addr;
			memcpy(&in6->s6_addr32[0], &faith_prefix,
			    sizeof(struct in6_addr) - sizeof(struct in_addr));
			memcpy(&in6->s6_addr32[3], ap, sizeof(struct in_addr));
		} else
#endif /* FAITH */
		GET_AI(cur->ai_next, gai_afd, ap, port);
		if (cur == &sentinel) {
			top = cur->ai_next;
			GET_CANONNAME(top, hp->h_name);
		}
		cur = cur->ai_next;
	}
#if SU_HAVE_IN6
	freehostent(hp);
#endif
	*res = top;
	return SUCCESS;
 free:
	if (top)
		freeaddrinfo(top);
#if SU_HAVE_IN6
	if (hp)
		freehostent(hp);
#endif
 bad:
	*res = NULL;
	return error;
}

/*
 * Issues to be discussed:
 * - Thread safe-ness must be checked
 * - Return values.  There seems to be no standard for return value (RFC2133)
 *   but INRIA implementation returns EAI_xxx defined for getaddrinfo().
 */

#define SUCCESS 0
#define YES 1
#define NO  0

static struct gni_afd {
	int a_af;
	int a_addrlen;
	int a_socklen;
	int a_off;
} gni_afdl [] = {
#if SU_HAVE_IN6
	{PF_INET6, sizeof(struct in6_addr), sizeof(struct sockaddr_in6),
		offsetof(struct sockaddr_in6, sin6_addr)},
#endif
	{PF_INET, sizeof(struct in_addr), sizeof(struct sockaddr_in),
		offsetof(struct sockaddr_in, sin_addr)},
	{0, 0, 0},
};

struct gni_sockinet {
	u_char	si_len;
	u_char	si_family;
	u_short	si_port;
};

#define ENI_NOSOCKET 	EAI_FAIL
#define ENI_NOSERVNAME	EAI_NONAME
#define ENI_NOHOSTNAME	EAI_NONAME
#define ENI_MEMORY	EAI_MEMORY
#define ENI_SYSTEM	EAI_SYSTEM
#define ENI_FAMILY	EAI_FAMILY
#define ENI_SALEN	EAI_MEMORY

static
int
getnameinfo(sa, salen, host, hostlen, serv, servlen, flags)
	const struct sockaddr *sa;
	size_t salen;
	char *host;
	size_t hostlen;
	char *serv;
	size_t servlen;
	int flags;
{
	struct gni_afd *gni_afd;
	struct servent *sp;
	struct hostent *hp;
	u_short port;
	int family, len, i;
	char *addr, *p;
	u_long v4a;
	u_char pfx;
	int h_error;
	char numserv[512];
	char numaddr[512];

	if (sa == NULL)
		return ENI_NOSOCKET;

#if SU_HAVE_SOCKADDR_SA_LEN
	len = sa->sa_len;
	if (len != salen) return ENI_SALEN;
#else
	len = salen;
#endif

	family = sa->sa_family;
	for (i = 0; gni_afdl[i].a_af; i++)
		if (gni_afdl[i].a_af == family) {
			gni_afd = &gni_afdl[i];
			goto found;
		}
	return ENI_FAMILY;

 found:
	if (len != gni_afd->a_socklen) return ENI_SALEN;

	port = ((struct gni_sockinet *)sa)->si_port; /* network byte order */
	addr = (char *)sa + gni_afd->a_off;

	if (serv == NULL || servlen == 0) {
		/* what we should do? */
	} else if (flags & NI_NUMERICSERV) {
		snprintf(numserv, sizeof(numserv), "%d", ntohs(port));
		if (strlen(numserv) > servlen)
			return ENI_MEMORY;
		strcpy(serv, numserv);
	} else {
		sp = getservbyport(port, (flags & NI_DGRAM) ? "udp" : "tcp");
		if (sp) {
			if (strlen(sp->s_name) > servlen)
				return ENI_MEMORY;
			strcpy(serv, sp->s_name);
		} else
			return ENI_NOSERVNAME;
	}

	switch (sa->sa_family) {
	case AF_INET:
		v4a = ((struct sockaddr_in *)sa)->sin_addr.s_addr;
		if (IN_MULTICAST(v4a) || IN_EXPERIMENTAL(v4a))
			flags |= NI_NUMERICHOST;
		v4a >>= IN_CLASSA_NSHIFT;
		if (v4a == 0 || v4a == IN_LOOPBACKNET)
			flags |= NI_NUMERICHOST;
		break;
#if SU_HAVE_IN6
	case AF_INET6:
		pfx = ((struct sockaddr_in6 *)sa)->sin6_addr.s6_addr8[0];
		if (pfx == 0 || pfx == 0xfe || pfx == 0xff)
			flags |= NI_NUMERICHOST;
		break;
#endif
	}
	if (host == NULL || hostlen == 0) {
		/* what should we do? */
	} else if (flags & NI_NUMERICHOST) {
		if (su_inet_ntop(gni_afd->a_af, addr, numaddr, sizeof(numaddr))
		    == NULL)
			return ENI_SYSTEM;
		if (strlen(numaddr) > hostlen)
			return ENI_MEMORY;
		strcpy(host, numaddr);
	} else {
#if SU_HAVE_IN6
		hp = getipnodebyaddr(addr, gni_afd->a_addrlen, gni_afd->a_af, &h_error);
#else
		hp = gethostbyaddr(addr, gni_afd->a_addrlen, gni_afd->a_af);
		h_error = h_errno;
#endif

		if (hp) {
			if (flags & NI_NOFQDN) {
				p = strchr(hp->h_name, '.');
				if (p) *p = '\0';
			}
			if (strlen(hp->h_name) > hostlen) {
#if SU_HAVE_IN6
				freehostent(hp);
#endif
				return ENI_MEMORY;
			}
			strcpy(host, hp->h_name);
#if SU_HAVE_IN6
			freehostent(hp);
#endif
		} else {
			if (flags & NI_NAMEREQD)
				return ENI_NOHOSTNAME;
			if (su_inet_ntop(gni_afd->a_af, addr,
					 numaddr, sizeof(numaddr))
			    == NULL)
				return ENI_NOHOSTNAME;
			if (strlen(numaddr) > hostlen)
				return ENI_MEMORY;
			strcpy(host, numaddr);
		}
	}
	return SUCCESS;
}

#endif	/* !HAVE_GETNAMEINFO */

#if !HAVE_FREEADDRINFO
static
void
freeaddrinfo(ai)
	struct addrinfo *ai;
{
	struct addrinfo *next;

	if (ai == NULL)
		return;

	do {
		next = ai->ai_next;
		if (ai->ai_canonname)
			free(ai->ai_canonname);
		/* no need to free(ai->ai_addr) */
		free(ai);
	} while ((ai = next) != NULL);
}
#endif

#if !HAVE_GAI_STRERROR
static
char *
gai_strerror(ecode)
	int ecode;
{
  switch (ecode) {
  case 0:
    return "success.";
#if defined(EAI_ADDRFAMILY)
  case EAI_ADDRFAMILY:
    return "address family for hostname not supported.";
#endif
#if defined(EAI_AGAIN)
  case EAI_AGAIN:
    return "temporary failure in name resolution.";
#endif
#if defined(EAI_BADFLAGS)
  case EAI_BADFLAGS:
    return "invalid value for ai_flags.";
#endif
#if defined(EAI_FAIL)
  case EAI_FAIL:
    return "non-recoverable failure in name resolution.";
#endif
#if defined(EAI_FAMILY)
  case EAI_FAMILY:
    return "ai_family not supported.";
#endif
#if defined(EAI_MEMORY)
  case EAI_MEMORY:
    return "memory allocation failure.";
#endif
#if defined(EAI_NODATA)
  case EAI_NODATA:
    return "no address associated with hostname.";
#endif
#if defined(EAI_NONAME)
  case EAI_NONAME:
    return "hostname nor servname provided, or not known.";
#endif
#if defined(EAI_SERVICE)
  case EAI_SERVICE:
    return "servname not supported for ai_socktype.";
#endif
#if defined(EAI_SOCKTYPE)
  case EAI_SOCKTYPE:
    return "ai_socktype not supported.";
#endif
#if defined(EAI_SYSTEM)
  case EAI_SYSTEM:
    return "system error returned in errno.";
#endif
#if defined(EAI_BADHINTS)
  case EAI_BADHINTS:
    return "invalid value for hints.";
#endif
#if defined(EAI_PROTOCOL)
  case EAI_PROTOCOL:
    return "resolved protocol is unknown.";
#endif
  default:
    return "unknown error.";
  }
}
#endif


/** Translate address and service.
 *
 * This is a getaddrinfo() supporting SCTP and other exotic protocols.
 */
int su_getaddrinfo(char const *node, char const *service,
		   su_addrinfo_t const *hints,
		   su_addrinfo_t **res)
{
  int retval;
  su_addrinfo_t *ai;
  char const *realservice = service;

  if (!service || service[0] == '\0')
    service = "0";

#if HAVE_SCTP
  if (res && hints && hints->ai_protocol == IPPROTO_SCTP) {
    su_addrinfo_t system_hints[1];
    int socktype;

    socktype = hints->ai_socktype;

    if (!(socktype == 0 ||
	  socktype == SOCK_SEQPACKET ||
	  socktype == SOCK_STREAM ||
	  socktype == SOCK_DGRAM))
      return EAI_SOCKTYPE;

    *system_hints = *hints;
    system_hints->ai_protocol = IPPROTO_TCP;
    system_hints->ai_socktype = SOCK_STREAM;

    retval = getaddrinfo(node, service, system_hints, res);
    if (retval)
      return retval;

    if (socktype == 0)
      socktype = SOCK_STREAM;

    for (ai = *res; ai; ai = ai->ai_next) {
      ai->ai_protocol = IPPROTO_SCTP;
      ai->ai_socktype = socktype;
    }

    return 0;
  }
#endif

  retval = getaddrinfo(node, service, hints, res);

  if (service != realservice && retval == EAI_SERVICE)
    retval = getaddrinfo(node, realservice, hints, res);

  if (retval == 0) {
    for (ai = *res; ai; ai = ai->ai_next) {
      if (ai->ai_protocol)
	continue;

      if (hints && hints->ai_protocol) {
	ai->ai_protocol = hints->ai_protocol;
	continue;
      }

      if (ai->ai_family != AF_INET
#if SU_HAVE_IN6
	  && ai->ai_family != AF_INET6
#endif
	  ) continue;

      if (ai->ai_socktype == SOCK_STREAM)
	ai->ai_protocol = IPPROTO_TCP;
      else if (ai->ai_socktype == SOCK_DGRAM)
	ai->ai_protocol = IPPROTO_UDP;
    }
  }
  return retval;
}

/** Free su_addrinfo_t structure allocated by su_getaddrinfo(). */
void su_freeaddrinfo(su_addrinfo_t *res)
{
  freeaddrinfo(res);
}

/** Return string describing address translation error. */
char const *su_gai_strerror(int errcode)
{
	return (char const *)gai_strerror(errcode);
}

/** Resolve socket address into hostname and service name.
 *
 * @note
 * This function uses now @RFC2133 prototype. The @RFC3493 redefines the
 * prototype as well as ai_addrlen to use socklen_t instead of size_t.
 * If your application allocates more than 2 gigabytes for resolving the
 * hostname, you probably lose.
 */
int
su_getnameinfo(const su_sockaddr_t *su, size_t sulen,
	       char *return_host, size_t hostlen,
	       char *return_serv, size_t servlen,
	       int flags)
{
  return getnameinfo(&su->su_sa, (socklen_t)sulen,
		     return_host, (socklen_t)hostlen,
		     return_serv, (socklen_t)servlen,
		     flags);
}
