#ifndef PARSE_VALUE_H_INCLUDED
#define PARSE_VALUE_H_INCLUDED

#include "xmlrpc-c/base.h"
#include "xmlrpc-c/xmlparser.h"

void
xmlrpc_parseValue(xmlrpc_env *    const envP,
                  unsigned int    const maxRecursion,
                  xml_element *   const elemP,
                  xmlrpc_value ** const valuePP);

#endif
