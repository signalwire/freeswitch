#include "bfcp_messages.h"
#include "bfcp_strings.h"
/* Generic BuildMessage Method */
bfcp_message *bfcp_build_message(bfcp_arguments* arguments)
{
	if(!(arguments->entity))	/* Conference ID, Transaction ID and User ID are missing */
		return NULL;		/* This is a fatal error, return with a failure */
	if((arguments->primitive) == 0)	/* The Primitive identifier is missing or invalid */
		return NULL;		/* This is a fatal error, return with a failure */

	switch_log_printf(SWITCH_CHANNEL_LOG,
                      SWITCH_LOG_INFO,
                      "bfcp_build_message primitive[%d/%s]\n",
                      arguments->primitive,
                      get_bfcp_primitive(arguments->primitive));

	switch(arguments->primitive) {
		case FloorRequest:
			return bfcp_build_message_FloorRequest(arguments->entity, arguments->fID, arguments->bID, arguments->pInfo, arguments->priority);
		case FloorRelease:
			return bfcp_build_message_FloorRelease(arguments->entity, arguments->frqID);
		case FloorRequestQuery:
			return bfcp_build_message_FloorRequestQuery(arguments->entity, arguments->frqID);
		case FloorRequestStatus:
			return bfcp_build_message_FloorRequestStatus(arguments->entity, arguments->frqInfo);
		case UserQuery:
			return bfcp_build_message_UserQuery(arguments->entity, arguments->bID);
		case UserStatus:
			return bfcp_build_message_UserStatus(arguments->entity, arguments->beneficiary, arguments->frqInfo);
		case FloorQuery:
			return bfcp_build_message_FloorQuery(arguments->entity, arguments->fID);
		case FloorStatus:
			return bfcp_build_message_FloorStatus(arguments->entity, arguments->fID, arguments->frqInfo);
		case ChairAction:
			return bfcp_build_message_ChairAction(arguments->entity, arguments->frqInfo);
		case ChairActionAck:
			return bfcp_build_message_ChairActionAck(arguments->entity);
		case Hello:
			return bfcp_build_message_Hello(arguments->entity);
		case HelloAck:
			return bfcp_build_message_HelloAck(arguments->entity, arguments->primitives, arguments->attributes);
		case Error:
			return bfcp_build_message_Error(arguments->entity, arguments->error, arguments->eInfo);
		default:
			return NULL;	/* Unrecognized Primitive: return with a failure */
	}
}

/* Build the message Common Header */
void bfcp_build_commonheader(bfcp_message *message, bfcp_entity *entity, unsigned short int primitive)
{
	unsigned int ch32;		/* 32 bits */
	unsigned short int ch16;	/* 16 bits */
	unsigned char *buffer = message->buffer;
	ch32 = (((ch32 & !(0xE0000000)) | (1)) << 29) +			/* First the Version (3 bits, set to 001) */
		(((ch32 & !(0x1F000000)) | (0)) << 24) +		/* then the Reserved (5 bits, ignored) */
		(((ch32 & !(0x00FF0000)) | (primitive)) << 16) +	/* the Primitive (8 bits) */
		((ch32 & !(0x0000FFFF)) | (message->length - 12)/4);	/* and the payload length (16 bits), dividing it by 4 to be 4-byte units */
	ch32 = htonl(ch32);		/* We want all protocol values in network-byte-order */
	memcpy(buffer, &ch32, 4);
	buffer = buffer+4;
	ch32 = htonl(entity->conferenceID);
	memcpy(buffer, &ch32, 4);
	buffer = buffer+4;
	ch16 = htons(entity->transactionID);
	memcpy(buffer, &ch16, 2);
	ch16 = htons(entity->userID);
	buffer = buffer+2;
	memcpy(buffer, &ch16, 2);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Primitive - (%d-%s)\n", primitive, get_bfcp_primitive(primitive));
}

/* Build the first 16 bits of the Attribute */
void bfcp_build_attribute_tlv(bfcp_message *message, unsigned short int position, unsigned short int type, unsigned short int mandatory_bit, unsigned short int length)
{
	unsigned short int tlv;
	unsigned char *buffer = message->buffer+position;
	tlv = (((tlv & !(0xFE00)) | (type)) << 9) +		/* Type takes 7 bits */
		(((tlv & !(0x0100)) | (mandatory_bit)) << 8) +	/* M is a bit */
		((tlv & !(0x00FF)) | (length));			/* The Lenght takes 8 bits (max 255 bytes) */
	tlv = htons(tlv);
	memcpy(buffer, &tlv, 2);		/* We copy the TLV to the buffer */
}



/* Build Messages */

/* 1*[FLOOR-ID] */
bfcp_message *bfcp_build_message_FloorRequest(bfcp_entity *entity, bfcp_floor_id_list *fID, unsigned short int bID, char *pInfo, unsigned short int priority)
{
	int err;
	bfcp_message *message = bfcp_new_message(NULL, 0);
	if(!message)	/* We could not allocate the memory, return a with failure */
		return NULL;
	bfcp_floor_id_list *temp = fID;
	while(temp) {	/* There can be more than one FLOOR-ID attribute */
		if(!temp->ID)	/* We need a valid FloorID */
			return NULL;
		err = bfcp_build_attribute_FLOOR_ID(message, temp->ID);
		if(err == -1)	/* We couldn't build this attribute, return with a failure */
			return NULL;
		temp = temp->next;
	}
	if(bID)	{	/* This attribute is not compulsory */
		err = bfcp_build_attribute_BENEFICIARY_ID(message, bID);
		if(err == -1)	/* We couldn't build this attribute, return with a failure */
			return NULL;
	}
	if(pInfo) {	/* This attribute is not compulsory */
		err = bfcp_build_attribute_PARTICIPANT_PROVIDED_INFO(message, pInfo);
		if(err == -1)	/* We couldn't build this attribute, return with a failure */
			return NULL;
	}
	if(priority<5) {	/* This attribute is not compulsory (must be between 0 and 4) */
		err = bfcp_build_attribute_PRIORITY(message, priority);
		if(err == -1)	/* We couldn't build this attribute, return with a failure */
			return NULL;
	}
	/* Append the attributes buffer to the common header one */
	bfcp_build_commonheader(message, entity, FloorRequest);
	return message;
}

bfcp_message *bfcp_build_message_FloorRelease(bfcp_entity *entity, unsigned short int frqID)
{
	int err;
	bfcp_message *message = bfcp_new_message(NULL, 0);
	if(!message)	/* We could not allocate the memory, return a with failure */
		return NULL;
	if(!frqID)	/* We need a valid FloorRequestID */
		return NULL;
	err = bfcp_build_attribute_FLOOR_REQUEST_ID(message, frqID);	/* This attribute is compulsory */
	if(err == -1)	/* We couldn't build this attribute, return with a failure */
		return NULL;
	bfcp_build_commonheader(message, entity, FloorRelease);
	return message;
}

bfcp_message *bfcp_build_message_FloorRequestQuery(bfcp_entity *entity, unsigned short int frqID)
{
	int err;
	bfcp_message *message = bfcp_new_message(NULL, 0);
	if(!message)	/* We could not allocate the memory, return a with failure */
		return NULL;
	if(!frqID)	/* We need a valid FloorRequestID */
		return NULL;
	err = bfcp_build_attribute_FLOOR_REQUEST_ID(message, frqID);	/* This attribute is compulsory */
	if(err == -1)	/* We couldn't build this attribute, return with a failure */
		return NULL;
	bfcp_build_commonheader(message, entity, FloorRequestQuery);
	return message;
}

bfcp_message *bfcp_build_message_FloorRequestStatus(bfcp_entity *entity, bfcp_floor_request_information *frqInfo)
{
	int err;
	bfcp_message *message = bfcp_new_message(NULL, 0);
	if(!message)	/* We could not allocate the memory, return a with failure */
		return NULL;
	if(!frqInfo)	/* We need a valid FloorRequestInformation */
		return NULL;
	err = bfcp_build_attribute_FLOOR_REQUEST_INFORMATION(message, frqInfo);	/* This attribute is compulsory */
	if(err == -1)	/* We couldn't build this attribute, return with a failure */
		return NULL;
	bfcp_build_commonheader(message, entity, FloorRequestStatus);
	return message;
}

bfcp_message *bfcp_build_message_UserQuery(bfcp_entity *entity, unsigned short int bID)
{
	int err;
	bfcp_message *message = bfcp_new_message(NULL, 0);
	if(!message)	/* We could not allocate the memory, return a with failure */
		return NULL;
	if(bID) {
		err = bfcp_build_attribute_BENEFICIARY_ID(message, bID);	/*This attribute is not compulsory */
		if(err == -1)	/* We couldn't build this attribute, return with a failure */
			return NULL;
	}
	bfcp_build_commonheader(message, entity, UserQuery);
	return message;
}

/* 1*[FLOOR-REQUEST-INFORMATION] */
bfcp_message *bfcp_build_message_UserStatus(bfcp_entity *entity, bfcp_user_information *beneficiary, bfcp_floor_request_information *frqInfo)
{
	int err;
	bfcp_message *message = bfcp_new_message(NULL, 0);
	if(!message)	/* We could not allocate the memory, return a with failure */
		return NULL;
	if(beneficiary)		/* This attribute is not compulsory */
		bfcp_build_attribute_BENEFICIARY_INFORMATION(message, beneficiary);
	bfcp_floor_request_information *temp = frqInfo;
	while(temp) {	/* There can be more than one FLOOR-REQUEST-INFORMATION attribute */
		err = bfcp_build_attribute_FLOOR_REQUEST_INFORMATION(message, temp);
		if(err == -1)	/* We couldn't build this attribute, return with a failure */
			return NULL;
		temp = temp->next;
	}
	bfcp_build_commonheader(message, entity, UserStatus);
	return message;
}

/* 1*[FLOOR-ID] */
bfcp_message *bfcp_build_message_FloorQuery(bfcp_entity *entity, bfcp_floor_id_list *fID)
{
	int err;
	bfcp_message *message = bfcp_new_message(NULL, 0);
	if(!message)	/* We could not allocate the memory, return a with failure */
		return NULL;
	bfcp_floor_id_list *temp = fID;
	if(temp) {	/* FLOOR-ID is not compulsory: if missing, the client is not interested */
			/* in receiving information on any floor anymore */
		while(temp) {	/* There can be more than one FLOOR-ID attribute */
			if(!temp->ID)	/* We need a valid FloorID */
				return NULL;
			err = bfcp_build_attribute_FLOOR_ID(message, temp->ID);
			if(err == -1)	/* We couldn't build this attribute, return with a failure */
				return NULL;
			temp = temp->next;
		}
	}
	bfcp_build_commonheader(message, entity, FloorQuery);
	return message;
}

/* *[FLOOR-REQUEST-INFORMATION] */
bfcp_message *bfcp_build_message_FloorStatus(bfcp_entity *entity, bfcp_floor_id_list *fID, bfcp_floor_request_information *frqInfo)
{
	int err;
	bfcp_message *message = bfcp_new_message(NULL, 0);
	if(!message)	/* We could not allocate the memory, return a with failure */
		return NULL;
	bfcp_floor_request_information *temp = frqInfo;
	if(fID) {	/* FLOOR-ID is not compulsory: if missing, the client was not interested */
			/* in receiving information on any floor anymore, so this is just an ack */
		if(!fID->ID)	/* We need a valid FloorID */
			return NULL;
		err = bfcp_build_attribute_FLOOR_ID(message, fID->ID);	/* This attribute is compulsory */
		if(err == -1)	/* We couldn't build this attribute, return with a failure */
			return NULL;
	}
	/* There can be more than one FLOOR-REQUEST-INFORMATION attribute */
	/* but the attribute is not compulsory */
	if(temp) {
		while(temp) {	/* There can be more than one FLOOR-REQUEST-INFORMATION attribute */
			err = bfcp_build_attribute_FLOOR_REQUEST_INFORMATION(message, temp);
			if(err == -1)	/* We couldn't build this attribute, return with a failure */
				return NULL;
			temp = temp->next;
		}
	}
	bfcp_build_commonheader(message, entity, FloorStatus);
	return message;
}

/* 1*[FLOOR-ID] */
bfcp_message *bfcp_build_message_ChairAction(bfcp_entity *entity, bfcp_floor_request_information *frqInfo)
{
	int err;
	bfcp_message *message = bfcp_new_message(NULL, 0);
	if(!message)	/* We could not allocate the memory, return a with failure */
		return NULL;
	if(!frqInfo)	/* We need a valid FloorRequestInformation */
		return NULL;
	err = bfcp_build_attribute_FLOOR_REQUEST_INFORMATION(message, frqInfo);	/* This attribute is compulsory */
	if(err == -1)	/* We couldn't build this attribute, return with a failure */
		return NULL;
	bfcp_build_commonheader(message, entity, ChairAction);
	return message;
}

bfcp_message *bfcp_build_message_ChairActionAck(bfcp_entity *entity)
{
	bfcp_message *message = bfcp_new_message(NULL, 0);	/* This primitive has no attributes */
	if(!message)	/* We could not allocate the memory, return a with failure */
		return NULL;
	bfcp_build_commonheader(message, entity, ChairActionAck);
	return message;
}

bfcp_message *bfcp_build_message_Hello(bfcp_entity *entity)
{
	bfcp_message *message = bfcp_new_message(NULL, 0);	/* This primitive has no attributes */
	if(!message)	/* We could not allocate the memory, return a with failure */
		return NULL;
	bfcp_build_commonheader(message, entity, Hello);
	return message;
}

bfcp_message *bfcp_build_message_HelloAck(bfcp_entity *entity, bfcp_supported_list *primitives, bfcp_supported_list *attributes)
{
	int err;
	bfcp_message *message = bfcp_new_message(NULL, 0);
	if(!message)	/* We could not allocate the memory, return a with failure */
		return NULL;
	err = bfcp_build_attribute_SUPPORTED_PRIMITIVES(message, primitives);
	if(err == -1)	/* We couldn't build this attribute, return with a failure */
		return NULL;
	err = bfcp_build_attribute_SUPPORTED_ATTRIBUTES(message, attributes);
	if(err == -1)	/* We couldn't build this attribute, return with a failure */
		return NULL;
	bfcp_build_commonheader(message, entity, HelloAck);
	return message;
}

bfcp_message *bfcp_build_message_Error(bfcp_entity *entity, bfcp_error *error, char *eInfo)
{
	int err;
	bfcp_message *message = bfcp_new_message(NULL, 0);
	if(!message)	/* We could not allocate the memory, return a with failure */
		return NULL;
	err = bfcp_build_attribute_ERROR_CODE(message, error);	/* This attribute is compulsory */
	if(err == -1)	/* We couldn't build this attribute, return with a failure */
		return NULL;
	if(eInfo) {
		err = bfcp_build_attribute_ERROR_INFO(message, eInfo);	/* This attribute is not compulsory */
		if(err == -1)	/* We couldn't build this attribute, return with a failure */
			return NULL;
	}
	bfcp_build_commonheader(message, entity, Error);
	return message;
}



/* Build Attributes */

int bfcp_build_attribute_BENEFICIARY_ID(bfcp_message *message, unsigned short int bID)
{
	int position = message->position;	/* We keep track of where the TLV will have to be */
	unsigned char *buffer = message->buffer+(message->position)+2;	/* We skip the TLV bytes */
	unsigned short int raw = htons(bID);			/* We want all protocol values in network-byte-order */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "BENEFICIARY_ID [%d]\n", bID);
	memcpy(buffer, &raw, 2);	/* We copy the ID to the buffer */
	bfcp_build_attribute_tlv(message, position, BENEFICIARY_ID, 1, 4);	/* Lenght is fixed (4 octets) */
	message->length = message->length+4;
	message->position = message->position+4;
	return 4;
}

int bfcp_build_attribute_FLOOR_ID(bfcp_message *message, unsigned short int fID)
{
	int position = message->position;	/* We keep track of where the TLV will have to be */
	unsigned char *buffer = message->buffer+(message->position)+2;	/* We skip the TLV bytes */
	unsigned short int raw = htons(fID);	/* We want all protocol values in network-byte-order */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "FLOOR_ID [%d]\n", fID);
	memcpy(buffer, &raw, 2);		/* We copy the ID to the buffer */
	bfcp_build_attribute_tlv(message, position, FLOOR_ID, 1, 4);	/* Lenght is fixed (4 octets) */
	message->length = message->length+4;
	message->position = message->position+4;
	return 4;
}

int bfcp_build_attribute_FLOOR_REQUEST_ID(bfcp_message *message, unsigned short int frqID)
{
	int position = message->position;	/* We keep track of where the TLV will have to be */
	unsigned char *buffer = message->buffer+(message->position)+2;	/* We skip the TLV bytes */
	unsigned short int raw = htons(frqID);	/* We want all protocol values in network-byte-order */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"FLOOR_REQUEST_ID [%d]\n", frqID);
	memcpy(buffer, &raw, 2);		/* We copy the ID to the buffer */
	bfcp_build_attribute_tlv(message, position, FLOOR_REQUEST_ID, 1, 4);	/* Lenght is fixed (4 octets) */
	message->length = message->length+4;
	message->position = message->position+4;
	return 4;
}

int bfcp_build_attribute_PRIORITY(bfcp_message *message, unsigned short int priority)
{
	int position = message->position;	/* We keep track of where the TLV will have to be */
	unsigned char *buffer = message->buffer+(message->position)+2;	/* We skip the TLV bytes */
	unsigned short int raw;	/* The 2-octets completing the Attribute */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "PRIORITY [%d/%s]\n", priority, get_bfcp_priority(priority));
	raw = (((raw & !(0xE000)) | (priority)) << 13) +	/* First the Priority (3 bits) */
		((raw & !(0x1FFF)) | 0);			/* and then the 13 Reserved bits */
	raw = htons(raw);		/* We want all protocol values in network-byte-order */
	memcpy(buffer, &raw, 2);	/* We copy the Priority to the buffer */
	bfcp_build_attribute_tlv(message, position, PRIORITY, 1, 4);	/* Lenght is fixed (4 octets) */
	message->length = message->length+4;
	message->position = message->position+4;
	return 4;
}

int bfcp_build_attribute_REQUEST_STATUS(bfcp_message *message, bfcp_request_status *rs)
{
	int position = message->position;	/* We keep track of where the TLV will have to be */
	unsigned char *buffer = message->buffer+(message->position)+2;	/* We skip the TLV bytes */
	unsigned short int raw;	/* The 2-octets completing the Attribute */
	switch_log_printf(SWITCH_CHANNEL_LOG,
                      SWITCH_LOG_INFO,
                      "REQUEST_STATUS Status[%d/%s] Queue Position[%d]\n",
                      rs->rs,
                      get_bfcp_status(rs->rs),
                      rs->qp );
	raw = (((raw & !(0xFF00)) | (rs->rs)) << 8) +	/* First the Request Status (8 bits) */
		((raw & !(0x00FF)) | (rs->qp));		/* and then the Queue Position (8 bits) */
	raw = htons(raw);		/* We want all protocol values in network-byte-order */
	memcpy(buffer, &raw, 2);	/* We copy the RS and QP to the buffer */
	bfcp_build_attribute_tlv(message, position, REQUEST_STATUS, 1, 4);	/* Lenght is fixed (4 octets) */
	message->length = message->length+4;
	message->position = message->position+4;
	return 4;
}

int bfcp_build_attribute_ERROR_CODE(bfcp_message *message, bfcp_error *error)
{
	if(!error)	/* There's no Error Code, return wih a failure */
		return -1;
	bfcp_unknown_m_error_details *temp;
	char ch;	/* 8 bits */
	int attrlen = 2;	/* The Lenght of the attribute (starting from the TLV) */
	int padding = 0;	/* Number of bytes of padding */
	int position = message->position;	/* We keep track of where the TLV will have to be */
	unsigned char *buffer = message->buffer+(message->position)+2;	/* We skip the TLV bytes */
	char raw = ((raw & !(0xFF)) | (error->code));	/* The Error Code is 8 bits */

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "ERROR_CODE [%d/%s]\n", error->code, get_bfcp_error_type(error->code));

	memcpy(buffer, &raw, 1);	/* We copy the Error Code to the buffer */
	buffer = buffer+1;
	attrlen = 3;
	switch(error->code) {
		case BFCP_UNKNOWN_MANDATORY_ATTRIBUTE:	/* For now, the only error that has more details is error 4 */
			if(!(error->details))
				return -1;	/* There has to be AT LEAST one error detail, for error 4*/
			temp = error->details;
			while(temp) {	/* Let's add a byte for each detail we find */
				ch = (((ch & !(0xFE)) | (temp->unknown_type)) << 1) +	/* Attribute: 7 bits */
					((ch & !(0x01)) | (temp->reserved));		/* Reserved: 1 bit */
				attrlen++;	/* We remember how many details we've written */
				temp = temp->next;
			}
			if(((attrlen+2)%4) != 0) {	/* We need padding */
				padding = 4-((attrlen+2)%4);
				memset(buffer+attrlen, 0, padding);
			}
			break;
		default:	/* All the others have none, so we add a byte of padding to the message */
			padding++;	/* We just need one byte: TLV+ErrorCode = 3 bytes */
			memset(buffer, 0, padding);
			break;
	}
	bfcp_build_attribute_tlv(message, position, ERROR_CODE, 1, attrlen);
	message->length = message->length+attrlen+padding;
	message->position = message->position+attrlen+padding;
	return attrlen;
}

int bfcp_build_attribute_ERROR_INFO(bfcp_message *message, char *eInfo)
{
	if(!eInfo)	/* The string is empty, return with a failure */
		return -1;
	int attrlen = 2;	/* The Lenght of the attribute (starting from the TLV) */
	int padding = 0;	/* Number of bytes of padding */
	int position = message->position;	/* We keep track of where the TLV will have to be */
	unsigned char *buffer = message->buffer+(message->position)+2;	/* We skip the TLV bytes */
	int eLen = strlen(eInfo);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "ERROR_INFO [%s]\n", eInfo? eInfo:"NULL" );

	memcpy(buffer, eInfo, eLen);
	buffer = buffer+eLen;
	if(((eLen+2)%4) != 0) {		/* We need padding */
		padding = 4-((eLen+2)%4);
		memset(buffer+eLen, 0, padding);
	}
	attrlen = attrlen+eLen;
	bfcp_build_attribute_tlv(message, position, ERROR_INFO, 1, attrlen);
	message->length = message->length+attrlen+padding;
	message->position = message->position+attrlen+padding;
	return attrlen;
}

int bfcp_build_attribute_PARTICIPANT_PROVIDED_INFO(bfcp_message *message, char *pInfo)
{
	if(!pInfo)	/* The string is empty, return with a failure */
		return -1;
	int attrlen = 2;	/* The Lenght of the attribute (starting from the TLV) */
	int padding = 0;	/* Number of bytes of padding */
	int position = message->position;		/* We keep track of where the TLV will have to be */
	unsigned char *buffer = message->buffer+(message->position)+2;	/* We skip the TLV bytes */
	int pLen = strlen(pInfo);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "PARTICIPANT_PROVIDED_INFO [%s]\n", pInfo? pInfo:"NULL" );

	memcpy(buffer, pInfo, pLen);
	buffer = buffer+pLen;
	if(((pLen+2)%4) != 0) {		/* We need padding */
		padding = 4-((pLen+2)%4);
		memset(buffer+pLen, 0, padding);
	}
	attrlen = attrlen+pLen;
	bfcp_build_attribute_tlv(message, position, PARTICIPANT_PROVIDED_INFO, 1, attrlen);
	message->length = message->length+attrlen+padding;
	message->position = message->position+attrlen+padding;
	return attrlen;
}

int bfcp_build_attribute_STATUS_INFO(bfcp_message *message, char *sInfo)
{
	if(!sInfo)	/* The string is empty, return with a failure */
		return -1;
	int attrlen = 2;	/* The Lenght of the attribute (starting from the TLV) */
	int padding = 0;	/* Number of bytes of padding */
	int position = message->position;		/* We keep track of where the TLV will have to be */
	unsigned char *buffer = message->buffer+(message->position)+2;	/* We skip the TLV bytes */
	int sLen = strlen(sInfo);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "STATUS_INFO [%s]\n", sInfo? sInfo:"NULL");

	memcpy(buffer, sInfo, sLen);
	buffer = buffer+sLen;
	if(((sLen+2)%4) != 0) {		/* We need padding */
		padding = 4-((sLen+2)%4);
		memset(buffer+sLen, 0, padding);
	}
	attrlen = attrlen+sLen;
	bfcp_build_attribute_tlv(message, position, STATUS_INFO, 1, attrlen);
	message->length = message->length+attrlen+padding;
	message->position = message->position+attrlen+padding;
	return attrlen;
}

int bfcp_build_attribute_SUPPORTED_ATTRIBUTES(bfcp_message *message, bfcp_supported_list *attributes)
{
	if(!attributes)	/* The supported attributes list is empty, return with a failure */
		return -1;
	int attrlen = 2;	/* The Lenght of the attribute (starting from the TLV) */
	int padding = 0;	/* Number of bytes of padding */
	int position = message->position;		/* We keep track of where the TLV will have to be */
	unsigned char *buffer = message->buffer+(message->position)+2;	/* We skip the TLV bytes */
	unsigned short int ch16;	/* 16 bits */
	bfcp_supported_list *temp = attributes;
	while(temp && temp->element) {	/* Fill all supported attributes */
		ch16 = temp->element;
		switch_log_printf(SWITCH_CHANNEL_LOG,
                          SWITCH_LOG_INFO,
                          "SUPPORTED_ATTRIBUTES [%d/%s]\n",
                          temp->element,
                          get_bfcp_attribute(temp->element));

		ch16 = ch16 << 1 ;
		ch16 = ch16 & 0xFE ; //( BIT R )
		memcpy(buffer, &ch16, 1);
		buffer = buffer+1;
		attrlen = attrlen+1;
		temp = temp->next;
	}
	if((attrlen%4) != 0) {		/* We need padding */
		padding = 4-(attrlen%4);
		memset(buffer, 0, padding);
	}
	bfcp_build_attribute_tlv(message, position, SUPPORTED_ATTRIBUTES, 1, attrlen);
	message->length = message->length+attrlen+padding;
	message->position = message->position+attrlen+padding;
	return attrlen;
}

int bfcp_build_attribute_SUPPORTED_PRIMITIVES(bfcp_message *message, bfcp_supported_list *primitives)
{
	if(!primitives)	/* The supported attributes list is empty, return with a failure */
		return -1;
	int attrlen = 2;	/* The Lenght of the attribute (starting from the TLV) */
	int padding = 0;	/* Number of bytes of padding */
	int position = message->position;		/* We keep track of where the TLV will have to be */
	unsigned char *buffer = message->buffer+(message->position)+2;	/* We skip the TLV bytes */
	unsigned short int ch16;	/* 16 bits */
	bfcp_supported_list *temp = primitives;
	while(temp) {	/* Fill all supported primitives */
		ch16 = temp->element;
		switch_log_printf(SWITCH_CHANNEL_LOG,
                          SWITCH_LOG_INFO,
                          "SUPPORTED_PRIMITIVES [%d/%s]\n",
                          temp->element,
                          get_bfcp_primitive(temp->element));

		memcpy(buffer, &ch16, 1);
		buffer = buffer+1;
		attrlen = attrlen+1;
		temp = temp->next;
	}
	if((attrlen%4) != 0) {		/* We need padding */
		padding = 4-(attrlen%4);
		memset(buffer, 0, padding);
	}
	bfcp_build_attribute_tlv(message, position, SUPPORTED_PRIMITIVES, 1, attrlen);
	message->length = message->length+attrlen+padding;
	message->position = message->position+attrlen+padding;
	return attrlen;
}

int bfcp_build_attribute_USER_DISPLAY_NAME(bfcp_message *message, char *display)
{
	if(!display)	/* The string is empty, return with a failure */
		return -1;
	int attrlen = 2;	/* The Lenght of the attribute (starting from the TLV) */
	int padding = 0;	/* Number of bytes of padding */
	int position = message->position;		/* We keep track of where the TLV will have to be */
	unsigned char *buffer = message->buffer+(message->position)+2;	/* We skip the TLV bytes */
	int dLen = strlen(display);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "USER_DISPLAY_NAME [%s]\n", display? display:"NULL");

	memcpy(buffer, display, dLen);
	buffer = buffer+dLen;
	if(((dLen+2)%4) != 0) {		/* We need padding */
		padding = 4-((dLen+2)%4);
		memset(buffer+dLen, 0, padding);
	}
	attrlen = attrlen+dLen;
	bfcp_build_attribute_tlv(message, position, USER_DISPLAY_NAME, 1, attrlen);
	message->length = message->length+attrlen+padding;
	message->position = message->position+attrlen+padding;
	return attrlen;
}

int bfcp_build_attribute_USER_URI(bfcp_message *message, char *uri)
{
	if(!uri)	/* The string is empty, return with a failure */
		return -1;
	int attrlen = 2;	/* The Lenght of the attribute (starting from the TLV) */
	int padding = 0;	/* Number of bytes of padding */
	int position = message->position;		/* We keep track of where the TLV will have to be */
	unsigned char *buffer = message->buffer+(message->position)+2;	/* We skip the TLV bytes */
	int uLen = strlen(uri);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "USER_URI [%s]\n", uri? uri:"NULL");

	memcpy(buffer, uri, uLen);
	buffer = buffer+uLen;
	if(((uLen+2)%4) != 0) {		/* We need padding */
		padding = 4-((uLen+2)%4);
		memset(buffer+uLen, 0, padding);
	}
	attrlen = attrlen+uLen;
	bfcp_build_attribute_tlv(message, position, USER_URI, 1, attrlen);
	message->length = message->length+attrlen+padding;
	message->position = message->position+attrlen+padding;
	return attrlen;
}

int bfcp_build_attribute_BENEFICIARY_INFORMATION(bfcp_message *message, bfcp_user_information *beneficiary)
{
	if(!beneficiary)	/* There's no Beneficiary User Information, return wih a failure */
		return -1;
	int attrlen = 2;	/* The Lenght of the attribute (starting from the TLV) */
	int position = message->position;	/* We keep track of where the TLV will have to be */
	int length = message->length;		/* We keep track of the length before the attributes */
	unsigned char *buffer = message->buffer+(message->position)+2;	/* We skip the TLV bytes */
	unsigned short int raw = htons(beneficiary->ID);	/* The 2-octets completing the Attribute Header */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "BENEFICIARY_INFORMATION Beneficiary ID[%d]\n", beneficiary->ID);

	memcpy(buffer, &raw, 2);		/* We copy the ID to the buffer */
	attrlen = attrlen+2;
	message->length = message->length+4;
	message->position = message->position+4;
	/* Here starts the ABNF (it's a grouped attribute) */
	int err;
	if(beneficiary->display) {	/* This attribute is not compulsory */
		err = bfcp_build_attribute_USER_DISPLAY_NAME(message, beneficiary->display);
		if(err == -1)	/* We couldn't build this attribute, return with an error */
			return -1;
		attrlen = attrlen+err;
		if((err%4) != 0)	/* We need padding */
			attrlen = attrlen+4-(err%4);
	}
	if(beneficiary->uri) {		/* This attribute is not compulsory */
		err = bfcp_build_attribute_USER_URI(message, beneficiary->uri);
		if(err == -1)	/* We couldn't build this attribute, return with an error */
			return -1;
		attrlen = attrlen+err;
		if((err%4) != 0)	/* We need padding */
			attrlen = attrlen+4-(err%4);
	}
	bfcp_build_attribute_tlv(message, position, BENEFICIARY_INFORMATION, 1, attrlen);
	message->length = length+attrlen;
	message->position = position+attrlen;
	return attrlen;
}

/* 1*[FLOOR-REQUEST-STATUS] */
int bfcp_build_attribute_FLOOR_REQUEST_INFORMATION(bfcp_message *message, bfcp_floor_request_information *frqInfo)
{
	if(!frqInfo)	/* There's no Floor Request Information, return wih a failure */
		return -1;
	bfcp_floor_request_status *temp;
	int attrlen = 2;	/* The Lenght of the attribute (starting from the TLV) */
	int position = message->position;	/* We keep track of where the TLV will have to be */
	int length = message->length;		/* We keep track of the length before the attributes */
	unsigned char *buffer = message->buffer+(message->position)+2;	/* We skip the TLV bytes */
	unsigned short int raw = htons(frqInfo->frqID);	/* The 2-octets completing the Attribute Header */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "FLOOR_REQUEST_INFORMATION Floor request ID [%d]\n", frqInfo->frqID);
	memcpy(buffer, &raw, 2);		/* We copy the ID to the buffer */
	attrlen = attrlen+2;
	message->length = message->length+4;
	message->position = message->position+4;
	/* Here starts the ABNF (it's a grouped attribute) */
	int err;
	if(frqInfo->oRS) {	/* This attribute is not compulsory */
		err = bfcp_build_attribute_OVERALL_REQUEST_STATUS(message, frqInfo->oRS);
		if(err == -1)	/* We couldn't build this attribute, return with an error */
			return -1;
		attrlen = attrlen+err;
		if((err%4) != 0)	/* We need padding */
			attrlen = attrlen+4-(err%4);
	}
	if(!(frqInfo->fRS))
		return -1;	/* FLOOR-REQUEST-STATUS is 1*, there has to be AT LEAST one, return with a failure */
	temp = frqInfo->fRS;
	while(temp) {	/* There can be more than one FLOOR-REQUEST-STATUS attribute */
		err = bfcp_build_attribute_FLOOR_REQUEST_STATUS(message, temp);
		if(err == -1)	/* We couldn't build this attribute, return with an error */
			return -1;
		attrlen = attrlen+err;
		if((err%4) != 0)	/* We need padding */
			attrlen = attrlen+4-(err%4);
		temp = temp->next;
	}
	if(frqInfo->beneficiary) {	/* This attribute is not compulsory */
		err = bfcp_build_attribute_BENEFICIARY_INFORMATION(message, frqInfo->beneficiary);
		if(err == -1)	/* We couldn't build this attribute, return with an error */
			return -1;
		attrlen = attrlen+err;
		if((err%4) != 0)	/* We need padding */
			attrlen = attrlen+4-(err%4);
	}
	if(frqInfo->requested_by) {	/* This attribute is not compulsory */
		err = bfcp_build_attribute_REQUESTED_BY_INFORMATION(message, frqInfo->requested_by);
		if(err == -1)	/* We couldn't build this attribute, return with an error */
			return -1;
		attrlen = attrlen+err;
		if((err%4) != 0)	/* We need padding */
			attrlen = attrlen+4-(err%4);
	}
	if(frqInfo->priority<5) {	/* This attribute is not compulsory (must be between 0 and 4) */
		err = bfcp_build_attribute_PRIORITY(message, frqInfo->priority);
		if(err == -1)	/* We couldn't build this attribute, return with an error */
			return -1;
		attrlen = attrlen+err;
		if((err%4) != 0)	/* We need padding */
			attrlen = attrlen+4-(err%4);
	}
	if(frqInfo->pInfo) {		/* This attribute is not compulsory */
		err = bfcp_build_attribute_PARTICIPANT_PROVIDED_INFO(message, frqInfo->pInfo);
		if(err == -1)	/* We couldn't build this attribute, return with an error */
			return -1;
		attrlen = attrlen+err;
		if((err%4) != 0)	/* We need padding */
			attrlen = attrlen+4-(err%4);
	}
	bfcp_build_attribute_tlv(message, position, FLOOR_REQUEST_INFORMATION, 1, attrlen);
	message->length = length+attrlen;
	message->position = position+attrlen;
	return attrlen;
}

int bfcp_build_attribute_REQUESTED_BY_INFORMATION(bfcp_message *message, bfcp_user_information *requested_by)
{
	if(!requested_by)	/* There's no RequestedBy User Information, return wih a failure */
		return -1;
	int attrlen = 2;	/* The Lenght of the attribute (starting from the TLV) */
	int position = message->position;	/* We keep track of where the TLV will have to be */
	int length = message->length;		/* We keep track of the length before the attributes */
	unsigned char *buffer = message->buffer+(message->position)+2;	/* We skip the TLV bytes */
	attrlen = attrlen+2;
	unsigned short int raw = htons(requested_by->ID);	/* The 2-octets completing the Attribute Header */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "REQUESTED_BY_INFORMATION Requested by ID[%d]\n", requested_by->ID);

	memcpy(buffer, &raw, 2);		/* We copy the ID to the buffer */
	message->length = message->length+4;
	message->position = message->position+4;
	/* Here starts the ABNF (it's a grouped attribute) */
	int err;
	if(requested_by->display) {	/* This attribute is not compulsory */
		err = bfcp_build_attribute_USER_DISPLAY_NAME(message, requested_by->display);
		if(err == -1)	/* We couldn't build this attribute, return with an error */
			return -1;
		attrlen = attrlen+err;
		if((err%4) != 0)	/* We need padding */
			attrlen = attrlen+4-(err%4);
	}
	if(requested_by->uri) {		/* This attribute is not compulsory */
		err = bfcp_build_attribute_USER_URI(message, requested_by->uri);
		if(err == -1)	/* We couldn't build this attribute, return with an error */
			return -1;
		attrlen = attrlen+err;
		if((err%4) != 0)	/* We need padding */
			attrlen = attrlen+4-(err%4);
	}
	bfcp_build_attribute_tlv(message, position, REQUESTED_BY_INFORMATION, 1, attrlen);
	message->length = length+attrlen;
	message->position = position+attrlen;
	return attrlen;
}

int bfcp_build_attribute_FLOOR_REQUEST_STATUS(bfcp_message *message, bfcp_floor_request_status *fRS)
{
	if(!fRS)	/* There's no Floor Request Status, return wih a failure */
		return -1;
	int attrlen = 2;	/* The Lenght of the attribute (starting from the TLV) */
	int position = message->position;	/* We keep track of where the TLV will have to be */
	int length = message->length;		/* We keep track of the length before the attributes */
	unsigned char *buffer = message->buffer+(message->position)+2;	/* We skip the TLV bytes */
	unsigned short int raw = htons(fRS->fID);	/* The 2-octets completing the Attribute Header */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "FLOOR_REQUEST_STATUS floorId [%d]\n", fRS->fID);

	memcpy(buffer, &raw, 2);		/* We copy the ID to the buffer */
	attrlen = attrlen+2;
	message->length = message->length+4;
	message->position = message->position+4;
	/* Here starts the ABNF (it's a grouped attribute) */
	int err;
	if(fRS->rs) {	/* This attribute is not compulsory */
		err = bfcp_build_attribute_REQUEST_STATUS(message, fRS->rs);
		if(err == -1)	/* We couldn't build this attribute, return with an error */
			return -1;
		attrlen = attrlen+err;
	}
	if(fRS->sInfo) {	/* This attribute is not compulsory */
		err = bfcp_build_attribute_STATUS_INFO(message, fRS->sInfo);
		if(err == -1)	/* We couldn't build this attribute, return with an error */
			return -1;
		attrlen = attrlen+err;
		if((err%4) != 0)	/* We need padding */
			attrlen = attrlen+4-(err%4);
	}
	bfcp_build_attribute_tlv(message, position, FLOOR_REQUEST_STATUS, 1, attrlen);
	message->length = length+attrlen;
	message->position = position+attrlen;
	return attrlen;
}

int bfcp_build_attribute_OVERALL_REQUEST_STATUS(bfcp_message *message, bfcp_overall_request_status *oRS)
{
	if(!oRS)	/* There's no Overall Request Status, return wih a failure */
		return -1;
	int attrlen = 2;	/* The Lenght of the attribute (starting from the TLV) */
	int position = message->position;	/* We keep track of where the TLV will have to be */
	int length = message->length;		/* We keep track of the length before the attributes */
	unsigned char *buffer = message->buffer+(message->position)+2;	/* We skip the TLV bytes */
	unsigned short int raw = htons(oRS->frqID);	/* The 2-octets completing the Attribute Header */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "OVERALL_REQUEST_STATUS FloorId[%d]\n", oRS->frqID);

	memcpy(buffer, &raw, 2);		/* We copy the ID to the buffer */
	attrlen = attrlen+2;
	message->length = message->length+4;
	message->position = message->position+4;
	/* Here starts the ABNF (it's a grouped attribute) */
	int err;
	if(oRS->rs) {	/* This attribute is not compulsory */
		err = bfcp_build_attribute_REQUEST_STATUS(message, oRS->rs);
		if(err == -1)	/* We couldn't build this attribute, return with an error */
			return -1;
		attrlen = attrlen+err;
	}
	if(oRS->sInfo) {	/* This attribute is not compulsory */
		err = bfcp_build_attribute_STATUS_INFO(message, oRS->sInfo);
		if(err == -1)	/* We couldn't build this attribute, return with an error */
			return -1;
		attrlen = attrlen+err;
		if((err%4) != 0)	/* We need padding */
			attrlen = attrlen+4-(err%4);
	}
	bfcp_build_attribute_tlv(message, position, OVERALL_REQUEST_STATUS, 1, attrlen);
	message->length = length+attrlen;
	message->position = position+attrlen;
	return attrlen;
}

int bfcp_build_attribute_NONCE(bfcp_message *message, unsigned short int nonce)
{
	int position = message->position;	/* We keep track of where the TLV will have to be */
	unsigned char *buffer = message->buffer+(message->position)+2;	/* We skip the TLV bytes */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "NONCE [%d]\n", nonce);
	nonce = htons(nonce);		/* We want all protocol values in network-byte-order */
	memcpy(buffer, &nonce, 2);	/* We copy the ID to the buffer */
	bfcp_build_attribute_tlv(message, position, NONCE, 1, 4);	/* Lenght is fixed (4 octets) */
	message->length = message->length+4;
	message->position = message->position+4;
	return 4;
}

int bfcp_build_attribute_DIGEST(bfcp_message *message, bfcp_digest *digest)
{
	if(!digest)	/* There's no Digest, return with a failure */
		return -1;
	int attrlen = 2;	/* The Lenght of the attribute (starting from the TLV) */
	int padding = 0;	/* Number of bytes of padding */
	int position = message->position;	/* We keep track of where the TLV will have to be */
	unsigned char *buffer = message->buffer+(message->position)+2;	/* We skip the TLV bytes */
	char raw = (raw & !(0xFF)) | (digest->algorithm);	/* The Algorithm is 8 bits */
	memcpy(buffer, &raw, 1);		/* We copy the Algorithm to the buffer */
	buffer = buffer+1;
	int dLen = strlen(digest->text);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "DIGEST text [%s]\n", digest->text? digest->text:"NULL");
	
	memcpy(buffer, digest->text, dLen);
	buffer = buffer+dLen;
	if(((dLen+3)%4) != 0) {		/* We need padding */
		padding = 4-((dLen+3)%4);
		memset(buffer+dLen, 0, padding);
	}
	attrlen = attrlen+dLen+1;
	bfcp_build_attribute_tlv(message, position, DIGEST, 1, attrlen);
	message->length = message->length+attrlen+padding;
	message->position = message->position+attrlen+padding;
	return attrlen;
}
