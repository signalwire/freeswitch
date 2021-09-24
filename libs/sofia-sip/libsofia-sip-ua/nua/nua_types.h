/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2008 Nokia Corporation.
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

#ifndef NUA_TYPES_H
/** Defined when <nua_types.h> has been included. */
#define NUA_TYPES_H

/**@internal @file nua_types.h
 * @brief Internal types for nua.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 */

typedef struct nua_handle_preferences nua_handle_preferences_t;
typedef struct nua_global_preferences nua_global_preferences_t;

#ifndef NUA_OWNER_T
#define NUA_OWNER_T struct nua_owner_s
#endif
typedef NUA_OWNER_T nua_owner_t;

typedef struct nua_dialog_state nua_dialog_state_t;
typedef struct nua_dialog_usage nua_dialog_usage_t;
typedef struct nua_server_request nua_server_request_t;
typedef struct nua_client_request nua_client_request_t;
typedef struct nua_dialog_peer_info nua_dialog_peer_info_t;

#ifndef NUA_SAVED_SIGNAL_T
#define NUA_SAVED_SIGNAL_T struct nua_saved_signal *
#endif

typedef NUA_SAVED_SIGNAL_T nua_saved_signal_t;

#endif
