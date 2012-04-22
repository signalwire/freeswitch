/*
 * Copyright (C) 1995, 1996, 1997, 1998, and 1999 WIDE Project.
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

#ifndef SU_ADDRINFO_H
/* Defined when <sofia-sip/su_addrinfo.h> has been included */
#define SU_ADDRINFO_H
/**@ingroup su_socket
 *
 * @file sofia-sip/su_addrinfo.h Network address and service translation.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Nov 30 17:07:04 EET 2005 ppessi
 */

#ifndef SU_TYPES_H
#include <sofia-sip/su_types.h>
#endif

#if SU_HAVE_BSDSOCK
#include <netdb.h>
#endif

SOFIA_BEGIN_DECLS

#if !SU_HAVE_ADDRINFO
/*
 * Error return codes from getaddrinfo()
 */
#ifndef EAI_ADDRFAMILY
#define	EAI_ADDRFAMILY	 1	/* address family for hostname not supported */
#define	EAI_AGAIN	 2	/* temporary failure in name resolution */
#define	EAI_BADFLAGS	 3	/* invalid value for ai_flags */
#define	EAI_FAIL	 4	/* non-recoverable failure in name resolution */
#define	EAI_FAMILY	 5	/* ai_family not supported */
#define	EAI_MEMORY	 6	/* memory allocation failure */
#define	EAI_NODATA	 7	/* no address associated with hostname */
#define	EAI_NONAME	 8	/* hostname nor servname provided, or not known */
#define	EAI_SERVICE	 9	/* servname not supported for ai_socktype */
#define	EAI_SOCKTYPE	10	/* ai_socktype not supported */
#define	EAI_SYSTEM	11	/* system error returned in errno */
#define EAI_BADHINTS	12
#define EAI_PROTOCOL	13
#define EAI_MAX		14
#endif

/*
 * Flag values for getaddrinfo()
 */
#ifndef AI_PASSIVE
#define	AI_PASSIVE	0x00000001 /* get address to use bind() */
#define	AI_CANONNAME	0x00000002 /* fill ai_canonname */
#define	AI_NUMERICHOST	0x00000004 /* prevent name resolution */
/* valid flags for addrinfo */
#define	AI_MASK		(AI_PASSIVE | AI_CANONNAME | AI_NUMERICHOST)

#define	AI_ALL		0x00000100 /* IPv6 and IPv4-mapped (with AI_V4MAPPED) */
#define	AI_V4MAPPED_CFG	0x00000200 /* accept IPv4-mapped if kernel supports */
#define	AI_ADDRCONFIG	0x00000400 /* only if any address is assigned */
#define	AI_V4MAPPED	0x00000800 /* accept IPv4-mapped IPv6 address */
/* special recommended flags for getipnodebyname */
#define	AI_DEFAULT	(AI_V4MAPPED_CFG | AI_ADDRCONFIG)
#endif

/*
 * Constants for getnameinfo()
 */
#ifndef NI_MAXHOST
#define	NI_MAXHOST	1025
#define	NI_MAXSERV	32
#endif

/*
 * Flag values for getnameinfo()
 */
#ifndef NI_NOFQDN
#define	NI_NOFQDN	0x00000001
#define	NI_NUMERICHOST	0x00000002
#define	NI_NAMEREQD	0x00000004
#define	NI_NUMERICSERV	0x00000008
#define	NI_DGRAM	0x00000010
#endif

struct addrinfo {
	int	ai_flags;	/* AI_PASSIVE, AI_CANONNAME */
	int	ai_family;	/* PF_xxx */
	int	ai_socktype;	/* SOCK_xxx */
	int	ai_protocol;	/* 0 or IPPROTO_xxx for IPv4 and IPv6 */
	size_t	ai_addrlen;	/* length of ai_addr */
	char	*ai_canonname;	/* canonical name for hostname */
	struct sockaddr *ai_addr;	/* binary address */
	struct addrinfo *ai_next;	/* next structure in linked list */
};
#endif	/* SU_WITH_GETADDRINFO */

#ifndef EAI_BADHINTS
#define EAI_BADHINTS	10012
#endif
#ifndef EAI_PROTOCOL
#define EAI_PROTOCOL	10013
#endif

#ifndef AI_MASK
#define	AI_MASK		(AI_PASSIVE | AI_CANONNAME | AI_NUMERICHOST)
#endif

/** @RFC1576 address info structure. */
typedef struct addrinfo su_addrinfo_t;

/** Translate address and service. */
SOFIAPUBFUN
int su_getaddrinfo(char const *node, char const *service,
		   su_addrinfo_t const *hints,
		   su_addrinfo_t **res);
/** Free su_addrinfo_t structure allocated by su_getaddrinfo(). */
SOFIAPUBFUN void su_freeaddrinfo(su_addrinfo_t *res);

/** Return string describing address translation error. */
SOFIAPUBFUN char const *su_gai_strerror(int errcode);

union su_sockaddr_u;

SOFIAPUBFUN
int su_getnameinfo(const union su_sockaddr_u *su, size_t sulen,
		   char *host, size_t hostlen,
		   char *serv, size_t servlen,
		   int flags);

SOFIA_END_DECLS

#endif	/* !defined(SU_ADDRINFO_H) */
