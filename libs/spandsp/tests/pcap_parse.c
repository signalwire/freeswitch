/*
 * SpanDSP - a series of DSP components for telephony
 *
 * pcap_parse.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2009 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Some code from SIPP (http://sf.net/projects/sipp) was used as a model
 * for how to work with PCAP files. That code was authored by Guillaume
 * TEISSIER from FTR&D 02/02/2006, and released under the GPL2 licence.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>

#if defined(HAVE_PCAP_H)
#include <pcap.h>
#endif
#include <netinet/in.h>
#include <netinet/udp.h>
#if defined(__HPUX)  ||  defined(__CYGWIN__)  ||  defined(__FreeBSD__)
#include <netinet/in_systm.h>
#endif
#include <netinet/ip.h>
#if !defined(__CYGWIN__)
#include <netinet/ip6.h>
#endif
#include <string.h>

#include <netinet/in.h>
#include <netinet/udp.h>
#include <time.h>

#include "udptl.h"
#include "spandsp.h"
#include "pcap_parse.h"

#if defined(__HPUX)  ||  defined(__DARWIN)  ||  defined(__CYGWIN__)  ||  defined(__FreeBSD__)

struct iphdr
{
#ifdef _HPUX_LI
    unsigned int ihl:4;
    unsigned int version:4;
#else
    unsigned int version:4;
    unsigned int ihl:4;
#endif
    uint8_t tos;
    uint16_t tot_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t check;
    uint32_t saddr;
    uint32_t daddr;
    /*The options start here. */
};
   
#endif

/* We define our own structures for Ethernet Header and IPv6 Header as they are not available on CYGWIN.
 * We only need the fields, which are necessary to determine the type of the next header.
 * we could also define our own structures for UDP and IPv4. We currently use the structures
 * made available by the platform, as we had no problems to get them on all supported platforms.
 */

typedef struct _ether_hdr
{
    char ether_dst[6];
    char ether_src[6];
    uint16_t ether_type;
} ether_hdr;

typedef struct _linux_sll_hdr
{
    uint16_t packet_type;
    uint16_t arphrd;
    uint16_t slink_length;
    uint8_t bytes[8];
    uint16_t ether_type;
} linux_sll_hdr;

typedef struct _null_hdr
{
    uint32_t pf_type;
} null_hdr;

#if !defined(__CYGWIN__)
typedef struct _ipv6_hdr
{
    char dontcare[6];
    u_int8_t nxt_header; /* we only need the next header, so we can determine, if the next header is UDP or not */
    char dontcare2[33];
} ipv6_hdr;
#endif

char errbuf[PCAP_ERRBUF_SIZE];

int pcap_scan_pkts(const char *file,
                   uint32_t src_addr,
                   uint16_t src_port,
                   uint32_t dest_addr,
                   uint16_t dest_port,
                   pcap_timing_update_handler_t *timing_update_handler,
                   pcap_packet_handler_t *packet_handler,
                   void *user_data)
{
    pcap_t *pcap;
    struct pcap_pkthdr *pkthdr;
    uint8_t *pktdata;
    const uint8_t *body;
    int body_len;
    int total_pkts;
    uint32_t pktlen;
    ether_hdr *ethhdr;
    linux_sll_hdr *sllhdr;
    null_hdr *nullhdr;
    struct iphdr *iphdr;
#if !defined(__CYGWIN__)
    ipv6_hdr *ip6hdr;
#endif
    struct udphdr *udphdr;
    int datalink;
    int packet_type;

    total_pkts = 0;
    if ((pcap = pcap_open_offline(file, errbuf)) == NULL)
    {
        fprintf(stderr, "Can't open PCAP file '%s'\n", file);
        return -1;
    }
    datalink = pcap_datalink(pcap);
    /* DLT_EN10MB seems to apply to all forms of ethernet, not just the 10MB kind. */
    switch (datalink)
    {
    case DLT_EN10MB:
        printf("Datalink type ethernet\n");
        break;
    case DLT_LINUX_SLL:
        printf("Datalink type cooked Linux socket\n");
        break;
    case DLT_NULL:
        printf("Datalink type NULL\n");
        break;
    default:
        fprintf(stderr, "Unsupported data link type %d\n", datalink);
        return -1;
    }

    pkthdr = NULL;
    pktdata = NULL;
#if defined(HAVE_PCAP_NEXT_EX)
    while (pcap_next_ex(pcap, &pkthdr, (const uint8_t **) &pktdata) == 1)
    {
#else
    if ((pkthdr = (struct pcap_pkthdr *) malloc(sizeof(*pkthdr))) == NULL)
    {
        fprintf(stderr, "Can't allocate memory for pcap pkthdr\n");
        return -1;
    }
    while ((pktdata = (uint8_t *) pcap_next(pcap, pkthdr)) != NULL)
    {
#endif
        switch (datalink)
        {
        case DLT_EN10MB:
            ethhdr = (ether_hdr *) pktdata;
            packet_type = ntohs(ethhdr->ether_type);
#if !defined(__CYGWIN__)
            if (packet_type != 0x0800     /* IPv4 */
                &&
                packet_type != 0x86DD)    /* IPv6 */
#else
            if (packet_type != 0x0800)    /* IPv4 */
#endif
            {
                continue;
            }
            iphdr = (struct iphdr *) ((uint8_t *) ethhdr + sizeof(*ethhdr));
            break;
        case DLT_LINUX_SLL:
            sllhdr = (linux_sll_hdr *) pktdata;
            packet_type = ntohs(sllhdr->ether_type);
#if !defined(__CYGWIN__)
            if (packet_type != 0x0800     /* IPv4 */
                &&
                packet_type != 0x86DD)    /* IPv6 */
#else
            if (packet_type != 0x0800)    /* IPv4 */
#endif
            {
                continue;
            }
            iphdr = (struct iphdr *) ((uint8_t *) sllhdr + sizeof(*sllhdr));
            break;
        case DLT_NULL:
            nullhdr = (null_hdr *) pktdata;
            if (nullhdr->pf_type != PF_INET  &&  nullhdr->pf_type != PF_INET6)
                continue;
            iphdr = (struct iphdr *) ((uint8_t *) nullhdr + sizeof(*nullhdr));
            break;
        default:
            continue;
        }
#if 0
        {
            int i;
            printf("--- %d -", pkthdr->caplen);
            for (i = 0;  i < pkthdr->caplen;  i++)
                printf(" %02x", pktdata[i]);
            printf("\n");
        }
#endif
#if !defined(__CYGWIN__)
        if (iphdr  &&  iphdr->version == 6)
        {
            /* ipv6 */
            pktlen = (uint32_t) pkthdr->len - sizeof(*ethhdr) - sizeof(*ip6hdr);
            ip6hdr = (ipv6_hdr *) (void *) iphdr;
            if (ip6hdr->nxt_header != IPPROTO_UDP)
                continue;
            udphdr = (struct udphdr *) ((uint8_t *) ip6hdr + sizeof(*ip6hdr));
        }
        else
#endif
        {
            /* ipv4 */
            if (iphdr->protocol != IPPROTO_UDP)
                continue;
#if defined(__DARWIN)  ||  defined(__CYGWIN__)  ||  defined(__FreeBSD__)
            udphdr = (struct udphdr *) ((uint8_t *) iphdr + (iphdr->ihl << 2) + 4);
            pktlen = (uint32_t) ntohs(udphdr->uh_ulen);
#elif defined ( __HPUX)
            udphdr = (struct udphdr *) ((uint8_t *) iphdr + (iphdr->ihl << 2));
            pktlen = (uint32_t) pkthdr->len - sizeof(*ethhdr) - sizeof(*iphdr);
#else
            udphdr = (struct udphdr *) ((uint8_t *) iphdr + (iphdr->ihl << 2));
            pktlen = (uint32_t) ntohs(udphdr->len);
#endif
        }

        timing_update_handler(user_data, &pkthdr->ts);

        if (src_addr  &&  ntohl(iphdr->saddr) != src_addr)
            continue;
#if defined(__DARWIN)  ||  defined(__CYGWIN__)  ||  defined(__FreeBSD__)
        if (src_port  &&  ntohs(udphdr->uh_sport) != src_port)
#else
        if (src_port  &&  ntohs(udphdr->source) != src_port)
#endif
            continue;
        if (dest_addr  &&  ntohl(iphdr->daddr) != dest_addr)
            continue;
#if defined(__DARWIN)  ||  defined(__CYGWIN__)  ||  defined(__FreeBSD__)
        if (dest_port  &&  ntohs(udphdr->uh_dport) != dest_port)
#else
        if (dest_port  &&  ntohs(udphdr->dest) != dest_port)
#endif
            continue;

        if (pkthdr->len != pkthdr->caplen)
        {
            fprintf(stderr, "Truncated packet - total len = %d, captured len = %d\n", pkthdr->len, pkthdr->caplen);
            exit(2);
        }
        body = (const uint8_t *) udphdr;
        body += sizeof(struct udphdr);
        body_len = pktlen - sizeof(struct udphdr);
        packet_handler(user_data, body, body_len);

        total_pkts++;
    }
    fprintf(stderr, "In pcap %s there were %d accepted packets\n", file, total_pkts);
    pcap_close(pcap);

    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
