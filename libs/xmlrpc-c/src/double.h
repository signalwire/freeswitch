#ifndef DOUBLE_H_INCLUDED
#define DOUBLE_H_INCLUDED

#include "xmlrpc-c/util.h"

void
xmlrpc_formatFloat(xmlrpc_env *  const envP,
                   double        const value,
                   const char ** const formattedP);

#endif
