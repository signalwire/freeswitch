

#include <switch.h>
#include <iksemel.h>
#include <test/switch_test.h>
#include <iks_helpers.h>

static const char *voxeo_grammar =
	"<iq id='8847' type='set' from='usera@192.168.1.10/voxeo3' to='e7632f74-8c55-11e2-84b0-e538fa88a1ef@192.168.1.10'><input xmlns='urn:xmpp:rayo:input:1' min-confidence='0.3' mode='DTMF' sensitivity='0.5'><grammar content-type='application/grammar+voxeo'><![CDATA[[1 DIGITS]]]></grammar></input></iq>";


static const char *repeating_bracket =
	"<iq id='8847' type='set' from='usera@192.168.1.10/voxeo3' to='e7632f74-8c55-11e2-84b0-e538fa88a1ef@192.168.1.10'><input xmlns='urn:xmpp:rayo:input:1' min-confidence='0.3' mode='DTMF' sensitivity='0.5'><grammar content-type='application/grammar+voxeo'><![CDATA[[1 DIGITS]>]]]]]]]]] ]] ]]></grammar></input></iq>";


static const char *normal_cdata =
	"<iq id='8847' type='set' from='usera@192.168.1.10/voxeo3' to='e7632f74-8c55-11e2-84b0-e538fa88a1ef@192.168.1.10'><input xmlns='urn:xmpp:rayo:input:1' min-confidence='0.3' mode='DTMF' sensitivity='0.5'><grammar content-type='application/grammar+voxeo'><![CDATA[1 DIGITS]]></grammar></input></iq>";


static const char *empty_cdata =
	"<iq id='8847' type='set' from='usera@192.168.1.10/voxeo3' to='e7632f74-8c55-11e2-84b0-e538fa88a1ef@192.168.1.10'><input xmlns='urn:xmpp:rayo:input:1' min-confidence='0.3' mode='DTMF' sensitivity='0.5'><grammar content-type='application/grammar+voxeo'><![CDATA[]]></grammar></input></iq>";

static const char *rayo_test_srgs =
	"<grammar xmlns=\"http://www.w3.org/2001/06/grammar\" root=\"MAINRULE\">\n"
	"  <rule id=\"MAINRULE\">\n"
	"    <one-of>\n"
	"      <item>\n"
	"        <item repeat=\"0-1\"> need a</item>\n"
	"        <item repeat=\"0-1\"> i need a</item>\n"
	"        <one-of>\n"
	"          <item> clue </item>\n"
	"        </one-of>\n"
	"        <tag> out.concept = \"clue\";</tag>\n"
	"      </item>\n"
	"      <item>\n"
	"        <item repeat=\"0-1\"> have an</item>\n"
	"        <item repeat=\"0-1\"> i have an</item>\n"
	"        <one-of>\n"
	"          <item> answer </item>\n"
	"        </one-of>\n"
	"        <tag> out.concept = \"answer\";</tag>\n"
	"      </item>\n"
	"    </one-of>\n"
	"  </rule>\n"
	"</grammar>";


#define MATCH 1
#define NO_MATCH 0


/**
 * main program
 */
FST_BEGIN()

FST_SUITE_BEGIN(iks)

FST_SETUP_BEGIN()
{
}
FST_SETUP_END()

FST_TEARDOWN_BEGIN()
{
}
FST_TEARDOWN_END()


FST_TEST_BEGIN(iks_cdata_bug)
{
	iks *iq = NULL;
	iks *input = NULL;
	iksparser *p = iks_dom_new(&iq);
	const char *cdata;
	fst_check(IKS_OK == iks_parse(p, voxeo_grammar, 0, 1));
	iks_parser_delete(p);
	fst_check((input = iks_find(iq, "input")));
	fst_check((cdata = iks_find_cdata(input, "grammar")));
	fst_check_string_equals("[1 DIGITS]", cdata);
	iks_delete(iq);
}
FST_TEST_END()

FST_TEST_BEGIN(repeating_bracket)
{
	iks *iq = NULL;
	iks *input = NULL;
	iksparser *p = iks_dom_new(&iq);
	const char *cdata;
	fst_check(IKS_OK == iks_parse(p, repeating_bracket, 0, 1));
	iks_parser_delete(p);
	fst_check((input = iks_find(iq, "input")));
	fst_check((cdata = iks_find_cdata(input, "grammar")));
	fst_check_string_equals("[1 DIGITS]>]]]]]]]]] ]] ", cdata);
	iks_delete(iq);
}
FST_TEST_END()

FST_TEST_BEGIN(normal_cdata)
{
	iks *iq = NULL;
	iks *input = NULL;
	iksparser *p = iks_dom_new(&iq);
	const char *cdata;
	fst_check(IKS_OK == iks_parse(p, normal_cdata, 0, 1));
	iks_parser_delete(p);
	fst_check((input = iks_find(iq, "input")));
	fst_check((cdata = iks_find_cdata(input, "grammar")));
	fst_check_string_equals("1 DIGITS", cdata);
	iks_delete(iq);
}
FST_TEST_END()

FST_TEST_BEGIN(empty_cdata)
{
	iks *iq = NULL;
	iks *input = NULL;
	iksparser *p = iks_dom_new(&iq);
	const char *cdata;
	fst_check(IKS_OK == iks_parse(p, empty_cdata, 0, 1));
	iks_parser_delete(p);
	fst_check((input = iks_find(iq, "input")));
	fst_check(NULL == (cdata = iks_find_cdata(input, "grammar")));
	iks_delete(iq);
}
FST_TEST_END()


FST_TEST_BEGIN(rayo_test_srgs)
{
	iks *grammar = NULL;
	iksparser *p = iks_dom_new(&grammar);
	fst_check(IKS_OK == iks_parse(p, rayo_test_srgs, 0, 1));
	iks_parser_delete(p);
	iks_delete(grammar);
}
FST_TEST_END()

FST_TEST_BEGIN(iks_helper_value_matches)
{
	fst_check(MATCH == value_matches("1", "1,2,3"));
	fst_check(MATCH == value_matches("2", "1,2,3"));
	fst_check(MATCH == value_matches("3", "1,2,3"));
	fst_check(NO_MATCH == value_matches("4", "1,2,3"));
	fst_check(NO_MATCH == value_matches("1,2", "1,2,3"));
	fst_check(NO_MATCH == value_matches(NULL, "1,2,3"));
	fst_check(NO_MATCH == value_matches(NULL, NULL));
	fst_check(NO_MATCH == value_matches("1", NULL));
	fst_check(NO_MATCH == value_matches("", "1,2,3"));
	fst_check(NO_MATCH == value_matches("", ""));
	fst_check(NO_MATCH == value_matches("1", ""));
	fst_check(MATCH == value_matches("duplex", "duplex,send,recv"));
	fst_check(MATCH == value_matches("send", "duplex,send,recv"));
	fst_check(MATCH == value_matches("recv", "duplex,send,recv"));
	fst_check(NO_MATCH == value_matches("sendrecv", "duplex,send,recv"));
	fst_check(MATCH == value_matches("duplex1", "duplex1,duplex2,duplex3"));
	fst_check(MATCH == value_matches("duplex2", "duplex1,duplex2,duplex3"));
	fst_check(MATCH == value_matches("duplex3", "duplex1,duplex2,duplex3"));
	fst_check(NO_MATCH == value_matches("duplex4", "duplex1,duplex2,duplex3"));
	fst_check(NO_MATCH == value_matches("duplex", "duplex1,duplex2,duplex3"));
}
FST_TEST_END()

FST_TEST_BEGIN(dialback_key)
{
	char *dialback_key;

	dialback_key = iks_server_dialback_key("s3cr3tf0rd14lb4ck", "xmpp.example.com", "example.org", "D60000229F");
	fst_check_string_equals("37c69b1cf07a3f67c04a5ef5902fa5114f2c76fe4a2686482ba5b89323075643", dialback_key);
	switch_safe_free(dialback_key);
	fst_check(NULL == (dialback_key = iks_server_dialback_key("", "xmpp.example.com", "example.org", "D60000229F")));
	switch_safe_free(dialback_key);
	fst_check(NULL == (dialback_key = iks_server_dialback_key("s3cr3tf0rd14lb4ck", "", "example.org", "D60000229F")));
	switch_safe_free(dialback_key);
	fst_check(NULL == (dialback_key = iks_server_dialback_key("s3cr3tf0rd14lb4ck", "xmpp.example.com", "", "D60000229F")));
	switch_safe_free(dialback_key);
	fst_check(NULL == (dialback_key = iks_server_dialback_key("s3cr3tf0rd14lb4ck", "xmpp.example.com", "example.org", "")));
	switch_safe_free(dialback_key);
	fst_check(NULL == (dialback_key = iks_server_dialback_key(NULL, "xmpp.example.com", "example.org", "D60000229F")));
	switch_safe_free(dialback_key);
	fst_check(NULL == (dialback_key = iks_server_dialback_key("s3cr3tf0rd14lb4ck", NULL, "example.org", "D60000229F")));
	switch_safe_free(dialback_key);
	fst_check(NULL == (dialback_key = iks_server_dialback_key("s3cr3tf0rd14lb4ck", "xmpp.example.com", NULL, "D60000229F")));
	switch_safe_free(dialback_key);
	fst_check(NULL == (dialback_key = iks_server_dialback_key("s3cr3tf0rd14lb4ck", "xmpp.example.com", "example.org", NULL)));
	switch_safe_free(dialback_key);
}
FST_TEST_END()

FST_TEST_BEGIN(validate_dtmf)
{
	fst_check(SWITCH_TRUE == iks_attrib_is_dtmf_digit("1"));
	fst_check(SWITCH_TRUE == iks_attrib_is_dtmf_digit("A"));
	fst_check(SWITCH_TRUE == iks_attrib_is_dtmf_digit("a"));
	fst_check(SWITCH_TRUE == iks_attrib_is_dtmf_digit("D"));
	fst_check(SWITCH_TRUE == iks_attrib_is_dtmf_digit("d"));
	fst_check(SWITCH_TRUE == iks_attrib_is_dtmf_digit("*"));
	fst_check(SWITCH_TRUE == iks_attrib_is_dtmf_digit("#"));
	fst_check(SWITCH_FALSE == iks_attrib_is_dtmf_digit("E"));
	fst_check(SWITCH_FALSE == iks_attrib_is_dtmf_digit(NULL));
	fst_check(SWITCH_FALSE == iks_attrib_is_dtmf_digit(""));
	fst_check(SWITCH_FALSE == iks_attrib_is_dtmf_digit("11"));
	fst_check(SWITCH_TRUE == validate_optional_attrib(iks_attrib_is_dtmf_digit, "A"));
	fst_check(SWITCH_TRUE == validate_optional_attrib(iks_attrib_is_dtmf_digit, "1"));
	fst_check(SWITCH_FALSE == validate_optional_attrib(iks_attrib_is_dtmf_digit, "Z"));
	fst_check(SWITCH_FALSE == validate_optional_attrib(iks_attrib_is_dtmf_digit, "11"));
	fst_check(SWITCH_TRUE == validate_optional_attrib(iks_attrib_is_dtmf_digit, NULL));
	fst_check(SWITCH_TRUE == validate_optional_attrib(iks_attrib_is_dtmf_digit, ""));
}
FST_TEST_END()


FST_SUITE_END()

FST_END()
