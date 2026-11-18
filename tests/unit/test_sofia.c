/*
 * Copyright (C) 2018-2019, Signalwire, Inc. ALL RIGHTS RESERVED
 * test_sofia.c -- Tests mod_sofia for memory leaks
 */

#include <switch.h>
#include <test/switch_test.h>

FST_CORE_DB_BEGIN("conf_sofia")
{
	FST_SUITE_BEGIN(switch_sofia)
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

		FST_TEST_BEGIN(sofia_leaks)
		{
		}
		FST_TEST_END()
	}
	FST_SUITE_END()
}
FST_CORE_END()
