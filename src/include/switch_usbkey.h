/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 * Eliot Gable <egable@gmail.com>
 *
 * switch_apr.h -- APR includes header
 *
 */
/*! \file switch_apr.h
    \brief APR includes header
	
	The things powered by APR are renamed into the switch_ namespace to provide a cleaner
	look to things and helps me to document what parts of APR I am using I'd like to take this
	opportunity to thank APR for all the awesome stuff it does and for making my life much easier.

*/
#ifndef SWITCH_USBKEY_H
#define SWITCH_USBKEY_H

#include <AuthManager.h>

#define RCS_KEY_ID RCS_KEY


SWITCH_BEGIN_EXTERN_C


SWITCH_DECLARE(BOOL) switch_StartAuthManagerEx(int nKeyID,char * szIPRErr, char * szLicSn, char * szLicPw, char *szLogDirectory, unsigned char ucLogLevel, unsigned char ucLogCreatePeriod);

SWITCH_DECLARE(BOOL) switch_CheckRCSKEY(int nKeyID);

SWITCH_DECLARE(BOOL) switch_GetCKMLicAuth();

SWITCH_DECLARE(BOOL) switch_CheckPassword(char * szLicPw);

SWITCH_DECLARE(void) switch_CloseAuthManager();
SWITCH_DECLARE(void) switch_RCSRunToCrash();

SWITCH_DECLARE(BOOL) switch_GetRCSTestAuth();

SWITCH_DECLARE(DWORD) switch_GetUsedPeriod(int nKeyID);
SWITCH_DECLARE(BOOL) switch_WriteUsedPeriod(int nKeyID, DWORD deUsedPeriod);
SWITCH_DECLARE(DWORD) switch_GetValidPeriod(int nKeyID);
SWITCH_DECLARE(DWORD) switch_GetExpAliveTime(int nKeyID);

SWITCH_DECLARE(DWORD) switch_GetUSBKeySerial(int nKeyID);

SWITCH_DECLARE(DWORD) switch_GetAuthChNum(int nKeyID, int* pnNormalChNum, int* pnExChNum, BOOL bRetryIfFail);
SWITCH_DECLARE(DWORD) switch_GetAuthExtNum(int nKeyID);
SWITCH_DECLARE(DWORD) switch_GetAuthApiNum(int nKeyID);

SWITCH_DECLARE(u64) switch_GetAuthModules(int nKeyID);
SWITCH_DECLARE(u64) switch_GetAuthFeatures(int nKeyID);
/** @} */

SWITCH_END_EXTERN_C
#endif
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

