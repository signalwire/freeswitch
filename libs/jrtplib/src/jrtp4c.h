/*
 * CCRTP4C (A wrapper for ccRTP so you can use it in C programs)
 * Copyright Anthony Minessale II <anthmct@yahoo.com>
 *
 */
#ifndef CCRTP4C_H
#define CCRTP4C_H

#ifdef __cplusplus
#include <rtpsession.h>
#include <rtppacket.h>
#include <rtpudpv4transmitter.h>
#include <rtpipv4address.h>
#include <rtpsessionparams.h>
#include <rtperrors.h>
#ifndef WIN32
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <winsock2.h>
#endif // WIN32
#if 0
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>
#endif

extern "C" {
#endif

	typedef enum {
		SR,
		RR,
		SDES,
		BYE,
		APP,
		UNK
	} jrtp4c_packet_t;

#ifndef uint32_t
#ifdef WIN32
	typedef unsigned int uint32_t;
#else
#include <stdint.h>
#endif
#endif

	struct jrtp4c;
	struct jrtp4c *jrtp4c_new(char *rx_ip, int rx_port, char *tx_ip, int tx_port, int payload, int sps, const char **err);
	void jrtp4c_destroy(struct jrtp4c **jrtp4c);
	int jrtp4c_read(struct jrtp4c *jrtp4c, void *data, int datalen);
	int jrtp4c_write(struct jrtp4c *jrtp4c, void *data, int datalen, uint32_t ts);
	uint32_t jrtp4c_start(struct jrtp4c *jrtp4c);
	uint32_t jrtp4c_get_ssrc(struct jrtp4c *jrtp4c);
	void jrtp4c_killread(struct jrtp4c *jrtp4c);
#ifdef __cplusplus
}
#endif

#endif
