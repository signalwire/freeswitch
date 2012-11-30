/*=============================================================================
                                 socket_openssl.c
===============================================================================
  This is the implementation of TChanSwitch and TChannel
  for an SSL (Secure Sockets Layer) connection based on an OpenSSL
  connection object -- what you create with SSL_new().

  This is just a template for future development.  It does not function
  (or even compile) today.

=============================================================================*/

#include "xmlrpc_config.h"

#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include "xmlrpc-c/util_int.h"
#include "xmlrpc-c/string_int.h"
#include "bool.h"
#include "mallocvar.h"
#include "trace.h"
#include "chanswitch.h"
#include "channel.h"
#include "socket.h"
#include "xmlrpc-c/abyss.h"

#include "socket_openssl.h"



struct channelOpenSsl {
/*----------------------------------------------------------------------------
   The properties/state of a TChannel unique to the OpenSSL variety.
-----------------------------------------------------------------------------*/
    SSL * sslP;
        /* SSL connection handle (such as is created by SSL_new() in
           the openssl library).
        */
    bool userSuppliedConn;
        /* The SSL connection belongs to the user; we did not create
           it.
        */
};



static bool
connected(int const fd) {
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
SocketOpensslInit(const char ** const errorP) {

	SSL_load_error_strings();
        /* readable error messages, don't call this if memory is tight */
	SSL_library_init();   /* initialize library */

	/* actions_to_seed_PRNG(); */

	*errorP = NULL;
}



void
SocketOpenSslTerm(void) {

	ERR_free_string();

}



/*=============================================================================
      TChannel
=============================================================================*/

static ChannelDestroyImpl channelDestroy;

static void
channelDestroy(TChannel * const channelP) {

    struct channelOpenssl * const channelOpensslP = channelP->implP;

    if (!socketUnixP->userSuppliedConn)
        SSL_shutdown(channelOpensslP->sslP);

    free(channelOpensslP);
	channelP->implP = 0;

}



static ChannelWriteImpl channelWrite;

static void
channelWrite(TChannel *            const channelP,
             const unsigned char * const buffer,
             uint32_t              const len,
             bool *                const failedP) {

    struct channelOpenssl * const channelOpensslP = channelP->implP;

    int bytesLeft;
    bool error;

    assert(sizeof(int) >= sizeof(len));

    for (bytesLeft = len, error = FALSE;
         bytesLeft > 0 && !error;
        ) {
        int const maxSend = (int)(-1) >> 1;

        int rc;
        
        rc = SSL_write(channelOpensslP->ssl, &buffer[len-bytesLeft],
                       MIN(maxSend, bytesLeft));

        if (ChannelTraceIsActive) {
            if (rc <= 0)
                fprintf(stderr,
                        "Abyss socket: SSL_write() failed.  rc=%d (%s)",
                        rc, SSL_get_error(rc));
            else
                fprintf(stderr, "Abyss socket: sent %u bytes: '%.*s'\n",
                        rc, rc, &buffer[len-bytesLeft]);
        }
        if (rc <= 0)
            /* 0 means connection closed; < 0 means severe error */
            error = TRUE;
        else
            bytesLeft -= rc;
    }
    *failedP = error;
}



ChannelReadImpl channelRead;

static void
channelRead(TChannel *      const channelP, 
            unsigned char * const buffer, 
            uint32_t        const bufferSize,
            uint32_t *      const bytesReceivedP,
            bool *          const failedP) {

    struct channelOpenssl * const channelOpensslP = channelP->implP;

    int rc;
    rc = SSL_read(channelOpensslP->sslP, buf, len);

    if (rc < 0) {
        *failedP = TRUE;
        if (ChannelTraceIsActive)
            fprintf(stderr, "Failed to receive data from OpenSSL connection.  "
                    "SSL_read() failed with rc %d (%s)\n",
                    rc, SSL_get_error(rc));
    } else {
        *failedP = FALSE;
        *bytesReceivedP = rc;

        if (ChannelTraceIsActive)
            fprintf(stderr, "Abyss channel: read %u bytes: '%.*s'\n",
                    *bytesReceivedP, (int)(*bytesReceivedP), buffer);
    }
}



static ChannelWaitImpl channelWait;

static void
channelWait(TChannel * const channelP,
            bool       const waitForRead,
            bool       const waitForWrite,
            uint32_t   const timeoutMs,
            bool *     const readyToReadP,
            bool *     const readyToWriteP,
            bool *     const failedP) {
/*----------------------------------------------------------------------------
  See socket_unix.c for an explanation of the purpose of this
  subroutine.

  We don't actually fulfill that purpose, though, because we don't know
  how yet.  Instead, we return immediately and hope that if Caller
  subsequently does a read or write, it blocks until it can do its thing.
-----------------------------------------------------------------------------*/

}



static ChannelInterruptImpl channelInterrupt;

static void
channelInterrupt(TChannel * const channelP) {
/*----------------------------------------------------------------------------
  Interrupt any waiting that a thread might be doing in channelWait()
  now or in the future.
-----------------------------------------------------------------------------*/

    /* This is trivial, since channelWait() doesn't actually wait */
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
makeChannelInfo(struct abyss_openssl_chaninfo ** const channelInfoPP,
                SSL *                            const sslP,
                const char **                    const errorP) {

    struct abyss_openssl_chaninfo * channelInfoP;

    MALLOCVAR(channelInfoP);
    
    if (channelInfoP == NULL)
        xmlrpc_asprintf(errorP, "Unable to allocate memory");
    else {
        
        *channelInfoPP = channelInfoP;

        *errorP = NULL;
    }
}



static void
makeChannelFromSsl(SSL *         const sslP,
                   TChannel **   const channelPP,
                   const char ** const errorP) {

    struct channelOpenssl * channelOpensslP;

    MALLOCVAR(channelOpensslP);
    
    if (channelOpensslP == NULL)
        xmlrpc_asprintf(errorP, "Unable to allocate memory for OpenSSL "
                        "socket descriptor");
    else {
        TChannel * channelP;
        
        channelOpensslP->sslP = sslP;
        channelOpensslP->userSuppliedSsl = TRUE;
        
        /* This should be ok as far as I can tell */
        ChannelCreate(&channelVtbl, channelOpensslP, &channelP);
        
        if (channelP == NULL)
            xmlrpc_asprintf(errorP, "Unable to allocate memory for "
                            "channel descriptor.");
        else {
            *channelPP = channelP;
            *errorP = NULL;
        }
        if (*errorP)
            free(channelOpensslP);
    }
}



void
ChannelOpensslCreateSsl(SSL *                            const sslP,
                        TChannel **                      const channelPP,
                        struct abyss_openssl_chaninfo ** const channelInfoPP,
                        const char **                    const errorP) {

    assert(sslP);

    makeChannelInfo(channelInfoPP, sslP, errorP);
    if (!*errorP) {
        makeChannelFromSsl(ssl, channelPP, errorP);
        
        if (*errorP) {
            free(*channelInfoPP);
        }
    }
}



/*=============================================================================
      TChanSwitch
=============================================================================*/

struct opensslSwitch {
/*----------------------------------------------------------------------------
   The properties/state of a TChanSwitch uniqe to the OpenSSL variety.

   Note that OpenSSL deals only in connected sockets, so this switch
   doesn't really have anything to do with OpenSSL except that it
   creates OpenSSL TChannels.  The switch is just a POSIX listening
   socket, and is almost identical to the Abyss Unix channel switch.
-----------------------------------------------------------------------------*/
    int fd;
        /* File descriptor of the POSIX socket (such as is created by
           socket() in the C library) for the socket.
        */
    bool userSuppliedFd;
        /* The file descriptor and associated POSIX socket belong to the
           user; we did not create it.
        */
};



static SwitchDestroyImpl chanSwitchDestroy;

static void
chanSwitchDestroy(TChanSwitch * const chanSwitchP) {

static void
chanSwitchDestroy(TChanSwitch * const chanSwitchP) {

    struct opensslSwitch * const opensslSwitchP = chanSwitchP->implP;

    if (!opensslSwitchP->userSuppliedFd)
        close(opensslSwitchP->fd);

    free(opensslSwitchP);
}



static SwitchListenImpl chanSwitchListen;

static void
chanSwitchListen(TChanSwitch * const chanSwitchP,
                 uint32_t      const backlog,
                 const char ** const errorP) {

    struct opensslSwitch * const opensslSwitchP = chanSwitchP->implP;

    int32_t const minus1 = -1;

    int rc;

    /* Disable the Nagle algorithm to make persistant connections faster */

    setsockopt(opensslSwitchP->fd, IPPROTO_TCP, TCP_NODELAY,
               &minus1, sizeof(minus1));

    rc = listen(opensslSwitchP->fd, backlog);

    if (rc == -1)
        xmlrpc_asprintf(errorP, "listen() failed with errno %d (%s)",
                        errno, strerror(errno));
    else
        *errorP = NULL;
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
    struct opensslSwitch * const listenSocketP = chanSwitchP->implP;

    bool interrupted;
    TChannel * channelP;

    interrupted = FALSE; /* Haven't been interrupted yet */
    channelP    = NULL;  /* No connection yet */
    *errorP     = NULL;  /* No error yet */

    while (!channelP && !*errorP && !interrupted) {
        struct sockaddr peerAddr;
        socklen_t size = sizeof(peerAddr);
        int rc;

        rc = accept(listenSocketP->fd, &peerAddr, &size);

        if (rc >= 0) {
            int const acceptedFd = rc;
            struct channelOpenssl * opensslChannelP;

            MALLOCVAR(opensslChannelP);

            if (!opensslChannelP)
                xmlrpc_asprintf(errorP, "Unable to allocate memory");
            else {
                struct abyss_openssl_chaninfo * channelInfoP;
                
                openChannelP->userSuppliedFd = FALSE;
                TODO("turn connected socket 'acceptedFd' into an OpenSSL "
                     "connection opensslChannelP->sslP");

                makeChannelInfo(&channelInfoP, peerAddr, size, errorP);
                if (!*errorP) {
                    *channelInfoPP = channelInfoP;

                    ChannelCreate(&channelVtbl, opensslChannelP, &channelP);
                    if (!channelP)
                        xmlrpc_asprintf(errorP,
                                        "Failed to create TChannel object.");
                    else
                        *errorP = NULL;

                    if (*errorP)
                        free(channelInfoP);
                }
                if (*errorP)
                    free(opensslChannelP);
            }
            if (*errorP)
                close(acceptedFd);
        } else if (errno == EINTR)
            interrupted = TRUE;
        else
            xmlrpc_asprintf(errorP, "accept() failed, errno = %d (%s)",
                            errno, strerror(errno));
    }
    *channelPP = channelP;
}



static SwitchInterruptImpl chanSwitchInterrupt;

static void
chanSwitchInterrupt(TChanSwitch * const chanSwitchP) {
/*----------------------------------------------------------------------------
  Interrupt any waiting that a thread might be doing in chanSwitchAccept()
  now or in the future.

  Actually, this is a no-op, since we don't yet know how to accomplish
  that.
-----------------------------------------------------------------------------*/

}



static struct TChanSwitchVtbl const chanSwitchVtbl = {
    &chanSwitchDestroy,
    &chanSwitchListen,
    &chanSwitchAccept,
};



static void
setSocketOptions(int           const fd,
                 const char ** const errorP) {

    int32_t n = 1;
    int rc;

    rc = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&n, sizeof(n));

    if (rc < 0)
        xmlrpc_asprintf(errorP, "Failed to set socket options.  "
                        "setsockopt() failed with errno %d (%s)",
                        errno, strerror(errno));
    else
        *errorP = NULL;
}



static void
bindSocketToPort(int              const fd,
                 struct in_addr * const addrP,
                 uint16_t         const portNumber,
                 const char **    const errorP) {

    struct sockaddr_in name;
    int rc;

    name.sin_family = AF_INET;
    name.sin_port   = htons(portNumber);
    if (addrP)
        name.sin_addr = *addrP;
    else
        name.sin_addr.s_addr = INADDR_ANY;

    rc = bind(fd, (struct sockaddr *)&name, sizeof(name));

    if (rc == -1)
        xmlrpc_asprintf(errorP, "Unable to bind socket to port number %hu.  "
                        "bind() failed with errno %d (%s)",
                        portNumber, errno, strerror(errno));
    else
        *errorP = NULL;
}



void
ChanSwitchOpensslCreate(unsigned short const portNumber,
                        TChanSwitch ** const chanSwitchPP,
                        const char **  const errorP) {
/*----------------------------------------------------------------------------
   Create an OpenSSL-based channel switch.

   Use an IP socket.

   Set the socket's local address so that a subsequent "listen" will listen
   on all IP addresses, port number 'portNumber'.
-----------------------------------------------------------------------------*/
    struct opensslSwitch * opensslSwitchP;

    MALLOCVAR(opensslSwitchP);

    if (!opensslSwitchP)
        xmlrpc_asprintf(errorP, "Unable to allocate memory for Openssl "
                        "channel switch descriptor structure.");
    else {
        int rc;
        rc = socket(AF_INET, SOCK_STREAM, 0);
        if (rc < 0)
            xmlrpc_asprintf(errorP, "socket() failed with errno %d (%s)",
                            errno, strerror(errno));
        else {
            opensslSwitchP->fd = rc;
            opensslSwitchP->userSuppliedFd = FALSE;

            setSocketOptions(opensslSwitchP->fd, errorP);
            if (!*errorP) {
                bindSocketToPort(opensslSwitchP->fd, NULL, portNumber, errorP);
                
                if (!*errorP)
                    ChanSwitchCreate(&chanSwitchVtbl, opensslSwitchP,
                                     chanSwitchPP);
            }
            if (*errorP)
                close(opensslSwitchP->fd);
        }
        if (*errorP)
            free(opensslSwitchP);
    }
}



void
ChanSwitchOpensslCreateFd(int            const fd,
                          TChanSwitch ** const chanSwitchPP,
                          const char **  const errorP) {
/*----------------------------------------------------------------------------
   Create an OpenSSL-based channel switch, based on a POSIX socket that
   is in listen state.
-----------------------------------------------------------------------------*/
    struct opensslSwitch * opensslSwitchP;

    if (connected(fd))
        xmlrpc_asprintf(errorP,
                        "Socket (file descriptor %d) is in connected "
                        "state.", fd);
    else {
        MALLOCVAR(opensslSwitchP);

        if (opensslSwitchP == NULL)
            xmlrpc_asprintf(errorP, "unable to allocate memory for Openssl "
                            "channel switch descriptor.");
        else {
            TChanSwitch * chanSwitchP;

            opensslSwitchP->fd = fd;
            opensslSwitchP->userSuppliedFd = TRUE;
            
            ChanSwitchCreate(&chanSwitchVtbl, opensslSwitchP, &chanSwitchP);

            if (chanSwitchP == NULL)
                xmlrpc_asprintf(errorP, "Unable to allocate memory for "
                                "channel switch descriptor");
            else {
                *chanSwitchPP = chanSwitchP;
                *errorP = NULL;
            }
            if (*errorP)
                free(opensslSwitchP);
        }
    }
}
