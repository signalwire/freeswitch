#include "bfcp_link_list.h"

/* Create a new linked list of BFCP queues */
struct bfcp_queue *bfcp_create_list(void)
{
	bfcp_queue *conference;

	/* Create a new list of BFCP queues */
	conference = (bfcp_queue *)calloc(1, sizeof(bfcp_queue));

	/* Initialize the list */
	if(conference == NULL)
		return NULL;
	else {
		conference->head = NULL;
		conference->tail = NULL;
	}

	return conference;
}

/* Initialize a new FloorRequest node */
struct bfcp_node *bfcp_init_request(unsigned short int userID, unsigned short int beneficiaryID, int priority, char *participant_info, unsigned short int floorID)
{
	if(userID == 0)
		return NULL;
	if(floorID == 0)
		return NULL;

	pnode newnode;
	pfloor floor_list;
	int dLen;

	/* Check if the priority argument is valid */
	if(priority > BFCP_HIGHEST_PRIORITY)
		priority = BFCP_HIGHEST_PRIORITY;

	/* Create a new node */
	newnode = (pnode)calloc(1, sizeof(bfcp_node));
	if(!newnode)
		return (NULL);

	newnode->next = newnode->prev= NULL;

	newnode->userID = userID;
	newnode->beneficiaryID = beneficiaryID;
	newnode->priority = priority;
	newnode->queue_position = 0;
	newnode->chair_info = NULL;
	newnode->floorrequest = NULL;

	/* Add participant-provided text, if present */
	if(participant_info != NULL) {
		dLen= strlen(participant_info);
		if(dLen != 0) {
			newnode->participant_info = (char *)calloc(1, dLen*sizeof(char)+1);
			if(newnode->participant_info == NULL)
				return NULL;
			memcpy(newnode->participant_info, participant_info, dLen+1);
		}
	} else
		newnode->participant_info= NULL;

	/* Create a new list of floors */
	floor_list = (pfloor)calloc(1, sizeof(bfcp_floor));

	/* Initialize the list */
	if(floor_list == NULL)
		return NULL;
	else{
		floor_list->floorID = floorID;
		floor_list->status = BFCP_FLOOR_STATE_WAITING;
		floor_list->chair_info = NULL;
#ifndef WIN32
		floor_list->pID = 0;
#else
		floor_list->pID.p = NULL;
#endif
		floor_list->next = NULL;
		newnode->floor = floor_list;
	}

	return newnode;
}

/* Add a floor to an existing FloorRequest */
int bfcp_add_floor_to_request(pnode newnode, unsigned short int floorID)
{
	if(newnode == NULL)
		return -1;
	if(floorID == 0)
		return -1;

	pfloor floor_list, ini_floor_list, floor;

	/* Check if a floor with the same ID already exists in the list */
	for(floor = newnode->floor; floor; floor = floor->next) {
		if(floor->floorID == floorID)
			/* This floor already exists in the list */
			return -1;
	}

	/* Create a new list of floors */
	floor_list = (pfloor)calloc(1, sizeof(bfcp_floor));

	/* Initialize the list */
	if(floor_list == NULL)
		return -1;
	else {
		if(newnode->floor == NULL) {
			floor_list->floorID = floorID;
			floor_list->status = BFCP_FLOOR_STATE_WAITING;
			floor_list->chair_info = NULL;
#ifndef WIN32
			floor_list->pID = 0;
#else
			floor_list->pID.p = NULL;
#endif
			floor_list->next= newnode->floor;
			newnode->floor= floor_list;
		} else if(newnode->floor->floorID < floorID) {
			floor_list->floorID = floorID;
			floor_list->status = BFCP_FLOOR_STATE_WAITING;
			floor_list->chair_info = NULL;
#ifndef WIN32
			floor_list->pID = 0;
#else
			floor_list->pID.p = NULL;
#endif
			floor_list->next = newnode->floor;
			newnode->floor = floor_list;
		} else {
			ini_floor_list = newnode->floor;
			while((ini_floor_list->next) && (ini_floor_list->next->floorID > floorID))
				ini_floor_list=ini_floor_list->next;
			floor_list->floorID = floorID;
			floor_list->status = BFCP_FLOOR_STATE_WAITING;
			floor_list->chair_info = NULL;
#ifndef WIN32
			floor_list->pID = 0;
#else
			floor_list->pID.p=NULL;
#endif
			floor_list->next = ini_floor_list->next;
			ini_floor_list->next = floor_list;
		}
	}

	return 0;
}

/* Add an existing FloorRequest to a BFCP queue */
int bfcp_insert_request(bfcp_queue *conference, pnode newnode, unsigned short int floorRequestID, char *chair_info)
{
	if(conference == NULL)
		return -1;
	if(newnode == NULL)
		return -1;
	if(floorRequestID == 0)
		return -1;

	pnode traverse;
	int dLen, y = 1;

	/* Add the floorRequestID to the node */
	newnode->floorRequestID = floorRequestID;

	if(chair_info != NULL) {
		dLen = strlen(chair_info);
		if(dLen != 0) {
			if(newnode->chair_info != NULL) {
				free(newnode->chair_info);
				newnode->chair_info = NULL;
			}
			newnode->chair_info = (char *)calloc(1, dLen*sizeof(char)+1);
			if(newnode->chair_info == NULL)
				return -1;
			memcpy(newnode->chair_info, chair_info, dLen+1);
		}
	} else
		newnode->chair_info = NULL;

	/* Insert the new node in the structure*/
	traverse = conference->head;

	/* If queue_position is not 0, we set the one the chair stated */
	if((newnode->queue_position != 0) && (traverse != NULL)) {
		/* Insert the new node in the structure*/
		traverse = conference->tail;
		while(traverse->prev) {
			if(y < newnode->queue_position)
				y = y + 1;
			else
				break;
			traverse = traverse->prev;
		}

		if(y != newnode->queue_position) {
			traverse = conference->head;
			newnode->queue_position = 0;
		}
		newnode->priority = 0;
	}

	if(traverse == NULL) {
		newnode->next = NULL;
		newnode->prev = NULL;
		conference->head = newnode;
		conference->tail = newnode;
	} else {
		if((traverse->priority >= newnode->priority) && (newnode->queue_position == 0)) {
			newnode->next = traverse;
			newnode->prev = NULL;
			conference->head = newnode;
			if(traverse)
				traverse->prev = newnode;
			if(!conference->head)
				conference->head = newnode;
			if(!conference->tail)
				conference->tail = newnode;
		} else {
			while(traverse->next && (traverse->next->priority < newnode->priority))
				traverse=traverse->next;

			newnode->next = traverse->next;
			traverse->next = newnode;
			newnode->prev = traverse;
			if(newnode->next)
				newnode->next->prev = newnode;
			else
				conference->tail = newnode;
		}
	}

	return ++floorRequestID;
}

/* Return the UserID of the user who made the request */
unsigned short int bfcp_give_user_of_request(bfcp_queue *conference, unsigned short int floorRequestID)
{
	if(conference == NULL)
		return 0;
	if(floorRequestID == 0)
		return 0;

	pnode traverse;
	traverse = conference->head;

	while(traverse && (traverse->floorRequestID != floorRequestID))
		traverse = traverse->next;

	if(!traverse || traverse->floorRequestID!= floorRequestID)
		/* This node is not in the list */
		return 0;
	else {
		if(traverse->beneficiaryID != 0)
			return(traverse->beneficiaryID);
		else
			return(traverse->userID);
	}
}

/* Return FoorRequestID, if user with userID exist in conference list,
   this API is basically called when user sends FloorRelease message with FloorRequestID 0 */
unsigned short int bfcp_get_floor_requestid(bfcp_queue *conference, unsigned short int userID)
{
	if (conference == NULL || userID == 0) {
		return 0;
	}

	pnode traverse;

	for (traverse = conference->head; traverse && (traverse->userID != userID); traverse = traverse->next);

	if (traverse) {
		return traverse->floorRequestID;
	}

	return 0;
}

/* Change status to a floor in a request */
int bfcp_change_status(bfcp_queue *conference, unsigned short int floorID, unsigned short int floorRequestID, int status, char *floor_chair_info)
{
	if(conference == NULL)
		return -1;
	if(floorRequestID == 0)
		return -1;
	if((status < BFCP_FLOOR_STATE_WAITING) || (status > BFCP_FLOOR_STATE_GRANTED))
		return -1;

	pnode traverse;
	pfloor floor;
	int dLen;

	traverse = conference->head;

	while(traverse && (traverse->floorRequestID != floorRequestID))
		traverse = traverse->next;

	if(!traverse || (traverse->floorRequestID != floorRequestID))
		/* This node is not in the list */
		return -1;

	floor = traverse->floor;
	while(floor) {
		if(floor->floorID == floorID) {
			floor->status = status;
			/* If there's chair-provided text, add it */
			if(floor_chair_info != NULL) {
				dLen = strlen(floor_chair_info);
				if(dLen != 0) {
					if(floor->chair_info != NULL) {
						free(floor->chair_info);
						floor->chair_info = NULL;
					}
					floor->chair_info = (char *)calloc(1, dLen*sizeof(char)+1);
					if(floor->chair_info == NULL)
						return -1;
					memcpy(floor->chair_info, floor_chair_info, dLen+1);
				}
			} else
			floor->chair_info = NULL;
			return 0;
		}
		floor = floor->next;
	}

	/* If we arrived so far, some error happened */
	return -1;

}

/* Check if the overall status is actually the status of every floor in the request (?) */
int bfcp_all_floor_status(bfcp_queue *conference, unsigned short int floorRequestID, int status)
{
	if(conference == NULL)
		return -1;
	if(floorRequestID == 0)
		return -1;
	if((status < BFCP_FLOOR_STATE_WAITING) || (status > BFCP_FLOOR_STATE_GRANTED))
		return -1;

	pnode traverse;
	pfloor floor;

	traverse = conference->head;

	while(traverse && (traverse->floorRequestID != floorRequestID))
		traverse = traverse->next;

	if(!traverse || (traverse->floorRequestID != floorRequestID))
		/* This node is not in the list */
		return -1;

	floor = traverse->floor;
	while(floor) {
		if(floor->status != status)
			return -1;
		floor = floor->next;
	}

	return 0;
}

/* Check if the pthread identifier is valid (?) */
int bfcp_all_floor_pID(pnode newnode)
{
	if(newnode == NULL)
		return 0;

	pfloor floor;

	floor = newnode->floor;

	while(floor) {
#ifndef WIN32
		if(floor->pID!=0)
#else
		if(floor->pID.p!=NULL)
#endif
			return -1;
		floor=floor->next;
	}

	return 0;
}

/* Remove a floor from a floor request information (?) */
int bfcp_delete_node_with_floorID(unsigned long int conferenceID, bfcp_queue *accepted, bfcp_queue *conference, unsigned short int floorID, bfcp_list_floors *lfloors, int type_list)
{
	if(conference == NULL)
		return -1;
	if(floorID == 0)
		return -1;

	pnode traverse, traverse_temp;
	pfloor floor, temp, next;
	floor_request_query *temp_request;
	floor_request_query *next_request;
	floor_request_query *floorrequest;
	int delete_node = 0, i = 0;
	unsigned short int userID;

	traverse = conference->head;

	while(traverse) {
		floor = traverse->floor;
		delete_node = 0;
		while((floor != NULL) && (floor->floorID >= floorID)) {
			if(floor->floorID == floorID) {
				/* The head pointer points to the node we want to delete */
				traverse_temp = traverse;

				if(traverse_temp->beneficiaryID !=0)
					userID = traverse_temp->beneficiaryID;
				else
					userID= traverse_temp->userID;

				floorrequest = traverse_temp->floorrequest;

				if(traverse_temp == conference->head) {
					conference->head = traverse_temp->next;
					traverse = conference->head;
					if(traverse != NULL) {
						traverse->prev = NULL;
						if(traverse->next == NULL)
							conference->tail = traverse;
					} else
						conference->tail = NULL;
				} else {
					if(traverse_temp->prev) {
						/* It is not the first element */
						traverse = traverse_temp->prev;
						traverse_temp->prev->next = traverse_temp->next;
					}
					if(traverse_temp->next) {
						/* It is not the last element */
						traverse = traverse_temp->prev;
						traverse_temp->next->prev = traverse_temp->prev;
					} else {
						traverse = NULL;
						conference->tail = traverse_temp->prev;
					}
				}

				/* Free all the elements from the floor list */
				temp = traverse_temp->floor;
				while(temp) {
					/* Kill the threads */
					if(type_list == 1) {
#ifndef WIN32
						if(temp->pID !=0) {
							pthread_cancel(temp->pID);
							temp->pID = 0;
						}
#else
						if(temp->pID.p != NULL) {
						pthread_cancel(temp->pID);
						temp->pID.p = NULL;
						}
#endif
					}

					next = temp->next;
					if(temp->status == BFCP_ACCEPTED) {
						i = bfcp_return_position_floor(lfloors, temp->floorID);
						if(i != -1) {
							if(lfloors != NULL) {
								if(lfloors->floors != NULL)
									lfloors->floors[i].floorState = BFCP_FLOOR_STATE_WAITING;
							}
						}
					}
					free(temp->chair_info);
					temp->chair_info = NULL;
					free(temp);
					temp = NULL;
					temp = next;
				}

				/* Free all the elements from the request list */
				temp_request = traverse_temp->floorrequest;
				while(temp_request != NULL) {
					next_request = temp_request->next;
					free(temp_request);
					temp_request = NULL;
					temp_request = next_request;
				}

				free(traverse_temp->participant_info);
				traverse_temp->participant_info = NULL;
				free(traverse_temp->chair_info);
				traverse_temp->chair_info = NULL;
				free(traverse_temp);
				traverse_temp = NULL;
				floor = NULL;
				delete_node = 1;
			} else
				floor = floor->next;
		}
		if((traverse != NULL) && (delete_node !=1))
			traverse = traverse->next;
	}

	return 0;
}

/* Remove all elements related to an user from a floor request information (?) */
int bfcp_delete_node_with_userID(bfcp_queue *conference, unsigned short int userID, bfcp_list_floors *lfloors)
{
	if(conference == NULL)
		return 0;
	if(userID == 0)
		return -1;
	if(lfloors == NULL)
		return 0;
	if(lfloors->floors == NULL)
		return 0;

	pnode traverse, traverse_temp;
	pfloor temp, next;
	floor_request_query *temp_request;
	floor_request_query *next_request;
	int i = 0;

	traverse = conference->head;

	while(traverse!=NULL) {
		if((traverse->userID == userID) || (traverse->beneficiaryID == userID)) {
			/* The head pointer points to the node we want to delete */
			traverse_temp = traverse;
			if(traverse_temp == conference->head) {
				conference->head = traverse_temp->next;
				traverse = conference->head;
				if(traverse != NULL) {
					traverse->prev = NULL;
					if(traverse->next == NULL)
						conference->tail = traverse;
				} else
					conference->tail = NULL;
			} else {
				if(traverse_temp->prev) {
					/* It is not the first element */
					traverse = traverse_temp->prev;
					traverse_temp->prev->next = traverse_temp->next;
				}
				if(traverse_temp->next) {
					/* It is not the last element */
					traverse = traverse_temp->prev;
					traverse_temp->next->prev = traverse_temp->prev;
				}
				else {
					traverse = NULL;
					conference->tail = traverse_temp->prev;
				}
			}
			/* Free all the elements from the floor list */
			temp = traverse_temp->floor;
			while(temp != NULL) {
				next = temp->next;
				if(temp->status == 2) {
					i = bfcp_return_position_floor(lfloors, temp->floorID);
					if(i != -1)
						lfloors->floors[i].floorState = BFCP_FLOOR_STATE_WAITING;
				}
				free(temp->chair_info);
				temp->chair_info = NULL;
				free(temp);
				temp = NULL;
				temp = next;
			}
			/* Free all the elements from the request list */
			temp_request = traverse_temp->floorrequest;
			while(temp_request != NULL) {
				next_request = temp_request->next;
				free(temp_request);
				temp_request = NULL;
				temp_request = next_request;
			}

			free(traverse_temp->participant_info);
			traverse_temp->participant_info = NULL;
			free(traverse_temp->chair_info);
			traverse_temp->chair_info = NULL;
			free(traverse_temp);
			traverse_temp = NULL;
		} else
		traverse = traverse->next;
	}

	return 0;
}

/* Remove a FloorRequest from the BFCP queues */
bfcp_floor *bfcp_delete_request(bfcp_queue *conference, unsigned short int floorRequestID, int type_queue)
{
	if(conference == NULL)
		return NULL;
	if(floorRequestID == 0)
		return NULL;

	pnode traverse;
	pfloor temp, floor;

	traverse = conference->head;

	while(traverse && (traverse->floorRequestID != floorRequestID))
		traverse = traverse->next;

	if(!traverse || (traverse->floorRequestID != floorRequestID))
		/* This node is not in the list */
		return NULL;

	/* Remove the thread if this node is in the pending queue*/
	if(type_queue==2) {
		floor = traverse->floor;
		while(floor) {
#ifndef WIN32
			if(floor->pID != 0) {
			pthread_cancel(floor->pID);
			floor->pID = 0;
			}
#else
			if(floor->pID.p != NULL) {
			pthread_cancel(floor->pID);
			floor->pID.p = NULL;
			}
#endif
			floor = floor->next;
		}
	}

	/* The head pointer points to the node we want to delete */
	if(traverse == conference->head)
		conference->head = traverse->next;
	if(traverse->prev)
		/* It is not the first element */
		traverse->prev->next = traverse->next;
	if(traverse->next)
		/* It is not the last element */
		traverse->next->prev = traverse->prev;
	else
		conference->tail = traverse->prev;

	/* temp is the list with all the floors */
	temp = traverse->floor;

	/* Remove the list of requests */
	remove_request_list_of_node(traverse->floorrequest);

	free(traverse->participant_info);
	traverse->participant_info = NULL;
	free(traverse->chair_info);
	traverse->chair_info = NULL;
	free(traverse);
	traverse = NULL;

	return temp;
}

/* Get a FloorRequest from one of the BFCP queues */
bfcp_node *bfcp_extract_request(bfcp_queue *conference, unsigned short int floorRequestID)
{
	if(conference == NULL)
		return NULL;
	if(floorRequestID == 0)
		return NULL;

	pnode traverse;
	traverse = conference->head;

	while(traverse && (traverse->floorRequestID != floorRequestID))
		traverse = traverse->next;

	if(!traverse || (traverse->floorRequestID != floorRequestID))
		/* This node is not in the list */
		return NULL;

	/* The head pointer points to the node we want to retrieve */
	if(traverse == conference->head)
		conference->head = traverse->next;
	if(traverse->prev)
		/* It is not the first element */
		traverse->prev->next = traverse->next;
	if(traverse->next)
		/* It is not the last element */
		traverse->next->prev = traverse->prev;
	else
		conference->tail = traverse->prev;

	return traverse;
}

/* Setup a FloorRequest so that it is to be sent in a BFCP message (?) */
int bfcp_floor_request_query_server(bfcp_queue *conference, unsigned short int floorRequestID, unsigned short int userID, int sockfd)
{
	if(conference == NULL)
		return -1;
	if(floorRequestID == 0)
		return -1;

	pnode traverse;
	traverse = conference->head;

	while(traverse && (traverse->floorRequestID != floorRequestID))
		traverse = traverse->next;

	if(!traverse || (traverse->floorRequestID != floorRequestID))
		/* This node is not in the list */
		return -1;

	return add_request_to_the_node(traverse, userID, sockfd);
}

/* Remove all requests from a list */
int bfcp_clean_request_list(bfcp_queue *conference)
{
	if(conference == NULL)
		return -1;

	pnode traverse, node;
	pfloor temp, next;
	floor_request_query *temp_request;
	floor_request_query *next_request;

	traverse = conference->head;
	while(traverse) {
		node = traverse;
		traverse = traverse->next;

		/* Free all the elements of the floor list */
		temp = node->floor;
		while(temp) {
			/* Free all the threads handling the request */
#ifndef WIN32
			if(temp->pID != 0) {
				pthread_cancel(temp->pID);
				temp->pID = 0;
			}
#else
			if(temp->pID.p != NULL) {
				pthread_cancel(temp->pID);
				temp->pID.p = NULL;
			}
#endif
			next = temp->next;
			free(temp->chair_info);
			temp->chair_info = NULL;
			free(temp);
			temp = NULL;
			temp = next;
		}
		/* Free all the elements from the request list */
		temp_request = node->floorrequest;
		while(temp_request) {
			next_request = temp_request->next;
			free(temp_request);
			temp_request = NULL;
			temp_request = next_request;
		}

		free(node->participant_info);
		node->participant_info = NULL;
		free(node->chair_info);
		node->chair_info = NULL;
		free(node);
		node = NULL;
	}
	conference->head = NULL;
	conference->tail = NULL;

	return 0;
}

/* Free a linked list of requests */
int bfcp_remove_request_list(bfcp_queue **conference_p)
{
	if(conference_p == NULL)
		return 0;

	int error;
	bfcp_queue *conference = *conference_p;

	error = bfcp_clean_request_list(conference);
	if(error==0) {
		free(conference);
		conference = NULL;
		*conference_p = NULL;
	}
	else
		return -1;

	return 0;
}

/* Kill all the running threads handling a specific FloorRequest */
int bfcp_kill_threads_request_with_FloorRequestID(bfcp_queue *conference, unsigned short int floorRequestID)
{
	if(conference == NULL)
		return -1;
	if(floorRequestID == 0)
		return -1;

	pnode traverse;
	pfloor floor;

	traverse = conference->head;

	while(traverse && (traverse->floorRequestID != floorRequestID))
		traverse = traverse->next;

	if(!traverse || (traverse->floorRequestID != floorRequestID))
		/* This node is not in the list */
		return -1;

	floor = traverse->floor;
	while(floor) {
#ifndef WIN32
		if(floor->pID != 0) {
			pthread_cancel(floor->pID);
			floor->pID = 0;
		}
#else
		if(floor->pID.p != NULL) {
			pthread_cancel(floor->pID);
			floor->pID.p = NULL;
		}
#endif
		floor = floor->next;
	}

	return 0;
}

/* Convert a 'bfcp_node' info to 'bfcp_floor_request_information' (bfcp_messages.h) */
bfcp_floor_request_information *bfcp_show_floorrequest_information(bfcp_queue *conference, lusers users, unsigned long int floorRequestID, int type_queue)
{
	if(conference == NULL)
		return NULL;

	pnode traverse;
	pfloor floor;
	bfcp_user_information *beneficiary_info, *user_info;
	bfcp_floor_request_information *frqInfo;
	bfcp_overall_request_status *oRS = NULL;
	bfcp_floor_request_status *fRS_temp = NULL, *fRS = NULL;

	traverse = conference->tail;

	if(!traverse)
		/* The list is empty */
		return NULL;

	while(traverse) {
		if(traverse->floorRequestID == floorRequestID) {
			if(type_queue == 0)
				/* Granted */
				oRS = bfcp_new_overall_request_status(traverse->floorRequestID, BFCP_GRANTED, 0, traverse->chair_info);
			else if(type_queue==1)
				/* Accepted */
				oRS = bfcp_new_overall_request_status(traverse->floorRequestID, BFCP_ACCEPTED, traverse->queue_position, traverse->chair_info);
			else if(type_queue==2)
				/* Pending */
				oRS = bfcp_new_overall_request_status(traverse->floorRequestID, BFCP_PENDING, traverse->priority, traverse->chair_info);
			else
				/* Revoke */
				oRS = bfcp_new_overall_request_status(traverse->floorRequestID, BFCP_REVOKED, 0, traverse->chair_info);

			floor = traverse->floor;
			if(floor != NULL) {
				fRS = bfcp_new_floor_request_status(floor->floorID, 0, 0, floor->chair_info);
				floor = floor->next;
				while(floor) {
					fRS_temp = bfcp_new_floor_request_status(floor->floorID, 0, 0, floor->chair_info);
					if(fRS_temp != NULL)
						bfcp_list_floor_request_status(fRS, fRS_temp, NULL);
					floor = floor->next;
				}
			}

			if(traverse->beneficiaryID !=0) {
				beneficiary_info = bfcp_show_user_information(users, traverse->beneficiaryID);
				if(beneficiary_info == NULL) {
					bfcp_free_user_information(beneficiary_info);
					return NULL;
				}
			} else
				beneficiary_info=NULL;

			if(traverse->userID != 0) {
				user_info = bfcp_show_user_information(users, traverse->userID);
				if(user_info == NULL) {
					bfcp_free_user_information(user_info);
					return NULL;
				}
			} else
				user_info = NULL;

			frqInfo = bfcp_new_floor_request_information(traverse->floorRequestID, oRS, fRS, beneficiary_info, user_info, traverse->priority, traverse->participant_info);
			return frqInfo;
		}
		traverse = traverse->prev;
	}

	/* If we arrived so far, something wrong happened */
	return NULL;
}

/* Create a new linked list of floors */
bfcp_floor *create_floor_list(unsigned short int floorID, int status, char *floor_chair_info)
{
	if(status > BFCP_GRANTED)
		return NULL;

	bfcp_floor *floor_list;
	int dLen;

	/* Create a new list of floors */
	floor_list = (pfloor)calloc(1, sizeof(bfcp_floor));

	/* Initialize the list*/
	if(floor_list == NULL)
		return NULL;
	else {
		floor_list->floorID = floorID;
		floor_list->status = status;
#ifndef WIN32
		floor_list->pID = 0;
#else
		floor_list->pID.p = NULL;
#endif
		/* If there's chair-provided text, add it */
		if(floor_chair_info != NULL) {
			dLen= strlen(floor_chair_info);
			if(dLen != 0) {
				floor_list->chair_info = (char *)calloc(1, dLen*sizeof(char)+1);
				if(floor_list->chair_info == NULL)
					return NULL;
				memcpy(floor_list->chair_info, floor_chair_info, dLen+1);
			}
		} else
			floor_list->chair_info = NULL;

		floor_list->next = NULL;
	}

	return floor_list;
}

/* Add floors to a list in order */
bfcp_floor *add_floor_list(bfcp_floor *floor_list, unsigned short int floorID, int status, char *floor_chair_info)
{
	if((status < 0) || (status > BFCP_GRANTED))
		return NULL;
	if(floor_list == NULL)
		return NULL;

	pfloor ini_floor_list, floor;
	int dLen;

    /* Create a new floor instance */
	floor = (pfloor)calloc(1, sizeof(bfcp_floor));

	/* Initialize the floor */
	if(floor == NULL)
		return NULL;

	if(floor_list->floorID < floorID) {
		floor->floorID = floorID;
		floor->status = status;
#ifndef WIN32
		floor->pID = 0;
#else
		floor->pID.p = NULL;
#endif
		/* If there's chair-provided text, add it */
		if(floor_chair_info != NULL) {
			dLen = strlen(floor_chair_info);
			if(dLen != 0) {
				floor->chair_info = (char *)calloc(1, dLen*sizeof(char)+1);
				if(floor->chair_info == NULL)
					return NULL;
				memcpy(floor->chair_info, floor_chair_info, dLen+1);
			}
		} else
			floor->chair_info = NULL;

		floor->next = floor_list;
		floor_list = floor;
	} else if(floor_list->floorID > floorID) {
		ini_floor_list = floor_list;
		while(ini_floor_list->next && (ini_floor_list->next->floorID > floorID))
			ini_floor_list = ini_floor_list->next;
		floor->floorID = floorID;
		floor->status = status;
#ifndef WIN32
		floor->pID = 0;
#else
		floor->pID.p = NULL;
#endif
		/* If there's chair-provided text, add it */
		if(floor_chair_info != NULL) {
			dLen = strlen(floor_chair_info);
			if(dLen != 0) {
				floor->chair_info = (char *)calloc(1, dLen*sizeof(char)+1);
				if(floor->chair_info == NULL)
					return NULL;
				memcpy(floor->chair_info, floor_chair_info, dLen+1);
			}
		} else
			floor->chair_info = NULL;

		floor->next = ini_floor_list->next;
		ini_floor_list->next = floor;
	}

	return(floor_list);
}

/* Free a linked list of requests */
int remove_floor_list(bfcp_floor *floor_list)
{
	pfloor next;

	/* Free all the elements from the floor list */
	while(floor_list) {
		next = floor_list->next;
		free(floor_list->chair_info);
		floor_list->chair_info = NULL;
		free(floor_list);
		floor_list = NULL;
		floor_list = next;
	}

	return 0;
}

/* Remove the list of requests from a request node */
int remove_request_list_of_node(floor_request_query *floorrequest)
{
	floor_request_query * next = NULL;

	/* Free all the elements from the floor list */
	while(floorrequest) {
		next = floorrequest->next;
		floorrequest->next = NULL;
		free(floorrequest);
		floorrequest = NULL;
		floorrequest = next;
	}

	return 0;
}

/* Add a request to a request node */
int add_request_to_the_node(pnode traverse, unsigned short int userID, int sockfd)
{
	if(traverse == NULL)
		return -1;

	int exist_query;
	floor_request_query *floorrequest = NULL;
	floor_request_query *newnode = NULL;

	floorrequest=traverse->floorrequest;

	/* If the request already exists in the list, we don't add it again */
	exist_query = 0;
	while(floorrequest != NULL) {
		if(floorrequest->userID == userID) {
			exist_query = 1;
			break;
		}
		floorrequest = floorrequest->next;
	}

	if(exist_query == 0) {
		/* Create the new node */
		newnode = (floor_request_query *)calloc(1, sizeof(floor_request_query));
		if(newnode == NULL)
			return -1;

		floorrequest = traverse->floorrequest;

		/* Add the request to the listto the list */
		newnode->userID = userID;
		newnode->fd = sockfd;
		newnode->next = floorrequest;
		traverse->floorrequest = newnode;
	}

	return 0;
}

/* Request a request from a request node */
int remove_request_from_the_node(pnode traverse, unsigned short int userID)
{
	if(traverse == NULL)
		return 0;

	floor_request_query *floorrequest = NULL;
	floor_request_query *newnode = NULL;

	floorrequest = traverse->floorrequest;
	if(floorrequest == NULL)
		return 0;

	/* If the request exists in the list, we remove it from the node */
	if(floorrequest->userID == userID) {
		traverse->floorrequest = floorrequest->next;
		floorrequest->next = NULL;
		free(floorrequest);
		floorrequest = NULL;
		return 0;
	}

	while(floorrequest->next != NULL) {
		if(floorrequest->next->userID == userID) {
			newnode = floorrequest->next;
			floorrequest->next = newnode->next;
			free(newnode);
			newnode = NULL;
			return 0;
		}
		floorrequest = floorrequest->next;
	}

	return 0;
}

/* ?? */
int bfcp_accepted_pending_node_with_floorID(unsigned long int conferenceID, bfcp_queue *accepted, bfcp_queue *conference, unsigned short int floorID, bfcp_list_floors *lfloors, int type_list)
{
	if(conference == NULL)
		return -1;
	if(floorID == 0)
		return -1;

	pnode traverse, traverse_temp;
	floor_request_query *floorrequest;
	int delete_node = 0, error = 0;
	unsigned short int userID;
	bfcp_node *newnode = NULL;
	pfloor floor;

	traverse = conference->head;

	while(traverse) {
		floor = traverse->floor;
		delete_node = 0;
		while((floor != NULL) && (floor->floorID >= floorID)) {
			if(floor->floorID == floorID) {
				floor->status = 1;
				floor->chair_info = NULL;
			}

			/* If all the floors in the request have been accepted... */
			if(bfcp_all_floor_status(conference, traverse->floorRequestID, 1) == 0) {
				traverse_temp = traverse->next;
				/* ...extract the node request from the Pending list... */
				newnode = bfcp_extract_request(conference, traverse->floorRequestID);

				/* ...and remove the threads for this node */
				floor = newnode->floor;
				while(floor) {
#ifndef WIN32
					if(floor->pID != 0) {
						pthread_cancel(floor->pID);
						floor->pID = 0;
					}
#else
					if(floor->pID.p != NULL) {
						pthread_cancel(floor->pID);
						floor->pID.p = NULL;
					}
#endif
				floor = floor->next;
				}

				/* Put the node in the Accepted list */
				/* Change the priority of the node to the lowest one */
				newnode->priority = BFCP_LOWEST_PRIORITY;

				error = bfcp_insert_request(accepted, newnode, traverse->floorRequestID, NULL);
				if(error == -1)
					return (-1);

				/* Add the floor request information to the floor nodes */
				if(newnode->beneficiaryID != 0)
					userID = newnode->beneficiaryID;
				else
					userID = newnode->userID;

				floorrequest = newnode->floorrequest;

				delete_node = 1;
				traverse = traverse_temp;
			} else
				floor = floor->next;
		}
		if((traverse != NULL) && (delete_node != 1))
			traverse = traverse->next;
	}

	return 0;
}

/* Change position in queue to a FloorRequest */
int bfcp_change_queue_position(bfcp_queue *conference, unsigned short int floorRequestID, int queue_position)
{
	if(conference == NULL)
		return 0;
	if(queue_position <0)
		return 0;

	pnode traverse;
	traverse = conference->head;

	while(traverse && (traverse->floorRequestID != floorRequestID))
		traverse = traverse->next;

	if(!traverse || (traverse->floorRequestID != floorRequestID))
		/* This node is not in the list */
		return 0;
	else {
		if(queue_position > traverse->queue_position)
			traverse->queue_position = queue_position;
	}

	return 0;
}
