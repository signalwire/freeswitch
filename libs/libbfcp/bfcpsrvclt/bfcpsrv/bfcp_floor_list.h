#ifndef _BFCP_FLOOR_LIST_H
#define _BFCP_FLOOR_LIST_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>


#define BFCP_FLOOR_STATE_WAITING	0
#define BFCP_FLOOR_STATE_ACCEPTED	1
#define BFCP_FLOOR_STATE_GRANTED	2


/* FloorQuery instance (to notify about floor events) */
typedef struct floor_query {
	unsigned short int userID;		/* User to notify */
	int fd;					/* File descriptor of the user's connection */
	struct sockaddr_in *client_addr;	/* client's address required if server running over UDP */
	struct floor_query *next;		/* Next FloorQuery in the list */
} floor_query;

/* Floor */
typedef struct bfcp_floors {
	unsigned short int floorID;		/* Floor ID */
	unsigned short int chairID;		/* UserID of the Chair for this floor */
	unsigned short int floorState;		/* Current state of the floor (free or not) */
	unsigned short int limit_granted_floor;	/* Number of users this floor can be granted to at the same time */
	struct floor_query *floorquery;		/* Users interested in events related to this floor (FloorQuery) */
} bfcp_floors;
/* Pointer to a Floor instance */
typedef bfcp_floors *floors;

/* Linked list of floors */
typedef struct bfcp_list_floors {
	unsigned short int number_floors;		/* The maximum allowed number of floors in this conference */
	unsigned short int actual_number_floors;	/* The currently available floors in this conference */
	struct bfcp_floors *floors;			/* List of floors */
} bfcp_list_floors;
/* Pointer to a list of floors */
typedef bfcp_list_floors *lfloors;


/*************************/
/* Floor-related methods */
/*************************/

/* Create a new linked list of floors */
struct bfcp_list_floors *bfcp_create_floor_list(unsigned short int Max_Num);
/* Add a floor (and its chair, if present) to a list of floors */
int bfcp_insert_floor(bfcp_list_floors *lfloors, unsigned short int floorID, unsigned short int chairID);
/* Get the number of currently available floors in a list */
int bfcp_return_number_floors(bfcp_list_floors *lfloors);
/* Change the maximum number of allowed floors in a conference */
int bfcp_change_number_floors(bfcp_list_floors *lfloors, unsigned short int Num);
/* Check if a floor exists in a conference */
int bfcp_exist_floor(bfcp_list_floors *lfloors, unsigned short int floorID);
/* Remove a floor from a floors list */
int bfcp_delete_floor(bfcp_list_floors *lfloors, unsigned short int floorID);
/* Change the chair's userID for a floor (setting chair to 0 removes it) */
int bfcp_change_chair(bfcp_list_floors *lfloors, unsigned short int floorID, unsigned short int chairID);
/* Get the maximum number of users that can be granted this floor at the same time */
int bfcp_return_number_granted_floor(bfcp_list_floors *lfloors, unsigned short int floorID);
/* Change the maximum number of users that can be granted this floor at the same time */
int bfcp_change_number_granted_floor(bfcp_list_floors *lfloors, unsigned short int floorID, unsigned short int limit_granted_floor);
/* Change the current status of a floor */
int bfcp_change_state_floor(bfcp_list_floors *lfloors, unsigned short int floorID, unsigned short int state);
/* Get the current status of a floor */
int bfcp_return_state_floor(bfcp_list_floors *lfloors, unsigned short int floorID);
/* Get the position of this floor in the list */
int bfcp_return_position_floor(bfcp_list_floors *lfloors, unsigned short int floorID);
/* Return the BFCP UserID of the chair of this floor */
unsigned short int bfcp_return_chair_floor(bfcp_list_floors *lfloors, unsigned short int floorID);
/* Check if the chair's BFCP UserID exists (i.e. if it's valid) */
int bfcp_exist_user_as_a_chair(bfcp_list_floors *lfloors, unsigned short int chairID);
/* Remove all floors from a list */
int bfcp_clean_floor_list(bfcp_list_floors *lfloors);
/* Free a linked list of floors */
int bfcp_remove_floor_list(bfcp_list_floors **lfloorsp);


/******************/
/* Helper methods */
/******************/

/* Remove a user from notifications related to floor events */
int remove_request_from_the_floor(bfcp_floors *floors, unsigned short int userID);

#endif
