/* $Id: natpmpc.c,v 1.6 2008/07/02 22:33:06 nanard Exp $ */
/* libnatpmp
 * Copyright (c) 2007-2008, Thomas BERNARD <miniupnp@free.fr>
 * http://miniupnp.free.fr/libnatpmp.html
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
#include <errno.h>
#include <string.h>
#include <unistd.h>
#ifdef WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include "natpmp.h"

void usage(FILE * out, const char * argv0)
{
	fprintf(out, "Usage :\n");
	fprintf(out, "  %s\n", argv0);
	fprintf(out, "\tdisplay the public IP address.\n");
	fprintf(out, "  %s -h\n", argv0);
	fprintf(out, "\tdisplay this help screen.\n");
	fprintf(out, "  %s -a <public port> <private port> <protocol> [lifetime]\n", argv0);
	fprintf(out, "\tadd a port mapping.\n");
	fprintf(out, "\n  In order to remove a mapping, set it with a lifetime of 0 seconds.\n");
	fprintf(out, "  To remove all mappings for your machine, use 0 as private port and lifetime.\n");
}

/* sample code for using libnatpmp */
int main(int argc, char * * argv)
{
	natpmp_t natpmp;
	natpmpresp_t response;
	int r;
	int sav_errno;
	struct timeval timeout;
	fd_set fds;
	int i;
	int protocol = 0;
	uint16_t privateport = 0;
	uint16_t publicport = 0;
	uint32_t lifetime = 3600;
	int command = 0;

#ifdef WIN32
	WSADATA wsaData;
	int nResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if(nResult != NO_ERROR)
	{
		fprintf(stderr, "WSAStartup() failed.\n");
		return -1;
	}
#endif

	/* argument parsing */
	for(i=1; i<argc; i++) {
		if(argv[i][0] == '-') {
			switch(argv[i][1]) {
			case 'h':
				usage(stdout, argv[0]);
				return 0;
			case 'a':
				command = 'a';
				if(argc < i + 3) {
					fprintf(stderr, "Not enough arguments for option -a\n");
					return 1;
				}
				i++;
				if(1 != sscanf(argv[i], "%hu", &publicport)) {
					fprintf(stderr, "%s is not a correct 16bits unsigned integer\n", argv[i]);
					return 1;
				}
				i++;
				if(1 != sscanf(argv[i], "%hu", &privateport)) {
					fprintf(stderr, "%s is not a correct 16bits unsigned integer\n", argv[i]);
					return 1;
				}
				i++;
				if(0 == strcasecmp(argv[i], "tcp"))
					protocol = NATPMP_PROTOCOL_TCP;
				else if(0 == strcasecmp(argv[i], "udp"))
					protocol = NATPMP_PROTOCOL_UDP;
				else {
					fprintf(stderr, "%s is not a valid protocol\n", argv[i]);
					return 1;
				}
				if(argc >= i) {
					i++;
					if(1 != sscanf(argv[i], "%u", &lifetime)) {
						fprintf(stderr, "%s is not a correct 32bits unsigned integer\n", argv[i]);
					}
				}
				break;
			default:
				fprintf(stderr, "Unknown option %s\n", argv[i]);
				usage(stderr, argv[0]);
				return 1;
			}
		} else {
			fprintf(stderr, "Unknown option %s\n", argv[i]);
			usage(stderr, argv[0]);
			return 1;
		}
	}

	/* initnatpmp() */
	r = initnatpmp(&natpmp);
	printf("initnatpmp() returned %d (%s)\n", r, r?"FAILED":"SUCCESS");
	if(r<0)
		return 1;

	/* sendpublicaddressrequest() */
	r = sendpublicaddressrequest(&natpmp);
	printf("sendpublicaddressrequest returned %d (%s)\n",
	       r, r==2?"SUCCESS":"FAILED");
	if(r<0)
		return 1;

	do {
		FD_ZERO(&fds);
		FD_SET(natpmp.s, &fds);
		getnatpmprequesttimeout(&natpmp, &timeout);
		select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
		r = readnatpmpresponseorretry(&natpmp, &response);
		sav_errno = errno;
		printf("readnatpmpresponseorretry returned %d (%s)\n",
		       r, r==0?"OK":(r==NATPMP_TRYAGAIN?"TRY AGAIN":"FAILED"));
		if(r<0 && r!=NATPMP_TRYAGAIN) {
#ifdef ENABLE_STRNATPMPERR
			fprintf(stderr, "readnatpmpresponseorretry() failed : %s\n",
			        strnatpmperr(r));
#endif
			fprintf(stderr, "  errno=%d '%s'\n", 
			        sav_errno, strerror(sav_errno));
		}
	} while(r==NATPMP_TRYAGAIN);
	if(r<0)
		return 1;

	/* TODO : check that response.type == 0 */
	printf("Public IP address : %s\n", inet_ntoa(response.pnu.publicaddress.addr));
	printf("epoch = %u\n", response.epoch);

	if(command == 'a') {
		/* sendnewportmappingrequest() */
		r = sendnewportmappingrequest(&natpmp, protocol,
        	                      privateport, publicport,
								  lifetime);
		printf("sendnewportmappingrequest returned %d (%s)\n",
		       r, r==12?"SUCCESS":"FAILED");
		if(r < 0)
			return 1;

		do {
			FD_ZERO(&fds);
			FD_SET(natpmp.s, &fds);
			getnatpmprequesttimeout(&natpmp, &timeout);
			select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
			r = readnatpmpresponseorretry(&natpmp, &response);
			printf("readnatpmpresponseorretry returned %d (%s)\n",
			       r, r==0?"OK":(r==NATPMP_TRYAGAIN?"TRY AGAIN":"FAILED"));
		} while(r==NATPMP_TRYAGAIN);
		if(r<0) {
#ifdef ENABLE_STRNATPMPERR
			fprintf(stderr, "readnatpmpresponseorretry() failed : %s\n",
			        strnatpmperr(r));
#endif
			return 1;
		}
	
		printf("Mapped public port %hu protocol %s to local port %hu "
		       "liftime %u\n",
	    	   response.pnu.newportmapping.mappedpublicport,
			   response.type == NATPMP_RESPTYPE_UDPPORTMAPPING ? "UDP" :
			    (response.type == NATPMP_RESPTYPE_TCPPORTMAPPING ? "TCP" :
			     "UNKNOWN"),
			   response.pnu.newportmapping.privateport,
			   response.pnu.newportmapping.lifetime);
		printf("epoch = %u\n", response.epoch);
	}

	r = closenatpmp(&natpmp);
	printf("closenatpmp() returned %d (%s)\n", r, r==0?"SUCCESS":"FAILED");
	if(r<0)
		return 1;

	return 0;
}

