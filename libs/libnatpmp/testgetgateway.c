/* $Id: testgetgateway.c,v 1.4 2008/07/02 22:33:06 nanard Exp $ */
/* libnatpmp
 * Copyright (c) 2007, Thomas BERNARD <miniupnp@free.fr>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */
#include <stdio.h>
#ifdef WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include "getgateway.h"

int main(int argc, char * * argv)
{
	struct in_addr gatewayaddr;
	int r;
#ifdef WIN32
	uint32_t temp = 0;
	r = getdefaultgateway(&temp);
	gatewayaddr.S_un.S_addr = temp;
#else
	r = getdefaultgateway(&(gatewayaddr.s_addr));
#endif
	if(r>=0)
		printf("default gateway : %s\n", inet_ntoa(gatewayaddr));
	else
		fprintf(stderr, "getdefaultgateway() failed\n");
	return 0;
}

