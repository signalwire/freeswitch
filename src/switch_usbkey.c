/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2015, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/F
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Michael Jerris <mike@jerris.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Michael Jerris <mike@jerris.com>
 * Eliot Gable <egable@gmail.com>
 * William King <william.king@quentustech.com>
 *
 * switch_apr.c -- apr wrappers and extensions
 *
 */

#include <switch.h>
#ifndef WIN32
#include <switch_private.h>
#endif
#include "private/switch_core_pvt.h"

#ifdef ENABLE_SSOFT
/* usbkey headers*/
#include <switch_usbkey.h>

SWITCH_DECLARE(BOOL) switch_StartAuthManagerEx(int nKeyID,char * szIPRErr, char * szLicSn, char * szLicPw, char *szLogDirectory, unsigned char ucLogLevel, unsigned char ucLogCreatePeriod)
{
	return StartAuthManagerEx(nKeyID, szIPRErr, szLicSn, szLicPw, szLogDirectory, ucLogLevel, ucLogCreatePeriod);
}
SWITCH_DECLARE(BOOL) switch_CheckRCSKEY(int nKeyID)
{
	return ChecKEY(nKeyID);
}
SWITCH_DECLARE(void) switch_CloseAuthManager()
{
	CloseAuthManager();
	return;
}

SWITCH_DECLARE(BOOL) switch_GetCKMLicAuth()
{
	return GetCKMLicAuth();
}

SWITCH_DECLARE(DWORD) switch_GetUSBKeySerial(int nKeyID)
{
	return GetUSBKeySerial(nKeyID);
}

SWITCH_DECLARE(void) switch_RCSRunToCrash()
{
	return RCSRunToCrash();
}

SWITCH_DECLARE(BOOL) switch_GetRCSTestAuth()
{
	return GetRCSTestAuth();
}

SWITCH_DECLARE(BOOL) switch_CheckPassword(char * szLicPw)
{
	return CheckPassword(szLicPw);
}

SWITCH_DECLARE(BOOL) switch_WriteUsedPeriod(int nKeyID, DWORD deUsedPeriod)
{
	return WriteUsedPeriod(nKeyID, deUsedPeriod);
}

SWITCH_DECLARE(DWORD) switch_GetUsedPeriod(int nKeyID)
{
	return GetUsedPeriod(nKeyID);
}

SWITCH_DECLARE(DWORD) switch_GetValidPeriod(int nKeyID)
{
	return GetValidPeriod(nKeyID);
}

SWITCH_DECLARE(DWORD) switch_GetExpAliveTime(int nKeyID)
{
	return GetExpAliveTime(nKeyID);
}

SWITCH_DECLARE(DWORD) switch_GetAuthChNum(int nKeyID, int* pnNormalChNum, int* pnExChNum, BOOL bRetryIfFail)
{
	return GetAuthChNum(nKeyID, pnNormalChNum, pnExChNum, bRetryIfFail);
}

SWITCH_DECLARE(DWORD) switch_GetAuthExtNum(int nKeyID)
{
	return GetAuthExtNum(nKeyID);
}

SWITCH_DECLARE(DWORD) switch_GetAuthApiNum(int nKeyID)
{
	return GetAuthApiNum(nKeyID);
}

SWITCH_DECLARE(u64) switch_GetAuthModules(int nKeyID)
{
	return GetAuthModules(nKeyID);
}
SWITCH_DECLARE(u64) switch_GetAuthFeatures(int nKeyID)
{
	return GetAuthFeatures(nKeyID);
}
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
