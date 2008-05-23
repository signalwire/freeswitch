#ifndef CONN_H_INCLUDED
#define CONN_H_INCLUDED

#include "bool.h"
#include "xmlrpc-c/abyss.h"
#include "thread.h"

struct TFile;

#define BUFFER_SIZE 4096 

struct _TConn {
    struct _TConn * nextOutstandingP;
        /* Link to the next connection in the list of outstanding
           connections
        */
    TServer * server;
    uint32_t buffersize;
        /* Index into the connection buffer (buffer[], below) where
           the next byte read on the connection will go.
        */
    uint32_t bufferpos;
        /* Index into the connection buffer (buffer[], below) where
           the next byte to be delivered to the user is.
        */
    uint32_t inbytes,outbytes;  
    TChannel * channelP;
    void * channelInfoP;
        /* Information about the channel, such as who is on the other end.
           Format depends on the type of channel.  The user of the connection
           is expected to know that type, because he supplied the channel
           when he created the channel.

           NULL means no channel info is available.
        */
    bool hasOwnThread;
    TThread * threadP;
    bool finished;
        /* We have done all the processing there is to do on this
           connection, other than possibly notifying someone that we're
           done.  One thing this signifies is that any thread or process
           that the connection spawned is dead or will be dead soon, so
           one could reasonably wait for it to be dead, e.g. with
           pthread_join().  Note that one can scan a bunch of processes
           for 'finished' status, but sometimes can't scan a bunch of
           threads for liveness.
        */
    const char * trace;
    TThreadProc * job;
    TThreadDoneFn * done;
    char buffer[BUFFER_SIZE];
};

typedef struct _TConn TConn;

TConn * ConnAlloc(void);

void ConnFree(TConn * const connectionP);

void
ConnCreate(TConn **            const connectionPP,
           TServer *           const serverP,
           TChannel *          const channelP,
           void *              const channelInfoP,
           TThreadProc *       const job,
           TThreadDoneFn *     const done,
           enum abyss_foreback const foregroundBackground,
           bool                const useSigchld,
           const char **       const errorP);

bool
ConnProcess(TConn * const connectionP);

bool
ConnKill(TConn * const connectionP);

void
ConnWaitAndRelease(TConn * const connectionP);

bool
ConnWrite(TConn *      const connectionP,
          const void * const buffer,
          uint32_t     const size);

bool
ConnRead(TConn *  const c,
         uint32_t const timems);

void
ConnReadInit(TConn * const connectionP);

bool
ConnWriteFromFile(TConn *              const connectionP,
                  const struct TFile * const fileP,
                  uint64_t             const start,
                  uint64_t             const last,
                  void *               const buffer,
                  uint32_t             const buffersize,
                  uint32_t             const rate);

TServer *
ConnServer(TConn * const connectionP);

void
ConnFormatClientAddr(TConn *       const connectionP,
                     const char ** const clientAddrPP);

#endif
