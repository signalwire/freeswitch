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

/**@CFILE tport_tag.c
 * @brief Tags for transport module
 *
 * @note This file is used to automatically generate 
 * tport_tag_ref.c and tport_tag_dll.c
 *
 * Copyright (c) 2002 Nokia Research Center.  All rights reserved.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Jun  6 00:38:07 2002 ppessi
 */

#include "config.h"

#include <string.h>
#include <assert.h>

#define TAG_NAMESPACE "tp"

#include "sofia-sip/tport.h"
#include <sofia-sip/su_tag_class.h>

/* ==== Globals ========================================================== */

/** Filter for tport tags. */
tagi_t tport_tags[] = { { tptag_any, 0 }, { TAG_END() } };

tag_typedef_t tptag_any = NSTAG_TYPEDEF(*);

tag_typedef_t tptag_ident = CSTRTAG_TYPEDEF(ident);
tag_typedef_t tptag_reuse = BOOLTAG_TYPEDEF(reuse);
tag_typedef_t tptag_fresh = BOOLTAG_TYPEDEF(fresh);
tag_typedef_t tptag_server = BOOLTAG_TYPEDEF(server);
tag_typedef_t tptag_public = INTTAG_TYPEDEF(public);
tag_typedef_t tptag_mtu = USIZETAG_TYPEDEF(mtu);
tag_typedef_t tptag_connect = BOOLTAG_TYPEDEF(connect);
tag_typedef_t tptag_sdwn_error = BOOLTAG_TYPEDEF(sdwn_error);
tag_typedef_t tptag_sdwn_after = BOOLTAG_TYPEDEF(sdwn_after);
tag_typedef_t tptag_close_after = BOOLTAG_TYPEDEF(sdwn_after);
tag_typedef_t tptag_idle = UINTTAG_TYPEDEF(idle);
tag_typedef_t tptag_timeout = UINTTAG_TYPEDEF(timeout);
tag_typedef_t tptag_sigcomp_lifetime = UINTTAG_TYPEDEF(sigcomp_lifetime);
tag_typedef_t tptag_certificate = STRTAG_TYPEDEF(certificate);
tag_typedef_t tptag_compartment = PTRTAG_TYPEDEF(compartment);

tag_typedef_t tptag_tls_version = UINTTAG_TYPEDEF(tls_version);
tag_typedef_t tptag_queuesize = UINTTAG_TYPEDEF(queuesize);
tag_typedef_t tptag_debug_drop = UINTTAG_TYPEDEF(debug_drop);
tag_typedef_t tptag_udp_rmem = UINTTAG_TYPEDEF(udp_rmem);
tag_typedef_t tptag_udp_wmem = UINTTAG_TYPEDEF(udp_wmem);
tag_typedef_t tptag_thrpsize = UINTTAG_TYPEDEF(thrpsize);
tag_typedef_t tptag_thrprqsize = UINTTAG_TYPEDEF(thrprqsize);
tag_typedef_t tptag_http_connect = STRTAG_TYPEDEF(http_connect);
tag_typedef_t tptag_stun_server = BOOLTAG_TYPEDEF(stun_server);
tag_typedef_t tptag_tos = INTTAG_TYPEDEF(tos);
tag_typedef_t tptag_log = INTTAG_TYPEDEF(log);
tag_typedef_t tptag_dump = STRTAG_TYPEDEF(dump);

