#include "bfcp_server.h"

/* Macro to check for errors after sending a BFCP message */
#ifdef WIN32
#define BFCP_SEND_CHECK_ERRORS(fds)				\
	if(error < 0) {						\
		if(arguments != NULL)				\
			bfcp_free_arguments(arguments);		\
		if(message != NULL)				\
			bfcp_free_message(message);		\
		return 0;					\
	} else if(error == 0) {					\
		if(arguments != NULL)				\
			bfcp_free_arguments(arguments);		\
		if(message != NULL)				\
			bfcp_free_message(message);		\
		close(fds);					\
		FD_CLR(fds, &allset);				\
		for(i = 0; i < BFCP_MAX_CONNECTIONS; i++) {	\
			if(client[i] == fds) {			\
				client[i] = -1;			\
				/* Free TLS Session as well */	\
				if(session[i]) {		\
					SSL_free(session[i]);	\
					session[i] = NULL;	\
				}				\
				break;				\
			}					\
		}						\
		return -1;					\
	}							\
	if(arguments != NULL)					\
		bfcp_free_arguments(arguments); 		\
	if(message != NULL)					\
		bfcp_free_message(message);
#else
#define BFCP_SEND_CHECK_ERRORS(fds)				\
	if(error < 0) {						\
		if(arguments != NULL)				\
			bfcp_free_arguments(arguments);		\
		if(message != NULL)				\
			bfcp_free_message(message);		\
		return 0;					\
	} else if(error == 0) {					\
		if(arguments != NULL)				\
			bfcp_free_arguments(arguments);		\
		if(message != NULL)				\
			bfcp_free_message(message);		\
		close(fds);					\
		for(i = 0; i < BFCP_MAX_CONNECTIONS; i++) {	\
			if(client[i] == fds) {			\
				client[i] = -1;			\
				int j=0;			\
				for(j = 0; j < fds_no; j++) {	\
					if(pollfds[j].fd == fds) {	\
						pollfds[j].fd = pollfds[fds_no-1].fd;	\
						pollfds[j].events = pollfds[fds_no-1].events;	\
						pollfds[j].revents = pollfds[fds_no-1].revents;	\
						fds_no--;	\
						break;		\
					}			\
				}				\
				/* Free TLS Session as well */	\
				if(session[i]) {		\
					SSL_free(session[i]);	\
					session[i] = NULL;	\
				}				\
				break;				\
			}					\
		}						\
		return -1;					\
	}							\
	if(arguments != NULL)					\
		bfcp_free_arguments(arguments); 		\
	if(message != NULL)					\
		bfcp_free_message(message);
#endif

/* Callbacks to notify the Application Server about BFCP events:
	- 'arguments' has all the arguments of the BFCP message;
	- 'outgoing_msg' is
		1 when the message has been SENT BY the FCS,
		0 when the message has been SENT TO the FCS.
 */
static int(*callback_func)(bfcp_arguments *arguments, int outgoing_msg);
static int(*notify_to_server_app)(bfcp_arguments *arguments, int outgoing_msg);


/* Headers for private methods */
static void *recv_thread(void *st_server);


/* Create a new Floor Control Server */
struct bfcp_server *bfcp_initialize_bfcp_server(unsigned short int Max_conf, unsigned short int port_server, int(*notify_to_server_app)(bfcp_arguments *arguments, int outgoing_msg), int transport, char *certificate, char *privatekey, char *restricted)
{
#ifdef WIN32
	WSADATA wsaData;
	WSAStartup(MAKEWORD(1, 1), &wsaData);
#endif

	bfcp struct_server;
	conference lconferences;
	struct sockaddr_in server_addr;
	int i = 0;
	int server_sock;

	if(Max_conf == 0)
		Max_conf = 1;
	if(port_server <= 1024)
		port_server = BFCP_FCS_DEFAULT_PORT;

	/* Create the new Floor Control Server */
	struct_server = (bfcp)calloc(1, sizeof(bfcp_server));
	if(struct_server == NULL)
		return NULL;

	/* Initialize and lock the mutex */
	bfcp_mutex_init(&count_mutex, NULL);
	bfcp_mutex_lock(&count_mutex);

	lconferences = (conference)calloc(Max_conf, sizeof(bfcp_conference));
	if(lconferences == NULL) {
		bfcp_mutex_unlock(&count_mutex);
		free(struct_server);
		struct_server = NULL;
		return NULL;
	}

	struct_server->list_conferences = lconferences;
	struct_server->Actual_number_conference = 0;
	struct_server->Max_number_conference = --Max_conf;

	if(!restricted)
		struct_server->restricted[0] = struct_server->restricted[1] = struct_server->restricted[2] = struct_server->restricted[3] = 0;
	else {
		if(sscanf(restricted, "%hu.%hu.%hu.%hu", &struct_server->restricted[0], &struct_server->restricted[1], &struct_server->restricted[2], &struct_server->restricted[3]) < 4)
			struct_server->restricted[0] = struct_server->restricted[1] = struct_server->restricted[2] = struct_server->restricted[3] = 0;
		if(struct_server->restricted[0] > 254)
			struct_server->restricted[0] = struct_server->restricted[1] = struct_server->restricted[2] = struct_server->restricted[3] = 0;
		if(struct_server->restricted[1] > 254)
			struct_server->restricted[0] = struct_server->restricted[1] = struct_server->restricted[2] = struct_server->restricted[3] = 0;
		if(struct_server->restricted[2] > 254)
			struct_server->restricted[0] = struct_server->restricted[1] = struct_server->restricted[2] = struct_server->restricted[3] = 0;
		if(struct_server->restricted[3] > 254)
			struct_server->restricted[0] = struct_server->restricted[1] = struct_server->restricted[2] = struct_server->restricted[3] = 0;
	}

	callback_func = notify_to_server_app;

	if (transport < 1) {
		struct_server->bfcp_transport = BFCP_OVER_TCP;
	} else if (transport == 1) {
		struct_server->bfcp_transport = BFCP_OVER_TLS;
	} else {
		struct_server->bfcp_transport = BFCP_OVER_UDP;
	}

	if(struct_server->bfcp_transport == BFCP_OVER_TLS) {
		/* Initialize TLS-related stuff */
		SSL_library_init();
		SSL_load_error_strings();

		/* The BFCP specification requires TLS */
		method = (SSL_METHOD *)TLSv1_server_method();
		if(!method)
			return NULL;
		/* Create and setup a new context that all sessions will inheritate */
		context = SSL_CTX_new(method);
		if(!context)
			return NULL;
		if(SSL_CTX_use_certificate_file(context, certificate, SSL_FILETYPE_PEM) < 1)
			return NULL;
		if(SSL_CTX_use_PrivateKey_file(context, privatekey, SSL_FILETYPE_PEM) < 1)
			return NULL;
		if(!SSL_CTX_check_private_key(context))
			return NULL;
		if(SSL_CTX_set_cipher_list(context, SSL_DEFAULT_CIPHER_LIST) < 0)
			return NULL;
	}

	/* Setup the socket-related stuff */
	if (transport <= 1) {
		server_sock = socket(AF_INET, SOCK_STREAM, 0);
	} else {
		server_sock = socket(AF_INET, SOCK_DGRAM, 0);
	}

	if(server_sock == -1)
		return NULL;

	if (transport == BFCP_OVER_UDP) {
		server_sock_udp = server_sock;
	} else {
		server_sock_tcp = server_sock;
	}

	int yes = 1;
#ifndef WIN32
	if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0)
#else
	if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(int)) < 0)
#endif
		return NULL;

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port_server);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	/* Bind the server to the chosen listening port */
	if(bind(server_sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) < 0) {
		/* Bind at the port failed... */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Bind at the port failed...\n");
		return NULL;
	}

	/* Listen */
	if (transport != BFCP_OVER_UDP) {
		if (listen(server_sock, BFCP_BACKLOG) == -1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Listen failed...\n");
			return NULL;
		}

		maxfd = server_sock;
		maxi = -1;

		for (i = 0; i < BFCP_MAX_CONNECTIONS; i++) {
			client[i] = -1;
#if 0
			pollfds[i+1].fd = -1;
			pollfds[i+1].events = 0;
#endif
		}
	}

#ifndef WIN32
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Going to poll fd=%d\n", server_sock);

	if (transport == BFCP_OVER_UDP) {
		poll_fd_udp[0].fd = server_sock;
		poll_fd_udp[0].events = POLLIN;
		close_udp_conn = 0;
	} else {
		pollfds[0].fd = server_sock;
		pollfds[0].events = POLLIN;
		fds_no = 1;
	}
#else
	if (transport != BFCP_OVER_UDP) {
		FD_ZERO(&wset);
		FD_SET(server_sock, &wset);
	}
#endif

	bfcp_mutex_unlock(&count_mutex);

	/* Create the thread that will handle incoming connections and messages */
	if (transport == BFCP_OVER_UDP) {
		if (pthread_create(&thread_udp, NULL, recv_thread, struct_server) < 0) {
			/* Couldn't start the receiving thread */
			return NULL;
		}

		pthread_detach(thread_udp);
	} else {
		if (pthread_create(&thread_tcp, NULL, recv_thread, struct_server) < 0) {
			/* Couldn't start the receiving thread */
			return NULL;
		}

		pthread_detach(thread_tcp);
	}

	return struct_server;
}

/* Destroy a currently running Floor Control Server */
int bfcp_destroy_bfcp_server(bfcp_server **serverp)
{
	if(serverp == NULL)
		return -1;

	int error = 0, i, max_conference;
	int server_sock;
	int transport;

	bfcp_server *server = *serverp;
	max_conference = server->Actual_number_conference - 1;

	bfcp_mutex_lock(&count_mutex);

	if (server->bfcp_transport == BFCP_OVER_UDP) {
		server_sock = server_sock_udp;
	} else {
		server_sock = server_sock_tcp;
	}

	transport = server->bfcp_transport;

	for(i = 0; i <= max_conference; i++) {
		/* Free all the handled information (floors, users, etc) */
		error = bfcp_remove_request_list(&(server->list_conferences[i].pending));
		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}
		error = bfcp_remove_request_list(&(server->list_conferences[i].accepted));
		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}
		error = bfcp_remove_request_list(&(server->list_conferences[i].granted));
		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}
		error = bfcp_remove_floor_list(&(server->list_conferences[i].floor));
		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}
		error = bfcp_remove_user_list(&(server->list_conferences[i].user));
		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}

		server->list_conferences[i].conferenceID = 0;
		server->list_conferences[i].chair_wait_request = 0;
		server->list_conferences[i].automatic_accepted_deny_policy = 0;
	}

	/* Free the list of active conferences */
	free(server->list_conferences);
	server->list_conferences = NULL;

	if(server->bfcp_transport == BFCP_OVER_TLS)
		/* Free the TLS-related stuff */
		SSL_CTX_free(context);

	/* Free the FCS stuff */
	if(error == 0) {
		free(server);
		server = NULL;
		*serverp = NULL;
	} else {
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	/* Destroy the mutex */
	bfcp_mutex_unlock(&count_mutex);
	bfcp_mutex_destroy(&count_mutex);

	/* Destroy the thread */
	if (transport == BFCP_OVER_UDP) {
		// pthread_cancel(thread_udp);
		close_udp_conn = 1;
	} else {
		pthread_cancel(thread_tcp);
	}

	/* Close the listening socket */
#ifndef WIN32
	shutdown(server_sock, SHUT_RDWR);
#else
	shutdown(server_sock, SD_BOTH);
#endif
	close(server_sock);

	return 0;
}

/* Create a new BFCP Conference and add it to the FCS */
int bfcp_initialize_conference_server(bfcp_server *conference_server, unsigned long int conferenceID, unsigned short int Max_Num_floors, unsigned short int Max_Number_Floor_Request, int automatic_accepted_deny_policy, unsigned long int chair_wait_request)
{
	if(conference_server == NULL)
		return -1;
	if(conference_server->list_conferences == NULL)
		return -1;
	if(conferenceID == 0)
		return -1;

	int i = 0, actual_conference;

	if(Max_Number_Floor_Request <= 0)
		Max_Number_Floor_Request = 1;
	if(Max_Num_floors <= 0)
		Max_Num_floors = 1;
	if((automatic_accepted_deny_policy != 0) && (automatic_accepted_deny_policy != 1))
		automatic_accepted_deny_policy = 0;
	if(chair_wait_request <= 0)
		chair_wait_request = 300; /* By default the FCS will wait 5 minutes for ChairActions */

	/* Initialization the conference */
	i = conference_server->Actual_number_conference;
	if(i > conference_server->Max_number_conference)
		/* The maximum allowed number of active conferences has already been reached */
		return -1;

	bfcp_mutex_lock(&count_mutex);

	actual_conference = conference_server->Actual_number_conference;
	for(i = 0; i<actual_conference; i++) {
		if(conference_server->list_conferences[i].conferenceID == conferenceID)
			break;
	}

	if(conference_server->list_conferences[i].conferenceID == conferenceID) {
		/* A conference with this conferenceID already exists */
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	/* Create a list for Pending request */
	conference_server->list_conferences[i].pending = bfcp_create_list();
	if(!conference_server->list_conferences[i].pending) {
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}
	/* Create a list for Accepted request */
	conference_server->list_conferences[i].accepted = bfcp_create_list();
	if(!conference_server->list_conferences[i].accepted) {
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}
	/* Create a list for Granted request */
	conference_server->list_conferences[i].granted = bfcp_create_list();
	if(!conference_server->list_conferences[i].granted) {
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	conference_server->list_conferences[i].conferenceID = conferenceID;

	/* Create a list for users */
	conference_server->list_conferences[i].user = bfcp_create_user_list(Max_Number_Floor_Request, Max_Num_floors);
	if(!conference_server->list_conferences[i].user) {
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	/* Create a list for floors */
	conference_server->list_conferences[i].floor = bfcp_create_floor_list(Max_Num_floors);
	if(!conference_server->list_conferences[i].floor) {
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	conference_server->list_conferences[i].chair_wait_request = chair_wait_request;
	conference_server->list_conferences[i].automatic_accepted_deny_policy = automatic_accepted_deny_policy;
	conference_server->list_conferences[i].floorRequestID = 1;

	/* Both the conference and its list of users inheritate the transport property from the FCS */
	conference_server->list_conferences[i].bfcp_transport = conference_server->bfcp_transport;
	conference_server->list_conferences[i].user->bfcp_transport = conference_server->bfcp_transport;

	conference_server->Actual_number_conference = ++i;

	bfcp_mutex_unlock(&count_mutex);

	return 0;
}

/* Destroy a currently managed BFCP Conference and remove it from the FCS */
int bfcp_destroy_conference_server(bfcp_server *conference_server, unsigned long int conferenceID)
{
	if(conferenceID <= 0)
		return -1;
	if(conference_server == NULL)
		return -1;

	int error = 0, i, actual_conference;
	conference remove_conference, last_conference;

	bfcp_mutex_lock(&count_mutex);

	actual_conference = conference_server->Actual_number_conference - 1;
	for(i = 0; i < actual_conference; i++) {
		if(conference_server->list_conferences[i].conferenceID == conferenceID)
			break;
	}

	if(conference_server->list_conferences[i].conferenceID == conferenceID) {
		remove_conference = &conference_server->list_conferences[i];
		last_conference = &conference_server->list_conferences[actual_conference];

		/* We free the list of Pending requests */
		error = bfcp_clean_request_list(remove_conference->pending);
		if(error == 0) {
			free(remove_conference->pending);
			remove_conference->pending = NULL;
		} else {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}
		/* We free the list of Accepted requests */
		error = bfcp_clean_request_list(remove_conference->accepted);
		if(error == 0) {
			free(remove_conference->accepted);
			remove_conference->accepted = NULL;
		} else {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}
		/* We free the list of Granted requests */
		error = bfcp_clean_request_list(remove_conference->granted);
		if(error == 0) {
			free(remove_conference->granted);
			remove_conference->granted = NULL;
		} else {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}
		/* We free the list of floors */
		error = bfcp_remove_floor_list(&(remove_conference->floor));
		if(error == 0) {
			free(remove_conference->floor);
			remove_conference->floor = NULL;
		}
		/* We free the list of users */
		error = bfcp_remove_user_list(&(remove_conference->user));
		if(error == 0) {
			free(remove_conference->user);
			remove_conference->user = NULL;
		}

		remove_conference->conferenceID = 0;
		remove_conference->chair_wait_request = 0;
		remove_conference->automatic_accepted_deny_policy = 0;

		if(i != actual_conference) {
			/* Swap the last element in queue and the one to delete (reorder) */
			remove_conference->pending = last_conference->pending;
			remove_conference->accepted = last_conference->accepted;
			remove_conference->granted = last_conference->granted;
			remove_conference->conferenceID = last_conference->conferenceID;
			remove_conference->user = last_conference->user;
			remove_conference->floor = last_conference->floor;
			remove_conference->chair_wait_request = last_conference->chair_wait_request;
			remove_conference->automatic_accepted_deny_policy = last_conference->automatic_accepted_deny_policy;

			/* Remove the last element of the queue, which now is the one to delete */
			last_conference->pending = NULL;
			last_conference->accepted = NULL;
			last_conference->granted = NULL;
			last_conference->user = NULL;
			last_conference->floor = NULL;
			last_conference->conferenceID = 0;
			last_conference->chair_wait_request = 0;
			last_conference->automatic_accepted_deny_policy = 0;
		}
	} else {
		/* A conference with this conferenceID does NOT exist */
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	conference_server->Actual_number_conference = actual_conference;

	bfcp_mutex_unlock(&count_mutex);

	return 0;
}

/* Change the maximum number of allowed conferences in the FCS */
int bfcp_change_number_bfcp_conferences_server(bfcp_server *server, unsigned short int Num)
{
	if(server == NULL)
		return -1;
	if(Num == 0)
		Num = 1;

	conference lconference;
	int i = 0;

	if((server->Actual_number_conference) > Num) {
		for(i = Num; i < server->Actual_number_conference; i++)
			bfcp_destroy_conference_server(server, server->list_conferences[i].conferenceID);
	}

	lconference = (conference)realloc(server->list_conferences, Num*sizeof(bfcp_conference));
	if(lconference == NULL)
		return -1;

	server->list_conferences = lconference;

	if((server->Actual_number_conference) > Num)
		server->Actual_number_conference = Num;

	server->Max_number_conference = --Num;

	return 0;
}

/* Change the maximum number of users that can be granted this floor at the same time */
int bfcp_change_number_granted_floor_server(bfcp_server *server, unsigned long int conferenceID, unsigned short int floorID, unsigned short int limit_granted_floor)
{
	if(server == NULL)
		return -1;
	if(floorID <= 0)
		return -1;
	if(limit_granted_floor <= 0)
		return -1;

	int actual_conference = 0, value = 0;
	int i = 0;

	actual_conference = server->Actual_number_conference - 1;
	for(i = 0; i < actual_conference; i++) {
		if(server->list_conferences[i].conferenceID == conferenceID)
			break;
	}

	bfcp_mutex_lock(&count_mutex);

	if(server->list_conferences[i].conferenceID == conferenceID) {
		value = bfcp_change_number_granted_floor(server->list_conferences[i].floor, floorID, limit_granted_floor);
		if(value == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}
	} else {
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	bfcp_mutex_unlock(&count_mutex);

	return 0;
}

/* Change the allowed number of per-floor requests for this list */
int bfcp_change_user_req_floors_server(bfcp_server *server, unsigned short int Max_Number_Floor_Request)
{
	if(server == NULL)
		return -1;
	if(Max_Number_Floor_Request == 0)
		Max_Number_Floor_Request = 1;

	int i = 0, value = 0;

	bfcp_mutex_lock(&count_mutex);

	for(i = 0; i < server->Actual_number_conference; i++) {
		value = bfcp_change_user_req_floors(server->list_conferences[i].user, Max_Number_Floor_Request);
		if(value == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}
	}

	bfcp_mutex_unlock(&count_mutex);

	return 0;
}

/* Change the automated policy for requests related to floors that have no chair */
int bfcp_change_chair_policy(bfcp_server *conference_server, unsigned long int conferenceID, int automatic_accepted_deny_policy, unsigned long int chair_wait_request)
{
	if(conference_server == NULL)
		return -1;
	if(conferenceID <= 0)
		return -1;
	if((automatic_accepted_deny_policy < 0) || (automatic_accepted_deny_policy > 1))
		automatic_accepted_deny_policy = 0;

	int i, actual_conference = 0;

	actual_conference = conference_server->Actual_number_conference - 1;
	for(i = 0; i < actual_conference; i++) {
		if(conference_server->list_conferences[i].conferenceID == conferenceID)
			break;
	}

	bfcp_mutex_lock(&count_mutex);

	if(conference_server->list_conferences[i].conferenceID == conferenceID) {
		if(chair_wait_request != 0)
			conference_server->list_conferences[i].chair_wait_request = chair_wait_request;
		conference_server->list_conferences[i].automatic_accepted_deny_policy = automatic_accepted_deny_policy;
	} else {
		/* A conference with this conferenceID does NOT exist */
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	bfcp_mutex_unlock(&count_mutex);

	return 0;
}

/* Add a floor to an existing conference */
int bfcp_add_floor_server(bfcp_server *server, unsigned long int conferenceID, unsigned short int floorID, unsigned short int ChairID, int limit_granted_floor)
{
	if(server == NULL)
		return -1;
	if(conferenceID <= 0)
		return -1;
	if(limit_granted_floor < 0)
		return -1;

	int actual_conference = 0, error, i;

	bfcp_mutex_lock(&count_mutex);

	actual_conference = server->Actual_number_conference - 1;
	for(i = 0; i < actual_conference; i++) {
		if(server->list_conferences[i].conferenceID == conferenceID)
			break;
	}

	if(server->list_conferences[i].conferenceID == conferenceID) {
		/* Check if the chair of the floor is a valid user */
		if((bfcp_existence_user(server->list_conferences[i].user, ChairID) == 0) || (ChairID == 0)) {
			error = bfcp_insert_floor(server->list_conferences[i].floor, floorID, ChairID);
			if(error == -1) {
				bfcp_mutex_unlock(&count_mutex);
				return -1;
			}
			error = bfcp_change_number_granted_floor(server->list_conferences[i].floor, floorID, limit_granted_floor);
			if(error == -1) {
				bfcp_mutex_unlock(&count_mutex);
				return -1;
			}
		}
	} else {
		/* A conference with this conferenceID does NOT exist */
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	bfcp_mutex_unlock(&count_mutex);

	return 0;
}

/* Remove a floor from an existing conference */
int bfcp_delete_floor_server(bfcp_server *server, unsigned long int conferenceID, unsigned short int floorID)
{
	int actual_conference=0, value=0, error, i;
	bfcp_queue *laccepted;

	if(server == NULL) return -1;
	if(conferenceID <= 0) return -1;

	bfcp_mutex_lock(&count_mutex);

	actual_conference = server->Actual_number_conference - 1;
	for(i = 0; i < actual_conference; i++) {
		if(server->list_conferences[i].conferenceID == conferenceID)
			break;
	}

	if(server->list_conferences[i].conferenceID == conferenceID) {
		/* Find the position of this floor in the floor list */
		value = bfcp_return_position_floor(server->list_conferences[i].floor, floorID);

		/* Remove the floor from the FloorRequests sublist of the user list */
		error = bfcp_delete_a_floor_from_user_list(server->list_conferences[i].user, value);
		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}

		/* Remove the floor from the Pending list */
		error = bfcp_delete_node_with_floorID(server->list_conferences[i].conferenceID, server->list_conferences[i].accepted, server->list_conferences[i].pending, floorID, server->list_conferences[i].floor, 1);
		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}
		/* Remove the floor from the Accepted list */
		error = bfcp_delete_node_with_floorID(server->list_conferences[i].conferenceID, server->list_conferences[i].accepted, server->list_conferences[i].accepted, floorID, server->list_conferences[i].floor,0);
		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}
		/* Remove the floor from the Granted list */
		error = bfcp_delete_node_with_floorID(server->list_conferences[i].conferenceID, server->list_conferences[i].accepted, server->list_conferences[i].granted, floorID, server->list_conferences[i].floor,0);
		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}

		/* Check if other requests that need this floor are in the Accepted list */
		laccepted = server->list_conferences[i].accepted;
		if(give_free_floors_to_the_accepted_nodes(server->list_conferences+i, laccepted, server->list_conferences[i].floor, NULL)== -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}

		/* Remove the floor from the floor list */
		error = bfcp_delete_floor(server->list_conferences[i].floor, floorID);
		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}
	} else {
		/* A conference with this conferenceID does NOT exist */
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	bfcp_mutex_unlock(&count_mutex);

	return 0;
}

/* Set a participant as chair of a floor */
int bfcp_add_chair_server(bfcp_server *server, unsigned long int conferenceID, unsigned short int floorID, unsigned short int ChairID)
{
	if(server == NULL)
		return -1;
	if(conferenceID <= 0)
		return -1;

	int error, i, actual_conference = 0;

	bfcp_mutex_lock(&count_mutex);

	actual_conference = server->Actual_number_conference - 1;
	for(i = 0; i < actual_conference; i++) {
		if(server->list_conferences[i].conferenceID == conferenceID)
			break;
	}

	if(server->list_conferences[i].conferenceID == conferenceID) {
		/* Check if this is a valid user of the conference */
		if(bfcp_existence_user(server->list_conferences[i].user, ChairID) == 0) {
			/* Add the ChairID to the floor list */
			error = bfcp_change_chair(server->list_conferences[i].floor, floorID, ChairID);
			if(error == -1) {
				bfcp_mutex_unlock(&count_mutex);
				return -1;
			}
		} else {
			/* A user with this userID does NOT exist */
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}
	} else {
		/* A conference with this conferenceID does NOT exist */
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	bfcp_mutex_unlock(&count_mutex);

	return 0;
}

/* Set no participant as chair for a floor */
int bfcp_delete_chair_server(bfcp_server *server, unsigned long int conferenceID, unsigned short int floorID)
{
	if(server == NULL)
		return -1;
	if(server->list_conferences == NULL)
		return -1;
	if(conferenceID <= 0)
		return -1;

	int error, i, actual_conference = 0, position_floor = 0;
	bfcp_floor *floor;
	floor_request_query *newrequest;
	pnode traverse;

	actual_conference = server->Actual_number_conference - 1;
	for(i = 0; i < actual_conference; i++) {
		if(server->list_conferences[i].conferenceID == conferenceID)
			break;
	}

	if(server->list_conferences[i].conferenceID != conferenceID)
		/* A conference with this conferenceID does NOT exist */
		return -1;

	if(server->list_conferences[i].floor != NULL) {
		position_floor = bfcp_return_position_floor(server->list_conferences[i].floor, floorID);
		if(position_floor == -1)
			return -1;
	} else
		return -1;

	/* Check if the chair of the floor is a valid user */
	if(server->list_conferences[i].floor->floors[position_floor].chairID != 0) {
		if(bfcp_existence_user(server->list_conferences[i].user, server->list_conferences[i].floor->floors[position_floor].chairID) == 0) {
			/* Check if the automated response policy is to deny requests */
			if(server->list_conferences[i].automatic_accepted_deny_policy == 1) {
				/* Notify all interested users before cancelling the requests */
				if((server->list_conferences[i].pending!=NULL) && (server->list_conferences[i].pending->head!=NULL)) {
					traverse = server->list_conferences[i].pending->head;
					while(traverse != NULL) {
						floor = traverse->floor;
						while(floor != NULL) {
							if(floor->floorID == floorID) {
								newrequest = traverse->floorrequest;
								while(newrequest != NULL) {
									error = bfcp_show_requestfloor_information(server->list_conferences[i].user, server->list_conferences[i].accepted, conferenceID, newrequest->userID, 0, traverse, BFCP_CANCELLED, newrequest->fd);
									if(error == -1) {
										bfcp_mutex_unlock(&count_mutex);
										return -1;
									}
									newrequest = newrequest->next;
								}
							}
							floor = floor->next;
						}
						traverse = traverse->next;
					}
				}
				/* Remove the floor from the Pending list */
				error = bfcp_delete_node_with_floorID(server->list_conferences[i].conferenceID, server->list_conferences[i].accepted, server->list_conferences[i].pending, floorID, server->list_conferences[i].floor,0);
				if(error == -1)
					return -1;
			}
			if(server->list_conferences[i].automatic_accepted_deny_policy == 0) {
				error = bfcp_accepted_pending_node_with_floorID(server->list_conferences[i].conferenceID, server->list_conferences[i].accepted, server->list_conferences[i].pending, floorID, server->list_conferences[i].floor, 0);
				if(error == -1)
					return -1;

				/* Notify all interested users before accepting the requests */
				if((server->list_conferences[i].accepted != NULL) && (server->list_conferences[i].accepted->head != NULL)) {
					traverse = server->list_conferences[i].accepted->head;
					while(traverse != NULL) {
						floor = traverse->floor;
						while(floor != NULL) {
							if(floor->floorID == floorID) {
								newrequest = traverse->floorrequest;

								while(newrequest != NULL) {
									error = bfcp_show_requestfloor_information(server->list_conferences[i].user, server->list_conferences[i].accepted, conferenceID, newrequest->userID, 0, traverse, BFCP_ACCEPTED, newrequest->fd);
									if(error == -1) {
										bfcp_mutex_unlock(&count_mutex);
										return -1;
									}
									newrequest = newrequest->next;
								}
							}
							floor = floor->next;
						}
						traverse = traverse->next;
					}
				}
				/* Check if the node should be in the Granted queue */
				if(give_free_floors_to_the_accepted_nodes(server->list_conferences+i, server->list_conferences[i].accepted, server->list_conferences[i].floor, NULL) == -1)
					return -1;
				else {
					/*send floor information after accepted all the floors*/
					if((server->list_conferences[i].granted != NULL) && (server->list_conferences[i].granted->head != NULL)) {
						traverse = server->list_conferences[i].granted->head;
						while(traverse != NULL) {
							floor = traverse->floor;
							while(floor != NULL) {
								if(floor->floorID == floorID) {
									newrequest = traverse->floorrequest;
									while(newrequest != NULL) {
										error = bfcp_show_requestfloor_information(server->list_conferences[i].user, server->list_conferences[i].accepted, conferenceID, newrequest->userID, 0, traverse, BFCP_GRANTED, newrequest->fd);
										if(error == -1) {
											bfcp_mutex_unlock(&count_mutex);
											return -1;
										}
										newrequest = newrequest->next;
									}
								}
								floor = floor->next;
							}
							traverse = traverse->next;
						}
					}
				}
			}
			/* Remove the ChairID from the floor list */
			error = bfcp_change_chair(server->list_conferences[i].floor, floorID, 0);
			if(error == -1)
				return -1;
		}
	}

	return 0;
}

/* Add a participant to the list of users of a BFCP Conference */
int bfcp_add_user_server(bfcp_server *server, unsigned long int conferenceID, unsigned short int userID, char *user_URI, char *user_display_name)
{
	if(server == NULL)
		return -1;
	if(conferenceID <= 0)
		return -1;

	int error, i, actual_conference = 0;

	bfcp_mutex_lock(&count_mutex);

	actual_conference = server->Actual_number_conference - 1;
	for(i = 0; i < actual_conference; i++) {
		if(server->list_conferences[i].conferenceID == conferenceID)
			break;
	}

	if(server->list_conferences[i].conferenceID == conferenceID) {
		/* Add a new user to the conference */
		error = bfcp_add_user(server->list_conferences[i].user, userID, user_URI, user_display_name);
		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}
	} else {
		/* A conference with this conferenceID does NOT exist */
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	bfcp_mutex_unlock(&count_mutex);

	return 0;
}

/* Set address of a user in bfcp_list_users of a BFCP conference */
int bfcp_set_user_address_server(bfcp_server *server,
				  unsigned long int conferenceID,
				  unsigned short int userID,
				  char *client_address,
				  unsigned short int client_port)
{
	if (server == NULL || conferenceID <= 0) {
		return -1;
	}

	int error, i, actual_conference = 0;

	bfcp_mutex_lock(&count_mutex);

	actual_conference = server->Actual_number_conference;

	for (i = 0; i < actual_conference && (server->list_conferences[i].conferenceID != conferenceID); i++);

	if (i < actual_conference) {
		/* Set address of user in the conference */
		error = bfcp_set_user_address(server->list_conferences[i].user, userID, client_address, client_port);

		if (error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}
	} else {
		/* A conference with this conferenceID does NOT exist */
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	bfcp_mutex_unlock(&count_mutex);

	return 0;
}

/* Retrieve participant address from user list of a BFCP conference */
struct sockaddr_in *bfcp_get_user_address_server(bfcp_server *server,
						  unsigned long int conferenceID,
						  unsigned short int userID)
{
	if (server == NULL || conferenceID <= 0 || userID <= 0) {
		return NULL;
	}

	int i, actual_conference = 0;
	struct sockaddr_in *client_address = NULL;

	bfcp_mutex_lock(&count_mutex);

	actual_conference = server->Actual_number_conference;

	for (i = 0; i < actual_conference && (server->list_conferences[i].conferenceID != conferenceID); i++);

	if (i < actual_conference) {
		/* Check if this is a valid user in the conference */
		if (bfcp_existence_user(server->list_conferences[i].user, userID) == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return NULL;
		}

		/* Get address of user */
		client_address = bfcp_get_user_address(server->list_conferences[i].user, userID);
	}

	bfcp_mutex_unlock(&count_mutex);

	return client_address;
}

/* Remove a participant from the list of users of a BFCP Conference */
int bfcp_delete_user_server(bfcp_server *server, unsigned long int conferenceID, unsigned short int userID)
{
	if(server == NULL)
		return -1;
	if(conferenceID <= 0)
		return -1;
	if(userID <= 0)
		return -1;

	int error, i, y, actual_conference = 0;
	bfcp_queue *laccepted;
	bfcp_list_floors *lfloors;
	pnode traverse;

	bfcp_mutex_lock(&count_mutex);

	actual_conference = server->Actual_number_conference - 1;
	for(i = 0; i < actual_conference; i++) {
		if(server->list_conferences[i].conferenceID == conferenceID)
			break;
	}

	if(server->list_conferences[i].conferenceID == conferenceID) {
		/* Check if this is a valid user in the conference */
		if(bfcp_existence_user(server->list_conferences[i].user, userID) == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}
		/* Remove the user from the FloorRequest list of every node */
		error = bfcp_remove_floorrequest_from_all_nodes(server->list_conferences+i, userID);
		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}
		/* Remove the user from the FloorQuery list */
		error = bfcp_remove_floorquery_from_all_nodes(server->list_conferences[i].floor, userID);
		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}
		/* Checks if the user is chair of any floors */
		if(bfcp_exist_user_as_a_chair(server->list_conferences[i].floor, userID) == 0) {
			/* This user is chair of at least one floor */
			lfloors = server->list_conferences[i].floor;
			for(y = 0; y < lfloors->actual_number_floors; y++) {
				if(lfloors->floors[y].chairID == userID) {
					error = bfcp_delete_chair_server(server, conferenceID, lfloors->floors[y].floorID);
					if(error == -1) {
						bfcp_mutex_unlock(&count_mutex);
						return -1;
					}
				}
			}
		}

		/* Notify all interested users before cancelling the Pending requests */
		if((server->list_conferences[i].pending != NULL) && (server->list_conferences[i].pending->head != NULL)) {
			traverse = server->list_conferences[i].pending->head;
			while(traverse != NULL) {
				if((traverse->userID == userID) || (traverse->beneficiaryID == userID)) {
					error = bfcp_print_information_floor(server->list_conferences+i, 0, 0, traverse, BFCP_CANCELLED);
					if(error == -1) {
						bfcp_mutex_unlock(&count_mutex);
						return -1;
					}
				}
				traverse = traverse->next;
			}
		}

		/* Notify all interested users before cancelling the Accepted requests */
		if((server->list_conferences[i].accepted != NULL) && (server->list_conferences[i].accepted->head != NULL)) {
			traverse = server->list_conferences[i].accepted->head;
			while(traverse != NULL) {
				if((traverse->userID == userID) || (traverse->beneficiaryID == userID)) {
					error = bfcp_print_information_floor(server->list_conferences+i, 0, 0, traverse, BFCP_CANCELLED);
					if(error == -1) {
						bfcp_mutex_unlock(&count_mutex);
						return -1;
					}
				}
				traverse = traverse->next;
			}
		}

		/* Notify all interested users before cancelling the Granted requests */
		if((server->list_conferences[i].granted != NULL) && (server->list_conferences[i].granted->head != NULL)) {
			traverse = server->list_conferences[i].granted->head;
			while(traverse != NULL) {
				if((traverse->userID == userID) || (traverse->beneficiaryID == userID)) {
					error = bfcp_print_information_floor(server->list_conferences+i, 0, 0, traverse, BFCP_CANCELLED);
					if(error == -1) {
						bfcp_mutex_unlock(&count_mutex);
						return -1;
					}
				}
				traverse = traverse->next;
			}
		}

		/* Remove the user from the Pending requests list */
		error = bfcp_delete_node_with_userID(server->list_conferences[i].pending, userID, server->list_conferences[i].floor);
		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}

		/* Remove the user from the Accepted requests list */
		error = bfcp_delete_node_with_userID(server->list_conferences[i].accepted, userID, server->list_conferences[i].floor);
		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}

		/* Remove the user from the Granted requests list */
		error = bfcp_delete_node_with_userID(server->list_conferences[i].granted, userID, server->list_conferences[i].floor);
		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}

		/* Finally remove the user from the conference */
		error = bfcp_delete_user(server->list_conferences[i].user, userID);
		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}

		/* Check if other requests that need the floors freed by the leaving user are in the Accepted list */
		laccepted = server->list_conferences[i].accepted;
		if(give_free_floors_to_the_accepted_nodes(server->list_conferences+i, laccepted, server->list_conferences[i].floor, NULL) == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}
	} else {
		/* A conference with this conferenceID does NOT exist */
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	bfcp_mutex_unlock(&count_mutex);

	return 0;
}

/* Create a new 'bfcp_floor_request_information' (bfcp_messages.h) out of a 'pnode' */
bfcp_floor_request_information *create_floor_request_userID(pnode traverse,lusers users, unsigned short int userID, unsigned short int request_status, int i)
{
	if(traverse == NULL)
		return NULL;
	if(users == NULL)
		return NULL;

	bfcp_user_information *user_info;
	bfcp_user_information *beneficiary_info;
	bfcp_floor *node;
	bfcp_floor_request_information *frqInfo = NULL;
	bfcp_overall_request_status *oRS = NULL;
	bfcp_floor_request_status *fRS_temp = NULL, *fRS = NULL;

	/* If there's a beneficiary, add its information */
	if(traverse->beneficiaryID != 0) {
		beneficiary_info = bfcp_show_user_information(users, traverse->beneficiaryID);
		if(beneficiary_info == NULL) {
			bfcp_free_user_information(beneficiary_info);
			return NULL;
		}
	} else
		beneficiary_info = NULL;
	/* If there's user information, add it */
	if(traverse->userID != 0) {
		user_info = bfcp_show_user_information(users, traverse->userID);
		if(user_info == NULL) {
		bfcp_free_user_information(user_info);
		return NULL;
		}
	} else
		user_info = NULL;

	node = traverse->floor;
	if(node != NULL) {
		/* Passing request status as argument and neglect the case of invalid primitive in BFCP message */
		fRS = bfcp_new_floor_request_status(node->floorID, request_status, 0, node->chair_info);
		if(fRS == NULL) return NULL;
		node = node->next;
		while(node != NULL) {
			/* Passing request status as argument and neglect the case of invalid primitive in BFCP message */
			fRS_temp = bfcp_new_floor_request_status(node->floorID, request_status, 0, node->chair_info);
			if(fRS_temp != NULL)
				bfcp_list_floor_request_status(fRS, fRS_temp, NULL);
			node=node->next;
		}
	} else
		return NULL;

	oRS = bfcp_new_overall_request_status(traverse->floorRequestID, request_status, i , traverse->chair_info);

	frqInfo = bfcp_new_floor_request_information(traverse->floorRequestID, oRS, fRS, beneficiary_info, user_info, traverse->priority ,traverse->participant_info);

	return frqInfo;
}

/* Create a new 'bfcp_floor_request_information' (bfcp_messages.h) out of a floor */
bfcp_floor_request_information *create_floor_message(unsigned short int floorID, pnode traverse, lusers users, unsigned short int request_status, int i)
{
	if(traverse == NULL)
		return NULL;
	if(users == NULL)
		return NULL;

	bfcp_user_information *user_info;
	bfcp_user_information *beneficiary_info;
	bfcp_floor_request_information *frqInfo = NULL;
	bfcp_floor_request_status *fRS = NULL;
	bfcp_overall_request_status *oRS = NULL;

	/* If there's a beneficiary, add its information */
	if(traverse->beneficiaryID != 0) {
		beneficiary_info = bfcp_show_user_information(users, traverse->beneficiaryID);
		if(beneficiary_info == NULL) {
			bfcp_free_user_information(beneficiary_info);
			return NULL;
		}
	} else
		beneficiary_info = NULL;

	/* If there's user information, add its information */
	if(traverse->userID != 0) {
		user_info = bfcp_show_user_information(users, traverse->userID);
		if(user_info == NULL) {
			bfcp_free_user_information(user_info);
			return NULL;
		}
	} else
		user_info = NULL;

	/* Setup the Floor Request Information by preparing its sub-attributes */
	fRS = bfcp_new_floor_request_status(floorID, request_status, 0, NULL);
	oRS = bfcp_new_overall_request_status(traverse->floorRequestID, request_status, i , traverse->chair_info);
	frqInfo = bfcp_new_floor_request_information(traverse->floorRequestID, oRS, fRS, beneficiary_info, user_info, traverse->priority ,traverse->participant_info);

	return frqInfo;
}

/* Setup and send a floorstatus BFCP message */
int bfcp_show_floor_information(unsigned long int conferenceID, unsigned short int TransactionID, unsigned short int userID, bfcp_conference *conference, unsigned short int floorID, int sockid, fd_set allset, int *client, pnode newnode, unsigned short int status)
{
	if(conference == NULL)
		return 0;

	int error, i;
	pnode traverse;
	pfloor floor;
	bfcp_message *message = NULL;
	bfcp_arguments *arguments = NULL;
	bfcp_floor_request_information *frqInfo = NULL, *list_frqInfo = NULL;
	bfcp_floor_id_list *fID;

	arguments = bfcp_new_arguments();
	if(!arguments)
		return -1;

	arguments->entity = bfcp_new_entity(conferenceID, TransactionID, userID);
	arguments->primitive = FloorStatus;
	if(floorID != 0)
		fID = bfcp_new_floor_id_list(floorID, 0);
	else fID = NULL;

	arguments->fID = fID;

	if((status > BFCP_GRANTED) && (newnode != NULL)) {
		frqInfo = create_floor_message(floorID, newnode, conference->user, status, 0);
		if((frqInfo != NULL) && (list_frqInfo != NULL))
			bfcp_add_floor_request_information_list(list_frqInfo, frqInfo, NULL);
		else if(list_frqInfo == NULL)
			list_frqInfo = frqInfo;
	}

	if(conference->granted != NULL) {
		/* This is the Granted queue */
		traverse = conference->granted->tail;
		while(traverse) {
			if((newnode == NULL) || (status <= BFCP_GRANTED) || ((newnode != NULL) && (newnode->floorRequestID != traverse->floorRequestID))) {
				floor = traverse->floor;
				while(floor) {
					if(floor->floorID == floorID) {
						frqInfo = create_floor_message(floorID, traverse, conference->user, 3, 0);
						if((frqInfo != NULL) && (list_frqInfo != NULL))
							bfcp_add_floor_request_information_list(list_frqInfo, frqInfo, NULL);
						else if(list_frqInfo == NULL)
							list_frqInfo = frqInfo;
					}
					floor = floor->next;
				}
			}
			traverse = traverse->prev;
		}
	}

	if(conference->accepted != NULL) {
		/* This is the Accepted queue */
		traverse = conference->accepted->tail;
		i = 1;
		while(traverse) {
			if((newnode == NULL) || (status <= BFCP_GRANTED) || ((newnode != NULL) && (newnode->floorRequestID != traverse->floorRequestID))) {
				floor = traverse->floor;
				while(floor) {
					if(floor->floorID == floorID) {
						frqInfo = create_floor_message(floorID, traverse, conference->user, 2, i);
						if((frqInfo != NULL) && (list_frqInfo != NULL))
							bfcp_add_floor_request_information_list(list_frqInfo, frqInfo, NULL);
						else if(list_frqInfo == NULL)
							list_frqInfo = frqInfo;
					}
					floor = floor->next;
				}
			}
			i = i + 1;
			traverse = traverse->prev;
		}
	}

	if(conference->pending != NULL) {
		/* This is the Accepted queue */
		traverse = conference->pending->tail;
		while(traverse) {
			if((newnode == NULL) || (status <= BFCP_GRANTED) || ((newnode != NULL) && (newnode->floorRequestID != traverse->floorRequestID))) {
				floor = traverse->floor;
				while(floor) {
					if(floor->floorID == floorID) {
						frqInfo = create_floor_message(floorID, traverse, conference->user, 1, 0);
						if((frqInfo != NULL) && (list_frqInfo != NULL))
							bfcp_add_floor_request_information_list(list_frqInfo, frqInfo, NULL);
						else if(list_frqInfo == NULL)
							list_frqInfo = frqInfo;
					}
					floor = floor->next;
				}
			}
			traverse = traverse->prev;
		}
	}

	arguments->frqInfo = list_frqInfo;

	if (conference->bfcp_transport == BFCP_OVER_UDP) {
		struct sockaddr_in *client_address = bfcp_get_user_address(conference->user, userID);
		error = send_message_to_client_over_udp(arguments, sockid, notify_to_server_app, client_address);
	} else {
		error = send_message_to_client(arguments, sockid, notify_to_server_app,
					       conference->bfcp_transport);
	}

	BFCP_SEND_CHECK_ERRORS(sockid);

	return 0;
}

/* Prepare the needed arguments for a FloorRequestStatus BFCP message */
int bfcp_print_information_floor(bfcp_conference *conference, unsigned short int userID, unsigned short int TransactionID, pnode newnode, unsigned short int status)
{
	if(conference == NULL)
		return 0;
	if(conference->floor == NULL)
		return 0;
	if(conference->floor->floors == NULL)
		return 0;
	if(status <= 0)
		return -1;
	if(newnode == NULL)
		return 0;

	int error, position;
	users user;
	pfloor floor;
	floor_request_query *newrequest;

	/* Prepare all floor information needed by interested users */
	floor = newnode->floor;
	while(floor) {
		position = bfcp_return_position_floor(conference->floor, floor->floorID);
		if(position >= 0) {
			if(conference->floor->floors != NULL) {
				user = conference->user->users;
				while (user) {
					if (status == BFCP_GRANTED && (user->userID != newnode->userID)) {
						/* Sending FloorStatus to all participants in conference except participant who was granted floor */
						error = bfcp_show_floor_information(conference->conferenceID,
								TransactionID, user->userID, conference, floor->floorID,
								user->fd, allset, client, newnode, status);
						if (error == -1) {
							return -1;
						}
					} else if (status == BFCP_RELEASED) {
						/* Sending FloorStatus to all participants in conference */
						error = bfcp_show_floor_information(conference->conferenceID,
								TransactionID, user->userID, conference, floor->floorID,
								user->fd, allset, client, newnode, status);
						if (error == -1) {
							return -1;
						}
					}

					user = user->next;
				}
			}
		}
		floor = floor->next;
	}

	newrequest = newnode->floorrequest;

	while(newrequest != NULL) {
		error = bfcp_show_requestfloor_information(conference->user, conference->accepted, conference->conferenceID, newrequest->userID, 0, newnode, status, newrequest->fd);
		if(error == -1)
			return -1;
		newrequest = newrequest->next;
	}

	return 0;
}

/* Setup and send a FloorRequestStatus BFCP message */
int bfcp_show_requestfloor_information(bfcp_list_users *list_users, bfcp_queue *accepted_queue, unsigned long int conferenceID, unsigned short int userID, unsigned short int TransactionID, pnode newnode, unsigned short int status, int socket)
{
	if(newnode == NULL)
		return 0;
	if(status <= 0)
		return 0;
	if(list_users == NULL)
		return 0;

	bfcp_arguments *arguments = NULL;
	bfcp_message *message = NULL;
	bfcp_floor *floorID;
	bfcp_user_information *beneficiary_info, *user_info;
	pnode traverse=NULL;
	bfcp_overall_request_status *oRS = NULL;
	bfcp_floor_request_status *fRS_temp = NULL, *fRS = NULL;
	int i = 0, error = 0;

	arguments = bfcp_new_arguments();
	if(!arguments)
		return -1;

	arguments->entity = bfcp_new_entity(conferenceID, TransactionID, userID);
	arguments->primitive = FloorRequestStatus;

	if(newnode->beneficiaryID != 0) {
		beneficiary_info = bfcp_new_user_information(newnode->beneficiaryID, (char *)bfcp_obtain_user_display_name(list_users, newnode->beneficiaryID), (char *)bfcp_obtain_userURI(list_users, newnode->beneficiaryID));
		if(beneficiary_info == NULL) {
			bfcp_free_user_information(beneficiary_info);
			return -1;
		}
	} else
		beneficiary_info = NULL;

	if(newnode->userID != 0) {
		user_info = bfcp_new_user_information(newnode->userID, (char *)bfcp_obtain_user_display_name(list_users, newnode->userID), (char *)bfcp_obtain_userURI(list_users, newnode->userID));
		if(user_info == NULL) {
			bfcp_free_user_information(user_info);
			return -1;
		}
	} else
		user_info = NULL;

	floorID = newnode->floor;
	if(floorID != NULL) {
		/* Passing request status as argument and neglect the case of invalid primitive in BFCP message */
		fRS = bfcp_new_floor_request_status(floorID->floorID, status, 0, floorID->chair_info);
	}
	if(fRS == NULL)
		return -1;
	floorID = floorID->next;
	while(floorID != NULL) {
		/* Passing request status as argument and neglect the case of invalid primitive in BFCP message */
		fRS_temp = bfcp_new_floor_request_status(floorID->floorID, status, 0, floorID->chair_info);
		if(fRS_temp != NULL) bfcp_list_floor_request_status(fRS, fRS_temp, NULL);
			floorID = floorID->next;
	}

	switch(status) {
		case BFCP_PENDING:
			/* Pending request */
			oRS = bfcp_new_overall_request_status(newnode->floorRequestID, status, newnode->priority, newnode->chair_info);
			break;
		case BFCP_ACCEPTED:
			/* Accepted request */
			oRS = bfcp_new_overall_request_status(newnode->floorRequestID, status, 0, newnode->chair_info);
			if(accepted_queue != NULL) {
				traverse = accepted_queue->tail;
				i = 1;
				while(traverse != NULL) {
					if(traverse->floorRequestID == newnode->floorRequestID) {
						oRS = bfcp_new_overall_request_status(traverse->floorRequestID, status, i , traverse->chair_info);
						break;
					} else
						i = i + 1;
					traverse = traverse->prev;
				}
			}
			break;
		default:
			oRS = bfcp_new_overall_request_status(newnode->floorRequestID, status, 0, newnode->chair_info);
				break;
	}

	arguments->frqInfo = bfcp_new_floor_request_information(newnode->floorRequestID, oRS, fRS, beneficiary_info, user_info, 0,newnode->participant_info);
	if (list_users->bfcp_transport == BFCP_OVER_UDP) {
		struct sockaddr_in *client_address = bfcp_get_user_address(list_users, userID);
		error = send_message_to_client_over_udp(arguments, socket, notify_to_server_app, client_address);
	} else {
		error = send_message_to_client(arguments, socket, notify_to_server_app,
								list_users->bfcp_transport);
	}

	BFCP_SEND_CHECK_ERRORS(socket);

	return 0;
}

/* Handle a BFCP message a client sent to the FCS */
bfcp_message *received_message_from_client(int sockfd, int i, int transport)
{
	int error = 0, total = 0;
	bfcp_message *message = NULL;
	int in_length;

	/* Reserve enough space for the common header (12 bytes) */
	unsigned char *common_header = (unsigned char *)calloc(1, 12);
	if(common_header == NULL)
		return NULL;

	if(transport == BFCP_OVER_TLS) {
		if(session[i])
			error = SSL_read(session[i], common_header, 12);
	} else	/* BFCP_OVER_TCP */
		error = recv(sockfd, common_header, 12, 0);
	if(error == 0) {
		/* The client closed the connection */
		free(common_header);
		common_header = NULL;
		close(sockfd);
		client[i] = -1;
#ifdef WIN32
		FD_CLR(sockfd, &allset);
#else
		int j=0;
		for(j = 0; j < fds_no; j++) {
			if(pollfds[j].fd == sockfd) {
				pollfds[j].fd = pollfds[fds_no-1].fd;
				pollfds[j].events = pollfds[fds_no-1].events;
				pollfds[j].revents = pollfds[fds_no-1].revents;
				fds_no--;
			}
		}
/*		pollfds[i+1].fd = -1;
		pollfds[i+1].events = 0;*/
#endif
		return NULL;
	}
	if(error == -1) {
		/* There was an error while receiving the message */
		free(common_header);
		common_header = NULL;
		return NULL;
	}

	message = bfcp_new_message(common_header, error);
	/* Get the message length from the header of the message and multiplying by 4 to get actual payload length, default it is in 4-byte units */
	in_length = bfcp_get_length(message)*4 + 12;
	/* Strip the header length, since we already got it: we are interested in the rest, the payload */
	total = in_length - 12;
	if(total < 0) {
		/* The reported length is corrupted */
		free(common_header);
		common_header = NULL;
		return NULL;
	}
	if(total == 0) {
		/* The whole message has already been received, there's no payload (e.g. ChairActionAck) */
		free(common_header);
		common_header = NULL;
		return message;
	}

	/* Reserve enough space for the rest of the message */
	unsigned char *buffer = (unsigned char *)calloc(1, total);
	if(buffer == NULL)
		return NULL;

	int missing = total;
	int bytes_read = 0;
	while(1) {
		if(missing <= 0)
			break;
		if(transport == BFCP_OVER_TLS) {	/* FIXME */
			if(session[i])
				error = SSL_read(session[i], buffer + bytes_read, total);
		} else	/* BFCP_OVER_TCP */
			error = recv(sockfd, buffer + bytes_read, total, 0);
		if(error == 0) {
			/* The FCS closed the connection */
			free(buffer);
			buffer = NULL;
			free(common_header);
			common_header = NULL;
			close(sockfd);
			return NULL;
		}
		if(error == -1) {
			/* There was an error while receiving the message */
			free(buffer);
			buffer = NULL;
			free(common_header);
			common_header = NULL;
			return NULL;
		}
		missing -= error;
		bytes_read += error;
	}
	if(in_length > BFCP_MAX_ALLOWED_SIZE) {
		/* The message is bigger than BFCP_MAX_ALLOWED_SIZE (64K), discard it */
		free(buffer);
		buffer = NULL;
		free(common_header);
		common_header = NULL;
		return NULL;
	}
	/* Remove the previously allocated message, which only contained the header */
	if(message != NULL)
		bfcp_free_message(message);

	/* Extend the buffer to hold all the message */
	common_header = (unsigned char *)realloc(common_header, in_length);

	if(memcpy(common_header+12, buffer,total) == NULL) {
		free(buffer);
		buffer = NULL;
		free(common_header);
		common_header = NULL;
		return NULL;
	}

	/* Create a new BFCP message out of the whole received buffer */
	message = bfcp_new_message(common_header, in_length);

	/* Free the space reserved for the buffers */
	free(buffer);
	buffer = NULL;
	free(common_header);
	common_header = NULL;
	return message;
}

/* Remove all floor requests made by a user from all existing nodes */
int bfcp_remove_floorrequest_from_all_nodes(bfcp_conference *server, unsigned short int userID)
{
	int error;

	if(server == NULL)
		return -1;

	error = bfcp_remove_floorrequest_from_a_queue(server->pending, userID);
	if(error == -1)
		return -1;

	error = bfcp_remove_floorrequest_from_a_queue(server->accepted, userID);
	if(error == -1)
		return -1;

	error = bfcp_remove_floorrequest_from_a_queue(server->granted, userID);
	if(error == -1)
		return -1;

	return 0;
}

 /* Handle a BFCP message a client sent to the FCS (UDP) */
bfcp_message *received_message_from_client_over_udp(int sockfd)
{
	int message_length = 0;
	bfcp_message *message = NULL;

	/* Reserve enough space for the common header (12 bytes) */
	unsigned char *message_read = (unsigned char *)calloc(1, BFCP_MAX_ALLOWED_SIZE);

	if (message_read == NULL) {
		return NULL;
	}

	memset(&cliaddr, 0, sizeof(cliaddr));

	message_length = recvfrom(sockfd, message_read, BFCP_MAX_ALLOWED_SIZE, 0, ( struct sockaddr *) &cliaddr, &m_addrlen);

	if (message_length == 0 || message_length == -1) {
		/* Message length was 0 or there was an error while receiving message */
		free(message_read);
		message_read = NULL;
		return NULL;
	}

	message = bfcp_new_message(message_read, message_length);

	free(message_read);
	message_read = NULL;
	return message;
}

/* Remove all floor requests made by a user from a queue */
int bfcp_remove_floorrequest_from_a_queue(bfcp_queue *conference, unsigned short int userID)
{
	if(conference == NULL)
		return -1;
	if(userID <= 0)
		return -1;

	pnode traverse;
	int error;

	traverse = conference->head;

	while(traverse) {
		error = remove_request_from_the_node(traverse, userID);
		if(error == -1)
			return -1;

		traverse = traverse->next;
	}

	return 0;
}

/* Disable and remove all floor events notifications to an user */
int bfcp_remove_floorquery_from_all_nodes(bfcp_list_floors *lfloors, unsigned short int userID)
{
	if(lfloors == NULL)
		return 0;
	if(lfloors->floors == NULL)
		return 0;
	if(userID <= 0)
		return -1;

	int i = 0, error = 0;

	for(i = 0; i < lfloors->actual_number_floors; i++) {
		error = remove_request_from_the_floor(lfloors->floors+i, userID);
		if(error == -1)
			return -1;
	}

	return 0;
}

/* A controller to check timeouts when waiting for a ChairAction */
void *watchdog(void *st_thread)
{
	bfcp_mutex_lock(&count_mutex);

	bfcp_thread *thread = (bfcp_thread *)st_thread;
	bfcp_conference *server;
	pfloor list_floors;
	unsigned long int chair_wait_request;
	unsigned long int floorRequestID;
	pnode traverse;

	server = thread->conf;
	floorRequestID = thread->floorRequestID;
	chair_wait_request = thread->chair_wait_request;

	/* Free the thread */
	free(thread);
	thread = NULL;

	bfcp_mutex_unlock(&count_mutex);

#ifndef WIN32
	sleep(chair_wait_request);
#else
	Sleep(chair_wait_request*1000);
#endif

	/*if the queue is a pending queue*/
	if((server != NULL) && (server->pending != NULL)) {
		traverse = server->pending->tail;
		while(traverse) {
			if((traverse->floorRequestID) == floorRequestID) {
				bfcp_print_information_floor(server, 0, 0, traverse, BFCP_CANCELLED);
				int beneficiary = traverse->beneficiaryID;
				if(beneficiary == 0)
					beneficiary = traverse->userID;
				list_floors = traverse->floor;
				while(list_floors != NULL) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(WATCHDOG) cancel: floor info is (tot %d) %d\n", server->floor->actual_number_floors, list_floors->floorID);
					int position_floor = bfcp_return_position_floor(server->floor, list_floors->floorID);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(WATCHDOG) cancel: position_floor=%d\n", position_floor);
					if(position_floor != -1) {
						int error = bfcp_deleted_user_request(server->user, beneficiary, position_floor);
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(WATCHDOG)   -- bfcp_deleted_user_request (user %hu) --> %d\n", beneficiary, error);
						//~ if(error == -1) {
							//~ bfcp_mutex_unlock(&count_mutex);
							//~ return -1;
						//~ }
					}
					list_floors = list_floors->next;
				}

			}
			traverse = traverse->prev;
		}
	}

	/* If the request is from the Pending list, remove it */
	list_floors = bfcp_delete_request(server->pending, floorRequestID, 0);

	/* Free all the elements from the floors list */
	remove_floor_list(list_floors);

	pthread_exit((void *)0);
	return NULL;
}

/* Handle an incoming FloorRequest message */
int bfcp_FloorRequest_server(bfcp_server *server, unsigned long int conferenceID, unsigned short int TransactionID, pnode newnode, int sockfd, int y)
{
	if(server == NULL)
		return -1;
	if(newnode == NULL)
		return -1;
	if(conferenceID <= 0)
		return -1;

	int actual_conference = 0, i, position_floor, error;
	unsigned short int chairID;
	pfloor tempnode, floor;
	pthread_t wid;	/* The thread identifier */
	struct_thread st_thread;
	unsigned long int floorRequestID;

	actual_conference = server->Actual_number_conference - 1;
	for(i = 0; i < actual_conference; i++) {
		if(server->list_conferences[i].conferenceID == conferenceID)
			break;
	}

	bfcp_mutex_lock(&count_mutex);

	if(server->list_conferences[i].floorRequestID <= 0)
		server->list_conferences[i].floorRequestID = 1;
	floorRequestID = server->list_conferences[i].floorRequestID;

	newnode->floorRequestID = floorRequestID;

	/* A buffer to compose error text messages when needed */
	char errortext[200];

	/* Check if this conference exists */
	if(server->list_conferences[i].conferenceID != conferenceID) {
		sprintf(errortext, "Conference %lu does not exist", conferenceID);
		bfcp_error_code(conferenceID, newnode->userID, TransactionID, BFCP_CONFERENCE_DOES_NOT_EXIST, errortext, NULL, sockfd, y, server->bfcp_transport);
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	/* Check if this user exists */
	if(bfcp_existence_user(server->list_conferences[i].user, newnode->userID) != 0) {
		sprintf(errortext, "User %hu does not exist in Conference %lu", newnode->userID, conferenceID);
		bfcp_error_code(conferenceID, newnode->userID, TransactionID, BFCP_USER_DOES_NOT_EXIST, errortext, NULL, sockfd, y, server->bfcp_transport);
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	/* If this FloorRequest has a beneficiaryID, check if the user is allowed to do this operation */
	if((newnode->beneficiaryID != 0) && (bfcp_exist_user_as_a_chair(server->list_conferences[i].floor, newnode->userID) != 0)) {
		sprintf(errortext, "Third-party FloorRequests only allowed for chairs (User %hu is not chair of any floor)", newnode->userID);
		bfcp_error_code(conferenceID, newnode->userID, TransactionID, BFCP_UNAUTHORIZED_OPERATION, errortext, NULL, sockfd, y, server->bfcp_transport);
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	/* Checks if the beneficiary user exists in the conference */
	if((newnode->beneficiaryID != 0) && (bfcp_existence_user(server->list_conferences[i].user, newnode->beneficiaryID) != 0)) {
		sprintf(errortext, "User %hu (beneficiary of the request) does not exist in Conference %lu", newnode->beneficiaryID, conferenceID);
		bfcp_error_code(conferenceID, newnode->userID, TransactionID, BFCP_USER_DOES_NOT_EXIST, errortext, NULL, sockfd, y, server->bfcp_transport);
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	/* Check if all the floors exist in the conference, otherwise send an error */
	if((newnode->floor) != NULL) {
		for(tempnode = newnode->floor; tempnode; tempnode = tempnode->next) {
			position_floor = bfcp_return_position_floor(server->list_conferences[i].floor, tempnode->floorID);
			if(position_floor == -1) {
				sprintf(errortext, "Floor %hu does not exist in Conference %lu", tempnode->floorID, conferenceID);
				bfcp_error_code(conferenceID, newnode->userID, TransactionID, BFCP_INVALID_FLOORID, errortext, NULL, sockfd, y, server->bfcp_transport);
				bfcp_mutex_unlock(&count_mutex);
				return -1;
			}

			/* If no chair is present and the policy is to autodeny requests, remove the request node */
			if((server->list_conferences[i].automatic_accepted_deny_policy == 1) && (bfcp_return_chair_floor(server->list_conferences[i].floor, tempnode->floorID) == 0)) {
				error = bfcp_show_requestfloor_information(server->list_conferences[i].user, server->list_conferences[i].accepted, server->list_conferences[i].conferenceID, newnode->userID, TransactionID, newnode, BFCP_DENIED, sockfd);
				if(error == -1) {
					bfcp_mutex_unlock(&count_mutex);
					return -1;
				}
				/* Remove the request node */
				remove_floor_list(newnode->floor);
				remove_request_list_of_node(newnode->floorrequest);
				free(newnode->participant_info);
				newnode->participant_info = NULL;
				free(newnode->chair_info);
				newnode->chair_info = NULL;
				free(newnode);
				newnode = NULL;
				bfcp_mutex_unlock(&count_mutex);
				return 0;
			}

			/* Check if this user has already reached the maximum number of ongoing requests for the same floor */
			if(bfcp_is_floor_request_full(conferenceID, TransactionID, server->list_conferences[i].user, newnode->beneficiaryID ? newnode->beneficiaryID : newnode->userID, position_floor, sockfd, y) == -1) {
				/* Remove the request node */
				remove_floor_list(newnode->floor);
				remove_request_list_of_node(newnode->floorrequest);
				free(newnode->participant_info);
				newnode->participant_info = NULL;
				free(newnode->chair_info);
				newnode->chair_info = NULL;
				free(newnode);
				newnode = NULL;
				bfcp_mutex_unlock(&count_mutex);
				return 0;
			}
		}
	} else {
		sprintf(errortext, "There are no floors in Conference %lu", conferenceID);
		bfcp_error_code(conferenceID, newnode->userID, TransactionID, BFCP_INVALID_FLOORID, errortext, NULL, sockfd, y, server->bfcp_transport);
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	/* Checks in which position each floor is, and add the request to the user's list */
	position_floor = 0;
	for(tempnode = newnode->floor; tempnode; tempnode = tempnode->next) {
		position_floor = bfcp_return_position_floor(server->list_conferences[i].floor, tempnode->floorID);
		if(position_floor != -1) {
			/* If the floor(s) is/are for a beneficiary, add the request to the beneficiary's list */
			if(newnode->beneficiaryID != 0)
				error = bfcp_add_user_request(conferenceID, TransactionID, server->list_conferences[i].user, newnode->beneficiaryID, position_floor, sockfd, y);
			else
				error = bfcp_add_user_request(conferenceID, TransactionID, server->list_conferences[i].user, newnode->userID, position_floor, sockfd, y);
			if(error == -1) {
				/* Remove the node */
				remove_floor_list(newnode->floor);
				remove_request_list_of_node(newnode->floorrequest);
				free(newnode->participant_info);
				newnode->participant_info = NULL;
				free(newnode->chair_info);
				newnode->chair_info = NULL;
				free(newnode);
				newnode = NULL;
				bfcp_mutex_unlock(&count_mutex);
				return 0;
			}
		} else {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}
	}

	/* Handle the Chair part */

	/* For each floor in the request, accept/deny it or ask for the chair's approval */
	if((newnode->floor) != NULL) {
		for(tempnode = newnode->floor; tempnode; tempnode = tempnode->next) {
			/*checks if this floor has a chair*/
			chairID= bfcp_return_chair_floor(server->list_conferences[i].floor, tempnode->floorID);
			/*if it has a chair*/
			if(chairID != 0) {
				/* Allocate a new thread handler */
				st_thread = (struct_thread)calloc(1, sizeof(bfcp_thread));
				if(st_thread == NULL) {
					bfcp_mutex_unlock(&count_mutex);
					return -1;
				}

				/* Initialize the structure the thread will handle */
				st_thread->conf = server->list_conferences + i;
				st_thread->chair_wait_request = server->list_conferences[i].chair_wait_request;
				st_thread->floorRequestID = floorRequestID;

				/* Create the thread */
				if( pthread_create(&wid, NULL, watchdog, st_thread) < 0) {
					bfcp_mutex_unlock(&count_mutex);
					return -1;
				}
				pthread_detach(wid);

				/*put the pID in the node*/
				tempnode->pID = wid;
			} else
				/* Change status of the floor to Accepted */
				tempnode->status = 1;
		}
	}

	/* Check if the floors are FREE (TODO check state) */
	floor = newnode->floor;
	while(floor != NULL) {
		if(floor->status == 1)
			floor = floor->next;
		else
			break;
	}

	if(floor == NULL) {
		/*put the node in the accept list*/
		/*change the priority of the node to the lowest one*/
		newnode->priority = 0;

		/* Remove the threads handling this node */
		floor = newnode->floor;
		while(floor) {
#ifndef WIN32
			if(floor->pID != 0) {
#else
			if(floor->pID.p != NULL) {
#endif
				pthread_cancel(floor->pID);
#ifndef WIN32
				floor->pID = 0;
#else
				floor->pID.p = NULL;
#endif
			}
			floor = floor->next;
		}

		error = bfcp_insert_request(server->list_conferences[i].accepted, newnode, floorRequestID, NULL);
		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}

		/* Prepare all floor information needed by interested users */
		error = bfcp_print_information_floor(server->list_conferences+i, 0, 0, newnode, BFCP_ACCEPTED);
		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}

		/* Notify the requester about all the most important information about his request */
		error = bfcp_show_requestfloor_information(server->list_conferences[i].user, server->list_conferences[i].accepted, server->list_conferences[i].conferenceID, newnode->userID, TransactionID, newnode, BFCP_ACCEPTED, sockfd);
		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}

		/* Check if the node should be moved to the Granted queue */
		if(server->list_conferences[i].floor != NULL) {
			if(server->list_conferences[i].floor->floors != NULL) {
				for (y = (server->list_conferences[i].floor->actual_number_floors-1); 0 <= y; y--) {
					if(bfcp_return_state_floor(server->list_conferences[i].floor, server->list_conferences[i].floor->floors[y].floorID)<bfcp_return_number_granted_floor(server->list_conferences[i].floor, server->list_conferences[i].floor->floors[y].floorID)) {
						if(check_accepted_node(server->list_conferences+i, newnode, server->list_conferences[i].floor->floors[y].floorID, NULL) == 0) {
							/* Notify the requester about all the most important information about his request */
							error = bfcp_show_requestfloor_information(server->list_conferences[i].user, server->list_conferences[i].accepted, server->list_conferences[i].conferenceID, newnode->userID, TransactionID, newnode, BFCP_GRANTED, sockfd);
							if(error == -1) {
								bfcp_mutex_unlock(&count_mutex);
								return -1;
							}
						}
					}
				}
			}
		}
	} else {
		/*put the node in the pending list*/
		error = bfcp_insert_request(server->list_conferences[i].pending, newnode, floorRequestID, NULL);
		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}

		/* Prepare all floor information needed by interested users */
		error = bfcp_print_information_floor(server->list_conferences+i, 0, 0, newnode, BFCP_PENDING);
		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}

		/* Notify the requester about all the most important information about his request */
		error = bfcp_show_requestfloor_information(server->list_conferences[i].user, server->list_conferences[i].accepted, server->list_conferences[i].conferenceID, newnode->userID, TransactionID, newnode, BFCP_PENDING, sockfd);
		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}
	}

	server->list_conferences[i].floorRequestID = server->list_conferences[i].floorRequestID + 1;

	/* The requester will constantly be notified about updates concerning his request */
	error = add_request_to_the_node(newnode, newnode->userID, sockfd);
	if(error == -1) {
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	bfcp_mutex_unlock(&count_mutex);

	return 0;
}

/* Handle an incoming FloorRelease message */
/*RequestStatus ->Cancelled if the floor had not been previous granted- Released if the floor had been previous granted*/
int bfcp_FloorRelease_server(bfcp_server *server, unsigned long int conferenceID, unsigned short int TransactionID, unsigned short int userID, unsigned long int floorRequestID, int sockfd, int y)
{
	if(server == NULL)
		return -1;
	if(conferenceID <= 0)
		return -1;

	int i, position_floor, actual_conference = 0, error, status_floor = 0;
	pfloor temp, list_floors = NULL, floor;
	pnode newnode = NULL;
	bfcp_queue *laccepted;

	actual_conference = server->Actual_number_conference - 1;
	for(i = 0; i < actual_conference; i++) {
		if(server->list_conferences[i].conferenceID == conferenceID)
			break;
	}

	bfcp_mutex_lock(&count_mutex);

	/* A buffer to compose error text messages when needed */
	char errortext[200];

	/* Check if this conference exists */
	if(server->list_conferences[i].conferenceID != conferenceID) {
		sprintf(errortext, "Conference %lu does not exist", conferenceID);
		bfcp_error_code(conferenceID, newnode->userID, TransactionID, BFCP_CONFERENCE_DOES_NOT_EXIST, errortext, NULL, sockfd, y, server->bfcp_transport);
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	/* Check if this user exists */
	if(bfcp_existence_user(server->list_conferences[i].user, userID) != 0) {
		sprintf(errortext, "User %hu does not exist in Conference %lu", userID, conferenceID);
		bfcp_error_code(conferenceID, userID, TransactionID, BFCP_USER_DOES_NOT_EXIST, errortext, NULL, sockfd, y, server->bfcp_transport);
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	/* If floorRequestID is 0 then fetching floorRequestID from accepeted/pending/granted
		floor list mapped with userID, if not found then generating error */
	if (floorRequestID <= 0 &&
        !(floorRequestID = bfcp_get_floor_requestid(server->list_conferences[i].accepted, userID)) &&
        !(floorRequestID = bfcp_get_floor_requestid(server->list_conferences[i].pending, userID)) &&
        !(floorRequestID = bfcp_get_floor_requestid(server->list_conferences[i].granted, userID))) {
        sprintf(errortext, "FloorRequest for user %hu does not exist in Conference %lu", userID, conferenceID);
        bfcp_error_code(conferenceID, userID, TransactionID, BFCP_FLOORREQUEST_DOES_NOT_EXIST, errortext,
		                NULL, sockfd, y, server->bfcp_transport);
        bfcp_mutex_unlock(&count_mutex);
        return -1;
	}

	/* Check if this request node is in the Accepted list */
	if(bfcp_give_user_of_request(server->list_conferences[i].accepted, floorRequestID) == userID) {
		newnode = bfcp_extract_request(server->list_conferences[i].accepted, floorRequestID);
		status_floor = BFCP_FLOOR_STATE_ACCEPTED;
	}

	if(newnode == NULL) {
		/* First check if the request node is in the Pending list */
		if(bfcp_give_user_of_request(server->list_conferences[i].pending, floorRequestID) == userID) {
			bfcp_kill_threads_request_with_FloorRequestID(server->list_conferences[i].pending, floorRequestID);
			newnode = bfcp_extract_request(server->list_conferences[i].pending, floorRequestID);
			status_floor = BFCP_FLOOR_STATE_ACCEPTED;
		}
		if(newnode == NULL) {
			/* Then check if the request node is in the Granted list */
			if(bfcp_give_user_of_request(server->list_conferences[i].granted, floorRequestID) == userID) {
				newnode = bfcp_extract_request(server->list_conferences[i].granted, floorRequestID);
				status_floor = BFCP_FLOOR_STATE_GRANTED;
			}
			if(newnode == NULL) {
				sprintf(errortext, "FloorRequest %lu does not exist in Conference %lu", floorRequestID, conferenceID);
				bfcp_error_code(conferenceID, userID, TransactionID, BFCP_FLOORREQUEST_DOES_NOT_EXIST, errortext, NULL, sockfd, y, server->bfcp_transport);
				bfcp_mutex_unlock(&count_mutex);
				return -1;
			}
		} else {
			/* Remove the thread if this request node is in the Pending list */
			floor = newnode->floor;
			while(floor) {
#ifndef WIN32
				if(floor->pID != 0) {
#else
				if(floor->pID.p != NULL) {
#endif
					pthread_cancel(floor->pID);
#ifndef WIN32
					floor->pID = 0;
#else
					floor->pID.p = NULL;
#endif
				}
				floor = floor->next;
			}
		}
	}

	/* Notify the releaser about the floor request information of the released request */
	if(status_floor == BFCP_FLOOR_STATE_ACCEPTED)
		error = bfcp_show_requestfloor_information(server->list_conferences[i].user, server->list_conferences[i].accepted, server->list_conferences[i].conferenceID, userID, TransactionID, newnode, BFCP_CANCELLED, sockfd);
	else
		error = bfcp_show_requestfloor_information(server->list_conferences[i].user, server->list_conferences[i].accepted, server->list_conferences[i].conferenceID, userID, TransactionID, newnode, BFCP_RELEASED, sockfd);
	if(error == -1) {
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	error = remove_request_from_the_node(newnode, userID);
	if(error == -1) {
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	/* Prepare all floor information needed by interested users */

	if(status_floor == BFCP_FLOOR_STATE_ACCEPTED)
		error = bfcp_print_information_floor(server->list_conferences+i, 0, 0, newnode, BFCP_CANCELLED);
	else
		error = bfcp_print_information_floor(server->list_conferences+i, 0, 0, newnode, BFCP_RELEASED);
	if(error == -1) {
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	list_floors = newnode->floor;

	/* Remove the list of requests */
	remove_request_list_of_node(newnode->floorrequest);

	free(newnode->participant_info);
	newnode->participant_info = NULL;
	free(newnode->chair_info);
	newnode->chair_info = NULL;
	free(newnode);
	newnode = NULL;

	while(list_floors != NULL) {
		position_floor = bfcp_return_position_floor(server->list_conferences[i].floor, list_floors->floorID);

		if(position_floor != -1) {
			if((bfcp_return_state_floor(server->list_conferences[i].floor, server->list_conferences[i].floor->floors[position_floor].floorID) >= 2) && (list_floors->status == 2))
				/* Change state of the floor to available (FREE) */
				bfcp_change_state_floor(server->list_conferences[i].floor, list_floors->floorID, 1);

			error = bfcp_deleted_user_request(server->list_conferences[i].user, userID, position_floor);
			if(error == -1) {
				bfcp_mutex_unlock(&count_mutex);
				return -1;
			}
		}

		/* Check if other requests that need this floor are in the Accepted list */
		laccepted = server->list_conferences[i].accepted;

		if(give_free_floors_to_the_accepted_nodes(server->list_conferences+i, laccepted, server->list_conferences[i].floor, NULL) == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}

		/* Destroy the floors list */
		temp = list_floors;
		list_floors = list_floors->next;
		free(temp->chair_info);
		temp->chair_info = NULL;
		free(temp);
		temp = NULL;
	}

	bfcp_mutex_unlock(&count_mutex);

	return 0;
}

/* Handle an incoming ChairAction message */
int bfcp_ChairAction_server(bfcp_server *server, unsigned long int conferenceID, bfcp_floor *list_floors, unsigned short int userID, unsigned long int floorRequestID, int RequestStatus, char *chair_info, int queue_position, unsigned short int TransactionID, int sockfd, int y)
{
	if(server == NULL)
		return -1;
	if(conferenceID <= 0)
		return -1;
	if(list_floors == NULL)
		return -1;
	if(userID <= 0)
		return -1;
	if(floorRequestID <= 0)
		return -1;
	if((RequestStatus != BFCP_ACCEPTED) && (RequestStatus != BFCP_DENIED) && (RequestStatus != BFCP_REVOKED))
		return -1;
	if(queue_position < 0)
		return -1;
	if(TransactionID <= 0)
		return -1;

	int actual_conference = 0, i, error, position_floor;
	pfloor floor, next, next_floors, tempnode, free_floors, tempfloors = NULL;
	bfcp_node *newnode = NULL;
	bfcp_floor *node = NULL;
	bfcp_queue *laccepted;
	bfcp_arguments *arguments = NULL;
	bfcp_message *message = NULL;
	int dLen;

	actual_conference = server->Actual_number_conference - 1;
	for(i = 0; i < actual_conference; i++) {
		if(server->list_conferences[i].conferenceID == conferenceID)
			break;
	}

	bfcp_mutex_lock(&count_mutex);

	/* A buffer to compose error text messages when needed */
	char errortext[200];

	/* Check if this conference exists */
	if(server->list_conferences[i].conferenceID != conferenceID) {
		sprintf(errortext, "Conference %lu does not exist", conferenceID);
		bfcp_error_code(conferenceID, newnode->userID, TransactionID, BFCP_CONFERENCE_DOES_NOT_EXIST, errortext, NULL, sockfd, y, server->bfcp_transport);
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	/* Check if this user exists */
	if(bfcp_existence_user(server->list_conferences[i].user, userID) != 0) {
		sprintf(errortext, "User %hu does not exist in Conference %lu", userID, conferenceID);
		bfcp_error_code(conferenceID, userID, TransactionID, BFCP_USER_DOES_NOT_EXIST, errortext, NULL, sockfd, y, server->bfcp_transport);
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	/* Check if all the floors exist in the conference, otherwise send an error */
	for(tempnode = list_floors; tempnode; tempnode = tempnode->next) {
		position_floor = bfcp_return_position_floor(server->list_conferences[i].floor, tempnode->floorID);
		if(position_floor == -1) {
			sprintf(errortext, "Floor %hu does not exist in Conference %lu", tempnode->floorID, conferenceID);
			bfcp_error_code(conferenceID, userID, TransactionID, BFCP_INVALID_FLOORID, errortext, NULL, sockfd, y, server->bfcp_transport);
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}

		/* Check if the user is allowed to do this operation */
		if(bfcp_return_chair_floor(server->list_conferences[i].floor, tempnode->floorID) != userID) {
			sprintf(errortext, "User %hu is not chair of Floor %hu in Conference %lu", userID, tempnode->floorID, conferenceID);
			bfcp_error_code(conferenceID, userID, TransactionID, BFCP_UNAUTHORIZED_OPERATION, errortext, NULL, sockfd, y, server->bfcp_transport);
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}
	}

	if(RequestStatus == BFCP_ACCEPTED) {
		/* The chair accepted a request */

		/* First check if the request node is in the Pending list */
		if(bfcp_give_user_of_request(server->list_conferences[i].pending, floorRequestID) == 0) {
			sprintf(errortext, "Pending FloorRequest %lu does not exist in Conference %lu", floorRequestID, conferenceID);
			bfcp_error_code(conferenceID, userID, TransactionID, BFCP_FLOORREQUEST_DOES_NOT_EXIST, errortext, NULL, sockfd, y, server->bfcp_transport);
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}
		/* Check if the floors involved in the accepted request exist */
		for(tempnode = list_floors; tempnode != NULL; tempnode = tempnode->next) {
			if(bfcp_change_status(server->list_conferences[i].pending, tempnode->floorID, floorRequestID, BFCP_FLOOR_STATE_ACCEPTED, tempnode->chair_info) != 0) {
				sprintf(errortext, "Floor %hu does not exist in Conference %lu", tempnode->floorID, conferenceID);
				bfcp_error_code(conferenceID, userID, TransactionID, BFCP_INVALID_FLOORID, errortext, NULL, sockfd, y, server->bfcp_transport);
				bfcp_mutex_unlock(&count_mutex);
				return -1;
			}
		}

		/* Set the queue_position value if it is needed */
		bfcp_change_queue_position(server->list_conferences[i].pending, floorRequestID, queue_position);

		/* If all the floors in the request have been accepted... */
		if(bfcp_all_floor_status(server->list_conferences[i].pending, floorRequestID, BFCP_FLOOR_STATE_ACCEPTED) == 0) {
			/* Extract the request node from the Pending list */
			newnode = bfcp_extract_request(server->list_conferences[i].pending, floorRequestID);

			/* Remove the threads handling this node */
			floor = newnode->floor;
			while(floor) {
#ifndef WIN32
				if(floor->pID != 0) {
#else
				if(floor->pID.p != NULL) {
#endif
					pthread_cancel(floor->pID);
#ifndef WIN32
					floor->pID = 0;
#else
					floor->pID.p = NULL;
#endif
				}
				floor = floor->next;
			}

			/* Move the node to the Accepted list */

			newnode->priority = BFCP_LOWEST_PRIORITY;

			error = bfcp_insert_request(server->list_conferences[i].accepted, newnode, floorRequestID, chair_info);
			if(error == -1) {
				bfcp_mutex_unlock(&count_mutex);
				return -1;
			}

			/* Prepare all floor information needed by interested users */
			error = bfcp_print_information_floor(server->list_conferences+i, 0, 0, newnode, BFCP_ACCEPTED);
			if(error == -1) {
				bfcp_mutex_unlock(&count_mutex);
				return -1;
			}

			/* Check if the node should be in the Granted queue */
			laccepted = server->list_conferences[i].accepted;
			if(give_free_floors_to_the_accepted_nodes(server->list_conferences+i, laccepted, server->list_conferences[i].floor, chair_info) == -1) {
				bfcp_mutex_unlock(&count_mutex);
				return -1;
			}
		}
	} else if(RequestStatus == BFCP_DENIED) {
		/* The chair denied a pending request */

		/* Extract the request node from the Pending list */
		newnode = bfcp_extract_request(server->list_conferences[i].pending, floorRequestID);
		if(newnode == NULL) {
			newnode = bfcp_extract_request(server->list_conferences[i].accepted, floorRequestID);
			if(newnode == NULL) {
				sprintf(errortext, "FloorRequest %lu does not exist in Conference %lu, neither Pending or Accepted", floorRequestID, conferenceID);
				bfcp_error_code(conferenceID, userID, TransactionID, BFCP_FLOORREQUEST_DOES_NOT_EXIST, errortext, NULL, sockfd, y, server->bfcp_transport);
				bfcp_mutex_unlock(&count_mutex);
				return -1;
			}
		}

		unsigned short int beneficiary = newnode->beneficiaryID;
		if(beneficiary == 0)
			beneficiary = newnode->floorrequest->userID;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Denying floor to beneficiary %hu\n", beneficiary);

		/* If there's chair-provided text information, add it to the request node */
		if(chair_info != NULL) {
			dLen = strlen(chair_info);
			if(dLen != 0) {
				if(newnode->chair_info != NULL) {
					free(newnode->chair_info);
					newnode->chair_info = NULL;
				}
				newnode->chair_info = (char *)calloc(1, dLen*sizeof(char)+1);
				memcpy(newnode->chair_info, chair_info, dLen+1);
			}
		}

		/* Remove the threads handling this node */
		floor = newnode->floor;
		while(floor != NULL) {
#ifndef WIN32
			if(floor->pID != 0) {
#else
			if(floor->pID.p != NULL) {
#endif
				pthread_cancel(floor->pID);
#ifndef WIN32
				floor->pID = 0;
#else
				floor->pID.p = NULL;
#endif
			}
			floor = floor->next;
		}

		/* Add the chair-provided information to each floor */
		for(tempnode = list_floors; tempnode != NULL; tempnode = tempnode->next) {
			for(node = newnode->floor; node != NULL; node = node->next) {
				if(node->floorID == tempnode->floorID) {
					/* If there's chair-provided text for this floor, add it */
					if(tempnode->chair_info != NULL) {
						dLen = strlen(tempnode->chair_info);
						if(dLen != 0) {
							if(node->chair_info != NULL) {
								free(node->chair_info);
								node->chair_info = NULL;
							}
							node->chair_info = (char *)calloc(1, dLen*sizeof(char)+1);
							memcpy(node->chair_info, tempnode->chair_info, dLen+1);
						}
					} else
						node->chair_info = NULL;
				}
			}
		}
		/* Prepare all floor information needed by interested users */
		error = bfcp_print_information_floor(server->list_conferences+i, 0, 0, newnode, BFCP_DENIED);
		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}

		/* Decrease requests counter */
		free_floors = newnode->floor;
		while(free_floors != NULL) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "deny: floor info is (tot %d) %d\n", server->list_conferences[i].floor->actual_number_floors, free_floors->floorID);
			int position_floor = bfcp_return_position_floor(server->list_conferences[i].floor, free_floors->floorID);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "deny: position_floor=%d\n", position_floor);
			if(position_floor != -1) {
				error = bfcp_deleted_user_request(server->list_conferences[i].user, beneficiary, position_floor);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "  -- bfcp_deleted_user_request (user %hu) --> %d\n", beneficiary, error);
				if(error == -1) {
					bfcp_mutex_unlock(&count_mutex);
					return -1;
				}
			}
			free_floors = free_floors->next;
		}

		/* Remove the request node from the Pending list */
		if(newnode != NULL) {
			tempfloors = newnode->floor;
			remove_floor_list(newnode->floor);
			remove_request_list_of_node(newnode->floorrequest);
			free(newnode->participant_info);
			newnode->participant_info = NULL;
			free(newnode->chair_info);
			newnode->chair_info = NULL;
			free(newnode);
			newnode = NULL;
		}

	} else if(RequestStatus == BFCP_REVOKED) {
		/* The chair revoked a previously granted request */

		/* Extract the request node from the Granted list */
		newnode = bfcp_extract_request(server->list_conferences[i].granted, floorRequestID);
		if(newnode == NULL) {
			sprintf(errortext, "Granted FloorRequest %lu does not exist in Conference %lu", floorRequestID, conferenceID);
			bfcp_error_code(conferenceID, userID, TransactionID, BFCP_FLOORREQUEST_DOES_NOT_EXIST, errortext, NULL, sockfd, y, server->bfcp_transport);
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}

		unsigned short int beneficiary = newnode->beneficiaryID;
		if(beneficiary == 0)
			beneficiary = newnode->floorrequest->userID;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Revoking floor to beneficiary %hu\n", beneficiary);

		/* If there's chair-provided text information, add it to the request node */
		if(chair_info != NULL) {
			dLen = strlen(chair_info);
			if(dLen != 0) {
				if(newnode->chair_info != NULL)
					free(newnode->chair_info);
				newnode->chair_info = (char *)calloc(1, dLen*sizeof(char)+1);
				memcpy(newnode->chair_info, chair_info, dLen+1);
			}
		}

		/* Add the chair-provided information to each floor */
		for(tempnode = list_floors; tempnode != NULL; tempnode = tempnode->next) {
			for(node = newnode->floor; node != NULL; node = node->next) {
				if(node->floorID == tempnode->floorID) {
					/* If there's chair-provided text for this floor, add it */
					if(tempnode->chair_info != NULL) {
						dLen = strlen(tempnode->chair_info);
						if(dLen != 0) {
							if(node->chair_info != NULL) {
								free(node->chair_info);
								node->chair_info = NULL;
							}
							node->chair_info = (char *)calloc(1, dLen*sizeof(char)+1);
							memcpy(node->chair_info, tempnode->chair_info, dLen+1);
						}
					} else
						node->chair_info = NULL;
				}
			}
		}

		/* Prepare all floor information needed by interested users */
		error = bfcp_print_information_floor(server->list_conferences+i, 0, 0, newnode, BFCP_REVOKED);
		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}

		/* Remove the request node from the Granted list */
		if(newnode != NULL) {
			tempfloors = newnode->floor;
			remove_request_list_of_node(newnode->floorrequest);
			free(newnode->participant_info);
			newnode->participant_info = NULL;
			free(newnode->chair_info);
			newnode->chair_info = NULL;
			free(newnode);
			newnode = NULL;
		}

		/* Set the state of the floors to FREE */
		free_floors = tempfloors;
		while(free_floors != NULL) {
			int position_floor = bfcp_return_position_floor(server->list_conferences[i].floor, free_floors->floorID);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "revoke: position_floor=%d\n", position_floor);
			if(position_floor != -1) {
				error = bfcp_change_state_floor(server->list_conferences[i].floor, free_floors->floorID, 1);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "  -- bfcp_change_state_floor --> %d\n", error);
				if(error == -1) {
					bfcp_mutex_unlock(&count_mutex);
					return -1;
				}
				error = bfcp_deleted_user_request(server->list_conferences[i].user, beneficiary, position_floor);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "  -- bfcp_deleted_user_request (user %hu) --> %d\n", beneficiary, error);
				if(error == -1) {
					bfcp_mutex_unlock(&count_mutex);
					return -1;
				}
			}
			next = free_floors->next;
			free(free_floors->chair_info);
			free_floors->chair_info = NULL;
			free(free_floors);
			free_floors = NULL;
			free_floors = next;
		}

		laccepted = server->list_conferences[i].accepted;
		if(give_free_floors_to_the_accepted_nodes(server->list_conferences+i, laccepted, server->list_conferences[i].floor, chair_info) == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}
	} else {
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	/* Free the floors list */
	while(list_floors) {
		next_floors = list_floors->next;
		free(list_floors->chair_info);
		list_floors->chair_info = NULL;
		free(list_floors);
		list_floors = NULL;
		list_floors = next_floors;
	}

	/* Send the ChairActionAck to the client */
	arguments = bfcp_new_arguments();
	if(!arguments) {
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}
	arguments->entity = bfcp_new_entity(conferenceID,TransactionID,userID);
	arguments->primitive = ChairActionAck;

	bfcp_mutex_unlock(&count_mutex);

	if (server->bfcp_transport == BFCP_OVER_UDP) {
		struct sockaddr_in *client_address = bfcp_get_user_address_server(server, conferenceID, userID);
		error = send_message_to_client_over_udp(arguments, sockfd, notify_to_server_app, client_address);
	} else {
		error = send_message_to_client(arguments, sockfd, notify_to_server_app, server->bfcp_transport);
	}

	BFCP_SEND_CHECK_ERRORS(sockfd);

	return 0;
}

/* Handle an incoming Hello message */
int bfcp_hello_server(bfcp_server *server, unsigned long int conferenceID, unsigned short int userID, unsigned short int TransactionID, int sockfd, int y)
{
	if(server == NULL)
		return -1;
	if(conferenceID <= 0)
		return -1;
	if(userID <= 0)
		return -1;
	if(TransactionID <= 0)
		return -1;

	int actual_conference = 0, i, error;
	bfcp_arguments *arguments = NULL;
	bfcp_message *message = NULL;

	actual_conference = server->Actual_number_conference - 1;
	for(i = 0; i < actual_conference; i++) {
		if(server->list_conferences[i].conferenceID == conferenceID)
		break;
	}

	bfcp_mutex_lock(&count_mutex);

	/* A buffer to compose error text messages when needed */
	char errortext[200];

	/* Check if this conference exists */
	if(server->list_conferences[i].conferenceID != conferenceID) {
		sprintf(errortext, "Conference %lu does not exist", conferenceID);
		bfcp_error_code(conferenceID, userID, TransactionID, BFCP_CONFERENCE_DOES_NOT_EXIST, errortext, NULL, sockfd, y, server->bfcp_transport);
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	/* Check if this user exists */
	if(bfcp_existence_user(server->list_conferences[i].user, userID) != 0) {
		sprintf(errortext, "User %hu does not exist in Conference %lu", userID, conferenceID);
		bfcp_error_code(conferenceID, userID, TransactionID, BFCP_USER_DOES_NOT_EXIST, errortext, NULL, sockfd, y, server->bfcp_transport);
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	/* Set sockfd with userID in user list */
	if (bfcp_set_user_sockfd(server->list_conferences[i].user, userID, sockfd) != 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to set socket file descriptor in the user list!\n");
	}

	arguments = bfcp_new_arguments();
	if(!arguments) {
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}
	arguments->entity = bfcp_new_entity(conferenceID,TransactionID,userID);
	arguments->primitive = HelloAck;

	/* Create a list of all the primitives the FCS supports */
	arguments->primitives= bfcp_new_supported_list(FloorRequest,
							FloorRelease,
							FloorRequestQuery,
							FloorRequestStatus,
							UserQuery,
							UserStatus,
							FloorQuery,
							FloorStatus,
							ChairAction,
							ChairActionAck,
							Hello,
							HelloAck,
							Error, 0);
	/* Create a list of all the attributes the FCS supports */
	arguments->attributes= bfcp_new_supported_list(BENEFICIARY_ID,
							FLOOR_ID,
							FLOOR_REQUEST_ID,
							PRIORITY,
							REQUEST_STATUS,
							ERROR_CODE,
							ERROR_INFO,
							PARTICIPANT_PROVIDED_INFO,
							STATUS_INFO,
							SUPPORTED_ATTRIBUTES,
							SUPPORTED_PRIMITIVES,
							USER_DISPLAY_NAME,
							USER_URI,
							BENEFICIARY_INFORMATION,
							FLOOR_REQUEST_INFORMATION,
							REQUESTED_BY_INFORMATION,
							FLOOR_REQUEST_STATUS,
							OVERALL_REQUEST_STATUS,
							NONCE,
							DIGEST, 0);

	bfcp_mutex_unlock(&count_mutex);

	if (server->bfcp_transport == BFCP_OVER_UDP) {
		struct sockaddr_in *client_address = bfcp_get_user_address_server(server, conferenceID, userID);
		error = send_message_to_client_over_udp(arguments, sockfd, notify_to_server_app, client_address);
	} else {
		error = send_message_to_client(arguments, sockfd, notify_to_server_app, server->bfcp_transport);
	}

	BFCP_SEND_CHECK_ERRORS(sockfd);

	return 0;
}

/* Handle an incoming UserQuery message */
int bfcp_userquery_server(bfcp_server *server, unsigned long int conferenceID, unsigned short int userID, unsigned short int TransactionID, unsigned short int beneficiaryID, int sockfd, int y)
{
	if(server == NULL)
		return -1;
	if(conferenceID <= 0)
		return -1;
	if(userID <= 0)
		return -1;

	bfcp_arguments *arguments = NULL;
	bfcp_message *message = NULL;
	bfcp_user_information *beneficiary_info;
	int error, actual_conference = 0, i, j;
	bfcp_floor_request_information *frqInfo = NULL, *list_frqInfo = NULL;
	pnode traverse;

	bfcp_mutex_lock(&count_mutex);

	actual_conference = server->Actual_number_conference - 1;
	for(i = 0; i < actual_conference; i++) {
		if(server->list_conferences[i].conferenceID == conferenceID)
			break;
	}

	/* A buffer to compose error text messages when needed */
	char errortext[200];

	/* Check if this conference exists */
	if(server->list_conferences[i].conferenceID != conferenceID) {
		sprintf(errortext, "Conference %lu does not exist", conferenceID);
		bfcp_error_code(conferenceID, userID, TransactionID, BFCP_CONFERENCE_DOES_NOT_EXIST, errortext, NULL, sockfd, y, server->bfcp_transport);
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	/* Check if this user exists */
	if(bfcp_existence_user(server->list_conferences[i].user, userID) != 0) {
		sprintf(errortext, "User %hu does not exist in Conference %lu", userID, conferenceID);
		bfcp_error_code(conferenceID, userID, TransactionID, BFCP_USER_DOES_NOT_EXIST, errortext, NULL, sockfd, y, server->bfcp_transport);
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	if(beneficiaryID != 0) {
		/* The user requested information about another user, the Beneficiary: */
		/* 	check if this beneficiary exists in the conference */
		if(bfcp_existence_user(server->list_conferences[i].user, beneficiaryID) != 0) {
			sprintf(errortext, "User %hu (beneficiary of the query) does not exist in Conference %lu", beneficiaryID, conferenceID);
			bfcp_error_code(conferenceID, userID, TransactionID, BFCP_USER_DOES_NOT_EXIST, errortext, NULL, sockfd, y, server->bfcp_transport);
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}
	}

	/* Prepare an UserStatus message */
	arguments = bfcp_new_arguments();
	if(!arguments) {
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	arguments->entity = bfcp_new_entity(conferenceID,TransactionID,userID);
	arguments->primitive = UserStatus;

	/* Add the Beneficiary information, if needed */
	if(beneficiaryID != 0) {
		beneficiary_info = bfcp_show_user_information(server->list_conferences[i].user, beneficiaryID);
		if(beneficiary_info == NULL) {
			bfcp_free_user_information(beneficiary_info);
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}
		userID= beneficiaryID;
	} else
		beneficiary_info = NULL;

	arguments->beneficiary = beneficiary_info;

	/*if the queue is a granted queue*/
	if(server->list_conferences[i].granted != NULL) {
		traverse = server->list_conferences[i].granted->tail;
		while(traverse) {
			if((traverse->userID == userID) || (traverse->beneficiaryID == userID)) {
				frqInfo = create_floor_request_userID(traverse,server->list_conferences[i].user, userID, 3, 0);
				if((frqInfo != NULL) && (list_frqInfo != NULL))
					bfcp_add_floor_request_information_list(list_frqInfo, frqInfo, NULL);
				else if(list_frqInfo == NULL)
					list_frqInfo = frqInfo;
			}
			traverse = traverse->prev;
		}
	}

	/* Accepted queue */
	if(server->list_conferences[i].accepted != NULL) {
		traverse = server->list_conferences[i].accepted->tail;
		j = 1;
		while(traverse) {
			if((traverse->userID == userID) || (traverse->beneficiaryID == userID)) {
				frqInfo = create_floor_request_userID(traverse,server->list_conferences[i].user, userID, 2, j);
				if((frqInfo != NULL) && (list_frqInfo != NULL))
					bfcp_add_floor_request_information_list(list_frqInfo, frqInfo, NULL);
				else if(list_frqInfo == NULL)
					list_frqInfo = frqInfo;
			}
			j = j + 1;
			traverse = traverse->prev;
		}
	}

	/* Pending queue */
	if(server->list_conferences[i].pending != NULL) {
		traverse = server->list_conferences[i].pending->tail;
		while(traverse) {
			if((traverse->userID == userID) || (traverse->beneficiaryID == userID)) {
				frqInfo = create_floor_request_userID(traverse,server->list_conferences[i].user, userID, 1, 0);
				if((frqInfo != NULL) && (list_frqInfo != NULL))
					bfcp_add_floor_request_information_list(list_frqInfo, frqInfo, NULL);
				else if(list_frqInfo == NULL)
					list_frqInfo = frqInfo;
			}
			traverse = traverse->prev;
		}
	}

	arguments->frqInfo = list_frqInfo;

	bfcp_mutex_unlock(&count_mutex);

	if (server->bfcp_transport == BFCP_OVER_UDP) {
		struct sockaddr_in *client_address = bfcp_get_user_address_server(server, conferenceID, userID);
		error = send_message_to_client_over_udp(arguments, sockfd, notify_to_server_app, client_address);
	} else {
		error = send_message_to_client(arguments, sockfd, notify_to_server_app, server->bfcp_transport);
	}

	BFCP_SEND_CHECK_ERRORS(sockfd);

	return 0;
}

/* Handle an incoming FloorQuery message */
int bfcp_floorquery_server(bfcp_server *server, unsigned long int conferenceID, bfcp_floor *list_floors, unsigned short int userID, unsigned short int TransactionID, int sockfd, int y)
{
	if(server == NULL)
		return -1;
	if(server->list_conferences == NULL)
		return -1;
	if(conferenceID <= 0)
		return -1;
	if(TransactionID <= 0)
		return -1;

	int error, i, actual_conference, position;
	bfcp_floor *next_floors, *tempnode;
	floor_query *query;
	int position_floor;

	bfcp_mutex_lock(&count_mutex);

	actual_conference = server->Actual_number_conference - 1;
	for(i = 0; i < actual_conference; i++) {
		if(server->list_conferences[i].conferenceID == conferenceID)
			break;
	}

	/* A buffer to compose error text messages when needed */
	char errortext[200];

	/* Check if this conference exists */
	if(server->list_conferences[i].conferenceID == conferenceID) {
		/* Check if the user exists */
		if(bfcp_existence_user(server->list_conferences[i].user, userID) != 0) {
			sprintf(errortext, "User %hu does not exist in Conference %lu", userID, conferenceID);
			bfcp_error_code(conferenceID, userID, TransactionID, BFCP_USER_DOES_NOT_EXIST, errortext, NULL, sockfd, y, server->bfcp_transport);
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}

		if (server->bfcp_transport == BFCP_OVER_UDP) {
			/* Fetching address of client so we can send FloorStatus message and that address */
			struct sockaddr_in *client_addr = bfcp_get_user_address(server->list_conferences[i].user, userID);
			error = bfcp_floor_query_server_over_udp(server->list_conferences[i].floor,
					list_floors, userID, client_addr, sockfd);
		} else {
			error = bfcp_floor_query_server(server->list_conferences[i].floor, list_floors,
					userID, sockfd);
		}

		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}
	} else {
		sprintf(errortext, "Conference %lu does not exist", conferenceID);
		bfcp_error_code(conferenceID, userID, TransactionID, BFCP_CONFERENCE_DOES_NOT_EXIST, errortext, NULL, sockfd, y, server->bfcp_transport);
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	/* Check if all the floors exist in the conference, otherwise send an error */
	if(list_floors != NULL) {
		for(tempnode = list_floors; tempnode; tempnode = tempnode->next) {
			position_floor = bfcp_return_position_floor(server->list_conferences[i].floor, tempnode->floorID);
			if(position_floor == -1) {
				sprintf(errortext, "Floor %hu does not exist in Conference %lu", tempnode->floorID, conferenceID);
				bfcp_error_code(conferenceID, userID, TransactionID, BFCP_INVALID_FLOORID, errortext, NULL, sockfd, y, server->bfcp_transport);
				bfcp_mutex_unlock(&count_mutex);
			return -1;
			}
		}
	}

	if(list_floors == NULL) {
		/* Remove the user from the list of FloorQueries */
		error = bfcp_remove_floorquery_from_all_nodes(server->list_conferences[i].floor, userID);
		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}
		error = bfcp_show_floor_information(conferenceID, TransactionID, userID, server->list_conferences+i, 0, sockfd, allset, client, NULL, 0);
		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}
	}

	while(list_floors) {
		position = bfcp_return_position_floor(server->list_conferences[i].floor, list_floors->floorID);
		if(position >= 0) {
			if(server->list_conferences[i].floor->floors != NULL) {
				query = server->list_conferences[i].floor->floors[position].floorquery;
				while(query != NULL) {
					if(query->userID == userID) {
						error = bfcp_show_floor_information(conferenceID, TransactionID, query->userID, server->list_conferences+i, list_floors->floorID, query->fd, allset, client, NULL, 0);
						if(error == -1) {
							bfcp_mutex_unlock(&count_mutex);
							return -1;
						}
					}
					query = query->next;
				}
			}
		}

		/* Remove the floor from the list */
		next_floors = list_floors->next;
		free(list_floors->chair_info);
		list_floors->chair_info = NULL;
		free(list_floors);
		list_floors = NULL;
		list_floors = next_floors;
	}

	bfcp_mutex_unlock(&count_mutex);

	return 0;
}

/* Handle an incoming FloorQuery message for UDP and also set client
   address in query list corrosponding to userID */
int bfcp_floor_query_server_over_udp(bfcp_list_floors *lfloors,
				      bfcp_floor *list_floors,
					  unsigned short int userID,
					  struct sockaddr_in *client_addr,
					  int sockfd)
{
	if (lfloors == NULL)
		return 0;
	if (userID <= 0)
		return -1;

	floor_query *query = NULL;
	floor_query *query_temp = NULL;
	floor_query *newnode = NULL;
	int i = 0, exist_user;

	i = lfloors->actual_number_floors-1;
	while (i >= 0) {
		if ((list_floors == NULL) || (lfloors->floors[i].floorID != list_floors->floorID)) {
			query = lfloors->floors[i].floorquery;
			if (query) {
				if (query->userID == userID) {
					lfloors->floors[i].floorquery = query->next;
					query->next = NULL;
					free(query);
					query = NULL;
				} else {
					while (query->next) {
						if (query->next->userID == userID) {
							/* Remove the query */
							query_temp= query->next;
							query->next = query_temp->next;
							query_temp->next = NULL;
							free(query_temp);
							query_temp = NULL;
							break;
						} else {
							query = query->next;
						}
					}
				}
			}

			if (list_floors == NULL) {
				i--;
			} else {
				if (lfloors->floors[i].floorID > list_floors->floorID) {
					i--;
				} else {
					list_floors = list_floors->next;
				}
			}
		} else {
			query = lfloors->floors[i].floorquery;
			/* If the query already exists, don't add it again */
			exist_user = 0;

			while (query) {
				if (query->userID == userID) {
					exist_user = 1;
					break;
				}
				query = query->next;
			}

			if (exist_user == 0) {
				/* Allocate a new node */
				newnode = (floor_query *)calloc(1, sizeof(floor_query));

				if (newnode == NULL) {
					return -1;
				}

				query = lfloors->floors[i].floorquery;
				/* Add the new query to the list of requests */
				newnode->userID = userID;
				newnode->fd = sockfd;
				newnode->client_addr = client_addr;
				newnode->next = query;
				lfloors->floors[i].floorquery = newnode;
			}

			i--;
			list_floors = list_floors->next;
		}
	}

	return 0;
}

/* (??) */
int bfcp_floor_query_server(bfcp_list_floors *lfloors, bfcp_floor *list_floors, unsigned short int userID, int sockfd)
{
	if(lfloors == NULL)
		return 0;
	if(userID <= 0)
		return -1;

	floor_query *query = NULL;
	floor_query *query_temp = NULL;
	floor_query *newnode = NULL;
	int i = 0, exist_user;

	i = lfloors->actual_number_floors-1;
	while(0 <= i) {
		if((list_floors == NULL) || (lfloors->floors[i].floorID != list_floors->floorID)) {
			query = lfloors->floors[i].floorquery;
			if(query != NULL) {
				if(query->userID == userID) {
					lfloors->floors[i].floorquery = query->next;
					query->next = NULL;
					free(query);
					query = NULL;
				} else {
					while(query->next) {
						if(query->next->userID == userID) {
							/* Remove the query */
							query_temp= query->next;
							query->next = query_temp->next;
							query_temp->next = NULL;
							free(query_temp);
							query_temp = NULL;
							break;
						} else
							query = query->next;
					}
				}
			}

			if(list_floors == NULL)
				i = i - 1;
			else {
				if(lfloors->floors[i].floorID > list_floors->floorID)
					i = i - 1;
				else
					list_floors = list_floors->next;
			}
		} else {
			query = lfloors->floors[i].floorquery;
			/* If the query already exists, don't add it again */
			exist_user = 0;
			while(query) {
				if(query->userID == userID) {
					exist_user = 1;
					break;
				}
				query = query->next;
			}
			if(exist_user == 0) {
				/* Allocate a new node */
				newnode = (floor_query *)calloc(1, sizeof(floor_query));
				if(newnode == NULL)
					return -1;

				query = lfloors->floors[i].floorquery;
				/* Add the new query to the list of requests */
				newnode->userID = userID;
				newnode->fd = sockfd;
				newnode->next = query;
				lfloors->floors[i].floorquery = newnode;
			}

			i = i - 1;
			list_floors = list_floors->next;
		}
	}

	return 0;
}

/* Handle an incoming FloorRequestQuery message */
int bfcp_floorrequestquery_server(bfcp_server *server, unsigned long int conferenceID, unsigned short int TransactionID, unsigned long int floorRequestID, unsigned short int userID, int sockfd, int y)
{
	if(server == NULL)
		return -1;
	if(conferenceID <= 0)
		return -1;
	if(userID <= 0)
		return -1;

	bfcp_arguments *arguments = NULL;
	bfcp_message *message = NULL;
	bfcp_floor_request_information *frqInfo, *list_frqInfo = NULL;
	int actual_conference, i, error;

	actual_conference = server->Actual_number_conference - 1;
	for(i = 0; i < actual_conference; i++) {
		if(server->list_conferences[i].conferenceID == conferenceID)
			break;
	}

	/* A buffer to compose error text messages when needed */
	char errortext[200];

	/* Check if this conference exists */
	if(server->list_conferences[i].conferenceID != conferenceID) {
		sprintf(errortext, "Conference %lu does not exist", conferenceID);
		bfcp_error_code(conferenceID, userID, TransactionID, BFCP_CONFERENCE_DOES_NOT_EXIST, errortext, NULL, sockfd, y, server->bfcp_transport);
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	/* Check if this user exists */
	if(bfcp_existence_user(server->list_conferences[i].user, userID) != 0) {
		sprintf(errortext, "User %hu does not exist in Conference %lu", userID, conferenceID);
		bfcp_error_code(conferenceID, userID, TransactionID, BFCP_USER_DOES_NOT_EXIST, errortext, NULL, sockfd, y, server->bfcp_transport);
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}

	/* Check if the request is in the Pending list */
	if(bfcp_floor_request_query_server(server->list_conferences[i].pending, floorRequestID, userID, sockfd) != 0) {
		/* Check if the request is in the Accepted list */
		if(bfcp_floor_request_query_server(server->list_conferences[i].accepted, floorRequestID, userID, sockfd) != 0) {
			/* Check if the request is in the Granted list */
			if(bfcp_floor_request_query_server(server->list_conferences[i].granted, floorRequestID, userID, sockfd) != 0) {
				sprintf(errortext, "FloorRequest %lu does not exist in Conference %lu", floorRequestID, conferenceID);
				bfcp_error_code(conferenceID, userID, TransactionID, BFCP_FLOORREQUEST_DOES_NOT_EXIST, errortext, NULL, sockfd, y, server->bfcp_transport);
				return -1;
			}
		}
	}

	/* Prepare the FloorRequestStatus message */
	arguments = bfcp_new_arguments();
	if(!arguments) {
		bfcp_mutex_unlock(&count_mutex);
		return -1;
	}
	arguments->entity = bfcp_new_entity(conferenceID,TransactionID,userID);
	arguments->primitive = FloorRequestStatus;

	/* Granted list*/
	frqInfo = bfcp_show_floorrequest_information(server->list_conferences[i].granted, server->list_conferences[i].user, floorRequestID, 0);
	if(frqInfo != NULL) {
		if(list_frqInfo != NULL)
			bfcp_add_floor_request_information_list(list_frqInfo, frqInfo, NULL);
		else
			list_frqInfo = frqInfo;
	}

	/* Accepted list*/
	frqInfo = bfcp_show_floorrequest_information(server->list_conferences[i].accepted, server->list_conferences[i].user, floorRequestID, 1);
	if(frqInfo != NULL) {
		if(list_frqInfo != NULL)
			bfcp_add_floor_request_information_list(list_frqInfo, frqInfo, NULL);
		else
			list_frqInfo = frqInfo;
	}

	/* Pending list*/
	frqInfo = bfcp_show_floorrequest_information(server->list_conferences[i].pending, server->list_conferences[i].user, floorRequestID, 2);
	if(frqInfo != NULL) {
		if(list_frqInfo != NULL)
			bfcp_add_floor_request_information_list(list_frqInfo, frqInfo, NULL);
		else
			list_frqInfo = frqInfo;
	}

	arguments->frqInfo = list_frqInfo;

	if (server->bfcp_transport == BFCP_OVER_UDP) {
		struct sockaddr_in *client_address = bfcp_get_user_address_server(server, conferenceID, userID);
		error = send_message_to_client_over_udp(arguments, sockfd, notify_to_server_app, client_address);
	} else {
		error = send_message_to_client(arguments, sockfd, notify_to_server_app, server->bfcp_transport);
	}

	BFCP_SEND_CHECK_ERRORS(sockfd);

	return 0;
}

/* Check if it's fine to grant a floor to an accepted request */
int check_accepted_node(bfcp_conference *conference, pnode queue_accepted, unsigned short int floorID, char *chair_info)
{
	pfloor revoke_floor;
	pnode newnode;
	int error;

	revoke_floor = queue_accepted->floor;
	while(revoke_floor) {
		if((floorID == revoke_floor->floorID) && ((revoke_floor->status == BFCP_FLOOR_STATE_WAITING) || (revoke_floor->status == BFCP_FLOOR_STATE_ACCEPTED))) {
			/* Change state to the floor again */
			bfcp_change_state_floor(conference->floor, revoke_floor->floorID, 0);
			/* Set it as Granted */
			revoke_floor->status = BFCP_FLOOR_STATE_GRANTED;
			revoke_floor = revoke_floor->next;
			break;
		} else if((floorID < revoke_floor->floorID) && (revoke_floor->status == BFCP_FLOOR_STATE_GRANTED))
			revoke_floor = revoke_floor->next;
		else
			break;
	}

	if((revoke_floor == NULL) && (bfcp_all_floor_status(conference->accepted, queue_accepted->floorRequestID, 2) == 0)) {
		/* Extract the request node from the Accepted list */
		newnode = bfcp_extract_request(conference->accepted, queue_accepted->floorRequestID);
		if(newnode == NULL) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}

		/* Now prepare to move the node to the Granted list */

		/* Change the priority of the request node to the lowest one */
		newnode->priority = BFCP_LOWEST_PRIORITY;
		/* Change the queue_position of the request node to the lowest one */
		newnode->queue_position = 0;
		/* Move the node to the Granted list */
		error = bfcp_insert_request(conference->granted, newnode, newnode->floorRequestID, chair_info);
		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}

		/* Prepare all floor information needed by interested users */
		error = bfcp_print_information_floor(conference, 0, 0, newnode, BFCP_GRANTED);
		if(error == -1) {
			bfcp_mutex_unlock(&count_mutex);
			return -1;
		}

		return 0;
	} else
		return 1;
}

/* Check if it's fine to grant a floor to an accepted request (??) */
int give_free_floors_to_the_accepted_nodes(bfcp_conference *conference, bfcp_queue *laccepted, bfcp_list_floors *lfloors, char *chair_info)
{
	if(lfloors == NULL)
		return 0;
	if(lfloors->floors == NULL)
		return 0;
	if(laccepted == NULL)
		return 0;

	int i;
	pnode queue_accepted;

	for (i = (lfloors->actual_number_floors - 1); 0 <= i; i--) {
		if(bfcp_return_state_floor(lfloors, lfloors->floors[i].floorID) < bfcp_return_number_granted_floor(lfloors, lfloors->floors[i].floorID)) {
			queue_accepted = laccepted->tail;
			while(queue_accepted) {
				if(check_accepted_node(conference, queue_accepted, lfloors->floors[i].floorID, chair_info) == 1)
					queue_accepted = queue_accepted->prev;
			}
		}
	}

	return 0;
}

/* Setup and send an Error reply to a participant */
int bfcp_error_code(unsigned long int conferenceID, unsigned short int userID, unsigned short int TransactionID, int code, char *error_info, bfcp_unknown_m_error_details *details, int sockfd, int i, int transport)
{
	int error, dLen;
	bfcp_arguments *arguments = NULL;
	bfcp_message *message = NULL;

	arguments = bfcp_new_arguments();
	if(!arguments)
		return -1;

	arguments->entity = bfcp_new_entity(conferenceID, TransactionID, userID);
	arguments->primitive = Error;

	arguments->error = bfcp_new_error(code, NULL);
	if(!arguments->error) {
		/* An error occurred when creating a new Error Code */
		bfcp_free_error(arguments->error);
		return -1;
	}

	if(error_info != NULL) {	/* If there's error text, add it */
		dLen = strlen(error_info);
		if(dLen != 0) {
			arguments->eInfo = (char *)calloc(1, dLen*sizeof(char)+1);
			memcpy(arguments->eInfo, error_info, dLen+1);
		} else
			arguments->eInfo = NULL;
	} else
		arguments->eInfo = NULL;

	if (transport == BFCP_OVER_UDP) {
		error = send_message_to_client_over_udp(arguments, sockfd, notify_to_server_app, NULL);
	} else {
		error = send_message_to_client(arguments, sockfd, notify_to_server_app, transport);
	}

	BFCP_SEND_CHECK_ERRORS(sockfd);

	return 0;
}

/* Send a BFCP message to a participant */
int send_message_to_client(bfcp_arguments *arguments, int sockfd, int(*notify_to_server_app)(bfcp_arguments *arguments, int outgoing_msg), int transport)
{
	if(arguments == NULL)
		return -1;

	int error = 0;
	int total = 0;		/* How many bytes have been sent */
	int bytesleft = 0;	/* How many bytes still have to be sent */
	struct timeval tv;

	bfcp_message *message = bfcp_build_message(arguments);
	if(!message) {
		if(arguments != NULL)
			bfcp_free_arguments(arguments);
		return -1;
	}

	callback_func(arguments, 1); 	/* Notify OUTGOING (1) message */

	bytesleft = message->length;

	/* Wait up to five seconds. */
	tv.tv_sec = 5;
	tv.tv_usec = 0;

#ifdef WIN32
	FD_ZERO(&wset);
	FD_SET(sockfd, &wset);
#endif
	/* Look for the TLS session associated with this file descriptor */
	int i;
	for(i = 0; i < BFCP_MAX_CONNECTIONS; i++) {
		if(client[i] == sockfd)
			break;
	}
	if(i == BFCP_MAX_CONNECTIONS)
		return -1;

	while(total < (message->length)) {
#ifdef WIN32
		error = select(sockfd+1, NULL, &wset, NULL, &tv);
		if(error == -1)
			return -1;	/* Error in the select */
		if(error == 0)
			return 0;	/* Timeout in the select */
		if(FD_ISSET(sockfd, &wset)) {
#endif
			if(transport == BFCP_OVER_TLS) {
				if(session[i])
					error = SSL_write(session[i], message->buffer + total, bytesleft);
			} else	/* BFCP_OVER_TCP */
				error = send(sockfd, message->buffer + total, bytesleft, 0);
			if(error == -1)	/* Error sending the message */
				break;
			total += error;
			bytesleft -= error;
#ifdef WIN32
		}
#endif
	}

	return error;
}

/* Send a BFCP message to a participant (UDP) */
int send_message_to_client_over_udp(bfcp_arguments *arguments,
                                    int sockfd,
                                    int(*notify_to_server_app)(bfcp_arguments *arguments, int outgoing_msg),
                                    struct sockaddr_in *client_address)
{
	if (arguments == NULL) {
		return -1;
	}

	int bytes_send = 0;
	int total = 0;		/* How many bytes have been sent */
	int bytesleft = 0;	/* How many bytes still have to be sent */

	bfcp_message *message = bfcp_build_message(arguments);

	if (!message) {
		if (arguments) {
			bfcp_free_arguments(arguments);
		}

		return -1;
	}

	callback_func(arguments, 1); 	/* Notify OUTGOING (1) message */

	bytesleft = message->length;

	if (!client_address) {
		client_address = &cliaddr;
	}

	while (total < (message->length)) {
		bytes_send = sendto(sockfd, message->buffer + total, bytesleft, 0,
			       (struct sockaddr *) client_address, sizeof(struct sockaddr_in));

		if (bytes_send == -1) /* Error sending the message */ {
			break;
		}

		total += bytes_send;
		bytesleft -= bytes_send;
	}

	return bytes_send;
}

/* Thread handling incoming connections and messages */
static void *recv_thread(void *st_server)
{
	int error = 0, nready, i = 0, sockfd, client_sock;
	bfcp_message *message = NULL;
	bfcp_received_message *recvM = NULL;
	bfcp_server *server = (bfcp_server *)st_server;
	unsigned int addrlen;
	struct sockaddr_in client_addr;
	pfloor list_floor;
	bfcp_floor_id_list *parse_floor;
	bfcp_floor_request_status *parse_floor_id;
	pnode node;
	bfcp_received_message_error *errors;
	bfcp_unknown_m_error_details *error_detail_list;

	/* A buffer to compose error text messages when needed */
	char errortext[200];

	if (server->bfcp_transport != BFCP_OVER_UDP) {

#ifdef WIN32
	FD_ZERO(&allset);
	FD_SET(server_sock_tcp, &allset); /* For TCP socket (server_sock_tcp) */
#else
	struct pollfd tmppollfds[BFCP_MAX_CONNECTIONS+1];
	int tmpfds_no = 0;
#endif

	while(1) {
#ifdef WIN32
		rset = allset;

		nready = select(maxfd+1, &rset, NULL, NULL, NULL);

		if(nready < 0) {
			close(server_sock_tcp);
			pthread_exit(0);
		}
		if(FD_ISSET(server_sock_tcp,&rset)) { /* For TCP socket (server_sock_tcp) */
#else
		memcpy(&tmppollfds, &pollfds, sizeof(struct pollfd)*(BFCP_MAX_CONNECTIONS+1));
		tmpfds_no = fds_no;
		while((nready = poll(tmppollfds, tmpfds_no, -1) < 0) && (errno == EINTR));
		if(nready < 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error polling, closing connection\n");
			switch(errno) {
				case EAGAIN:
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "\tEAGAIN\n");
					break;
				case EFAULT:
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "\tEFAULT\n");
					break;
				case EINTR:
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "\tEINTR\n");
					break;
				case EINVAL:
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "\tEINVAL\n");
					break;
				default:
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "\tboh\n");
					break;
			}
			close(server_sock_tcp); /* For TCP socket (server_sock_tcp) */
			pthread_exit(0);
		}
		if(tmppollfds[0].revents == POLLIN) {
#endif
			addrlen = sizeof(client_addr);
			client_sock = accept(server_sock_tcp, (struct sockaddr *)&client_addr, &addrlen); /* For TCP socket (server_sock_tcp) */

			/* Check if there is any IP_based restriction */
			unsigned short int ip[4];
			sscanf(inet_ntoa(client_addr.sin_addr), "%hu.%hu.%hu.%hu", &ip[0], &ip[1], &ip[2], &ip[3]);
			int ok = 0;
			if((server->restricted[0] == 0) || (ip[0] == server->restricted[0]))
				ok++;
			if((server->restricted[1] == 0) || (ip[1] == server->restricted[1]))
				ok++;
			if((server->restricted[2] == 0) || (ip[2] == server->restricted[2]))
				ok++;
			if((server->restricted[3] == 0) || (ip[3] == server->restricted[3]))
				ok++;
			if(ok < 4) {	/* Didn't pass restriction test */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Client didn't pass restriction\n");
#ifndef WIN32
				shutdown(client_sock, SHUT_RDWR);
#else
				shutdown(client_sock, SD_BOTH);
#endif
				close(client_sock);
				continue;
			}

			for(i = 0; i < BFCP_MAX_CONNECTIONS; i++) {
				if(client[i] < 0) {
					client[i] = client_sock;
					if(server->bfcp_transport == BFCP_OVER_TLS) {
						/* Setup a TLS session for this new connection:
							TODO: better management of TLS-related errors
						if(session[i]) {
							SSL_free(session[i]);
							session[i] = NULL;
						}	*/
						session[i] = SSL_new(context);
						if(!session[i])
							break;
						if(SSL_set_fd(session[i], client_sock) < 1)
							break;
						if(SSL_accept(session[i]) < 1)
							break;
					}
					break;
				}
			}

			if(i == BFCP_MAX_CONNECTIONS) {
				/* Too many clients connected */
#ifndef WIN32
				shutdown(client_sock, SHUT_RDWR);
#else
				shutdown(client_sock, SD_BOTH);
#endif
				close(client_sock);
				continue;
			}

#ifdef WIN32
			FD_SET(client_sock, &allset);
#else
// 			pollfds[i+1].fd = client[i];
// 			pollfds[i+1].events = POLLIN;
			pollfds[fds_no].fd = client[i];
			pollfds[fds_no].events = POLLIN;
			fds_no++;
#endif

			if(client_sock > maxfd)
				maxfd = client_sock;

			if(i > maxi)
				maxi = i;

			if(--nready <= 0)
				continue;

#ifndef WIN32
			error = fcntl(client_sock, O_NONBLOCK, 1);
#endif
		}

#ifdef WIN32
		for(i = 0; i <= maxi; i++) {
			if((sockfd = client[i]) < 0)
				continue;
#else
		int j=0;
		for(j = 1; j < tmpfds_no; j++) {
#endif
#ifdef WIN32
			if(FD_ISSET(sockfd, &rset)) {
#else
			if(tmppollfds[j].revents == POLLIN) {
				for(i = 0; i <= maxi; i++) {
					if(client[i] == tmppollfds[j].fd)
						break;
				}
				if(i > maxi)
					continue;
				if((sockfd = client[i]) < 0)
					continue;
#endif
				message = received_message_from_client(sockfd, i, server->bfcp_transport);
				if(message != NULL) {
					recvM = bfcp_parse_message(message);
					if(recvM != NULL) {
						/* Notify the application about the incoming message through the callback */
						callback_func(recvM->arguments, 0);
						if(recvM->errors != NULL) {
							errors = recvM->errors;
							switch(errors->code) {
								case BFCP_UNKNOWN_PRIMITIVE:
									sprintf(errortext, "Unknown primitive %i", recvM->primitive);
									bfcp_error_code(recvM->entity->conferenceID, recvM->entity->userID, recvM->entity->transactionID, BFCP_UNKNOWN_PRIMITIVE, errortext, NULL, sockfd, i, server->bfcp_transport);
									break;
								case BFCP_UNKNOWN_MANDATORY_ATTRIBUTE:
									error_detail_list = bfcp_new_unknown_m_error_details_list(errors->attribute, 0);
									errors = errors->next;
									while(errors) {
										if(errors->code == BFCP_UNKNOWN_MANDATORY_ATTRIBUTE)
											error = bfcp_add_unknown_m_error_details_list(error_detail_list, errors->attribute, 0);
										if(error == -1)
											break;
										errors = errors->next;
									}
									if(error != -1)
										bfcp_error_code(recvM->entity->conferenceID, recvM->entity->userID, recvM->entity->transactionID, BFCP_UNKNOWN_MANDATORY_ATTRIBUTE, "Unknown Mandatory Attributes in the header", error_detail_list, sockfd, i, server->bfcp_transport);
									break;
								default:
									break;
							}
						} else {
							/* Check the primitive in the message and do what's needed */
							switch(recvM->primitive) {
								case Hello:
									if(recvM->entity != NULL)
										bfcp_hello_server(server, recvM->entity->conferenceID, recvM->entity->userID, recvM->entity->transactionID, sockfd, i);
									break;
								case ChairAction:
									if(recvM->entity != NULL) {
										if(recvM->arguments != NULL) {
											if(recvM->arguments->frqInfo != NULL) {
												if(recvM->arguments->frqInfo->oRS != NULL) {
													if(recvM->arguments->frqInfo->fRS != NULL) {
														parse_floor_id=recvM->arguments->frqInfo->fRS;
														if(parse_floor_id != NULL) {
															list_floor = create_floor_list(parse_floor_id->fID, 0, parse_floor_id->sInfo);
															parse_floor_id = parse_floor_id->next;
															while(parse_floor_id != NULL) {
																list_floor = add_floor_list(list_floor, parse_floor_id->fID, 0, parse_floor_id->sInfo);
																parse_floor_id = parse_floor_id->next;
															}
															error = bfcp_ChairAction_server(server, recvM->entity->conferenceID, list_floor, recvM->entity->userID, recvM->arguments->frqInfo->frqID, recvM->arguments->frqInfo->oRS->rs->rs, recvM->arguments->frqInfo->oRS->sInfo, 0,recvM->entity->transactionID, sockfd, i);
															if(error == -1) {
																remove_floor_list(list_floor);
																break;
															}
														}
													}
												}
											}
										}
									}
									break;
								case FloorQuery:
									if(recvM->entity != NULL) {
										if(recvM->arguments != NULL) {
											if(recvM->arguments->fID != NULL) {
												parse_floor =recvM->arguments->fID;
												if(parse_floor != NULL) {
													list_floor = create_floor_list(parse_floor->ID, 0, NULL);
													parse_floor = parse_floor->next;
													while(parse_floor != NULL) {
														list_floor = add_floor_list(list_floor, parse_floor->ID, 0, NULL);
														parse_floor = parse_floor->next;
													}
												}
											}
											else
												list_floor = NULL;
											error = bfcp_floorquery_server(server, recvM->entity->conferenceID, list_floor, recvM->entity->userID, recvM->entity->transactionID, sockfd, i);
											if(error == -1)
												remove_floor_list(list_floor);
										}
									}
									break;
								case UserQuery:
									if(recvM->entity != NULL) {
										if(recvM->arguments != NULL)
											error = bfcp_userquery_server(server, recvM->entity->conferenceID, recvM->entity->userID, recvM->entity->transactionID, recvM->arguments->bID, sockfd, i);
									}
									break;
								case FloorRequestQuery:
									if(recvM->entity != NULL) {
										if(recvM->arguments != NULL) {
											if(recvM->arguments->frqID != 0)
												error = bfcp_floorrequestquery_server(server, recvM->entity->conferenceID, recvM->entity->transactionID, recvM->arguments->frqID, recvM->entity->userID, sockfd, i);
										}
									}
									break;
								case FloorRelease:
									if(recvM->entity != NULL) {
										if(recvM->arguments != NULL) {
											error = bfcp_FloorRelease_server(server, recvM->entity->conferenceID, recvM->entity->transactionID, recvM->entity->userID, recvM->arguments->frqID, sockfd, i);
										}
									}
									break;
								case FloorRequest:
									if(recvM->entity != NULL) {
										if(recvM->arguments != NULL) {
											if(recvM->arguments->fID != NULL) {
												parse_floor = recvM->arguments->fID;
												if(parse_floor != NULL) {
													node = bfcp_init_request(recvM->entity->userID, recvM->arguments->bID, recvM->arguments->priority, recvM->arguments->pInfo, parse_floor->ID);
													parse_floor = parse_floor->next;
													while(parse_floor != NULL) {
														error = bfcp_add_floor_to_request(node, parse_floor->ID);
														if(error != 0)
															break;
														parse_floor = parse_floor->next;
													}
													error = bfcp_FloorRequest_server(server, recvM->entity->conferenceID, recvM->entity->transactionID, node, sockfd, i);
												}
											}
										}
									}
									break;
								case FloorRequestStatusAck:
								case ErrorAck:
								case FloorStatusAck:
									break;
								default:
									sprintf(errortext, "Unknown primitive %i", recvM->primitive);
									bfcp_error_code(recvM->entity->conferenceID, recvM->entity->userID, recvM->entity->transactionID, BFCP_UNKNOWN_PRIMITIVE, errortext, NULL, sockfd, i, server->bfcp_transport);
									break;
							}
						}
					}
					if(message != NULL)
						bfcp_free_message(message);
					if(recvM != NULL)
						bfcp_free_received_message(recvM);
				}
				if(--nready <= 0)
					break;
			}
		}
	}
	} else { /* recv_thread() part for server of UDP transport socket */
#ifdef WIN32
	fd_set rset;
	FD_ZERO(&rset);
#endif
		while (1) {
#ifdef WIN32
			FD_SET(server_sock_udp, &rset);
			nready = select(server_sock_udp+1, &rset, NULL, NULL, NULL);
#else
			while((nready = poll(poll_fd_udp, 1, -1) < 0) && (errno == EINTR));
#endif
			if (nready < 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error polling, closing connection\n");
				switch(errno) {
					case EAGAIN:
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "\tEAGAIN\n");
						break;
					case EFAULT:
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "\tEFAULT\n");
						break;
					case EINTR:
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "\tEINTR\n");
						break;
					case EINVAL:
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "\tEINVAL\n");
						break;
					default:
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "\tboh\n");
						break;
				}
				close(server_sock_udp);
				pthread_exit(0);
			}

			if (close_udp_conn) {
				close(server_sock_udp);
				pthread_exit(0);
			}

#ifdef WIN32
			if (FD_ISSET(server_sock_udp, &rset)) {
#else
			if (poll_fd_udp[0].revents == POLLIN) {
#endif
				message = received_message_from_client_over_udp(server_sock_udp);

				if(message) {
					recvM = bfcp_parse_message(message);

					if (recvM) {
						/* Notify the application about the incoming message (0) through the callback */
						callback_func(recvM->arguments, 0);

						if (recvM->errors) {
							errors = recvM->errors;

							switch (errors->code) {
								case BFCP_UNKNOWN_PRIMITIVE:
									sprintf(errortext, "Unknown primitive %i", recvM->primitive);
									bfcp_error_code(recvM->entity->conferenceID, recvM->entity->userID, recvM->entity->transactionID, BFCP_UNKNOWN_PRIMITIVE, errortext, NULL, server_sock_udp, i, server->bfcp_transport);
									break;
								case BFCP_UNKNOWN_MANDATORY_ATTRIBUTE:
									error_detail_list = bfcp_new_unknown_m_error_details_list(errors->attribute, 0);
									errors = errors->next;

									while (errors) {
										if (errors->code == BFCP_UNKNOWN_MANDATORY_ATTRIBUTE) {
											error = bfcp_add_unknown_m_error_details_list(error_detail_list, errors->attribute, 0);
										}
										if (error == -1) {
											break;
										}

										errors = errors->next;
									}
									if (error != -1) {
										bfcp_error_code(recvM->entity->conferenceID, recvM->entity->userID, recvM->entity->transactionID, BFCP_UNKNOWN_MANDATORY_ATTRIBUTE, "Unknown Mandatory Attributes in the header", error_detail_list, server_sock_udp, i, server->bfcp_transport);
									}
									break;
								default:
									break;
							}
						} else {
							/* Check the primitive in the message and do what's needed */
							switch (recvM->primitive) {
								case Hello:
									if (recvM->entity) {
										bfcp_hello_server(server, recvM->entity->conferenceID, recvM->entity->userID, recvM->entity->transactionID, server_sock_udp, i);
									}
									break;
								case ChairAction:
									if ((recvM->entity) &&
									   (recvM->arguments) &&
									   (recvM->arguments->frqInfo) &&
									   (recvM->arguments->frqInfo->oRS) &&
									   (recvM->arguments->frqInfo->fRS) &&
									   (parse_floor_id = recvM->arguments->frqInfo->fRS)) {
										list_floor = create_floor_list(parse_floor_id->fID, 0, parse_floor_id->sInfo);
										parse_floor_id = parse_floor_id->next;

										while (parse_floor_id) {
											list_floor = add_floor_list(list_floor, parse_floor_id->fID, 0, parse_floor_id->sInfo);
											parse_floor_id = parse_floor_id->next;
										}

										error = bfcp_ChairAction_server(server, recvM->entity->conferenceID, list_floor, recvM->entity->userID, recvM->arguments->frqInfo->frqID, recvM->arguments->frqInfo->oRS->rs->rs, recvM->arguments->frqInfo->oRS->sInfo, 0,recvM->entity->transactionID, server_sock_udp, i);

										if (error == -1) {
											remove_floor_list(list_floor);
											break;
										}
									}
									break;
								case FloorQuery:
									if ((recvM->entity) &&
									   (recvM->arguments)) {
										if (recvM->arguments->fID) {
											parse_floor =recvM->arguments->fID;
											if (parse_floor) {
												list_floor = create_floor_list(parse_floor->ID, 0, NULL);
												parse_floor = parse_floor->next;

												while (parse_floor) {
													list_floor = add_floor_list(list_floor, parse_floor->ID, 0, NULL);
													parse_floor = parse_floor->next;
												}
											}
										} else {
											list_floor = NULL;
										}
										error = bfcp_floorquery_server(server, recvM->entity->conferenceID, list_floor, recvM->entity->userID, recvM->entity->transactionID, server_sock_udp, i);

										if (error == -1) {
											remove_floor_list(list_floor);
										}
									}
									break;
								case UserQuery:
									if ((recvM->entity) &&
									   (recvM->arguments)) {
										error = bfcp_userquery_server(server, recvM->entity->conferenceID, recvM->entity->userID, recvM->entity->transactionID, recvM->arguments->bID, server_sock_udp, i);
									}
									break;
								case FloorRequestQuery:
									if ((recvM->entity) &&
									   (recvM->arguments) &&
									   (recvM->arguments->frqID != 0)) {
										error = bfcp_floorrequestquery_server(server, recvM->entity->conferenceID, recvM->entity->transactionID, recvM->arguments->frqID, recvM->entity->userID, server_sock_udp, i);
									}
									break;
								case FloorRelease:
									if ((recvM->entity) &&
									   (recvM->arguments)) {
										error = bfcp_FloorRelease_server(server, recvM->entity->conferenceID, recvM->entity->transactionID, recvM->entity->userID, recvM->arguments->frqID, server_sock_udp, i);
									}
									break;
								case FloorRequest:
									if ((recvM->entity) &&
									   (recvM->arguments) &&
									   (recvM->arguments->fID) &&
									   (parse_floor = recvM->arguments->fID)) {
										node = bfcp_init_request(recvM->entity->userID, recvM->arguments->bID, recvM->arguments->priority, recvM->arguments->pInfo, parse_floor->ID);
										parse_floor = parse_floor->next;

										while (parse_floor) {
											error = bfcp_add_floor_to_request(node, parse_floor->ID);

											if (error) {
												break;
											}

											parse_floor = parse_floor->next;
										}

										error = bfcp_FloorRequest_server(server, recvM->entity->conferenceID, recvM->entity->transactionID, node, server_sock_udp, i);
									}
									break;
								case FloorRequestStatusAck:
								case ErrorAck:
								case FloorStatusAck:
									break;
								default:
									sprintf(errortext, "Unknown primitive %i", recvM->primitive);
									bfcp_error_code(recvM->entity->conferenceID, recvM->entity->userID, recvM->entity->transactionID, BFCP_UNKNOWN_PRIMITIVE, errortext, NULL, server_sock_udp, i, server->bfcp_transport);
									break;
							}
						}
					}
					if (message) {
						bfcp_free_message(message);
					}
					if (recvM) {
						bfcp_free_received_message(recvM);
					}
				}
#ifdef WIN32
			}
#else
			} else if((poll_fd_udp[0].revents & POLLERR) ||
                                (poll_fd_udp[0].revents & POLLHUP) ||
                                (poll_fd_udp[0].revents & POLLNVAL)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "We got an error polling!!\n");
				close(server_sock_udp);
				pthread_exit(0);

			}
#endif
		}
	}

}
