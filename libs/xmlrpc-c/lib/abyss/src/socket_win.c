/*=============================================================================
                                 socket_win.c
===============================================================================
  This is the implementation of TChanSwitch and TChannel
  for a Winsock socket.
=============================================================================*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <winsock2.h>

#include "xmlrpc_config.h"
#include "xmlrpc-c/util_int.h"
#include "xmlrpc-c/string_int.h"
#include "mallocvar.h"
#include "trace.h"
#include "chanswitch.h"
#include "channel.h"
#include "socket.h"
#include "xmlrpc-c/abyss.h"

#include "socket_win.h"

#ifndef socklen_t
typedef int socklen_t;
#endif

/* =============================================================
   Provided nice error strings, NOT available in system errors.
   ============================================================= */

typedef struct tagSOCKERRS {
   int err;       // WSAGetLastError() value
   char * desc;   // description of error
} SOCKERR;

/* list shamelessly copied from apache apr errorcodes.c - Grmt 2011-06-16 */

SOCKERR sSockErr[] = {
    WSAEINTR,           "Interrupted system call",
    WSAEBADF,           "Bad file number",
    WSAEACCES,          "Permission denied",
    WSAEFAULT,          "Bad address",
    WSAEINVAL,          "Invalid argument",
    WSAEMFILE,          "Too many open sockets",
    WSAEWOULDBLOCK,     "Operation would block",
    WSAEINPROGRESS,     "Operation now in progress",
    WSAEALREADY,        "Operation already in progress",
    WSAENOTSOCK,        "Socket operation on non-socket",
    WSAEDESTADDRREQ,    "Destination address required",
    WSAEMSGSIZE,        "Message too long",
    WSAEPROTOTYPE,      "Protocol wrong type for socket",
    WSAENOPROTOOPT,     "Bad protocol option",
    WSAEPROTONOSUPPORT, "Protocol not supported",
    WSAESOCKTNOSUPPORT, "Socket type not supported",
    WSAEOPNOTSUPP,      "Operation not supported on socket",
    WSAEPFNOSUPPORT,    "Protocol family not supported",
    WSAEAFNOSUPPORT,    "Address family not supported",
    WSAEADDRINUSE,      "Address already in use",
    WSAEADDRNOTAVAIL,   "Can't assign requested address",
    WSAENETDOWN,        "Network is down",
    WSAENETUNREACH,     "Network is unreachable",
    WSAENETRESET,       "Net connection reset",
    WSAECONNABORTED,    "Software caused connection abort",
    WSAECONNRESET,      "Connection reset by peer",
    WSAENOBUFS,         "No buffer space available",
    WSAEISCONN,         "Socket is already connected",
    WSAENOTCONN,        "Socket is not connected",
    WSAESHUTDOWN,       "Can't send after socket shutdown",
    WSAETOOMANYREFS,    "Too many references, can't splice",
    WSAETIMEDOUT,       "Connection timed out",
    WSAECONNREFUSED,    "Connection refused",
    WSAELOOP,           "Too many levels of symbolic links",
    WSAENAMETOOLONG,    "File name too long",
    WSAEHOSTDOWN,       "Host is down",
    WSAEHOSTUNREACH,    "No route to host",
    WSAENOTEMPTY,       "Directory not empty",
    WSAEPROCLIM,        "Too many processes",
    WSAEUSERS,          "Too many users",
    WSAEDQUOT,          "Disc quota exceeded",
    WSAESTALE,          "Stale NFS file handle",
    WSAEREMOTE,         "Too many levels of remote in path",
    WSASYSNOTREADY,     "Network system is unavailable",
    WSAVERNOTSUPPORTED, "Winsock version out of range",
    WSANOTINITIALISED,  "WSAStartup not yet called",
    WSAEDISCON,         "Graceful shutdown in progress",
    WSAHOST_NOT_FOUND,  "Host not found",
    WSANO_DATA,         "No host data of that type was found",
    0,                  NULL
};


static const char *
getWSAError(int const wsaErrno) {

    SOCKERR * pseP;
  
    pseP = &sSockErr[0];  // initial value
   
    while (pseP->desc) {
        if (pseP->err == wsaErrno)
            return pseP->desc;
        
        ++pseP;
    }

    return "No description available";
}



struct socketWin {
/*----------------------------------------------------------------------------
   The properties/state of a TSocket unique to a Unix TSocket.
-----------------------------------------------------------------------------*/
    SOCKET winsock;
    bool userSuppliedWinsock;
        /* 'socket' was supplied by the user; it belongs to him */
    HANDLE interruptEvent;
};

static
bool
connected(SOCKET const fd) {
/*----------------------------------------------------------------------------
   Return TRUE iff the socket on file descriptor 'fd' is in the connected
   state.
   If 'fd' does not identify a stream socket or we are unable to determine
   the state of the stream socket, the answer is "false".
-----------------------------------------------------------------------------*/
    bool connected;
    struct sockaddr sockaddr;
    socklen_t nameLen;
    int rc;

    nameLen = sizeof(sockaddr);

    rc = getpeername(fd, &sockaddr, &nameLen);

    if (rc == 0)
        connected = TRUE;
    else
        connected = FALSE;

    return connected;
}



void
SocketWinInit(const char ** const errorP) {

    WORD wVersionRequested;
    WSADATA wsaData;
    int err;
 
    wVersionRequested = MAKEWORD(1, 0);
 
    err = WSAStartup(wVersionRequested, &wsaData);

    if (err != 0) {
        int const lastError = WSAGetLastError();
        xmlrpc_asprintf(errorP, "WSAStartup() faild with error %d (%s)",
                        lastError, getWSAError(lastError));
    } else
        *errorP = NULL;
}



void
SocketWinTerm(void) {
    
    WSACleanup();
}



/*=============================================================================
      TChannel
=============================================================================*/

static ChannelDestroyImpl channelDestroy;

static void
channelDestroy(TChannel * const channelP) {

    struct socketWin * const socketWinP = channelP->implP;

    if (!socketWinP->userSuppliedWinsock)
        closesocket(socketWinP->winsock);

    CloseHandle(socketWinP->interruptEvent);

    free(socketWinP);
    channelP->implP = 0;

}



static ChannelWriteImpl channelWrite;

static void
channelWrite(TChannel *            const channelP,
             const unsigned char * const buffer,
             uint32_t              const len,
             bool *                const failedP) {

    struct socketWin * const socketWinP = channelP->implP;

    size_t bytesLeft;
    bool error;
	int to_count = 0;
	int lastError = 0;

	for (bytesLeft = len, error = FALSE; bytesLeft > 0 && !error;) {
        size_t const maxSend = 	4096 * 2; /* with respect to resource allocation this might be a better value than 2^31 */ 

        int rc = send(socketWinP->winsock, buffer + len - bytesLeft, MIN(maxSend, bytesLeft), 0);
		if (rc > 0) {          /* 0 means connection closed; < 0 means severe error */
			to_count = 0;
		    bytesLeft -= rc;
		} 
		else if (!rc) {
			error = TRUE;
			fprintf(stderr, "Abyss: send() failed: connection closed");
		}
		else {
			error = TRUE;
			lastError = WSAGetLastError();
            if (lastError == WSAEWOULDBLOCK || lastError == ERROR_IO_PENDING) {
				SleepEx(20, TRUE);  /* give socket another chance after xx millisec) */
				if (++to_count < 300) {
					error = FALSE;
				}
			    //  fprintf(stderr, "Abyss: send() failed with errno %d (%s) cnt %d, will retry\n", lastError, getWSAError(lastError), to_count);
			}
			if (error) fprintf(stderr, "Abyss: send() failed with errno %d (%s)\n", lastError, getWSAError(lastError));
		}
    }

	*failedP = error;
}



static ChannelReadImpl channelRead;

static void
channelRead(TChannel *   const channelP, 
            unsigned char * const buffer, 
            uint32_t     const bufferSize,
            uint32_t *   const bytesReceivedP,
            bool * const failedP) {

    struct socketWin * const socketWinP = channelP->implP;
    int retries = 300; 
	
	for (*failedP = TRUE; *failedP && retries; retries--) {
		int rc = recv(socketWinP->winsock, buffer, bufferSize, 0);
		int lastError = WSAGetLastError();

		if (rc < 0) {
			if (lastError == WSAEWOULDBLOCK || lastError == ERROR_IO_PENDING) {
				fprintf(stderr, "Abyss: recv() failed with errno %d (%s) cnt %d, will retry\n", lastError, getWSAError(lastError), retries);
				SleepEx(30, TRUE);  /* give socket another chance after xx millisec)*/
				*failedP = FALSE;
			} else {
				fprintf(stderr, "Abyss: recv() failed with errno %d (%s)\n", lastError, getWSAError(lastError));
				break;
			}
		} else {
			*failedP = FALSE;
			*bytesReceivedP = rc;

			if (ChannelTraceIsActive)
				fprintf(stderr, "Abyss channel: read %u bytes: '%.*s'\n", *bytesReceivedP, (int)(*bytesReceivedP), buffer);
		}
	}
}



static ChannelWaitImpl channelWait;

static void
channelWait(TChannel * const channelP,
            bool       const waitForRead,
            bool       const waitForWrite,
            uint32_t   const timems,
            bool *     const readyToReadP,
            bool *     const readyToWriteP,
            bool *     const failedP) {

    struct socketWin * const socketWinP = channelP->implP;

    fd_set rfds, wfds;
    TIMEVAL tv;
    bool failed, readRdy, writeRdy, timedOut;

    FD_ZERO(&rfds);
    FD_ZERO(&wfds);

    if (waitForRead)
        FD_SET(socketWinP->winsock, &rfds);

    if (waitForWrite)
        FD_SET(socketWinP->winsock, &wfds);

    tv.tv_sec  = timems / 1000;
    tv.tv_usec = timems % 1000;
 
    for (failed = FALSE, readRdy = FALSE, writeRdy = FALSE, timedOut = FALSE;
         !failed && !readRdy && !writeRdy && !timedOut;
        ) {

        int rc;

        rc = select(socketWinP->winsock + 1, &rfds, &wfds, NULL,
                    (timems == TIME_INFINITE ? NULL : &tv));

        switch(rc) {   
        case 0:
            timedOut = TRUE;
            break;
        case -1:  /* socket error */
            if (WSAGetLastError() != WSAEINTR)
                failed = TRUE;
        break;
        default:
            if (FD_ISSET(socketWinP->winsock, &rfds))
                readRdy = TRUE;
            if (FD_ISSET(socketWinP->winsock, &wfds))
                writeRdy = TRUE;
        }
    }

    if (failedP)
        *failedP       = failed;
    if (readyToReadP)
        *readyToReadP  = readRdy;
    if (readyToWriteP)
        *readyToWriteP = writeRdy;
}



static ChannelInterruptImpl channelInterrupt;

static void
channelInterrupt(TChannel * const channelP) {
/*----------------------------------------------------------------------------
  Interrupt any waiting that a thread might be doing in channelWait()
  now or in the future.

  Actually, this is just a no-op because we don't yet know how to
  accomplish that.  (But we could probably do it the same way
  chanSwitchInterrupt() works -- no one has needed it enough yet to do that
  work).
-----------------------------------------------------------------------------*/

}



void
ChannelWinGetPeerName(TChannel *           const channelP,
                      struct sockaddr_in * const inAddrP,
                      const char **        const errorP) {

    struct socketWin * const socketWinP = channelP->implP;

    socklen_t addrlen;
    int rc;
    struct sockaddr sockAddr;

    addrlen = sizeof(sockAddr);
    
    rc = getpeername(socketWinP->winsock, &sockAddr, &addrlen);

    if (rc != 0) {
        int const lastError = WSAGetLastError();
        xmlrpc_asprintf(errorP, "getpeername() failed.  WSA error = %d (%s)",
                        lastError, getWSAError(lastError));
    } else {
        if (addrlen != sizeof(sockAddr))
            xmlrpc_asprintf(errorP, "getpeername() returned a socket address "
                            "of the wrong size: %u.  Expected %u",
                            addrlen, sizeof(sockAddr));
        else {
            if (sockAddr.sa_family != AF_INET)
                xmlrpc_asprintf(errorP,
                                "Socket does not use the Inet (IP) address "
                                "family.  Instead it uses family %d",
                                sockAddr.sa_family);
            else {
                *inAddrP = *(struct sockaddr_in *)&sockAddr;

                *errorP = NULL;
            }
        }
    }
}



static ChannelFormatPeerInfoImpl channelFormatPeerInfo;

static void
channelFormatPeerInfo(TChannel *    const channelP,
                      const char ** const peerStringP) {

    struct socketWin * const socketWinP = channelP->implP;

    struct sockaddr sockaddr;
    socklen_t sockaddrLen;
    int rc;

    sockaddrLen = sizeof(sockaddr);
    
    rc = getpeername(socketWinP->winsock, &sockaddr, &sockaddrLen);
    
    if (rc != 0) {
        int const lastError = WSAGetLastError();
        xmlrpc_asprintf(peerStringP, "?? getpeername() failed.  "
                        "WSAERROR %d (%s)",
                        lastError, getWSAError(lastError));
    } else {
        switch (sockaddr.sa_family) {
        case AF_INET: {
            struct sockaddr_in * const sockaddrInP =
                (struct sockaddr_in *) &sockaddr;
            if (sockaddrLen < sizeof(*sockaddrInP))
                xmlrpc_asprintf(peerStringP, "??? getpeername() returned "
                                "the wrong size");
            else {
                unsigned char * const ipaddr = (unsigned char *)
                    &sockaddrInP->sin_addr.s_addr;
                xmlrpc_asprintf(peerStringP, "%u.%u.%u.%u:%hu",
                                ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3],
                                sockaddrInP->sin_port);
            }
        } break;
        default:
            xmlrpc_asprintf(peerStringP, "??? AF=%u", sockaddr.sa_family);
        }
    }
}



static struct TChannelVtbl const channelVtbl = {
    &channelDestroy,
    &channelWrite,
    &channelRead,
    &channelWait,
    &channelInterrupt,
    &channelFormatPeerInfo,
};



static void
makeChannelFromWinsock(SOCKET        const winsock,
                       TChannel **   const channelPP,
                       const char ** const errorP) {

    struct socketWin * socketWinP;

    MALLOCVAR(socketWinP);
    
    if (socketWinP == NULL)
        xmlrpc_asprintf(errorP, "Unable to allocate memory for Windows "
                        "socket descriptor");
    else {
        TChannel * channelP;
        
        socketWinP->winsock = winsock;
        socketWinP->userSuppliedWinsock = TRUE;
        socketWinP->interruptEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

        ChannelCreate(&channelVtbl, socketWinP, &channelP);
        
        if (channelP == NULL)
            xmlrpc_asprintf(errorP, "Unable to allocate memory for "
                            "channel descriptor.");
        else {
            *channelPP = channelP;
            *errorP = NULL;
        }
        if (*errorP) {
            CloseHandle(socketWinP->interruptEvent);
            free(socketWinP);
        }
    }
}



static void
makeChannelInfo(struct abyss_win_chaninfo ** const channelInfoPP,
                struct sockaddr              const peerAddr,
                socklen_t                    const peerAddrLen,
                const char **                const errorP) {

    struct abyss_win_chaninfo * channelInfoP;

    MALLOCVAR(channelInfoP);
    
    if (channelInfoP == NULL)
        xmlrpc_asprintf(errorP, "Unable to allocate memory");
    else {
        channelInfoP->peerAddrLen = peerAddrLen;
        channelInfoP->peerAddr    = peerAddr;
        
        *channelInfoPP = channelInfoP;

        *errorP = NULL;
    }
}



void
ChannelWinCreateWinsock(SOCKET                       const fd,
                        TChannel **                  const channelPP,
                        struct abyss_win_chaninfo ** const channelInfoPP,
                        const char **                const errorP) {

    struct sockaddr peerAddr;
    socklen_t peerAddrLen;
    int rc;

    peerAddrLen = sizeof(peerAddr);

    rc = getpeername(fd, &peerAddr, &peerAddrLen);

    if (rc != 0) {
        int const lastError = WSAGetLastError();
        if (lastError == WSAENOTCONN) {
            /* NOTE: This specific string 'not in connected' is
               required by one of the rpctest suite items, in abyss.c
               (line 186), hence the separation of the error messages
               in this case ...
            */
            xmlrpc_asprintf(errorP, "Socket on file descriptor %d "
                            "is not in connected state. WSAERROR = %d (%s)",
                            fd, lastError, getWSAError(lastError));
        } else
            xmlrpc_asprintf(errorP, "getpeername() failed. WSAERROR = %d (%s)",
                        lastError, getWSAError(lastError));
    } else {
        makeChannelInfo(channelInfoPP, peerAddr, peerAddrLen, errorP);
        if (!*errorP) {
            makeChannelFromWinsock(fd, channelPP, errorP);

            if (*errorP)
                free(*channelInfoPP);
        }
    }
}


/*=============================================================================
      TChanSwitch
=============================================================================*/

static SwitchDestroyImpl chanSwitchDestroy;

void
chanSwitchDestroy(TChanSwitch * const chanSwitchP) {

    struct socketWin * const socketWinP = chanSwitchP->implP;

    if (!socketWinP->userSuppliedWinsock)
        closesocket(socketWinP->winsock);

    CloseHandle(socketWinP->interruptEvent);

    free(socketWinP);
}



static SwitchListenImpl chanSwitchListen;

static void
chanSwitchListen(TChanSwitch * const chanSwitchP,
                 uint32_t      const backlog,
                 const char ** const errorP) {

    struct socketWin * const socketWinP = chanSwitchP->implP;

    int32_t const minus1 = -1;

    int rc;

    /* Disable the Nagle algorithm to make persistant connections faster */

    setsockopt(socketWinP->winsock, IPPROTO_TCP, TCP_NODELAY,
               (const char *)&minus1, sizeof(minus1));

    rc = listen(socketWinP->winsock, backlog);

    if (rc != 0) {
        int const lastError = WSAGetLastError();
        xmlrpc_asprintf(errorP, "setsockopt() failed with WSAERROR %d (%s)",
                        lastError, getWSAError(lastError));
    } else
        *errorP = NULL;
}



static void
createChannelForAccept(int             const acceptedWinsock,
                       struct sockaddr const peerAddr,
                       TChannel **     const channelPP,
                       void **         const channelInfoPP,
                       const char **   const errorP) {

    struct abyss_win_chaninfo * channelInfoP;
    makeChannelInfo(&channelInfoP, peerAddr, sizeof(peerAddr), errorP);
    if (!*errorP) {
        struct socketWin * acceptedSocketP;

        MALLOCVAR(acceptedSocketP);

        if (!acceptedSocketP)
            xmlrpc_asprintf(errorP, "Unable to allocate memory");
        else {
            TChannel * channelP;

            acceptedSocketP->winsock             = acceptedWinsock;
            acceptedSocketP->userSuppliedWinsock = FALSE;
            acceptedSocketP->interruptEvent      =
                CreateEvent(NULL, FALSE, FALSE, NULL);

            ChannelCreate(&channelVtbl, acceptedSocketP, &channelP);
            if (!channelP)
                xmlrpc_asprintf(errorP,
                                "Failed to create TChannel object.");
            else {
                *errorP        = NULL;
                *channelPP     = channelP;
                *channelInfoPP = channelInfoP;
            }
            if (*errorP) {
                CloseHandle(acceptedSocketP->interruptEvent);
                free(acceptedSocketP);
            }
        }
    }
}



static SwitchAcceptImpl  chanSwitchAccept;

static void
chanSwitchAccept(TChanSwitch * const chanSwitchP,
                 TChannel **   const channelPP,
                 void **       const channelInfoPP,
                 const char ** const errorP) {
/*----------------------------------------------------------------------------
   Accept a connection via the channel switch *chanSwitchP.  Return as
   *channelPP the channel for the accepted connection.

   If no connection is waiting at *chanSwitchP, wait until one is.

   If we receive a signal while waiting, return immediately with
   *channelPP == NULL.
-----------------------------------------------------------------------------*/
    struct socketWin * const listenSocketP = chanSwitchP->implP;
    HANDLE acceptEvent = WSACreateEvent();
    bool interrupted;
    TChannel * channelP;

    interrupted = FALSE; /* Haven't been interrupted yet */
    channelP    = NULL;  /* No connection yet */
    *errorP     = NULL;  /* No error yet */

    WSAEventSelect(listenSocketP->winsock, acceptEvent,
                   FD_ACCEPT | FD_CLOSE | FD_READ);

    while (!channelP && !*errorP && !interrupted) {
        HANDLE interrupts[2] = {acceptEvent, listenSocketP->interruptEvent};
        int rc;
        struct sockaddr peerAddr;
        socklen_t size = sizeof(peerAddr);

        rc = WaitForMultipleObjects(2, interrupts, FALSE, INFINITE);
        if (WAIT_OBJECT_0 + 1 == rc) {
            interrupted = TRUE;
            continue;
        };

        rc = accept(listenSocketP->winsock, &peerAddr, &size);

        if (rc >= 0) {
            int const acceptedWinsock = rc;

            createChannelForAccept(acceptedWinsock, peerAddr,
                                   &channelP, channelInfoPP, errorP);

            if (*errorP)
                closesocket(acceptedWinsock);
        } else {
            int const lastError = WSAGetLastError();

            if (lastError == WSAEINTR)
                interrupted = TRUE;
            else
                xmlrpc_asprintf(errorP,
                                "accept() failed, WSA error = %d (%s)",
                                lastError, getWSAError(lastError));
        }
    }
    *channelPP = channelP;
    CloseHandle(acceptEvent);
}



static SwitchInterruptImpl chanSwitchInterrupt;

static void
chanSwitchInterrupt(TChanSwitch * const chanSwitchP) {
/*----------------------------------------------------------------------------
  Interrupt any waiting that a thread might be doing in chanSwitchAccept()
  now or in the future.
-----------------------------------------------------------------------------*/
    struct socketWin * const listenSocketP = chanSwitchP->implP;

    SetEvent(listenSocketP->interruptEvent);
}



static struct TChanSwitchVtbl const chanSwitchVtbl = {
    &chanSwitchDestroy,
    &chanSwitchListen,
    &chanSwitchAccept,
    &chanSwitchInterrupt,
};



static void
setSocketOptions(SOCKET        const fd,
                 const char ** const errorP) {

    int32_t const n = 1;

    int rc;

    rc = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&n, sizeof(n));

    if (rc != 0) {
        int const lastError = WSAGetLastError();
        xmlrpc_asprintf(errorP, "Failed to set socket options.  "
                        "setsockopt() failed with WSAERROR %d (%s)",
                        lastError, getWSAError(lastError));
    } else
        *errorP = NULL;
}



void
bindSocketToPort(SOCKET           const winsock,
                 struct in_addr * const addrP,
                 uint16_t         const portNumber,
                 const char **    const errorP) {
    
    struct sockaddr_in name;
    int rc;
	int one = 1;

    ZeroMemory(&name, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port   = htons(portNumber);
    if (addrP)
        name.sin_addr = *addrP;

	setsockopt(winsock, SOL_SOCKET, SO_REUSEADDR, (void *)&one, sizeof(int));
    rc = bind(winsock, (struct sockaddr *)&name, sizeof(name));

    if (rc != 0) {
        int const lastError = WSAGetLastError();
        xmlrpc_asprintf(errorP, "Unable to bind socket to port number %u.  "
                        "bind() failed with WSAERROR %i (%s)",
                        portNumber, lastError, getWSAError(lastError));
    } else
        *errorP = NULL;
}



void
ChanSwitchWinCreate(uint16_t       const portNumber,
                    TChanSwitch ** const chanSwitchPP,
                    const char **  const errorP) {
/*----------------------------------------------------------------------------
   Create a Winsock-based channel switch.

   Set the socket's local address so that a subsequent "listen" will listen
   on all IP addresses, port number 'portNumber'.
-----------------------------------------------------------------------------*/
    struct socketWin * socketWinP;

    MALLOCVAR(socketWinP);

    if (!socketWinP)
        xmlrpc_asprintf(errorP, "Unable to allocate memory for Windows socket "
                        "descriptor structure.");
    else {
        SOCKET winsock;

        winsock = socket(AF_INET, SOCK_STREAM, 0);

        if (winsock == 0 || winsock == INVALID_SOCKET) {
            int const lastError = WSAGetLastError();
            xmlrpc_asprintf(errorP, "socket() failed with WSAERROR %d (%s)",
                            lastError, getWSAError(lastError));
        } else {
            socketWinP->winsock = winsock;
            socketWinP->userSuppliedWinsock = FALSE;
            socketWinP->interruptEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
            
            setSocketOptions(socketWinP->winsock, errorP);
            if (!*errorP) {
                bindSocketToPort(socketWinP->winsock, NULL, portNumber,
                                 errorP);
                if (!*errorP)
                    ChanSwitchCreate(&chanSwitchVtbl, socketWinP,
                                     chanSwitchPP);
            }

            if (*errorP) {
                CloseHandle(socketWinP->interruptEvent);
                closesocket(winsock);
            }
        }
        if (*errorP)
            free(socketWinP);
    }
}



void
ChanSwitchWinCreateWinsock(SOCKET         const winsock,
                           TChanSwitch ** const chanSwitchPP,
                           const char **  const errorP) {

    struct socketWin * socketWinP;

    if (connected(winsock))
        xmlrpc_asprintf(errorP, "Socket is in connected state.");
    else {
        MALLOCVAR(socketWinP);

        if (socketWinP == NULL)
            xmlrpc_asprintf(errorP, "unable to allocate memory for Windows "
                            "socket descriptor.");
        else {
            TChanSwitch * chanSwitchP;

            socketWinP->winsock = winsock;
            socketWinP->userSuppliedWinsock = TRUE;
            socketWinP->interruptEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

            ChanSwitchCreate(&chanSwitchVtbl, socketWinP, &chanSwitchP);

            if (chanSwitchP == NULL)
                xmlrpc_asprintf(errorP, "Unable to allocate memory for "
                                "channel switch descriptor");
            else {
                *chanSwitchPP = chanSwitchP;
                *errorP = NULL;
            }
            if (*errorP) {
                CloseHandle(socketWinP->interruptEvent);
                free(socketWinP);
            }
        }
    }
}
