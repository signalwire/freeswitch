#include "bfcp_user_list.h"

/* From 'bfcp_server.h': Setup and send an Error reply to a participant */
extern int bfcp_error_code(unsigned long int ConferenceID, unsigned short int userID, unsigned short int TransactionID, int code, char *error_info, bfcp_unknown_m_error_details *details, int sockfd, int i, int transport);


/* Create a new linked list of users */
struct bfcp_list_users *bfcp_create_user_list (unsigned short int Max_Number_Floor_Request, unsigned short int Num_floors)
{
	bfcp_list_users *lusers;

	if(Max_Number_Floor_Request <= 0)
		Max_Number_Floor_Request = 1;
	if(Num_floors <= 0)
		Num_floors = 1;

	/* Allocate a new list of users */
	lusers = (bfcp_list_users *)calloc(1, sizeof(bfcp_list_users));

	/* Initialize the list */
	if(lusers == NULL)
		return(NULL);
	else {
		lusers->max_number_floor_request = Max_Number_Floor_Request;
		lusers->maxnumberfloors = --Num_floors;
		lusers->users = NULL;
	}

	return(lusers);
}

/* Check ifparticipant with such userID exists in this list */
int bfcp_existence_user(lusers list_users, unsigned short int userID)
{
	users temp_list_users;

	if(list_users == NULL)
		return -1;
	if(userID <= 0)
		return -1;

	temp_list_users= list_users->users;

	while(temp_list_users && (temp_list_users->userID != userID))
		temp_list_users = temp_list_users->next;

	if(temp_list_users && (temp_list_users->userID == userID))
		/* This user is in the list */
		return 0;
	else
		/* This user is NOT in the list */
		return -1;
}

/* Change the allowed number of per-floor requests for this list */
int bfcp_change_user_req_floors(lusers list_users, unsigned short int Max_Number_Floor_Request)
{
	if(list_users == NULL)
		return -1;

	list_users->max_number_floor_request = Max_Number_Floor_Request;

	return 0;
}

/* Change the allowed number of floors in the conference for this list */
int bfcp_change_number_users_floors(lusers list_users, unsigned short int Num_floors)
{
	if(list_users == NULL)
		return -1;
	if(Num_floors <= 0)
		return -1;

	users user = NULL;
	int *list_floors = NULL, i = 0;

	for(user = list_users->users; user; user= user->next) {
		if(list_users->maxnumberfloors >= Num_floors) {
			for(i = Num_floors; i <= list_users->maxnumberfloors; i++)
				user->numberfloorrequest[i] = 0;
		}

		list_floors = (int *)realloc(user->numberfloorrequest, (Num_floors)*sizeof(int));
		if(list_floors == NULL)
			return -1;

		user->numberfloorrequest= list_floors;

		if(Num_floors > list_users->maxnumberfloors) {
			for(i = list_users->maxnumberfloors+1; i < Num_floors; i++)
				user->numberfloorrequest[i]=0;
		}
	}

	list_users->maxnumberfloors = --Num_floors;

	return 0;
}

/* Add a new user to this list */
int bfcp_add_user(lusers list_users, unsigned short int userID, char *user_URI, char *user_display_name)
{
	if(list_users == NULL)
		return -1;
	if(userID <= 0)
		return -1;

	users ini_user_list = NULL, user = NULL, node_user = NULL;
	int dLen = 0;

	/* First we check if a user with the same ID already exists in the list */
	for(user = list_users->users; user; user = user->next) {
		if(user->userID == userID)
			/* This user already exists in the list */
			return -1;
	}

	/* Create a new user instance */
	node_user = (bfcp_user *)calloc(1, sizeof(bfcp_user));

	/* Initialize the new user */
	if(node_user == NULL)
		return -1;
	else {
		node_user->userID = userID;
		/* Add the CSP URI text */
		if(user_URI != NULL) {
			dLen = strlen(user_URI);
			if(dLen != 0) {
				node_user->user_URI = (char *)calloc(1, dLen*sizeof(char)+1);
				if(node_user->user_URI == NULL)
					return -1;
				memcpy(node_user->user_URI, user_URI, dLen+1);
			}
		} else	/* No CSP URI for this user */
			node_user->user_URI = NULL;

		/* Add the Display Name text */
		if(user_display_name != NULL) {
			dLen = strlen(user_display_name);
			if(dLen != 0) {
				node_user->user_display_name = (char *)calloc(1, dLen*sizeof(char)+1);
				if(node_user->user_display_name == NULL)
					return -1;
				memcpy(node_user->user_display_name, user_display_name, dLen+1);
			}
		} else	/* No Display Name for this user */
			node_user->user_display_name = NULL;

		/* Create a list for floor requests by this user */
		node_user->numberfloorrequest = (int *)calloc(list_users->maxnumberfloors + 1, sizeof(int));
		if(node_user->numberfloorrequest == NULL)
			return -1;

		node_user->client_addr = NULL;
		node_user->fd = 0;

		if(list_users->users == NULL) {
			node_user->next = list_users->users;
			list_users->users = node_user;
		} else if(list_users->users->userID < userID) {
			node_user->next = list_users->users;
			list_users->users = node_user;
		} else {
			ini_user_list = list_users->users;
			while(ini_user_list->next && (ini_user_list->next->userID > userID)) {
				ini_user_list = ini_user_list->next;
			}
			node_user->next = ini_user_list->next;
			ini_user_list->next = node_user;
		}
	}

	return(0);
}

/* Delete an existing user from this list */
int bfcp_delete_user(lusers list_users, unsigned short int userID)
{
	if(list_users == NULL)
		return 0;
	if(userID <= 0)
		return -1;

	users temp_list_users, temp_node_user;

	temp_list_users = list_users->users;
	if(temp_list_users == NULL)
		/* The users' list is empty */
		return 0;
	else {
		if(temp_list_users->userID == userID)
			list_users->users = temp_list_users->next;
		else {
			while((temp_list_users->next != NULL) && (temp_list_users->next->userID != userID))
				temp_list_users = temp_list_users->next;
			if(temp_list_users->next!=NULL) {
				temp_node_user = temp_list_users;
				temp_list_users = temp_list_users->next;
				temp_node_user->next = temp_list_users->next;
			}
		}
	}

	/* This node is not in the list */
	if((temp_list_users == NULL) || (temp_list_users->userID != userID))
		/* This user does not exist in the conference */
		return 0;

	/* Free the user node */
	free(temp_list_users->user_URI);
	temp_list_users->user_URI = NULL;
	free(temp_list_users->user_display_name);
	temp_list_users->user_display_name = NULL;
	free(temp_list_users->numberfloorrequest);
	temp_list_users->numberfloorrequest = NULL;
	free(temp_list_users->client_addr);
	temp_list_users->client_addr = NULL;
	free(temp_list_users);
	temp_list_users = NULL;

	return(0);
}

/* Set address of user in this list */
int bfcp_set_user_address(lusers list_users, unsigned short int userID, char *client_address, unsigned short int client_port)
{
	if (list_users == NULL || userID <= 0) {
		return -1;
	}

	users user;

	/* Need to find user node with same userID */
	for (user = list_users->users; user && (user->userID != userID); user = user->next);

	if (!user) {
		/* user does not present in userlist */
		return -1;
	}

	if (!user->client_addr) {
		 user->client_addr = (struct sockaddr_in *) malloc(sizeof(struct sockaddr_in));
	}

	user->client_addr->sin_addr.s_addr = inet_addr(client_address);
	user->client_addr->sin_port = htons(client_port);
	user->client_addr->sin_family = AF_INET;
	memset(&(user->client_addr->sin_zero), '\0', 8);
	return 0;

}

/* Retrieve address of user from the list */
struct sockaddr_in *bfcp_get_user_address(lusers list_users, unsigned short int userID)
{
	if (list_users == NULL || userID <= 0) {
		return NULL;
	}

	users user;

	/* Need to find user node with same userID */
	for (user = list_users->users; user && (user->userID != userID); user = user->next);

	if (user) {
		/* user present in userlist */
		return user->client_addr;
	}

	return NULL;
}


/* Set socket file descriptor of specific user in the list */
int bfcp_set_user_sockfd(lusers list_users, unsigned short int userID, int sockfd)
{
	if (list_users == NULL || userID <= 0) {
		return -1;
	}

	users user;

	/* Need to find user node with userID */
	for (user = list_users->users; user && (user->userID != userID); user = user->next);

	if (user) {
		user->fd = sockfd; /* sockfd is required later for sending FloorStatus to
							user if floor status changes to granted or released */
		return 0;
	}

	/* user not present in list */
	return -1;
}

/* Add a new FloorRequest to the list of requests made by this user */
int bfcp_add_user_request(unsigned long int ConferenceID, unsigned short int TransactionID, lusers list_users, unsigned short int userID, int position_floor, int sockfd, int y)
{
	if(userID <= 0)
		return -1;
	if(position_floor < 0)
		return -1;
	if(list_users == NULL)
		return -1;

	users temp_list_users;

	if(position_floor >list_users->maxnumberfloors)
		/* This floor is not in the conference */
		return -1;

	temp_list_users= list_users->users;
	while((temp_list_users != NULL) && (temp_list_users->userID != userID))
		temp_list_users = temp_list_users->next;

	/* ifthe node is in the list, print it */
	if((temp_list_users != NULL) && (temp_list_users->userID == userID)) {
		if((temp_list_users->numberfloorrequest[position_floor]) >= list_users->max_number_floor_request) {
			char errortext[200];
			sprintf(errortext, "User %hu has already reached the maximum allowed number of requests (%i) for the same floor in Conference %lu", userID, list_users->max_number_floor_request, ConferenceID);
			bfcp_error_code(ConferenceID, userID, TransactionID, BFCP_MAX_FLOORREQUESTS_REACHED, errortext, NULL, sockfd, y, list_users->bfcp_transport);
			return -1;
		} else
			temp_list_users->numberfloorrequest[position_floor] = temp_list_users->numberfloorrequest[position_floor]+1;
	} else
		/* This user is not in the list */
		return -1;

	return 0;
}

/* Check ifthis user has already reached the max number of requests that can be made for this floor */
int bfcp_is_floor_request_full(unsigned long int ConferenceID, unsigned short int TransactionID, lusers list_users, unsigned short int userID, int position_floor, int sockfd, int y)
{
	if(userID <= 0)
		return -1;
	if(position_floor < 0)
		return -1;
	if(list_users == NULL)
		return -1;

	users temp_list_users;

	if(position_floor > list_users->maxnumberfloors)
		/* This floor is not in the conference */
		return -1;

	temp_list_users = list_users->users;
	if(temp_list_users==NULL)
		return -1;

	while((temp_list_users != NULL) && (temp_list_users->userID != userID))
		temp_list_users = temp_list_users->next;

	/* ifthe node is in the list, print it */
	if((temp_list_users != NULL) && (temp_list_users->userID == userID)) {
		if(temp_list_users->numberfloorrequest[position_floor] >= list_users->max_number_floor_request) {
			char errortext[200];
			sprintf(errortext, "User %hu has already reached the maximum allowed number of requests (%i) for the same floor in Conference %lu", userID, list_users->max_number_floor_request, ConferenceID);
			bfcp_error_code(ConferenceID, userID, TransactionID, BFCP_MAX_FLOORREQUESTS_REACHED, errortext, NULL, sockfd, y, list_users->bfcp_transport);
			return -1;
		}
	}

	return(0);
}

/* Clean all requests made by all users of this list regarding a specific floor */
int bfcp_clean_all_users_request_of_a_floor(lusers list_users, unsigned short int floorID)
{
	if(list_users == NULL)
		return -1;
	if(floorID <= 0)
		return -1;

	users temp_list_users;

	if(floorID > list_users->maxnumberfloors+1)
		/* This floor is not in the conference */
		return -1;

	temp_list_users = list_users->users;

	for(temp_list_users = list_users->users; temp_list_users; temp_list_users = temp_list_users->next)
		temp_list_users->numberfloorrequest[floorID-1] = 0;

	return(0);
}

/* Stop handling a specific floor in this list and remove it */
int bfcp_delete_a_floor_from_user_list(lusers list_users, unsigned short int floorID)
{
	if(list_users == NULL)
		return -1;

	users temp_list_users;
	int i;
	int *list_floors;

	if(floorID>list_users->maxnumberfloors+1)
		/* This floor is not in the conference */
		return -1;

	temp_list_users = list_users->users;

	for(temp_list_users = list_users->users; temp_list_users; temp_list_users = temp_list_users->next) {
		for(i = floorID + 1; i < list_users->maxnumberfloors; i++)
			temp_list_users->numberfloorrequest[i-1] = temp_list_users->numberfloorrequest[i];
		temp_list_users->numberfloorrequest[i-1] = 0;

		list_floors = (int *)realloc(temp_list_users->numberfloorrequest, (list_users->maxnumberfloors+1)*sizeof(int));
		if(list_floors == NULL)
			return -1;
	}

	list_users->maxnumberfloors = list_users->maxnumberfloors - 1;

	return(0);
}

/* Decrease the number of requests made by a user for a floor */
int bfcp_deleted_user_request(lusers list_users, unsigned short int userID, int position_floor)
{
	if(list_users == NULL)
		return -1;
	if(userID <= 0)
		return -1;
	if(position_floor < 0)
		return -1;

	users temp_list_users;

	if(position_floor>list_users->maxnumberfloors)
		/* This floor is not in this list */
		return -1;

	temp_list_users = list_users->users;

	while(temp_list_users && (temp_list_users->userID != userID))
		temp_list_users = temp_list_users->next;

	if(temp_list_users && (temp_list_users->userID == userID)) {
		if(temp_list_users->numberfloorrequest[position_floor] > 0)
			temp_list_users->numberfloorrequest[position_floor] = temp_list_users->numberfloorrequest[position_floor] -1;
	} else
		/* This user is not in the list */
		return -1;

	return(0);
}

/* Remove all users from a list */
int bfcp_clean_user_list(lusers list_users)
{
	if(list_users == NULL)
		return -1;

	users temp_list_users, temp;

	temp_list_users= list_users->users;
	if(temp_list_users == NULL)
		return 0;

	while(temp_list_users) {
		temp = temp_list_users;
		temp_list_users = temp_list_users->next;
		free(temp->user_URI);
		temp->user_URI = NULL;
		free(temp->user_display_name);
		temp->user_display_name = NULL;
		free(temp->numberfloorrequest);
		temp->numberfloorrequest = NULL;
		free(temp);
		temp = NULL;
	}

	list_users->users=NULL;
	return(0);
}

/* Free a linked list of users */
int bfcp_remove_user_list(bfcp_list_users **lusers_p)
{
	if(lusers_p == NULL)
		return 0;

	int error;
	bfcp_list_users *lusers = *lusers_p;

	error = bfcp_clean_user_list(lusers);

	if(error == 0) {
		free(lusers);
		lusers = NULL;
		*lusers_p = NULL;
	} else
		return -1;

	return(0);

}

/* Convert a 'bfcp_user' info to 'bfcp_user_information' (bfcp_messages.h) */
bfcp_user_information *bfcp_show_user_information(lusers list_users, unsigned short int userID)
{
	if(list_users == NULL)
		return NULL;
	if(userID <= 0)
		return NULL;

	users temp_list_users;
	bfcp_user_information *user_info;

	temp_list_users = list_users->users;

	while(temp_list_users && temp_list_users->userID!= userID)
		temp_list_users = temp_list_users->next;

	/* ifthe node is in the list, convert it to a 'bfcp_user_information' */
	if(temp_list_users && (temp_list_users->userID == userID))
		user_info = bfcp_new_user_information(temp_list_users->userID, temp_list_users->user_display_name, temp_list_users->user_URI);
	else
		/* This user is not in the list */
		return NULL;

	return user_info;
}

/* Get the CSP URI for this BFCP UserID */
char *bfcp_obtain_userURI(lusers list_users, unsigned short int userID)
{
	if(userID <= 0)
		return NULL;
	if(list_users == NULL)
		return NULL;

	users temp_list_users;

	temp_list_users= list_users->users;
	while((temp_list_users != NULL) && (temp_list_users->userID != userID))
		temp_list_users = temp_list_users->next;

	/* ifthe node is in the list, return the URI information */
	if((temp_list_users != NULL) && (temp_list_users->userID == userID))
		return temp_list_users->user_URI;
	else
		return NULL;
}

/* Get the Display Name for this BFCP UserID */
char *bfcp_obtain_user_display_name(lusers list_users, unsigned short int userID)
{
	if(userID <= 0)
		return NULL;
	if(list_users == NULL)
		return NULL;

	users temp_list_users;

	temp_list_users= list_users->users;
	while((temp_list_users != NULL) && (temp_list_users->userID != userID))
		temp_list_users = temp_list_users->next;

	if((temp_list_users != NULL) && (temp_list_users->userID == userID))
		return temp_list_users->user_display_name;
	else
		return NULL;
}
