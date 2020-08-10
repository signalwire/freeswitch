/* String definitions for BFCP numeric values */


#include "bfcp_messages.h"

static const struct bfcp_primitives {
	int primitive;
	const char *description;
} bfcp_primitive[] = {
	{ FloorRequest, "FloorRequest" },
	{ FloorRelease, "FloorRelease" },
	{ FloorRequestQuery, "FloorRequestQuery" },
	{ FloorRequestStatus, "FloorRequestStatus" },
	{ UserQuery, "UserQuery" },
	{ UserStatus, "UserStatus" },
	{ FloorQuery, "FloorQuery" },
	{ FloorStatus, "FloorStatus" },
	{ ChairAction, "ChairAction" },
	{ ChairActionAck, "ChairActionAck" },
	{ Hello, "Hello" },
	{ HelloAck, "HelloAck" },
	{ Error, "Error" },
	{ FloorRequestStatusAck, "FloorRequestStatusAck" },
	{ ErrorAck, "ErrorAck" },
	{ FloorStatusAck, "FloorStatusAck" },
};

static const struct bfcp_attributes {
	int attribute;
	const char *description;
} bfcp_attribute[] = {
	{ BENEFICIARY_ID, "BENEFICIARY-ID" },
	{ FLOOR_ID, "FLOOR-ID" },
	{ FLOOR_REQUEST_ID, "FLOOR-REQUEST-ID" },
	{ PRIORITY, "PRIORITY" },
	{ REQUEST_STATUS, "REQUEST-STATUS" },
	{ ERROR_CODE, "ERROR-CODE" },
	{ ERROR_INFO, "ERROR-INFO" },
	{ PARTICIPANT_PROVIDED_INFO, "PARTICIPANT-PROVIDED-INFO" },
	{ STATUS_INFO, "STATUS-INFO" },
	{ SUPPORTED_ATTRIBUTES, "SUPPORTED-ATTRIBUTES" },
	{ SUPPORTED_PRIMITIVES, "SUPPORTED-PRIMITIVES" },
	{ USER_DISPLAY_NAME, "USER-DISPLAY-NAME" },
	{ USER_URI, "USER-URI" },
	{ BENEFICIARY_INFORMATION, "BENEFICIARY-INFORMATION" },
	{ FLOOR_REQUEST_INFORMATION, "FLOOR-REQUEST-INFORMATION" },
	{ REQUESTED_BY_INFORMATION, "REQUESTED-BY-INFORMATION" },
	{ FLOOR_REQUEST_STATUS, "FLOOR-REQUEST-STATUS" },
	{ OVERALL_REQUEST_STATUS, "OVERALL-REQUEST-STATUS" },
	{ NONCE, "NONCE" },
	{ DIGEST, "DIGEST" },
};

static const struct bfcp_statuses {
	int status;
	const char *description;
} bfcp_status[] = {
	{ BFCP_PENDING, "Pending" },
	{ BFCP_ACCEPTED, "Accepted" },
	{ BFCP_GRANTED, "Granted" },
	{ BFCP_DENIED, "Denied" },
	{ BFCP_CANCELLED, "Cancelled" },
	{ BFCP_RELEASED, "Released" },
	{ BFCP_REVOKED, "Revoked" },
};

static const struct bfcp_priorities {
	int priority;
	const char *description;
} bfcp_priority[] = {
	{ BFCP_LOWEST_PRIORITY, "Lowest" },
	{ BFCP_LOW_PRIORITY, "Low" },
	{ BFCP_NORMAL_PRIORITY, "Normal" },
	{ BFCP_HIGH_PRIORITY, "High" },
	{ BFCP_HIGHEST_PRIORITY, "Highest" },
};

static const struct bfcp_error_types {
	int error;
	const char *description;
} bfcp_error_type[] = {
	{ BFCP_CONFERENCE_DOES_NOT_EXIST, "Conference does not Exist"},
	{ BFCP_USER_DOES_NOT_EXIST, "User does not Exist"},
	{ BFCP_UNKNOWN_PRIMITIVE, "Unknown Primitive"},
	{ BFCP_UNKNOWN_MANDATORY_ATTRIBUTE, "Unknown Mandatory Attribute"},
	{ BFCP_UNAUTHORIZED_OPERATION, "Unauthorized Operation"},
	{ BFCP_INVALID_FLOORID, "Invalid Floor ID"},
	{ BFCP_FLOORREQUEST_DOES_NOT_EXIST, "Floor Request ID Does Not Exist"},
	{ BFCP_MAX_FLOORREQUESTS_REACHED, "You have Already Reached the Maximum Number of Ongoing Floor Requests for this Floor"},
	{ BFCP_USE_TLS, "Use TLS"},
	{ BFCP_DIGEST_ATTRIBUTE_REQUIRED, "Digest Attribute Required"},
	{ BFCP_INVALID_NONCE, "Invalid Nonce"},
	{ BFCP_AUTHENTICATION_FAILED, "Authentication Failed"},
};

static const struct bfcp_parsing_errors {
	int error;
	const char *description;
} bfcp_parsing_error[] = {
	{ BFCP_WRONG_VERSION, "Wrong Version Bit" },
	{ BFCP_RESERVED_NOT_ZERO, "Reserved bits not zeroed" },
	{ BFCP_UNKNOWN_PRIMITIVE, "Unknown Primitive" },
	{ BFCP_UNKNOWN_ATTRIBUTE, "Unknown Attribute" },
	{ BFCP_WRONG_LENGTH, "Wrong Length" },
	{ BFCP_PARSING_ERROR, "Parsing Error" },
};


/*!
  \brief Retrieve the string for corresponding BFCP primitive value
  \param p BFCP primitive value
  \return BFCP primitive string
 */
const char* get_bfcp_primitive(int p);

/*!
  \brief Retrieve the string for corresponding BFCP attribute value
  \param a BFCP attribute value
  \return BFCP attribute string
 */
const char* get_bfcp_attribute(int a);

/*!
  \brief Retrieve the string for corresponding BFCP status value
  \param s BFCP status value
  \return BFCP status string
 */
const char* get_bfcp_status(int s);

/*!
  \brief Retrieve the string for corresponding BFCP priority
  \param p BFCP priority value
  \return BFCP priority string
 */
const char* get_bfcp_priority(unsigned short int p);

/*!
  \brief Retrieve the string for corresponding BFCP error_type
  \param et BFCP error_type value
  \return BFCP error_type string
 */
const char* get_bfcp_error_type(int et);

/*!
  \brief Retrieve the string for corresponding BFCP parsing_error
  \param e BFCP parsing_error value
  \return BFCP parsing_error string
 */
const char* get_bfcp_parsing_errors(int e);

