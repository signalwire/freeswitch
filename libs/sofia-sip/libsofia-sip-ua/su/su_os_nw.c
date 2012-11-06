/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Contact: Pekka Pessi <pekka.pessi@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

/**@ingroup su_os_nw
 *
 * @CFILE su_os_nw.c
 * Implementation of OS-specific network events interface.
 *
 * @author Martti Mela <Martti.Mela@nokia.com>
 * @date Created: Fri Aug 11 07:30:04 2006 mela
 *
 */

#include "config.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#define SU_MSG_ARG_T    struct su_network_changed_s

#include "sofia-sip/su.h"
#include "sofia-sip/su_alloc.h"
#include "sofia-sip/su_wait.h"
#include "sofia-sip/su_debug.h"
#include "sofia-sip/su_os_nw.h"
#include "sofia-sip/su_debug.h"

#if defined(__APPLE_CC__)
# define SU_NW_CHANGE_PTHREAD 1
#endif

#if defined (SU_NW_CHANGE_PTHREAD)
# define SU_HAVE_NW_CHANGE 1
# include <pthread.h>
#endif

#if defined(__APPLE_CC__)
#include <AvailabilityMacros.h>
#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SCDynamicStore.h>
#include <SystemConfiguration/SCDynamicStoreKey.h>
#include <SystemConfiguration/SCSchemaDefinitions.h>
#endif /* __APPLE_CC__ */

struct su_network_changed_s {
  su_root_t                  *su_root;
  su_home_t                  *su_home;

#if defined (__APPLE_CC__)
  SCDynamicStoreRef           su_storeRef[1];
  CFRunLoopSourceRef          su_sourceRef[1];
#endif
#if defined (SU_NW_CHANGE_PTHREAD)
  pthread_t                   su_os_thread;
#endif

  su_network_changed_f       *su_network_changed_cb;
  su_network_changed_magic_t *su_network_changed_magic;
};

#if defined(__APPLE_CC__)
static void su_nw_changed_msg_recv(su_root_magic_t *rm,
				   su_msg_r msg,
				   su_network_changed_t *snc)
{
  su_network_changed_magic_t *magic = snc->su_network_changed_magic;

  assert(magic);

  /* SU_DEBUG_5(("su_nw_changed_msg_recv: entering.\n")); */

  snc->su_network_changed_cb(magic, snc->su_root);

  return;
}


void nw_changed_cb(SCDynamicStoreRef store,
		   CFArrayRef changedKeys,
		   void *info)
{
  su_network_changed_t *snc = (su_network_changed_t *) info;
  su_network_changed_t *snc2;
  su_msg_r rmsg = SU_MSG_R_INIT;

  SU_DEBUG_7(("nw_changed_cb: entering.\n" VA_NONE));

  if (su_msg_create(rmsg,
		    su_root_task(snc->su_root),
		    su_root_task(snc->su_root),
		    su_nw_changed_msg_recv,
		    sizeof *snc) == SU_FAILURE) {

    return;
  }

  snc2 = su_msg_data(rmsg); assert(snc2);
  snc2->su_root = snc->su_root;
  snc2->su_home = snc->su_home;
  memcpy(snc2->su_storeRef, snc->su_storeRef, sizeof(SCDynamicStoreRef));
  memcpy(snc2->su_sourceRef, snc->su_sourceRef, sizeof(CFRunLoopSourceRef));
  snc2->su_os_thread = snc->su_os_thread;
  snc2->su_network_changed_cb = snc->su_network_changed_cb;
  snc2->su_network_changed_magic = snc->su_network_changed_magic;

  if (su_msg_send(rmsg) == SU_FAILURE) {
    su_msg_destroy(rmsg);
    return;
  }

  return;
}

static OSStatus
CreateIPAddressListChangeCallbackSCF(SCDynamicStoreCallBack callback,
				     void *contextPtr,
				     SCDynamicStoreRef *storeRef,
				     CFRunLoopSourceRef *sourceRef)
    // Create a SCF dynamic store reference and a
    // corresponding CFRunLoop source.  If you add the
    // run loop source to your run loop then the supplied
    // callback function will be called when local IP
    // address list changes.
{
    OSStatus                err = 0;
    SCDynamicStoreContext   context = {0, NULL, NULL, NULL, NULL};
    SCDynamicStoreRef       ref;
    CFStringRef             pattern;
    CFArrayRef              patternList;
    CFRunLoopSourceRef      rls;

    assert(callback   != NULL);
    assert( storeRef  != NULL);
    assert(*storeRef  == NULL);
    assert( sourceRef != NULL);
    assert(*sourceRef == NULL);

    ref = NULL;
    pattern = NULL;
    patternList = NULL;
    rls = NULL;

    // Create a connection to the dynamic store, then create
    // a search pattern that finds all IPv4 entities.
    // The pattern is "State:/Network/Service/[^/]+/IPv4".

    context.info = contextPtr;
    ref = SCDynamicStoreCreate( NULL,
                                CFSTR("AddIPAddressListChangeCallbackSCF"),
                                callback,
                                &context);
    //err = MoreSCError(ref);
    if (err == noErr) {
        pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(
                                NULL,
                                kSCDynamicStoreDomainState,
                                kSCCompAnyRegex,
                                kSCEntNetIPv4);
        //err = MoreSCError(pattern);
    }

    // Create a pattern list containing just one pattern,
    // then tell SCF that we want to watch changes in keys
    // that match that pattern list, then create our run loop
    // source.

    if (err == noErr) {
        patternList = CFArrayCreate(NULL,
                                    (const void **) &pattern, 1,
                                    &kCFTypeArrayCallBacks);
        //err = CFQError(patternList);
    }
    if (err == noErr) {
      //err = MoreSCErrorBoolean(
                SCDynamicStoreSetNotificationKeys(
                    ref,
                    NULL,
                    patternList);
		//      );
    }
    if (err == noErr) {
        rls = SCDynamicStoreCreateRunLoopSource(NULL, ref, 0);
        //err = MoreSCError(rls);
    }

    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls,
		       kCFRunLoopDefaultMode);

    // Clean up.

    //CFQRelease(pattern);
    //CFQRelease(patternList);
    if (err != noErr) {
      //CFQRelease(ref);
        ref = NULL;
    }
    *storeRef = ref;
    *sourceRef = rls;

    assert( (err == noErr) == (*storeRef  != NULL) );
    assert( (err == noErr) == (*sourceRef != NULL) );

    return err;
}

static void *su_start_nw_os_thread(void *ptr)
{
  su_network_changed_t *snc = (su_network_changed_t *) ptr;

  assert(snc);

  CreateIPAddressListChangeCallbackSCF(nw_changed_cb,
				       (void *) snc,
				       snc->su_storeRef,
				       snc->su_sourceRef);

  CFRunLoopRun();

  return NULL;
}
#endif

/** Register a callback for the network change event.
 *
 * @since New in @VERSION_1_12_2.
 */
su_network_changed_t
*su_root_add_network_changed(su_home_t *home, su_root_t *root,
			     su_network_changed_f *network_changed_cb,
			     su_network_changed_magic_t *magic)
{
  su_network_changed_t *snc = NULL;

  assert(home && root && network_changed_cb && magic);

#if defined (SU_HAVE_NW_CHANGE)
  snc = su_zalloc(home, sizeof *snc);

  if (!snc)
    return NULL;

  snc->su_home = home;
  snc->su_root = root;
  snc->su_network_changed_cb = network_changed_cb;
  snc->su_network_changed_magic = magic;

# if defined (SU_NW_CHANGE_PTHREAD)
  if ((pthread_create(&(snc->su_os_thread), NULL,
		      su_start_nw_os_thread,
		      (void *) snc)) != 0) {
    return NULL;
  }
# endif
#endif
  return snc;
}

/** Remove a callback registered for the network change event.
 *
 * @since New in @VERSION_1_12_2.
 */
int su_root_remove_network_changed(su_network_changed_t *snc)
{
  return -1;
}
