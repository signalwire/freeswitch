#ifndef _BFCP_LINK_LIST_H
#define _BFCP_LINK_LIST_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Definition for threads (Asterisk redefines pthreads) */
#include "bfcp_threads.h"

#include "bfcp_floor_list.h"
#include "bfcp_user_list.h"

/* The BFCP messages building/parsing library */
#include <bfcp_messages.h>

/* FloorRequestQuery instance (to notify about request events) */
typedef struct floor_request_query {
	unsigned short int userID;		/* User to notify */
	int fd;					/* File descriptor of the user's connection */
	struct floor_request_query *next;	/* Next FloorRequestQuery in the list */
} floor_request_query;

/* Floor information */
typedef struct bfcp_floor {
	unsigned short int floorID;	/* FloorID in this request */
	int status;			/* Floor state as defined in bfcp_floor_list.h */
	char *chair_info;		/* Chair-provided text about ChairActions */
	pthread_t pID;			/* Thread handling this request */
	struct bfcp_floor *next;	/* Next Floor in the list */
} bfcp_floor;
/* Pointer to a specific instance */
typedef bfcp_floor *pfloor;

/* FloorRequest information */
typedef struct bfcp_node {
	unsigned short int floorRequestID;
	unsigned short int userID;
	unsigned short int beneficiaryID;
	int priority;					/* Priority in the Pending queue */
	int queue_position;				/* Queue Position in the Accepted list */
	char *participant_info;				/* Participant-provided text */
	char *chair_info;				/* Chair-provided text about ChairActions */
	struct floor_request_query *floorrequest;	/* Array of queries for this request */
	struct bfcp_floor *floor;			/* List of floors in this request */
	struct bfcp_node *next, *prev;			/* This is a double-linked list */
} bfcp_node;
/* Pointer to a specific instance */
typedef bfcp_node *pnode;


/*
A BFCP queue is structured as follows:

		NULL  <----------- prev  <----------- prev
	head ---> object1      object2       object3 <--- tail
		(struct bfcp_node) (struct bfcp_node) (struct bfcp_node)
		next ------------> next ------------> NULL
*/
typedef struct bfcp_queue {
	pnode head;  /* The first element in the list */
	pnode tail;  /* The last element in the list */
} bfcp_queue;


/************************/
/* List-related methods */
/************************/

/* Create a new linked list of BFCP queues */
struct bfcp_queue *bfcp_create_list(void);
/* Initialize a new FloorRequest node */
struct bfcp_node *bfcp_init_request(unsigned short int userID, unsigned short int beneficiaryID, int priority, char *text, unsigned short int floorID);
/* Add a floor to an existing FloorRequest */
int bfcp_add_floor_to_request(pnode newnode, unsigned short int floorID);
/* Add an existing FloorRequest to a BFCP queue */
int bfcp_insert_request(bfcp_queue *conference, pnode newnode, unsigned short int floorRequestID, char *chair_info);
/* Return the UserID of the user who made the request */
unsigned short int bfcp_give_user_of_request(bfcp_queue *conference, unsigned short int floorRequestID);

/*!
 \brief Retrieve FloorRequestID, if user with userID exist in conference list
 \param conference The linked list of currently managed conferences
 \param userID user_id of user whose FloorRequestID is needed
 \return FloorRequestID If user exist in conference list, else return 0
 */
unsigned short int bfcp_get_floor_requestid(bfcp_queue *conference, unsigned short int userID);

/* Change status to a floor in a request */
int bfcp_change_status(bfcp_queue *conference, unsigned short int floorID, unsigned short int floorRequestID, int status, char *floor_chair_info);
/* Check if the overall status is actually the status of every floor in the request (?) */
int bfcp_all_floor_status(bfcp_queue *conference, unsigned short int floorRequestID, int status);
/* Check if the pthread identifier is valid (?) */
int bfcp_all_floor_pID(pnode newnode);
/* Remove a floor from a floor request information (?) */
int bfcp_delete_node_with_floorID(unsigned long int conferenceID, bfcp_queue *accepted, bfcp_queue *conference, unsigned short int floorID, bfcp_list_floors *lfloors, int type_list);
/* Remove all elements related to an user from a floor request information (?) */
int bfcp_delete_node_with_userID(bfcp_queue *conference, unsigned short int userID, bfcp_list_floors *lfloors);
/* Remove a FloorRequest from the BFCP queues */
bfcp_floor *bfcp_delete_request(bfcp_queue *conference, unsigned short int floorRequestID, int type_queue);
/* Get a FloorRequest from one of the BFCP queues */
bfcp_node *bfcp_extract_request(bfcp_queue *conference, unsigned short int floorRequestID);
/* Setup a FloorRequest so that it is to be sent in a BFCP message (?) */
int bfcp_floor_request_query_server(bfcp_queue *conference, unsigned short int floorRequestID, unsigned short int userID, int sockfd);
/* Remove all requests from a list */
int bfcp_clean_request_list(bfcp_queue *conference);
/* Free a linked list of requests */
int bfcp_remove_request_list(bfcp_queue **conference);
/* Kill all the running threads handling a specific FloorRequest */
int bfcp_kill_threads_request_with_FloorRequestID(bfcp_queue *conference, unsigned short int floorRequestID);
/* Convert a 'bfcp_node' info to 'bfcp_floor_request_information' (bfcp_messages.h) */
bfcp_floor_request_information *bfcp_show_floorrequest_information(bfcp_queue *conference, lusers users, unsigned long int FloorRequestID, int type_queue);


/******************/
/* Helper methods */
/******************/

/* Create a new linked list of floors */
bfcp_floor *create_floor_list(unsigned short int floorID, int status, char *floor_chair_info);
/* Add floors to a list in order */
bfcp_floor *add_floor_list(bfcp_floor *floor_list, unsigned short int floorID, int status, char *floor_chair_info);
/* Free a linked list of requests */
int remove_floor_list(bfcp_floor *floor_list);
/* Remove the list of requests from a request node */
int remove_request_list_of_node(floor_request_query *floorrequest);
/* Add a request to a request node */
int add_request_to_the_node(pnode traverse, unsigned short int userID, int sockfd);
/* Request a request from a request node */
int remove_request_from_the_node(pnode traverse, unsigned short int userID);
/* ?? */
int bfcp_accepted_pending_node_with_floorID(unsigned long int conferenceID, bfcp_queue *accepted, bfcp_queue *conference, unsigned short int floorID, bfcp_list_floors *lfloors, int type_list);
/* Change position in queue to a FloorRequest */
int bfcp_change_queue_position(bfcp_queue *conference, unsigned short int floorRequestID, int queue_position);

#endif
