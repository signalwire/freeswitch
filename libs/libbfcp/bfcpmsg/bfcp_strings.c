#include "bfcp_strings.h"

/* Retrieve the string of BFCP primitive */
const char* get_bfcp_primitive(int p)
{
    char *primitive = "Unknown Primitive";

    if ( p > 0 || p <= 16 ) {
        return bfcp_primitive[p-1].description ;
    }

    return primitive;
}

/* Retrieve the string of BFCP attribute */
const char* get_bfcp_attribute(int a)
{
    char *attribute = "Unknown Attribute";

    if ( a > 0 || a <= 20 ) {
        return bfcp_attribute[a-1].description;
    }

    return attribute ;
}

/* Retrieve the string of BFCP status */
const char* get_bfcp_status(int s)
{
    char *status = "Invalid";

    if ( s > 0 || s <= 7 ) {
        return bfcp_status[s-1].description;
    }

    return status;
}

/* Retrieve the string of BFCP priority */
const char* get_bfcp_priority(unsigned short int p)
{
    char *priority = "Lowest";

    if (p > 0 || p <= 4) {
        return bfcp_priority[p-1].description;
    }

    return priority;
}

/* Retrieve the string of BFCP error type */
const char* get_bfcp_error_type(int et)
{
    char *error_type = "Unknown Error Type";

    if (et > 0 || et <= 12) {
        return bfcp_error_type[et-1].description;
    }

    return error_type;
}

/* Retrieve the string of parsing error */
const char* get_bfcp_parsing_errors(int e)
{
    char *error = "Unknown Parsing Error";

    if (e > 0 || e <= 6) {
        return bfcp_parsing_error[e - 1].description;
    }

    return error;
}
