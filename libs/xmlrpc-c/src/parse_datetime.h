#ifndef PARSE_DATETIME_H_INCLUDED
#define PARSE_DATETIME_H_INCLUDED

#include "xmlrpc-c/util.h"
#include "xmlrpc-c/base.h"

void
xmlrpc_parseDatetime(xmlrpc_env *    const envP,
                     const char *    const str,
                     xmlrpc_value ** const valuePP);

#endif
