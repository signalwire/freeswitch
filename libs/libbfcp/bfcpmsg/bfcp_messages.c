#include "bfcp_messages.h"

/* Create a New Arguments Structure */
bfcp_arguments *bfcp_new_arguments(void)
{
	bfcp_arguments *arguments = calloc(1, sizeof(bfcp_arguments));
	if(!arguments)	/* We could not allocate the memory, return with a failure */
		return NULL;
	arguments->primitive = 0;
	arguments->entity = NULL;
	arguments->fID = NULL;
	arguments->frqID = 0;
	arguments->bID = 0;
	arguments->priority = 0;
	arguments->frqInfo = 0;
	arguments->beneficiary = NULL;
	arguments->rs = NULL;
	arguments->pInfo = NULL;
	arguments->sInfo = NULL;
	arguments->error = NULL;
	arguments->eInfo = NULL;
	arguments->primitives = NULL;
	arguments->attributes = NULL;
	arguments->nonce = 0;
	arguments->digest = NULL;
	return arguments;
}

/* Free an Arguments Structure */
int bfcp_free_arguments(bfcp_arguments *arguments)
{
	int res = 0;	/* We keep track here of the results of the sub-freeing methods*/
	if(!arguments)	/* There's nothing to free, return with a failure */
		return -1;
	if(arguments->entity)
		res += bfcp_free_entity(arguments->entity);
	if(arguments->fID)
		res += bfcp_free_floor_id_list(arguments->fID);
	if(arguments->frqInfo)
		res += bfcp_free_floor_request_information_list(arguments->frqInfo);
	if(arguments->beneficiary)
		res += bfcp_free_user_information(arguments->beneficiary);
	if(arguments->rs)
		res += bfcp_free_request_status(arguments->rs);
	if(arguments->pInfo)
		free(arguments->pInfo);
	if(arguments->sInfo)
		free(arguments->sInfo);
	if(arguments->error)
		res += bfcp_free_error(arguments->error);
	if(arguments->eInfo)
		free(arguments->eInfo);
	if(arguments->primitives)
		res += bfcp_free_supported_list(arguments->primitives);
	if(arguments->attributes)
		res += bfcp_free_supported_list(arguments->attributes);
	if(arguments->digest)
		res += bfcp_free_digest(arguments->digest);
	free(arguments);
	if(!res)	/* No error occurred, succesfully freed the structure */
		return 0;
	else		/* res was not 0, so some error occurred, return with a failure */
		return -1;
}

/* Create a New Message (if buffer is NULL or length is 0, creates an empty message) */
bfcp_message *bfcp_new_message(unsigned char *buffer, unsigned short int length)
{
	bfcp_message *message = calloc(1, sizeof(bfcp_message));
	if(!message)	/* We could not allocate the memory, return a with failure */
		return NULL;
	if(!buffer) {	/* Buffer is empty, so we want an empy message, a template */
		message->buffer = calloc(BFCP_MAX_ALLOWED_SIZE, sizeof(unsigned char));
		message->position = 12;			/* We start after the Common Header (12 octets) */
		message->length = 12;			/* even if we haven't written it yet */
	} else {	/* Buffer is not empty, create a message around it */
		message->buffer = calloc(length, sizeof(unsigned char));
		memcpy(message->buffer, buffer, length);	/* We copy the buffer in our message */
		message->position = 0;				/* Start from the beginning */
		message->length = length;			/* The length of the message is the length we pass */
	}
	return message;
}

/* Create a Copy of a Message */
bfcp_message *bfcp_copy_message(bfcp_message *message)
{
	if(!message)	/* The message is not valid, return with a failure */
		return NULL;
	bfcp_message *copy = calloc(sizeof(bfcp_message), sizeof(unsigned char));
	if(!copy)	/* We could not allocate the memory, return a with failure */
		return NULL;
	copy->position = 0;
	copy->length = message->length;
	copy->buffer = calloc(copy->length, sizeof(unsigned char));
	memcpy(copy->buffer, message->buffer, copy->length);
	return copy;
}

/* Free a Message */
int bfcp_free_message(bfcp_message *message)
{
	if(!message)	/* There's nothing to free, return with a failure */
		return -1;
	if(message->buffer)
		free(message->buffer);
	free(message);
	return 0;
}

/* Create a New Entity (Conference ID, Transaction ID, User ID) */
bfcp_entity *bfcp_new_entity(unsigned long int conferenceID, unsigned short int transactionID, unsigned short int userID)
{
	bfcp_entity *entity = calloc(1, sizeof(bfcp_entity));
	if(!entity)	/* We could not allocate the memory, return a with failure */
		return NULL;
	entity->conferenceID = conferenceID;
	entity->transactionID = transactionID;
	entity->userID = userID;
	return entity;
}

/* Free an Entity */
int bfcp_free_entity(bfcp_entity *entity)
{
	if(!entity)	/* There's nothing to free, return with a failure */
		return -1;
	free(entity);
	return 0;
}

/* Create a new Floor ID list (first argument must be a valid ID, last argument MUST be 0) */
bfcp_floor_id_list *bfcp_new_floor_id_list(unsigned short int fID, ...)
{
	bfcp_floor_id_list *first, *previous, *next;
	va_list ap;
	va_start(ap, fID);
	first = calloc(1, sizeof(bfcp_floor_id_list));
	if(!first)	/* We could not allocate the memory, return a with failure */
		return NULL;
	first->ID = fID;
	first->next = NULL;
	previous = first;
	fID = va_arg(ap, int);
	while(fID) {
		next = calloc(1, sizeof(bfcp_floor_id_list));
		if(!next)	/* We could not allocate the memory, return a with failure */
			return NULL;
		next->ID = fID;
		next->next = NULL;
		previous->next = next;
		previous = next;
		fID = va_arg(ap, int);
	}
	va_end(ap);
	return first;
}

/* Add IDs to an existing Floor ID list (last argument MUST be 0) */
int bfcp_add_floor_id_list(bfcp_floor_id_list *list, unsigned short int fID, ...)
{
	bfcp_floor_id_list *previous, *next;
	va_list ap;
	va_start(ap, fID);
	if(!list)	/* List doesn't exist, return a with failure */
		return -1;
	next = list;
	while(next) {	/* We search the last element in the list, to append the new IDs to */
		previous = next;
		next = previous->next;
	}	/* previous is now the pointer to the actually last element in the list */
	while(fID) {
		next = calloc(1, sizeof(bfcp_floor_id_list));
		if(!next)	/* We could not allocate the memory, return a with failure */
			return -1;
		next->ID = fID;
		next->next = NULL;
		previous->next = next;	/* We append the new ID to the list */
		previous = next;		/* and we update the pointers */
		fID = va_arg(ap, int);
	}
	va_end(ap);
	return 0;
}

/* Free a Floor ID list */
int bfcp_free_floor_id_list(bfcp_floor_id_list *list)
{
	if(!list)	/* There's nothing to free, return with a failure */
		return -1;
	bfcp_floor_id_list *next = NULL, *temp = list;
	while(temp) {
		next = temp->next;
		free(temp);
		temp = next;
	}
	return 0;
}

/* Create a new Supported (Primitives/Attributes) list (last argument MUST be 0) */
bfcp_supported_list *bfcp_new_supported_list(unsigned short int element, ...)
{
	bfcp_supported_list *first, *previous, *next;
	va_list ap;
	va_start(ap, element);
	first = calloc(1, sizeof(bfcp_supported_list));
	if(!first)	/* We could not allocate the memory, return a with failure */
		return NULL;
	first->element = element;
	previous = first;
	element = va_arg(ap, int);
	while(element) {
		next = calloc(1, sizeof(bfcp_supported_list));
		if(!next)	/* We could not allocate the memory, return a with failure */
			return NULL;
		next->element = element;
		previous->next = next;
		previous = next;
		element = va_arg(ap, int);
	}
	va_end(ap);
	return first;
}

/* Free a Supported (Primitives/Attributes) list */
int bfcp_free_supported_list(bfcp_supported_list *list)
{
	if(!list)	/* There's nothing to free, return with a failure */
		return -1;
	bfcp_supported_list *next = NULL, *temp = list;
	while(temp) {
		next = temp->next;
		free(temp);
		temp = next;
	}
	return 0;
}

/* Create a New Request Status (RequestStatus/QueuePosition) */
bfcp_request_status *bfcp_new_request_status(unsigned short int rs, unsigned short int qp)
{
	bfcp_request_status *request_status = calloc(1, sizeof(bfcp_request_status));
	if(!request_status)	/* We could not allocate the memory, return a with failure */
		return NULL;
	request_status->rs = rs;
	request_status->qp = qp;
	return request_status;
}

/* Free a Request Status (RequestStatus/QueuePosition) */
int bfcp_free_request_status(bfcp_request_status *request_status)
{
	if(!request_status)	/* There's nothing to free, return with a failure */
		return -1;
	free(request_status);
	return 0;
}

/* Create a New Error (Code/Details) */
bfcp_error *bfcp_new_error(unsigned short int code, void *details)
{
	bfcp_error *error = calloc(1, sizeof(bfcp_error));
	if(!error)	/* We could not allocate the memory, return a with failure */
		return NULL;
	error->code = code;
	error->details = details;
	return error;
}

/* Free an Error (Code/Details) */
int bfcp_free_error(bfcp_error *error)
{
	if(!error)	/* There's nothing to free, return with a failure */
		return -1;
	return bfcp_free_unknown_m_error_details_list(error->details);
}

/* Create a New Error Details list (for Error 4: UNKNOWN_M) (last argument MUST be 0) */
bfcp_unknown_m_error_details *bfcp_new_unknown_m_error_details_list(unsigned short int attribute, ...)
{
	bfcp_unknown_m_error_details *first, *previous, *next;
	va_list ap;
	va_start(ap, attribute);
	first = calloc(1, sizeof(bfcp_unknown_m_error_details));
	if(!first)	/* We could not allocate the memory, return a with failure */
		return NULL;
	first->unknown_type = attribute;
	first->reserved = 0;
	previous = first;
	attribute = va_arg(ap, int);
	while(attribute) {
		next = calloc(1, sizeof(bfcp_unknown_m_error_details));
		if(!next)	/* We could not allocate the memory, return a with failure */
			return NULL;
		next->unknown_type = attribute;
		next->reserved = 0;
		previous->next = next;
		previous = next;
		attribute = va_arg(ap, int);
	}
	va_end(ap);
	return first;
}

/* Add Attributes to an existing Error Details list (for Error 4: UNKNOWN_M) (last argument MUST be 0) */
int bfcp_add_unknown_m_error_details_list(bfcp_unknown_m_error_details *list, unsigned short int attribute, ...)
{
	bfcp_unknown_m_error_details *previous, *next;
	va_list ap;
	va_start(ap, attribute);
	if(!list)	/* List doesn't exist, return a with failure */
		return -1;
	next = list;
	while(next) {	/* We search the last element in the list, to append the new Attributes to */
		previous = next;
		next = previous->next;
	}	/* previous is now the pointer to the actually last element in the list */
	while(attribute) {
		next = calloc(1, sizeof(bfcp_unknown_m_error_details));
		if(!next)	/* We could not allocate the memory, return a with failure */
			return -1;
		next->unknown_type = attribute;
		next->reserved = 0;
		previous->next = next;	/* We append the new ID to the list */
		previous = next;		/* and we update the pointers */
		attribute = va_arg(ap, int);
	}
	va_end(ap);
	return 0;
}

/* Free an Error Details list */
int bfcp_free_unknown_m_error_details_list(bfcp_unknown_m_error_details *details)
{
	if(!details)	/* There's nothing to free, return with a failure */
		return -1;
	bfcp_unknown_m_error_details *next = NULL, *temp = details;
	while(temp) {
		next = temp->next;
		free(temp);
		temp = next;
	}
	return 0;
}

/* Create a New User (Beneficiary/RequestedBy) Information */
bfcp_user_information *bfcp_new_user_information(unsigned short int ID, char *display, char *uri)
{
	bfcp_user_information *info = calloc(1, sizeof(bfcp_user_information));
	if(!info)	/* We could not allocate the memory, return a with failure */
		return NULL;
	info->ID = ID;
	if(display) {
		info->display = calloc(strlen(display)+1, sizeof(char));
		info->display = strcpy(info->display, display);	/* We copy the Display string */
	}
	if(uri) {
		info->uri = calloc(strlen(uri)+1, sizeof(char));
		info->uri = strcpy(info->uri, uri);		/* We copy the URI string */
	}
	return info;
}

/* Free an User (Beneficiary/RequestedBy) Information */
int bfcp_free_user_information(bfcp_user_information *info)
{
	if(!info)	/* There's nothing to free, return with a failure */
		return -1;
	if(info->display)
		free(info->display);
	if(info->uri)
		free(info->uri);
	free(info);
	return 0;
}

/* Create a new Floor Request Information */
bfcp_floor_request_information *bfcp_new_floor_request_information(unsigned short int frqID, bfcp_overall_request_status *oRS, bfcp_floor_request_status *fRS, bfcp_user_information *beneficiary, bfcp_user_information *requested_by, unsigned short int priority , char *pInfo)
{
	bfcp_floor_request_information *frqInfo = calloc(1, sizeof(bfcp_floor_request_information));
	if(!frqInfo)	/* We could not allocate the memory, return a with failure */
		return NULL;
	frqInfo->frqID = frqID;
	frqInfo->oRS = oRS;
	frqInfo->fRS = fRS;
	frqInfo->beneficiary = beneficiary;
	frqInfo->requested_by = requested_by;
	frqInfo->priority = priority;
	if(pInfo) {
		frqInfo->pInfo = calloc(strlen(pInfo)+1, sizeof(char));
		frqInfo->pInfo = strcpy(frqInfo->pInfo, pInfo);	/* We copy the Participant Provided Info */
	}
	frqInfo->next = NULL;	/* We link them through bfcp_list_floor_request_information (...) */
	return frqInfo;
}

/* Create a Floor Request Information list (last argument MUST be NULL) */
int bfcp_list_floor_request_information(bfcp_floor_request_information *frqInfo, ...)
{
	bfcp_floor_request_information *previous, *next;
	va_list ap;
	va_start(ap, frqInfo);
	if(!frqInfo)	/* The lead pointer is not valid, return a with failure */
		return -1;
	previous = frqInfo;
	next = va_arg(ap, bfcp_floor_request_information *);
	while(next) {
		previous->next = next;
		previous = next;
		next = va_arg(ap, bfcp_floor_request_information *);
	}
	va_end(ap);
	return 0;
}

/* Add elements to an existing Floor Request Information list (last argument MUST be NULL) */
int bfcp_add_floor_request_information_list(bfcp_floor_request_information *list, ...)
{
	bfcp_floor_request_information *previous, *next;
	va_list ap;
	va_start(ap, list);
	if(!list)	/* List doesn't exist, return a with failure */
		return -1;
	next = list;
	while(next) {	/* We search the last element in the list, to append the new IDs to */
		previous = next;
		next = previous->next;
	}	/* previous is now the pointer to the actually last element in the list */
	next = va_arg(ap, bfcp_floor_request_information *);
	while(next) {
		previous->next = next;	/* We append the new ID to the list */
		previous = next;		/* and we update the pointers */
		next = va_arg(ap, bfcp_floor_request_information *);
	}
	va_end(ap);
	return 0;
}

/* Free a Floor Request Information list */
int bfcp_free_floor_request_information_list(bfcp_floor_request_information *frqInfo)
{
	int res = 0;	/* We keep track here of the results of the sub-freeing methods*/
	if(!frqInfo)	/* There's nothing to free, return with a failure */
		return -1;
	bfcp_floor_request_information *next = NULL, *temp = frqInfo;
	while(temp) {
		next = temp->next;
		if(temp->oRS)
			res += bfcp_free_overall_request_status(temp->oRS);
		if(temp->fRS)
			res += bfcp_free_floor_request_status_list(temp->fRS);
		if(temp->beneficiary)
			res += bfcp_free_user_information(temp->beneficiary);
		if(temp->requested_by)
			res += bfcp_free_user_information(temp->requested_by);
		if(temp->pInfo)
			free(temp->pInfo);
		free(temp);
		temp = next;
	}
	if(!res)	/* No error occurred, succesfully freed the structure */
		return 0;
	else		/* res was not 0, so some error occurred, return with a failure */
		return -1;
}

/* Create a New Floor Request Status (FloorID/RequestStatus/QueuePosition/StatusInfo) */
bfcp_floor_request_status *bfcp_new_floor_request_status(unsigned short int fID, unsigned short int rs, unsigned short int qp, char *sInfo)
{
	bfcp_floor_request_status *floor_request_status = calloc(1, sizeof(bfcp_floor_request_status));
	if(!floor_request_status)	/* We could not allocate the memory, return a with failure */
		return NULL;
	floor_request_status->fID = fID;
	floor_request_status->rs = bfcp_new_request_status(rs, qp);
	if(!floor_request_status->rs)
		return NULL;
	if(sInfo) {
		floor_request_status->sInfo = calloc(strlen(sInfo)+1, sizeof(char));
		if(!floor_request_status->sInfo)
			return NULL;
		floor_request_status->sInfo = strcpy(floor_request_status->sInfo, sInfo);	/* We copy the Status Info */
	}
	floor_request_status->next = NULL;	/* We link them through bfcp_list_floor_request_status (...) */
	return floor_request_status;
}

/* Create a Floor Request Status list (last argument MUST be NULL) */
int bfcp_list_floor_request_status(bfcp_floor_request_status *fRS, ...)
{
	bfcp_floor_request_status *previous, *next;
	va_list ap;
	va_start(ap, fRS);
	if(!fRS)	/* The lead pointer is not valid, return a with failure */
		return -1;
	previous = fRS;
	next = va_arg(ap, bfcp_floor_request_status *);
	while(next) {
		previous->next = next;
		previous = next;
		next = va_arg(ap, bfcp_floor_request_status *);
	}
	va_end(ap);
	return 0;
}

/* Add elements to an existing Floor Request Status list (last argument MUST be NULL) */
int bfcp_add_floor_request_status_list(bfcp_floor_request_status *list, ...)
{
	bfcp_floor_request_status *previous, *next;
	va_list ap;
	va_start(ap, list);
	if(!list)	/* List doesn't exist, return a with failure */
		return -1;
	next = list;
	while(next) {	/* We search the last element in the list, to append the new IDs to */
		previous = next;
		next = previous->next;
	}	/* previous is now the pointer to the actually last element in the list */
	next = va_arg(ap, bfcp_floor_request_status *);
	while(next) {
		previous->next = next;	/* We append the new ID to the list */
		previous = next;		/* and we update the pointers */
		next = va_arg(ap, bfcp_floor_request_status *);
	}
	va_end(ap);
	return 0;
}

/* Free a Floor Request Status list */
int bfcp_free_floor_request_status_list(bfcp_floor_request_status *floor_request_status)
{
	int res = 0;	/* We keep track here of the results of the sub-freeing methods*/
	if(!floor_request_status)	/* There's nothing to free, return with a failure */
		return -1;
	bfcp_floor_request_status *next = NULL, *temp = floor_request_status;
	while(temp) {
		next = temp->next;
		if(temp->rs)
			res += bfcp_free_request_status(temp->rs);
		if(temp->sInfo)
			free(temp->sInfo);
		free(temp);
		temp = next;
	}
	if(!res)	/* No error occurred, succesfully freed the structure */
		return 0;
	else		/* res was not 0, so some error occurred, return with a failure */
		return -1;
}

/* Create a New Overall Request Status (FloorRequestID/RequestStatus/QueuePosition/StatusInfo) */
bfcp_overall_request_status *bfcp_new_overall_request_status(unsigned short int frqID, unsigned short int rs, unsigned short int qp, char *sInfo)
{
	bfcp_overall_request_status *overall_request_status = calloc(1, sizeof(bfcp_overall_request_status));
	if(!overall_request_status)	/* We could not allocate the memory, return a with failure */
		return NULL;
	overall_request_status->frqID = frqID;
	overall_request_status->rs = bfcp_new_request_status(rs, qp);
	if(!overall_request_status->rs)
		return NULL;
	if(sInfo) {
		overall_request_status->sInfo = calloc(strlen(sInfo)+1, sizeof(char));
		if(!overall_request_status->sInfo)
			return NULL;
		overall_request_status->sInfo = strcpy(overall_request_status->sInfo, sInfo);	/* We copy the Status Info */
	}
	return overall_request_status;
}

/* Free an Overall Request Status */
int bfcp_free_overall_request_status(bfcp_overall_request_status *overall_request_status)
{
	int res = 0;	/* We keep track here of the results of the sub-freeing methods*/
	if(!overall_request_status)	/* There's nothing to free, return with a failure */
		return -1;
	if(overall_request_status->rs)
		res += bfcp_free_request_status(overall_request_status->rs);
	if(overall_request_status->sInfo)
		free(overall_request_status->sInfo);
	free(overall_request_status);
	return res;
}

/* Create a New Digest */
bfcp_digest *bfcp_new_digest(unsigned short int algorithm)
{
	bfcp_digest *digest = calloc(1, sizeof(bfcp_digest));
	if(!digest)	/* We could not allocate the memory, return a with failure */
		return NULL;
	digest->algorithm = algorithm;
	digest->text = NULL;
	return digest;
}

/* Free a Digest */
int bfcp_free_digest(bfcp_digest *digest)
{
	if(!digest)	/* There's nothing to free, return with a failure */
		return -1;
	if(digest->text)
		free(digest->text);
	free(digest);
	return 0;
}
