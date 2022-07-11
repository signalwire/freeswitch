
/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2021, Anthony Minessale II <anthm@freeswitch.org>
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
 * Andrey Volk <andrey@signalwire.com>
 *
 *
 * test_nuafail.c - Checks if sofia-sip leaks on profile start fail
 *
 */

#include <switch.h>
#include <test/switch_test.h>

FST_CORE_EX_BEGIN("./conf-nuafail", SCF_VG | SCF_USE_SQL)
{
	FST_MODULE_BEGIN(mod_sofia, nuafail)
	{
		FST_SETUP_BEGIN()
		{
			fst_requires_module("mod_sofia");
		}
		FST_SETUP_END()

		FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()

		FST_TEST_BEGIN(do_nothing)
		{
		}
		FST_TEST_END()
	}
	FST_MODULE_END()
}
FST_CORE_END()
