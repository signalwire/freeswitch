/*
* mod_mariadb for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
* Copyright (C) 2019, Andrey Volk <andywolk@gmail.com>
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
* The Original Code is ported from FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
*
* The Initial Developer of the Original Code is
* Anthony Minessale II <anthm@freeswitch.org>
* Portions created by the Initial Developer are Copyright (C)
* the Initial Developer. All Rights Reserved.
*
* Contributor(s):
* Andrey Volk <andywolk@gmail.com>
*
* mariadb_dsn.hpp -- Connection string parser header
*
*/

#ifndef _MARIADB_DSN_H_
#define _MARIADB_DSN_H_

#include <mysql.h>

#ifdef __cplusplus
extern "C" {
#endif

MYSQL* STDCALL mysql_dsn_connect(MYSQL *mysql, const char *connection_string, unsigned long clientflag);

#ifdef __cplusplus
}
#endif

#endif //_MARIADB_DSN_H_

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
