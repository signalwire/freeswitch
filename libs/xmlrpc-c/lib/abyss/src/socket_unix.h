#ifndef SOCKET_UNIX_H_INCLUDED
#define SOCKET_UNIX_H_INCLUDED

#include <sys/socket.h>

#include <xmlrpc-c/abyss.h>

void
SocketUnixInit(const char ** const errorP);

void
SocketUnixTerm(void);

#endif
