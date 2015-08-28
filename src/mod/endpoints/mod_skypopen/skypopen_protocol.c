/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2011, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * This module (mod_gsmopen) has been contributed by:
 *
 * Giovanni Maruzzelli <gmaruzz@gmail.com>
 *
 * Maintainer: Giovanni Maruzzelli <gmaruzz@gmail.com>
 *
 * skypopen_protocol.c -- Low Level Interface for mod_skypopen
 *
 */


#include "skypopen.h"

#ifdef ASTERISK
#define skypopen_sleep usleep
#define skypopen_strncpy strncpy
#define tech_pvt p
extern int skypopen_debug;
extern char *skypopen_console_active;
#else /* FREESWITCH */
#define skypopen_sleep switch_sleep
#define skypopen_strncpy switch_copy_string
extern switch_memory_pool_t *skypopen_module_pool;
extern switch_endpoint_interface_t *skypopen_endpoint_interface;
#endif /* ASTERISK */
int samplerate_skypopen = SAMPLERATE_SKYPOPEN;

extern int running;
extern char *interface_status[];
extern char *skype_callflow[];

/*************************************/
/* suspicious globals FIXME */
#ifdef WIN32
DWORD win32_dwThreadId;
#else

// CLOUDTREE (Thomas Hazel)
static int global_x_error = Success;
extern struct SkypopenList global_handles_list;
extern switch_status_t remove_interface(char *the_interface, switch_bool_t force);

#endif /* WIN32 */
/*************************************/
#ifndef WIN32
int skypopen_socket_create_and_bind(private_t *tech_pvt, int *which_port)
#else
int skypopen_socket_create_and_bind(private_t *tech_pvt, unsigned short *which_port)
#endif							//WIN32
{
	int s = -1;
	struct sockaddr_in my_addr;
#ifndef WIN32
	int start_port = 6001;
	unsigned int size = sizeof(int);
#else
	unsigned short start_port = 6001;
	int size = sizeof(int);
#endif //WIN32
	int sockbufsize = 0;
	int flag = 0;


	memset(&my_addr, 0, sizeof(my_addr));
	my_addr.sin_family = AF_INET;
	my_addr.sin_addr.s_addr = htonl(0x7f000001);	/* use the localhost */

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		ERRORA("socket Error\n", SKYPOPEN_P_LOG);
		return -1;
	}

	if (*which_port != 0)
		start_port = *which_port;
#ifdef WIN32
	start_port = (unsigned short) next_port();
#else
	start_port = (unsigned short) next_port();
#endif
	my_addr.sin_port = htons(start_port);
	*which_port = start_port;
	while (bind(s, (struct sockaddr *) &my_addr, sizeof(struct sockaddr)) < 0) {
		DEBUGA_SKYPE("*which_port=%d, tech_pvt->tcp_cli_port=%d, tech_pvt->tcp_srv_port=%d\n", SKYPOPEN_P_LOG, *which_port, tech_pvt->tcp_cli_port,
					 tech_pvt->tcp_srv_port);
		DEBUGA_SKYPE("bind errno=%d, error: %s\n", SKYPOPEN_P_LOG, errno, strerror(errno));
		start_port++;
		my_addr.sin_port = htons(start_port);
		*which_port = start_port;
		DEBUGA_SKYPE("*which_port=%d, tech_pvt->tcp_cli_port=%d, tech_pvt->tcp_srv_port=%d\n", SKYPOPEN_P_LOG, *which_port, tech_pvt->tcp_cli_port,
					 tech_pvt->tcp_srv_port);

		if (start_port > 65000) {
			ERRORA("NO MORE PORTS! *which_port=%d, tech_pvt->tcp_cli_port=%d, tech_pvt->tcp_srv_port=%d\n", SKYPOPEN_P_LOG, *which_port,
				   tech_pvt->tcp_cli_port, tech_pvt->tcp_srv_port);
			return -1;
		}
	}

	DEBUGA_SKYPE("Binded! *which_port=%d, tech_pvt->tcp_cli_port=%d, tech_pvt->tcp_srv_port=%d\n", SKYPOPEN_P_LOG, *which_port, tech_pvt->tcp_cli_port,
				 tech_pvt->tcp_srv_port);

	sockbufsize = 0;
	size = sizeof(int);
	getsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *) &sockbufsize, &size);
	DEBUGA_SKYPE("1 SO_RCVBUF is %d, size is %d\n", SKYPOPEN_P_LOG, sockbufsize, size);
	sockbufsize = 0;
	size = sizeof(int);
	getsockopt(s, SOL_SOCKET, SO_SNDBUF, (char *) &sockbufsize, &size);
	DEBUGA_SKYPE("1 SO_SNDBUF is %d, size is %d\n", SKYPOPEN_P_LOG, sockbufsize, size);



#ifdef WIN32
	sockbufsize = SAMPLES_PER_FRAME * 8;
#else
	sockbufsize = SAMPLES_PER_FRAME * 8;
#endif //WIN32
	size = sizeof(int);
	if (tech_pvt->setsockopt) {
		setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *) &sockbufsize, size);
	}

	sockbufsize = 0;
	size = sizeof(int);
	getsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *) &sockbufsize, &size);
	DEBUGA_SKYPE("2 SO_RCVBUF is %d, size is %d\n", SKYPOPEN_P_LOG, sockbufsize, size);

#ifdef WIN32
	sockbufsize = SAMPLES_PER_FRAME * 8;
#else
	sockbufsize = SAMPLES_PER_FRAME * 8;
#endif //WIN32
	size = sizeof(int);
	if (tech_pvt->setsockopt) {
		setsockopt(s, SOL_SOCKET, SO_SNDBUF, (char *) &sockbufsize, size);
	}

	sockbufsize = 0;
	size = sizeof(int);
	getsockopt(s, SOL_SOCKET, SO_SNDBUF, (char *) &sockbufsize, &size);
	DEBUGA_SKYPE("2 SO_SNDBUF is %d, size is %d\n", SKYPOPEN_P_LOG, sockbufsize, size);

	flag = 0;
	getsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, &size);
	DEBUGA_SKYPE("TCP_NODELAY is %d\n", SKYPOPEN_P_LOG, flag);
	flag = 1;
	if (tech_pvt->setsockopt) {
		setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, size);
	}
	flag = 0;
	getsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, &size);
	DEBUGA_SKYPE("TCP_NODELAY is %d\n", SKYPOPEN_P_LOG, flag);




	return s;
}

int skypopen_signaling_read(private_t *tech_pvt)
{
	char read_from_pipe[4096];
	char message[4096];
	char message_2[4096];
	char *buf, obj[512] = "", id[512] = "", prop[512] = "", value[512] = "", *where;
	char **stringp = NULL;
	int a;
	unsigned int howmany;
	unsigned int i;

	memset(read_from_pipe, 0, 4096);
	memset(message, 0, 4096);
	memset(message_2, 0, 4096);

	howmany = skypopen_pipe_read(tech_pvt->SkypopenHandles.fdesc[0], (short *) read_from_pipe, sizeof(read_from_pipe));

	a = 0;
	for (i = 0; i < howmany; i++) {
		message[a] = read_from_pipe[i];
		a++;

		if (read_from_pipe[i] == '\0') {
			//if (!strstr(message, "DURATION")) {
			DEBUGA_SKYPE("READING: |||%s||| \n", SKYPOPEN_P_LOG, message);
			strncpy(tech_pvt->message, message, sizeof(tech_pvt->message));
			//}
			if (!strcasecmp(message, "SILENT_MODE OFF")) {
				if (tech_pvt->silent_mode) {
					DEBUGA_SKYPE("Resetting SILENT_MODE on skype_call: %s.\n", SKYPOPEN_P_LOG, id);
					skypopen_signaling_write(tech_pvt, "SET SILENT_MODE ON");
					//switch_sleep(1000);
				}
			}
			if (!strcasecmp(message, "ERROR 68")) {
				DEBUGA_SKYPE
					("If I don't connect immediately, please give the Skype client authorization to be connected by Skypopen (and to not ask you again)\n",
					 SKYPOPEN_P_LOG);
				skypopen_sleep(1000000);
				skypopen_signaling_write(tech_pvt, "PROTOCOL 999");
				skypopen_sleep(20000);
				return 0;
			}
			if (!strncasecmp(message, "ERROR 92 CALL", 12)) {
				ERRORA("Skype got ERROR: |||%s|||, the (skypeout) number we called was not recognized as valid\n", SKYPOPEN_P_LOG, message);
				tech_pvt->skype_callflow = CALLFLOW_STATUS_FINISHED;
				DEBUGA_SKYPE("skype_call now is DOWN\n", SKYPOPEN_P_LOG);
				tech_pvt->skype_call_id[0] = '\0';

				if (tech_pvt->interface_state != SKYPOPEN_STATE_HANGUP_REQUESTED) {
					tech_pvt->interface_state = SKYPOPEN_STATE_DOWN;
					return CALLFLOW_INCOMING_HANGUP;
				} else {
					tech_pvt->interface_state = SKYPOPEN_STATE_DOWN;
				}
			}

			if (!strncasecmp(message, "ERROR", 4)) {
				if (!strncasecmp(message, "ERROR 96 CALL", 12)) {
					DEBUGA_SKYPE
						("Skype got ERROR: |||%s|||, we are trying to use this interface to make or receive a call, but another call is half-active on this interface. Let's the previous one to continue.\n",
						 SKYPOPEN_P_LOG, message);
				} else if (!strncasecmp(message, "ERROR 99 CALL", 12)) {
					DEBUGA_SKYPE("Skype got ERROR: |||%s|||, another call is active on this interface\n\n\n", SKYPOPEN_P_LOG, message);
					tech_pvt->interface_state = SKYPOPEN_STATE_ERROR_DOUBLE_CALL;
				} else if (!strncasecmp(message, "ERROR 531 VOICEMAIL", 18)) {
					NOTICA("Skype got ERROR about VOICEMAIL, no problem: |||%s|||\n", SKYPOPEN_P_LOG, message);
				} else if (!strncasecmp(message, "ERROR 529 VOICEMAIL", 18)) {
					NOTICA("Skype got ERROR about VOICEMAIL, no problem: |||%s|||\n", SKYPOPEN_P_LOG, message);
				} else if (!strncasecmp(message, "ERROR 592 ALTER CALL", 19)) {
					NOTICA("Skype got ERROR about TRANSFERRING, no problem: |||%s|||\n", SKYPOPEN_P_LOG, message);
				} else if (!strncasecmp(message, "ERROR 559 CALL", 13) | !strncasecmp(message, "ERROR 556 CALL", 13)) {
					if (tech_pvt->interface_state == SKYPOPEN_STATE_PREANSWER) {
						DEBUGA_SKYPE("Skype got ERROR about a failed action (probably TRYING to ANSWER A CALL), let's go down: |||%s|||\n", SKYPOPEN_P_LOG,
									 message);
						tech_pvt->skype_callflow = CALLFLOW_STATUS_FINISHED;
						DEBUGA_SKYPE("skype_call now is DOWN\n", SKYPOPEN_P_LOG);
						tech_pvt->skype_call_id[0] = '\0';
						tech_pvt->interface_state = SKYPOPEN_STATE_DOWN;
						return CALLFLOW_INCOMING_HANGUP;

					} else {
						DEBUGA_SKYPE("Skype got ERROR about a failed action (probably TRYING to HANGUP A CALL), no problem: |||%s|||\n", SKYPOPEN_P_LOG,
									 message);
					}
				} else if (!strncasecmp(message, "ERROR 36 Not online", 18)) {
					char msg_to_skype[256];
					ERRORA("Skype client is not online, eg: not connected to Skype network, probably got a temporary net outage: |||%s|||\n",
						   SKYPOPEN_P_LOG, message);
					if (strlen(tech_pvt->skype_call_id)) {
						sprintf(msg_to_skype, "ALTER CALL %s HANGUP", tech_pvt->skype_call_id);
						skypopen_signaling_write(tech_pvt, msg_to_skype);
					}
					if (strlen(tech_pvt->ring_id)) {
						sprintf(msg_to_skype, "ALTER CALL %s END HANGUP", tech_pvt->ring_id);
						skypopen_signaling_write(tech_pvt, msg_to_skype);
					}
					tech_pvt->interface_state = SKYPOPEN_STATE_DOWN;
					return CALLFLOW_INCOMING_HANGUP;
				} else if (!strncasecmp(message, "ERROR 589 ALTER CALL", 19)) {
					char msg_to_skype[256];
					DEBUGA_SKYPE("Skype client was not able to correctly manage tcp audio sockets, probably got a local or remote hangup: |||%s|||\n",
								 SKYPOPEN_P_LOG, message);
					if (strlen(tech_pvt->skype_call_id)) {
						sprintf(msg_to_skype, "ALTER CALL %s HANGUP", tech_pvt->skype_call_id);
						skypopen_signaling_write(tech_pvt, msg_to_skype);
					}
					if (strlen(tech_pvt->ring_id)) {
						sprintf(msg_to_skype, "ALTER CALL %s END HANGUP", tech_pvt->ring_id);
						skypopen_signaling_write(tech_pvt, msg_to_skype);
					}
					tech_pvt->interface_state = SKYPOPEN_STATE_DOWN;
					return CALLFLOW_INCOMING_HANGUP;
				} else {
					ERRORA("Skype got ERROR: |||%s|||\n", SKYPOPEN_P_LOG, message);
					tech_pvt->skype_callflow = CALLFLOW_STATUS_FINISHED;
					ERRORA("skype_call now is DOWN\n", SKYPOPEN_P_LOG);
					tech_pvt->skype_call_id[0] = '\0';

					if (tech_pvt->interface_state != SKYPOPEN_STATE_HANGUP_REQUESTED) {
						tech_pvt->interface_state = SKYPOPEN_STATE_DOWN;
						return CALLFLOW_INCOMING_HANGUP;
					} else {
						tech_pvt->interface_state = SKYPOPEN_STATE_DOWN;
					}
				}
			}

			skypopen_strncpy(message_2, message, sizeof(message) - 1);
			buf = message;
			stringp = &buf;
			where = strsep(stringp, " ");
			if (!where) {
				WARNINGA("Skype MSG without spaces: %s\n", SKYPOPEN_P_LOG, message);
			}

			if (!strcasecmp(message, "CURRENTUSERHANDLE")) {
				skypopen_strncpy(obj, where, sizeof(obj) - 1);
				where = strsep(stringp, " ");
				skypopen_strncpy(id, where, sizeof(id) - 1);
				if (!strcasecmp(id, tech_pvt->skype_user)) {
					tech_pvt->SkypopenHandles.currentuserhandle = 1;
					DEBUGA_SKYPE
						("Skype MSG: message: %s, currentuserhandle: %s, cuh: %s, skype_user: %s!\n",
						 SKYPOPEN_P_LOG, message, obj, id, tech_pvt->skype_user);
				}
			}
			if (!strcasecmp(message, "USER")) {
				skypopen_strncpy(obj, where, sizeof(obj) - 1);
				where = strsep(stringp, " ");
				skypopen_strncpy(id, where, sizeof(id) - 1);
				where = strsep(stringp, " ");
				skypopen_strncpy(prop, where, sizeof(prop) - 1);
				if (!strcasecmp(prop, "RECEIVEDAUTHREQUEST")) {
					char msg_to_skype[256];
					DEBUGA_SKYPE("Skype MSG: message: %s, obj: %s, id: %s, prop: %s!\n", SKYPOPEN_P_LOG, message, obj, id, prop);
					sprintf(msg_to_skype, "SET USER %s ISAUTHORIZED TRUE", id);
					skypopen_signaling_write(tech_pvt, msg_to_skype);
				}
			}
			if (!strcasecmp(message, "MESSAGE")) {
				skypopen_strncpy(obj, where, sizeof(obj) - 1);
				where = strsep(stringp, " ");
				skypopen_strncpy(id, where, sizeof(id) - 1);
				where = strsep(stringp, " ");
				skypopen_strncpy(prop, where, sizeof(prop) - 1);
				if (!strcasecmp(prop, "STATUS")) {
					where = strsep(stringp, " ");
					skypopen_strncpy(value, where, sizeof(value) - 1);
					if (!strcasecmp(value, "RECEIVED")) {
						char msg_to_skype[256];
						DEBUGA_SKYPE("Skype MSG: message: %s, obj: %s, id: %s, prop: %s value: %s!\n", SKYPOPEN_P_LOG, message, obj, id, prop, value);
						//TODO: authomatically flag messages as read based on config param
						sprintf(msg_to_skype, "SET MESSAGE %s SEEN", id);
						skypopen_signaling_write(tech_pvt, msg_to_skype);
					}
				} else if (!strcasecmp(prop, "BODY")) {
					char msg_to_skype[256];
					DEBUGA_SKYPE("Skype MSG: message: %s, obj: %s, id: %s, prop: %s!\n", SKYPOPEN_P_LOG, message, obj, id, prop);
					//TODO: authomatically flag messages as read based on config param
					sprintf(msg_to_skype, "SET MESSAGE %s SEEN", id);
					skypopen_signaling_write(tech_pvt, msg_to_skype);
				}
			}
			if (!strcasecmp(message, "CHAT")) {
				char msg_to_skype[256];
				int i;
				int found;

				skypopen_strncpy(obj, where, sizeof(obj) - 1);
				where = strsep(stringp, " ");
				skypopen_strncpy(id, where, sizeof(id) - 1);
				where = strsep(stringp, " ");
				skypopen_strncpy(prop, where, sizeof(prop) - 1);
				skypopen_strncpy(value, *stringp, sizeof(value) - 1);

				if (!strcasecmp(prop, "STATUS") && !strcasecmp(value, "DIALOG")) {
					DEBUGA_SKYPE("CHAT %s is DIALOG\n", SKYPOPEN_P_LOG, id);
					sprintf(msg_to_skype, "GET CHAT %s DIALOG_PARTNER", id);
					skypopen_signaling_write(tech_pvt, msg_to_skype);
				}

				if (!strcasecmp(prop, "DIALOG_PARTNER")) {
					DEBUGA_SKYPE("CHAT %s has DIALOG_PARTNER %s\n", SKYPOPEN_P_LOG, id, value);
					found = 0;
					for (i = 0; i < MAX_CHATS; i++) {
						if (strlen(tech_pvt->chats[i].chatname) == 0 || !strcmp(tech_pvt->chats[i].chatname, id)) {
							strncpy(tech_pvt->chats[i].chatname, id, sizeof(tech_pvt->chats[i].chatname));
							strncpy(tech_pvt->chats[i].dialog_partner, value, sizeof(tech_pvt->chats[i].dialog_partner));
							found = 1;
							break;
						}
					}
					if (!found) {
						ERRORA("why we do not have a chats slot free? we have more than %d chats in parallel?\n", SKYPOPEN_P_LOG, MAX_CHATS);
					}

					DEBUGA_SKYPE("CHAT %s is in position %d in the chats array, chatname=%s, dialog_partner=%s\n", SKYPOPEN_P_LOG, id, i,
								 tech_pvt->chats[i].chatname, tech_pvt->chats[i].dialog_partner);
				}

			}


			if (!strcasecmp(message, "CHATMESSAGE")) {
				char msg_to_skype[256];
				int i;
				int found;

				skypopen_strncpy(obj, where, sizeof(obj) - 1);
				where = strsep(stringp, " ");
				skypopen_strncpy(id, where, sizeof(id) - 1);
				where = strsep(stringp, " ");
				skypopen_strncpy(prop, where, sizeof(prop) - 1);
				skypopen_strncpy(value, *stringp, sizeof(value) - 1);

				if (!tech_pvt->report_incoming_chatmessages) {
					if (!strcasecmp(prop, "STATUS") && !strcasecmp(value, "RECEIVED")) {
						sprintf(msg_to_skype, "SET CHATMESSAGE %s SEEN", id);
						skypopen_signaling_write(tech_pvt, msg_to_skype);
					}
				} else {
					if (!strcasecmp(prop, "STATUS") && !strcasecmp(value, "RECEIVED")) {
						DEBUGA_SKYPE("RECEIVED CHATMESSAGE %s, let's see which type it is\n", SKYPOPEN_P_LOG, id);
						sprintf(msg_to_skype, "GET CHATMESSAGE %s TYPE", id);
						skypopen_signaling_write(tech_pvt, msg_to_skype);
					}

					if (!strcasecmp(prop, "TYPE") && !strcasecmp(value, "SAID")) {
						DEBUGA_SKYPE("CHATMESSAGE %s is of type SAID, let's get the other infos\n", SKYPOPEN_P_LOG, id);
						found = 0;
						for (i = 0; i < MAX_CHATMESSAGES; i++) {
							if (strlen(tech_pvt->chatmessages[i].id) == 0) {
								strncpy(tech_pvt->chatmessages[i].id, id, sizeof(tech_pvt->chatmessages[i].id));
								strncpy(tech_pvt->chatmessages[i].type, value, sizeof(tech_pvt->chatmessages[i].type));
								found = 1;
								break;
							}
						}
						if (!found) {
							ERRORA("why we do not have a chatmessages slot free? we have more than %d chatmessages in parallel?\n", SKYPOPEN_P_LOG,
								   MAX_CHATMESSAGES);
						} else {
							DEBUGA_SKYPE("CHATMESSAGE %s is in position %d in the chatmessages array, type=%s, id=%s\n", SKYPOPEN_P_LOG, id, i,
										 tech_pvt->chatmessages[i].type, tech_pvt->chatmessages[i].id);
							sprintf(msg_to_skype, "GET CHATMESSAGE %s CHATNAME", id);
							skypopen_signaling_write(tech_pvt, msg_to_skype);
							//skypopen_sleep(1000);
							sprintf(msg_to_skype, "GET CHATMESSAGE %s FROM_HANDLE", id);
							skypopen_signaling_write(tech_pvt, msg_to_skype);
							//skypopen_sleep(1000);
							sprintf(msg_to_skype, "GET CHATMESSAGE %s FROM_DISPNAME", id);
							skypopen_signaling_write(tech_pvt, msg_to_skype);
							//skypopen_sleep(1000);
							sprintf(msg_to_skype, "GET CHATMESSAGE %s BODY", id);
							skypopen_signaling_write(tech_pvt, msg_to_skype);
						}
					}

					if (!strcasecmp(prop, "CHATNAME")) {
						DEBUGA_SKYPE("CHATMESSAGE %s belongs to the CHAT %s\n", SKYPOPEN_P_LOG, id, value);
						found = 0;
						for (i = 0; i < MAX_CHATMESSAGES; i++) {
							if (!strcmp(tech_pvt->chatmessages[i].id, id)) {
								strncpy(tech_pvt->chatmessages[i].chatname, value, sizeof(tech_pvt->chatmessages[i].chatname));
								found = 1;
								break;
							}
						}
						if (!found) {
							DEBUGA_SKYPE("why chatmessage %s was not found in the chatmessages array??\n", SKYPOPEN_P_LOG, id);
						}
					}
					if (!strcasecmp(prop, "FROM_HANDLE")) {
						DEBUGA_SKYPE("CHATMESSAGE %s was sent by FROM_HANDLE %s\n", SKYPOPEN_P_LOG, id, value);
						found = 0;
						for (i = 0; i < MAX_CHATMESSAGES; i++) {
							if (!strcmp(tech_pvt->chatmessages[i].id, id)) {
								strncpy(tech_pvt->chatmessages[i].from_handle, value, sizeof(tech_pvt->chatmessages[i].from_handle));
								found = 1;
								break;
							}
						}
						if (!found) {
							DEBUGA_SKYPE("why chatmessage %s was not found in the chatmessages array??\n", SKYPOPEN_P_LOG, id);
						}

					}
					if (!strcasecmp(prop, "FROM_DISPNAME")) {
						DEBUGA_SKYPE("CHATMESSAGE %s was sent by FROM_DISPNAME %s\n", SKYPOPEN_P_LOG, id, value);
						found = 0;
						for (i = 0; i < MAX_CHATMESSAGES; i++) {
							if (!strcmp(tech_pvt->chatmessages[i].id, id)) {
								strncpy(tech_pvt->chatmessages[i].from_dispname, value, sizeof(tech_pvt->chatmessages[i].from_dispname));
								found = 1;
								break;
							}
						}
						if (!found) {
							DEBUGA_SKYPE("why chatmessage %s was not found in the chatmessages array??\n", SKYPOPEN_P_LOG, id);
						}

					}
					if (!strcasecmp(prop, "BODY")) {
						DEBUGA_SKYPE("CHATMESSAGE %s has BODY %s\n", SKYPOPEN_P_LOG, id, value);
						found = 0;
						for (i = 0; i < MAX_CHATMESSAGES; i++) {
							if (!strcmp(tech_pvt->chatmessages[i].id, id)) {
								strncpy(tech_pvt->chatmessages[i].body, value, sizeof(tech_pvt->chatmessages[i].body));
								found = 1;
								break;
							}
						}
						if (!found) {
							DEBUGA_SKYPE("why chatmessage %s was not found in the chatmessages array??\n", SKYPOPEN_P_LOG, id);
						} else {
							DEBUGA_SKYPE
								("CHATMESSAGE %s is in position %d in the chatmessages array, type=%s, id=%s, chatname=%s, from_handle=%s, from_dispname=%s, body=%s\n",
								 SKYPOPEN_P_LOG, id, i, tech_pvt->chatmessages[i].type, tech_pvt->chatmessages[i].id, tech_pvt->chatmessages[i].chatname,
								 tech_pvt->chatmessages[i].from_handle, tech_pvt->chatmessages[i].from_dispname, tech_pvt->chatmessages[i].body);
							if (strcmp(tech_pvt->chatmessages[i].from_handle, tech_pvt->skype_user)) {	//if the message was not sent by myself
								incoming_chatmessage(tech_pvt, i);
								memset(&tech_pvt->chatmessages[i], '\0', sizeof(tech_pvt->chatmessages[i]));

								sprintf(msg_to_skype, "SET CHATMESSAGE %s SEEN", id);
								skypopen_signaling_write(tech_pvt, msg_to_skype);
							} else {
								DEBUGA_SKYPE
									("CHATMESSAGE %s is in position %d in the chatmessages array, type=%s, id=%s, chatname=%s, from_handle=%s, from_dispname=%s, body=%s NOT DELETED\n",
									 SKYPOPEN_P_LOG, id, i, tech_pvt->chatmessages[i].type, tech_pvt->chatmessages[i].id, tech_pvt->chatmessages[i].chatname,
									 tech_pvt->chatmessages[i].from_handle, tech_pvt->chatmessages[i].from_dispname, tech_pvt->chatmessages[i].body);
								memset(&tech_pvt->chatmessages[i], '\0', sizeof(tech_pvt->chatmessages[i]));
								DEBUGA_SKYPE("chatmessage %s HAS BEEN DELETED\n", SKYPOPEN_P_LOG, id);
							}

						}

					}
				}

			}


			if (!strcasecmp(message, "VOICEMAIL")) {
				char msg_to_skype[1024];

				skypopen_strncpy(obj, where, sizeof(obj) - 1);
				where = strsep(stringp, " ");
				skypopen_strncpy(id, where, sizeof(id) - 1);
				where = strsep(stringp, " ");
				skypopen_strncpy(prop, where, sizeof(prop) - 1);
				where = strsep(stringp, " ");
				skypopen_strncpy(value, where, sizeof(value) - 1);
				where = strsep(stringp, " ");

				//DEBUGA_SKYPE
				//("Skype MSG: message: %s, obj: %s, id: %s, prop: %s, value: %s,where: %s!\n",
				//SKYPOPEN_P_LOG, message, obj, id, prop, value, where ? where : "NULL");

				if (!strcasecmp(prop, "STATUS") && !strcasecmp(value, "RECORDING") ) {
					DEBUGA_SKYPE("VOICEMAIL %s INPUT\n", SKYPOPEN_P_LOG, id);
					sprintf(msg_to_skype, "ALTER VOICEMAIL %s SET_INPUT PORT=\"%d\"", id, tech_pvt->tcp_cli_port);
					skypopen_signaling_write(tech_pvt, msg_to_skype);
				} else if (!strcasecmp(prop, "STATUS") && !strcasecmp(value, "PLAYING") ) {
					DEBUGA_SKYPE("VOICEMAIL %s OUTPUT\n", SKYPOPEN_P_LOG, id);
					sprintf(msg_to_skype, "ALTER VOICEMAIL %s SET_OUTPUT PORT=\"%d\"", id, tech_pvt->tcp_srv_port);
					skypopen_signaling_write(tech_pvt, msg_to_skype);
					sprintf(tech_pvt->skype_voicemail_id_greeting, "%s", id);

				} else if (!strcasecmp(prop, "TYPE") && !strcasecmp(value, "OUTGOING") ) {
					DEBUGA_SKYPE("VOICEMAIL OUTGOING id is %s\n", SKYPOPEN_P_LOG, id);
					sprintf(tech_pvt->skype_voicemail_id, "%s", id);
				} else if (!strcasecmp(prop, "STATUS") && !strcasecmp(value, "PLAYED") ) {
					//switch_ivr_broadcast( tech_pvt->session_uuid_str, "gentones::%(500,0,800)",SMF_ECHO_ALEG|SMF_ECHO_BLEG);
					switch_ivr_broadcast( tech_pvt->session_uuid_str, "gentones::%(500,0,800)",SMF_ECHO_BLEG);
					memset(tech_pvt->skype_voicemail_id_greeting, '\0', sizeof(tech_pvt->skype_voicemail_id_greeting));

				}
			}

			if (!strcasecmp(message, "CALL")) {
				skypopen_strncpy(obj, where, sizeof(obj) - 1);
				where = strsep(stringp, " ");
				skypopen_strncpy(id, where, sizeof(id) - 1);
				where = strsep(stringp, " ");
				skypopen_strncpy(prop, where, sizeof(prop) - 1);
				where = strsep(stringp, " ");
				skypopen_strncpy(value, where, sizeof(value) - 1);
				where = strsep(stringp, " ");

				//DEBUGA_SKYPE
				//("Skype MSG: message: %s, obj: %s, id: %s, prop: %s, value: %s,where: %s!\n",
				//SKYPOPEN_P_LOG, message, obj, id, prop, value, where ? where : "NULL");

				if (!strcasecmp(prop, "PARTNER_HANDLE")) {
					if (tech_pvt->interface_state == SKYPOPEN_STATE_IDLE) {
						/* we are NOT inside an active call */
						DEBUGA_SKYPE("Call %s go to skypopen_partner_handle_ring\n", SKYPOPEN_P_LOG, id);
						skypopen_strncpy(tech_pvt->ring_id, id, sizeof(tech_pvt->ring_id));
						skypopen_strncpy(tech_pvt->ring_value, value, sizeof(tech_pvt->ring_value));
						skypopen_strncpy(tech_pvt->answer_id, id, sizeof(tech_pvt->answer_id));
						skypopen_strncpy(tech_pvt->answer_value, value, sizeof(tech_pvt->answer_value));
						skypopen_partner_handle_ring(tech_pvt);
					} else {
						/* we are inside an active call */
						if (!strcasecmp(tech_pvt->skype_call_id, id)) {
							/* this is the call in which we are calling out */
							DEBUGA_SKYPE("Call %s DO NOTHING\n", SKYPOPEN_P_LOG, id);
						} else {
							DEBUGA_SKYPE("Call %s TRY TRANSFER\n", SKYPOPEN_P_LOG, id);
							skypopen_strncpy(tech_pvt->ring_id, id, sizeof(tech_pvt->ring_id));
							skypopen_strncpy(tech_pvt->ring_value, value, sizeof(tech_pvt->ring_value));
							skypopen_strncpy(tech_pvt->answer_id, id, sizeof(tech_pvt->answer_id));
							skypopen_strncpy(tech_pvt->answer_value, value, sizeof(tech_pvt->answer_value));
							skypopen_transfer(tech_pvt);
						}
					}
				}
				if (!strcasecmp(prop, "PARTNER_DISPNAME")) {
					snprintf(tech_pvt->callid_name, sizeof(tech_pvt->callid_name) - 1, "%s%s%s", value, where ? " " : "", where ? where : "");
				}
				if (!strcasecmp(prop, "CONF_ID") && !strcasecmp(value, "0")) {
				}
				if (!strcasecmp(prop, "CONF_ID") && strcasecmp(value, "0")) {
					DEBUGA_SKYPE("the skype_call %s is a conference call\n", SKYPOPEN_P_LOG, id);
				}
				if (!strcasecmp(prop, "DTMF")) {
					DEBUGA_SKYPE("Call %s received a DTMF: %s\n", SKYPOPEN_P_LOG, id, value);
					dtmf_received(tech_pvt, value);
				}
				if (!strcasecmp(prop, "FAILUREREASON")) {
					DEBUGA_SKYPE("Skype FAILED on skype_call %s. Let's wait for the FAILED message.\n", SKYPOPEN_P_LOG, id);
				}
#if 0
#ifndef WIN32
				if (!strcasecmp(prop, "DURATION")) {	/* each 20 seconds, we zero the buffers and sync the timers */
					if (!((atoi(value) % 20))) {
						if (tech_pvt->read_buffer) {
							switch_mutex_lock(tech_pvt->mutex_audio_srv);
							switch_buffer_zero(tech_pvt->read_buffer);
							if (tech_pvt->timer_read.timer_interface && tech_pvt->timer_read.timer_interface->timer_next) {
								switch_core_timer_sync(&tech_pvt->timer_read);
							}
							if (tech_pvt->timer_read_srv.timer_interface && tech_pvt->timer_read_srv.timer_interface->timer_next) {
								switch_core_timer_sync(&tech_pvt->timer_read_srv);
							}
							switch_mutex_unlock(tech_pvt->mutex_audio_srv);
						}

						if (tech_pvt->write_buffer) {
							switch_mutex_lock(tech_pvt->mutex_audio_cli);
							switch_buffer_zero(tech_pvt->write_buffer);
							if (tech_pvt->timer_write.timer_interface && tech_pvt->timer_write.timer_interface->timer_next) {
								switch_core_timer_sync(&tech_pvt->timer_write);
							}
							switch_mutex_unlock(tech_pvt->mutex_audio_cli);
						}
						DEBUGA_SKYPE("Synching audio on skype_call: %s.\n", SKYPOPEN_P_LOG, id);
					}
				}
#endif //WIN32
#endif //0
				if (!strcasecmp(prop, "DURATION") && (!strcasecmp(value, "1"))) {
					if (strcasecmp(id, tech_pvt->skype_call_id)) {
						skypopen_strncpy(tech_pvt->skype_call_id, id, sizeof(tech_pvt->skype_call_id) - 1);
						DEBUGA_SKYPE("We called a Skype contact and he answered us on skype_call: %s.\n", SKYPOPEN_P_LOG, id);
					}
				}

				if (!strcasecmp(prop, "DURATION") && (tech_pvt->interface_state == SKYPOPEN_STATE_ERROR_DOUBLE_CALL)) {
					char msg_to_skype[1024];
					skypopen_strncpy(tech_pvt->skype_call_id, id, sizeof(tech_pvt->skype_call_id) - 1);
					WARNINGA("We are in a double call situation, trying to get out hanging up call id: %s.\n", SKYPOPEN_P_LOG, id);
					sprintf(msg_to_skype, "ALTER CALL %s END HANGUP", id);
					skypopen_signaling_write(tech_pvt, msg_to_skype);
					sprintf(msg_to_skype, "ALTER CALL %s HANGUP", id);
					skypopen_signaling_write(tech_pvt, msg_to_skype);
					//skypopen_sleep(10000);
				}


				if (!strcasecmp(prop, "VM_DURATION") && (!strcasecmp(value, "0"))) {
					char msg_to_skype[1024];

					NOTICA("We called a Skype contact and he started Skype voicemail on our skype_call: %s.\n", SKYPOPEN_P_LOG, id);

					if (!strlen(tech_pvt->session_uuid_str)) {
						DEBUGA_SKYPE("no tech_pvt->session_uuid_str\n", SKYPOPEN_P_LOG);
					}
					if (tech_pvt->skype_callflow != CALLFLOW_STATUS_REMOTEHOLD) {
						if (!strlen(tech_pvt->session_uuid_str) || !strlen(tech_pvt->skype_call_id)
								|| !strcasecmp(tech_pvt->skype_call_id, id)) {
							skypopen_strncpy(tech_pvt->skype_call_id, id, sizeof(tech_pvt->skype_call_id) - 1);
							DEBUGA_SKYPE("skype_call: %s is now active\n", SKYPOPEN_P_LOG, id);

							if (tech_pvt->skype_callflow != CALLFLOW_STATUS_EARLYMEDIA) {
								tech_pvt->skype_callflow = CALLFLOW_STATUS_INPROGRESS;
								tech_pvt->interface_state = SKYPOPEN_STATE_UP;

								if (tech_pvt->tcp_cli_thread == NULL) {
									DEBUGA_SKYPE("START start_audio_threads\n", SKYPOPEN_P_LOG);
									if (start_audio_threads(tech_pvt)) {
										WARNINGA("start_audio_threads FAILED\n", SKYPOPEN_P_LOG);
										return CALLFLOW_INCOMING_HANGUP;
									}
								}
							}
							tech_pvt->skype_callflow = CALLFLOW_STATUS_INPROGRESS;
							if (skypopen_answered(tech_pvt) != SWITCH_STATUS_SUCCESS) {
								sprintf(msg_to_skype, "ALTER CALL %s HANGUP", id);
								skypopen_signaling_write(tech_pvt, msg_to_skype);
							}
						} else {
							DEBUGA_SKYPE("I'm on %s, skype_call %s is NOT MY call, ignoring\n", SKYPOPEN_P_LOG, tech_pvt->skype_call_id, id);
						}
					} else {
						tech_pvt->skype_callflow = CALLFLOW_STATUS_INPROGRESS;
						DEBUGA_SKYPE("Back from REMOTEHOLD!\n", SKYPOPEN_P_LOG);
					}
				}

				if (!strcasecmp(prop, "STATUS")) {

					if (!strcasecmp(value, "RINGING")) {
						char msg_to_skype[1024];
						if (tech_pvt->interface_state == SKYPOPEN_STATE_IDLE) {
							// CLOUDTREE (Thomas Hazel)
							skypopen_strncpy(tech_pvt->skype_call_id, id, sizeof(tech_pvt->skype_call_id) - 1);

							/* we are NOT inside an active call */
							DEBUGA_SKYPE("NO ACTIVE calls in this moment, skype_call %s is RINGING, to ask PARTNER_DISPNAME and PARTNER_HANDLE\n",
										 SKYPOPEN_P_LOG, id);
							sprintf(msg_to_skype, "GET CALL %s PARTNER_DISPNAME", id);
							skypopen_signaling_write(tech_pvt, msg_to_skype);
							//skypopen_sleep(100);
							sprintf(msg_to_skype, "GET CALL %s PARTNER_HANDLE", id);
							skypopen_signaling_write(tech_pvt, msg_to_skype);
							//skypopen_sleep(10000);
						} else {
							/* we are inside an active call */
							if (!strcasecmp(tech_pvt->skype_call_id, id)) {
								// CLOUDTREE (Thomas Hazel)
								tech_pvt->ringing_state = SKYPOPEN_RINGING_PRE;

								/* this is the call in which we are calling out */
								tech_pvt->skype_callflow = CALLFLOW_STATUS_RINGING;
								tech_pvt->interface_state = SKYPOPEN_STATE_RINGING;
								skypopen_strncpy(tech_pvt->skype_call_id, id, sizeof(tech_pvt->skype_call_id) - 1);
								DEBUGA_SKYPE("Our remote party in skype_call %s is RINGING\n", SKYPOPEN_P_LOG, id);
								if (remote_party_is_ringing(tech_pvt) != SWITCH_STATUS_SUCCESS) {
									DEBUGA_SKYPE
										("We are getting the RINGING from a call we probably canceled, trying to get out hanging up call id: %s.\n",
										 SKYPOPEN_P_LOG, id);
									sprintf(msg_to_skype, "ALTER CALL %s END HANGUP", id);
									skypopen_signaling_write(tech_pvt, msg_to_skype);
									sprintf(msg_to_skype, "ALTER CALL %s HANGUP", id);
									skypopen_signaling_write(tech_pvt, msg_to_skype);
									tech_pvt->skype_call_id[0] = '\0';
									// CLOUDTREE (Thomas Hazel)
									tech_pvt->ringing_state = SKYPOPEN_RINGING_INIT;
									tech_pvt->skype_callflow = CALLFLOW_CALL_IDLE;
									tech_pvt->interface_state = SKYPOPEN_STATE_DOWN;
									DEBUGA_SKYPE("we're now DOWN\n", SKYPOPEN_P_LOG);
									return CALLFLOW_INCOMING_HANGUP;

								}
							} else {
								DEBUGA_SKYPE
									("We are in another call, but skype_call %s is RINGING on us, let's ask PARTNER_HANDLE, so maybe we'll TRANSFER\n",
									 SKYPOPEN_P_LOG, id);
								sprintf(msg_to_skype, "GET CALL %s PARTNER_HANDLE", id);
								skypopen_signaling_write(tech_pvt, msg_to_skype);
								//skypopen_sleep(10000);
							}
						}
					} else if (!strcasecmp(value, "EARLYMEDIA")) {
						char msg_to_skype[1024];
						tech_pvt->skype_callflow = CALLFLOW_STATUS_EARLYMEDIA;
						tech_pvt->interface_state = SKYPOPEN_STATE_DIALING;
						DEBUGA_SKYPE("Our remote party in skype_call %s is EARLYMEDIA\n", SKYPOPEN_P_LOG, id);
						if (tech_pvt->tcp_cli_thread == NULL) {
							DEBUGA_SKYPE("START start_audio_threads\n", SKYPOPEN_P_LOG);
							if (start_audio_threads(tech_pvt)) {
								ERRORA("start_audio_threads FAILED\n", SKYPOPEN_P_LOG);
								return CALLFLOW_INCOMING_HANGUP;
							}
						}
						//skypopen_sleep(1000);
						sprintf(msg_to_skype, "ALTER CALL %s SET_INPUT PORT=\"%d\"", id, tech_pvt->tcp_cli_port);
						skypopen_signaling_write(tech_pvt, msg_to_skype);
						//skypopen_sleep(1000);
						sprintf(msg_to_skype, "#output ALTER CALL %s SET_OUTPUT PORT=\"%d\"", id, tech_pvt->tcp_srv_port);
						skypopen_signaling_write(tech_pvt, msg_to_skype);

						remote_party_is_early_media(tech_pvt);
					} else if (!strcasecmp(value, "MISSED") || !strcasecmp(value, "FINISHED")) {
						if (!strcasecmp(tech_pvt->skype_call_id, id)) {
							DEBUGA_SKYPE("skype_call %s is MY call, now I'm going DOWN\n", SKYPOPEN_P_LOG, id);
							if (tech_pvt->interface_state != SKYPOPEN_STATE_HANGUP_REQUESTED) {
								return CALLFLOW_INCOMING_HANGUP;
							} else {
								tech_pvt->interface_state = SKYPOPEN_STATE_DOWN;
							}
						} else {
							DEBUGA_SKYPE("skype_call %s is NOT MY call, ignoring\n", SKYPOPEN_P_LOG, id);
						}

					} else if (!strcasecmp(value, "CANCELLED")) {
						tech_pvt->skype_callflow = CALLFLOW_STATUS_CANCELLED;
						DEBUGA_SKYPE("we tried to call Skype on skype_call %s and Skype has now CANCELLED\n", SKYPOPEN_P_LOG, id);
						tech_pvt->skype_call_id[0] = '\0';
						if (tech_pvt->interface_state != SKYPOPEN_STATE_HANGUP_REQUESTED) {
							tech_pvt->interface_state = SKYPOPEN_STATE_DOWN;
							return CALLFLOW_INCOMING_HANGUP;
						} else {
							tech_pvt->interface_state = SKYPOPEN_STATE_DOWN;
						}
					} else if (!strcasecmp(value, "FAILED")) {
						tech_pvt->skype_callflow = CALLFLOW_STATUS_FAILED;
						DEBUGA_SKYPE("we tried to call Skype on skype_call %s and Skype has now FAILED\n", SKYPOPEN_P_LOG, id);
						tech_pvt->skype_call_id[0] = '\0';
						skypopen_strncpy(tech_pvt->skype_call_id, id, sizeof(tech_pvt->skype_call_id) - 1);
						tech_pvt->interface_state = SKYPOPEN_STATE_DOWN;
						return CALLFLOW_INCOMING_HANGUP;
					} else if (!strcasecmp(value, "REFUSED")) {
						if (!strcasecmp(id, tech_pvt->skype_call_id)) {
							/* this is the id of the call we are in, probably we generated it */
							tech_pvt->skype_callflow = CALLFLOW_STATUS_REFUSED;
							DEBUGA_SKYPE("we tried to call Skype on skype_call %s and Skype has now REFUSED\n", SKYPOPEN_P_LOG, id);
							skypopen_strncpy(tech_pvt->skype_call_id, id, sizeof(tech_pvt->skype_call_id) - 1);
							tech_pvt->interface_state = SKYPOPEN_STATE_DOWN;
							tech_pvt->skype_call_id[0] = '\0';
							return CALLFLOW_INCOMING_HANGUP;
						} else {
							/* we're here because were us that refused an incoming call */
							DEBUGA_SKYPE("we REFUSED skype_call %s\n", SKYPOPEN_P_LOG, id);
						}
					} else if (!strcasecmp(value, "TRANSFERRING")) {
						DEBUGA_SKYPE("skype_call %s is transferring\n", SKYPOPEN_P_LOG, id);
					} else if (!strcasecmp(value, "TRANSFERRED")) {
						DEBUGA_SKYPE("skype_call %s has been transferred\n", SKYPOPEN_P_LOG, id);
					} else if (!strcasecmp(value, "ROUTING")) {
						tech_pvt->skype_callflow = CALLFLOW_STATUS_ROUTING;
						tech_pvt->interface_state = SKYPOPEN_STATE_DIALING;
						skypopen_strncpy(tech_pvt->skype_call_id, id, sizeof(tech_pvt->skype_call_id) - 1);
						DEBUGA_SKYPE("skype_call: %s is now ROUTING\n", SKYPOPEN_P_LOG, id);
					} else if (!strcasecmp(value, "UNPLACED")) {
						tech_pvt->skype_callflow = CALLFLOW_STATUS_UNPLACED;
						tech_pvt->interface_state = SKYPOPEN_STATE_DIALING;
						skypopen_strncpy(tech_pvt->skype_call_id, id, sizeof(tech_pvt->skype_call_id) - 1);
						DEBUGA_SKYPE("skype_call: %s is now UNPLACED\n", SKYPOPEN_P_LOG, id);
					} else if (!strcasecmp(value, "INPROGRESS")) {
						char msg_to_skype[1024];

						if (!strlen(tech_pvt->session_uuid_str)) {
							DEBUGA_SKYPE("no tech_pvt->session_uuid_str\n", SKYPOPEN_P_LOG);
						}
						if (tech_pvt->skype_callflow != CALLFLOW_STATUS_REMOTEHOLD) {
							if (!strlen(tech_pvt->session_uuid_str) || !strlen(tech_pvt->skype_call_id)
								|| !strcasecmp(tech_pvt->skype_call_id, id)) {
								skypopen_strncpy(tech_pvt->skype_call_id, id, sizeof(tech_pvt->skype_call_id) - 1);
								DEBUGA_SKYPE("skype_call: %s is now active\n", SKYPOPEN_P_LOG, id);

								if (tech_pvt->skype_callflow != CALLFLOW_STATUS_EARLYMEDIA) {
									tech_pvt->skype_callflow = CALLFLOW_STATUS_INPROGRESS;
									tech_pvt->interface_state = SKYPOPEN_STATE_UP;

									if (tech_pvt->tcp_cli_thread == NULL) {
										DEBUGA_SKYPE("START start_audio_threads\n", SKYPOPEN_P_LOG);
										if (start_audio_threads(tech_pvt)) {
											WARNINGA("start_audio_threads FAILED\n", SKYPOPEN_P_LOG);
											return CALLFLOW_INCOMING_HANGUP;
										}
									}
									//skypopen_sleep(1000);
									sprintf(msg_to_skype, "ALTER CALL %s SET_INPUT PORT=\"%d\"", id, tech_pvt->tcp_cli_port);
									skypopen_signaling_write(tech_pvt, msg_to_skype);
									//skypopen_sleep(1000);
									sprintf(msg_to_skype, "#output ALTER CALL %s SET_OUTPUT PORT=\"%d\"", id, tech_pvt->tcp_srv_port);
									skypopen_signaling_write(tech_pvt, msg_to_skype);
								}
								tech_pvt->skype_callflow = CALLFLOW_STATUS_INPROGRESS;
								if (skypopen_answered(tech_pvt) != SWITCH_STATUS_SUCCESS) {
									sprintf(msg_to_skype, "ALTER CALL %s HANGUP", id);
									skypopen_signaling_write(tech_pvt, msg_to_skype);
								}
							} else {
								DEBUGA_SKYPE("I'm on %s, skype_call %s is NOT MY call, ignoring\n", SKYPOPEN_P_LOG, tech_pvt->skype_call_id, id);
							}
						} else {
							tech_pvt->skype_callflow = CALLFLOW_STATUS_INPROGRESS;
							DEBUGA_SKYPE("Back from REMOTEHOLD!\n", SKYPOPEN_P_LOG);
						}

					} else if (!strcasecmp(value, "LOCALHOLD")) {
						char msg_to_skype[256];
						DEBUGA_SKYPE("skype_call: %s is now LOCALHOLD, let's hangup\n", SKYPOPEN_P_LOG, id);
						sprintf(msg_to_skype, "ALTER CALL %s HANGUP", id);
						skypopen_signaling_write(tech_pvt, msg_to_skype);
					} else if (!strcasecmp(value, "REMOTEHOLD")) {
						tech_pvt->skype_callflow = CALLFLOW_STATUS_REMOTEHOLD;
						DEBUGA_SKYPE("skype_call: %s is now REMOTEHOLD\n", SKYPOPEN_P_LOG, id);

					} else if (!strcasecmp(value, "BUSY")) {
						tech_pvt->skype_callflow = CALLFLOW_STATUS_FAILED;
						DEBUGA_SKYPE
							("we tried to call Skype on skype_call %s and remote party (destination) was BUSY. Our outbound call has failed\n",
							 SKYPOPEN_P_LOG, id);
						skypopen_strncpy(tech_pvt->skype_call_id, id, sizeof(tech_pvt->skype_call_id) - 1);
						tech_pvt->interface_state = SKYPOPEN_STATE_DOWN;
						tech_pvt->skype_call_id[0] = '\0';
						//skypopen_sleep(1000);
						return CALLFLOW_INCOMING_HANGUP;
					} else if (!strcasecmp(value, "WAITING_REDIAL_COMMAND")) {
						tech_pvt->skype_callflow = CALLFLOW_STATUS_FAILED;
						DEBUGA_SKYPE
							("we tried to call Skype on skype_call %s and remote party (destination) has rejected us (WAITING_REDIAL_COMMAND). Our outbound call has failed\n",
							 SKYPOPEN_P_LOG, id);
						skypopen_strncpy(tech_pvt->skype_call_id, id, sizeof(tech_pvt->skype_call_id) - 1);
						tech_pvt->interface_state = SKYPOPEN_STATE_DOWN;
						tech_pvt->skype_call_id[0] = '\0';
						//skypopen_sleep(1000);
						return CALLFLOW_INCOMING_HANGUP;
					} else if (!strncmp(value, "VM_", 2)) {
						DEBUGA_SKYPE ("Our skype_call %s is in Skype voicemail: %s\n", SKYPOPEN_P_LOG, id, value);
					} else {
						WARNINGA("skype_call: %s, STATUS: %s is not recognized\n", SKYPOPEN_P_LOG, id, value);
					}
				}				//STATUS
			}					//CALL
			/* the "numbered" messages that follows are used by the directory application, not yet ported */
			if (!strcasecmp(message, "#333")) {
				memset(tech_pvt->skype_friends, 0, 4096);
				skypopen_strncpy(tech_pvt->skype_friends, &message_2[11], 4095);
			}
			if (!strcasecmp(message, "#222")) {
				memset(tech_pvt->skype_fullname, 0, 512);
				skypopen_strncpy(tech_pvt->skype_fullname, &message_2[10], 511);
			}
			if (!strcasecmp(message, "#765")) {
				memset(tech_pvt->skype_displayname, 0, 512);
				skypopen_strncpy(tech_pvt->skype_displayname, &message_2[10], 511);
			}
			a = 0;
		}						//message end
	}							//read_from_pipe
	return 0;
}

void *skypopen_do_tcp_srv_thread_func(void *obj)
{
	private_t *tech_pvt = obj;
	int s;
	unsigned int len;
#if defined(WIN32) && !defined(__CYGWIN__)
	int sin_size;
	int size = sizeof(int);
#else /* WIN32 */
	unsigned int sin_size;
	unsigned int size = sizeof(int);
#endif /* WIN32 */
	unsigned int fd;
	short srv_in[SAMPLES_PER_FRAME * 10];
	struct sockaddr_in remote_addr;
	int sockbufsize = 0;

	s = skypopen_socket_create_and_bind(tech_pvt, &tech_pvt->tcp_srv_port);
	if (s < 0) {
		ERRORA("skypopen_socket_create_and_bind error!\n", SKYPOPEN_P_LOG);
		return NULL;
	}
	DEBUGA_SKYPE("started tcp_srv_thread thread.\n", SKYPOPEN_P_LOG);

	listen(s, 6);

	sin_size = sizeof(remote_addr);

	while (tech_pvt && tech_pvt->interface_state != SKYPOPEN_STATE_DOWN
		   && (tech_pvt->skype_callflow == CALLFLOW_STATUS_INPROGRESS
			   || tech_pvt->skype_callflow == CALLFLOW_STATUS_EARLYMEDIA
			   || tech_pvt->skype_callflow == CALLFLOW_STATUS_REMOTEHOLD || tech_pvt->skype_callflow == SKYPOPEN_STATE_UP)) {

		unsigned int fdselectgio;
		int rtgio;
		fd_set fsgio;
		struct timeval togio;

		if (!(running && tech_pvt->running))
			break;
		FD_ZERO(&fsgio);
		togio.tv_usec = MS_SKYPOPEN * 1000;
		togio.tv_sec = 0;
		fdselectgio = s;
		FD_SET(fdselectgio, &fsgio);

		rtgio = select(fdselectgio + 1, &fsgio, NULL, NULL, &togio);

		if (rtgio) {

			while (s > 0 && (fd = accept(s, (struct sockaddr *) &remote_addr, &sin_size)) > 0) {
				DEBUGA_SKYPE("ACCEPTED here I send you %d\n", SKYPOPEN_P_LOG, tech_pvt->tcp_srv_port);

				sockbufsize = 0;
				size = sizeof(int);
				getsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *) &sockbufsize, &size);
				DEBUGA_SKYPE("3 SO_RCVBUF is %d, size is %d\n", SKYPOPEN_P_LOG, sockbufsize, size);
				sockbufsize = 0;
				size = sizeof(int);
				getsockopt(s, SOL_SOCKET, SO_SNDBUF, (char *) &sockbufsize, &size);
				DEBUGA_SKYPE("3 SO_SNDBUF is %d, size is %d\n", SKYPOPEN_P_LOG, sockbufsize, size);


				if (!(running && tech_pvt->running))
					break;
				while (tech_pvt && tech_pvt->interface_state != SKYPOPEN_STATE_DOWN
					   && tech_pvt->interface_state != SKYPOPEN_STATE_IDLE
					   && tech_pvt->interface_state != SKYPOPEN_STATE_HANGUP_REQUESTED
					   && (tech_pvt->skype_callflow == CALLFLOW_STATUS_INPROGRESS
						   || tech_pvt->skype_callflow == CALLFLOW_STATUS_EARLYMEDIA
						   || tech_pvt->skype_callflow == CALLFLOW_STATUS_REMOTEHOLD || tech_pvt->skype_callflow == SKYPOPEN_STATE_UP)) {

					unsigned int fdselect;
					int rt = 1;
					fd_set fs;
					struct timeval to;
					int nospace;

					if (!(running && tech_pvt->running))
						break;
#if 1
					fdselect = fd;
					FD_ZERO(&fs);
					FD_SET(fdselect, &fs);
					to.tv_usec = MS_SKYPOPEN * 1000 * 3;
					to.tv_sec = 0;
#endif //0

					if (tech_pvt->timer_read_srv.timer_interface && tech_pvt->timer_read_srv.timer_interface->timer_next) {
						switch_core_timer_next(&tech_pvt->timer_read_srv);
					} else {
						skypopen_sleep(20000);

					}
					rt = select(fdselect + 1, &fs, NULL, NULL, &to);
					if (rt > 0) {

						if (tech_pvt->skype_callflow != CALLFLOW_STATUS_REMOTEHOLD) {
							len = recv(fd, (char *) srv_in, BYTES_PER_FRAME * 2, 0);
						} else {
							//skypopen_sleep(10000);
							continue;
						}
						if (tech_pvt->begin_to_read == 0) {
							DEBUGA_SKYPE("len=%d\n", SKYPOPEN_P_LOG, len);
							//skypopen_sleep(10000);
							continue;
						}

						if (len == -1) {
							DEBUGA_SKYPE("len=%d, error: %s\n", SKYPOPEN_P_LOG, len, strerror(errno));
							break;
						}
						nospace = 0;
						if (len > 0) {
							switch_mutex_lock(tech_pvt->mutex_audio_srv);
							if (tech_pvt->read_buffer) {
								if (switch_buffer_freespace(tech_pvt->read_buffer) < len) {
									switch_buffer_zero(tech_pvt->read_buffer);
									switch_buffer_write(tech_pvt->read_buffer, srv_in, len);
									nospace = 1;
								} else {
									switch_buffer_write(tech_pvt->read_buffer, srv_in, len);
								}
							}
							switch_mutex_unlock(tech_pvt->mutex_audio_srv);
							if (nospace) {
								DEBUGA_SKYPE("NO SPACE in READ BUFFER: there was no space for: %d\n", SKYPOPEN_P_LOG, len);
							}
						} else if (len == 0) {
							DEBUGA_SKYPE("CLOSED\n", SKYPOPEN_P_LOG);
							break;
						} else {
							DEBUGA_SKYPE("len=%d\n", SKYPOPEN_P_LOG, len);
						}

					} else if (rt == 0) {
						continue;
					} else {
						DEBUGA_SKYPE("SRV rt=%d\n", SKYPOPEN_P_LOG, rt);
						break;
					}

				}

				DEBUGA_SKYPE("Skype incoming audio GONE\n", SKYPOPEN_P_LOG);
				tech_pvt->skype_callflow = CALLFLOW_INCOMING_HANGUP;
				skypopen_close_socket(fd);
				break;
			}
			break;
		}
	}

	DEBUGA_SKYPE("incoming audio (read) server (I am it) EXITING\n", SKYPOPEN_P_LOG);
	skypopen_close_socket(s);
	s = -1;
	//DEBUGA_SKYPE("debugging_hangup PRE srv lock\n", SKYPOPEN_P_LOG);
	switch_mutex_lock(tech_pvt->mutex_thread_audio_srv);
	//DEBUGA_SKYPE("debugging_hangup srv lock\n", SKYPOPEN_P_LOG);
	tech_pvt->tcp_srv_thread = NULL;
	switch_mutex_unlock(tech_pvt->mutex_thread_audio_srv);
	//DEBUGA_SKYPE("debugging_hangup srv unlock\n", SKYPOPEN_P_LOG);
	return NULL;
}

void *skypopen_do_tcp_cli_thread_func(void *obj)
{
	private_t *tech_pvt = obj;
	int s;
	struct sockaddr_in remote_addr;
	unsigned int len;
	unsigned int fd;
	short cli_out[SAMPLES_PER_FRAME * 2 * 10];
#ifdef WIN32
	int sin_size;
	int size = sizeof(int);
#else
	unsigned int sin_size;
	unsigned int size = sizeof(int);
#endif /* WIN32 */
	int sockbufsize = 0;

	s = skypopen_socket_create_and_bind(tech_pvt, &tech_pvt->tcp_cli_port);
	if (s < 0) {
		ERRORA("skypopen_socket_create_and_bind error!\n", SKYPOPEN_P_LOG);
		return NULL;
	}



	DEBUGA_SKYPE("started tcp_cli_thread thread.\n", SKYPOPEN_P_LOG);

	listen(s, 6);

	sin_size = sizeof(remote_addr);

	while (tech_pvt && tech_pvt->interface_state != SKYPOPEN_STATE_DOWN
		   && (tech_pvt->skype_callflow == CALLFLOW_STATUS_INPROGRESS
			   || tech_pvt->skype_callflow == CALLFLOW_STATUS_EARLYMEDIA
			   || tech_pvt->skype_callflow == CALLFLOW_STATUS_REMOTEHOLD || tech_pvt->skype_callflow == SKYPOPEN_STATE_UP)) {

		unsigned int fdselectgio;
		int rtgio;
		fd_set fsgio;
		struct timeval togio;

		if (!(running && tech_pvt->running))
			break;
		FD_ZERO(&fsgio);
		togio.tv_usec = MS_SKYPOPEN * 1000 * 3;
		togio.tv_sec = 0;
		fdselectgio = s;
		FD_SET(fdselectgio, &fsgio);

		rtgio = select(fdselectgio + 1, &fsgio, NULL, NULL, &togio);

		if (rtgio) {

			while (s > 0 && (fd = accept(s, (struct sockaddr *) &remote_addr, &sin_size)) > 0) {
				DEBUGA_SKYPE("ACCEPTED here you send me %d\n", SKYPOPEN_P_LOG, tech_pvt->tcp_cli_port);

				sockbufsize = 0;
				size = sizeof(int);
				getsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *) &sockbufsize, &size);
				DEBUGA_SKYPE("4 SO_RCVBUF is %d, size is %d\n", SKYPOPEN_P_LOG, sockbufsize, size);
				sockbufsize = 0;
				size = sizeof(int);
				getsockopt(s, SOL_SOCKET, SO_SNDBUF, (char *) &sockbufsize, &size);
				DEBUGA_SKYPE("4 SO_SNDBUF is %d, size is %d\n", SKYPOPEN_P_LOG, sockbufsize, size);



				if (!(running && tech_pvt->running))
					break;
				while (tech_pvt && tech_pvt->interface_state != SKYPOPEN_STATE_DOWN
					   && tech_pvt->interface_state != SKYPOPEN_STATE_IDLE
					   && tech_pvt->interface_state != SKYPOPEN_STATE_HANGUP_REQUESTED
					   && (tech_pvt->skype_callflow == CALLFLOW_STATUS_INPROGRESS
						   || tech_pvt->skype_callflow == CALLFLOW_STATUS_EARLYMEDIA
						   || tech_pvt->skype_callflow == CALLFLOW_STATUS_REMOTEHOLD || tech_pvt->skype_callflow == SKYPOPEN_STATE_UP)) {
					size_t bytes_to_write;

					if (!(running && tech_pvt->running))
						break;

					if (tech_pvt->timer_write.timer_interface && tech_pvt->timer_write.timer_interface->timer_next
						&& tech_pvt->interface_state != SKYPOPEN_STATE_HANGUP_REQUESTED) {
						switch_core_timer_next(&tech_pvt->timer_write);
					} else {
						skypopen_sleep(20000);
					}

					if (tech_pvt->begin_to_write == 0) {
						memset(cli_out, 255, sizeof(cli_out));
						bytes_to_write = BYTES_PER_FRAME;
						len = send(fd, (char *) cli_out, bytes_to_write, 0);
						if (len == -1) {
							DEBUGA_SKYPE("len=%d, error: %s\n", SKYPOPEN_P_LOG, len, strerror(errno));
							break;
						}
						//skypopen_sleep(10000);
						continue;
					} else {

						bytes_to_write = 0;

						if (tech_pvt->skype_callflow == CALLFLOW_INCOMING_HANGUP) {
							break;
						}
						switch_mutex_lock(tech_pvt->mutex_audio_cli);
						if (tech_pvt->write_buffer && switch_buffer_inuse(tech_pvt->write_buffer)) {
							bytes_to_write = switch_buffer_read(tech_pvt->write_buffer, cli_out, BYTES_PER_FRAME);
						}
						switch_mutex_unlock(tech_pvt->mutex_audio_cli);

						if (!bytes_to_write) {
							if (tech_pvt->write_silence_when_idle) {
								memset(cli_out, 255, sizeof(cli_out));
								bytes_to_write = BYTES_PER_FRAME;
								//DEBUGA_SKYPE("WRITE Silence!\n", SKYPOPEN_P_LOG);
							} else {
								continue;
							}
						}
						/* send the 16khz frame to the Skype client waiting for incoming audio to be sent to the remote party */
						if (tech_pvt->skype_callflow != CALLFLOW_STATUS_REMOTEHOLD) {
							len = send(fd, (char *) cli_out, bytes_to_write, 0);
							if (len == -1) {
								DEBUGA_SKYPE("len=%d, error: %s\n", SKYPOPEN_P_LOG, len, strerror(errno));
								break;
							}
							if (len != bytes_to_write) {
								DEBUGA_SKYPE("len=%d\n", SKYPOPEN_P_LOG, len);
							}
						}
					}

				}
				DEBUGA_SKYPE("Skype outbound audio GONE\n", SKYPOPEN_P_LOG);
				tech_pvt->skype_callflow = CALLFLOW_INCOMING_HANGUP;
				skypopen_close_socket(fd);
				break;
			}
			break;
		}
	}

	DEBUGA_SKYPE("outbound audio server (I am it) EXITING\n", SKYPOPEN_P_LOG);
	skypopen_close_socket(s);
	s = -1;
	//DEBUGA_SKYPE("debugging_hangup PRE cli lock\n", SKYPOPEN_P_LOG);
	switch_mutex_lock(tech_pvt->mutex_thread_audio_cli);
	//DEBUGA_SKYPE("debugging_hangup cli lock\n", SKYPOPEN_P_LOG);
	tech_pvt->tcp_cli_thread = NULL;
	switch_mutex_unlock(tech_pvt->mutex_thread_audio_cli);
	//DEBUGA_SKYPE("debugging_hangup cli unlock\n", SKYPOPEN_P_LOG);
	return NULL;
}

int skypopen_senddigit(private_t *tech_pvt, char digit)
{
	char msg_to_skype[1024];

	DEBUGA_SKYPE("DIGIT received: %c\n", SKYPOPEN_P_LOG, digit);
	if (digit != 'a' && digit != 'A' && digit != 'b' && digit != 'B' && digit != 'c' && digit != 'C' && digit != 'd' && digit != 'D') {
		sprintf(msg_to_skype, "SET CALL %s DTMF %c", tech_pvt->skype_call_id, digit);
		skypopen_signaling_write(tech_pvt, msg_to_skype);
	} else {
		WARNINGA("Received DTMF DIGIT \"%c\", but not relayed to Skype client because Skype client accepts only 0-9*#\n", SKYPOPEN_P_LOG, digit);
	}

	return 0;
}

int skypopen_call(private_t *tech_pvt, char *rdest, int timeout)
{
	char msg_to_skype[1024];

	DEBUGA_SKYPE("Calling Skype, rdest is: %s\n", SKYPOPEN_P_LOG, rdest);

	sprintf(msg_to_skype, "CALL %s", rdest);
	if (skypopen_signaling_write(tech_pvt, msg_to_skype) < 0) {
		ERRORA("failed to communicate with Skype client, now exit\n", SKYPOPEN_P_LOG);
		return -1;
	}
	return 0;
}

/***************************/
/* PLATFORM SPECIFIC */
/***************************/
#if defined(WIN32) && !defined(__CYGWIN__)
int skypopen_pipe_read(switch_file_t *pipe, short *buf, int howmany)
{
	switch_size_t quantity;

	quantity = howmany;

	switch_file_read(pipe, buf, &quantity);

	howmany = (int)quantity;

	return howmany;
}

int skypopen_pipe_write(switch_file_t *pipe, short *buf, int howmany)
{
	switch_size_t quantity;

	quantity = howmany;

	switch_file_write(pipe, buf, &quantity);

	howmany = (int)quantity;

	return howmany;
}

int skypopen_close_socket(unsigned int fd)
{
	int res;

	res = closesocket(fd);

	return res;
}

int skypopen_audio_init(private_t *tech_pvt)
{
	switch_status_t rv;
	rv = switch_file_pipe_create(&tech_pvt->audiopipe_srv[0], &tech_pvt->audiopipe_srv[1], skypopen_module_pool);
	rv = switch_file_pipe_create(&tech_pvt->audiopipe_cli[0], &tech_pvt->audiopipe_cli[1], skypopen_module_pool);
	return 0;
}
#else /* WIN32 */
int skypopen_pipe_read(int pipe, short *buf, int howmany)
{
	howmany = read(pipe, buf, howmany);
	return howmany;
}

int skypopen_pipe_write(int pipe, short *buf, int howmany)
{
	if (buf) {
		howmany = write(pipe, buf, howmany);
		return howmany;
	} else {
		return 0;
	}
}

int skypopen_close_socket(unsigned int fd)
{
	int res;

	res = close(fd);

	return res;
}

int skypopen_audio_init(private_t *tech_pvt)
{
	if (pipe(tech_pvt->audiopipe_srv)) {
		fcntl(tech_pvt->audiopipe_srv[0], F_SETFL, O_NONBLOCK);
		fcntl(tech_pvt->audiopipe_srv[1], F_SETFL, O_NONBLOCK);
	}
	if (pipe(tech_pvt->audiopipe_cli)) {
		fcntl(tech_pvt->audiopipe_cli[0], F_SETFL, O_NONBLOCK);
		fcntl(tech_pvt->audiopipe_cli[1], F_SETFL, O_NONBLOCK);
	}

/* this pipe is the audio fd for asterisk to poll on during a call. FS do not use it */
	tech_pvt->skypopen_sound_capt_fd = tech_pvt->audiopipe_srv[0];

	return 0;
}
#endif /* WIN32 */

#ifdef WIN32

enum {
	SKYPECONTROLAPI_ATTACH_SUCCESS = 0,	/*  Client is successfully 
										   attached and API window handle can be found
										   in wParam parameter */
	SKYPECONTROLAPI_ATTACH_PENDING_AUTHORIZATION = 1,	/*  Skype has acknowledged
														   connection request and is waiting
														   for confirmation from the user. */
	/*  The client is not yet attached 
	 * and should wait for SKYPECONTROLAPI_ATTACH_SUCCESS message */
	SKYPECONTROLAPI_ATTACH_REFUSED = 2,	/*  User has explicitly
										   denied access to client */
	SKYPECONTROLAPI_ATTACH_NOT_AVAILABLE = 3,	/*  API is not available
												   at the moment.
												   For example, this happens when no user
												   is currently logged in. */
	/*  Client should wait for 
	 * SKYPECONTROLAPI_ATTACH_API_AVAILABLE 
	 * broadcast before making any further */
	/*  connection attempts. */
	SKYPECONTROLAPI_ATTACH_API_AVAILABLE = 0x8001
};

/* Visual C do not have strsep? */
char
    *strsep(char **stringp, const char *delim)
{
	char *res;

	if (!stringp || !*stringp || !**stringp)
		return (char *) 0;

	res = *stringp;
	while (**stringp && !strchr(delim, **stringp))
		++(*stringp);

	if (**stringp) {
		**stringp = '\0';
		++(*stringp);
	}

	return res;
}

int skypopen_signaling_write(private_t *tech_pvt, char *msg_to_skype)
{
	static char acInputRow[1024];
	COPYDATASTRUCT oCopyData;

	DEBUGA_SKYPE("SENDING: |||%s||||\n", SKYPOPEN_P_LOG, msg_to_skype);

	sprintf(acInputRow, "%s", msg_to_skype);
	/*  send command to skype */
	oCopyData.dwData = 0;
	oCopyData.lpData = acInputRow;
	oCopyData.cbData = strlen(acInputRow) + 1;
	if (oCopyData.cbData != 1) {
		if (SendMessage
			(tech_pvt->SkypopenHandles.win32_hGlobal_SkypeAPIWindowHandle, WM_COPYDATA,
			 (WPARAM) tech_pvt->SkypopenHandles.win32_hInit_MainWindowHandle, (LPARAM) & oCopyData) == FALSE) {
			ERRORA("Sending message failed - probably Skype crashed.\n\nPlease shutdown Skypopen, then launch Skypopen and try again.\n", SKYPOPEN_P_LOG);
			return -1;
		}
	}

	return 0;

}

LRESULT APIENTRY skypopen_present(HWND hWindow, UINT uiMessage, WPARAM uiParam, LPARAM ulParam)
{
	LRESULT lReturnCode;
	int fIssueDefProc;
	private_t *tech_pvt = NULL;

	lReturnCode = 0;
	fIssueDefProc = 0;
	tech_pvt = (private_t *)(intptr_t) GetWindowLong(hWindow, GWLP_USERDATA);

	if (!running) {
		DEBUGA_SKYPE("let's DIE!\n", SKYPOPEN_P_LOG);
		tech_pvt->SkypopenHandles.win32_hInit_MainWindowHandle = NULL;
		PostQuitMessage(0);
		return lReturnCode;
	}
	switch (uiMessage) {
	case WM_CREATE:
		tech_pvt = (private_t *) ((LPCREATESTRUCT) ulParam)->lpCreateParams;
		SetWindowLong(hWindow, GWLP_USERDATA, (LONG) (intptr_t)tech_pvt);
		DEBUGA_SKYPE("got CREATE\n", SKYPOPEN_P_LOG);
		break;
	case WM_DESTROY:
		DEBUGA_SKYPE("got DESTROY\n", SKYPOPEN_P_LOG);
		tech_pvt->SkypopenHandles.win32_hInit_MainWindowHandle = NULL;
		PostQuitMessage(0);
		break;
	case WM_COPYDATA:
		if (tech_pvt->SkypopenHandles.win32_hGlobal_SkypeAPIWindowHandle == (HWND) uiParam) {
			unsigned int howmany;
			char msg_from_skype[2048];

			PCOPYDATASTRUCT poCopyData = (PCOPYDATASTRUCT) ulParam;

			memset(msg_from_skype, '\0', sizeof(msg_from_skype));
			skypopen_strncpy(msg_from_skype, (const char *) poCopyData->lpData, sizeof(msg_from_skype) - 2);

			howmany = strlen(msg_from_skype) + 1;
			howmany = skypopen_pipe_write(tech_pvt->SkypopenHandles.fdesc[1], (short *) msg_from_skype, howmany);
			lReturnCode = 1;
		}
		break;
	default:
		if (tech_pvt && tech_pvt->SkypopenHandles.win32_uiGlobal_MsgID_SkypeControlAPIAttach) {
			if (uiMessage == tech_pvt->SkypopenHandles.win32_uiGlobal_MsgID_SkypeControlAPIAttach) {
				switch (ulParam) {
				case SKYPECONTROLAPI_ATTACH_SUCCESS:
					if (!tech_pvt->SkypopenHandles.currentuserhandle) {
						//DEBUGA_SKYPE("\n\n\tConnected to Skype API!\n", SKYPOPEN_P_LOG);
						tech_pvt->SkypopenHandles.api_connected = 1;
						tech_pvt->SkypopenHandles.win32_hGlobal_SkypeAPIWindowHandle = (HWND) uiParam;
						tech_pvt->SkypopenHandles.win32_hGlobal_SkypeAPIWindowHandle = tech_pvt->SkypopenHandles.win32_hGlobal_SkypeAPIWindowHandle;
					}
					break;
				case SKYPECONTROLAPI_ATTACH_PENDING_AUTHORIZATION:
					skypopen_sleep(20000);
					break;
				case SKYPECONTROLAPI_ATTACH_REFUSED:
					ERRORA("Skype client refused to be connected by Skypopen!\n", SKYPOPEN_P_LOG);
					break;
				case SKYPECONTROLAPI_ATTACH_NOT_AVAILABLE:
					ERRORA("Skype API not (yet?) available\n", SKYPOPEN_P_LOG);
					break;
				case SKYPECONTROLAPI_ATTACH_API_AVAILABLE:
					DEBUGA_SKYPE("Skype API available\n", SKYPOPEN_P_LOG);
					skypopen_sleep(20000);
					break;
				default:
					WARNINGA("GOT AN UNKNOWN SKYPE WINDOWS MSG\n", SKYPOPEN_P_LOG);
				}
				lReturnCode = 1;
				break;
			}
		}
		fIssueDefProc = 1;
		break;
	}
	if (fIssueDefProc)
		lReturnCode = DefWindowProc(hWindow, uiMessage, uiParam, ulParam);
	return (lReturnCode);
}

int win32_Initialize_CreateWindowClass(private_t *tech_pvt)
{
	unsigned char *paucUUIDString;
	RPC_STATUS lUUIDResult;
	int fReturnStatus;
	UUID oUUID;

	fReturnStatus = 0;
	lUUIDResult = UuidCreate(&oUUID);
	tech_pvt->SkypopenHandles.win32_hInit_ProcessHandle = (HINSTANCE) OpenProcess(PROCESS_DUP_HANDLE, FALSE, GetCurrentProcessId());
	if (tech_pvt->SkypopenHandles.win32_hInit_ProcessHandle != NULL && (lUUIDResult == RPC_S_OK || lUUIDResult == RPC_S_UUID_LOCAL_ONLY)) {
		if (UuidToString(&oUUID, &paucUUIDString) == RPC_S_OK) {
			WNDCLASS oWindowClass;

			strcpy(tech_pvt->SkypopenHandles.win32_acInit_WindowClassName, "Skype-API-Skypopen-");
			strcat(tech_pvt->SkypopenHandles.win32_acInit_WindowClassName, (char *) paucUUIDString);

			oWindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
			oWindowClass.lpfnWndProc = (WNDPROC) & skypopen_present;
			oWindowClass.cbClsExtra = 0;
			oWindowClass.cbWndExtra = 0;
			oWindowClass.hInstance = tech_pvt->SkypopenHandles.win32_hInit_ProcessHandle;
			oWindowClass.hIcon = NULL;
			oWindowClass.hCursor = NULL;
			oWindowClass.hbrBackground = NULL;
			oWindowClass.lpszMenuName = NULL;
			oWindowClass.lpszClassName = tech_pvt->SkypopenHandles.win32_acInit_WindowClassName;

			if (RegisterClass(&oWindowClass) != 0)
				fReturnStatus = 1;

			RpcStringFree(&paucUUIDString);
		}
	}
	if (fReturnStatus == 0)
		CloseHandle(tech_pvt->SkypopenHandles.win32_hInit_ProcessHandle);
	tech_pvt->SkypopenHandles.win32_hInit_ProcessHandle = NULL;
	return (fReturnStatus);
}

void win32_DeInitialize_DestroyWindowClass(private_t *tech_pvt)
{
	UnregisterClass(tech_pvt->SkypopenHandles.win32_acInit_WindowClassName, tech_pvt->SkypopenHandles.win32_hInit_ProcessHandle);
	CloseHandle(tech_pvt->SkypopenHandles.win32_hInit_ProcessHandle);
	tech_pvt->SkypopenHandles.win32_hInit_ProcessHandle = NULL;
}

int win32_Initialize_CreateMainWindow(private_t *tech_pvt)
{
	tech_pvt->SkypopenHandles.win32_hInit_MainWindowHandle =
		CreateWindowEx(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE,
					   tech_pvt->SkypopenHandles.win32_acInit_WindowClassName, "",
					   WS_BORDER | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT,
					   128, 128, NULL, 0, tech_pvt->SkypopenHandles.win32_hInit_ProcessHandle, tech_pvt);
	return (tech_pvt->SkypopenHandles.win32_hInit_MainWindowHandle != NULL ? 1 : 0);
}

void win32_DeInitialize_DestroyMainWindow(private_t *tech_pvt)
{
	if (tech_pvt->SkypopenHandles.win32_hInit_MainWindowHandle != NULL)
		DestroyWindow(tech_pvt->SkypopenHandles.win32_hInit_MainWindowHandle), tech_pvt->SkypopenHandles.win32_hInit_MainWindowHandle = NULL;
}

void *skypopen_do_skypeapi_thread_func(void *obj)
{
	private_t *tech_pvt = obj;
#if defined(WIN32) && !defined(__CYGWIN__)
	switch_status_t rv;

	switch_file_pipe_create(&tech_pvt->SkypopenHandles.fdesc[0], &tech_pvt->SkypopenHandles.fdesc[1], skypopen_module_pool);
	rv = switch_file_pipe_create(&tech_pvt->SkypopenHandles.fdesc[0], &tech_pvt->SkypopenHandles.fdesc[1], skypopen_module_pool);
#else /* WIN32 */
	if (pipe(tech_pvt->SkypopenHandles.fdesc)) {
		fcntl(tech_pvt->SkypopenHandles.fdesc[0], F_SETFL, O_NONBLOCK);
		fcntl(tech_pvt->SkypopenHandles.fdesc[1], F_SETFL, O_NONBLOCK);
	}
#endif /* WIN32 */

	tech_pvt->SkypopenHandles.win32_uiGlobal_MsgID_SkypeControlAPIAttach = RegisterWindowMessage("SkypeControlAPIAttach");
	tech_pvt->SkypopenHandles.win32_uiGlobal_MsgID_SkypeControlAPIDiscover = RegisterWindowMessage("SkypeControlAPIDiscover");

	skypopen_sleep(200000);		//0,2 sec

	if (tech_pvt->SkypopenHandles.win32_uiGlobal_MsgID_SkypeControlAPIAttach != 0
		&& tech_pvt->SkypopenHandles.win32_uiGlobal_MsgID_SkypeControlAPIDiscover != 0) {
		if (win32_Initialize_CreateWindowClass(tech_pvt)) {
			if (win32_Initialize_CreateMainWindow(tech_pvt)) {
				if (SendMessage
					(HWND_BROADCAST,
					 tech_pvt->SkypopenHandles.win32_uiGlobal_MsgID_SkypeControlAPIDiscover,
					 (WPARAM) tech_pvt->SkypopenHandles.win32_hInit_MainWindowHandle, 0) != 0) {
					tech_pvt->SkypopenHandles.win32_hInit_MainWindowHandle = tech_pvt->SkypopenHandles.win32_hInit_MainWindowHandle;
					while (running && tech_pvt->running) {
						MSG oMessage;
						if (!(running && tech_pvt->running))
							break;
						while (GetMessage(&oMessage, 0, 0, 0)) {
							TranslateMessage(&oMessage);
							DispatchMessage(&oMessage);
						}
					}
				}
				win32_DeInitialize_DestroyMainWindow(tech_pvt);
			}
			win32_DeInitialize_DestroyWindowClass(tech_pvt);
		}
	}
	tech_pvt->skypopen_api_thread = NULL;
	DEBUGA_SKYPE("EXITING\n", SKYPOPEN_P_LOG);
	return NULL;
}

#else /* NOT WIN32 */

// CLOUDTREE (Thomas Hazel)
int xio_error_handler(Display * dpy)
{
	private_t *tech_pvt = NULL;
	struct SkypopenHandles *handle;

	ERRORA("Fatal display error for %d, %s\n", SKYPOPEN_P_LOG, skypopen_list_size(&global_handles_list), dpy->display_name);

	handle = skypopen_list_remove_by_value(&global_handles_list, dpy);
	if (handle != NULL) {
#ifdef XIO_ERROR_BY_SETJMP
		siglongjmp(handle->ioerror_context, 1);
#endif
#ifdef XIO_ERROR_BY_UCONTEXT
		setcontext(&handle->ioerror_context);
#endif
	}

	ERRORA("Fatal display error for %p, %s - failed to siglongjmp\n", SKYPOPEN_P_LOG, (void *) handle, dpy->display_name);

	return 0;
}

int xio_error_handler2(Display * dpy, XErrorEvent * err)
{
	private_t *tech_pvt = NULL;
	struct SkypopenHandles *handle;
	global_x_error = err->error_code;

	ERRORA("Received error code %d from X Server\n\n", SKYPOPEN_P_LOG, global_x_error);
	ERRORA("Display error for %d, %s\n", SKYPOPEN_P_LOG, skypopen_list_size(&global_handles_list), dpy->display_name);

	handle = skypopen_list_remove_by_value(&global_handles_list, dpy);
	if (handle != NULL) {
#ifdef XIO_ERROR_BY_SETJMP
		siglongjmp(handle->ioerror_context, 1);
#endif
#ifdef XIO_ERROR_BY_UCONTEXT
		setcontext(&handle->ioerror_context);
#endif
	}

	ERRORA("Fatal display error for %p, %s - failed to siglongjmp\n", SKYPOPEN_P_LOG, (void *) handle, dpy->display_name);

	return 0;
}


int X11_errors_handler(Display * dpy, XErrorEvent * err)
{
	private_t *tech_pvt = NULL;
	(void) dpy;

	global_x_error = err->error_code;
	ERRORA("Received error code %d from X Server\n\n", SKYPOPEN_P_LOG, global_x_error);
	return 0;					/*  ignore the error */
}

int skypopen_send_message(private_t *tech_pvt, const char *message_P)
{
	struct SkypopenHandles *SkypopenHandles = &tech_pvt->SkypopenHandles;
	Window w_P = SkypopenHandles->skype_win;
	Display *disp = SkypopenHandles->disp;
	Window handle_P = SkypopenHandles->win;


	Atom atom1 = XInternAtom(disp, "SKYPECONTROLAPI_MESSAGE_BEGIN", False);
	Atom atom2 = XInternAtom(disp, "SKYPECONTROLAPI_MESSAGE", False);
	unsigned int pos = 0;
	unsigned int len = strlen(message_P);
	XEvent e;

	//skypopen_sleep(1000);
	//XFlush(disp);

	memset(&e, 0, sizeof(e));
	e.xclient.type = ClientMessage;
	e.xclient.message_type = atom1;	/*  leading message */
	e.xclient.display = disp;
	e.xclient.window = handle_P;
	e.xclient.format = 8;

	// CLOUDTREE (Thomas Hazel)
	global_x_error = Success;

	do {
		unsigned int i;
		for (i = 0; i < 20 && i + pos <= len; ++i)
			e.xclient.data.b[i] = message_P[i + pos];
		XSendEvent(disp, w_P, False, 0, &e);

		e.xclient.message_type = atom2;	/*  following messages */
		pos += i;
	} while (pos <= len);

	XFlush(disp);

	// CLOUDTREE (Thomas Hazel)
	if (global_x_error != Success) {
		ERRORA("Sending message failed with status %d\n", SKYPOPEN_P_LOG, global_x_error);
		tech_pvt->running = 0;
		return 0;
	}

	return 1;
}

int skypopen_signaling_write(private_t *tech_pvt, char *msg_to_skype)
{

	DEBUGA_SKYPE("SENDING: |||%s||||\n", SKYPOPEN_P_LOG, msg_to_skype);


	if (!skypopen_send_message(tech_pvt, msg_to_skype)) {
		ERRORA
			("Sending message failed - probably Skype crashed.\n\nPlease shutdown Skypopen, then restart Skype, then launch Skypopen and try again.\n",
			 SKYPOPEN_P_LOG);
		return -1;
	}

	return 0;

}

int skypopen_present(struct SkypopenHandles *SkypopenHandles)
{
	Atom skype_inst = XInternAtom(SkypopenHandles->disp, "_SKYPE_INSTANCE", True);

	Atom type_ret;
	int format_ret;
	unsigned long nitems_ret;
	unsigned long bytes_after_ret;
	unsigned char *prop;
	int status;
	private_t *tech_pvt = NULL;

	status =
		XGetWindowProperty(SkypopenHandles->disp, DefaultRootWindow(SkypopenHandles->disp),
						   skype_inst, 0, 1, False, XA_WINDOW, &type_ret, &format_ret, &nitems_ret, &bytes_after_ret, &prop);

	/*  sanity check */
	if (status != Success || format_ret != 32 || nitems_ret != 1) {
		SkypopenHandles->skype_win = (Window) - 1;
		DEBUGA_SKYPE("Skype instance not found\n", SKYPOPEN_P_LOG);
		running = 0;
		SkypopenHandles->api_connected = 0;
		return 0;
	}

	SkypopenHandles->skype_win = *(const unsigned long *) prop & 0xffffffff;
	DEBUGA_SKYPE("Skype instance found with id #%d\n", SKYPOPEN_P_LOG, (unsigned int) SkypopenHandles->skype_win);
	SkypopenHandles->api_connected = 1;
	return 1;
}

void skypopen_clean_disp(void *data)
{

	int *dispptr;
	int disp;
	private_t *tech_pvt = NULL;

	dispptr = data;
	disp = *dispptr;

	if (disp) {
		DEBUGA_SKYPE("to be destroyed disp %d\n", SKYPOPEN_P_LOG, disp);
		close(disp);
		DEBUGA_SKYPE("destroyed disp\n", SKYPOPEN_P_LOG);
	} else {
		DEBUGA_SKYPE("NOT destroyed disp\n", SKYPOPEN_P_LOG);
	}
	DEBUGA_SKYPE("OUT destroyed disp\n", SKYPOPEN_P_LOG);
	skypopen_sleep(20000);
}

void *skypopen_do_skypeapi_thread_func(void *obj)
{

	private_t *tech_pvt = obj;
	struct SkypopenHandles *SkypopenHandles;
	char buf[512];
	Display *disp = NULL;
	Window root = -1;
	Window win = -1;
	int xfd;
	fd_set xfds;

	if (!strlen(tech_pvt->X11_display))
		strcpy(tech_pvt->X11_display, getenv("DISPLAY"));

	if (!tech_pvt->tcp_srv_port)
		tech_pvt->tcp_srv_port = 10160;

	if (!tech_pvt->tcp_cli_port)
		tech_pvt->tcp_cli_port = 10161;

	if (pipe(tech_pvt->SkypopenHandles.fdesc)) {
		fcntl(tech_pvt->SkypopenHandles.fdesc[0], F_SETFL, O_NONBLOCK);
		fcntl(tech_pvt->SkypopenHandles.fdesc[1], F_SETFL, O_NONBLOCK);
	}
	SkypopenHandles = &tech_pvt->SkypopenHandles;
	disp = XOpenDisplay(tech_pvt->X11_display);
	if (!disp) {
		ERRORA("Cannot open X Display '%s', exiting skype thread\n", SKYPOPEN_P_LOG, tech_pvt->X11_display);
		running = 0;

		// CLOUDTREE (Thomas Hazel)
		tech_pvt->skypopen_api_thread = NULL;
		remove_interface(tech_pvt->skype_user, FALSE);
		return NULL;
	} else {
		DEBUGA_SKYPE("X Display '%s' opened\n", SKYPOPEN_P_LOG, tech_pvt->X11_display);
	}

	// CLOUDTREE (Thomas Hazel)
#ifndef WIN32
	{
		char interfacename[256];

		skypopen_list_add(&global_handles_list, SkypopenHandles);
		sprintf(interfacename, "#%s", tech_pvt->name);

#ifdef XIO_ERROR_BY_SETJMP
		if (sigsetjmp(SkypopenHandles->ioerror_context, 1) != 0) {
			switch_core_session_t *session = NULL;
			tech_pvt->interface_state = SKYPOPEN_STATE_DEAD;
			ERRORA("Fatal display error for %s - successed to jump\n", SKYPOPEN_P_LOG, tech_pvt->X11_display);

			session = switch_core_session_locate(tech_pvt->session_uuid_str);
			if (session) {
				switch_channel_t *channel = switch_core_session_get_channel(session);

				if (channel) {

					switch_mutex_lock(tech_pvt->flag_mutex);
					switch_clear_flag(tech_pvt, TFLAG_IO);
					switch_clear_flag(tech_pvt, TFLAG_VOICE);
					if (switch_test_flag(tech_pvt, TFLAG_PROGRESS)) {
						switch_clear_flag(tech_pvt, TFLAG_PROGRESS);
					}
					switch_mutex_unlock(tech_pvt->flag_mutex);


					switch_core_session_rwunlock(session);
					WARNINGA("Closing session for %s\n", SKYPOPEN_P_LOG, interfacename);
					switch_channel_hangup(channel, SWITCH_CAUSE_CRASH);
				} else {
					WARNINGA("NO CHANNEL ?\n", SKYPOPEN_P_LOG);
					switch_core_session_rwunlock(session);
				}
			}

			WARNINGA("Removing skype interface %s\n", SKYPOPEN_P_LOG, interfacename);
			remove_interface(interfacename, TRUE);
			return NULL;
		}
#endif
#ifdef XIO_ERROR_BY_UCONTEXT
		getcontext(&SkypopenHandles->ioerror_context);

		if (skypopen_list_find(&global_handles_list, SkypopenHandles) == NULL) {
			switch_core_session_t *session = NULL;
			tech_pvt->interface_state = SKYPOPEN_STATE_DEAD;
			ERRORA("Fatal display error for %s - successed to jump\n", SKYPOPEN_P_LOG, tech_pvt->X11_display);

			session = switch_core_session_locate(tech_pvt->session_uuid_str);
			if (session) {
				switch_channel_t *channel = switch_core_session_get_channel(session);


				if (channel) {
					switch_mutex_lock(tech_pvt->flag_mutex);
					switch_clear_flag(tech_pvt, TFLAG_IO);
					switch_clear_flag(tech_pvt, TFLAG_VOICE);
					if (switch_test_flag(tech_pvt, TFLAG_PROGRESS)) {
						switch_clear_flag(tech_pvt, TFLAG_PROGRESS);
					}
					switch_mutex_unlock(tech_pvt->flag_mutex);


					switch_core_session_rwunlock(session);
					WARNINGA("Closing session for %s\n", SKYPOPEN_P_LOG, interfacename);
					switch_channel_hangup(channel, SWITCH_CAUSE_CRASH);

				} else {
					WARNINGA("NO CHANNEL ?\n", SKYPOPEN_P_LOG);
				}
				//skypopen_sleep(500000);
			}

			WARNINGA("Removing skype interface %s\n", SKYPOPEN_P_LOG, interfacename);
			//tech_pvt->skypopen_api_thread = NULL;
			remove_interface(interfacename, TRUE);
			//XCloseDisplay(disp);
			return NULL;
		}
#endif
	}
#endif /* NOT WIN32 */

	xfd = XConnectionNumber(disp);
	fcntl(xfd, F_SETFD, FD_CLOEXEC);

	SkypopenHandles->disp = disp;

	if (skypopen_present(SkypopenHandles)) {
		root = DefaultRootWindow(disp);
		win = XCreateSimpleWindow(disp, root, 0, 0, 1, 1, 0, BlackPixel(disp, DefaultScreen(disp)), BlackPixel(disp, DefaultScreen(disp)));

		SkypopenHandles->win = win;

		snprintf(buf, 512, "NAME skypopen");

		if (!skypopen_send_message(tech_pvt, buf)) {
			ERRORA("Sending message failed - probably Skype crashed. Please run/restart Skype manually and launch Skypopen again\n", SKYPOPEN_P_LOG);

			// CLOUDTREE (Thomas Hazel)
#ifndef WIN32
			skypopen_list_remove_by_reference(&global_handles_list, SkypopenHandles);
#endif

			XCloseDisplay(disp);
			running = 0;
			return NULL;
		}

		snprintf(buf, 512, "PROTOCOL 999");
		if (!skypopen_send_message(tech_pvt, buf)) {
			ERRORA("Sending message failed - probably Skype crashed. Please run/restart Skype manually and launch Skypopen again\n", SKYPOPEN_P_LOG);

			// CLOUDTREE (Thomas Hazel)
#ifndef WIN32
			skypopen_list_remove_by_reference(&global_handles_list, SkypopenHandles);
#endif

			XCloseDisplay(disp);
			running = 0;
			return NULL;
		}

		{
			/* perform an events loop */
			XEvent an_event;
			char buf[21];		/*  can't be longer */
			char buffer[17000];
			char continuebuffer[17000];
			char *b;
			int i;
			int continue_is_broken = 0;
			int there_were_continues = 0;
			struct timeval tv;
			Atom atom_begin = XInternAtom(disp, "SKYPECONTROLAPI_MESSAGE_BEGIN", False);
			Atom atom_continue = XInternAtom(disp, "SKYPECONTROLAPI_MESSAGE", False);

			memset(buffer, '\0', 17000);
			memset(continuebuffer, '\0', 17000);
			b = buffer;

			while (running && tech_pvt->running) {


				FD_ZERO(&xfds);
				FD_SET(xfd, &xfds);

				tv.tv_usec = 100000;
				tv.tv_sec = 0;



				if (select(xfd + 1, &xfds, 0, 0, &tv)) {

					while (XPending(disp)) {



						XNextEvent(disp, &an_event);
						if (!(running && tech_pvt->running))
							break;
						switch (an_event.type) {
						case ClientMessage:

							if (an_event.xclient.format != 8) {
								//skypopen_sleep(1000); //0.1 msec
								break;
							}

							for (i = 0; i < 20 && an_event.xclient.data.b[i] != '\0'; ++i)
								buf[i] = an_event.xclient.data.b[i];

							buf[i] = '\0';

							if (an_event.xclient.message_type == atom_begin) {
								if (strlen(buffer)) {
									unsigned int howmany;
									howmany = strlen(b) + 1;
									howmany = write(SkypopenHandles->fdesc[1], b, howmany);
									WARNINGA
										("A begin atom while the previous message is not closed???? value of previous message (between vertical bars) is=|||%s|||, will be lost\n",
										 SKYPOPEN_P_LOG, buffer);
									memset(buffer, '\0', 17000);
								}
								if (continue_is_broken) {
									continue_is_broken = 0;
									there_were_continues = 1;
								}
							}
							if (an_event.xclient.message_type == atom_continue) {
								if (!strlen(buffer)) {
									WARNINGA
										("Got a 'continue' XAtom without a previous 'begin'. It's value (between vertical bars) is=|||%s|||, let's store it and hope next 'begin' will be the good one\n",
										 SKYPOPEN_P_LOG, buf);
									strcat(continuebuffer, buf);
									continue_is_broken = 1;
									if (!strncmp(buf, "ognised identity", 15)) {
										WARNINGA
											("Got a 'continue' XAtom without a previous 'begin'. It's value (between vertical bars) is=|||%s|||. Let's introduce a 1 second delay.\n",
											 SKYPOPEN_P_LOG, buf);
										skypopen_sleep(1000000);	//1 sec
									}
									skypopen_sleep(20000);	//20 msec
									break;
								}
							}
							if (continue_is_broken) {
								XFlush(disp);
								skypopen_sleep(20000);	//20 msec 
								WARNINGA("continue_is_broken\n", SKYPOPEN_P_LOG);
								continue;
							}
							strcat(buffer, buf);
							strcat(buffer, continuebuffer);
							memset(continuebuffer, '\0', 17000);

							if (i < 20 || there_were_continues) {	/* last fragment */
								unsigned int howmany;

								howmany = strlen(b) + 1;
								howmany = write(SkypopenHandles->fdesc[1], b, howmany);
								memset(buffer, '\0', 17000);
								//XFlush(disp);
								there_were_continues = 0;
							}
							//skypopen_sleep(1000); //0.1 msec
							break;
						default:
							//skypopen_sleep(1000); //0.1 msec
							break;
						}		//switch event.type
					}			//while XPending
					XFlush(disp);

				}				// if select
			}					//while running





		}
	} else {
		ERRORA("Skype is not running, maybe crashed. Please run/restart Skype and relaunch Skypopen\n", SKYPOPEN_P_LOG);
		running = 0;
	}

	DEBUGA_SKYPE("EXITING\n", SKYPOPEN_P_LOG);

	// CLOUDTREE (Thomas Hazel)
#ifndef WIN32
	skypopen_list_remove_by_reference(&global_handles_list, SkypopenHandles);
#endif

	tech_pvt->skypopen_api_thread = NULL;
	return NULL;

}
#endif // WIN32

int inbound_channel_answered(private_t *tech_pvt)
{
	int res = 0;
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;

	session = switch_core_session_locate(tech_pvt->session_uuid_str);
	if (session) {
		channel = switch_core_session_get_channel(session);

		if (channel) {
			switch_mutex_lock(tech_pvt->flag_mutex);
			switch_set_flag(tech_pvt, TFLAG_IO);
			switch_mutex_unlock(tech_pvt->flag_mutex);
		} else {
			ERRORA("no channel\n", SKYPOPEN_P_LOG);
		}
		switch_core_session_rwunlock(session);
	} else {
		ERRORA("no session\n", SKYPOPEN_P_LOG);

	}
	return res;
}


int skypopen_answered(private_t *tech_pvt)
{

	int res = SWITCH_STATUS_SUCCESS;
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;

	if (strlen(tech_pvt->session_uuid_str)) {
		session = switch_core_session_locate(tech_pvt->session_uuid_str);
		if (session) {
			channel = switch_core_session_get_channel(session);

			if (channel) {
				if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
					tech_pvt->interface_state = SKYPOPEN_STATE_UP;
					DEBUGA_SKYPE("Outbound Channel Answered! session_uuid_str=%s\n", SKYPOPEN_P_LOG, tech_pvt->session_uuid_str);
					outbound_channel_answered(tech_pvt);
				} else {
					DEBUGA_SKYPE("answered Inbound Channel!\n\n\n\n", SKYPOPEN_P_LOG);
					inbound_channel_answered(tech_pvt);
				}

			} else {
				ERRORA("no channel after INPROGRESS?\n", SKYPOPEN_P_LOG);
				return SWITCH_STATUS_FALSE;
			}
			switch_core_session_rwunlock(session);
		} else {
			WARNINGA("no session after INPROGRESS, let's hangup\n", SKYPOPEN_P_LOG);
			return SWITCH_STATUS_FALSE;
		}
	} else {
		WARNINGA("no tech_pvt->session_uuid_str after INPROGRESS, let's hangup\n", SKYPOPEN_P_LOG);
		return SWITCH_STATUS_FALSE;
	}
	return res;
}
