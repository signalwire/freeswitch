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

/**@CFILE auth_tag.c
 * @brief Tags for authentication verification module for NTA servers.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Created: Wed Apr 11 15:14:03 2001 ppessi
 */

#include "config.h"

#define TAG_NAMESPACE "auth"

#include "sofia-sip/auth_module.h"

#include <sofia-sip/su_tag_class.h>
#include <sofia-sip/url_tag_class.h>

/**@def AUTHTAG_ANY()
 *
 * Filter tag matching any AUTHTAG_*().
 */
tag_typedef_t authtag_any = NSTAG_TYPEDEF(*);

/**@def AUTHTAG_MODULE()
 *
 * Pointer to an authentication server module (auth_mod_t).
 *
 * The tag item AUTHTAG_MODULE() contains pointer to an authentication server
 * module. It is used to pass an already initialized authentication module
 * to a server object (like web server or registrar object).
 */
tag_typedef_t authtag_module = PTRTAG_TYPEDEF(module);

/**@def AUTHTAG_METHOD()
 *
 * Name of the authentication scheme.
 *
 * The tag AUTHTAG_METHOD() specifies the authentication module and scheme
 * to be used by the auth_module. The name can specify a basic
 * authentication module, like "Digest" or "Basic", or an plugin module,
 * like "SGMF+Digest".
 *
 * @sa See <sofia-sip/auth_plugin.h> for plugin interface.
 */
tag_typedef_t authtag_method = STRTAG_TYPEDEF(method);

/**@def AUTHTAG_REALM()
 *
 * Authentication realm used by authentication server.
 *
 * The tag authtag_method specifies the authentication realm used by the @b
 * auth_module.  For servers, the domain name in the request URI is inserted
 * in the realm returned to the client if the realm string contains an
 * asterisk @c "*".  Only the first asterisk is replaced by request domain
 * name.
 *
 * @p Default Value
 * "*".
 */
tag_typedef_t authtag_realm = STRTAG_TYPEDEF(realm);

/**@def AUTHTAG_OPAQUE()
 *
 * Opaque data used by authentication server.
 *
 * The tag authtag_opaque is used to pass opaque data to the @b auth_module.
 * The opaque data will be included in all the challenges (however, the data
 * is prefixed with a "." and other opaque data used by the algorithms.
 *
 * @p Default Value
 * "".
 */
tag_typedef_t authtag_opaque = STRTAG_TYPEDEF(opaque);

/**@def AUTHTAG_DB()
 *
 * Name of authentication database used by authentication server.
 *
 * The tag AUTHTAG_DB() specifies the file name used to store the
 * authentication data. The file contains triplets as follows:
 *
 * @code
 * user:password:realm
 * @endcode
 *
 * @note
 * Currently, the passwords are stored as plaintext.
 */
tag_typedef_t authtag_db = STRTAG_TYPEDEF(db);

/**@def AUTHTAG_QOP()
 *
 * Quality-of-protection used by Digest authentication.
 *
 * The tag AUTHTAG_QOP() specifies the qop scheme to be used by the
 * digest authentication.
 */
tag_typedef_t authtag_qop = STRTAG_TYPEDEF(qop);

/**@def AUTHTAG_ALGORITHM()
 *
 * Authentication algorithm used by Digest authentication.
 *
 * The tag AUTHTAG_ALGORITHM() specifies the qop scheme to be used by the
 * digest authentication.
 */
tag_typedef_t authtag_algorithm = STRTAG_TYPEDEF(algorithm);

/**@def AUTHTAG_EXPIRES()
 *
 * Nonce expiration time for Digest authentication.
 *
 * The tag AUTHTAG_EXPIRES() specifies the time in seconds that a nonce is
 * considered valid. If 0, the nonce lifetime unbounded. The default time is
 * 3600 seconds.
 */
tag_typedef_t authtag_expires = UINTTAG_TYPEDEF(expires);

/**@def AUTHTAG_NEXT_EXPIRES()
 *
 * Next nonce expiration time for Digest authentication.
 *
 * The tag AUTHTAG_NEXT_EXPIRES() specifies the time in seconds that a
 * nextnonce sent in Authentication-Info header is considered valid. If 0,
 * the nonce lifetime is unbounded. The default time is 3600 seconds.
 */
tag_typedef_t authtag_next_expires = UINTTAG_TYPEDEF(next_expires);

/**@def AUTHTAG_MAX_NCOUNT()
 *
 * Max nonce count value.
 *
 * The tag AUTHTAG_MAX_NCOUNT() specifies the maximum number of times a
 * nonce should be used.
 *
 * @todo Count actual usages and don't trust "nc" parameter only.
 */
tag_typedef_t authtag_max_ncount = UINTTAG_TYPEDEF(max_ncount);

/**@def AUTHTAG_BLACKLIST()
 *
 * Blacklist time.
 *
 * The tag AUTHTAG_BLACKLIST() specifies the time the server delays its
 * response if it is given bad credentials or malformed nonce. The default
 * time is 5 seconds.
 *
 * @todo Implement delayed response.
 */
tag_typedef_t authtag_blacklist = UINTTAG_TYPEDEF(blacklist);

/**@def AUTHTAG_FORBIDDEN()
 *
 * Respond with 403 Forbidden.
 *
 * When given a true argument, the tag AUTHTAG_FORBIDDEN() specifies that the
 * server responds with 403 Forbidden (instead of 401/407) when it receives
 * bad credentials.
 */
tag_typedef_t authtag_forbidden = BOOLTAG_TYPEDEF(forbidden);

/**@def AUTHTAG_ANONYMOUS()
 *
 * Allow anonymous access.
 *
 * When given a true argument, the tag AUTHTAG_ANONYMOUS() allows
 * authentication module to accept the account "anonymous" with an empty
 * password. The auth_status_t::as_anonymous flag is set in auth_status_t
 * structure after anonymous authentication.
 */
tag_typedef_t authtag_anonymous = BOOLTAG_TYPEDEF(anonymous);

/**@def AUTHTAG_FAKE()
 *
 * Fake authentication process.
 *
 * When given a true argument, the tag AUTHTAG_FAKE() causes authentication
 * module to allow access with any password when the username is valid. The
 * auth_status_t::as_fake flag is set in auth_status_t structure after a
 * fake authentication.
 */
tag_typedef_t authtag_fake = BOOLTAG_TYPEDEF(fake);

/**@def AUTHTAG_REMOTE()
 *
 * Remote authenticator URL.
 *
 * The tag AUTHTAG_REMOTE() is used to specify URL for remote authenticator.
 * The meaning of the URL is specific to the authentication module. The
 * authentication module is selected by AUTHTAG_METHOD().
 */
tag_typedef_t authtag_remote = URLTAG_TYPEDEF(remote);

/**@def AUTHTAG_ALLOW()
 *
 * Comma-separated list of methods that are not challenged.
 *
 * The tag AUTHTAG_ALLOW() takes its argument a string containing a
 * comma-separated list of methods, for example,
 * @code
 * AUTHTAG_ALLOW("ACK, BYE, CANCEL").
 * @endcode
 *
 * The specified methods are not challenged by the authentication module.
 * For example, this may include SIP ACK method or SIP methods only used
 * within an already established dialog.
 */
tag_typedef_t authtag_allow = STRTAG_TYPEDEF(allow);

/**@def AUTHTAG_MASTER_KEY()
 *
 * Private master key for the authentication module.
 *
 * The tag AUTHTAG_MASTER_KEY() specifies a private master key that can be
 * used by the authentication module for various purposes (for instance,
 * validating that nonces are really generated by it).
 */
tag_typedef_t authtag_master_key = STRTAG_TYPEDEF(master_key);

/**@def AUTHTAG_CACHE_USERS()
 *
 * Time to cache user data.
 *
 * The tag AUTHTAG_CACHE_USERS() specifies how many seconds the user data is
 * cached locally. Default value is typically 30 minutes.
 */
tag_typedef_t authtag_cache_users = UINTTAG_TYPEDEF(cache_users);

/**@def AUTHTAG_CACHE_ERRORS()
 *
 * Time to cache errors.
 *
 * The tag AUTHTAG_CACHE_ERRORS() specifies the lifetime in seconds for
 * errors in the local authentication data cache. Note that the errors
 * generated locally (e.g., because of connectivity problem with
 * authentication server) have maximum lifetime of 2 minutes.
 */
tag_typedef_t authtag_cache_errors = UINTTAG_TYPEDEF(cache_errors);

