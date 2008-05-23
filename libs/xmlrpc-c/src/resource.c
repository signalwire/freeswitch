#include "xmlrpc_config.h"

#include "xmlrpc-c/base.h"


/*=========================================================================
**  Resource Limits
**=========================================================================
*/ 

static size_t limits[XMLRPC_LAST_LIMIT_ID + 1] = {
    XMLRPC_NESTING_LIMIT_DEFAULT,
    XMLRPC_XML_SIZE_LIMIT_DEFAULT
};

void
xmlrpc_limit_set (int    const limit_id,
                  size_t const value) {

    XMLRPC_ASSERT(0 <= limit_id && limit_id <= XMLRPC_LAST_LIMIT_ID);
    limits[limit_id] = value;
}



size_t
xmlrpc_limit_get(int const limit_id) {

    XMLRPC_ASSERT(0 <= limit_id && limit_id <= XMLRPC_LAST_LIMIT_ID);
    return limits[limit_id];
}
