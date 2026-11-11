/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2026, Anthony Minessale II <anthm@freeswitch.org>
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
 * Dmitry Verenitsin <dmitry.verenitsin@signalwire.com>
 *
 *
 * switch_timer.c -- timer tests
 *
 */
#include <switch.h>
#include <test/switch_test.h>

static int module_loaded = 0;

FST_MINCORE_BEGIN("./conf")

FST_SUITE_BEGIN(switch_timer)

FST_SETUP_BEGIN()
{
	if (!module_loaded) {
		const char *err = NULL;
		switch_loadable_module_init(SWITCH_FALSE);
		switch_loadable_module_load_module("", "CORE_SOFTTIMER_MODULE", SWITCH_TRUE, &err);
		module_loaded = 1;
	}
}
FST_SETUP_END()

FST_TEARDOWN_BEGIN()
{
}
FST_TEARDOWN_END()

/* timer_check returns FALSE with non-zero diff when no tick has elapsed,
 * returns SUCCESS with diff=0 after sleeping past the interval */
FST_TEST_BEGIN(test_timer_check)
{
	switch_timer_t timer = { 0 };
	switch_status_t status;

	fst_requires(switch_core_timer_init(&timer, "soft", 20, 160, fst_pool) == SWITCH_STATUS_SUCCESS);

	/* immediately after init - no tick yet */
	status = switch_core_timer_check(&timer, SWITCH_FALSE);
	fst_check(status == SWITCH_STATUS_FALSE);
	fst_check(timer.diff != 0);

	/* sleep past the 20ms interval */
	switch_sleep(50000); /* 50ms */

	/* now the tick should be detected */
	status = switch_core_timer_check(&timer, SWITCH_FALSE);
	fst_check(status == SWITCH_STATUS_SUCCESS);
	fst_check(timer.diff == 0);

	switch_core_timer_destroy(&timer);
}
FST_TEST_END()

/* step=FALSE does not advance tick/samplecount, step=TRUE advances by exactly 1 tick */
FST_TEST_BEGIN(test_timer_check_step)
{
	switch_timer_t timer = { 0 };
	switch_size_t tick_before;
	uint32_t samplecount_before;
	switch_status_t status;

	fst_requires(switch_core_timer_init(&timer, "soft", 20, 160, fst_pool) == SWITCH_STATUS_SUCCESS);

	switch_sleep(50000); /* 50ms - let the timer tick */

	tick_before = timer.tick;
	samplecount_before = timer.samplecount;

	/* step=FALSE: reports ready but does not advance */
	status = switch_core_timer_check(&timer, SWITCH_FALSE);
	fst_check(status == SWITCH_STATUS_SUCCESS);
	fst_check(timer.tick == tick_before);
	fst_check(timer.samplecount == samplecount_before);

	/* step=TRUE: advances tick by 1, samplecount by samples */
	status = switch_core_timer_check(&timer, SWITCH_TRUE);
	fst_check(status == SWITCH_STATUS_SUCCESS);
	fst_check(timer.tick == tick_before + 1);
	fst_check(timer.samplecount == samplecount_before + 160);

	switch_core_timer_destroy(&timer);
}
FST_TEST_END()

FST_SUITE_END()

FST_MINCORE_END()
