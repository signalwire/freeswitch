

#include <switch.h>
#include "test.h"
#include "nlsml.h"

static const char *nlsml_good =
	"<result x-model=\"http://theYesNoModel\""
	" xmlns:xf=\"http://www.w3.org/2000/xforms\""
	" grammar=\"http://theYesNoGrammar\">"
	"<interpretation>"
	"<xf:instance>"
	"<myApp:yes_no>"
	"<response>yes</response>"
	"</myApp:yes_no>"
	"</xf:instance>"
	"<input>ok</input>"
	"</interpretation>"
	"</result>";

static const char *nlsml_bad =
	"<result grammar=\"http://grammar\" x-model=\"http://dataModel\"\n"
	"xmlns:xf=\"http://www.w3.org/2000/xforms\"\n"
	"  <interpretation/>\n"
	"</result>\n";

static const char *nlsml_match_with_model_instance =
	"<result grammar=\"http://grammar\" x-model=\"http://dataModel\"\n"
	" xmlns:xf=\"http://www.w3.org/2000/xforms\">\"\n"
	"  <interpretation confidence=\"75\" grammar=\"http://grammar\"\n"
	"    x-model=\"http://dataModel\"\n"
	"    xmlns:xf=\"http://www.w3.org/2000/xforms\">\n"
	"\n"
	"    <model>\n"
	"      <xf:group name=\"nameAddress\">\n"
	"          <string name=\"name\"/>\n"
	"          <string name=\"street\"/>\n"
	"          <string name=\"city\"/>\n"
	"          <string name=\"state\"/>\n"
	"          <string name=\"zip\">\n"
	"            <mask>ddddd</mask>\n"
	"          </string>\n"
	"      </xf:group>\n"
	"    </model>\n"
	"\n"
	"    <xf:instance name=\"nameAddress\">\n"
	"      <nameAddress>\n"
	"          <street confidence=\"75\">123 Maple Street</street>\n"
	"          <city>Mill Valley</city>\n"
	"          <state>CA</state>\n"
	"          <zip>90952</zip>\n"
	"      </nameAddress>\n"
	"    </xf:instance>\n"
	"    <input>\n"
	"      My address is 123 Maple Street,\n"
	"      Mill Valley, California, 90952\n"
	"    </input>n"
	"  </interpretation>\n"
	"</result>\n";

static const char *nlsml_multi_input =
	"<result grammar=\"http://grammar\" x-model=\"http://dataModel\"\n"
	" xmlns:xf=\"http://www.w3.org/2000/xforms\">\"\n"
	"  <interpretation confidence=\"75\" grammar=\"http://grammar\"\n"
	"    x-model=\"http://dataModel\"\n"
	"    xmlns:xf=\"http://www.w3.org/2000/xforms\">\n"
	"\n"
	"    <input>\n"
	"       <input mode=\"speech\" confidence=\"50\"\n"
	"         timestamp-start=\"2000-04-03T0:00:00\"\n"
	"         timestamp-end=\"2000-04-03T0:00:00.2\">fried</input>\n"
	"       <input mode=\"speech\" confidence=\"100\"\n"
	"         timestamp-start=\"2000-04-03T0:00:00.25\"\n"
	"         timestamp-end=\"2000-04-03T0:00:00.6\">onions</input>\n"
	"    </input>\n"
	"  </interpretation>\n"
	"</result>\n";

static const char *nlsml_no_input =
	"<result grammar=\"http://grammar\" x-model=\"http://dataModel\"\n"
	" xmlns:xf=\"http://www.w3.org/2000/xforms\">\"\n"
	"  <interpretation confidence=\"100\" grammar=\"http://grammar\"\n"
	"    x-model=\"http://dataModel\"\n"
	"    xmlns:xf=\"http://www.w3.org/2000/xforms\">\n"
	"\n"
	"    <input>\n"
	"       <noinput/>\n"
	"    </input>\n"
	"  </interpretation>\n"
	"</result>\n";

static const char *nlsml_multi_input_dtmf =
	"<result grammar=\"http://grammar\" x-model=\"http://dataModel\"\n"
	" xmlns:xf=\"http://www.w3.org/2000/xforms\">\"\n"
	"  <interpretation confidence=\"100\" grammar=\"http://grammar\"\n"
	"    x-model=\"http://dataModel\"\n"
	"    xmlns:xf=\"http://www.w3.org/2000/xforms\">\n"
	"\n"
	"    <input>\n"
	"     <input mode=\"speech\"><nomatch/></input>\n"
	"     <input mode=\"dtmf\">1 2 3 4</input>\n"
	"    </input>\n"
	"  </interpretation>\n"
	"</result>\n";

static const char *nlsml_meta =
	"<result grammar=\"http://grammar\" x-model=\"http://dataModel\"\n"
	" xmlns:xf=\"http://www.w3.org/2000/xforms\">\n"
	"<interpretation grammar=\"http://toppings\"\n"
	" xmlns:xf=\"http://www.w3.org/2000/xforms\">\n"
	"  <input mode=\"speech\">\n"
	"    what toppings do you have?\n"
	"  </input>\n"
	"  <xf:model>\n"
	"    <xf:group xf:name=\"question\">\n"
	"      <xf:string xf:name=\"questioned_item\"/>\n"
	"      <xf:string xf:name=\"questioned_property\"/>\n"
	"    </xf:group>\n"
	"  </xf:model>\n"
	"  <xf:instance>\n"
	"    <xf:question>\n"
	"      <xf:questioned_item>toppings</xf:questioned_item>\n"
	"      <xf:questioned_property>\n"
	"    availability\n"
	"      </xf:questioned_property>\n"
	"    </xf:question>\n"
	"  </xf:instance>\n"
	"</interpretation>\n"
	"</result>\n";

static const char *nlsml_simple_ambiguity =
	"<result xmlns:xf=\"http://www.w3.org/2000/xforms\"\n"
	"   grammar=\"http://flight\">\n"
	"  <interpretation confidence=\"60\">\n"
	"    <input mode=\"speech\">\n"
	"      I want to go to Pittsburgh\n"
	"    </input>\n"
	"    <xf:model>\n"
	"      <group name=\"airline\">\n"
	"        <string name=\"to_city\"/>\n"
	"      </group>\n"
	"    </xf:model>\n"
	"    <xf:instance>\n"
	"      <myApp:airline>\n"
	"        <to_city>Pittsburgh</to_city>\n"
	"      </myApp:airline>\n"
	"    </xf:instance>\n"
	"  </interpretation>\n"
	"  <interpretation confidence=\"40\">\n"
	"      <input>I want to go to Stockholm</input>\n"
	"    <xf:model>\n"
	"      <group name=\"airline\">\n"
	"        <string name=\"to_city\"/>\n"
	"      </group>\n"
	"    </xf:model>\n"
	"    <xf:instance>\n"
	"      <myApp:airline>\n"
	"        <to_city>Stockholm</to_city>\n"
	"      </myApp:airline>\n"
	"    </xf:instance>\n"
	"  </interpretation>\n"
	"</result>\n";

const char *nlsml_mixed_initiative =
	"<result xmlns:xf=\"http://www.w3.org/2000/xforms\"\n"
	"   grammar=\"http://foodorder\">\n"
	"  <interpretation confidence=\"100\" >\n"
	"    <xf:model>\n"
	"      <group name=\"order\">\n"
	"        <group name=\"food_item\" maxOccurs=\"*\">\n"
	"          <group name=\"pizza\" >\n"
	"            <string name=\"ingredients\" maxOccurs=\"*\"/>\n"
	"          </group>\n"
	"          <group name=\"burger\">\n"
	"            <string name=\"ingredients\" maxOccurs=\"*/\">\n"
	"          </group>\n"
	"        </group>\n"
	"        <group name=\"drink_item\" maxOccurs=\"*\">\n"
	"          <string name=\"size\">\n"
	"          <string name=\"type\">\n"
	"        </group>\n"
	"        <string name=\"delivery_method\"/>\n"
	"      </group>\n"
	"    </xf:model>\n"
	"    <xf:instance>\n"
	"      <myApp:order>\n"
	"        <food_item confidence=\"100\">\n"
	"          <pizza>\n"
	"            <xf:ingredients confidence=\"100\">\n"
	"              pepperoni\n"
	"            </xf:ingredients>\n"
	"            <xf:ingredients confidence=\"100\">\n"
	"              cheese\n"
	"            </xf:ingredients>\n"
	"          </pizza>\n"
	"          <pizza>\n"
	"            <ingredients>sausage</ingredients>\n"
	"          </pizza>\n"
	"        </food_item>\n"
	"        <drink_item confidence=\"100\">\n"
	"          <size>2-liter</size>\n"
	"        </drink_item>\n"
	"        <delivery_method>to go</delivery_method>\n"
	"      </myApp:order>\n"
	"    </xf:instance>\n"
	"      <input mode=\"speech\">I would like 2 pizzas,\n"
	"         one with pepperoni and cheese, one with sausage\n"
	"         and a bottle of coke, to go.\n"
	"      </input>\n"
	"  </interpretation>\n"
	"</result>\n";

static const char *nlsml_no_match =
	"<result grammar=\"http://grammar\" x-model=\"http://dataModel\"\n"
	" xmlns:xf=\"http://www.w3.org/2000/xforms\">\"\n"
	"  <interpretation confidence=\"100\" grammar=\"http://grammar\"\n"
	"    x-model=\"http://dataModel\"\n"
	"    xmlns:xf=\"http://www.w3.org/2000/xforms\">\n"
	"\n"
	"    <input>\n"
	"     <input mode=\"speech\"><nomatch/></input>\n"
	"     <input mode=\"dtmf\"><nomatch/></input>\n"
	"    </input>\n"
	"  </interpretation>\n"
	"</result>\n";

/**
 * Test parsing NLSML example results
 */
static void test_parse_nlsml_examples(void)
{
	ASSERT_EQUALS(NMT_MATCH, nlsml_parse(nlsml_good, "1234"));
	ASSERT_EQUALS(NMT_BAD_XML, nlsml_parse(nlsml_bad, "1234"));
	ASSERT_EQUALS(NMT_MATCH, nlsml_parse(nlsml_match_with_model_instance, "1234"));
	ASSERT_EQUALS(NMT_MATCH, nlsml_parse(nlsml_multi_input, "1234"));
	ASSERT_EQUALS(NMT_NOINPUT, nlsml_parse(nlsml_no_input, "1234"));
	ASSERT_EQUALS(NMT_MATCH, nlsml_parse(nlsml_multi_input_dtmf, "1234"));
	ASSERT_EQUALS(NMT_MATCH, nlsml_parse(nlsml_meta, "1234"));
	ASSERT_EQUALS(NMT_MATCH, nlsml_parse(nlsml_simple_ambiguity, "1234"));
	ASSERT_EQUALS(NMT_MATCH, nlsml_parse(nlsml_mixed_initiative, "1234"));
	ASSERT_EQUALS(NMT_NOMATCH, nlsml_parse(nlsml_no_match, "1234"));
}

static const char *nlsml_dtmf_result =
	"<result xmlns='http://www.ietf.org/xml/ns/mrcpv2' "
	"xmlns:xf='http://www.w3.org/2000/xforms'><interpretation>"
	"<input><input mode='dtmf' confidence='100'>1 2 3 4</input>"
	"</input></interpretation></result>";

/**
 * Test parsing NLSML example results
 */
static void test_create_dtmf_match(void)
{
	iks *result = nlsml_create_dtmf_match("1234");
	char *result_str;
	ASSERT_NOT_NULL(result);
	result_str = iks_string(NULL, result);
	ASSERT_STRING_EQUALS(nlsml_dtmf_result, result_str);
	iks_free(result_str);
}

static const char *nlsml_good_normalized =
	"<result x-model='http://theYesNoModel'"
	" xmlns:xf='http://www.w3.org/2000/xforms'"
	" grammar='http://theYesNoGrammar'"
	" xmlns='http://www.ietf.org/xml/ns/mrcpv2'>"
	"<interpretation>"
	"<xf:instance>"
	"<myApp:yes_no>"
	"<response>yes</response>"
	"</myApp:yes_no>"
	"</xf:instance>"
	"<input>ok</input>"
	"</interpretation>"
	"</result>";

/**
 * Test NLSML normalization
 */
static void test_normalize(void)
{
	iks *result = nlsml_normalize(nlsml_good);
	ASSERT_NOT_NULL(result);
	ASSERT_STRING_EQUALS(nlsml_good_normalized, iks_string(NULL, result));
}

/**
 * main program
 */
int main(int argc, char **argv)
{
	const char *err;
	TEST_INIT
	nlsml_init();
	TEST(test_parse_nlsml_examples);
	TEST(test_create_dtmf_match);
	TEST(test_normalize);
	return 0;
}

