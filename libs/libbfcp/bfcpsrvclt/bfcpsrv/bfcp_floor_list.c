#include "bfcp_floor_list.h"

/* Create a new linked list of floors */
struct bfcp_list_floors *bfcp_create_floor_list(unsigned short int Max_Num)
{
	bfcp_list_floors * lfloors;
	bfcp_floors *floors;

	if(Max_Num == 0)
		Max_Num = 1;

	/* Allocate a new container for the list of floors */
	lfloors = (bfcp_list_floors *)calloc(1, sizeof(bfcp_list_floors));
	if(lfloors == NULL)
		return NULL;

	/* Allocate a new array of floors */
	floors = (bfcp_floors *)calloc(Max_Num, sizeof(bfcp_floors));
	if(floors == NULL) {
		free(lfloors);
		lfloors = NULL;
		return NULL;
	} else {
		lfloors->number_floors = --Max_Num;
		lfloors->actual_number_floors = 0;
		lfloors->floors = floors;
	}

	return lfloors;
}

/* Add a floor (and its chair, if present) to a list of floors */
int bfcp_insert_floor(bfcp_list_floors *lfloors, unsigned short int floorID, unsigned short int chairID)
{
	if(lfloors == NULL)
		return -1;
	if(floorID == 0)
		return -1;

	int number_floors, i = 0;

	while((i < lfloors->actual_number_floors) && (lfloors->floors[i].floorID != floorID))
		i = i + 1;

	if((i < lfloors->actual_number_floors) && (lfloors->floors[i].floorID == floorID))
		/* A floor with the same floorID already exists in this conference */
		return -1;

	number_floors = lfloors->number_floors;
	if(lfloors->actual_number_floors > number_floors)
		/* The maximum number of allowed floors has already been reached */
		return -1;

	/* Initialize the floor information */
	lfloors->floors[lfloors->actual_number_floors].floorID = floorID;
	lfloors->floors[lfloors->actual_number_floors].chairID = chairID;
	lfloors->floors[lfloors->actual_number_floors].floorState = 0;
	lfloors->floors[lfloors->actual_number_floors].limit_granted_floor = 2;
	lfloors->floors[lfloors->actual_number_floors].floorquery = NULL;
	lfloors->actual_number_floors= lfloors->actual_number_floors + 1;

	return lfloors->actual_number_floors;
}

/* Get the number of currently available floors in a list */
int bfcp_return_number_floors(bfcp_list_floors *lfloors)
{
	if(lfloors == NULL)
		return -1;

	return lfloors->number_floors;
}

/* Change the maximum number of allowed floors in a conference */
int bfcp_change_number_floors(bfcp_list_floors *lfloors, unsigned short int Num)
{
	if(lfloors == NULL)
		return -1;

	if(Num == 0)
		Num = 1;

	bfcp_floors *floors = NULL;
	floor_query *query = NULL;
	floor_query *temp = NULL;
	int i = 0;

	if(lfloors->number_floors >= Num) {
		for(i = Num;i <= lfloors->number_floors; i++) {
			lfloors->floors[i].floorID = 0;
			lfloors->floors[i].chairID = 0;
			lfloors->floors[i].floorState = 0;
			lfloors->floors[i].limit_granted_floor = 0;

			/* Free the list of floor queries */
			query = lfloors->floors[i].floorquery;
			while(query) {
				temp = query;
				query = query->next;
				free(temp);
				temp = NULL;
			}
		}
	}

	floors = (bfcp_floors *)realloc(lfloors->floors, Num*sizeof(bfcp_floors));
	if(floors == NULL)
		return -1;

	lfloors->floors = floors;

	if(Num > lfloors->number_floors) {
		for(i = lfloors->number_floors+1; i < Num; i++) {
			lfloors->floors[i].floorID = 0;
			lfloors->floors[i].chairID = 0;
			lfloors->floors[i].floorState = 0;
			lfloors->floors[i].limit_granted_floor = 0;

			/* Free the list of floor queries*/
			query = lfloors->floors[i].floorquery;
			while(query) {
				temp = query;
				query = query->next;
				free(temp);
				temp = NULL;
			}
		}
	}

	if((lfloors->actual_number_floors) > Num)
		lfloors->actual_number_floors = Num;

	lfloors->number_floors = --Num;

	return 0;
}

/* Check if a floor exists in a conference */
int bfcp_exist_floor(bfcp_list_floors *lfloors, unsigned short int floorID)
{
	if(lfloors == NULL)
		return -1;
	if(floorID == 0)
		return -1;

	int i = 0;

	while((i < lfloors->actual_number_floors) && (lfloors->floors[i].floorID != floorID))
		i = i + 1;

	if((i < lfloors->actual_number_floors) && (lfloors->floors[i].floorID == floorID))
		/* The floor exists */
		return 0;
	else
		/* The floor does NOT exist */
		return -1;
}

/* Remove a floor from a floors list */
int bfcp_delete_floor(bfcp_list_floors *lfloors, unsigned short int floorID)
{
	if(floorID == 0)
		return -1;
	if(lfloors == NULL)
		return -1;

	int i = 0;
	floor_query *query = NULL;
	floor_query *temp = NULL;

	while(i < lfloors->actual_number_floors && (lfloors->floors[i].floorID != floorID))
		i = i + 1;

	if(i >= lfloors->actual_number_floors)
		/* This floor does not exist in this conference */
		return -1;

	i = i + 1;
	while(i < lfloors->actual_number_floors) {
		/* Reorder the whole list */
		lfloors->floors[i-1].floorID = lfloors->floors[i].floorID;
		lfloors->floors[i-1].chairID = lfloors->floors[i].chairID;
		lfloors->floors[i-1].floorState = lfloors->floors[i].floorState;
		lfloors->floors[i-1].limit_granted_floor = lfloors->floors[i].limit_granted_floor;
		lfloors->floors[i-1].floorquery = lfloors->floors[i].floorquery;
		i++;
	}

	lfloors->floors[i-1].floorID = 0;
	lfloors->floors[i-1].chairID = 0;
	lfloors->floors[i-1].floorState = 0;
	lfloors->floors[i-1].limit_granted_floor = 0;

	query = lfloors->floors[i-1].floorquery;
	while(query) {
		temp = query;
		query = query->next;
		free(temp);
		temp = NULL;
	}

	lfloors->actual_number_floors = lfloors->actual_number_floors - 1;

	return lfloors->actual_number_floors;
}

/* Change the chair's userID for a floor (setting chair to 0 removes it) */
int bfcp_change_chair(bfcp_list_floors *lfloors, unsigned short int floorID, unsigned short int chairID)
{
	if(floorID == 0)
		return -1;
	if(lfloors == NULL)
		return -1;

	int i = 0;

	while((i < lfloors->actual_number_floors) && (lfloors->floors[i].floorID != floorID))
		i = i + 1;

	if(i >= lfloors->actual_number_floors)
		/* A floor with this floorID does not exists in this conference */
		return -1;

	lfloors->floors[i].chairID = chairID;

	return 0;
}

/* Get the maximum number of users that can be granted this floor at the same time */
int bfcp_return_number_granted_floor(bfcp_list_floors *lfloors, unsigned short int floorID)
{
	if(lfloors == NULL)
		return -1;
	if(floorID == 0)
		return -1;

	int i = 0;
	while((i < lfloors->actual_number_floors) && (lfloors->floors[i].floorID != floorID))
		i = i + 1;

	if(i >= lfloors->actual_number_floors)
		/* A floor with this floorID does not exists in this conference */
		return -1;

	return lfloors->floors[i].limit_granted_floor;
}

/* Change the maximum number of users that can be granted this floor at the same time */
int bfcp_change_number_granted_floor(bfcp_list_floors *lfloors, unsigned short int floorID, unsigned short int limit_granted_floor)
{
	if(floorID == 0)
		return -1;
	if(lfloors == NULL)
		return -1;

	int i = 0;

	while((i < lfloors->actual_number_floors) && (lfloors->floors[i].floorID != floorID))
		i = i + 1;

	if(i >= lfloors->actual_number_floors)
		/* A floor with this floorID does not exists in this conference */
		return -1;

	if(limit_granted_floor == 0)
		lfloors->floors[i].limit_granted_floor = USHRT_MAX;
	else
		lfloors->floors[i].limit_granted_floor = (1 + limit_granted_floor);

	return 0;
}

/* Change the current status of a floor */
int bfcp_change_state_floor(bfcp_list_floors *lfloors, unsigned short int floorID, unsigned short int state)
{
	int i = 0;

	if(floorID == 0)
		return -1;
	if(lfloors == NULL)
		return -1;
	if((state != BFCP_FLOOR_STATE_WAITING) && (state != BFCP_FLOOR_STATE_ACCEPTED))
		return -1;

	while((i < lfloors->actual_number_floors) && (lfloors->floors[i].floorID != floorID))
		i = i + 1;

	if(i >= lfloors->actual_number_floors)
		/* A floor with this floorID does not exists in this conference */
		return -1;

	if(state == BFCP_FLOOR_STATE_ACCEPTED) {
		/* Decrease the state */
		lfloors->floors[i].floorState = lfloors->floors[i].floorState - 1;
		if(lfloors->floors[i].floorState == BFCP_FLOOR_STATE_ACCEPTED)
			lfloors->floors[i].floorState = BFCP_FLOOR_STATE_WAITING;
	} else {
		/* Increase the state */
		lfloors->floors[i].floorState = lfloors->floors[i].floorState + 1;
		if(lfloors->floors[i].floorState == BFCP_FLOOR_STATE_ACCEPTED)
			lfloors->floors[i].floorState = BFCP_FLOOR_STATE_GRANTED;
	}

	return 0;
}

/* Get the current status of a floor */
int bfcp_return_state_floor(bfcp_list_floors *lfloors, unsigned short int floorID)
{
	int i = 0;

	if(floorID == 0)
		return -1;
	if(lfloors == NULL)
		return -1;
	if(lfloors->floors == NULL)
		return -1;

	while((i < lfloors->actual_number_floors) && (lfloors->floors[i].floorID != floorID))
		i = i + 1;

	if(i >= lfloors->actual_number_floors)
		/* A floor with this floorID does not exists in this conference */
		return -1;

	return lfloors->floors[i].floorState;
}

/* Get the position of this floor in the list */
int bfcp_return_position_floor(bfcp_list_floors *lfloors, unsigned short int floorID)
{
	if(floorID == 0)
		return -1;
	if(lfloors == NULL)
		return -1;

	int i = 0;

	while((i < lfloors->actual_number_floors) && (lfloors->floors[i].floorID != floorID))
		i = i + 1;

	if(i >= lfloors->actual_number_floors)
		/* A floor with this floorID does not exists in this conference */
		return -1;

	return i;
}

/* Return the BFCP UserID of the chair of this floor */
unsigned short int bfcp_return_chair_floor(bfcp_list_floors *lfloors, unsigned short int floorID)
{
	if(floorID == 0)
		return 0;
	if(lfloors == NULL)
		return 0;

	int i = 0;

	while((i < lfloors->actual_number_floors) && (lfloors->floors[i].floorID != floorID))
		i = i + 1;

	if(i >= lfloors->actual_number_floors)
	  	/* A floor with this floorID does not exists in this conference */
		  return 0;

    return lfloors->floors[i].chairID;
}

/* Check if the chair's BFCP UserID exists (i.e. if it's valid) */
int bfcp_exist_user_as_a_chair(bfcp_list_floors *lfloors, unsigned short int chairID)
{
	if(lfloors == NULL)
		return -1;

	int i;

	for(i = 0; i < lfloors->actual_number_floors; i++) {
		if(lfloors->floors[i].chairID == chairID)
			return 0;
	}

	/* If we arrived so far, che chair is not valid user */
	return -1;
}

/* Remove all floors from a list */
int bfcp_clean_floor_list(bfcp_list_floors *lfloors)
{
	if(lfloors == NULL)
		return -1;

	floor_query *query = NULL;
	floor_query *temp = NULL;;
	int i;

	for(i = 0; i < lfloors->actual_number_floors; i++) {
		lfloors->floors[i].floorID = 0;
		lfloors->floors[i].chairID = 0;
		lfloors->floors[i].floorState = 0;
		lfloors->floors[i].limit_granted_floor = 0;

		query = lfloors->floors[i].floorquery;
		while(query) {
			temp = query;
			query = query->next;
			free(temp);
			temp = NULL;
		}
	}

	lfloors->actual_number_floors = 0;

	return 0;
}

/* Free a linked list of floors */
int bfcp_remove_floor_list(bfcp_list_floors **lfloorsp)
{
	if(lfloorsp == NULL)
		return 0;

	int error;

	bfcp_list_floors *lfloors = *lfloorsp;

	error = bfcp_clean_floor_list(lfloors);
	if(error == 0) {
		free(lfloors->floors);
		lfloors->floors = NULL;
		free(lfloors);
		*lfloorsp = NULL;
	} else
		return -1;

	return 0;
}

/* Remove a user from notifications related to floor events */
int remove_request_from_the_floor(bfcp_floors *floors, unsigned short int userID)
{
	if(floors == NULL)
		return 0;

	floor_query *floorquery = NULL;
	floor_query *newnode = NULL;

	floorquery = floors->floorquery;
	if(floorquery == NULL)
		return 0;

	if(floorquery->userID == userID) {
		/* If this user exists in the request list, we remove it from the node */
		floors->floorquery = floorquery->next;
		floorquery->next = NULL;
		free(floorquery);
		floorquery = NULL;
		return 0;
	}

	while(floorquery->next != NULL) {
		if(floorquery->next->userID == userID) {
			newnode= floorquery->next;
			floorquery->next = newnode->next;
			newnode->next = NULL;
			free(newnode);
			newnode = NULL;
			return 0;
		}
		floorquery = floorquery->next;
	}

	return 0;
}
