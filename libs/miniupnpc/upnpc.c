/* $Id: upnpc.c,v 1.65 2008/10/14 18:05:27 nanard Exp $ */
/* Project : miniupnp
 * Author : Thomas Bernard
 * Copyright (c) 2005-2008 Thomas Bernard
 * This software is subject to the conditions detailed in the
 * LICENCE file provided in this distribution.
 * */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WIN32
#include <winsock2.h>
#define snprintf _snprintf
#endif
#include "miniwget.h"
#include "miniupnpc.h"
#include "upnpcommands.h"
#include "upnperrors.h"

/* protofix() checks if protocol is "UDP" or "TCP" 
 * returns NULL if not */
const char * protofix(const char * proto)
{
	static const char proto_tcp[4] = { 'T', 'C', 'P', 0};
	static const char proto_udp[4] = { 'U', 'D', 'P', 0};
	int i, b;
	for(i=0, b=1; i<4; i++)
		b = b && (   (proto[i] == proto_tcp[i]) 
		          || (proto[i] == (proto_tcp[i] | 32)) );
	if(b)
		return proto_tcp;
	for(i=0, b=1; i<4; i++)
		b = b && (   (proto[i] == proto_udp[i])
		          || (proto[i] == (proto_udp[i] | 32)) );
	if(b)
		return proto_udp;
	return 0;
}

static void DisplayInfos(struct UPNPUrls * urls,
                         struct IGDdatas * data)
{
	char externalIPAddress[16];
	char connectionType[64];
	char status[64];
	char lastconnerr[64];
	unsigned int uptime;
	unsigned int brUp, brDown;
	int r;
	UPNP_GetConnectionTypeInfo(urls->controlURL,
	                           data->servicetype,
							   connectionType);
	if(connectionType[0])
		printf("Connection Type : %s\n", connectionType);
	else
		printf("GetConnectionTypeInfo failed.\n");
	UPNP_GetStatusInfo(urls->controlURL, data->servicetype,
	                   status, &uptime, lastconnerr);
	printf("Status : %s, uptime=%u, LastConnectionError : %s\n",
	       status, uptime, lastconnerr);
	UPNP_GetLinkLayerMaxBitRates(urls->controlURL_CIF, data->servicetype_CIF,
			&brDown, &brUp);
	printf("MaxBitRateDown : %u bps   MaxBitRateUp %u bps\n", brDown, brUp);
	r = UPNP_GetExternalIPAddress(urls->controlURL,
	                          data->servicetype,
							  externalIPAddress);
	if(r != UPNPCOMMAND_SUCCESS)
		printf("GetExternalIPAddress() returned %d\n", r);
	if(externalIPAddress[0])
		printf("ExternalIPAddress = %s\n", externalIPAddress);
	else
		printf("GetExternalIPAddress failed.\n");
}

static void GetConnectionStatus(struct UPNPUrls * urls,
                               struct IGDdatas * data)
{
	unsigned int bytessent, bytesreceived, packetsreceived, packetssent;
	DisplayInfos(urls, data);
	bytessent = UPNP_GetTotalBytesSent(urls->controlURL_CIF, data->servicetype_CIF);
	bytesreceived = UPNP_GetTotalBytesReceived(urls->controlURL_CIF, data->servicetype_CIF);
	packetssent = UPNP_GetTotalPacketsSent(urls->controlURL_CIF, data->servicetype_CIF);
	packetsreceived = UPNP_GetTotalPacketsReceived(urls->controlURL_CIF, data->servicetype_CIF);
	printf("Bytes:   Sent: %8u\tRecv: %8u\n", bytessent, bytesreceived);
	printf("Packets: Sent: %8u\tRecv: %8u\n", packetssent, packetsreceived);
}

static void ListRedirections(struct UPNPUrls * urls,
                             struct IGDdatas * data)
{
	int r;
	int i = 0;
	char index[6];
	char intClient[16];
	char intPort[6];
	char extPort[6];
	char protocol[4];
	char desc[80];
	char enabled[6];
	char rHost[64];
	char duration[16];
	/*unsigned int num=0;
	UPNP_GetPortMappingNumberOfEntries(urls->controlURL, data->servicetype, &num);
	printf("PortMappingNumberOfEntries : %u\n", num);*/
	do {
		snprintf(index, 6, "%d", i);
		rHost[0] = '\0'; enabled[0] = '\0';
		duration[0] = '\0'; desc[0] = '\0';
		extPort[0] = '\0'; intPort[0] = '\0'; intClient[0] = '\0';
		r = UPNP_GetGenericPortMappingEntry(urls->controlURL, data->servicetype,
		                               index,
		                               extPort, intClient, intPort,
									   protocol, desc, enabled,
									   rHost, duration);
		if(r==0)
		/*
			printf("%02d - %s %s->%s:%s\tenabled=%s leaseDuration=%s\n"
			       "     desc='%s' rHost='%s'\n",
			       i, protocol, extPort, intClient, intPort,
				   enabled, duration,
				   desc, rHost);
				   */
			printf("%2d %s %5s->%s:%-5s '%s' '%s'\n",
			       i, protocol, extPort, intClient, intPort,
			       desc, rHost);
		else
			printf("GetGenericPortMappingEntry() returned %d (%s)\n",
			       r, strupnperror(r));
		i++;
	} while(r==0);
}

/* Test function 
 * 1 - get connection type
 * 2 - get extenal ip address
 * 3 - Add port mapping
 * 4 - get this port mapping from the IGD */
static void SetRedirectAndTest(struct UPNPUrls * urls,
                               struct IGDdatas * data,
							   const char * iaddr,
							   const char * iport,
							   const char * eport,
                               const char * proto)
{
	char externalIPAddress[16];
	char intClient[16];
	char intPort[6];
	int r;

	if(!iaddr || !iport || !eport || !proto)
	{
		fprintf(stderr, "Wrong arguments\n");
		return;
	}
	proto = protofix(proto);
	if(!proto)
	{
		fprintf(stderr, "invalid protocol\n");
		return;
	}
	
	UPNP_GetExternalIPAddress(urls->controlURL,
	                          data->servicetype,
							  externalIPAddress);
	if(externalIPAddress[0])
		printf("ExternalIPAddress = %s\n", externalIPAddress);
	else
		printf("GetExternalIPAddress failed.\n");
	
	r = UPNP_AddPortMapping(urls->controlURL, data->servicetype,
	                        eport, iport, iaddr, 0, proto, 0);
	if(r!=UPNPCOMMAND_SUCCESS)
		printf("AddPortMapping(%s, %s, %s) failed with code %d\n",
		       eport, iport, iaddr, r);

	r = UPNP_GetSpecificPortMappingEntry(urls->controlURL,
	                                 data->servicetype,
    	                             eport, proto,
									 intClient, intPort);
	if(r!=UPNPCOMMAND_SUCCESS)
		printf("GetSpecificPortMappingEntry() failed with code %d\n", r);
	
	if(intClient[0]) {
		printf("InternalIP:Port = %s:%s\n", intClient, intPort);
		printf("external %s:%s %s is redirected to internal %s:%s\n",
		       externalIPAddress, eport, proto, intClient, intPort);
	}
}

static void
RemoveRedirect(struct UPNPUrls * urls,
               struct IGDdatas * data,
			   const char * eport,
			   const char * proto)
{
	int r;
	if(!proto || !eport)
	{
		fprintf(stderr, "invalid arguments\n");
		return;
	}
	proto = protofix(proto);
	if(!proto)
	{
		fprintf(stderr, "protocol invalid\n");
		return;
	}
	r = UPNP_DeletePortMapping(urls->controlURL, data->servicetype, eport, proto, 0);
	printf("UPNP_DeletePortMapping() returned : %d\n", r);
}


/* sample upnp client program */
int main(int argc, char ** argv)
{
	char command = 0;
	char ** commandargv = 0;
	int commandargc = 0;
	struct UPNPDev * devlist = 0;
	char lanaddr[16];	/* my ip address on the LAN */
	int i;
	const char * rootdescurl = 0;
	const char * multicastif = 0;
	const char * minissdpdpath = 0;

#ifdef WIN32
	WSADATA wsaData;
	int nResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if(nResult != NO_ERROR)
	{
		fprintf(stderr, "WSAStartup() failed.\n");
		return -1;
	}
#endif
    printf("upnpc : miniupnpc library test client. (c) 2006-2008 Thomas Bernard\n");
    printf("Go to http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/\n"
	       "for more information.\n");
	/* command line processing */
	for(i=1; i<argc; i++)
	{
		if(argv[i][0] == '-')
		{
			if(argv[i][1] == 'u')
				rootdescurl = argv[++i];
			else if(argv[i][1] == 'm')
				multicastif = argv[++i];
			else if(argv[i][1] == 'p')
				minissdpdpath = argv[++i];
			else
			{
				command = argv[i][1];
				i++;
				commandargv = argv + i;
				commandargc = argc - i;
				break;
			}
		}
		else
		{
			fprintf(stderr, "option '%s' invalid\n", argv[i]);
		}
	}

	if(!command || (command == 'a' && commandargc<4)
	   || (command == 'd' && argc<2)
	   || (command == 'r' && argc<2))
	{
		fprintf(stderr, "Usage :\t%s [options] -a ip port external_port protocol\n\t\tAdd port redirection\n", argv[0]);
		fprintf(stderr, "       \t%s [options] -d external_port protocol\n\t\tDelete port redirection\n", argv[0]);
		fprintf(stderr, "       \t%s [options] -s\n\t\tGet Connection status\n", argv[0]);
		fprintf(stderr, "       \t%s [options] -l\n\t\tList redirections\n", argv[0]);
		fprintf(stderr, "       \t%s [options] -r port1 protocol1 [port2 protocol2] [...]\n\t\tAdd all redirections to the current host\n", argv[0]);
		fprintf(stderr, "\nprotocol is UDP or TCP\n");
		fprintf(stderr, "Options:\n");
		fprintf(stderr, "  -u url : bypass discovery process by providing the XML root description url.\n");
		fprintf(stderr, "  -m address : provide ip address of the interface to use for sending SSDP multicast packets.\n");
		fprintf(stderr, "  -p path : use this path for MiniSSDPd socket.\n");
		return 1;
	}

	if( rootdescurl
	  || (devlist = upnpDiscover(2000, multicastif, minissdpdpath, 0)))
	{
		struct UPNPDev * device;
		struct UPNPUrls urls;
		struct IGDdatas data;
		if(devlist)
		{
			printf("List of UPNP devices found on the network :\n");
			for(device = devlist; device; device = device->pNext)
			{
				printf(" desc: %s\n st: %s\n\n",
					   device->descURL, device->st);
			}
		}
		i = 1;
		if( (rootdescurl && UPNP_GetIGDFromUrl(rootdescurl, &urls, &data, lanaddr, sizeof(lanaddr)))
		  || (i = UPNP_GetValidIGD(devlist, &urls, &data, lanaddr, sizeof(lanaddr))))
		{
			switch(i) {
			case 1:
				printf("Found valid IGD : %s\n", urls.controlURL);
				break;
			case 2:
				printf("Found a (not connected?) IGD : %s\n", urls.controlURL);
				printf("Trying to continue anyway\n");
				break;
			case 3:
				printf("UPnP device found. Is it an IGD ? : %s\n", urls.controlURL);
				printf("Trying to continue anyway\n");
				break;
			default:
				printf("Found device (igd ?) : %s\n", urls.controlURL);
				printf("Trying to continue anyway\n");
			}
			printf("Local LAN ip address : %s\n", lanaddr);
			#if 0
			printf("getting \"%s\"\n", urls.ipcondescURL);
			descXML = miniwget(urls.ipcondescURL, &descXMLsize);
			if(descXML)
			{
				/*fwrite(descXML, 1, descXMLsize, stdout);*/
				free(descXML); descXML = NULL;
			}
			#endif

			switch(command)
			{
			case 'l':
				DisplayInfos(&urls, &data);
				ListRedirections(&urls, &data);
				break;
			case 'a':
				SetRedirectAndTest(&urls, &data,
				                   commandargv[0], commandargv[1],
				                   commandargv[2], commandargv[3]);
				break;
			case 'd':
				RemoveRedirect(&urls, &data, commandargv[0], commandargv[1]);
				break;
			case 's':
				GetConnectionStatus(&urls, &data);
				break;
			case 'r':
				for(i=0; i<commandargc; i+=2)
				{
					/*printf("port %s protocol %s\n", argv[i], argv[i+1]);*/
					SetRedirectAndTest(&urls, &data,
					                   lanaddr, commandargv[i],
									   commandargv[i], commandargv[i+1]);
				}
				break;
			default:
				fprintf(stderr, "Unknown switch -%c\n", command);
			}

			FreeUPNPUrls(&urls);
		}
		else
		{
			fprintf(stderr, "No valid UPNP Internet Gateway Device found.\n");
		}
		freeUPNPDevlist(devlist); devlist = 0;
	}
	else
	{
		fprintf(stderr, "No IGD UPnP Device found on the network !\n");
	}

	/*puts("************* HOP ***************");*/

	return 0;
}

