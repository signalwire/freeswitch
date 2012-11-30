#ifndef CHANSWITCH_H_INCLUDED
#define CHANSWITCH_H_INCLUDED

/*============================================================================
   This is the generic channel switch interface for Abyss.  It
   includes both the generic interface to a channel switch from above
   and the interface between generic channel switch code and a
   particular channel implementation (e.g. POSIX) below.

   A channel switch is what creates a channel -- it brokers a connection
   between an HTTP client and server.
============================================================================*/

#include "bool.h"
#include "int.h"
#include "xmlrpc-c/abyss.h"

typedef void SwitchDestroyImpl(TChanSwitch * const socketP);

typedef void SwitchListenImpl(TChanSwitch * const chanSwitchP,
                              uint32_t      const backlog,
                              const char ** const errorP);

typedef void SwitchAcceptImpl(TChanSwitch * const chanSwitchP,
                              TChannel **   const channelPP,
                              void **       const channelInfoP,
                              const char ** const errorP);

typedef void SwitchInterruptImpl(TChanSwitch * const chanSwitchP);

struct TChanSwitchVtbl {
    SwitchDestroyImpl   * destroy;
    SwitchListenImpl    * listen;
    SwitchAcceptImpl    * accept;
    SwitchInterruptImpl * interrupt;
};

struct _TChanSwitch {
    unsigned int           signature;
        /* With both background and foreground use of switches, and
           background being both fork and pthread, it is very easy to
           screw up switch lifetime and try to destroy twice.  We use
           this signature to help catch such bugs.
        */
    void *                 implP;
    struct TChanSwitchVtbl vtbl;
};

extern bool SwitchTraceIsActive;

void
ChanSwitchInit(const char ** const errorP);

void
ChanSwitchTerm(void);

void
ChanSwitchCreate(const struct TChanSwitchVtbl * const vtblP,
                 void *                         const implP,
                 TChanSwitch **                 const chanSwitchPP);

void
ChanSwitchListen(TChanSwitch * const chanSwitchP,
                 uint32_t      const backlog,
                 const char ** const errorP);

void
ChanSwitchAccept(TChanSwitch * const chanSwitchP,
                 TChannel **   const channelPP,
                 void **       const channelInfoPP,
                 const char ** const errorP);

void
ChanSwitchInterrupt(TChanSwitch * const chanSwitchP);

#endif


