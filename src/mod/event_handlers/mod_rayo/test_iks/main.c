

#include <switch.h>
#include <iksemel.h>
#include "test.h"
#include "iks_helpers.h"

static const char *voxeo_grammar =
	"<iq id='8847' type='set' from='usera@192.168.1.10/voxeo3' to='e7632f74-8c55-11e2-84b0-e538fa88a1ef@192.168.1.10'><input xmlns='urn:xmpp:rayo:input:1' min-confidence='0.3' mode='DTMF' sensitivity='0.5'><grammar content-type='application/grammar+voxeo'><![CDATA[[1 DIGITS]]]></grammar></input></iq>";

static void test_iks_cdata_bug(void)
{
	iks *iq = NULL;
	iks *input = NULL;
	iksparser *p = iks_dom_new(&iq);
	const char *cdata;
	ASSERT_EQUALS(IKS_OK, iks_parse(p, voxeo_grammar, 0, 1));
	iks_parser_delete(p);
	ASSERT_NOT_NULL((input = iks_find(iq, "input")));
	ASSERT_NOT_NULL((cdata = iks_find_cdata(input, "grammar")));
	ASSERT_STRING_EQUALS("[1 DIGITS]", cdata);
	iks_delete(iq);
}

static const char *repeating_bracket =
	"<iq id='8847' type='set' from='usera@192.168.1.10/voxeo3' to='e7632f74-8c55-11e2-84b0-e538fa88a1ef@192.168.1.10'><input xmlns='urn:xmpp:rayo:input:1' min-confidence='0.3' mode='DTMF' sensitivity='0.5'><grammar content-type='application/grammar+voxeo'><![CDATA[[1 DIGITS]>]]]]]]]]] ]] ]]></grammar></input></iq>";

static void test_repeating_bracket(void)
{
	iks *iq = NULL;
	iks *input = NULL;
	iksparser *p = iks_dom_new(&iq);
	const char *cdata;
	ASSERT_EQUALS(IKS_OK, iks_parse(p, repeating_bracket, 0, 1));
	iks_parser_delete(p);
	ASSERT_NOT_NULL((input = iks_find(iq, "input")));
	ASSERT_NOT_NULL((cdata = iks_find_cdata(input, "grammar")));
	ASSERT_STRING_EQUALS("[1 DIGITS]>]]]]]]]]] ]] ", cdata);
	iks_delete(iq);
}

static const char *normal_cdata =
	"<iq id='8847' type='set' from='usera@192.168.1.10/voxeo3' to='e7632f74-8c55-11e2-84b0-e538fa88a1ef@192.168.1.10'><input xmlns='urn:xmpp:rayo:input:1' min-confidence='0.3' mode='DTMF' sensitivity='0.5'><grammar content-type='application/grammar+voxeo'><![CDATA[1 DIGITS]]></grammar></input></iq>";

static void test_normal_cdata(void)
{
	iks *iq = NULL;
	iks *input = NULL;
	iksparser *p = iks_dom_new(&iq);
	const char *cdata;
	ASSERT_EQUALS(IKS_OK, iks_parse(p, normal_cdata, 0, 1));
	iks_parser_delete(p);
	ASSERT_NOT_NULL((input = iks_find(iq, "input")));
	ASSERT_NOT_NULL((cdata = iks_find_cdata(input, "grammar")));
	ASSERT_STRING_EQUALS("1 DIGITS", cdata);
	iks_delete(iq);
}

static const char *empty_cdata =
	"<iq id='8847' type='set' from='usera@192.168.1.10/voxeo3' to='e7632f74-8c55-11e2-84b0-e538fa88a1ef@192.168.1.10'><input xmlns='urn:xmpp:rayo:input:1' min-confidence='0.3' mode='DTMF' sensitivity='0.5'><grammar content-type='application/grammar+voxeo'><![CDATA[]]></grammar></input></iq>";

static void test_empty_cdata(void)
{
	iks *iq = NULL;
	iks *input = NULL;
	iksparser *p = iks_dom_new(&iq);
	const char *cdata;
	ASSERT_EQUALS(IKS_OK, iks_parse(p, empty_cdata, 0, 1));
	iks_parser_delete(p);
	ASSERT_NOT_NULL((input = iks_find(iq, "input")));
	ASSERT_NULL((cdata = iks_find_cdata(input, "grammar")));
	iks_delete(iq);
}

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

static void test_rayo_test_srgs(void)
{
	iks *grammar = NULL;
	iksparser *p = iks_dom_new(&grammar);
	ASSERT_EQUALS(IKS_OK, iks_parse(p, rayo_test_srgs, 0, 1));
	iks_parser_delete(p);
	iks_delete(grammar);
}

#define MATCH 1
#define NO_MATCH 0

static void test_iks_helper_value_matches(void)
{
	ASSERT_EQUALS(MATCH, value_matches("1", "1,2,3"));
	ASSERT_EQUALS(MATCH, value_matches("2", "1,2,3"));
	ASSERT_EQUALS(MATCH, value_matches("3", "1,2,3"));
	ASSERT_EQUALS(NO_MATCH, value_matches("4", "1,2,3"));
	ASSERT_EQUALS(NO_MATCH, value_matches("1,2", "1,2,3"));
	ASSERT_EQUALS(NO_MATCH, value_matches(NULL, "1,2,3"));
	ASSERT_EQUALS(NO_MATCH, value_matches(NULL, NULL));
	ASSERT_EQUALS(NO_MATCH, value_matches("1", NULL));
	ASSERT_EQUALS(NO_MATCH, value_matches("", "1,2,3"));
	ASSERT_EQUALS(NO_MATCH, value_matches("", ""));
	ASSERT_EQUALS(NO_MATCH, value_matches("1", ""));
	ASSERT_EQUALS(MATCH, value_matches("duplex", "duplex,send,recv"));
	ASSERT_EQUALS(MATCH, value_matches("send", "duplex,send,recv"));
	ASSERT_EQUALS(MATCH, value_matches("recv", "duplex,send,recv"));
	ASSERT_EQUALS(NO_MATCH, value_matches("sendrecv", "duplex,send,recv"));
	ASSERT_EQUALS(MATCH, value_matches("duplex1", "duplex1,duplex2,duplex3"));
	ASSERT_EQUALS(MATCH, value_matches("duplex2", "duplex1,duplex2,duplex3"));
	ASSERT_EQUALS(MATCH, value_matches("duplex3", "duplex1,duplex2,duplex3"));
	ASSERT_EQUALS(NO_MATCH, value_matches("duplex4", "duplex1,duplex2,duplex3"));
	ASSERT_EQUALS(NO_MATCH, value_matches("duplex", "duplex1,duplex2,duplex3"));
}

static void test_dialback_key(void)
{
	ASSERT_STRING_EQUALS("37c69b1cf07a3f67c04a5ef5902fa5114f2c76fe4a2686482ba5b89323075643", iks_server_dialback_key("s3cr3tf0rd14lb4ck", "xmpp.example.com", "example.org", "D60000229F"));
	ASSERT_NULL(iks_server_dialback_key("", "xmpp.example.com", "example.org", "D60000229F"));
	ASSERT_NULL(iks_server_dialback_key("s3cr3tf0rd14lb4ck", "", "example.org", "D60000229F"));
	ASSERT_NULL(iks_server_dialback_key("s3cr3tf0rd14lb4ck", "xmpp.example.com", "", "D60000229F"));
	ASSERT_NULL(iks_server_dialback_key("s3cr3tf0rd14lb4ck", "xmpp.example.com", "example.org", ""));
	ASSERT_NULL(iks_server_dialback_key(NULL, "xmpp.example.com", "example.org", "D60000229F"));
	ASSERT_NULL(iks_server_dialback_key("s3cr3tf0rd14lb4ck", NULL, "example.org", "D60000229F"));
	ASSERT_NULL(iks_server_dialback_key("s3cr3tf0rd14lb4ck", "xmpp.example.com", NULL, "D60000229F"));
	ASSERT_NULL(iks_server_dialback_key("s3cr3tf0rd14lb4ck", "xmpp.example.com", "example.org", NULL));
}

static void test_validate_dtmf(void)
{
	ASSERT_EQUALS(SWITCH_TRUE, iks_attrib_is_dtmf_digit("1"));
	ASSERT_EQUALS(SWITCH_TRUE, iks_attrib_is_dtmf_digit("A"));
	ASSERT_EQUALS(SWITCH_TRUE, iks_attrib_is_dtmf_digit("a"));
	ASSERT_EQUALS(SWITCH_TRUE, iks_attrib_is_dtmf_digit("D"));
	ASSERT_EQUALS(SWITCH_TRUE, iks_attrib_is_dtmf_digit("d"));
	ASSERT_EQUALS(SWITCH_TRUE, iks_attrib_is_dtmf_digit("*"));
	ASSERT_EQUALS(SWITCH_TRUE, iks_attrib_is_dtmf_digit("#"));
	ASSERT_EQUALS(SWITCH_FALSE, iks_attrib_is_dtmf_digit("E"));
	ASSERT_EQUALS(SWITCH_FALSE, iks_attrib_is_dtmf_digit(NULL));
	ASSERT_EQUALS(SWITCH_FALSE, iks_attrib_is_dtmf_digit(""));
	ASSERT_EQUALS(SWITCH_FALSE, iks_attrib_is_dtmf_digit("11"));
	ASSERT_EQUALS(SWITCH_TRUE, validate_optional_attrib(iks_attrib_is_dtmf_digit, "A"));
	ASSERT_EQUALS(SWITCH_TRUE, validate_optional_attrib(iks_attrib_is_dtmf_digit, "1"));
	ASSERT_EQUALS(SWITCH_FALSE, validate_optional_attrib(iks_attrib_is_dtmf_digit, "Z"));
	ASSERT_EQUALS(SWITCH_FALSE, validate_optional_attrib(iks_attrib_is_dtmf_digit, "11"));
	ASSERT_EQUALS(SWITCH_TRUE, validate_optional_attrib(iks_attrib_is_dtmf_digit, NULL));
	ASSERT_EQUALS(SWITCH_TRUE, validate_optional_attrib(iks_attrib_is_dtmf_digit, ""));
}

/**
 * main program
 */
int main(int argc, char **argv)
{
	const char *err;
	TEST_INIT
	TEST(test_iks_cdata_bug);
	TEST(test_repeating_bracket);
	TEST(test_normal_cdata);
	TEST(test_empty_cdata);
	TEST(test_rayo_test_srgs);
	TEST(test_iks_helper_value_matches);
	TEST(test_dialback_key);
	TEST(test_validate_dtmf);
	return 0;
}
