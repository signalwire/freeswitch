#ifndef HANDLER_H_INCLUDED
#define HANDLER_H_INCLUDED

#include "bool.h"
#include "xmlrpc-c/abyss.h"

typedef struct BIHandler BIHandler;

BIHandler *
HandlerCreate(void);

void
HandlerDestroy(BIHandler * const handlerP);


void
HandlerSetMimeType(BIHandler * const handlerP,
                   MIMEType *  const mimeTypeP);

void
HandlerSetFilesPath(BIHandler *  const handlerP,
                    const char * const filesPath);

void
HandlerAddDefaultFN(BIHandler *  const handlerP,
                    const char * const fileName);

abyss_bool
HandlerDefaultBuiltin(TSession * const sessionP);

#endif
