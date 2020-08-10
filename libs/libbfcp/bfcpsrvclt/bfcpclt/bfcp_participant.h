#ifndef _BFCP_PARTICIPANT_H
#define _BFCP_PARTICIPANT_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifndef WIN32
#include <netdb.h>
#include <poll.h>
#endif
#include <pthread.h>

/* Needed for TCP/TLS/BFCP support */
#include <openssl/rsa.h>
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/pem.h>

/* The library supports both TCP/BFCP and TCP/TLS/BFCP */
#define BFCP_OVER_TCP	0
#define BFCP_OVER_TLS	1

/* Library to build and parse BFCP messages */
#include <bfcp_messages.h>

#define BFCP_FCS_DEFAULT_PORT 2345	/* By default the Floor Control Server will listen on this port */

/* Participant-related Floor information */
typedef struct bfcp_floors_participant {
	unsigned short floorID;			/* Floor Identifier */
	char *sInfo;				/* Status Info (text) */
	struct bfcp_floors_participant *next;
} bfcp_floors_participant;
typedef bfcp_floors_participant *floors_participant;

/* BFCP Participant */
typedef struct bfcp_participant_information {
	unsigned long int conferenceID;		/* Conference Identifier */
	unsigned short int userID;			/* User Identifier (the participant) */
	floors_participant pfloors;		/* List of floors this user is aware of */
} bfcp_participant_information;
typedef bfcp_participant_information *conference_participant;


#if defined __cplusplus
	extern "C" {
#endif

/* BFCP Participant-related operations */

/* Create a new Participant */
struct bfcp_participant_information *bfcp_initialize_bfcp_participant(unsigned long int conferenceID, unsigned short int userID, char *IP_address_server, unsigned short int port_server, void( *received_msg)(bfcp_received_message *recv_msg), int transport);
/* Destroy an existing Participant */
int bfcp_destroy_bfcp_participant(bfcp_participant_information **participant);
/* Add a floor to the list of floors the participant will be aware of */
int bfcp_insert_floor_participant(conference_participant participant, unsigned short int floorID);
/* Delete a floor from the list of floors the participant is aware of */
int bfcp_delete_floor_participant(conference_participant participant, unsigned short int floorID);


/* BFCP Participant side Messages-related operations */

/* Hello */
int bfcp_hello_participant(conference_participant participant);
/* FloorRequest */
int bfcp_floorRequest_participant(conference_participant participant, unsigned short int beneficiaryID, unsigned short int priority, bfcp_floors_participant *list_floors, char *participant_info);
/* FloorRelease */
int bfcp_floorRelease_participant(conference_participant participant, unsigned short int floorRequestID);
/* FloorRequestQuery */
int bfcp_floorRequestQuery_participant(conference_participant participant, unsigned short int floorRequestID);
/* UserQuery */
int bfcp_userQuery_participant(conference_participant participant, unsigned short int beneficiaryID);
/* FloorQuery */
int bfcp_floorQuery_participant(conference_participant participant, bfcp_floors_participant *list_floors);
/* ChairAction */
int bfcp_chairAction_participant(conference_participant participant, unsigned short int floorRequestID, char *statusInfo, unsigned short int status, bfcp_floors_participant *list_floors, unsigned short int queue_position);


/* Helper operations */

/* Create a 'bfcp_floors_participant' element */
bfcp_floors_participant *create_floor_list_p(unsigned short int floorID, char *status_info);
/* Create a 'bfcp_floors_participant' element and add it to an existing list */
bfcp_floors_participant *insert_floor_list_p(floors_participant floor_list, unsigned short int floorID, char *status_info);
/* Destroy a list of 'bfcp_floors_participant' elements */
int remove_floor_list_p(floors_participant floor_list);

#if defined __cplusplus
	}
#endif

#endif
