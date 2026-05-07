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

FST_MINCORE_BEGIN("./conf")
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
			fst_check_string_equals(xml->child->name, "tag");
			fst_check(xml->child->flags & SWITCH_XML_CDATA);
			switch_xml_free(xml);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_utf_8)
		{
			const char *text = "<xml>Voulez-Vous Parler Français</xml>";
			switch_xml_t xml = switch_xml_parse_str_dynamic((char *)text, SWITCH_TRUE);
			char *xml_string = NULL;

			fst_requires(xml);
			xml_string = switch_xml_toxml(xml, SWITCH_FALSE);
			fst_requires(xml_string);
			fst_check_string_equals(xml_string, "<xml>Voulez-Vous Parler Fran&#xE7;ais</xml>\n");
			free(xml_string);

			xml_string = switch_xml_toxml_ex(xml, SWITCH_FALSE, SWITCH_FALSE);
			fst_requires(xml_string);
			fst_check_string_equals(xml_string, "<xml>Voulez-Vous Parler Français</xml>\n");
			switch_xml_free(xml);
			free(xml_string);

			text = "<xml>你好，中文</xml>";
			xml = switch_xml_parse_str_dynamic((char *)text, SWITCH_TRUE);

			fst_requires(xml);
			xml_string = switch_xml_toxml(xml, SWITCH_FALSE);
			fst_requires(xml_string);
			fst_check_string_equals(xml_string, "<xml>&#x4F60;&#x597D;&#xFF0C;&#x4E2D;&#x6587;</xml>\n");
			free(xml_string);

			xml_string = switch_xml_toxml_ex(xml, SWITCH_FALSE, SWITCH_FALSE);
			fst_requires(xml_string);
			fst_check_string_equals(xml_string, "<xml>你好，中文</xml>\n");
			switch_xml_free(xml);
			free(xml_string);

			text = "<xml><tag><![CDATA[Voulez-Vous Parler Français]]></tag></xml>";

			xml = switch_xml_parse_str_dynamic((char *)text, SWITCH_TRUE);
			fst_requires(xml);
			xml_string = switch_xml_toxml(xml, SWITCH_FALSE);
			fst_requires(xml_string);
			fst_check_string_equals(xml_string, "<xml>\n  <tag>Voulez-Vous Parler Fran&#xE7;ais</tag>\n</xml>\n");
			free(xml_string);

			xml_string = switch_xml_toxml_ex(xml, SWITCH_FALSE, SWITCH_FALSE);
			fst_requires(xml_string);
			fst_check_string_equals(xml_string, "<xml>\n  <tag>Voulez-Vous Parler Français</tag>\n</xml>\n");
			switch_xml_free(xml);
			free(xml_string);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_dtd)
		{
			const char *text = "<xml><!DOCTYPE Response [<!ENTITY lol \"haha\"><!ENTITY lol1 \"&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;\">]><Response><Say>&lol1;</Say></Response></xml>";
			switch_xml_t xml = switch_xml_parse_str_dynamic((char *)text, SWITCH_TRUE);
			char *xml_string = NULL;

			fst_requires(xml);
			xml_string = switch_xml_toxml_ex(xml, SWITCH_FALSE, SWITCH_FALSE);
			fst_requires(xml_string);
			fst_check_string_equals(xml_string, "<xml>\n  <Response>\n    <Say>hahahahahahahahahahahahahahahahahahahaha</Say>\n  </Response>\n</xml>\n");
			free(xml_string);
			switch_xml_free(xml);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_dtd_disable)
		{
			const char *text = "<xml><!DOCTYPE Response [<!ENTITY lol \"haha\"><!ENTITY lol1 \"&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;\">]><Response><Say>&lol1;</Say></Response></xml>";
			switch_xml_t xml = NULL;
			char *xml_string = NULL;

			switch_core_set_variable("xml_disable_dtd", "true");
			xml = switch_xml_parse_str_dynamic((char *)text, SWITCH_TRUE);
			fst_requires(xml);
			xml_string = switch_xml_toxml_ex(xml, SWITCH_FALSE, SWITCH_FALSE);
			fst_requires(xml_string);
			fst_check_string_equals(xml_string, "<xml>\n  <Response>\n    <Say>&amp;lol1;</Say>\n  </Response>\n</xml>\n");
			free(xml_string);
			switch_xml_free(xml);
			switch_core_set_variable("xml_disable_dtd", "false");
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_dtd_with_comments)
		{
			const char *text = "<xml><!DOCTYPE Response [<!--COMMENT1--><!ENTITY lol \"haha\"><!ENTITY lol1 \"&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;\"><!--COMMENT2-->]><Response><Say>&lol1;</Say></Response></xml>";
			switch_xml_t xml = NULL;
			char *xml_string = NULL;

			xml = switch_xml_parse_str_dynamic((char *)text, SWITCH_TRUE);
			fst_requires(xml);
			xml_string = switch_xml_toxml_ex(xml, SWITCH_FALSE, SWITCH_FALSE);
			fst_requires(xml_string);
			fst_check_string_equals(xml_string, "<xml>\n  <Response>\n    <Say>hahahahahahahahahahahahahahahahahahahaha</Say>\n  </Response>\n</xml>\n");
			free(xml_string);
			switch_xml_free(xml);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_dtd_disable_with_comments)
		{
			const char *text = "<xml><!DOCTYPE Response [<!--COMMENT1--><!ENTITY lol \"haha\"><!ENTITY lol1 \"&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;\"><!--COMMENT2-->]><Response><Say>&lol1;</Say></Response></xml>";
			switch_xml_t xml = NULL;
			char *xml_string = NULL;

			switch_core_set_variable("xml_disable_dtd", "true");
			xml = switch_xml_parse_str_dynamic((char *)text, SWITCH_TRUE);
			fst_requires(xml);
			xml_string = switch_xml_toxml_ex(xml, SWITCH_FALSE, SWITCH_FALSE);
			fst_requires(xml_string);
			fst_check_string_equals(xml_string, "<xml>\n  <Response>\n    <Say>&amp;lol1;</Say>\n  </Response>\n</xml>\n");
			free(xml_string);
			switch_xml_free(xml);
			switch_core_set_variable("xml_disable_dtd", "false");
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_exponential_entity_expansion)
		{
			/* Test handling of exponentially nested entity definitions
			 * Each entity references the previous one 10 times, creating
			 * 10^10 total references which would consume excessive memory
			 * if fully expanded. Parser should enforce expansion limits.
			 */
			const char *nested_entities =
				"<?xml version=\"1.0\"?>\n"
				"<!DOCTYPE lolz [\n"
				"<!ENTITY lol \"lol\">\n"
				"<!ELEMENT lolz (#PCDATA)>\n"
				"<!ENTITY lol1 \"&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;\">\n"
				"<!ENTITY lol2 \"&lol1;&lol1;&lol1;&lol1;&lol1;&lol1;&lol1;&lol1;&lol1;&lol1;\">\n"
				"<!ENTITY lol3 \"&lol2;&lol2;&lol2;&lol2;&lol2;&lol2;&lol2;&lol2;&lol2;&lol2;\">\n"
				"<!ENTITY lol4 \"&lol3;&lol3;&lol3;&lol3;&lol3;&lol3;&lol3;&lol3;&lol3;&lol3;\">\n"
				"<!ENTITY lol5 \"&lol4;&lol4;&lol4;&lol4;&lol4;&lol4;&lol4;&lol4;&lol4;&lol4;\">\n"
				"<!ENTITY lol6 \"&lol5;&lol5;&lol5;&lol5;&lol5;&lol5;&lol5;&lol5;&lol5;&lol5;\">\n"
				"<!ENTITY lol7 \"&lol6;&lol6;&lol6;&lol6;&lol6;&lol6;&lol6;&lol6;&lol6;&lol6;\">\n"
				"<!ENTITY lol8 \"&lol7;&lol7;&lol7;&lol7;&lol7;&lol7;&lol7;&lol7;&lol7;&lol7;\">\n"
				"<!ENTITY lol9 \"&lol8;&lol8;&lol8;&lol8;&lol8;&lol8;&lol8;&lol8;&lol8;&lol8;\">\n"
				"<!ENTITY lol10 \"&lol9;&lol9;&lol9;&lol9;&lol9;&lol9;&lol9;&lol9;&lol9;&lol9;\">\n"
				"]>\n"
				"<lolz>&lol10;</lolz>";

			switch_xml_t xml = switch_xml_parse_str_dynamic((char *)nested_entities, SWITCH_TRUE);

			if (xml) {
				const char *error = switch_xml_error(xml);
				if (error && *error) {
					/* Parser enforced expansion limits */
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
						"Parser correctly enforced entity expansion limits: %s\n", error);
					switch_xml_free(xml);
				} else {
					/* Parser did not enforce limits */
					switch_xml_free(xml);
					fst_fail("Parser did not enforce entity expansion limits");
				}
			} else {
				/* Parser returned NULL */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
					"Parser rejected excessive entity expansion\n");
			}
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_entity_expansion_limit)
		{
			/* Test that reasonable entity usage still works */
			const char *safe_entities =
				"<?xml version=\"1.0\"?>\n"
				"<!DOCTYPE test [\n"
				"<!ENTITY company \"FreeSWITCH\">\n"
				"<!ENTITY product \"&company; Media Server\">\n"
				"]>\n"
				"<test>&product;</test>";

			switch_xml_t xml = switch_xml_parse_str_dynamic((char *)safe_entities, SWITCH_TRUE);

			fst_requires(xml);
			fst_check_string_equals(xml->txt, "FreeSWITCH Media Server");
			switch_xml_free(xml);
		}
		FST_TEST_END()
	}
	FST_SUITE_END()
}
FST_MINCORE_END()
