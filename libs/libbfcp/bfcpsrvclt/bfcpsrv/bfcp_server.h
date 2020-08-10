#ifndef _BFCP_SERVER_H
#define _BFCP_SERVER_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>

#ifndef WIN32
#include <poll.h>
#endif

#include "bfcp_link_list.h"

/* The BFCP messages building/parsing library */
#include <bfcp_messages.h>

/* Definition for threads (Asterisk redefines pthreads) */
#include "bfcp_threads.h"
#include "switch.h"

#define BFCP_FCS_DEFAULT_PORT	2345	/* The default port the Floor Control Server will bind to */
#define BFCP_BACKLOG		10	/* The default value for the pending connections queue */
#define BFCP_MAX_CONNECTIONS 	1000	/* The default value for how many connections the server can hold at the same time */

/* The library supports  TCP/BFCP, TCP/TLS/BFCP and UDP/BFCP */
#define BFCP_OVER_TCP	0
#define BFCP_OVER_TLS	1
#define BFCP_OVER_UDP	2

struct sockaddr_in cliaddr;	/* Stores client address of current message read and later may be used to send response to client */
socklen_t m_addrlen;        /* Stores length of client address */

/* BFCP Conference instance */
typedef struct bfcp_conference {
	int bfcp_transport;			/* BFCP/TCP (0) or BFCP/TLS (1) or BFCP/UDP (2) */
	bfcp_queue *pending;			/* The Pending list */
	bfcp_queue *accepted;			/* The Accepted list */
	bfcp_queue *granted;			/* The Granted list */
	unsigned long int conferenceID;		/* The BFCP ConferenceID */
	unsigned short int floorRequestID;	/* The base FloorRequestID to increment at each new request */
	bfcp_list_users *user;			/* The Users list */
	bfcp_list_floors *floor;		/* The Floors list */
	unsigned long int chair_wait_request;	/* Time in miliseconds that the system will wait for a ChairAction */
	int automatic_accepted_deny_policy;	/* Policy for automated responses when a chair is missing (0 = accept request, 1 = reject request) */
} bfcp_conference;
/* Pointer to a BFCP Conference */
typedef bfcp_conference *conference;

/* Floor Control Server (FCS) */
typedef struct bfcp_server {
	int bfcp_transport;				/* BFCP/TCP (0) or BFCP/TLS (1) or BFCP/UDP (2) */
	unsigned short int Actual_number_conference;	/* The current number of managed conferences */
	unsigned short int Max_number_conference;	/* The maximum number of allowed conferences */
	bfcp_conference *list_conferences;		/* The linked list of currently managed conferences */
	unsigned short int restricted[4];
	/* Callback to notify the Application Server about BFCP events */
	int( *callback_func)(bfcp_arguments *arguments, int outgoing_msg);
} bfcp_server;
/* Pointer to a FCS instance */
typedef bfcp_server *bfcp;

/* Thread handling a specific FloorRequest instance */
typedef struct bfcp_thread {
	bfcp_conference *conf;			/* BFCP Conference this thread is related to */
	unsigned long int chair_wait_request;	/* Time in milisecons that the thread will wait for a ChairAction */
	unsigned short int floorRequestID;	/* Identifier of the FloorRequest this thread is handling */
} bfcp_thread;
/* Pointer to a thread */
typedef bfcp_thread *struct_thread;

/* Thread- and mutex-related variables */
bfcp_mutex_t count_mutex;
pthread_t thread_tcp, thread_udp;

/* Socket-related variables (File descriptors and sets) */
int maxfd, server_sock_tcp, server_sock_udp, maxi, fds_no;
int close_udp_conn; /* Flag used to exit from thread running for UDP socket */
int client[BFCP_MAX_CONNECTIONS];
fd_set rset, wset, allset;
#ifndef WIN32
struct pollfd pollfds[BFCP_MAX_CONNECTIONS+1];
struct pollfd poll_fd_udp[1];  /* For UDP socket */
#endif

/* TLS-related stuff: an array of sessions is needed to pair up with client[i] */
SSL_CTX *context;
SSL_METHOD *method;
SSL *session[BFCP_MAX_CONNECTIONS];


/**************************/
/* Server-related methods */
/**************************/

/* Create a new Floor Control Server */
struct bfcp_server *bfcp_initialize_bfcp_server(unsigned short int Max_conf, unsigned short int port_server, int(*notify_to_server_app)(bfcp_arguments *arguments, int outgoing_msg), int transport, char *certificate, char *privatekey, char *restrict);
/* Destroy a currently running Floor Control Server */
int bfcp_destroy_bfcp_server(bfcp_server **serverp);
/* Create a new BFCP Conference and add it to the FCS */
int bfcp_initialize_conference_server(bfcp_server *server, unsigned long int ConferenceID, unsigned short int Max_Num_Floors, unsigned short int Max_Number_Floor_Request, int automatic_accepted_deny_policy, unsigned long int chair_wait_request);
/* Destroy a currently managed BFCP Conference and remove it from the FCS */
int bfcp_destroy_conference_server(bfcp_server *server, unsigned long int ConferenceID);
/* Change the maximum number of allowed conferences in the FCS */
int bfcp_change_number_bfcp_conferences_server(bfcp_server *server, unsigned short int Num);
/* Change the maximum number of users that can be granted this floor at the same time */
int bfcp_change_number_granted_floor_server(bfcp_server *server, unsigned long int ConferenceID, unsigned short int FloorID, unsigned short int limit_granted_floor);
/* Change the allowed number of per-floor requests for this list */
int bfcp_change_user_req_floors_server(bfcp_server *server, unsigned short int Max_Number_Floor_Request);
/* Change the automated policy for requests related to floors that have no chair */
int bfcp_change_chair_policy(bfcp_server *server, unsigned long int ConferenceID, int automatic_accepted_deny_policy, unsigned long int chair_wait_request);
/* Add a floor to an existing conference */
int bfcp_add_floor_server(bfcp_server *server, unsigned long int ConferenceID, unsigned short int FloorID, unsigned short int ChairID, int limit_granted_floor);
/* Remove a floor from an existing conference */
int bfcp_delete_floor_server(bfcp_server *server, unsigned long int ConferenceID, unsigned short int FloorID);
/* Set a participant as chair of a floor */
int bfcp_add_chair_server(bfcp_server *server, unsigned long int ConferenceID, unsigned short int FloorID, unsigned short int ChairID);
/* Set no participant as chair for a floor */
int bfcp_delete_chair_server(bfcp_server *server, unsigned long int ConferenceID, unsigned short int FloorID);
/* Add a participant to the list of users of a BFCP Conference */
int bfcp_add_user_server(bfcp_server *server, unsigned long int ConferenceID, unsigned short int userID, char *user_URI, char *user_display_name);
/* Remove a participant from the list of users of a BFCP Conference */
int bfcp_delete_user_server(bfcp_server *server, unsigned long int ConferenceID, unsigned short int userID);

/*!
 \brief Set user address of participant with userID in the user_list
 \param server Pointer to Floor Control Server instance
 \param conferenceID BFCP conferenceID
 \param userID BFCP userID
 \param client_address char pointer pointing to IP address of client
 \param client_port Port at which client is listening
 \return 0, If address successfully set else return -1
 */
int bfcp_set_user_address_server(bfcp_server *server,
				  unsigned long int conferenceID,
				  unsigned short int userID,
				  char *client_address,
				  unsigned short int client_port);

/*!
 \brief Retrieve user address of participant with userID in the user_list
 \param server Pointer to Floor Control Server instance
 \param conferenceID BFCP conferenceID
 \param userID BFCP userID
 \return Pointer to client's socket address of type struct sockaddr_in
 */
struct sockaddr_in *bfcp_get_user_address_server(bfcp_server *server,
						  unsigned long int conferenceID,
						  unsigned short int userID);

/******************/
/* Helper methods */
/******************/

/* Create a new 'bfcp_floor_request_information' (bfcp_messages.h) out of a 'pnode' */
bfcp_floor_request_information *create_floor_request_userID(pnode traverse, lusers users, unsigned short int userID, unsigned short int request_status, int i);
/* Create a new 'bfcp_floor_request_information' (bfcp_messages.h) out of a floor */
bfcp_floor_request_information *create_floor_message(unsigned short int floorID, pnode traverse, lusers users, unsigned short int request_status, int i);
/* Setup and send a FloorStatus BFCP message */
int bfcp_show_floor_information(unsigned long int ConferenceID, unsigned short int TransactionID, unsigned short int userID, bfcp_conference *conference, unsigned short int floorID, int sockid, fd_set allset, int *client, pnode newnode, unsigned short int status);
/* Prepare the needed arguments for a FloorRequestStatus BFCP message */
int bfcp_print_information_floor(bfcp_conference *conference, unsigned short int userID, unsigned short int TransactionID, pnode newnode, unsigned short int status);
/* Setup and send a FloorRequestStatus BFCP message */
int bfcp_show_requestfloor_information(bfcp_list_users *list_users, bfcp_queue *accepted_queue, unsigned long int ConferenceID, unsigned short int userID, unsigned short int TransactionID, pnode newnode, unsigned short int status, int socket);
/* Handle a BFCP message a client sent to the FCS */
bfcp_message *received_message_from_client(int sockfd, int i, int transport);

/*!
 \brief Handle a BFCP message a client sent to the FCS (UDP)
 \param sockfd File descriptor of UDP socket of server
 \return Pointer to bfcp_message
 */
bfcp_message *received_message_from_client_over_udp(int sockfd);

/* Remove all floor requests made by a user from all existing nodes */
int bfcp_remove_floorrequest_from_all_nodes(bfcp_conference *server, unsigned short int userID);
/* Remove all floor requests made by a user from a queue */
int bfcp_remove_floorrequest_from_a_queue(bfcp_queue *conference, unsigned short int userID);
/* Disable and remove all floor events notifications to an user */
int bfcp_remove_floorquery_from_all_nodes(bfcp_list_floors *lfloors, unsigned short int userID);
/* A controller to check timeouts when waiting for a ChairAction */
void *watchdog(void *st_thread);
/* Handle an incoming FloorRequest message */
int bfcp_FloorRequest_server(bfcp_server *server, unsigned long int ConferenceID, unsigned short int TransactionID, pnode newnode, int sockfd, int y);
/* Handle an incoming FloorRelease message */
int bfcp_FloorRelease_server(bfcp_server *server, unsigned long int ConferenceID, unsigned short int TransactionID, unsigned short int userID, unsigned long int FloorRequestID, int sockfd, int y);
/* Handle an incoming ChairAction message */
int bfcp_ChairAction_server(bfcp_server *server, unsigned long int ConferenceID, bfcp_floor *list_floors, unsigned short int userID, unsigned long int FloorRequestID, int RequestStatus, char *chair_info, int queue_position, unsigned short int TransactionID, int sockfd, int y);
/* Handle an incoming Hello message */
int bfcp_hello_server(bfcp_server *server, unsigned long int ConferenceID, unsigned short int userID, unsigned short int TransactionID, int sockfd, int y);
/* Handle an incoming UserQuery message */
int bfcp_userquery_server(bfcp_server *server, unsigned long int ConferenceID, unsigned short int userID, unsigned short int TransactionID, unsigned short int beneficiaryID, int sockfd, int y);
/* Handle an incoming FloorQuery message */
int bfcp_floorquery_server(bfcp_server *server, unsigned long int ConferenceID, bfcp_floor *list_floors, unsigned short int userID, unsigned short int TransactionID, int sockfd, int y);
/* (??) */
int bfcp_floor_query_server(bfcp_list_floors *lfloors, bfcp_floor *list_floors, unsigned short int userID, int sockfd);

/*!
 \brief Add new floor query for a floor in query list, if not exit for user with userID
        and also set client address corrospoding to participant, is specific for UDP socket
 \param lfloors Pointer to a list of floors
 \param list_floors Pointer to a specific instance of Floor Information
 \param userID BFCP userID
 \param client_addr BFCP particiapnt addrss
 \param sockfd file descriptor of UDP socket of server
 \return 0, if floor query added or already present else -1
 */
int bfcp_floor_query_server_over_udp(bfcp_list_floors *lfloors,
                                     bfcp_floor *list_floors,
                                     unsigned short int userID,
                                     struct sockaddr_in *client_addr,
                                     int sockfd);

/* Handle an incoming FloorRequestQuery message */
int bfcp_floorrequestquery_server(bfcp_server *server, unsigned long int ConferenceID, unsigned short int TransactionID,    unsigned long int FloorRequestID, unsigned short int userID, int sockfd, int y);
/* Check if it's fine to grant a floor to an accepted request */
int check_accepted_node(bfcp_conference *conference, pnode queue_accepted, unsigned short int floorID, char *chair_info);
/* Check if it's fine to grant a floor to an accepted request (??) */
int give_free_floors_to_the_accepted_nodes(bfcp_conference *conference, bfcp_queue *laccepted, bfcp_list_floors *lfloors, char *chair_info);
/* Setup and send an Error reply to a participant */
int bfcp_error_code(unsigned long int ConferenceID, unsigned short int userID, unsigned short int TransactionID, int code, char *error_info, bfcp_unknown_m_error_details *details, int sockfd, int i, int transport);
/* Send a BFCP message to a participant */
int send_message_to_client(bfcp_arguments *arguments, int sockfd, int(*notify_to_server_app)(bfcp_arguments *arguments, int outgoing_msg), int transport);

/*!
 \brief Send a BFCP message to a participant (UDP)
 \param arguments Pointer of bfcp_arguments
 \param sockfd File descriptor to participant address
 \param notify_to_server_app Notify the Application Server about BFCP events
 \param client_address Socket address of particiapnt
 \return Number of bytes send, if any error occur then return -1;
 */
int send_message_to_client_over_udp(bfcp_arguments *arguments,
                                    int sockfd,
                                    int(*notify_to_server_app)(bfcp_arguments *arguments, int outgoing_msg),
                                    struct sockaddr_in *client_address);

#endif
