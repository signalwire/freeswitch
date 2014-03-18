/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2006 Nokia Corporation.
 *
 * Contact: Pekka Pessi <pekka.pessi@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

/**@CFILE tport_logging.c Logging transported messages.
 *
 * See tport.docs for more detailed description of tport interface.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <Martti.Mela@nokia.com>
 *
 * @date Created: Fri Mar 24 08:45:49 EET 2006 ppessi
 */

#include "config.h"
#include "msg_internal.h"

#include "tport_internal.h"

#include <sofia-sip/su.h>
#include <sofia-sip/su_string.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>

/**@var TPORT_LOG
 *
 * Environment variable determining if parsed message contents are logged.
 *
 * If the TPORT_LOG environment variable is set, the tport module logs the
 * contents of parsed messages. This eases debugging the signaling greatly.
 *
 * @sa TPORT_DUMP, TPORT_DEBUG, tport_log
 */
#ifdef DOXYGEN
extern char const TPORT_LOG[];	/* dummy declaration for Doxygen */
#endif

/**@var TPORT_DUMP
 *
 * Environment variable for transport data dump.
 *
 * The received and sent data is dumped to the file specified by TPORT_DUMP
 * environment variable. This can be used to save message traces and help
 * hairy debugging tasks.
 *
 * @sa TPORT_LOG, TPORT_DEBUG, tport_log
 */
#ifdef DOXYGEN
extern char const TPORT_DUMP[];	/* dummy declaration for Doxygen */
#endif

/**@var TPORT_CAPT
 *
 * Environment variable for transport data capturing.
 *
 * The received and sent data is dumped to the capture server specified by TPORT_CAPT
 * environment variable. This can be used to save message traces into database and help
 * hairy debugging tasks.
 *
 * @sa TPORT_LOG, TPORT_DEBUG, TPORT_CAPT, tport_log
 */
#ifdef DOXYGEN
extern char const TPORT_CAPT[];	/* dummy declaration for Doxygen */
#endif


/**@var TPORT_DEBUG
 *
 * Environment variable determining the debug log level for @b tport module.
 *
 * The TPORT_DEBUG environment variable is used to determine the debug logging
 * level for @b tport module. The default level is 3.
 *
 * @sa <sofia-sip/su_debug.h>, tport_log, SOFIA_DEBUG
 */
#ifdef DOXYGEN
extern char const TPORT_DEBUG[]; /* dummy declaration for Doxygen */
#endif

/**Debug log for @b tport module.
 *
 * The tport_log is the log object used by @b tport module. The level of
 * #tport_log is set using #TPORT_DEBUG environment variable.
 */
su_log_t tport_log[] = {
  SU_LOG_INIT("tport", "TPORT_DEBUG", SU_DEBUG)
};



/** Initialize logging. */
int tport_open_log(tport_master_t *mr, tagi_t *tags)
{
  int n;
  int log_msg = mr->mr_log != 0;
  char const *dump = NULL;
  char const *capt = NULL;;
  
  if(mr->mr_capt_name) capt = mr->mr_capt_name;
  
  n = tl_gets(tags,
	      TPTAG_LOG_REF(log_msg),
	      TPTAG_DUMP_REF(dump),
	      TPTAG_CAPT_REF(capt),
	      TAG_END());

  if (getenv("MSG_STREAM_LOG") != NULL || getenv("TPORT_LOG") != NULL)
    log_msg = 1;
  mr->mr_log = log_msg ? MSG_DO_EXTRACT_COPY : 0;

  if (getenv("TPORT_CAPT"))
    capt = getenv("TPORT_CAPT");
  if (getenv("MSG_DUMP"))
    dump = getenv("MSG_DUMP");
  if (getenv("TPORT_DUMP"))
    dump = getenv("TPORT_DUMP");
 
  if(capt) {

        char *captname, *p, *host_s;
        char port[10];
        su_addrinfo_t *ai = NULL, hints[1] = {{ 0 }};
        unsigned len =0;

        if (mr->mr_capt_name && mr->mr_capt_sock && strcmp(capt, mr->mr_capt_name) == 0)                
              return n;

        captname = su_strdup(mr->mr_home, capt);
        if (captname == NULL)
              return n;
                           
        if(strncmp(captname, "udp:",4) != 0) {
              su_log("tport_open_log: capturing. Only udp protocol supported [%s]\n", captname);          
              return n;
        } 
        
        /* separate proto and host */
        p = captname+4;
        if( (*(p)) == '\0') {
                su_log("malformed ip address\n");
                return n;
        }
        host_s = p;

        if( (p = strrchr(p+1, ':')) == 0 ) {
                su_log("no host or port specified\n");
                return n;
        }
 
        /*the address contains a port number*/
        *p = '\0';
        p++;

        if (atoi(p) <1024  || atoi(p)>65536)
        {
                su_log("invalid port number; must be in [1024,65536]\n");
                return n;
        }

        strncpy(port, p, sizeof(port));
                        
        *p = '\0'; 
        
        /* check if we have [] */
        if (host_s[0] == '[') {
              len = strlen(host_s + 1) - 1;              
              if(host_s[len+1] != ']') {
                su_log("bracket not closed\n");
                return n;            
            }            
            memmove(host_s, host_s + 1, len);
            host_s[len] = '\0';
        }                              

        /* and again */
        captname = su_strdup(mr->mr_home, capt);
        if (captname == NULL) return n;
        
        su_free(mr->mr_home, mr->mr_capt_name);
        mr->mr_capt_name = captname;

        if (mr->mr_capt_sock)
          su_close(mr->mr_capt_sock), mr->mr_capt_sock = 0;        

        /* HINTS && getaddrinfo */
        hints->ai_flags = AI_NUMERICSERV;
        hints->ai_family = AF_UNSPEC; 
        hints->ai_socktype = SOCK_DGRAM;
        hints->ai_protocol = IPPROTO_UDP;
        

        if (su_getaddrinfo(host_s, port, hints, &ai)) {
            su_perror("capture: su_getaddrinfo()");
            return n;
        }
        
	mr->mr_capt_sock = su_socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (mr->mr_capt_sock == INVALID_SOCKET) {
	    su_perror("capture: invalid socket");
	    return n;
	}

	su_setblocking(mr->mr_capt_sock, 0);         /* Don't block */

	if (connect(mr->mr_capt_sock, ai->ai_addr, (socklen_t)(ai->ai_addrlen)) == -1) {
	    if (errno != EINPROGRESS) {
		    su_perror("capture: socket connect");
		    return n;
	    }	                
	}
		
	su_freeaddrinfo(ai);        
  }
  else if(mr->mr_capt_sock) {
      /* close capture server*/
      su_close(mr->mr_capt_sock);
      mr->mr_capt_sock = 0;
  }

  if (dump) {
    time_t now;
    char *dumpname;
    
    if (mr->mr_dump && strcmp(dump, mr->mr_dump) == 0)
      return n;
    dumpname = su_strdup(mr->mr_home, dump);
    if (dumpname == NULL)
      return n;
    su_free(mr->mr_home, mr->mr_dump);
    mr->mr_dump = dumpname;

    if (mr->mr_dump_file && mr->mr_dump_file != stdout)
      fclose(mr->mr_dump_file), mr->mr_dump_file = NULL;

    if (strcmp(dumpname, "-"))
      mr->mr_dump_file = fopen(dumpname, "ab"); /* XXX */
    else
      mr->mr_dump_file = stdout;

    if (mr->mr_dump_file) {
      time(&now);
      fprintf(mr->mr_dump_file, "dump started at %s\n\n", ctime(&now));
    }
  }

  return n;
}

/** Create log stamp */
void tport_stamp(tport_t const *self, msg_t *msg,
		 char stamp[128], char const *what,
		 size_t n, char const *via,
		 su_time_t now)
{
  char label[24] = "";
  char *comp = "";
  char name[SU_ADDRSIZE] = "";
  su_sockaddr_t const *su;
  unsigned short second, minute, hour;
  /* should check for ifdef HAVE_LOCALTIME_R instead -_- */
#if defined(HAVE_GETTIMEOFDAY) || defined(HAVE_CLOCK_MONOTONIC)
  struct tm nowtm = { 0 };
  time_t nowtime = (now.tv_sec - SU_TIME_EPOCH); /* see su_time0.c 'now' is not really 'now', so we decrease it by SU_TIME_EPOCH */
#endif

  assert(self); assert(msg);

#if defined(HAVE_GETTIMEOFDAY) || defined(HAVE_CLOCK_MONOTONIC)
  localtime_r(&nowtime, &nowtm);
  second = nowtm.tm_sec;
  minute = nowtm.tm_min;
  hour = nowtm.tm_hour;
#else
  second = (unsigned short)(now.tv_sec % 60);
  minute = (unsigned short)((now.tv_sec / 60) % 60);
  hour = (unsigned short)((now.tv_sec / 3600) % 24);
#endif

  su = msg_addr(msg);

#if SU_HAVE_IN6
  if (su->su_family == AF_INET6) {
    if (su->su_sin6.sin6_flowinfo)
      snprintf(label, sizeof(label), "/%u", ntohl(su->su_sin6.sin6_flowinfo));
  }
#endif

  if (msg_addrinfo(msg)->ai_flags & TP_AI_COMPRESSED)
    comp = ";comp=sigcomp";

  su_inet_ntop(su->su_family, SU_ADDR(su), name, sizeof(name));

  snprintf(stamp, 128,
	   "%s "MOD_ZU" bytes %s %s/[%s]:%u%s%s at %02u:%02u:%02u.%06lu:\n",
	   what, (size_t)n, via, self->tp_name->tpn_proto,
	   name, ntohs(su->su_port), label[0] ? label : "", comp,
	   hour, minute, second, now.tv_usec);
}

/** Dump the data from the iovec */
void tport_dump_iovec(tport_t const *self, msg_t *msg,
		      size_t n, su_iovec_t const iov[], size_t iovused,
		      char const *what, char const *how)
{
  tport_master_t *mr;
  char stamp[128];
  size_t i;

  assert(self); assert(msg);

  mr = self->tp_master;
  if (!mr->mr_dump_file)
    return;

  tport_stamp(self, msg, stamp, what, n, how, su_now());
  fputs(stamp, mr->mr_dump_file);

  for (i = 0; i < iovused && n > 0; i++) {
    size_t len = iov[i].mv_len;
    if (len > n)
      len = n;
    if (fwrite(iov[i].mv_base, len, 1, mr->mr_dump_file) != len)
      break;
    n -= len;
  }

  fputs("\v\n", mr->mr_dump_file);
  fflush(mr->mr_dump_file);
}

/** Capture the data from the iovec */
void tport_capt_msg(tport_t const *self, msg_t *msg, size_t n, 
                    su_iovec_t const iov[], size_t iovused, char const *what)
{

   int buflen = 0, error;
   su_sockaddr_t const *su, *su_self;
   struct hep_hdr hep_header;
#if __sun__
   struct hep_iphdr hep_ipheader = {{{{0}}}};
#else
   struct hep_iphdr hep_ipheader = {{0}};   
#endif
#if SU_HAVE_IN6
   struct hep_ip6hdr hep_ip6header = {{{{0}}}};
#endif   
   int eth_frame_len = 8000;
   char* buffer;
   size_t i, dst = 0;   
   tport_master_t *mr;

   assert(self); assert(msg);

   su = msg_addr(msg);
   su_self = self->tp_pri->pri_primary->tp_addr;

   mr = self->tp_master;

   /* If we don't have socket, go out */
   if (!mr->mr_capt_sock) {
         su_log("error: capture socket is not open\n");
         return;
   }

   /*buffer for ethernet frame*/
   buffer = (void*)malloc(eth_frame_len);

   /* VOIP Header */   
   hep_header.hp_v =  1;
   hep_header.hp_f = su->su_family; 
   /* Header Length */   
   hep_header.hp_l = sizeof(struct hep_hdr);   
   
   /* PROTOCOL */
   if(strcmp(self->tp_name->tpn_proto, "tcp") == 0) hep_header.hp_p = IPPROTO_TCP;
   else if(strcmp(self->tp_name->tpn_proto, "tls") == 0) hep_header.hp_p = IPPROTO_IDP; /* FAKE*/
   else if(strcmp(self->tp_name->tpn_proto, "sctp") == 0) hep_header.hp_p = IPPROTO_SCTP;
   else if(strcmp(self->tp_name->tpn_proto, "ws") == 0) hep_header.hp_p = IPPROTO_TCP;
   else if(strcmp(self->tp_name->tpn_proto, "wss") == 0) hep_header.hp_p = IPPROTO_TCP;
   else hep_header.hp_p = IPPROTO_UDP; /* DEFAULT UDP */

   /* Check destination */         
   if(strncmp("sent", what, 4) == 0) dst = 1;
      
   /* copy destination and source IPs*/
   if(su->su_family == AF_INET) {

       memcpy(dst ? &hep_ipheader.hp_dst : &hep_ipheader.hp_src, &su->su_sin.sin_addr.s_addr, sizeof(su->su_sin.sin_addr.s_addr));
       memcpy(dst ? &hep_ipheader.hp_src : &hep_ipheader.hp_dst, &su_self->su_sin.sin_addr.s_addr, sizeof(su_self->su_sin.sin_addr.s_addr));
       hep_header.hp_l += sizeof(struct hep_iphdr);
   }
#if SU_HAVE_IN6
   else {   
       memcpy(dst ? &hep_ip6header.hp6_dst : &hep_ip6header.hp6_src, &su->su_sin.sin_addr.s_addr, sizeof(su->su_sin.sin_addr.s_addr));
       memcpy(dst ? &hep_ip6header.hp6_src : &hep_ip6header.hp6_dst, &su_self->su_sin.sin_addr.s_addr, sizeof(su_self->su_sin.sin_addr.s_addr));
       hep_header.hp_l += sizeof(struct hep_ip6hdr);       
   }
#endif     

   hep_header.hp_dport = dst ? su->su_port : su_self->su_port;
   hep_header.hp_sport = dst ? su_self->su_port : su->su_port;

      
   /* Copy hepheader */
   memset(buffer, '\0', eth_frame_len);
   memcpy(buffer, &hep_header, sizeof(struct hep_hdr));
   buflen = sizeof(struct hep_hdr);
   
   if(su->su_family == AF_INET) {
       memcpy(buffer + buflen, &hep_ipheader, sizeof(struct hep_iphdr));
       buflen += sizeof(struct hep_iphdr);      
   }
#if SU_HAVE_IN6   
   else if(su->su_family == AF_INET6) {
       memcpy(buffer+buflen, &hep_ip6header, sizeof(struct hep_ip6hdr));
       buflen += sizeof(struct hep_ip6hdr);   
   }   
#endif 
   else {
       su_perror("error: tport_logging: capture: unsupported protocol family");
       goto done;
   }           
   
   for (i = 0; i < iovused && n > 0; i++) {
       size_t len = iov[i].mv_len;
       if (len > n)
            len = n;   
       /* if the packet too big for us */
       if((buflen + len) > eth_frame_len) 
              break;

      memcpy(buffer + buflen , (void*)iov[i].mv_base, len);
      buflen +=len;
      n -= len;
   }
   
   /* check if we have error i.e. capture server is down */
   if ((error = su_soerror(mr->mr_capt_sock))) {
      su_perror("error: tport_logging: capture socket error");
      goto done;
   }

   su_send(mr->mr_capt_sock, buffer, buflen, 0);   

done:
   /* Now we release it */
   if(buffer) free(buffer);  
   return;
}


/** Log the message. */
void tport_log_msg(tport_t *self, msg_t *msg,
		   char const *what, char const *via,
		   su_time_t now)
{
  char stamp[128];
  msg_iovec_t iov[80];
  size_t i, iovlen = msg_iovec(msg, iov, 80);
  size_t linelen = 0, n, logged = 0, truncated = 0;
  int skip_lf = 0;
  int j, unprintable = 0;

#define MSG_SEPARATOR \
  "------------------------------------------------------------------------\n"
#define MAX_LINELEN 2047

  for (i = n = 0; i < iovlen && i < 80; i++)
    n += iov[i].mv_len;

  tport_stamp(self, msg, stamp, what, n, via, now);
  su_log("%s   " MSG_SEPARATOR, stamp);

  for (i = 0; truncated == 0 && i < iovlen && i < 80; i++) {
    char *s = iov[i].mv_base, *end = s + iov[i].mv_len;

    if (skip_lf && s < end && s[0] == '\n') { s++; logged++; skip_lf = 0; }

    while (s < end) {
		if (s[0] == '\0') {
			truncated = logged;
			break;
		}

		n = su_strncspn(s, end - s, "\r\n");

		if (linelen + n > MAX_LINELEN) {
			n = MAX_LINELEN - linelen;
			truncated = logged + n;
		}
		
		if (!unprintable) {
			for (j = 0; j < 4; j++) {
				if (s[j] == 0) break;
				if (s[j] != 9 && s[j] != 10 && s[j] != 13 && (s[j] < 32 || s[j] > 126)) {
					unprintable++;
				}
			}
		}

		if (unprintable) {
			if (unprintable == 1)
				su_log("\n   <ENCODED DATA>");
			unprintable++;
		} else {
			su_log("%s%.*s", linelen > 0 ? "" : "   ", (int)n, s);
		}

		s += n, linelen += n, logged += n;

		if (truncated)
			break;
		if (s == end)
			break;
		
		linelen = 0;
		su_log("\n");
		
		/* Skip eol */
		if (s[0] == '\r') {
			s++, logged++;
			if (s == end) {
				skip_lf = 1;
				continue;
			}
		}

		if (s[0] == '\n') {
			s++, logged++;
		}
    }
  }

  su_log("%s   " MSG_SEPARATOR, linelen > 0 ? "\n" : "");

  if (!truncated && i == 80)
    truncated = logged;

  if (truncated)
    su_log("   *** message truncated at "MOD_ZU" ***\n", truncated);
}
