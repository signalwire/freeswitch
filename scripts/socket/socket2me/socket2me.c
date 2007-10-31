/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 *    * Neither the name of [original copyright holder] nor the names of
 *      its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written
 *      permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Copyright (C) 2007, Anthony Minessale II <anthmct@yahoo.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h> 
#include <string.h>
#include <unistd.h>
#include <spandsp.h>


#define SOCKET2ME_DEBUG 0
#define MAXPENDING 10000
#define RCVBUFSIZE 4198
#define PORT_MIN 9000
#define PORT_MAX 10000

static int use_port = PORT_MIN;

static void phase_b_handler(t30_state_t *s, void *user_data, int result)
{
    int session;
    session = (intptr_t) user_data;

    printf("Phase B handler on session %d - (0x%X) %s\n", session, result, t30_frametype(result));
}     

static void phase_d_handler(t30_state_t *s, void *user_data, int result)
{
    int session;
    char ident[21];
    t30_stats_t t;

    session = (intptr_t) user_data;

    printf("Phase D handler on session %d - (0x%X) %s\n", session, result, t30_frametype(result));
    t30_get_transfer_statistics(s, &t);
    printf( "Phase D: bit rate %d\n", t.bit_rate);
    printf( "Phase D: ECM %s\n", (t.error_correcting_mode)  ?  "on"  :  "off");
    printf( "Phase D: pages transferred %d\n", t.pages_transferred);
    printf( "Phase D: image size %d x %d\n", t.width, t.length);
    printf( "Phase D: image resolution %d x %d\n", t.x_resolution, t.y_resolution);
    printf( "Phase D: bad rows %d\n", t.bad_rows);
    printf( "Phase D: longest bad row run %d\n", t.longest_bad_row_run);
    printf( "Phase D: compression type %d\n", t.encoding);
    printf( "Phase D: image size %d\n", t.image_size);
    t30_get_local_ident(s, ident);
    printf( "Phase D: local ident '%s'\n", ident);
    t30_get_far_ident(s, ident);
    printf( "Phase D: remote ident '%s'\n", ident);
}

static void phase_e_handler(t30_state_t *s, void *user_data, int result)
{
    int session;
    t30_stats_t t;
    const char *u;
    char ident[21];
    
    session = (intptr_t) user_data;
    printf("Phase E handler on session %d - (%d) %s\n", session, result, t30_completion_code_to_str(result));    
    t30_get_transfer_statistics(s, &t);
    printf( "Phase E: bit rate %d\n", t.bit_rate);
    printf( "Phase E: ECM %s\n", (t.error_correcting_mode)  ?  "on"  :  "off");
    printf( "Phase E: pages transferred %d\n", t.pages_transferred);
    printf( "Phase E: image size %d x %d\n", t.width, t.length);
    printf( "Phase E: image resolution %d x %d\n", t.x_resolution, t.y_resolution);
    printf( "Phase E: bad rows %d\n", t.bad_rows);
    printf( "Phase E: longest bad row run %d\n", t.longest_bad_row_run);
    printf( "Phase E: coding method %s\n", t4_encoding_to_str(t.encoding));
    printf( "Phase E: image size %d bytes\n", t.image_size);
    t30_get_local_ident(s, ident);
    printf( "Phase E: local ident '%s'\n", ident);
    t30_get_far_ident(s, ident);
    printf( "Phase E: remote ident '%s'\n", ident);
    if ((u = t30_get_far_country(s)))
        printf( "Phase E: Remote was made in '%s'\n", u);
    if ((u = t30_get_far_vendor(s)))
        printf( "Phase E: Remote was made by '%s'\n", u);
    if ((u = t30_get_far_model(s)))
        printf( "Phase E: Remote is model '%s'\n", u);
}

static int document_handler(t30_state_t *s, void *user_data, int event)
{
    int session;
    
    session = (intptr_t) user_data;
    printf("Document handler on session %d - event %d\n", session, event);
    return FALSE;
}

void die(char *error_str)
{
    perror(error_str);
    exit(1);
}


static void set_vars(char *data)
{
  char *start, *end, *p=malloc(strlen(data)+1);
  char name[8192],value[8192];

  if(!p) {
    perror("malloc");
    exit(1);
  }

  memcpy(p,data,strlen(data)+1);
  start=p;


  while(start != 0 && *start != '\0') {
    if(end = strchr(start,'\r')) {
      *end = '\0';
      if(*(end + 1) == '\n') {
	end+=2;
      } else {
	end++;
      }
    } else {
      return;
    }

    sscanf(start,"%s: %s",name,value);
    setenv(name,value,1);
    start = end;
  }
  free(p);
}



static int cheezy_get_var(char *data, char *name, char *buf, size_t buflen)
{
  char *p=data;

  /* the old way didnt make sure that variable values were used for the name hunt
   * and didnt ensure that only a full match of the variable name was used
   */

  do {
    if(!strncmp(p,name,strlen(name)) && *(p+strlen(name))==':') break;
  } while((p = (strstr(p,"\n")+1))!=(char *)1);


  if (p != (char *)1 && *p!='\0') {
    char *v, *e;

    if ((v = strchr(p, ':'))) {
      v++;
      while(v && *v == ' ') {
	v++;
      }
      if (v)  {
	if (!(e = strchr(v, '\r'))) {
	  e = strchr(v, '\n');
	}
      }
			
      if (v && e) {
	int cplen;
	int len = e - v;
	
	if (len > buflen - 1) {
	  cplen = buflen -1;
	} else {
	  cplen = len;
	}
	
	strncpy(buf, v, cplen);
	*(buf+cplen) = '\0';
	return 1;
      }
      
    }
  }
  return 0;
}

void client_run(int client_socket, char *local_ip, int local_port, char *remote_ip, int remote_port)
{
    char sendbuf[RCVBUFSIZE], recvbuf[RCVBUFSIZE], infobuf[RCVBUFSIZE];
	struct sockaddr_in addr = {0}, sendaddr = {0};
    int read_bytes;
	int usock;
	int reuse_addr = 1;
    fax_state_t fax;
	char tmp[512], fn[512], *file_name = "/tmp/test.tiff";
	int send_fax = FALSE;
	int g711 = 0;
	int pcmu = 0;

	snprintf(sendbuf, sizeof(sendbuf), "connect\n\n");
	send(client_socket, sendbuf, strlen(sendbuf), 0);

    if ((read_bytes = recv(client_socket, infobuf, sizeof(infobuf), 0)) < 0) {
        die("recv() failed");
	}

#if SOCKET2ME_DEBUG
	printf("READ [%s]\n", infobuf);
#endif

	if (cheezy_get_var(infobuf, "Channel-Read-Codec-Name", tmp, sizeof(tmp))) {
		if (!strcasecmp(tmp, "pcmu")) {
			g711 = 1;
			pcmu = 1;
		} else if (!strcasecmp(tmp, "pcma")) {
			g711 = 1;
		}
	}


	snprintf(sendbuf, sizeof(sendbuf), "sendmsg\n"
			 "call-command: unicast\n"
			 "local-ip: %s\n"
			 "local-port: %d\n"
			 "remote-ip: %s\n"
			 "remote-port: %d\n"
			 "transport: udp\n"
			 "%s"
			 "\n",
			 local_ip, local_port,
			 remote_ip, remote_port,
			 g711 ? "flags: native\n" : ""
			 );

	
	if (cheezy_get_var(infobuf, "variable_fax_file_name", fn, sizeof(fn))) {
		file_name = fn;
	}

	if (cheezy_get_var(infobuf, "variable_fax_mode", tmp, sizeof(tmp))) {
		if (!strcasecmp(tmp, "send")) {
			send_fax = TRUE;
		}
	}

	if (cheezy_get_var(infobuf, "variable_fax_preexec", tmp, sizeof(tmp))) {
	  set_vars(infobuf);
	  system(tmp);
	}

#if SOCKET2ME_DEBUG
	printf("SEND: [%s]\n", sendbuf);
#endif
	send(client_socket, sendbuf, strlen(sendbuf), 0);

	memset(recvbuf, 0, sizeof(recvbuf));
    if ((read_bytes = recv(client_socket, recvbuf, sizeof(recvbuf), 0)) < 0) {
        die("recv() failed");
	}
#if SOCKET2ME_DEBUG
	printf("READ [%s]\n", recvbuf);
#endif
	
	if ((usock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        die("socket() failed");
	}

	setsockopt(usock, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    /*addr.sin_addr.s_addr = inet_addr(remote_ip);*/
    addr.sin_port = htons(remote_port);

    sendaddr.sin_family = AF_INET;
    sendaddr.sin_addr.s_addr = inet_addr(local_ip);
    sendaddr.sin_port = htons(local_port);
	
    if (bind(usock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        die("bind() failed");
	}

	printf("%s Fax filename: [%s] from %s:%d -> %s:%d\n", send_fax ? "Sending" : "Receiving", file_name, local_ip, local_port, remote_ip, remote_port);
	
    fax_init(&fax, send_fax);
    t30_set_local_ident(&fax.t30_state, "Socket 2 ME");
    t30_set_header_info(&fax.t30_state, "Socket 2 ME");
	if (send_fax) {
		t30_set_tx_file(&fax.t30_state, file_name, -1, -1);
	} else {
		t30_set_rx_file(&fax.t30_state, file_name, -1);
	}
    t30_set_phase_b_handler(&fax.t30_state, phase_b_handler, NULL);
    t30_set_phase_d_handler(&fax.t30_state, phase_d_handler, NULL);
    t30_set_phase_e_handler(&fax.t30_state, phase_e_handler, NULL);
    t30_set_document_handler(&fax.t30_state, document_handler, NULL);

    t30_set_ecm_capability(&fax.t30_state, TRUE);
    t30_set_supported_compressions(&fax.t30_state, T30_SUPPORT_T4_1D_COMPRESSION | T30_SUPPORT_T4_2D_COMPRESSION | T30_SUPPORT_T6_COMPRESSION);

    t30_set_supported_image_sizes(&fax.t30_state, T30_SUPPORT_US_LETTER_LENGTH | T30_SUPPORT_US_LEGAL_LENGTH | T30_SUPPORT_UNLIMITED_LENGTH
                                  | T30_SUPPORT_215MM_WIDTH | T30_SUPPORT_255MM_WIDTH | T30_SUPPORT_303MM_WIDTH);
    t30_set_supported_resolutions(&fax.t30_state, T30_SUPPORT_STANDARD_RESOLUTION | T30_SUPPORT_FINE_RESOLUTION | T30_SUPPORT_SUPERFINE_RESOLUTION
                                  | T30_SUPPORT_R8_RESOLUTION | T30_SUPPORT_R16_RESOLUTION);

	for (;;) {
		struct sockaddr_in local_addr = {0};
        size_t cliAddrLen = sizeof(local_addr);
		unsigned char audiobuf[1024], rawbuf[1024], outbuf[1024];
		short *usebuf = NULL;
		int tx, tx_bytes, bigger, sample_count;
		fd_set ready;

		FD_ZERO(&ready);

		FD_SET(usock, &ready);
		FD_SET(client_socket, &ready);
		
		bigger = usock > client_socket ? usock : client_socket;
		select(++bigger, &ready, NULL, NULL, NULL);
		
		if (FD_ISSET(client_socket, &ready)) {
			memset(recvbuf, 0, sizeof(recvbuf));
			if ((read_bytes = recv(client_socket, recvbuf, sizeof(recvbuf), 0)) < 0) {
				die("recv() failed");
			}

			if (read_bytes == 0) {
				break;
			}
#if SOCKET2ME_DEBUG
			printf("READ [%s]\n", recvbuf);
#endif
		}

		if (!FD_ISSET(usock, &ready)) {
			continue;
		}

        if ((read_bytes = recvfrom(usock, audiobuf, sizeof(audiobuf), 0, (struct sockaddr *) &local_addr, &cliAddrLen)) < 0) {
			die("recvfrom() failed");
		}

		if (g711) {
			int i;
			short *rp = (short *) rawbuf;
			
			for (i = 0; i < read_bytes; i++) {
				if (pcmu) {
					rp[i] = ulaw_to_linear(audiobuf[i]);
				} else {
					rp[i] = alaw_to_linear(audiobuf[i]);
				}
			}
			usebuf = rp;
			sample_count = read_bytes;
		} else {
			usebuf = (short *) audiobuf;
			sample_count = read_bytes / 2;
		}
		
		fax_rx(&fax, usebuf, sample_count);
#if SOCKET2ME_DEBUG
		printf("Handling client %s:%d %d bytes\n", inet_ntoa(local_addr.sin_addr), ntohs(local_addr.sin_port), read_bytes);
#endif

		
		if ((tx = fax_tx(&fax, (short *)outbuf, sample_count)) < 0) {
            printf("Fax Error\n");
			break;
        } else if (!tx) {
			continue;
		}
		

		if (g711) {
			int i;
			short *bp = (short *) outbuf;
			for (i = 0; i < tx; i++) {
				if (pcmu) {
					rawbuf[i] = linear_to_ulaw(bp[i]);
				} else {
					rawbuf[i] = linear_to_alaw(bp[i]);
				}
			}
			usebuf = (short *) rawbuf;
			tx_bytes = tx;
		} else {
			usebuf = (short *)outbuf;
			tx_bytes = tx * 2;
		}	
		

		cliAddrLen = sizeof(sendaddr);
        if (sendto(usock, usebuf, tx_bytes, 0, (struct sockaddr *) &sendaddr, sizeof(sendaddr)) != tx_bytes) {
			die("sendto() sent a different number of bytes than expected");
		}
	}

	close(client_socket);
	close(usock);
		
    t30_terminate(&fax.t30_state);
    fax_release(&fax);

    if (cheezy_get_var(infobuf, "variable_fax_postexec", tmp, sizeof(tmp))) {
      set_vars(infobuf);
      system(tmp);
    }
	printf("Done\n");
	snprintf(sendbuf, sizeof(sendbuf), "hangup\n\n");
	send(client_socket, sendbuf, strlen(sendbuf), 0);
	
}

int client_accept(int servSock)
{
    int client_sock;                    
    struct sockaddr_in echoClntAddr; 
    unsigned int clntLen;            

    clntLen = sizeof(echoClntAddr);
    
    if ((client_sock = accept(servSock, (struct sockaddr *) &echoClntAddr, &clntLen)) < 0) {
		die("accept() failed");
	}
    
    printf("Client Connect: [%s]\n", inet_ntoa(echoClntAddr.sin_addr));

    return client_sock;
}

int main(int argc, char *argv[]) 
{
	int servSock, client_sock;
	int port = 8084;
	struct sockaddr_in addr;
	pid_t pid;
	unsigned int process_count = 0;
	int reuse_addr = 1;
	char *local_ip = NULL;
	char *remote_ip = NULL;
	char *signal_port_name = NULL;

	if (argc > 0) {
		local_ip = argv[1];
	}
	if (argc > 1) {
		remote_ip = argv[2];
	}
	if (argc > 2) {
		signal_port_name = argv[3];
	}

	if (!local_ip) {
		local_ip = "127.0.0.1";
	}

	if (!remote_ip) {
		remote_ip = "127.0.0.1";
	}

	if (signal_port_name) {
		port = atoi(signal_port_name);
	}
	
	if ((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		die("Socket Error!\n");
	}
	setsockopt(servSock, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
			   
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
	
    if (bind(servSock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		die("Bind Error!\n");
		return -1;
	}

    if (listen(servSock, MAXPENDING) < 0) {
		die("Listen error\n");
		return -1;
	}
	for (;;) {
		int local_port = use_port++;
		int remote_port = use_port++;

		if (use_port++ >= PORT_MAX) {
			use_port = PORT_MIN;
		}
		
		client_sock = client_accept(servSock);

		if ((pid = fork()) < 0) {
			die("fork() failed");
		} else if (pid == 0) {

			close(servSock);   

			client_run(client_sock, local_ip, local_port, remote_ip, remote_port);
			exit(0);           
		}

#if SOCKET2ME_DEBUG
		printf("with child process: %d\n", (int) pid);
#endif
		close(client_sock);       
		process_count++;      

		while (process_count) {
			pid = waitpid((pid_t) -1, NULL, WNOHANG);  
			if (pid < 0) {
				die("waitpid() failed");
			} else if (pid == 0) {
				break;
			} else {
				process_count--;  
			}
		}
	}
}
