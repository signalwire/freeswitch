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

#ifndef SU_OS_NW_H
/** Defined when <sofia-sip/su_os_nw.h> has been included. */
#define SU_OS_NW_H

/**@ingroup su_os_nw
 * @file sofia-sip/su_os_nw.h Network change events.
 *
 * @author Martti Mela <Martti.Mela@nokia.com>
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Aug 15 09:05:18 EEST 2006  martti.mela@nokia.com
 *
 * @since Experimental in @VERSION_1_12_2.
 * Note that this is expected to change in future releases.
 */

/* ---------------------------------------------------------------------- */
/* Includes */

#ifndef SU_H
#include "sofia-sip/su.h"
#endif

#ifndef SU_TIME_H
#include "sofia-sip/su_time.h"
#endif

#ifndef SU_ALLOC_H
#include "sofia-sip/su_alloc.h"
#endif

#ifndef SU_WAIT_H
#include "sofia-sip/su_wait.h"
#endif

#if SU_HAVE_POLL
#include <sys/poll.h>
#endif

SOFIA_BEGIN_DECLS

/* ---------------------------------------------------------------------- */
/* network-changed callback */

#ifndef SU_NETWORK_CHANGED_MAGIC_T
/**Default type of application context for network_changed function.
 *
 * Application may define the typedef ::su_network_changed_magic_t to appropriate type
 * by defining macro #SU_NETWORK_CHANGED_MAGIC_T before including <sofia-sip/su_os_nw.h>, for
 * example,
 * @code
 * #define SU_NETWORK_CHANGED_MAGIC_T struct context
 * #include <sofia-sip/su_os_nw.h>
 * @endcode
 *
 * @since New in @VERSION_1_12_2.
 */
#define SU_NETWORK_CHANGED_MAGIC_T void
#endif

/** <a href="#su_root_t">Root context</a> pointer type.
 *
 * Application may define the typedef ::su_network_changed_magic_t to appropriate type
 * by defining macro #SU_NETWORK_CHANGED_MAGIC_T before including <sofia-sip/su_os_nw.h>, for
 * example,
 * @code
 * #define SU_NETWORK_CHANGED_MAGIC_T struct context
 * #include <sofia-sip/su_os_nw.h>
 * @endcode
 *
 * @since New in @VERSION_1_12_2.
 */
typedef SU_NETWORK_CHANGED_MAGIC_T su_network_changed_magic_t;

typedef struct su_network_changed_s su_network_changed_t;

/** Function prototype for network-changed callback .
 *
 *
 * @since New in @VERSION_1_12_2.
 */
typedef void (su_network_changed_f)(su_network_changed_magic_t *, su_root_t *);


/* ---------------------------------------------------------------------- */
/* Functions */

/* Network-changed */
SOFIAPUBFUN su_network_changed_t *
su_root_add_network_changed(su_home_t *home,
			    su_root_t *root,
			    su_network_changed_f *,
			    su_network_changed_magic_t *);
SOFIAPUBFUN int su_root_remove_network_changed(su_network_changed_t *);

SOFIA_END_DECLS

#endif /* SU_OS_NW_H */
