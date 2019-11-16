/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2019, Anthony Minessale II <anthm@freeswitch.org>
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
 * Chris Rienzo <chris@signalwire.com>
 * Seven Du <dujinfang@gmail.com>
 *
 *
 * switch_xml.c -- tests core xml functions
 *
 */
#include <switch.h>
#include <stdlib.h>

#include <test/switch_test.h>

FST_MINCORE_BEGIN()
{
	FST_SUITE_BEGIN(switch_xml)
	{
		FST_SETUP_BEGIN()
		{
		}
		FST_SETUP_END()

		FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()

		FST_TEST_BEGIN(test_cdata)
		{
			const char *text = "<xml><![CDATA[Tom & Jerry]]></xml>";
			switch_xml_t xml = switch_xml_parse_str_dynamic((char *)text, SWITCH_TRUE);

			fst_requires(xml);
			fst_check(xml->flags & SWITCH_XML_CDATA);
			switch_xml_free(xml);

			text = "<xml><tag><![CDATA[Tom & Jerry]]></tag></xml>";

			xml = switch_xml_parse_str_dynamic((char *)text, SWITCH_TRUE);
			fst_requires(xml);
			fst_check((xml->flags & SWITCH_XML_CDATA) == 0);
			xml = xml->child;
			fst_check_string_equals(xml->name, "tag");
			fst_check(xml->flags & SWITCH_XML_CDATA);
			switch_xml_free(xml);
		}
		FST_TEST_END()
	}
	FST_SUITE_END()
}
FST_MINCORE_END()
