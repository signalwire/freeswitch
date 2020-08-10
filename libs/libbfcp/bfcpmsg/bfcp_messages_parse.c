#include "bfcp_messages.h"
#include "bfcp_strings.h"

unsigned short int bfcp_get_length(bfcp_message *message)
{
	if(!message)
		return 0;
	unsigned long int ch32;		/* 32 bits */
	unsigned short int length;	/* 16 bits */
	unsigned char *buffer = message->buffer;
	memcpy(&ch32, buffer, 4);	/* We copy the first 4 octets of the header */
	ch32 = ntohl(ch32);
	length = (ch32 & 0x0000FFFF);	/* We get the Lenght of the message */
	return length;
}

int bfcp_get_primitive(bfcp_message *message)
{
	if(!message)
		return 0;
	unsigned long int ch32;			/* 32 bits */
	int primitive;
	unsigned char *buffer = message->buffer;
	memcpy(&ch32, buffer, 4);		/* We copy the first 4 octets of the header */
	ch32 = ntohl(ch32);
	primitive = ((ch32 & 0x00FF0000) >> 16);	/* We get the Primitive identifier */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Primitive[%d/0x%x][%s]\n",primitive,primitive,get_bfcp_primitive(primitive));
	return primitive;
}

unsigned long int bfcp_get_conferenceID(bfcp_message *message)
{
	if(!message)
		return 0;
	unsigned long int ch32;		/* 32 bits */
	unsigned long int conferenceID;	/* 32 bits */
	unsigned char *buffer = message->buffer + 4;
	memcpy(&ch32, buffer, 4);	/* We skip the first 4 octets of the header and copy */
	conferenceID = ntohl(ch32);	/* We get the conferenceID of the message */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "conferenceID[%lu]\n",conferenceID);
	return conferenceID;
}

unsigned short int bfcp_get_transactionID(bfcp_message *message)
{
	if(!message)
		return 0;
	unsigned long int ch16;			/* 16 bits */
	unsigned short int transactionID;	/* 16 bits */
	unsigned char *buffer = message->buffer + 8;
	memcpy(&ch16, buffer, 2);		/* We skip the first 8 octets of the header and copy */
	transactionID = ntohs(ch16);		/* We get the transactionID of the message */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "TransactionID[0x%x]",transactionID);
	return transactionID;
}

unsigned short int bfcp_get_userID(bfcp_message *message)
{
	if(!message)
		return 0;
	unsigned long int ch16;		/* 16 bits */
	unsigned short int userID;	/* 16 bits */
	unsigned char *buffer = message->buffer + 10;
	memcpy(&ch16, buffer, 2);	/* We skip the first 10 octets of the header and copy */
	userID = ntohs(ch16);		/* We get the userID of the message */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "userID[0x%x]\n",userID);
	return userID;
}

bfcp_received_message *bfcp_new_received_message(void)
{
	bfcp_received_message *recvM = calloc(1, sizeof(bfcp_received_message));
	if(!recvM)	/* We could not allocate the memory, return with a failure */
		return NULL;
	recvM->arguments = NULL;
	recvM->version = 0;
	recvM->reserved = 0;
	recvM->length = 0;
	recvM->primitive = 0;
	recvM->entity = bfcp_new_entity(0, 0, 0);
	recvM->first_attribute = NULL;
	recvM->errors = NULL;
	return recvM;
}

int bfcp_free_received_message(bfcp_received_message *recvM)
{
	int res = 0;	/* We keep track here of the results of the sub-freeing methods*/
	if(!recvM)	/* There's nothing to free, return with a failure */
		return -1;
	if(recvM->arguments)
		res += bfcp_free_arguments(recvM->arguments);
	if(recvM->entity)
		res += bfcp_free_entity(recvM->entity);
	if(recvM->first_attribute)
		res += bfcp_free_received_attribute(recvM->first_attribute);
	if(recvM->errors)
		res += bfcp_free_received_message_errors(recvM->errors);
	free(recvM);
	if(!res)	/* No error occurred, succesfully freed the structure */
		return 0;
	else		/* res was not 0, so some error occurred, return with a failure */
		return -1;
}

bfcp_received_message_error *bfcp_received_message_add_error(bfcp_received_message_error *error, unsigned short int attribute, unsigned short int code)
{
	bfcp_received_message_error *temp, *previous;
	if(!error) {	/* The Error list doesn't exist yet, we create a new one */
		error = calloc(1, sizeof(bfcp_received_message_error));
		if(!error)	/* We could not allocate the memory, return with a failure */
			return NULL;
		error->attribute = attribute;
		error->code = code;
		error->next = NULL;
		return error;
	} else {
		previous = error;
		temp = previous->next;
		while(temp) {	/* We search the last added error */
			previous = temp;
			temp = previous->next;
		}
		temp = calloc(1, sizeof(bfcp_received_message_error));
		if(!temp)	/* We could not allocate the memory, return with a failure */
			return NULL;
		temp->attribute = attribute;
		temp->code = code;
		temp->next = NULL;
		previous->next = temp;	/* Update the old last link in the list */
		return error;
	}
}

int bfcp_free_received_message_errors(bfcp_received_message_error *errors)
{
	if(!errors)	/* There's nothing to free, return with a failure */
		return -1;
	bfcp_received_message_error *next = NULL, *temp = errors;
	while(temp) {
		next = temp->next;
		free(temp);
		temp = next;
	}
	return 0;
}

bfcp_received_attribute *bfcp_new_received_attribute(void)
{
	bfcp_received_attribute *recvA = calloc(1, sizeof(bfcp_received_attribute));
	if(!recvA)	/* We could not allocate the memory, return with a failure */
		return NULL;
	recvA->type = 0;
	recvA->mandatory_bit = 0;
	recvA->length = 0;
	recvA->position = 0;
	recvA->valid = 1;	/* By default we mark the attribute as valid */
	recvA->next = NULL;
	return recvA;
}

int bfcp_free_received_attribute(bfcp_received_attribute *recvA)
{
	if(!recvA)	/* There's nothing to free, return with a failure */
		return -1;
	bfcp_received_attribute *next = NULL, *temp = recvA;
	while(temp) {
		next = temp->next;
		free(temp);
		temp = next;
	}
	return 0;
}

bfcp_received_message *bfcp_parse_message(bfcp_message *message)
{
	bfcp_received_attribute *temp1 = NULL, *temp2 = NULL, *previous = NULL;
	unsigned char *buffer;
	unsigned short int ch16;	/* 16 bits */
	unsigned int ch32;		/* 32 bits */
	bfcp_received_message *recvM = bfcp_new_received_message();
	if(!recvM)	/* We could not allocate the memory, return with a failure */
		return NULL;
	/* First we read the Common Header and we parse it */
	buffer = message->buffer;
	memcpy(&ch32, buffer, 4);	/* We copy the first 4 octets of the header */
	ch32 = ntohl(ch32);
	recvM->version = ((ch32 & 0xE0000000) >> 29);	/* Version bits (must be 001) */
	if((recvM->version) != 1) {	/* Version is wrong, return with an error */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Unsupported protocol version %d\n", recvM->version);
		recvM->errors = bfcp_received_message_add_error(recvM->errors, 0, BFCP_WRONG_VERSION);
		if(!(recvM->errors))
			return NULL;	/* An error occurred while recording the error, return with failure */
	}
	recvM->reserved = ((ch32 & 0x1F000000) >> 24);	/* Reserved bits (they should be ignored but we check them anyway) */
	if((recvM->reserved) != 0) {	/* Reserved bits are not 0, return with an error */
		recvM->errors = bfcp_received_message_add_error(recvM->errors, 0, BFCP_RESERVED_NOT_ZERO);
		if(!(recvM->errors))
			return NULL;	/* An error occurred while recording the error, return with failure */
	}
	recvM->primitive = ((ch32 & 0x00FF0000) >> 16);	/* Primitive identifier */

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Primitive(%d) - %s\n", recvM->primitive, get_bfcp_primitive(recvM->primitive));

	recvM->length = (ch32 & 0x0000FFFF)*4 + 12;	/* Payload length is in 4-byte units */
	if(((recvM->length) != message->length) || ((recvM->length%4) != 0)) {	/* The message length is wrong */
			/* Either the length in the header is different from the length of the buffer... */
			/*   ...or the length is not a multiple of 4, meaning it's surely not aligned */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error: payload length does not match data read from network.\n");
		switch_log_printf(SWITCH_CHANNEL_LOG,
                          SWITCH_LOG_ERROR,
                          "We read %u bytes from network while message length calculated from mayload length  is %u.\n",
                          message->length,
                          recvM->length);

		recvM->errors = bfcp_received_message_add_error(recvM->errors, 0, BFCP_WRONG_LENGTH);
		if(!(recvM->errors))
			return NULL;	/* An error occurred while recording the error, return with failure */
	}
	if(recvM->errors)	/* There are errors in the header, we won't proceed further */
		return recvM;
	buffer = buffer+4;
	memcpy(&ch32, buffer, 4);	/* Conference ID */
	recvM->entity->conferenceID = ntohl(ch32);
	buffer = buffer+4;
	memcpy(&ch16, buffer, 2);	/* Transaction ID */
	recvM->entity->transactionID = ntohs(ch16);
	buffer = buffer+2;
	memcpy(&ch16, buffer, 2);	/* User ID */
	recvM->entity->userID = ntohs(ch16);
	buffer = buffer+2;

	switch_log_printf(SWITCH_CHANNEL_LOG,
                      SWITCH_LOG_INFO,
                      "ConferenceID[%lu], UserID[%u], TransactionID[%u]\n",
                      recvM->entity->conferenceID,
                      recvM->entity->userID,
                      recvM->entity->transactionID);

	message->position = 12;	/* We've read the Common Header */
	while(recvM->length>message->position) {	/* We start parsing attributes too */
		temp1 = bfcp_parse_attribute(message);
		if(!temp1)	/* We could not parse the attribute, return with a failure */
			return NULL;
		if(!(recvM->first_attribute)) {		/* This is the first attribute we read */
			recvM->first_attribute = temp1;	/* Let's save it as first attribute */
			recvM->first_attribute->next = NULL;
			temp2 = temp1;
		}
		else {		/* It's not the first attribute, let's manage the list */
			temp2->next = temp1;
			temp1->next = NULL;
			previous = temp2;
			temp2 = temp1;
		}
		if(temp1->length == 0) {
		/* If the length of the attribute is 0 we have to stop, we'd fall in an eternal loop
			We save this error regarding this attribute (temp1 -> Wrong Lenght)
			and we save it for the attribute before (temp2) too, since it lead us here */
			temp1->valid = 0;		/* We mark the attribute as not valid */
			recvM->errors = bfcp_received_message_add_error(recvM->errors, temp1->type, BFCP_WRONG_LENGTH);
			if(!(recvM->errors))
				return NULL;	/* An error occurred while recording the error, return with failure */
			if(previous) {	/* Only add the error if there's an attribute before */
				previous->valid = 0;	/* We mark the attribute as not valid */
				recvM->errors = bfcp_received_message_add_error(recvM->errors, previous->type, BFCP_WRONG_LENGTH);
				if(!(recvM->errors))
					return NULL;	/* An error occurred while recording the error, return with failure */
			}
			message->position = recvM->length;	/* We don't go on parsing, since we can't jump this attribute */
		}
	}
	if(bfcp_parse_arguments(recvM, message) == -1) {/* We could not parse the arguments of the message */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to parse BFCP message arguments.\n");
		return NULL;
	}			/* Return with a failure */
	recvM->length = recvM->length-12;	/* Before leaving, we remove the Common Header Lenght (12 bytes) */
						/* from the Payload Lenght of the message */
	return recvM;
}

bfcp_received_attribute *bfcp_parse_attribute(bfcp_message *message)
{
	int padding = 0;
	unsigned char *buffer;
	unsigned short int ch16;	/* 16 bits */
	bfcp_received_attribute *recvA = bfcp_new_received_attribute();
	if(!recvA)	/* We could not allocate the memory, return with a failure */
		return NULL;
	recvA->position = message->position;	/* The position of the attribute in the buffer */
	buffer = message->buffer+recvA->position;
	memcpy(&ch16, buffer, 2);		/* TLV: Attribute Header */
	ch16 = ntohs(ch16);
	recvA->type = ((ch16 & 0xFE00) >> 9);		/* Type */
	recvA->mandatory_bit = ((ch16 & 0x0100) >> 8);	/* M */
	recvA->length = (ch16 & 0x00FF);		/* Lenght */
	buffer = buffer+2;
	if(((recvA->length)%4) != 0) 	/* There's padding stuff to jump too */
		padding = 4-((recvA->length)%4);
	message->position = message->position+recvA->length +	/* There could be some padding */
				padding;	 		/* (which is not signed in length) */
	return recvA;
}

int bfcp_parse_arguments(bfcp_received_message *recvM, bfcp_message *message)
{
	bfcp_received_attribute *temp = recvM->first_attribute;
	if(!recvM)	/* We could not allocate the memory, return with a failure */
		return -1;
	int floorID;
	bfcp_floor_id_list *tempID = NULL;	/* To manage the FLOOR-ID List */
	bfcp_floor_request_information *tempInfo = NULL, *previousInfo = NULL;/* FLOOR-REQUEST-INFORMATION list */
	recvM->arguments = bfcp_new_arguments();
	recvM->arguments->primitive = recvM->primitive;	/* Primitive */
	recvM->arguments->entity = bfcp_new_entity(recvM->entity->conferenceID, /* Entity, we copy it not to */
			recvM->entity->transactionID, recvM->entity->userID);	/* to risk a double free()  */
	if(!(recvM->arguments))		/* We could not allocate the memory, return with a failure */
		return -1;
	while(temp) {
		if(!(temp->valid)) {	/* If the attribute is marked as not valid, we skip it... */
			temp = temp->next;
			continue;
		} else switch(temp->type) {	/* ...if it's valid, we parse it */
			case BENEFICIARY_ID:
				recvM->arguments->bID = bfcp_parse_attribute_BENEFICIARY_ID(message, temp);
				if(!recvM->arguments->bID) {	/* An error occurred in parsing this attribute */
					recvM->errors = bfcp_received_message_add_error(recvM->errors, temp->type, BFCP_PARSING_ERROR);
					if(!(recvM->errors))
						return -1;	/* An error occurred while recording the error, return with failure */
				}
				break;
			case FLOOR_ID:
				if(!tempID) {	/* Create a list, it's the first ID we add */
					floorID = bfcp_parse_attribute_FLOOR_ID(message, temp);
					if(!floorID) {		/* An error occurred while parsing this attribute */
						recvM->errors = bfcp_received_message_add_error(recvM->errors, temp->type, BFCP_PARSING_ERROR);
						if(!(recvM->errors))
							return -1;	/* An error occurred while recording the error, return with failure */
					}
					recvM->arguments->fID = bfcp_new_floor_id_list(floorID, 0);
					tempID = recvM->arguments->fID;
					if(!tempID)	/* An error occurred in creating a new ID */
						return -1;
					break;
				} else {	/* We already have a list, add the new FloorID to it */
					floorID = bfcp_parse_attribute_FLOOR_ID(message, temp);
					if(!floorID) {		/* An error occurred while parsing this attribute */
						recvM->errors = bfcp_received_message_add_error(recvM->errors, temp->type, BFCP_PARSING_ERROR);
						if(!(recvM->errors))
							return -1;	/* An error occurred while recording the error, return with failure */
					}
					if(bfcp_add_floor_id_list(tempID, floorID, 0) == -1)
						return -1;
					break;
				}
			case FLOOR_REQUEST_ID:
				recvM->arguments->frqID = bfcp_parse_attribute_FLOOR_REQUEST_ID(message, temp);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "FLOOR_REQUEST_ID[%d]\n",recvM->arguments->frqID);
				break;
			case PRIORITY:
				recvM->arguments->priority = bfcp_parse_attribute_PRIORITY(message, temp);
				if(recvM->arguments->priority>4) {	/* An error occurred in parsing this attribute */
					recvM->errors = bfcp_received_message_add_error(recvM->errors, temp->type, BFCP_PARSING_ERROR);
					if(!(recvM->errors))
						return -1;	/* An error occurred while recording the error, return with failure */
				}
				break;
			case REQUEST_STATUS:
				recvM->arguments->rs = bfcp_parse_attribute_REQUEST_STATUS(message, temp);
				if(!(recvM->arguments->rs)) {	/* An error occurred in parsing this attribute */
					recvM->errors = bfcp_received_message_add_error(recvM->errors, temp->type, BFCP_PARSING_ERROR);
					if(!(recvM->errors))
						return -1;	/* An error occurred while recording the error, return with failure */
				}
				break;
			case ERROR_CODE:
				recvM->arguments->error = bfcp_parse_attribute_ERROR_CODE(message, temp);
				if(!(recvM->arguments->error)) {	/* An error occurred in parsing this attribute */
					recvM->errors = bfcp_received_message_add_error(recvM->errors, temp->type, BFCP_PARSING_ERROR);
					if(!(recvM->errors))
						return -1;	/* An error occurred while recording the error, return with failure */
				}
				break;
			case ERROR_INFO:
				recvM->arguments->eInfo = bfcp_parse_attribute_ERROR_INFO(message, temp);
				if(!(recvM->arguments->eInfo)) {	/* An error occurred in parsing this attribute */
					recvM->errors = bfcp_received_message_add_error(recvM->errors, temp->type, BFCP_PARSING_ERROR);
					if(!(recvM->errors))
						return -1;	/* An error occurred while recording the error, return with failure */
				}
				break;
			case PARTICIPANT_PROVIDED_INFO:
				recvM->arguments->pInfo = bfcp_parse_attribute_PARTICIPANT_PROVIDED_INFO(message, temp);
				switch_log_printf(SWITCH_CHANNEL_LOG,
                                  SWITCH_LOG_INFO,
                                  "PARTICIPANT_PROVIDED_INFO[%s]\n",
                                  recvM->arguments->pInfo?recvM->arguments->pInfo:"");

				if(!(recvM->arguments->pInfo)) {	/* An error occurred in parsing this attribute */
					recvM->errors = bfcp_received_message_add_error(recvM->errors, temp->type, BFCP_PARSING_ERROR);
					if(!(recvM->errors))
						return -1;	/* An error occurred while recording the error, return with failure */
				}
				break;
			case STATUS_INFO:
				recvM->arguments->sInfo = bfcp_parse_attribute_STATUS_INFO(message, temp);
				switch_log_printf(SWITCH_CHANNEL_LOG,
                                  SWITCH_LOG_INFO,
                                  "STATUS_INFO[%s]\n",
                                  recvM->arguments->sInfo?recvM->arguments->sInfo:"");

				if(!(recvM->arguments->sInfo)) {	/* An error occurred in parsing this attribute */
					recvM->errors = bfcp_received_message_add_error(recvM->errors, temp->type, BFCP_PARSING_ERROR);
					if(!(recvM->errors))
						return -1;	/* An error occurred while recording the error, return with failure */
				}
				break;
			case SUPPORTED_ATTRIBUTES:
				recvM->arguments->attributes = bfcp_parse_attribute_SUPPORTED_ATTRIBUTES(message, temp);
				if(!(recvM->arguments->attributes)) {	/* An error occurred in parsing this attribute */
					recvM->errors = bfcp_received_message_add_error(recvM->errors, temp->type, BFCP_PARSING_ERROR);
					if(!(recvM->errors))
						return -1;	/* An error occurred while recording the error, return with failure */
				}
				break;
			case SUPPORTED_PRIMITIVES:
				recvM->arguments->primitives = bfcp_parse_attribute_SUPPORTED_PRIMITIVES(message, temp);
				if(!(recvM->arguments->primitives)) {	/* An error occurred in parsing this attribute */
					recvM->errors = bfcp_received_message_add_error(recvM->errors, temp->type, BFCP_PARSING_ERROR);
					if(!(recvM->errors))
						return -1;	/* An error occurred while recording the error, return with failure */
				}
				break;
			case USER_DISPLAY_NAME:
				return -1;	/* We can't have this Attribute directly in a primitive */
			case USER_URI:
				return -1;	/* We can't have this Attribute directly in a primitive */
			case BENEFICIARY_INFORMATION:
				recvM->arguments->beneficiary = bfcp_parse_attribute_BENEFICIARY_INFORMATION(message, temp);
				if(!(recvM->arguments->beneficiary)) {	/* An error occurred in parsing this attribute */
					recvM->errors = bfcp_received_message_add_error(recvM->errors, temp->type, BFCP_PARSING_ERROR);
					if(!(recvM->errors))
						return -1;	/* An error occurred while recording the error, return with failure */
				}
				break;
			case FLOOR_REQUEST_INFORMATION:
				if(!tempInfo) {	/* Create a list, it's the first F.R.Info we add */
					recvM->arguments->frqInfo = bfcp_parse_attribute_FLOOR_REQUEST_INFORMATION(message, temp);
					if(!(recvM->arguments->frqInfo)) {	/* An error occurred in parsing this attribute */
						recvM->errors = bfcp_received_message_add_error(recvM->errors, temp->type, BFCP_PARSING_ERROR);
						if(!(recvM->errors))
							return -1;	/* An error occurred while recording the error, return with failure */
					}
					tempInfo = previousInfo = recvM->arguments->frqInfo;
					break;
				} else {
					tempInfo = bfcp_parse_attribute_FLOOR_REQUEST_INFORMATION(message, temp);
					if(!tempInfo) {	/* An error occurred in parsing this attribute */
						recvM->errors = bfcp_received_message_add_error(recvM->errors, temp->type, BFCP_PARSING_ERROR);
						if(!(recvM->errors))
							return -1;	/* An error occurred while recording the error, return with failure */
					}
					tempInfo->next = NULL;
					previousInfo->next = tempInfo;
					previousInfo = tempInfo;
					break;
				}
				break;
			case REQUESTED_BY_INFORMATION:
				return -1;	/* We can't have this Attribute directly in a primitive */
			case FLOOR_REQUEST_STATUS:
				return -1;	/* We can't have this Attribute directly in a primitive */
			case OVERALL_REQUEST_STATUS:
				return -1;	/* We can't have this Attribute directly in a primitive */
			case NONCE:
				recvM->arguments->nonce = bfcp_parse_attribute_NONCE(message, temp);
				if(!recvM->arguments->nonce) {	/* An error occurred in parsing this attribute */
					recvM->errors = bfcp_received_message_add_error(recvM->errors, temp->type, BFCP_PARSING_ERROR);
					if(!(recvM->errors))
						return -1;	/* An error occurred while recording the error, return with failure */
				}
				break;
			case DIGEST:
				recvM->arguments->digest = bfcp_parse_attribute_DIGEST(message, temp);
				if(!(recvM->arguments->digest)) {	/* An error occurred in parsing this attribute */
					recvM->errors = bfcp_received_message_add_error(recvM->errors, temp->type, BFCP_PARSING_ERROR);
					if(!(recvM->errors))
						return -1;	/* An error occurred while recording the error, return with failure */
				}
				break;
			default:	/* An unrecognized attribute, remember it */
				recvM->errors = bfcp_received_message_add_error(recvM->errors, temp->type, BFCP_UNKNOWN_ATTRIBUTE);
				if(!(recvM->errors))
					return -1;	/* An error occurred while recording the error, return with failure */
				temp->valid = 0;	/* We mark the attribute as not valid */
				break;
			}
		temp = temp->next;
	}
	return 0;
}

/* In parsing the attributes, we return 0 (or NULL) if something is WRONG, and return the value if all is ok */
/* This is necessary since many arguments are unsigned, so cannot return -1 to notify that an error happened */
/* In parsing the priority, an error is not 0, but a return value more than 4 (since priority is [0, 4]) */
int bfcp_parse_attribute_BENEFICIARY_ID(bfcp_message *message, bfcp_received_attribute *recvA)
{
	if(recvA->length != 4) {/* The length of this attribute is wrong */
		switch_log_printf(SWITCH_CHANNEL_LOG,
                          SWITCH_LOG_ERROR,
                          "Parse attribute BENEFICIARY_ID length [%d] is incorrect. Should be 4.\n",
                          recvA->length);
		return 0;
	}
	unsigned short int ch16;	/* 16 bits */
	unsigned char *buffer = message->buffer+recvA->position+2;	/* Skip the Header */
	memcpy(&ch16, buffer, 2);
	ch16 = ntohs(ch16);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "BENEFICIARY_ID = [%d]\n", ch16);
	return ch16;
}

int bfcp_parse_attribute_FLOOR_ID(bfcp_message *message, bfcp_received_attribute *recvA)
{
	if(recvA->length != 4) {/* The length of this attribute is wrong */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Wrong FLOOR_ID length [%d]. Should be 4.\n", recvA->length);
		return 0;
	}
	unsigned short int ch16;	/* 16 bits */
	unsigned char *buffer = message->buffer+recvA->position+2;	/* Skip the Header */
	memcpy(&ch16, buffer, 2);
	ch16 = ntohs(ch16);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "FLOOR_ID = [%d]\n", ch16);
	return ch16;
}

int bfcp_parse_attribute_FLOOR_REQUEST_ID(bfcp_message *message, bfcp_received_attribute *recvA)
{
	if(recvA->length != 4) {/* The length of this attribute is wrong */
		switch_log_printf(SWITCH_CHANNEL_LOG,
                          SWITCH_LOG_ERROR,
                          "parse attribute FLOOR_REQUEST_ID length [%d] is not correct. Should be 4.\n",
                          recvA->length);
		return 0;
	}
	unsigned short int ch16;	/* 16 bits */
	unsigned char *buffer = message->buffer+recvA->position+2;	/* Skip the Header */
	memcpy(&ch16, buffer, 2);
	ch16 = ntohs(ch16);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "FLOOR_REQUEST_ID = [%d]\n", ch16);
	return ch16;
}

int bfcp_parse_attribute_PRIORITY(bfcp_message *message, bfcp_received_attribute *recvA)
{
	if(recvA->length != 4) {/* The length of this attribute is wrong */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"Parse attribute PRIORITY length [%d] incorrect. Should be 4.\n",recvA->length );
		return 0;
	}
	unsigned short int ch16;	/* 16 bits */
	unsigned char *buffer = message->buffer+recvA->position+2;	/* Skip the Header */
	memcpy(&ch16, buffer, 2);
	ch16 = ntohs(ch16);
	if((ch16 & (0x1FFF)) != 0)	/* The Reserved bits are not 0 */
		return 0;		/* Return with a failure */
	return ((ch16 & (0xE000)) >> 13);
}

bfcp_request_status *bfcp_parse_attribute_REQUEST_STATUS(bfcp_message *message, bfcp_received_attribute *recvA)
{
	if(recvA->length != 4) {/* The length of this attribute is wrong */
		switch_log_printf(SWITCH_CHANNEL_LOG,
                          SWITCH_LOG_ERROR,
                          "Parse attribute REQUEST_STATUS length [%d] is not correct. Should be 4.\n",
                          recvA->length);
		return NULL;
	}
	bfcp_request_status *rs;
	unsigned short int ch16;	/* 16 bits */
	unsigned char *buffer = message->buffer+recvA->position+2;	/* Skip the Header */
	memcpy(&ch16, buffer, 2);
	ch16 = ntohs(ch16);
	rs = bfcp_new_request_status(
		((ch16 & (0xFF00)) >> 8), 	/* Request Status */
		(ch16 & (0x00FF)));		/* Queue Position */
	if(!rs)		/* An error occurred when creating the new Request Status */
		return NULL;	/* Return with a failure */

	switch_log_printf(SWITCH_CHANNEL_LOG,
                      SWITCH_LOG_INFO,
                      "REQUEST_STATUS request Status [%s] Queue Position [%d]\n",
                      get_bfcp_status(rs->rs), rs->qp );
	return rs;
}

bfcp_error *bfcp_parse_attribute_ERROR_CODE(bfcp_message *message, bfcp_received_attribute *recvA)
{
	if(recvA->length<3) {/* The length of this attribute is wrong */
		switch_log_printf(SWITCH_CHANNEL_LOG,
                          SWITCH_LOG_INFO,
                          "Parse attribute ERROR_CODE length [%d] is incorrect. Should be at lease 3.\n",
                          recvA->length );
		return NULL;
	}
	bfcp_error *error = bfcp_new_error(0, NULL);
	bfcp_unknown_m_error_details *first, *previous, *next;
	if(!error)	/* An error occurred when creating a new Error Code */
		return NULL;	/* Return with a failure */
	char ch;	/* 8 bits */
	int i;
	int number = 0;	/* The number of specific details we might find */
	unsigned char *buffer = message->buffer+recvA->position+2;	/* Skip the Header */
	memcpy(&ch, buffer, 1);
	error->code = (unsigned short int)ch;	/* We get the Error Code*/
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"ERROR_CODE [%s] (%d)",get_bfcp_error_type(error->code), error->code);

	switch(error->code) {
		case BFCP_UNKNOWN_MANDATORY_ATTRIBUTE:	/* For now, the only error that has more details is error 4 */
			number = recvA->length-3;	/* Each error detail takes 1 byte */
			if(number == 0) {
				/* There has to be AT LEAST one error detail, for error 4*/
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing error code detailed information.\n");
				return NULL;
			}

			first = calloc(1, sizeof(bfcp_unknown_m_error_details));
			if(!first)	/* An error occurred in creating a new Details list */
				return NULL;
			buffer++;	/* We skip the Error Code byte */
			memcpy(&ch, buffer, 1);
			first->unknown_type = ((ch & (0xFE)) >> 1);	/* The Unknown Attribute, 7 bits */
			first->reserved = (ch & (0x01));		/* The Reserved bit */
			previous = first;
			if(number>1) {		/* Let's parse each other detail we find */
				for(i = 1;i<number;i++) {
					next = calloc(1, sizeof(bfcp_unknown_m_error_details));
					if(!next) {/* An error occurred in creating a new Details list */
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Parse attribute ERROR_CODE details. Failed to allocate memory.\n");
						return NULL;
					}

					buffer++;	/* We go to the next byte */
					memcpy(&ch, buffer, 1);
					next->unknown_type = ((ch & (0xFE)) >> 1);	/* The Unknown Attribute, 7 bits */
					next->reserved = (ch & (0x01));	/* The Reserved bit */
					previous->next = next;
					previous = next;
				}
			}
			error->details = first;
			break;
		default:	/* All the others have none, so we can ignore it */
			break;
	}
	return error;
}

char *bfcp_parse_attribute_ERROR_INFO(bfcp_message *message, bfcp_received_attribute *recvA)
{
	if(recvA->length<3)	{/* The length of this attribute is wrong */
		switch_log_printf(SWITCH_CHANNEL_LOG,
                          SWITCH_LOG_ERROR,
                          "Parse attribute ERROR_INFO length [%d] is incorrect. Should be at least 3.\n",
                          recvA->length);
		return NULL;
	}

	char ch = '\0';
	unsigned char *buffer = message->buffer+recvA->position+2;	/* Skip the Header */
	char *eInfo = calloc(recvA->length-1, sizeof(char));	/* Lenght is TLV Header too */
	memcpy(eInfo, buffer, recvA->length-2);
	memcpy(eInfo+(recvA->length-2), &ch, 1);	/* We add a terminator char to the string */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Parse attribute ERROR_INFO [%s]\n", eInfo?eInfo:"NULL");
	return eInfo;
}

char *bfcp_parse_attribute_PARTICIPANT_PROVIDED_INFO(bfcp_message *message, bfcp_received_attribute *recvA)
{
	if(recvA->length<3) {/* The length of this attribute is wrong */
		switch_log_printf(SWITCH_CHANNEL_LOG,
                          SWITCH_LOG_ERROR,
                          "Parse attribute PARTICIPANT_PROVIDED_INFO length [%d] is incorrect. Should be at least 3.\n",
                          recvA->length);
		return NULL;
	}

	char ch = '\0';
	unsigned char *buffer = message->buffer+recvA->position+2;	/* Skip the Header */
	char *pInfo = calloc(recvA->length-1, sizeof(char));	/* Lenght is TLV Header too */
	memcpy(pInfo, buffer, recvA->length-2);
	memcpy(pInfo+(recvA->length-2), &ch, 1);	/* We add a terminator char to the string */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Parse attribute PARTICIPANT_PROVIDED_INFO [%s].\n", pInfo? pInfo:"NULL");
	return pInfo;
}

char *bfcp_parse_attribute_STATUS_INFO(bfcp_message *message, bfcp_received_attribute *recvA)
{
	if(recvA->length<3) {/* The length of this attribute is wrong */
		switch_log_printf(SWITCH_CHANNEL_LOG,
                          SWITCH_LOG_ERROR,
                          "Parse attribute STATUS_INFO length [%d] is incorrect. Should be at least 3.\n",
                          recvA->length);
		return NULL;
	}

	char ch = '\0';
	unsigned char *buffer = message->buffer+recvA->position+2;	/* Skip the Header */
	char *sInfo = calloc(recvA->length-1, sizeof(char));	/* Lenght is TLV Header too */
	memcpy(sInfo, buffer, recvA->length-2);
	memcpy(sInfo+(recvA->length-2), &ch, 1);	/* We add a terminator char to the string */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "STATUS_INFO [%s]\n", sInfo? sInfo:"NULL");
	return sInfo;
}

bfcp_supported_list *bfcp_parse_attribute_SUPPORTED_ATTRIBUTES(bfcp_message *message, bfcp_received_attribute *recvA)
{
	if(recvA->length<3) {/* The length of this attribute is wrong */
		switch_log_printf(SWITCH_CHANNEL_LOG,
                          SWITCH_LOG_ERROR,
                          "Parse attribute SUPPORTED_ATTRIBUTES length [%d] incorrect should be at lease 3",
                          recvA->length);
		return NULL;
	}

	int i;
	bfcp_supported_list *first, *previous, *next;
	unsigned short int ch16;	/* 16 bits */
	unsigned char *buffer = message->buffer+recvA->position+2;	/* Skip the Header */
	int number = (recvA->length-2)/2;	/* Each supported attribute takes 2 bytes */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "SUPPORTED_ATTRIBUTES number of attributes %d\n", number);
	if(!number)
		return NULL;	/* No supported attributes? */
	first = calloc(1, sizeof(bfcp_supported_list));
	if(!first)	/* An error occurred in creating a new Supported Attributes list */
		return NULL;
	memcpy(&ch16, buffer, 2);
	first->element = ntohs(ch16);
	previous = first;
	if(number>1) {		/* Let's parse each other supported attribute we find */
		for(i = 1;i<number;i++) {
			next = calloc(1, sizeof(bfcp_supported_list));
			if(!next) {/* An error occurred in creating a new Supported Attributes list */
				switch_log_printf(SWITCH_CHANNEL_LOG,
                                  SWITCH_LOG_ERROR,
                                  "Failed to allocate memory to parse SUPPORTED_ATTRIBUTES at index %d.\n",
                                  i);
				return NULL;
			}

			buffer = buffer+2;	/* Skip to the next supported attribute */
			memcpy(&ch16, buffer, 2);
			next->element = ntohs(ch16);
			previous->next = next;
			previous = next;

			switch_log_printf(SWITCH_CHANNEL_LOG,
                              SWITCH_LOG_INFO,
                              "SUPPORTED_ATTRIBUTE: [%d-%s]\n",
                              next->element,
                              get_bfcp_attribute(next->element));
		}
	}
	return first;
}

bfcp_supported_list *bfcp_parse_attribute_SUPPORTED_PRIMITIVES(bfcp_message *message, bfcp_received_attribute *recvA)
{
	if(recvA->length<3) {/* The length of this attribute is wrong */
		switch_log_printf(SWITCH_CHANNEL_LOG,
                          SWITCH_LOG_ERROR,
                          "parse attribute SUPPORTED_PRIMITIVES invalid length [%d]. Should be more than 3.\n",
                          recvA->length);
		return NULL;
	}

	int i;
	bfcp_supported_list *first, *previous, *next;
	unsigned short int ch16;	/* 16 bits */
	unsigned char *buffer = message->buffer+recvA->position+2;	/* Skip the Header */
	int number = (recvA->length-2)/2;	/* Each supported primitive takes 2 bytes */
	if(!number)
		return NULL;	/* No supported primitives? */

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Parse attribute SUPPORTED_PRIMITIVES number of primitives %d.\n", number);

	first = calloc(1, sizeof(bfcp_supported_list));
	if(!first)	/* An error occurred in creating a new Supported Attributes list */
		return NULL;
	memcpy(&ch16, buffer, 2);
	first->element = ntohs(ch16);
	previous = first;
	if(number>1) {		/* Let's parse each other supported primitive we find */
		for(i = 1;i<number;i++) {
			next = calloc(1, sizeof(bfcp_supported_list));
			if(!next)	/* An error occurred in creating a new Supported Attributes list */
				return NULL;
			buffer = buffer+2;	/* Skip to the next supported primitive */
			memcpy(&ch16, buffer, 2);
			next->element = ntohs(ch16);
			previous->next = next;
			previous = next;

			switch_log_printf(SWITCH_CHANNEL_LOG,
                              SWITCH_LOG_INFO,
                              "Parse attribute SUPPORTED_PRIMITIVES [%d-%s]\n",
                              next->element,
                              get_bfcp_primitive(next->element));
		}
	}
	return first;
}

char *bfcp_parse_attribute_USER_DISPLAY_NAME(bfcp_message *message, bfcp_received_attribute *recvA)
{
	if(recvA->length<3) {/* The length of this attribute is wrong */
		switch_log_printf(SWITCH_CHANNEL_LOG,
                          SWITCH_LOG_ERROR,
                          "Parse attribute USER_DISPLAY_NAME invalid length [%d]. Should be more than 3.\n",
                          recvA->length);
		return NULL;
	}
	char ch = '\0';
	unsigned char *buffer = message->buffer+recvA->position+2;	/* Skip the Header */
	char *display = calloc(recvA->length-1, sizeof(char));	/* Lenght is TLV Header too */
	memcpy(display, buffer, recvA->length-2);
	memcpy(display+(recvA->length-2), &ch, 1);	/* We add a terminator char to the string */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Parse attribute USER_DISPLAY_NAME [%s]\n", display? display:"NULL");
	return display;
}

char *bfcp_parse_attribute_USER_URI(bfcp_message *message, bfcp_received_attribute *recvA)
{
	if(recvA->length<3) {/* The length of this attribute is wrong */
		switch_log_printf(SWITCH_CHANNEL_LOG,
                          SWITCH_LOG_ERROR,
                          "Parse attribute USER_URI invalid length [%d]. Should be more than 3.\n",
                          recvA->length);
		return NULL;
	}
	char ch = '\0';
	unsigned char *buffer = message->buffer+recvA->position+2;	/* Skip the Header */
	char *uri = calloc(recvA->length-1, sizeof(char));	/* Lenght is TLV Header too */
	memcpy(uri, buffer, recvA->length-2);
	memcpy(uri+(recvA->length-2), &ch, 1);	/* We add a terminator char to the string */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "USER_URI [%s]\n", uri? uri:"NULL");
	return uri;
}

bfcp_user_information *bfcp_parse_attribute_BENEFICIARY_INFORMATION(bfcp_message *message, bfcp_received_attribute *recvA)
{
	if(recvA->length<3) {/* The length of this attribute is wrong */
		switch_log_printf(SWITCH_CHANNEL_LOG,
                          SWITCH_LOG_ERROR,
                          "Parse attribute BENEFICIARY_INFORMATION invalid length [%d]. Should be more than 3.\n",
                          recvA->length);
		return NULL;
	}
	int position;
	bfcp_received_attribute *attribute = NULL;
	bfcp_user_information *beneficiary = NULL;
	unsigned short int ch16, bID;	/* 16 bits */
	char *display = NULL, *uri = NULL;
	unsigned char *buffer = message->buffer+recvA->position+2;	/* Skip the Header */
	memcpy(&ch16, buffer, 2);
	bID = ntohs(ch16);	/* The first value, BeneficiaryID */
	/* Display and URI are not compulsory, they might not be in the message, let's check it */

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "BENEFICIARY_INFORMATION(Beneficiary ID[%d])\n", bID);

	message->position = recvA->position+4;
	while(recvA->length>((message->position)-(recvA->position))) {
		position = message->position;	/* Remember where message was before attribute parsing */
		attribute = bfcp_parse_attribute(message);
		if(!attribute)	/* An error occurred while parsing this attribute */
			return NULL;
		switch(attribute->type) {
			case USER_DISPLAY_NAME:
				display = bfcp_parse_attribute_USER_DISPLAY_NAME(message, attribute);
				if(!display)	/* An error occurred while parsing this attribute */
					return NULL;
				break;
			case USER_URI:
				uri = bfcp_parse_attribute_USER_URI(message, attribute);
				if(!uri)	/* An error occurred while parsing this attribute */
					return NULL;
				break;
			default:	/* There's an attribute that shouldn't be here... */
				break;
		}
		message->position = position+attribute->length;
		if(((attribute->length)%4) != 0)
			message->position = message->position+4-((attribute->length)%4);
		bfcp_free_received_attribute(attribute);
	}
	beneficiary = bfcp_new_user_information(bID, display, uri);
	if(!beneficiary)	/* An error occurred in creating a new Beneficiary User Information */
		return NULL;
	if(display)
		free(display);
	if(uri)
		free(uri);
	message->position = recvA->position+recvA->length;
	if(((recvA->length)%4) != 0)
		message->position = message->position+4-((recvA->length)%4);
	return beneficiary;
}

bfcp_floor_request_information *bfcp_parse_attribute_FLOOR_REQUEST_INFORMATION(bfcp_message *message, bfcp_received_attribute *recvA)
{
	if(recvA->length<3) {/* The length of this attribute is wrong */
		switch_log_printf(SWITCH_CHANNEL_LOG,
                          SWITCH_LOG_ERROR,
                          "Parse attribute FLOOR_REQUEST_INFORMATION invalid length [%d]. Should be more than 3.\n",
                          recvA->length);
		return NULL;
	}
	int position;
	unsigned short int frqID, priority = 0;
	bfcp_floor_request_information *frqInfo = NULL;
	bfcp_received_attribute *attribute = NULL;
	unsigned short int ch16;	/* 16 bits */
	bfcp_overall_request_status *oRS = NULL;
	bfcp_floor_request_status *fRS = NULL, *tempRS = NULL;
	bfcp_user_information *beneficiary = NULL, *requested_by = NULL;
	char *pInfo = NULL;
	unsigned char *buffer = message->buffer+recvA->position+2;	/* Skip the Header */
	memcpy(&ch16, buffer, 2);
	frqID = ntohs(ch16);	/* The first value, FloorRequestID */
	/* Some attributes are not compulsory, they might not be in the message, let's check it */
	/* FLOOR-REQUEST-STATUS has 1* multiplicity, there has to be AT LEAST one, */
	/* So remember to check its presence outside, in server/client */

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "FLOOR_REQUEST_INFORMATION(Floor request ID[%d])\n", frqID);

	message->position = recvA->position+4;
	while(recvA->length>((message->position)-(recvA->position))) {
		position = message->position;	/* Remember where message was before attribute parsing */
		attribute = bfcp_parse_attribute(message);
		if(!attribute)	/* An error occurred while parsing this attribute */
			return NULL;

		switch_log_printf(SWITCH_CHANNEL_LOG,
                          SWITCH_LOG_INFO,
                          "Parse attribute FLOOR_REQUEST_INFORMATION Floor request ID[%d] Attribute type(%s)\n",
                          frqID,
                          get_bfcp_attribute(attribute->type));

		switch(attribute->type) {
			case OVERALL_REQUEST_STATUS:
				oRS = bfcp_parse_attribute_OVERALL_REQUEST_STATUS(message, attribute);
				if(!oRS)	/* An error occurred while parsing this attribute */
					return NULL;
				break;
			case FLOOR_REQUEST_STATUS:
				if(!fRS) {	/* Create a list, it's the first FLOOR-REQUEST-STATUS we add */
					fRS = bfcp_parse_attribute_FLOOR_REQUEST_STATUS(message, attribute);
					if(!fRS)	/* An error occurred while parsing this attribute */
						return NULL;
					break;
				} else {	/* We already have a list, add the new FLOOR-REQUEST-STATUS to it */
					tempRS = bfcp_parse_attribute_FLOOR_REQUEST_STATUS(message, attribute);
					if(!tempRS)	/* An error occurred while parsing this attribute */
						return NULL;
					if(bfcp_add_floor_request_status_list(fRS, tempRS, NULL) == -1)
						return NULL;
					break;
				}
			case BENEFICIARY_INFORMATION:
				beneficiary = bfcp_parse_attribute_BENEFICIARY_INFORMATION(message, attribute);
				if(!beneficiary)	/* An error occurred while parsing this attribute */
					return NULL;
				break;
			case REQUESTED_BY_INFORMATION:
				requested_by = bfcp_parse_attribute_REQUESTED_BY_INFORMATION(message, attribute);
				if(!requested_by)	/* An error occurred while parsing this attribute */
					return NULL;
				break;
			case PRIORITY:
				priority = bfcp_parse_attribute_PRIORITY(message, attribute);
				if(priority>4)		/* An error occurred while parsing this attribute */
					return NULL;
				break;
			case PARTICIPANT_PROVIDED_INFO:
				pInfo = bfcp_parse_attribute_PARTICIPANT_PROVIDED_INFO(message, attribute);
				if(!pInfo)		/* An error occurred while parsing this attribute */
					return NULL;
				break;
			default:	/* There's an attribute that shouldn't be here... */
				break;
		}
		message->position = position+attribute->length;
		if(((attribute->length)%4) != 0)
			message->position = message->position+4-((attribute->length)%4);
		bfcp_free_received_attribute(attribute);
	}
	frqInfo = bfcp_new_floor_request_information(frqID, oRS, fRS, beneficiary, requested_by, priority, pInfo);
	if(!frqInfo)	/* An error occurred in creating a new Floor Request Information */
		return NULL;
	if(pInfo)
		free(pInfo);
	message->position = recvA->position+recvA->length;
	if(((recvA->length)%4) != 0)
		message->position = message->position+4-((recvA->length)%4);
	return frqInfo;
}

bfcp_user_information *bfcp_parse_attribute_REQUESTED_BY_INFORMATION(bfcp_message *message, bfcp_received_attribute *recvA)
{
	if(recvA->length<3) {/* The length of this attribute is wrong */
		switch_log_printf(SWITCH_CHANNEL_LOG,
                          SWITCH_LOG_ERROR,
                          "Parse attribute REQUEST_BY_INFORMATION invalid length [%d]. Should be more than 3.\n",
                          recvA->length);
		return NULL;
	}

	int position;
	bfcp_received_attribute *attribute = NULL;
	bfcp_user_information *requested_by = NULL;
	unsigned short int ch16, rID;	/* 16 bits */
	char *display = NULL, *uri = NULL;
	unsigned char *buffer = message->buffer+recvA->position+2;	/* Skip the Header */
	memcpy(&ch16, buffer, 2);
	rID = ntohs(ch16);	/* The first value, ReceivedByID */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Parse attribute REQUESTED_BY_INFORMATION Requested by ID[%d].\n", rID);
	/* Display and URI are not compulsory, they might not be in the message, let's check it */
	message->position = recvA->position+4;
	while(recvA->length>((message->position)-(recvA->position))) {
		position = message->position;	/* Remember where message was before attribute parsing */
		attribute = bfcp_parse_attribute(message);
		if(!attribute)	/* An error occurred while parsing this attribute */
			return NULL;

		switch_log_printf(SWITCH_CHANNEL_LOG,
                          SWITCH_LOG_INFO,
                          "Parse attribute REQUESTED_BY_INFORMATION request ID[%d] Attribute type(%s).\n",
                          rID,
                          get_bfcp_attribute(attribute->type));

		switch(attribute->type) {
			case USER_DISPLAY_NAME:
				display = bfcp_parse_attribute_USER_DISPLAY_NAME(message, attribute);
				if(!display)	/* An error occurred while parsing this attribute */
					return NULL;
				break;
			case USER_URI:
				uri = bfcp_parse_attribute_USER_URI(message, attribute);
				if(!uri)	/* An error occurred while parsing this attribute */
					return NULL;
				break;
			default:	/* There's an attribute that shouldn't be here... */
				break;
		}
		message->position = position+attribute->length;
		if(((attribute->length)%4) != 0)
			message->position = message->position+4-((attribute->length)%4);
		bfcp_free_received_attribute(attribute);
	}
	requested_by = bfcp_new_user_information(rID, display, uri);
	if(!requested_by)	/* An error occurred in creating a new Beneficiary User Information */
		return NULL;
	if(display)
		free(display);
	if(uri)
		free(uri);
	message->position = recvA->position+recvA->length;
	if(((recvA->length)%4) != 0)
		message->position = message->position+4-((recvA->length)%4);
	return requested_by;
}

bfcp_floor_request_status *bfcp_parse_attribute_FLOOR_REQUEST_STATUS(bfcp_message *message, bfcp_received_attribute *recvA)
{
	if(recvA->length<3) {/* The length of this attribute is wrong */
		switch_log_printf(SWITCH_CHANNEL_LOG,
                          SWITCH_LOG_ERROR,
                          "Parse attribute FLOOR_REQUEST_STATUS invalid length [%d]. Should be more than 3.\n",
                          recvA->length);
		return NULL;
	}
	int position;
	unsigned short int fID;
	bfcp_received_attribute *attribute = NULL;
	bfcp_floor_request_status *fRS = NULL;
	unsigned short int ch16;	/* 16 bits */
	bfcp_request_status *rs = NULL;
	char *sInfo = NULL;
	unsigned char *buffer = message->buffer+recvA->position+2;	/* Skip the Header */
	memcpy(&ch16, buffer, 2);
	fID = ntohs(ch16);	/* The first value, FloorID */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "FLOOR_REQUEST_STATUS floorId [%d]\n", fID);
	/* Some attributes are not compulsory, they might not be in the message, let's check it */
	message->position = recvA->position+4;
	while(recvA->length>((message->position)-(recvA->position))) {
		position = message->position;	/* Remember where message was before attribute parsing */
		attribute = bfcp_parse_attribute(message);
		if(!attribute)	/* An error occurred while parsing this attribute */
			return NULL;
		switch(attribute->type) {
			case REQUEST_STATUS:
				rs = bfcp_parse_attribute_REQUEST_STATUS(message, attribute);
				if(!rs)	/* An error occurred while parsing this attribute */
					return NULL;
				break;
			case STATUS_INFO:
				sInfo = bfcp_parse_attribute_STATUS_INFO(message, attribute);
				if(!sInfo)	/* An error occurred while parsing this attribute */
					return NULL;
				break;
			default:	/* There's an attribute that shouldn't be here... */
				switch_log_printf(SWITCH_CHANNEL_LOG,
                                  SWITCH_LOG_ERROR,
                                  "FLOOR_REQUEST_STATUS - unexpected attribute type(%s)\n",
                                  get_bfcp_attribute(attribute->type));
				break;
		}
		message->position = position+attribute->length;
		if(((attribute->length)%4) != 0)
			message->position = message->position+4-((attribute->length)%4);
		bfcp_free_received_attribute(attribute);
	}
	fRS = bfcp_new_floor_request_status(fID, rs->rs, rs->qp, sInfo);
	if(!fRS)	/* An error occurred in creating a new Floor Request Information */
		return NULL;
	if(rs)
		bfcp_free_request_status(rs);
	if(sInfo)
		free(sInfo);
	message->position = recvA->position+recvA->length;
	if(((recvA->length)%4) != 0)
		message->position = message->position+4-((recvA->length)%4);
	return fRS;
}

bfcp_overall_request_status *bfcp_parse_attribute_OVERALL_REQUEST_STATUS(bfcp_message *message, bfcp_received_attribute *recvA)
{
	if(recvA->length<3) {/* The length of this attribute is wrong */
		switch_log_printf(SWITCH_CHANNEL_LOG,
                          SWITCH_LOG_ERROR,
                          "Parse attribute OVERALL_REQUEST_STATUS invalid length [%d]. Should be more than 3.\n",
                          recvA->length);
		return NULL;
	}
	int position;
	unsigned short int frqID;
	bfcp_received_attribute *attribute = NULL;
	bfcp_overall_request_status *oRS = NULL;
	unsigned short int ch16;	/* 16 bits */
	bfcp_request_status *rs = NULL;
	char *sInfo = NULL;
	unsigned char *buffer = message->buffer+recvA->position+2;	/* Skip the Header */
	memcpy(&ch16, buffer, 2);
	frqID = ntohs(ch16);	/* The first value, FloorID */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Parse attribute OVERALL_REQUEST_STATUS FloorRequestId[%d]\n", frqID);
	/* Some attributes are not compulsory, they might not be in the message, let's check it */
	message->position = recvA->position+4;
	while(recvA->length>((message->position)-(recvA->position))) {
		position = message->position;	/* Remember where message was before attribute parsing */
		attribute = bfcp_parse_attribute(message);
		if(!attribute)	/* An error occurred while parsing this attribute */
			return NULL;
		switch_log_printf(SWITCH_CHANNEL_LOG,
                          SWITCH_LOG_INFO,
                          "Parse attribute OVERALL_REQUEST_STATUS Requested by(%s).\n",
                          get_bfcp_attribute(attribute->type));

		switch(attribute->type) {
			case REQUEST_STATUS:
				rs = bfcp_parse_attribute_REQUEST_STATUS(message, attribute);
				if(!rs)	/* An error occurred while parsing this attribute */
					return NULL;
				break;
			case STATUS_INFO:
				sInfo = bfcp_parse_attribute_STATUS_INFO(message, attribute);
				if(!sInfo)	/* An error occurred while parsing this attribute */
					return NULL;
				break;
			default:	/* There's an attribute that shouldn't be here... */
				break;
		}
		message->position = position+attribute->length;
		if(((attribute->length)%4) != 0)
			message->position = message->position+4-((attribute->length)%4);
		bfcp_free_received_attribute(attribute);
	}
	oRS = bfcp_new_overall_request_status(frqID, rs->rs, rs->qp, sInfo);
	if(!oRS)	/* An error occurred in creating a new Floor Request Information */
		return NULL;
	if(rs)
		bfcp_free_request_status(rs);
	if(sInfo)
		free(sInfo);
	message->position = recvA->position+recvA->length;
	if(((recvA->length)%4) != 0)
		message->position = message->position+4-((recvA->length)%4);
	return oRS;
}

int bfcp_parse_attribute_NONCE(bfcp_message *message, bfcp_received_attribute *recvA)
{
	if(recvA->length != 4) {/* The length of this attribute is wrong */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse attribute NONCE length [%d]. Should be 4.\n", recvA->length);
		return -1;
	}
	unsigned short int ch16;	/* 16 bits */
	unsigned char *buffer = message->buffer+recvA->position+2;	/* Skip the Header */
	memcpy(&ch16, buffer, 2);
	ch16 = ntohs(ch16);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Parse attribute NONCE [%d]\n", ch16);
	return ch16;
}

bfcp_digest *bfcp_parse_attribute_DIGEST(bfcp_message *message, bfcp_received_attribute *recvA)
{
	if(recvA->length<4) {/* The length of this attribute is wrong */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse attribute DIGEST length [%d]. Should be at least 4.\n", recvA->length);
		return NULL;
	}
	char ch;	/* 8 bits */
	unsigned char *buffer = message->buffer+recvA->position+2;	/* Skip the Header */
	memcpy(&ch, buffer, 1);
	bfcp_digest *digest = bfcp_new_digest((unsigned short int)ch);	/* Algorithm */
	buffer = buffer+1;	/* Skip the Algorithm byte */
	digest->text = calloc(recvA->length-2, sizeof(char));	/* Lenght is TLV + Algorithm byte too */
	if(!digest)	/* An error occurred in creating a new Digest */
		return NULL;
	memcpy(digest->text, buffer, recvA->length-3);
	ch = '\0';
	memcpy((digest->text)+(recvA->length-3), &ch, 1);	/* We add a terminator char to the string */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Parse attribute DIGEST text[%s]\n", digest->text);
	return digest;
}
