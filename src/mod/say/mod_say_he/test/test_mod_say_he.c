/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2026, Anthony Minessale II <anthm@freeswitch.org>
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
 * Avi Marcus <marcus@bestfone.com>
 *
 *
 * test_mod_say_he.c -- tests mod_say_he
 *
 * Each row is one say_string call and the files it must return.
 * The comment is the Hebrew those files say. The "and" files are shown as a prefix:
 * digits/va = וַ, digits/ve = וְ, digits/uu = וּ
 *
 */

#include <switch.h>
#include <test/switch_test.h>

typedef struct {
	const char *type;
	const char *method;
	const char *gender;
	const char *tosay;
	const char *expected;
} say_he_test_t;

static const say_he_test_t say_he_tests[] = {
	{"number", "pronounced", "masculine", "0", "file_string://digits/0.wav"}, /* אפס */
	{"number", "pronounced", "masculine", "1", "file_string://digits/1_m.wav"}, /* אחד */
	{"number", "pronounced", "masculine", "2", "file_string://digits/2_m.wav"}, /* שניים */
	{"number", "pronounced", "masculine", "10", "file_string://digits/10_m.wav"}, /* עשרה */
	{"number", "pronounced", "masculine", "11", "file_string://digits/11_m.wav"}, /* אחד עשר */
	{"number", "pronounced", "masculine", "15", "file_string://digits/15_m.wav"}, /* חמישה עשר */
	{"number", "pronounced", "masculine", "19", "file_string://digits/19_m.wav"}, /* תשעה עשר */
	{"number", "pronounced", "masculine", "20", "file_string://digits/20.wav"}, /* עשרים */
	{"number", "pronounced", "masculine", "21", "file_string://digits/20.wav!digits/ve.wav!digits/1_m.wav"}, /* עשרים וְאחד */
	{"number", "pronounced", "masculine", "22", "file_string://digits/20.wav!digits/uu.wav!digits/2_m.wav"}, /* עשרים וּשניים */
	{"number", "pronounced", "masculine", "25", "file_string://digits/20.wav!digits/va.wav!digits/5_m.wav"}, /* עשרים וַחמישה */
	{"number", "pronounced", "masculine", "27", "file_string://digits/20.wav!digits/ve.wav!digits/7_m.wav"}, /* עשרים וְשבעה */
	{"number", "pronounced", "masculine", "29", "file_string://digits/20.wav!digits/ve.wav!digits/9_m.wav"}, /* עשרים וְתשעה */
	{"number", "pronounced", "masculine", "38", "file_string://digits/30.wav!digits/uu.wav!digits/8_m.wav"}, /* שלושים וּשמונה */
	{"number", "pronounced", "masculine", "100", "file_string://digits/100.wav"}, /* מאה */
	{"number", "pronounced", "masculine", "105", "file_string://digits/100.wav!digits/va.wav!digits/5_m.wav"}, /* מאה וַחמישה */
	{"number", "pronounced", "masculine", "110", "file_string://digits/100.wav!digits/va.wav!digits/10_m.wav"}, /* מאה וַעשרה */
	{"number", "pronounced", "masculine", "115", "file_string://digits/100.wav!digits/va.wav!digits/15_m.wav"}, /* מאה וַחמישה עשר */
	{"number", "pronounced", "masculine", "119", "file_string://digits/100.wav!digits/ve.wav!digits/19_m.wav"}, /* מאה וְתשעה עשר */
	{"number", "pronounced", "masculine", "120", "file_string://digits/100.wav!digits/ve.wav!digits/20.wav"}, /* מאה וְעשרים */
	{"number", "pronounced", "masculine", "125", "file_string://digits/100.wav!digits/20.wav!digits/va.wav!digits/5_m.wav"}, /* מאה עשרים וַחמישה */
	{"number", "pronounced", "masculine", "250", "file_string://digits/200.wav!digits/va.wav!digits/50.wav"}, /* מאתיים וַחמישים */
	{"number", "pronounced", "masculine", "999", "file_string://digits/900.wav!digits/90.wav!digits/ve.wav!digits/9_m.wav"}, /* תשע מאות תשעים וְתשעה */
	{"number", "pronounced", "masculine", "1000", "file_string://digits/1000.wav"}, /* אלף */
	{"number", "pronounced", "masculine", "1001", "file_string://digits/1000.wav!digits/ve.wav!digits/1_m.wav"}, /* אלף וְאחד */
	{"number", "pronounced", "masculine", "1015", "file_string://digits/1000.wav!digits/va.wav!digits/15_m.wav"}, /* אלף וַחמישה עשר */
	{"number", "pronounced", "masculine", "1020", "file_string://digits/1000.wav!digits/ve.wav!digits/20.wav"}, /* אלף וְעשרים */
	{"number", "pronounced", "masculine", "1100", "file_string://digits/1000.wav!digits/uu.wav!digits/100.wav"}, /* אלף וּמאה */
	{"number", "pronounced", "masculine", "1234", "file_string://digits/1000.wav!digits/200.wav!digits/30.wav!digits/ve.wav!digits/4_m.wav"}, /* אלף מאתיים שלושים וְארבעה */
	{"number", "pronounced", "masculine", "2000", "file_string://digits/2000.wav"}, /* אלפיים */
	{"number", "pronounced", "masculine", "3000", "file_string://digits/3000.wav"}, /* שלושת אלפים */
	{"number", "pronounced", "masculine", "10000", "file_string://digits/10000.wav"}, /* עשרת אלפים */
	{"number", "pronounced", "masculine", "11000", "file_string://digits/11_m.wav!digits/thousand.wav"}, /* אחד עשר אלף */
	{"number", "pronounced", "masculine", "15000", "file_string://digits/15_m.wav!digits/thousand.wav"}, /* חמישה עשר אלף */
	{"number", "pronounced", "masculine", "15003", "file_string://digits/15_m.wav!digits/thousand.wav!digits/uu.wav!digits/3_m.wav"}, /* חמישה עשר אלף וּשלושה */
	{"number", "pronounced", "masculine", "20000", "file_string://digits/20.wav!digits/thousand.wav"}, /* עשרים אלף */
	{"number", "pronounced", "masculine", "25000", "file_string://digits/20.wav!digits/va.wav!digits/5_m.wav!digits/thousand.wav"}, /* עשרים וַחמישה אלף */
	{"number", "pronounced", "masculine", "100000", "file_string://digits/100.wav!digits/thousand.wav"}, /* מאה אלף */
	{"number", "pronounced", "masculine", "115000", "file_string://digits/100.wav!digits/va.wav!digits/15_m.wav!digits/thousand.wav"}, /* מאה וַחמישה עשר אלף */
	{"number", "pronounced", "masculine", "1000000", "file_string://digits/million.wav"}, /* מיליון */
	{"number", "pronounced", "masculine", "1000001", "file_string://digits/million.wav!digits/ve.wav!digits/1_m.wav"}, /* מיליון וְאחד */
	{"number", "pronounced", "masculine", "1015000", "file_string://digits/million.wav!digits/va.wav!digits/15_m.wav!digits/thousand.wav"}, /* מיליון וַחמישה עשר אלף */
	{"number", "pronounced", "masculine", "2000000", "file_string://digits/shney.wav!digits/million.wav"}, /* שני מיליון */
	{"number", "pronounced", "masculine", "22000000", "file_string://digits/20.wav!digits/uu.wav!digits/2_m.wav!digits/million.wav"}, /* עשרים וּשניים מיליון */
	{"number", "pronounced", "masculine", "2300000", "file_string://digits/shney.wav!digits/million.wav!digits/uu.wav!digits/300.wav!digits/thousand.wav"}, /* שני מיליון וּשלוש מאות אלף */
	{"number", "pronounced", "masculine", "1150", "file_string://digits/1000.wav!digits/100.wav!digits/va.wav!digits/50.wav"}, /* אלף מאה וַחמישים */
	{"number", "pronounced", "masculine", "1500", "file_string://digits/1000.wav!digits/va.wav!digits/500.wav"}, /* אלף וַחמש מאות */
	{"number", "pronounced", "masculine", "1400", "file_string://digits/1000.wav!digits/ve.wav!digits/400.wav"}, /* אלף וְארבע מאות */
	{"number", "pronounced", "masculine", "120000", "file_string://digits/100.wav!digits/ve.wav!digits/20.wav!digits/thousand.wav"}, /* מאה וְעשרים אלף */
	{"number", "pronounced", "masculine", "20005", "file_string://digits/20.wav!digits/thousand.wav!digits/va.wav!digits/5_m.wav"}, /* עשרים אלף וַחמישה */
	{"number", "pronounced", "masculine", "1000080", "file_string://digits/million.wav!digits/uu.wav!digits/80.wav"}, /* מיליון וּשמונים */
	{"number", "pronounced", "feminine", "1", "file_string://digits/1.wav"}, /* אחת */
	{"number", "pronounced", "feminine", "2", "file_string://digits/2.wav"}, /* שתיים */
	{"number", "pronounced", "feminine", "10", "file_string://digits/10.wav"}, /* עשר */
	{"number", "pronounced", "feminine", "15", "file_string://digits/15.wav"}, /* חמש עשרה */
	{"number", "pronounced", "feminine", "21", "file_string://digits/20.wav!digits/ve.wav!digits/1.wav"}, /* עשרים וְאחת */
	{"number", "pronounced", "feminine", "22", "file_string://digits/20.wav!digits/uu.wav!digits/2.wav"}, /* עשרים וּשתיים */
	{"number", "pronounced", "feminine", "23", "file_string://digits/20.wav!digits/ve.wav!digits/3.wav"}, /* עשרים וְשלוש */
	{"number", "pronounced", "feminine", "28", "file_string://digits/20.wav!digits/uu.wav!digits/8.wav"}, /* עשרים וּשמונה */
	{"number", "pronounced", "feminine", "112", "file_string://digits/100.wav!digits/uu.wav!digits/12.wav"}, /* מאה וּשתים עשרה */
	{"number", "pronounced", "feminine", "115", "file_string://digits/100.wav!digits/ve.wav!digits/15.wav"}, /* מאה וְחמש עשרה */
	{"number", "pronounced", "feminine", "1999", "file_string://digits/1000.wav!digits/900.wav!digits/90.wav!digits/ve.wav!digits/9.wav"}, /* אלף תשע מאות תשעים וְתשע */
	{"number", "pronounced", "feminine", "2019", "file_string://digits/2000.wav!digits/uu.wav!digits/19.wav"}, /* אלפיים וּתשע עשרה */
	{"number", "pronounced", "feminine", "2026", "file_string://digits/2000.wav!digits/20.wav!digits/ve.wav!digits/6.wav"}, /* אלפיים עשרים וְשש */
	{"number", "pronounced", "feminine", "3.05", "file_string://digits/3.wav!digits/dot.wav!digits/0.wav!digits/5.wav"}, /* שלוש נקודה אפס חמש */
	{"number", "pronounced", "feminine", "0.5", "file_string://digits/0.wav!digits/dot.wav!digits/5.wav"}, /* אפס נקודה חמש */
	{"number", "iterated", "feminine", "0527", "file_string://digits/0.wav!digits/5.wav!digits/2.wav!digits/7.wav"}, /* אפס חמש שתיים שבע */
	{"number", "counted", "masculine", "1", "file_string://digits/h-1_m.wav"}, /* ראשון */
	{"number", "counted", "masculine", "10", "file_string://digits/h-10_m.wav"}, /* עשירי */
	{"number", "counted", "feminine", "2", "file_string://digits/h-2.wav"}, /* שנייה */
	{"currency", "pronounced", "masculine", "0", "file_string://digits/0.wav!currency/shkalim.wav"}, /* אפס שקלים */
	{"currency", "pronounced", "masculine", "1", "file_string://currency/shekel.wav!digits/1_m.wav"}, /* שקל אחד */
	{"currency", "pronounced", "masculine", "2", "file_string://digits/shney.wav!currency/shkalim.wav"}, /* שני שקלים */
	{"currency", "pronounced", "masculine", "3", "file_string://digits/3_m.wav!currency/shkalim.wav"}, /* שלושה שקלים */
	{"currency", "pronounced", "masculine", "15", "file_string://digits/15_m.wav!currency/shkalim.wav"}, /* חמישה עשר שקלים */
	{"currency", "pronounced", "masculine", "22", "file_string://digits/20.wav!digits/uu.wav!digits/2_m.wav!currency/shkalim.wav"}, /* עשרים וּשניים שקלים */
	{"currency", "pronounced", "masculine", "1000", "file_string://digits/1000.wav!currency/shkalim.wav"}, /* אלף שקלים */
	{"currency", "pronounced", "masculine", "-5", "file_string://currency/negative.wav!digits/5_m.wav!currency/shkalim.wav"}, /* מינוס חמישה שקלים */
	{"currency", "pronounced", "masculine", "1.01", "file_string://currency/shekel.wav!digits/1_m.wav!digits/ve.wav!currency/agora.wav!digits/1.wav"}, /* שקל אחד וְאגורה אחת */
	{"currency", "pronounced", "masculine", "2.50", "file_string://digits/shney.wav!currency/shkalim.wav!digits/va.wav!digits/50.wav!currency/agorot.wav"}, /* שני שקלים וַחמישים אגורות */
	{"currency", "pronounced", "masculine", "3.02", "file_string://digits/3_m.wav!currency/shkalim.wav!digits/uu.wav!digits/shtey.wav!currency/agorot.wav"}, /* שלושה שקלים וּשתי אגורות */
	{"currency", "pronounced", "masculine", "3.1", "file_string://digits/3_m.wav!currency/shkalim.wav!digits/ve.wav!digits/10.wav!currency/agorot.wav"}, /* שלושה שקלים וְעשר אגורות */
	{"currency", "pronounced", "masculine", "3.15", "file_string://digits/3_m.wav!currency/shkalim.wav!digits/ve.wav!digits/15.wav!currency/agorot.wav"}, /* שלושה שקלים וְחמש עשרה אגורות */
	{"currency", "pronounced", "masculine", "3.30", "file_string://digits/3_m.wav!currency/shkalim.wav!digits/uu.wav!digits/30.wav!currency/agorot.wav"}, /* שלושה שקלים וּשלושים אגורות */
	{"currency", "pronounced", "masculine", "3.35", "file_string://digits/3_m.wav!currency/shkalim.wav!digits/uu.wav!digits/30.wav!digits/ve.wav!digits/5.wav!currency/agorot.wav"}, /* שלושה שקלים וּשלושים וְחמש אגורות */
	{"currency", "pronounced", "masculine", "3.57", "file_string://digits/3_m.wav!currency/shkalim.wav!digits/va.wav!digits/50.wav!digits/ve.wav!digits/7.wav!currency/agorot.wav"}, /* שלושה שקלים וַחמישים וְשבע אגורות */
	{"currency", "pronounced", "masculine", "3.85", "file_string://digits/3_m.wav!currency/shkalim.wav!digits/uu.wav!digits/80.wav!digits/ve.wav!digits/5.wav!currency/agorot.wav"}, /* שלושה שקלים וּשמונים וְחמש אגורות */
	{"currency", "pronounced", "masculine", "3.99", "file_string://digits/3_m.wav!currency/shkalim.wav!digits/ve.wav!digits/90.wav!digits/ve.wav!digits/9.wav!currency/agorot.wav"}, /* שלושה שקלים וְתשעים וְתשע אגורות */
	{"currency", "pronounced", "masculine", "3.55", "file_string://digits/3_m.wav!currency/shkalim.wav!digits/va.wav!digits/50.wav!digits/ve.wav!digits/5.wav!currency/agorot.wav"}, /* שלושה שקלים וַחמישים וְחמש אגורות */
	{"time_measurement", "pronounced", "feminine", "1:01:01", "file_string://time/hour.wav!digits/1.wav!time/minute.wav!digits/1.wav!time/second.wav!digits/1.wav"}, /* שעה אחת דקה אחת שנייה אחת */
	{"time_measurement", "pronounced", "feminine", "2:02:02", "file_string://digits/shtey.wav!time/hours.wav!digits/shtey.wav!time/minutes.wav!digits/shtey.wav!time/seconds.wav"}, /* שתי שעות שתי דקות שתי שניות */
	{"time_measurement", "pronounced", "feminine", "3:15:00", "file_string://digits/3.wav!time/hours.wav!digits/15.wav!time/minutes.wav!digits/0.wav!time/seconds.wav"}, /* שלוש שעות חמש עשרה דקות אפס שניות */
};

FST_CORE_BEGIN(".")
{
	FST_SUITE_BEGIN(test_mod_say_he)
	{
		FST_SETUP_BEGIN()
		{
			fst_requires_module("mod_say_he");
		}
		FST_SETUP_END()

		FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()

		FST_TEST_BEGIN(say_string)
		{
			size_t i;

			for (i = 0; i < sizeof(say_he_tests) / sizeof(say_he_tests[0]); i++) {
				const say_he_test_t *t = &say_he_tests[i];
				char *result = NULL;

				switch_ivr_say_string(NULL, "he", "wav", t->tosay, "he", t->type, t->method, t->gender, &result);
				if (!result || strcmp(result, t->expected)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "say_string he %s %s %s [%s]\n  expected: %s\n  got:      %s\n",
									  t->type, t->method, t->gender, t->tosay, t->expected, switch_str_nil(result));
				}
				fst_check_string_equals(result, t->expected);
				switch_safe_free(result);
			}
		}
		FST_TEST_END()
	}
	FST_SUITE_END()
}
FST_CORE_END()
