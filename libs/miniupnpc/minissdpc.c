/* $Id: minissdpc.c,v 1.7 2008/12/18 17:45:48 nanard Exp $ */
/* Project : miniupnp
 * Author : Thomas BERNARD
 * copyright (c) 2005-2008 Thomas Bernard
 * This software is subjet to the conditions detailed in the
 * provided LICENCE file. */
/*#include <syslog.h>*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _MSC_VER
#include <io.h>
typedef intptr_t ssize_t;
#define read _read
#define write _write
#define close _close
#else
#include <unistd.h>
#endif
#include <sys/types.h>
#ifdef WIN32
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <io.h>
#else
#include <sys/socket.h>
#include <sys/un.h>
#endif

#include "minissdpc.h"
#include "miniupnpc.h"

#include "codelength.h"

struct UPNPDev *
getDevicesFromMiniSSDPD(const char * devtype, const char * socketpath)
{
#ifdef _MSC_VER
	/* sockaddr_un not supported on msvc*/
	return NULL;
#else
	struct UPNPDev * tmp;
	struct UPNPDev * devlist = NULL;
	unsigned char buffer[2048];
	ssize_t n;
	unsigned char * p;
	unsigned char * url;
	unsigned int i;
	unsigned int urlsize, stsize, usnsize, l, plen;
	int s;
	struct sockaddr_un addr;

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if(s < 0)
	{
		/*syslog(LOG_ERR, "socket(unix): %m");*/
		perror("socket(unix)");
		return NULL;
	}
	addr.sun_family = AF_UNIX;
	plen = strlen(socketpath);
	if (plen + 1 > sizeof(addr.sun_path)) {
		plen = sizeof(addr.sun_path) - 1;
	}
	memset(addr.sun_path, 0, sizeof(addr.sun_path));
	memcpy(addr.sun_path, socketpath, plen);
	if(connect(s, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) < 0)
	{
		/*syslog(LOG_WARNING, "connect(\"%s\"): %m", socketpath);*/
		close(s);
		return NULL;
	}
	stsize = strlen(devtype);
	buffer[0] = 1; /* request type 1 : request devices/services by type */
	p = buffer + 1;
	l = stsize;	CODELENGTH(l, p);
	memcpy(p, devtype, stsize);
	p += stsize;
	if(write(s, buffer, p - buffer) < 0)
	{
		/*syslog(LOG_ERR, "write(): %m");*/
		perror("minissdpc.c: write()");
		close(s);
		return NULL;
	}
	n = read(s, buffer, sizeof(buffer));
	if(n<=0)
	{
		perror("minissdpc.c: read()");
		close(s);
		return NULL;
	}
	p = buffer + 1;
	for(i = 0; i < buffer[0]; i++)
	{
		if(p+2>=buffer+sizeof(buffer))
			break;
		DECODELENGTH(urlsize, p);
		if(p+urlsize+2>=buffer+sizeof(buffer))
			break;
		url = p;
		p += urlsize;
		DECODELENGTH(stsize, p);
		if(p+stsize+2>=buffer+sizeof(buffer))
			break;
		tmp = (struct UPNPDev *)malloc(sizeof(struct UPNPDev)+urlsize+stsize);
		tmp->pNext = devlist;
		tmp->descURL = tmp->buffer;
		tmp->st = tmp->buffer + 1 + urlsize;
		memcpy(tmp->buffer, url, urlsize);
		tmp->buffer[urlsize] = '\0';
		memcpy(tmp->buffer + urlsize + 1, p, stsize);
		p += stsize;
		tmp->buffer[urlsize+1+stsize] = '\0';
		devlist = tmp;
		/* added for compatibility with recent versions of MiniSSDPd 
		 * >= 2007/12/19 */
		DECODELENGTH(usnsize, p);
		p += usnsize;
		if(p>buffer + sizeof(buffer))
			break;
	}
	close(s);
	return devlist;
#endif
}

