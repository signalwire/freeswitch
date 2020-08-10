#ifndef _BFCP_MESSAGES_H
#define _BFCP_MESSAGES_H

#ifndef WIN32
#include <arpa/inet.h>	/* For htonl and htons */
#else
#include <winsock2.h>	/* For htonl and htons (Win32) */
#endif
#include <stdarg.h>	/* For functions with variable arguments */
#include <stdlib.h>	/* For calloc */
#include <string.h>	/* For memcpy */

#include <stdio.h>

#include "switch.h"


/* Primitives */
#define FloorRequest		1
#define FloorRelease		2
#define FloorRequestQuery	3
#define FloorRequestStatus	4
#define UserQuery		5
#define UserStatus		6
#define FloorQuery		7
#define FloorStatus		8
#define ChairAction		9
#define ChairActionAck		10
#define Hello			11
#define HelloAck		12
#define Error			13
#define FloorRequestStatusAck	14
#define ErrorAck		15
#define FloorStatusAck		16

/* Attributes */
#define BENEFICIARY_ID			1
#define FLOOR_ID			2
#define FLOOR_REQUEST_ID		3
#define PRIORITY			4
#define REQUEST_STATUS			5
#define ERROR_CODE			6
#define ERROR_INFO			7
#define PARTICIPANT_PROVIDED_INFO	8
#define STATUS_INFO			9
#define SUPPORTED_ATTRIBUTES		10
#define SUPPORTED_PRIMITIVES		11
#define USER_DISPLAY_NAME		12
#define USER_URI			13
#define BENEFICIARY_INFORMATION		14
#define FLOOR_REQUEST_INFORMATION	15
#define REQUESTED_BY_INFORMATION	16
#define FLOOR_REQUEST_STATUS		17
#define OVERALL_REQUEST_STATUS		18
#define NONCE				19
#define DIGEST				20

/* Request Status Values */
#define BFCP_PENDING	1
#define BFCP_ACCEPTED	2
#define BFCP_GRANTED	3
#define BFCP_DENIED	4
#define BFCP_CANCELLED	5
#define BFCP_RELEASED	6
#define BFCP_REVOKED	7

/* Priority Values */
#define BFCP_LOWEST_PRIORITY	0
#define BFCP_LOW_PRIORITY	1
#define BFCP_NORMAL_PRIORITY	2
#define BFCP_HIGH_PRIORITY	3
#define BFCP_HIGHEST_PRIORITY	4


/* Error Codes */
#define BFCP_CONFERENCE_DOES_NOT_EXIST		1
#define BFCP_USER_DOES_NOT_EXIST		2
#define BFCP_UNKNOWN_PRIMITIVE			3
#define BFCP_UNKNOWN_MANDATORY_ATTRIBUTE	4
#define BFCP_UNAUTHORIZED_OPERATION		5
#define BFCP_INVALID_FLOORID			6
#define BFCP_FLOORREQUEST_DOES_NOT_EXIST	7
#define BFCP_MAX_FLOORREQUESTS_REACHED		8
#define BFCP_USE_TLS				9
#define BFCP_DIGEST_ATTRIBUTE_REQUIRED		10
#define BFCP_INVALID_NONCE			11
#define BFCP_AUTHENTICATION_FAILED		12

/* Parsing-specific Error Codes */
#define BFCP_WRONG_VERSION	1
#define BFCP_RESERVED_NOT_ZERO	2
#define BFCP_UNKNOWN_PRIMITIVE	3
#define BFCP_UNKNOWN_ATTRIBUTE	4
#define BFCP_WRONG_LENGTH	5
#define BFCP_PARSING_ERROR	6


/* Maximum allow size for BFCP messages is 64Kbytes, since the Payload Length in the header is 16 bit */
#define BFCP_MAX_ALLOWED_SIZE	65535


/* BFCP Message */
typedef struct bfcp_message {
	unsigned char *buffer;		/* The buffer containing the message */
	unsigned short int position;	/* The position indicator for the buffer */
	unsigned short int length;	/* The length of the message */
} bfcp_message;

/* Helping Structures for bit masks and so on */
typedef struct bfcp_entity {
	unsigned long int conferenceID;
	unsigned short int transactionID;
	unsigned short int userID;
} bfcp_entity;

typedef struct bfcp_floor_id_list {	/* FLOOR-ID list, to manage the multiple FLOOR-ID attributes */
	unsigned short int ID;			/* FLOOR-ID */
	struct bfcp_floor_id_list *next;	/* Pointer to next FLOOR-ID instance */
} bfcp_floor_id_list;

typedef struct bfcp_supported_list {	/* list to manage all the supported attributes and primitives */
	unsigned short int element;		/* Element (Attribute/Primitive) */
	struct bfcp_supported_list *next;	/* Pointer to next supported element instance */
} bfcp_supported_list;

typedef struct bfcp_request_status {
	unsigned short int rs;	/* Request Status */
	unsigned short int qp;	/* Queue Position */
} bfcp_request_status;

typedef struct bfcp_error {
	unsigned short int code;	/* Error Code */
	void *details;			/* Error Details */
} bfcp_error;

typedef struct bfcp_unknown_m_error_details {	/* These are specific details for error 4: UNKNOWN_M */
	unsigned short int unknown_type;	/* 7-bits to specify the unknown received mandatory attribute */
	unsigned short int reserved;		/* A reserved bit, which must be set to 0 and ignored, at the moment */
	struct bfcp_unknown_m_error_details *next;	/* This is a linked list */
} bfcp_unknown_m_error_details;

typedef struct bfcp_user_information {	/* Help structure for BENEFICIARY-INFORMATION and REQUESTED-BY-INFORMATION */
	unsigned short int ID;		/* For the INFORMATION-HEADER */
	char *display;			/* USER-DISPLAY-NAME, optional */
	char *uri;			/* USER-URI, optional */
} bfcp_user_information;

typedef struct bfcp_floor_request_status {	/* Help structure for FLOOR-REQUEST-STATUS */
	unsigned short int fID;			/* FLOOR-REQUEST-STATUS-HEADER */
	bfcp_request_status *rs;		/* REQUEST-STATUS, optional */
	char *sInfo;				/* STATUS-INFO, optional */
	struct bfcp_floor_request_status *next;	/* pointer to next instance (to manage lists) */
} bfcp_floor_request_status;

typedef struct bfcp_overall_request_status {	/* Help structure for OVERALL-REQUEST-STATUS */
	unsigned short int frqID;		/* OVERALL-REQUEST-STATUS-HEADER */
	bfcp_request_status *rs;		/* REQUEST-STATUS, optional */
	char *sInfo;				/* STATUS-INFO, optional */
} bfcp_overall_request_status;

typedef struct bfcp_floor_request_information {		/* Help structure for FLOOR-REQUEST-INFORMATION */
	unsigned short int frqID;			/* FLOOR-REQUEST-INFORMATION-HEADER */
	bfcp_overall_request_status *oRS;		/* OVERALL-REQUEST-STATUS, optional */
	bfcp_floor_request_status *fRS;			/* FLOOR-REQUEST-STATUS list */
	bfcp_user_information *beneficiary;		/* BENEFICIARY-INFORMATION, optional */
	bfcp_user_information *requested_by;		/* REQUESTED-BY-INFORMATION, optional */
	unsigned short int priority;			/* PRIORITY, optional */
	char *pInfo;					/* PARTICIPANT-PROVIDED-INFO, optional */
	struct bfcp_floor_request_information *next;	/* pointer to next instance (to manage lists) */
} bfcp_floor_request_information;

typedef struct bfcp_digest {
	unsigned short int algorithm;	/* (currently UNUSED) */
	char *text;			/* (currently UNUSED) */
} bfcp_digest;

typedef struct bfcp_arguments {
	unsigned short int primitive;			/* Message Primitive */
	bfcp_entity *entity;				/* Conference ID, Transaction ID, User ID */
	bfcp_floor_id_list *fID;			/* Floor ID list */
	unsigned short int frqID;			/* Floor Request ID */
	unsigned short int bID;				/* Beneficiary ID */
	unsigned short int priority;			/* Priority */
	bfcp_floor_request_information *frqInfo;	/* Floor Request Information */
	bfcp_user_information *beneficiary;		/* Beneficiary Information */
	bfcp_request_status *rs;			/* Request Status */
	char *pInfo;					/* Participant Provided Info */
	char *sInfo;					/* Status Info */
	bfcp_error *error;				/* Error Code & Details */
	char *eInfo;					/* Error Info */
	bfcp_supported_list *primitives;		/* Supported Primitives list */
	bfcp_supported_list *attributes;		/* Supported Primitives list */
	unsigned short int nonce;			/* Nonce (currently UNUSED) */
	bfcp_digest *digest;				/* Digest Algorithm & Text */
} bfcp_arguments;

/* Parsing Help Structures */
typedef struct bfcp_received_attribute {	/* A chained list to manage all attributes in a received message */
	int type;				/* The attribute type */
	int mandatory_bit;			/* The Mandatory Bit */
	int length;				/* The length of the attribute */
	int position;				/* Its position in the message buffer */
	int valid;				/* If errors occur in parsing, the attribute is marked as not valid */
	struct bfcp_received_attribute *next;	/* Pointer to next attribute in the message */
} bfcp_received_attribute;

typedef struct bfcp_received_message {
	bfcp_arguments *arguments;			/* The message unpacked in its original arguments */
	int version;					/* The version of the received message */
	int reserved;					/* The reserved bits */
	int primitive;					/* The primitive of the message */
	int length;					/* The length of the message */
	bfcp_entity *entity;				/* The entities of the message (IDs) */
	bfcp_received_attribute *first_attribute;	/* A list of all attributes in the message */
	struct bfcp_received_message_error *errors;	/* If errors occur, we write them here */
} bfcp_received_message;

typedef struct bfcp_received_message_error {
	int attribute;					/* The attribute where the error happened */
	int code;					/* The Parsing-specific Error Code */
	struct bfcp_received_message_error *next;	/* There could be more errors, it's a linked list */
} bfcp_received_message_error;


#if defined __cplusplus
	extern "C" {
#endif
/* Creating and Freeing Methods for the Structures */
/* Create a New Arguments Structure */
bfcp_arguments *bfcp_new_arguments(void);
/* Free an Arguments Structure */
int bfcp_free_arguments(bfcp_arguments *arguments);

/* Create a New Message (if buffer is NULL, creates an empty message) */
bfcp_message *bfcp_new_message(unsigned char *buffer, unsigned short int length);
/* Create a Copy of a Message */
bfcp_message *bfcp_copy_message(bfcp_message *message);
/* Free a Message */
int bfcp_free_message(bfcp_message *message);

/* Create a New Entity (Conference ID, Transaction ID, User ID) */
bfcp_entity *bfcp_new_entity(unsigned long int conferenceID, unsigned short int transactionID, unsigned short int userID);
/* Free an Entity */
int bfcp_free_entity(bfcp_entity *entity);

/* Create a new Floor ID list (first argument must be a valid ID, last argument MUST be 0) */
bfcp_floor_id_list *bfcp_new_floor_id_list(unsigned short int fID, ...);
/* Add IDs to an existing Floor ID list (last argument MUST be 0) */
int bfcp_add_floor_id_list(bfcp_floor_id_list *list, unsigned short int fID, ...);
/* Free a Floor ID list */
int bfcp_free_floor_id_list(bfcp_floor_id_list *list);

/* Create a new Supported (Primitives/Attributes) list (last argument MUST be 0) */
bfcp_supported_list *bfcp_new_supported_list(unsigned short int element, ...);
/* Free a Supported (Primitives/Attributes) list */
int bfcp_free_supported_list(bfcp_supported_list *list);

/* Create a New Request Status (RequestStatus/QueuePosition) */
bfcp_request_status *bfcp_new_request_status(unsigned short int rs, unsigned short int qp);
/* Free a Request Status (RequestStatus/QueuePosition) */
int bfcp_free_request_status(bfcp_request_status *request_status);

/* Create a New Error (Code/Details) */
bfcp_error *bfcp_new_error(unsigned short int code, void *details);
/* Free an Error (Code/Details) */
int bfcp_free_error(bfcp_error *error);

/* Create a New Error Details list (for Error 4: UNKNOWN_M) (last argument MUST be 0) */
bfcp_unknown_m_error_details *bfcp_new_unknown_m_error_details_list(unsigned short int attribute, ...);
/* Add Attributes to an existing Error Details list (for Error 4: UNKNOWN_M) (last argument MUST be 0) */
int bfcp_add_unknown_m_error_details_list(bfcp_unknown_m_error_details *list, unsigned short int attribute, ...);
/* Free an Error Details list */
int bfcp_free_unknown_m_error_details_list(bfcp_unknown_m_error_details *details);

/* Create a New User (Beneficiary/RequestedBy) Information */
bfcp_user_information *bfcp_new_user_information(unsigned short int ID, char *display, char *uri);
/* Free an User (Beneficiary/RequestedBy) Information */
int bfcp_free_user_information(bfcp_user_information *info);

/* Create a new Floor Request Information */
bfcp_floor_request_information *bfcp_new_floor_request_information(unsigned short int frqID, bfcp_overall_request_status *oRS, bfcp_floor_request_status *fRS, bfcp_user_information *beneficiary, bfcp_user_information *requested_by, unsigned short int priority ,char *pInfo);
/* Create a Floor Request Information list (last argument MUST be NULL) */
int bfcp_list_floor_request_information(bfcp_floor_request_information *frqInfo, ...);
/* Add elements to an existing Floor Request Information list (last argument MUST be NULL) */
int bfcp_add_floor_request_information_list(bfcp_floor_request_information *list, ...);
/* Free a Floor Request Information list */
int bfcp_free_floor_request_information_list(bfcp_floor_request_information *frqInfo);

/* Create a New Floor Request Status (FloorID/RequestStatus/QueuePosition/StatusInfo) */
bfcp_floor_request_status *bfcp_new_floor_request_status(unsigned short int fID, unsigned short int rs, unsigned short int qp, char *sInfo);
/* Create a Floor Request Status list (last argument MUST be NULL) */
int bfcp_list_floor_request_status(bfcp_floor_request_status *fRS, ...);
/* Add elements to an existing Floor Request Status list (last argument MUST be NULL) */
int bfcp_add_floor_request_status_list(bfcp_floor_request_status *list, ...);
/* Free a Floor Request Status list */
int bfcp_free_floor_request_status_list(bfcp_floor_request_status *floor_request_status);

/* Create a New Overall Request Status (FloorRequestID/RequestStatus/QueuePosition/StatusInfo) */
bfcp_overall_request_status *bfcp_new_overall_request_status(unsigned short int frqID, unsigned short int rs, unsigned short int qp, char *sInfo);
/* Free an Overall Request Status */
int bfcp_free_overall_request_status(bfcp_overall_request_status *overall_request_status);

/* Create a New Digest */
bfcp_digest *bfcp_new_digest(unsigned short int algorithm);
/* Free a Digest */
int bfcp_free_digest(bfcp_digest *digest);


/* Build Methods */
/* Generic BuildMessage Method */
bfcp_message *bfcp_build_message(bfcp_arguments* arguments);
/* Build Headers */
void bfcp_build_commonheader(bfcp_message *message, bfcp_entity *entity, unsigned short int primitive);
void bfcp_build_attribute_tlv(bfcp_message *message, unsigned short int position, unsigned short int type, unsigned short int mandatory_bit, unsigned short int length);
/* Build Specific Messages */
bfcp_message *bfcp_build_message_FloorRequest(bfcp_entity *entity, bfcp_floor_id_list *fID, unsigned short int bID, char *pInfo, unsigned short int priority);
bfcp_message *bfcp_build_message_FloorRelease(bfcp_entity *entity, unsigned short int frqID);
bfcp_message *bfcp_build_message_FloorRequestQuery(bfcp_entity *entity, unsigned short int frqID);
bfcp_message *bfcp_build_message_FloorRequestStatus(bfcp_entity *entity, bfcp_floor_request_information *frqInfo);
bfcp_message *bfcp_build_message_UserQuery(bfcp_entity *entity, unsigned short int bID);
bfcp_message *bfcp_build_message_UserStatus(bfcp_entity *entity, bfcp_user_information *beneficiary, bfcp_floor_request_information *frqInfo);
bfcp_message *bfcp_build_message_FloorQuery(bfcp_entity *entity, bfcp_floor_id_list *fID);
bfcp_message *bfcp_build_message_FloorStatus(bfcp_entity *entity, bfcp_floor_id_list *fID, bfcp_floor_request_information *frqInfo);
bfcp_message *bfcp_build_message_ChairAction(bfcp_entity *entity, bfcp_floor_request_information *frqInfo);
bfcp_message *bfcp_build_message_ChairActionAck(bfcp_entity *entity);
bfcp_message *bfcp_build_message_Hello(bfcp_entity *entity);
bfcp_message *bfcp_build_message_HelloAck(bfcp_entity *entity, bfcp_supported_list *primitives, bfcp_supported_list *attributes);
bfcp_message *bfcp_build_message_Error(bfcp_entity *entity, bfcp_error *error, char *eInfo);

/* Build Attributes */
int bfcp_build_attribute_BENEFICIARY_ID(bfcp_message *message, unsigned short int bID);
int bfcp_build_attribute_FLOOR_ID(bfcp_message *message, unsigned short int fID);
int bfcp_build_attribute_FLOOR_REQUEST_ID(bfcp_message *message, unsigned short int frqID);
int bfcp_build_attribute_PRIORITY(bfcp_message *message, unsigned short int priority);
int bfcp_build_attribute_REQUEST_STATUS(bfcp_message *message, bfcp_request_status *rs);
int bfcp_build_attribute_ERROR_CODE(bfcp_message *message, bfcp_error *error);
int bfcp_build_attribute_ERROR_INFO(bfcp_message *message, char *eInfo);
int bfcp_build_attribute_PARTICIPANT_PROVIDED_INFO(bfcp_message *message, char *pInfo);
int bfcp_build_attribute_STATUS_INFO(bfcp_message *message, char *sInfo);
int bfcp_build_attribute_SUPPORTED_ATTRIBUTES(bfcp_message *message, bfcp_supported_list *attributes);
int bfcp_build_attribute_SUPPORTED_PRIMITIVES(bfcp_message *message, bfcp_supported_list *primitives);
int bfcp_build_attribute_USER_DISPLAY_NAME(bfcp_message *message, char *display);
int bfcp_build_attribute_USER_URI(bfcp_message *message, char *uri);
int bfcp_build_attribute_BENEFICIARY_INFORMATION(bfcp_message *message, bfcp_user_information *beneficiary);
int bfcp_build_attribute_FLOOR_REQUEST_INFORMATION(bfcp_message *message, bfcp_floor_request_information *frqInfo);
int bfcp_build_attribute_REQUESTED_BY_INFORMATION(bfcp_message *message, bfcp_user_information *requested_by);
int bfcp_build_attribute_FLOOR_REQUEST_STATUS(bfcp_message *message, bfcp_floor_request_status *fRS);
int bfcp_build_attribute_OVERALL_REQUEST_STATUS(bfcp_message *message, bfcp_overall_request_status *oRS);
int bfcp_build_attribute_NONCE(bfcp_message *message, unsigned short int nonce);
int bfcp_build_attribute_DIGEST(bfcp_message *message, bfcp_digest *digest);


/* Parse Methods */
unsigned short int bfcp_get_length(bfcp_message *message);
int bfcp_get_primitive(bfcp_message *message);
unsigned long int bfcp_get_conferenceID(bfcp_message *message);
unsigned short int bfcp_get_transactionID(bfcp_message *message);
unsigned short int bfcp_get_userID(bfcp_message *message);
bfcp_received_message *bfcp_new_received_message(void);
int bfcp_free_received_message(bfcp_received_message *recvM);
bfcp_received_message_error *bfcp_received_message_add_error(bfcp_received_message_error *error, unsigned short int attribute, unsigned short int code);
int bfcp_free_received_message_errors(bfcp_received_message_error *errors);
bfcp_received_attribute *bfcp_new_received_attribute(void);
int bfcp_free_received_attribute(bfcp_received_attribute *recvA);
bfcp_received_message *bfcp_parse_message(bfcp_message *message);
bfcp_received_attribute *bfcp_parse_attribute(bfcp_message *message);
int bfcp_parse_arguments(bfcp_received_message *recvM, bfcp_message *message);
int bfcp_parse_attribute_BENEFICIARY_ID(bfcp_message *message, bfcp_received_attribute *recvA);
int bfcp_parse_attribute_FLOOR_ID(bfcp_message *message, bfcp_received_attribute *recvA);
int bfcp_parse_attribute_FLOOR_REQUEST_ID(bfcp_message *message, bfcp_received_attribute *recvA);
int bfcp_parse_attribute_PRIORITY(bfcp_message *message, bfcp_received_attribute *recvA);
bfcp_request_status *bfcp_parse_attribute_REQUEST_STATUS(bfcp_message *message, bfcp_received_attribute *recvA);
bfcp_error *bfcp_parse_attribute_ERROR_CODE(bfcp_message *message, bfcp_received_attribute *recvA);
char *bfcp_parse_attribute_ERROR_INFO(bfcp_message *message, bfcp_received_attribute *recvA);
char *bfcp_parse_attribute_PARTICIPANT_PROVIDED_INFO(bfcp_message *message, bfcp_received_attribute *recvA);
char *bfcp_parse_attribute_STATUS_INFO(bfcp_message *message, bfcp_received_attribute *recvA);
bfcp_supported_list *bfcp_parse_attribute_SUPPORTED_ATTRIBUTES(bfcp_message *message, bfcp_received_attribute *recvA);
bfcp_supported_list *bfcp_parse_attribute_SUPPORTED_PRIMITIVES(bfcp_message *message, bfcp_received_attribute *recvA);
char *bfcp_parse_attribute_USER_DISPLAY_NAME(bfcp_message *message, bfcp_received_attribute *recvA);
char *bfcp_parse_attribute_USER_URI(bfcp_message *message, bfcp_received_attribute *recvA);
bfcp_user_information *bfcp_parse_attribute_BENEFICIARY_INFORMATION(bfcp_message *message, bfcp_received_attribute *recvA);
bfcp_floor_request_information *bfcp_parse_attribute_FLOOR_REQUEST_INFORMATION(bfcp_message *message, bfcp_received_attribute *recvA);
bfcp_user_information *bfcp_parse_attribute_REQUESTED_BY_INFORMATION(bfcp_message *message, bfcp_received_attribute *recvA);
bfcp_floor_request_status *bfcp_parse_attribute_FLOOR_REQUEST_STATUS(bfcp_message *message, bfcp_received_attribute *recvA);
bfcp_overall_request_status *bfcp_parse_attribute_OVERALL_REQUEST_STATUS(bfcp_message *message, bfcp_received_attribute *recvA);
int bfcp_parse_attribute_NONCE(bfcp_message *message, bfcp_received_attribute *recvA);
bfcp_digest *bfcp_parse_attribute_DIGEST(bfcp_message *message, bfcp_received_attribute *recvA);

#if defined __cplusplus
	}
#endif

#endif
