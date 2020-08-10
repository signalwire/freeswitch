#ifndef _BFCP_USER_LIST_H
#define _BFCP_USER_LIST_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Needed for TCP/TLS/BFCP support */
#include <openssl/rsa.h>
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/pem.h>

/* The BFCP messages building/parsing library */
#include <bfcp_messages.h>

enum chair_type {NO_CHAIR = 0, CHAIR};

/* BFCP Participant (User) */
typedef struct bfcp_user {
	unsigned short int userID;		/* UserID */
	char *user_URI;			/* Call Signaling Protocol URI */
	char *user_display_name;	/* User-friendly Display Name */
	int *numberfloorrequest;	/* Number of floor requests made by this user for each floor in the conference */
	struct sockaddr_in *client_addr;/* Socket address of participant, used in case of UDP socket*/
	int fd;				/* socket fd of client*/
	struct bfcp_user *next;		/* Next user in the list */
} bfcp_user;
/* Pointer to a specific user */
typedef bfcp_user *users;

/* List of participants to the conference */
typedef struct bfcp_list_users {
	int bfcp_transport;		/* BFCP/TCP (0) or BFCP/TLS (1) */
	unsigned short int max_number_floor_request;	/* Policy regarding the max allowed number of requests each user can make for a specific floor */
	unsigned short int maxnumberfloors;		/* The max number of allowed floors in this conference */
	struct bfcp_user *users;	/* The linked list of all users in the conference */
} bfcp_list_users;
/* Pointer to a specific list of users */
typedef bfcp_list_users *lusers;


/************************/
/* User-related methods */
/************************/

/* Create a new linked list of users */
struct bfcp_list_users *bfcp_create_user_list (unsigned short int Max_Number_Floor_Request, unsigned short int Num_floors);
/* Check if participant with such userID exists in this list */
int bfcp_existence_user(lusers list_users, unsigned short int userID);
/* Change the allowed number of per-floor requests for this list */
int bfcp_change_user_req_floors(lusers list_users, unsigned short int Max_Number_Floor_Request);
/* Change the allowed number of floors in the conference for this list */
int bfcp_change_number_users_floors(lusers list_users, unsigned short int Num_floors);
/* Add a new user to this list */
int bfcp_add_user(lusers list_users, unsigned short int userID, char *user_URI, char *user_display_name);

/*!
 \brief Set address of user in this list
 \param list_users Pointer to a specific list of users
 \param userID BFCP userID
 \param client_address char pointer to IP addrss of user
 \param client_port Port at which user is listening
 \return 0, If client_address successfully set else return -1
 */
int bfcp_set_user_address(lusers list_users, unsigned short int userID, char *client_address, unsigned short int client_port);

/*!
 \brief Retrieve address of user
 \param list_users Pointer to a specific list of users
 \param userID BFCP userID
 \return Socket address of user of type struct sockaddr_in*
 */
struct sockaddr_in *bfcp_get_user_address(lusers list_users, unsigned short int userID);

/*!
 \brief Set socket file descriptor to user list
 \param list_users Pointer to a specific list of users
 \param userID BFCP userID
 \param sockfd File descriptor of user
 \return 0, If successfully set else return -1
 */
int bfcp_set_user_sockfd(lusers list_users, unsigned short int userID, int sockfd);

/* Delete an existing user from this list */
int bfcp_delete_user(lusers list_users, unsigned short int userID);
/* Add a new FloorRequest to the list of requests made by this user */
int bfcp_add_user_request(unsigned long int ConferenceID, unsigned short int TransactionID, lusers list_users, unsigned short int userID, int position_floor, int sockfd, int y);
/* Check if this user has already reached the max number of requests that can be made for this floor */
int bfcp_is_floor_request_full(unsigned long int ConferenceID, unsigned short int TransactionID, lusers list_users, unsigned short int userID, int position_floor, int sockfd, int y);
/* Clean all requests made by all users of this list regarding a specific floor */
int bfcp_clean_all_users_request_of_a_floor(lusers list_users, unsigned short int floorID);
/* Stop handling a specific floor in this list and remove it */
int bfcp_delete_a_floor_from_user_list(lusers list_users, unsigned short int floorID);
/* Decrease the number of requests made by a user for a floor */
int bfcp_deleted_user_request(lusers list_users, unsigned short int userID, int position_floor);
/* Remove all users from a list */
int bfcp_clean_user_list(lusers list_users);
/* Free a linked list of users */
int bfcp_remove_user_list(bfcp_list_users **lusers_p);
/* Convert a 'bfcp_user' info to 'bfcp_user_information' (bfcp_messages.h) */
bfcp_user_information *bfcp_show_user_information(lusers list_users, unsigned short int userID);


/******************/
/* Helper methods */
/******************/

/* Get the CSP URI for this BFCP UserID */
char *bfcp_obtain_userURI(lusers list_users, unsigned short int userID);
/* Get the Display Name for this BFCP UserID */
char *bfcp_obtain_user_display_name(lusers list_users, unsigned short int userID);

#endif
