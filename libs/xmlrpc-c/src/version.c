#include "version.h"

#include "xmlrpc-c/base.h"

unsigned int const xmlrpc_version_major = XMLRPC_VERSION_MAJOR;
unsigned int const xmlrpc_version_minor = XMLRPC_VERSION_MINOR;
unsigned int const xmlrpc_version_point = XMLRPC_VERSION_POINT;

void
xmlrpc_version(unsigned int * const majorP,
               unsigned int * const minorP,
               unsigned int * const pointP) {

    *majorP = XMLRPC_VERSION_MAJOR;
    *minorP = XMLRPC_VERSION_MINOR;
    *pointP = XMLRPC_VERSION_POINT;
}

