#include "bfcp_participant.h"

/* Macro to check for errors after sending a BFCP message */
#define BFCP_SEND_CHECK_ERRORS 				\
	if(error <= 0) {				\
		if(arguments != NULL)			\
			bfcp_free_arguments(arguments);	\
		if(message != NULL)			\
			bfcp_free_message(message);	\
		close(server_sock);			\
		pthread_mutex_unlock(&count_mutex);	\
		return -1;				\
	} else {					\
		bfcp_free_arguments(arguments);		\
		bfcp_free_message(message);		\
		pthread_mutex_unlock(&count_mutex);	\
	}


static unsigned short int base_transactionID = 1;	/* TransactionID of the client */
#ifndef WIN32
static struct pollfd pollfds[1];
#endif

/* Thread- and mutex-related variables */
static pthread_mutex_t count_mutex;
static pthread_t thread;

/* Socket-related variables */
static int server_sock;		/* File descriptor of the connection towards the FCS */
static fd_set wset;		/* Select-set for writing data to the FCS */

/* TLS-related stuff */
static int bfcp_transport;	/* Wheter we use TCP/BFCP or TCP/TLS/BFCP */
static SSL_CTX *context;	/* SSL Context */
static SSL_METHOD *method;	/* SSL Method (TLSv1) */
static SSL *session;		/* SSL Session */


/* Headers for private methods */
int send_message_to_server(bfcp_message *message, int sockfd);
bfcp_message *received_message_from_server(int sockfd, int *close_socket);
void *recv_thread(void *func);


/* Create a new Participant */
struct bfcp_participant_information *bfcp_initialize_bfcp_participant(unsigned long int conferenceID, unsigned short int userID, char *IP_address_server, unsigned short int port_server, void( *received_msg)(bfcp_received_message *recv_msg), int transport)
{
	if(conferenceID == 0) {
		printf("Invalid conference ID\n");
		return NULL;
	}
	if(userID == 0) {
		printf("Invalid userID\n");
		return NULL;
	}
	if(port_server <= 1024) {
		printf("Invalid port\n");
		port_server = BFCP_FCS_DEFAULT_PORT;
	}
	if(IP_address_server == NULL) {
		printf("Invalid IP\n");
		return NULL;
	}

#ifdef WIN32
	WSADATA wsaData;
	WSAStartup(MAKEWORD(1, 1), &wsaData);
#endif

	if(transport < 1)
		bfcp_transport = BFCP_OVER_TCP;
	else
		bfcp_transport = BFCP_OVER_TLS;

	if(bfcp_transport == BFCP_OVER_TLS) {
		/* Initialize TLS-related stuff */
		SSL_library_init();
		SSL_load_error_strings();

		method = (SSL_METHOD*)TLSv1_client_method();
		if(!method)
			return NULL;
		context = SSL_CTX_new(method);
		if(!context)
			return NULL;
		if(SSL_CTX_set_cipher_list(context, SSL_DEFAULT_CIPHER_LIST) < 0)
			return NULL;
	}

	struct sockaddr_in server_addr;
	conference_participant struct_participant;

	/* Allocate and setup the new participant */
	struct_participant = (conference_participant)calloc(1, sizeof(bfcp_participant_information));
	if(struct_participant == NULL) {
		printf("Couldn't allocate participant\n");
		return NULL;
	}

	struct_participant->conferenceID = conferenceID;
	struct_participant->userID = userID;
	struct_participant->pfloors = NULL;

	/* Initialize the mutex */
	pthread_mutex_init(&count_mutex, NULL);
	pthread_mutex_lock(&count_mutex);

	/* Handle the socket-related operations */
	server_sock = socket(AF_INET,SOCK_STREAM,0);
	if(server_sock == -1) {
		pthread_mutex_unlock(&count_mutex);
		printf("Couldn't create socket\n");
		return NULL;
	}

	/* Fill in the server information */
	server_addr.sin_family = AF_INET;
#ifndef WIN32
	if((inet_aton(IP_address_server, &server_addr.sin_addr)) <= 0) {	/* Not a numeric IP... */
#else
	server_addr.sin_addr.s_addr = inet_addr(IP_address_server);
	if((inet_addr(IP_address_server)) <= 0) {
#endif
		struct hostent *host = gethostbyname(IP_address_server);	/* ...resolve name */
		if(!host) {
			pthread_mutex_unlock(&count_mutex);
			printf("Couldn't get host\n");
			return NULL;
		}
		server_addr.sin_addr = *(struct in_addr *)host->h_addr_list;
	}
	server_addr.sin_port = htons(port_server);
	memset(&(server_addr.sin_zero), '\0', 8);

	/* Connect to the Floor Control Server */
	if(connect(server_sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
		pthread_mutex_unlock(&count_mutex);
		printf("Couldn't connect (is the server up?)\n");
		return NULL;
	}

	if(bfcp_transport == BFCP_OVER_TLS) {
		/* Once the connection has been established, make the SSL Handshake */
		session = SSL_new(context);
		if(!session) {
#ifndef WIN32
			shutdown(server_sock, SHUT_RDWR);
#else
			shutdown(server_sock, SD_BOTH);
#endif
			close(server_sock);
			pthread_mutex_unlock(&count_mutex);
			return NULL;
		}
		if(SSL_set_fd(session, server_sock) < 1) {
#ifndef WIN32
			shutdown(server_sock, SHUT_RDWR);
#else
			shutdown(server_sock, SD_BOTH);
#endif
			close(server_sock);
			pthread_mutex_unlock(&count_mutex);
			return NULL;
		}
		if(SSL_connect(session) < 1) {
#ifndef WIN32
			shutdown(server_sock, SHUT_RDWR);
#else
			shutdown(server_sock, SD_BOTH);
#endif
			close(server_sock);
			pthread_mutex_unlock(&count_mutex);
			return NULL;
		}
	}

	FD_ZERO(&wset);
	FD_SET(server_sock, &wset);

	/* Create the thread that will handle all incoming messages from the FCS */
	if(pthread_create(&thread, NULL, recv_thread, (void *)received_msg) < 0) {
		pthread_mutex_unlock(&count_mutex);
		printf("Couldn't create thread\n");
		return NULL;
	}
	pthread_detach(thread);
	pthread_mutex_unlock(&count_mutex);

	return struct_participant;
}

/* Destroy an existing Participant */
int bfcp_destroy_bfcp_participant(bfcp_participant_information **participantp)
{
	if(participantp == NULL)
		return -1;

	bfcp_participant_information *participant = *participantp;

	remove_floor_list_p(participant->pfloors);
	free(participant);
	participant = NULL;
	*participantp = NULL;

	/* Close the connection towards the FCS */
#ifndef WIN32
	shutdown(server_sock, SHUT_RDWR);
#else
	shutdown(server_sock, SD_BOTH);
#endif
	close(server_sock);

	if(bfcp_transport == BFCP_OVER_TLS) {
		/* Free TLS-related stuff */
		SSL_free(session);
		SSL_CTX_free(context);
	}

	/* Destroy the thread */
	pthread_cancel(thread);

	bfcp_transport = -1;

	return 0;
}

/* Add a floor to the list of floors the participant will be aware of */
int bfcp_insert_floor_participant(conference_participant participant, unsigned short int floorID)
{
	if(floorID <= 0)
		return -1;
	if(participant == NULL)
		return -1;

   	participant->pfloors = insert_floor_list_p(participant->pfloors, floorID, NULL);

	return 0;
}

/* Delete a floor from the list of floors the participant is aware of */
int bfcp_delete_floor_participant(conference_participant participant, unsigned short int floorID)
{
	if(floorID <= 0)
		return -1;
	if(participant == NULL)
		return -1;

	floors_participant floor, temp_floor;
	floor = participant->pfloors;

	if(floor != NULL) {
		if(floor->floorID == floorID) {
			participant->pfloors = floor->next;
		} else {
			while((floor->next != NULL) && (floor->next->floorID != floorID))
				floor = floor->next;
			if(floor->next!=NULL) {
				temp_floor = floor;
				floor = floor->next;
				temp_floor->next = floor->next;
			}
		}
	}

	if((floor == NULL) || (floor->floorID != floorID))
		/* This floorID is not in the list */
		return -1;

	free(floor);
	floor = NULL;

	return 0;
}


/* BFCP Participant side Messages-related operations */

/* Hello */
int bfcp_hello_participant(conference_participant participant)
{
	if(participant == NULL)
		return -1;

	int error;
	bfcp_arguments *arguments;
	bfcp_message *message;

	pthread_mutex_lock(&count_mutex);

	/* Prepare a new 'Hello' message */
	arguments = bfcp_new_arguments();
	arguments->primitive = Hello;
	arguments->entity = bfcp_new_entity(participant->conferenceID, base_transactionID, participant->userID);
	message = bfcp_build_message(arguments);
	if(!message) {
		pthread_mutex_unlock(&count_mutex);
		return -1;
	}

	pthread_mutex_unlock(&count_mutex);

	/* Send the message to the FCS */
	error = send_message_to_server(message, server_sock);
	BFCP_SEND_CHECK_ERRORS;

	return 0;
}

/* FloorRequest */
int bfcp_floorRequest_participant(conference_participant participant, unsigned short int beneficiaryID, unsigned short int priority, bfcp_floors_participant *list_floors, char *participant_info)
{
	if(participant == NULL)
		return -1;
	if(priority > BFCP_HIGHEST_PRIORITY)
		priority = BFCP_HIGHEST_PRIORITY;
	if(list_floors == NULL)
		return -1;

	bfcp_arguments *arguments = NULL;
	bfcp_message *message;
	int error, dLen;
	bfcp_floor_id_list *node = NULL;

	bfcp_floors_participant *tempnode, *temp_list_floors, *floors_IDs;

	/* Check if all the provided floors exist, otherwise give an error */
	for(tempnode = list_floors; tempnode; tempnode = tempnode->next) {
		temp_list_floors = participant->pfloors;
		while(temp_list_floors && (temp_list_floors->floorID > tempnode->floorID))
			temp_list_floors = temp_list_floors->next;
		if((temp_list_floors == NULL) || (tempnode->floorID != temp_list_floors->floorID))
			/* This floor is not in the list */
			return -1;

	}

	pthread_mutex_lock(&count_mutex);

	/* Prepare a new 'FloorRequest' message */
	arguments = bfcp_new_arguments();
	arguments->primitive = FloorRequest;
	arguments->entity = bfcp_new_entity(participant->conferenceID, base_transactionID, participant->userID);

	floors_IDs = list_floors;
	if(floors_IDs != NULL)
		node = bfcp_new_floor_id_list(floors_IDs->floorID, 0);
	floors_IDs = floors_IDs->next;

	while(floors_IDs != NULL){
		bfcp_add_floor_id_list(node,floors_IDs->floorID, 0);
		floors_IDs = floors_IDs->next;
	}

	arguments->fID = node;

	if(beneficiaryID > 0)
		arguments->bID = beneficiaryID;

	if(participant_info != NULL) {
		/* If there's Participant-provided Info text, add it */
		dLen = strlen(participant_info);
		if(dLen != 0) {
			arguments->pInfo = (char *)calloc(1, dLen*sizeof(char)+1);
			if(arguments->pInfo == NULL)
				return -1;
			memcpy(arguments->pInfo, participant_info, dLen+1);
		} else
			arguments->pInfo = NULL;
	}

	arguments->priority = priority;

	message = bfcp_build_message(arguments);
	if(!message) {
		pthread_mutex_unlock(&count_mutex);
		return -1;
	}

	remove_floor_list_p(list_floors);

	pthread_mutex_unlock(&count_mutex);

	/* Send the message to the FCS */
	error = send_message_to_server(message, server_sock);
	BFCP_SEND_CHECK_ERRORS;

	return 0;
}

/* FloorRelease */
int bfcp_floorRelease_participant(conference_participant participant, unsigned short int floorRequestID)
{
	if(participant == NULL)
		return -1;
	if(floorRequestID <= 0)
		return -1;

	bfcp_arguments *arguments = NULL;
	bfcp_message *message;
	int error;

	pthread_mutex_lock(&count_mutex);

	/* Prepare a new 'FloorRelease' message */
	arguments = bfcp_new_arguments();
	arguments->primitive = FloorRelease;
	arguments->entity = bfcp_new_entity(participant->conferenceID, base_transactionID, participant->userID);

	arguments->frqID = floorRequestID;

	message = bfcp_build_message(arguments);
	if(!message) {
		pthread_mutex_unlock(&count_mutex);
		return -1;
	}

	pthread_mutex_unlock(&count_mutex);

	/* Send the message to the FCS */
	error = send_message_to_server(message, server_sock);
	BFCP_SEND_CHECK_ERRORS;

	return 0;
}

/* FloorRequestQuery */
int bfcp_floorRequestQuery_participant(conference_participant participant, unsigned short int floorRequestID)
{
	if(participant == NULL)
		return -1;
	if(floorRequestID <= 0)
		return -1;

	bfcp_arguments *arguments = NULL;
	bfcp_message *message;
	int error;

	pthread_mutex_lock(&count_mutex);

	/* Prepare a new 'FloorRequestQuery' message */
	arguments = bfcp_new_arguments();
	arguments->primitive = FloorRequestQuery;
	arguments->entity = bfcp_new_entity(participant->conferenceID, base_transactionID, participant->userID);

	arguments->frqID = floorRequestID;

	message = bfcp_build_message(arguments);
	if(!message) {
		pthread_mutex_unlock(&count_mutex);
		return -1;
	}

	pthread_mutex_unlock(&count_mutex);

	/* Send the message to the FCS */
	error = send_message_to_server(message, server_sock);
	BFCP_SEND_CHECK_ERRORS;

	return 0;
}

/* UserQuery */
int bfcp_userQuery_participant(conference_participant participant, unsigned short int beneficiaryID)
{
	if(participant == NULL)
		return -1;

	bfcp_arguments *arguments = NULL;
	bfcp_message *message;
	int error;

	pthread_mutex_lock(&count_mutex);

	/* Prepare a new 'UserQuery' message */
	arguments = bfcp_new_arguments();
	arguments->primitive = UserQuery;
	arguments->entity = bfcp_new_entity(participant->conferenceID, base_transactionID, participant->userID);

	arguments->bID = beneficiaryID;

	message = bfcp_build_message(arguments);
	if(!message) {
		pthread_mutex_unlock(&count_mutex);
		return -1;
	}

	pthread_mutex_unlock(&count_mutex);

	/* Send the message to the FCS */
	error = send_message_to_server(message, server_sock);
	BFCP_SEND_CHECK_ERRORS;

	return 0;
}

/* UserQuery */
int bfcp_floorQuery_participant(conference_participant participant, bfcp_floors_participant *list_floors)
{
	if(participant == NULL)
		return -1;

	bfcp_floors_participant *temp_list_floors, *tempnode;
	bfcp_arguments *arguments = NULL;
	bfcp_message *message;
	bfcp_floors_participant *floors_IDs;
	int error;

	/* Check if all the provided floors exist, otherwise give an error */
	for(tempnode = list_floors; tempnode; tempnode = tempnode->next) {
		temp_list_floors = participant->pfloors;
		while(temp_list_floors && (temp_list_floors->floorID > tempnode->floorID))
			temp_list_floors = temp_list_floors->next;

		if((temp_list_floors == NULL) || (tempnode->floorID != temp_list_floors->floorID))
			/* This floor is not in the list */
			return -1;
	}

	pthread_mutex_lock(&count_mutex);

	/* Prepare a new 'FloorQuery' message */
	arguments = bfcp_new_arguments();
	arguments->primitive = FloorQuery;
	arguments->entity = bfcp_new_entity(participant->conferenceID, base_transactionID, participant->userID);

	floors_IDs = list_floors;
	while(floors_IDs != NULL) {
		if(arguments->fID)
			bfcp_add_floor_id_list(arguments->fID, floors_IDs->floorID, 0);
		else
			arguments->fID = bfcp_new_floor_id_list(floors_IDs->floorID, 0);
		floors_IDs = floors_IDs->next;
	}

	message = bfcp_build_message(arguments);
	if(!message) {
		pthread_mutex_unlock(&count_mutex);
		return -1;
	}

	remove_floor_list_p(list_floors);

	pthread_mutex_unlock(&count_mutex);

	/* Send the message to the FCS */
	error = send_message_to_server(message, server_sock);
	BFCP_SEND_CHECK_ERRORS;

	return 0;
}

int bfcp_chairAction_participant(conference_participant participant, unsigned short int floorRequestID, char *statusInfo, unsigned short int status, bfcp_floors_participant *list_floors, unsigned short int queue_position)
{
	if(participant == NULL)
		return -1;
	if(list_floors == NULL)
		return -1;
	if(floorRequestID <= 0)
		return -1;
	if((status != BFCP_ACCEPTED) && (status != BFCP_DENIED) && (status != BFCP_REVOKED))
		return -1;

	bfcp_floors_participant *temp_list_floors, *tempnode;
	bfcp_arguments *arguments;
	bfcp_message *message;
	bfcp_floors_participant *floors_IDs;
	int error;
	bfcp_floor_request_status *fRS = NULL, *fRS_temp;
	bfcp_overall_request_status *oRS;

	/* Check if all the floors in the request exist, otherwise give an error */
	for(tempnode = list_floors; tempnode; tempnode = tempnode->next) {
		temp_list_floors = participant->pfloors;
		while(temp_list_floors && (temp_list_floors->floorID > tempnode->floorID))
			temp_list_floors = temp_list_floors->next;
		if((temp_list_floors == NULL) || (tempnode->floorID != temp_list_floors->floorID))
			/* This floor is not in the list */
			return -1;
	}

	pthread_mutex_lock(&count_mutex);

	arguments = bfcp_new_arguments();
	arguments->primitive = ChairAction;
	arguments->entity = bfcp_new_entity(participant->conferenceID, base_transactionID, participant->userID);

	if(status != BFCP_ACCEPTED)
		queue_position = 0;

	floors_IDs = list_floors;
	if(floors_IDs != NULL)
		fRS = bfcp_new_floor_request_status(floors_IDs->floorID, 0, 0, floors_IDs->sInfo);
	if(fRS == NULL)
		return -1;
	floors_IDs = floors_IDs->next;
	while(floors_IDs != NULL) {
		fRS_temp = bfcp_new_floor_request_status(floors_IDs->floorID, 0, 0, floors_IDs->sInfo);
		if(fRS_temp != NULL)
			bfcp_list_floor_request_status(fRS, fRS_temp, NULL);
		floors_IDs = floors_IDs->next;
	}

	oRS = bfcp_new_overall_request_status(floorRequestID, status, queue_position, statusInfo);

	arguments->frqInfo = bfcp_new_floor_request_information(floorRequestID, oRS, fRS, NULL, NULL, 0, NULL);

	message = bfcp_build_message(arguments);
	if(!message) {
		pthread_mutex_unlock(&count_mutex);
		return -1;
	}

	remove_floor_list_p(list_floors);

	pthread_mutex_unlock(&count_mutex);

	/* Send the message to the FCS */
	error = send_message_to_server(message, server_sock);
	BFCP_SEND_CHECK_ERRORS;

	return 0;
}


/* Helper operations */

/* Create a 'bfcp_floors_participant' element */
bfcp_floors_participant *create_floor_list_p(unsigned short int floorID, char *status_info)
{
	bfcp_floors_participant *floor_list;
	int dLen;

	/* Allocate a new element */
	floor_list = (bfcp_floors_participant *)calloc(1, sizeof(bfcp_floors_participant));
	if(floor_list == NULL)
		return NULL;

	else {
		/* Initialize the new element */
		floor_list->floorID = floorID;
		if(status_info != NULL) {
			dLen = strlen(status_info);
			if(dLen != 0) {
				floor_list->sInfo = (char *)calloc(1, dLen*sizeof(char)+1);
				if(floor_list->sInfo == NULL)
					return NULL;
				memcpy(floor_list->sInfo, status_info, dLen+1);
			}
		} else
			floor_list->sInfo = NULL;
		floor_list->next = NULL;
	}

	return floor_list;
}

/* Create a 'bfcp_floors_participant' element and add it to an existing list */
bfcp_floors_participant *insert_floor_list_p(floors_participant floor_list, unsigned short int floorID, char *status_info)
{
	if(floorID <= 0)
		return NULL;

	floors_participant floor, ini_floor_list;
	int dLen;

	/* First check if such a floor already exists */
	ini_floor_list = floor_list;
	while(ini_floor_list) {
		if(ini_floor_list->floorID == floorID)
			/* FloorID already exists */
			return NULL;
		ini_floor_list = ini_floor_list->next;
	}
	ini_floor_list = NULL;

	/* Allocate a new element */
	floor = (floors_participant)calloc(1, sizeof(bfcp_floors_participant));
	if(floor == NULL)
		return NULL;

	if(floor_list == NULL) {
		floor->floorID = floorID;
		if(status_info != NULL) {
			/* If there's Status Info text, add it */
			dLen = strlen(status_info);
			if(dLen != 0){
				floor->sInfo = (char *)calloc(1, dLen*sizeof(char)+1);
				if(floor->sInfo == NULL)
					return NULL;
				memcpy(floor->sInfo, status_info, dLen+1);
			}
		} else
			floor->sInfo = NULL;
		floor->next = NULL;
		floor_list =floor;
	} else if(floor_list->floorID < floorID) {
		floor->floorID = floorID;
		if(status_info != NULL) {
			/* If there's Status Info text, add it */
			dLen = strlen(status_info);
			if(dLen != 0){
				floor->sInfo = (char *)calloc(1, dLen*sizeof(char)+1);
				if(floor->sInfo == NULL)
					return NULL;
				memcpy(floor->sInfo, status_info, dLen+1);
			}
		} else
			floor->sInfo = NULL;
		floor->next = floor_list;
		floor_list = floor;
	} else {
		ini_floor_list = floor_list;
		while(ini_floor_list->next && (ini_floor_list->next->floorID > floorID))
			ini_floor_list = ini_floor_list->next;
		floor->floorID = floorID;
		if(status_info != NULL) {
			/* If there's Status Info text, add it */
			dLen = strlen(status_info);
			if(dLen != 0){
				floor->sInfo = (char *)calloc(1, dLen*sizeof(char)+1);
				if(floor->sInfo == NULL)
					return NULL;
				memcpy(floor->sInfo, status_info, dLen+1);
			}
		} else
			floor->sInfo = NULL;

		floor->next = ini_floor_list->next;
		ini_floor_list->next = floor;
	}

	return floor_list;
}

/* Destroy a list of 'bfcp_floors_participant' elements */
int remove_floor_list_p(floors_participant floor_list)
{
	floors_participant next;

	/* Free all the elements from the floors list */
	while(floor_list){
		next = floor_list->next;
		free(floor_list->sInfo);
		floor_list->sInfo = NULL;
		free(floor_list);
		floor_list = NULL;
		floor_list = next;
	}

	return 0;
}


/* Private methods */

/* Send an already composed message (buffer) to the FCS */
int send_message_to_server(bfcp_message *message, int sockfd)
{
	if(message == NULL)
		return -1;

	int error = 0;
	int total = 0;		/* How many bytes have been sent so far */
	int bytesleft = 0;	/* How many bytes still have to be sent */
	bytesleft = message->length;

	/* Wait up to ten seconds before timeout */
	struct timeval tv;
	tv.tv_sec = 10;
	tv.tv_usec = 0;

	while(total < message->length) {
#ifdef WIN32
		error = select(sockfd+1, NULL, &wset, NULL, &tv);
		if(error < 0)
			return -1;	/* Select error */
		if(error == 0)
			return -1;	/* Timeout */
		if(FD_ISSET(sockfd, &wset))  {
#endif
			if(bfcp_transport == BFCP_OVER_TLS)
				error = SSL_write(session, message->buffer+total, bytesleft);
			else	/* BFCP_OVER_TCP */
				error = send(sockfd, (const char *)(message->buffer+total), bytesleft, 0);
			if(error == -1)		/* Error sending the message */
				break;
			total += error;		/* Update the sent and to-be-sent bytes */
			bytesleft -= error;
#ifdef WIN32
		}
#endif
	}

	base_transactionID++;

	return error;
}

/* Handle a just received message */
bfcp_message *received_message_from_server(int sockfd, int *close_socket)
{
	int error, total = 0;
	bfcp_message *message = NULL;
	int in_length;

	/* Reserve enough space for the common header (12 bytes) */
	unsigned char *common_header = (unsigned char *)calloc(1, 12);

	*close_socket = 0;
	/* First look in the common header, since it will report the length of the whole message */
	if(bfcp_transport == BFCP_OVER_TLS)
		error = SSL_read(session, common_header, 12);
	else	/* BFCP_OVER_TCP */
		error = recv(sockfd, (char *)(common_header), 12, 0);
	if(error == 0) {
		/* The FCS closed the connection */
		printf("Server closed connection!\n");
		free(common_header);
		common_header = NULL;
		*close_socket = 1;
		close(sockfd);
		return NULL;
	}
	if(error == -1) {
		/* There was an error while receiving the message */
		free(common_header);
		common_header = NULL;
		return NULL;
	}
	message = bfcp_new_message(common_header, error);
	/* Get the message length from the header of the message */
	in_length = bfcp_get_length(message) + 12;
	/* Strip the header length, since we already got it: we are interested in the rest, the payload */
	total = in_length - 12;
	if(total < 0) {
		/* The reported length is corrupted */
		free(common_header);
		common_header = NULL;
		return NULL;
	}
	if(total == 0){
		/* The whole message has already been received, there's no payload (e.g. ChairActionAck) */
		free(common_header);
		common_header = NULL;
		return message;
	}

	/* Reserve enough space for the rest of the message */
	unsigned char *buffer = (unsigned char *)calloc(1, total);

	if(bfcp_transport == BFCP_OVER_TLS) {	/* FIXME */
		error = SSL_read(session, buffer, total);
	} else {	/* BFCP_OVER_TCP */
		int missing = total;
		while(1) {
			if(missing <= 0)
				break;
			error = recv(sockfd, (char *)(buffer), total, 0);
			if(error == 0) {
				/* The FCS closed the connection */
				printf("Server closed connection!\n");
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
		}
	}
	if(error == 0) {
		/* The FCS closed the connection */
		free(buffer);
		buffer = NULL;
		free(common_header);
		common_header = NULL;
		close(sockfd);
		*close_socket = 1;
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

/* Thread listening for incoming messages from the FCS */
void *recv_thread(void *func)
{
	int(*received_msg)(bfcp_received_message *recv_msg) = func;

	int error;
	bfcp_message *message = NULL;
	bfcp_received_message *recv_msg = NULL;
	int close_socket;

#ifdef WIN32
	fd_set rset;
	FD_ZERO(&rset);
#endif
	for(;;) {
#ifdef WIN32
		FD_SET(server_sock, &rset);
		error = select(server_sock+1, &rset, NULL, NULL, NULL);
#else
		pollfds[0].fd = server_sock;
		pollfds[0].events = POLLIN;
		error = poll(pollfds, 1, -1);
#endif
		if(error < 0) {
			close(server_sock);
			if(bfcp_transport == BFCP_OVER_TLS)
				SSL_free(session);
			pthread_exit(0);
		}
#ifndef WIN32
		if(pollfds[0].revents == POLLIN) {
#endif
			message = received_message_from_server(server_sock, &close_socket);
			if(message != NULL) {
				recv_msg = bfcp_parse_message(message);
				if(recv_msg != NULL)
					/* Trigger the application callback to notify about the new incoming message */
					received_msg(recv_msg);
			} else {
				if(close_socket == 1)
					pthread_exit(0);
			}
			if(message != NULL)
				bfcp_free_message(message);
#ifndef WIN32
                } else if((pollfds[0].revents & POLLERR) ||
                                (pollfds[0].revents & POLLHUP) ||
                                (pollfds[0].revents & POLLNVAL)) {
                        printf("We got an error polling!!\n");
                        close(server_sock);
                        if(bfcp_transport == BFCP_OVER_TLS && session != NULL)
                                SSL_free(session);
                        pthread_exit(0);
		}
#endif
	}
}
