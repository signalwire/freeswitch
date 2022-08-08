/*
* FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
* Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
*
* Version: MPL 1.1
*
* The contents of this file are subject to the Mozilla Public License Version
* 1.1 (the "License"); you may not use this file except in compliance with
* the License. You may obtain a copy of the License at
* http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
*
* The Initial Developer of the Original Code is
* Anthony Minessale II <anthm@freeswitch.org>
* Portions created by the Initial Developer are Copyright (C)
* the Initial Developer. All Rights Reserved.
*
* Based on mod_skel by
* Anthony Minessale II <anthm@freeswitch.org>
*
* Contributor(s):
*
* Paul Mateer <paul.mateer@jci.com>
*
* mod_event_socket_hash.c -- Performs authentication hashing for the event socket
*
*/
#ifndef MOD_EVENT_SOCKET_HASH_H
#define MOD_EVENT_SOCKET_HASH_H

switch_bool_t validate_hash(const char *auth_hash);
switch_bool_t validate_password(const char *auth_hash, const char *password);
const char *create_auth_hash(const char * password);

#endif /* MOD_EVENT_SOCKET_HASH_H */
