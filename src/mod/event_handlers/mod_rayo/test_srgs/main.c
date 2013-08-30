

#include <switch.h>
#include "test.h"
#include "srgs.h"


static const char *adhearsion_menu_grammar =
	"<grammar xmlns=\"http://www.w3.org/2001/06/grammar\" version=\"1.0\" xml:lang=\"en-US\" mode=\"dtmf\" root=\"options\" tag-format=\"semantics/1.0-literals\">"
	"  <rule id=\"options\" scope=\"public\">\n"
	"    <one-of>\n"
	"      <item><tag>0</tag>1</item>\n"
	"      <item><tag>1</tag>5</item>\n"
	"      <item><tag>2</tag>7</item>\n"
	"      <item><tag>3</tag>9</item>\n"
	"    </one-of>\n"
	"  </rule>\n"
	"</grammar>\n";

/**
 * Test matching against adhearsion menu grammar
 */
static void test_match_adhearsion_menu_grammar(void)
{
	struct srgs_parser *parser;
	struct srgs_grammar *grammar;
	const char *interpretation;

	parser = srgs_parser_new("1234");
	ASSERT_NOT_NULL((grammar = srgs_parse(parser, adhearsion_menu_grammar)));

	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "0", &interpretation));
	ASSERT_NULL(interpretation);
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "1", &interpretation));
	ASSERT_STRING_EQUALS("0", interpretation);
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "2", &interpretation));
	ASSERT_NULL(interpretation);
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "3", &interpretation));
	ASSERT_NULL(interpretation);
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "4", &interpretation));
	ASSERT_NULL(interpretation);
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "5", &interpretation));
	ASSERT_STRING_EQUALS("1", interpretation);
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "6", &interpretation));
	ASSERT_NULL(interpretation);
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "7", &interpretation));
	ASSERT_STRING_EQUALS("2", interpretation);
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "8", &interpretation));
	ASSERT_NULL(interpretation);
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "9", &interpretation));
	ASSERT_STRING_EQUALS("3", interpretation);
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "#", &interpretation));
	ASSERT_NULL(interpretation);
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "*", &interpretation));
	ASSERT_NULL(interpretation);
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "A", &interpretation));
	ASSERT_NULL(interpretation);
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "27", &interpretation));
	ASSERT_NULL(interpretation);
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "223", &interpretation));
	ASSERT_NULL(interpretation);
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "0123456789*#", &interpretation));
	ASSERT_NULL(interpretation);

	srgs_parser_destroy(parser);
}


static const char *adhearsion_ask_grammar =
	"<grammar xmlns=\"http://www.w3.org/2001/06/grammar\" version=\"1.0\" xml:lang=\"en-US\" mode=\"dtmf\" root=\"inputdigits\">"
	"  <rule id=\"inputdigits\" scope=\"public\">\n"
	"    <one-of>\n"
	"      <item>0</item>\n"
	"      <item>1</item>\n"
	"      <item>2</item>\n"
	"      <item>3</item>\n"
	"      <item>4</item>\n"
	"      <item>5</item>\n"
	"      <item>6</item>\n"
	"      <item>7</item>\n"
	"      <item>8</item>\n"
	"      <item>9</item>\n"
	"      <item>#</item>\n"
	"      <item>*</item>\n"
	"    </one-of>\n"
	"  </rule>\n"
	"</grammar>\n";

/**
 * Test matching against adhearsion ask grammar
 */
static void test_match_adhearsion_ask_grammar(void)
{
	struct srgs_parser *parser;
	struct srgs_grammar *grammar;
	const char *interpretation;

	parser = srgs_parser_new("1234");
	ASSERT_NOT_NULL((grammar = srgs_parse(parser, adhearsion_ask_grammar)));

	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "0", &interpretation));
	ASSERT_NULL(interpretation);
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "1", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "2", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "3", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "4", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "5", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "6", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "7", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "8", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "9", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "#", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "*", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "A", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "27", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "223", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "0123456789*#", &interpretation));

	srgs_parser_destroy(parser);
}

static const char *multi_digit_grammar =
	"<grammar xmlns=\"http://www.w3.org/2001/06/grammar\" version=\"1.0\" xml:lang=\"en-US\" mode=\"dtmf\" root=\"misc\">"
	"  <rule id=\"misc\" scope=\"public\">\n"
	"    <one-of>\n"
	"      <item>01</item>\n"
	"      <item>13</item>\n"
	"      <item> 24</item>\n"
	"      <item>36 </item>\n"
	"      <item>223</item>\n"
	"      <item>5 5</item>\n"
	"      <item>63</item>\n"
	"      <item>76</item>\n"
	"      <item>8 8 0</item>\n"
	"      <item>93</item>\n"
	"      <item> # 2 </item>\n"
	"      <item>*3</item>\n"
	"      <item>  27</item>\n"
	"    </one-of>\n"
	"  </rule>\n"
	"</grammar>\n";

/**
 * Test matching against grammar with multiple digits per item
 */
static void test_match_multi_digit_grammar(void)
{
	struct srgs_parser *parser;
	struct srgs_grammar *grammar;
	const char *interpretation;

	parser = srgs_parser_new("1234");
	ASSERT_NOT_NULL((grammar = srgs_parse(parser, multi_digit_grammar)));

	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "0", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "1", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "2", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "3", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "4", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "5", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "6", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "7", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "8", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "9", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "#", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "*", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "A", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "27", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "223", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "0123456789*#", &interpretation));

	srgs_parser_destroy(parser);
}

static const char *multi_rule_grammar =
	"<grammar xmlns=\"http://www.w3.org/2001/06/grammar\" version=\"1.0\" xml:lang=\"en-US\" mode=\"dtmf\">"
	"  <rule id=\"misc\" scope=\"public\">\n"
	"    <one-of>\n"
	"      <item>01</item>\n"
	"      <item>13</item>\n"
	"      <item> 24</item>\n"
	"      <item>36 </item>\n"
	"      <item>5 5</item>\n"
	"      <item>63</item>\n"
	"    </one-of>\n"
	"  </rule>\n"
	"  <rule id=\"misc2\" scope=\"public\">\n"
	"    <one-of>\n"
	"      <item>76</item>\n"
	"      <item>8 8 0</item>\n"
	"      <item>93</item>\n"
	"      <item> # 2 </item>\n"
	"      <item>*3</item>\n"
	"      <item>  27</item>\n"
	"      <item>223</item>\n"
	"    </one-of>\n"
	"  </rule>\n"
	"</grammar>\n";

static void test_match_multi_rule_grammar(void)
{
	struct srgs_parser *parser;
	struct srgs_grammar *grammar;
	const char *interpretation;

	parser = srgs_parser_new("1234");
	ASSERT_NOT_NULL((grammar = srgs_parse(parser, multi_rule_grammar)));

	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "0", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "1", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "2", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "3", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "4", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "5", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "6", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "7", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "8", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "9", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "#", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "*", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "A", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "27", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "223", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "0123456789*#", &interpretation));

	srgs_parser_destroy(parser);
}

static const char *rayo_example_grammar =
	"<grammar mode=\"dtmf\" version=\"1.0\""
	"    xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
	"    xsi:schemaLocation=\"http://www.w3.org/2001/06/grammar\n"
	"                        http://www.w3.org/TR/speech-grammar/grammar.xsd\""
	"    xmlns=\"http://www.w3.org/2001/06/grammar\">\n"
	"\n"
	"    <rule id=\"digit\">\n"
	"    <one-of>\n"
	"       <item> 0 </item>\n"
	"       <item> 1 </item>\n"
	"       <item> 2 </item>\n"
	"       <item> 3 </item>\n"
	"       <item> 4 </item>\n"
	"       <item> 5 </item>\n"
	"       <item> 6 </item>\n"
	"       <item> 7 </item>\n"
	"       <item> 8 </item>\n"
	"       <item> 9 </item>\n"
	"    </one-of>\n"
	"    </rule>\n"
	"\n"
	"    <rule id=\"pin\" scope=\"public\">\n"
	"    <one-of>\n"
	"       <item>\n"
	"         <item repeat=\"4\"><ruleref uri=\"#digit\"/></item>\n"
	"           #\n"
	"         </item>"
	"       <item>"
	"         * 9 \n"
	"       </item>\n"
	"    </one-of>\n"
	"    </rule>\n"
	"</grammar>\n";

static void test_match_rayo_example_grammar(void)
{
	struct srgs_parser *parser;
	struct srgs_grammar *grammar;
	const char *interpretation;

	parser = srgs_parser_new("1234");
	ASSERT_NOT_NULL((grammar = srgs_parse(parser, rayo_example_grammar)));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "0", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "1", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "2", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "3", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "4", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "5", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "6", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "7", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "8", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "9", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "#", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "*", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "A", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "*9", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "1234#", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "2321#", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "27", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "223", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "0123456789*#", &interpretation));

	srgs_parser_destroy(parser);
}

static const char *bad_ref_grammar =
	"<grammar mode=\"dtmf\" version=\"1.0\""
	"    xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
	"    xsi:schemaLocation=\"http://www.w3.org/2001/06/grammar\n"
	"                        http://www.w3.org/TR/speech-grammar/grammar.xsd\""
	"    xmlns=\"http://www.w3.org/2001/06/grammar\">\n"
	"\n"
	"    <rule id=\"digit\">\n"
	"    <one-of>\n"
	"       <item> 0 </item>\n"
	"       <item> 1 </item>\n"
	"       <item> 2 </item>\n"
	"       <item> 3 </item>\n"
	"       <item> 4 </item>\n"
	"       <item> 5 </item>\n"
	"       <item> 6 </item>\n"
	"       <item> 7 </item>\n"
	"       <item> 8 </item>\n"
	"       <item> 9 </item>\n"
	"    </one-of>\n"
	"    </rule>\n"
	"\n"
	"    <rule id=\"pin\" scope=\"public\">\n"
	"    <one-of>\n"
	"       <item>\n"
	"         <item repeat=\"4\"><ruleref uri=\"#digi\"/></item>\n"
	"           #\n"
	"         </item>"
	"       <item>"
	"         * 9 \n"
	"       </item>\n"
	"    </one-of>\n"
	"    </rule>\n"
	"</grammar>\n";

static const char *adhearsion_ask_grammar_bad =
	"<grammar xmlns=\"http://www.w3.org/2001/06/grammar\" version=\"1.0\" xml:lang=\"en-US\" mode=\"dtmf\" root=\"inputdigits\">"
	"  <rule id=\"inputdigits\" scope=\"public\">\n"
	"    <one-of>\n"
	"      <item>0</item>\n"
	"      <item>1</item\n"
	"      <item>2</item>\n"
	"      <item>3</item>\n"
	"      <item>4</item>\n"
	"      <item>5</item>\n"
	"      <item>6</item>\n"
	"      <item>7</item>\n"
	"      <item>8</item>\n"
	"      <item>9</item>\n"
	"      <item>#</item>\n"
	"      <item>*</item>\n"
	"    </one-of>\n"
	"  </rule>\n"
	"</grammar>\n";

static void test_parse_grammar(void)
{
	struct srgs_parser *parser;

	parser = srgs_parser_new("1234");

	ASSERT_NOT_NULL(srgs_parse(parser, adhearsion_ask_grammar));
	ASSERT_NULL(srgs_parse(parser, adhearsion_ask_grammar_bad));
	ASSERT_NULL(srgs_parse(parser, NULL));
	ASSERT_NULL(srgs_parse(NULL, adhearsion_ask_grammar));
	ASSERT_NULL(srgs_parse(NULL, adhearsion_ask_grammar_bad));
	ASSERT_NULL(srgs_parse(parser, bad_ref_grammar));

	srgs_parser_destroy(parser);
}

static const char *repeat_item_grammar_bad =
	"<grammar mode=\"dtmf\" version=\"1.0\""
	"    xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
	"    xsi:schemaLocation=\"http://www.w3.org/2001/06/grammar\n"
	"                        http://www.w3.org/TR/speech-grammar/grammar.xsd\""
	"    xmlns=\"http://www.w3.org/2001/06/grammar\">\n"
	"\n"
	"    <rule id=\"pin\" scope=\"public\">\n"
	"       <item>\n"
	"         <item repeat=\"3-1\">4</item>\n"
	"           #\n"
	"       </item>"
	"    </rule>\n"
	"</grammar>\n";

static const char *repeat_item_grammar_bad2 =
	"<grammar mode=\"dtmf\" version=\"1.0\""
	"    xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
	"    xsi:schemaLocation=\"http://www.w3.org/2001/06/grammar\n"
	"                        http://www.w3.org/TR/speech-grammar/grammar.xsd\""
	"    xmlns=\"http://www.w3.org/2001/06/grammar\">\n"
	"\n"
	"    <rule id=\"pin\" scope=\"public\">\n"
	"       <item>\n"
	"         <item repeat=\"-1\">4</item>\n"
	"           #\n"
	"       </item>"
	"    </rule>\n"
	"</grammar>\n";

static const char *repeat_item_grammar_bad3 =
	"<grammar mode=\"dtmf\" version=\"1.0\""
	"    xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
	"    xsi:schemaLocation=\"http://www.w3.org/2001/06/grammar\n"
	"                        http://www.w3.org/TR/speech-grammar/grammar.xsd\""
	"    xmlns=\"http://www.w3.org/2001/06/grammar\">\n"
	"\n"
	"    <rule id=\"pin\" scope=\"public\">\n"
	"       <item>\n"
	"         <item repeat=\"1--1\">4</item>\n"
	"           #\n"
	"       </item>"
	"    </rule>\n"
	"</grammar>\n";

static const char *repeat_item_grammar_bad4 =
	"<grammar mode=\"dtmf\" version=\"1.0\""
	"    xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
	"    xsi:schemaLocation=\"http://www.w3.org/2001/06/grammar\n"
	"                        http://www.w3.org/TR/speech-grammar/grammar.xsd\""
	"    xmlns=\"http://www.w3.org/2001/06/grammar\">\n"
	"\n"
	"    <rule id=\"pin\" scope=\"public\">\n"
	"       <item>\n"
	"         <item repeat=\"ABC\">4</item>\n"
	"           #\n"
	"       </item>"
	"    </rule>\n"
	"</grammar>\n";

static const char *repeat_item_grammar_bad5 =
	"<grammar mode=\"dtmf\" version=\"1.0\""
	"    xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
	"    xsi:schemaLocation=\"http://www.w3.org/2001/06/grammar\n"
	"                        http://www.w3.org/TR/speech-grammar/grammar.xsd\""
	"    xmlns=\"http://www.w3.org/2001/06/grammar\">\n"
	"\n"
	"    <rule id=\"pin\" scope=\"public\">\n"
	"       <item>\n"
	"         <item repeat=\"\">4</item>\n"
	"           #\n"
	"       </item>"
	"    </rule>\n"
	"</grammar>\n";

static const char *repeat_item_grammar_bad6 =
	"<grammar mode=\"dtmf\" version=\"1.0\""
	"    xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
	"    xsi:schemaLocation=\"http://www.w3.org/2001/06/grammar\n"
	"                        http://www.w3.org/TR/speech-grammar/grammar.xsd\""
	"    xmlns=\"http://www.w3.org/2001/06/grammar\">\n"
	"\n"
	"    <rule id=\"pin\" scope=\"public\">\n"
	"       <item>\n"
	"         <item repeat=\"1-Z\">4</item>\n"
	"           #\n"
	"       </item>"
	"    </rule>\n"
	"</grammar>\n";

static const char *repeat_item_grammar =
	"<grammar mode=\"dtmf\" version=\"1.0\""
	"    xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
	"    xsi:schemaLocation=\"http://www.w3.org/2001/06/grammar\n"
	"                        http://www.w3.org/TR/speech-grammar/grammar.xsd\""
	"    xmlns=\"http://www.w3.org/2001/06/grammar\">\n"
	"\n"
	"    <rule id=\"digit\">\n"
	"    <one-of>\n"
	"       <item> 0 </item>\n"
	"       <item> 1 </item>\n"
	"       <item> 2 </item>\n"
	"       <item> 3 </item>\n"
	"       <item> 4 </item>\n"
	"       <item> 5 </item>\n"
	"       <item> 6 </item>\n"
	"       <item> 7 </item>\n"
	"       <item> 8 </item>\n"
	"       <item> 9 </item>\n"
	"    </one-of>\n"
	"    </rule>\n"
	"\n"
	"    <rule id=\"pin\" scope=\"public\">\n"
	"    <one-of>\n"
	"       <item>\n"
	"         <item repeat=\"4-4\"><ruleref uri=\"#digit\"/></item>\n"
	"           #\n"
	"         </item>"
	"       <item>"
	"         * 9 \n"
	"       </item>\n"
	"    </one-of>\n"
	"    </rule>\n"
	"</grammar>\n";

static const char *repeat_item_range_grammar =
	"<grammar mode=\"dtmf\" version=\"1.0\""
	"    xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
	"    xsi:schemaLocation=\"http://www.w3.org/2001/06/grammar\n"
	"                        http://www.w3.org/TR/speech-grammar/grammar.xsd\""
	"    xmlns=\"http://www.w3.org/2001/06/grammar\">\n"
	"\n"
	"    <rule id=\"digit\">\n"
	"    <one-of>\n"
	"       <item> 0 </item>\n"
	"       <item> 1 </item>\n"
	"       <item> 2 </item>\n"
	"       <item> 3 </item>\n"
	"       <item> 4 </item>\n"
	"       <item> 5 </item>\n"
	"       <item> 6 </item>\n"
	"       <item> 7 </item>\n"
	"       <item> 8 </item>\n"
	"       <item> 9 </item>\n"
	"    </one-of>\n"
	"    </rule>\n"
	"\n"
	"    <rule id=\"pin\" scope=\"public\">\n"
	"    <one-of>\n"
	"       <item>\n"
	"         <item repeat=\"4-6\"><ruleref uri=\"#digit\"/></item>\n"
	"           #\n"
	"         </item>"
	"       <item>"
	"         * 9 \n"
	"       </item>\n"
	"    </one-of>\n"
	"    </rule>\n"
	"</grammar>\n";

static const char *repeat_item_optional_grammar =
	"<grammar mode=\"dtmf\" version=\"1.0\""
	"    xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
	"    xsi:schemaLocation=\"http://www.w3.org/2001/06/grammar\n"
	"                        http://www.w3.org/TR/speech-grammar/grammar.xsd\""
	"    xmlns=\"http://www.w3.org/2001/06/grammar\">\n"
	"\n"
	"    <rule id=\"digit\">\n"
	"    <one-of>\n"
	"       <item> 0 </item>\n"
	"       <item> 1 </item>\n"
	"       <item> 2 </item>\n"
	"       <item> 3 </item>\n"
	"       <item> 4 </item>\n"
	"       <item> 5 </item>\n"
	"       <item> 6 </item>\n"
	"       <item> 7 </item>\n"
	"       <item> 8 </item>\n"
	"       <item> 9 </item>\n"
	"    </one-of>\n"
	"    </rule>\n"
	"\n"
	"    <rule id=\"pin\" scope=\"public\">\n"
	"    <one-of>\n"
	"       <item>\n"
	"         <item repeat=\"0-1\"><ruleref uri=\"#digit\"/></item>\n"
	"           #\n"
	"         </item>"
	"       <item>"
	"         * 9 \n"
	"       </item>\n"
	"    </one-of>\n"
	"    </rule>\n"
	"</grammar>\n";

static const char *repeat_item_star_grammar =
	"<grammar mode=\"dtmf\" version=\"1.0\""
	"    xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
	"    xsi:schemaLocation=\"http://www.w3.org/2001/06/grammar\n"
	"                        http://www.w3.org/TR/speech-grammar/grammar.xsd\""
	"    xmlns=\"http://www.w3.org/2001/06/grammar\">\n"
	"\n"
	"    <rule id=\"digit\">\n"
	"    <one-of>\n"
	"       <item> 0 </item>\n"
	"       <item> 1 </item>\n"
	"       <item> 2 </item>\n"
	"       <item> 3 </item>\n"
	"       <item> 4 </item>\n"
	"       <item> 5 </item>\n"
	"       <item> 6 </item>\n"
	"       <item> 7 </item>\n"
	"       <item> 8 </item>\n"
	"       <item> 9 </item>\n"
	"    </one-of>\n"
	"    </rule>\n"
	"\n"
	"    <rule id=\"pin\" scope=\"public\">\n"
	"    <one-of>\n"
	"       <item>\n"
	"         <item repeat=\"0-\"><ruleref uri=\"#digit\"/></item>\n"
	"           #\n"
	"         </item>"
	"       <item>"
	"         * 9 \n"
	"       </item>\n"
	"    </one-of>\n"
	"    </rule>\n"
	"</grammar>\n";

static const char *repeat_item_plus_grammar =
	"<grammar mode=\"dtmf\" version=\"1.0\""
	"    xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
	"    xsi:schemaLocation=\"http://www.w3.org/2001/06/grammar\n"
	"                        http://www.w3.org/TR/speech-grammar/grammar.xsd\""
	"    xmlns=\"http://www.w3.org/2001/06/grammar\">\n"
	"\n"
	"    <rule id=\"digit\">\n"
	"    <one-of>\n"
	"       <item> 0 </item>\n"
	"       <item> 1 </item>\n"
	"       <item> 2 </item>\n"
	"       <item> 3 </item>\n"
	"       <item> 4 </item>\n"
	"       <item> 5 </item>\n"
	"       <item> 6 </item>\n"
	"       <item> 7 </item>\n"
	"       <item> 8 </item>\n"
	"       <item> 9 </item>\n"
	"    </one-of>\n"
	"    </rule>\n"
	"\n"
	"    <rule id=\"pin\" scope=\"public\">\n"
	"    <one-of>\n"
	"       <item>\n"
	"         <item repeat=\"1-\"><ruleref uri=\"#digit\"/></item>\n"
	"           #\n"
	"         </item>"
	"       <item>"
	"         * 9 \n"
	"       </item>\n"
	"    </one-of>\n"
	"    </rule>\n"
	"</grammar>\n";

static void test_repeat_item_grammar(void)
{
	struct srgs_parser *parser;
	struct srgs_grammar *grammar;
	const char *interpretation;

	parser = srgs_parser_new("1234");
	ASSERT_NULL(srgs_parse(parser, repeat_item_grammar_bad));
	ASSERT_NULL(srgs_parse(parser, repeat_item_grammar_bad2));
	ASSERT_NULL(srgs_parse(parser, repeat_item_grammar_bad3));
	ASSERT_NULL(srgs_parse(parser, repeat_item_grammar_bad4));
	ASSERT_NULL(srgs_parse(parser, repeat_item_grammar_bad5));
	ASSERT_NULL(srgs_parse(parser, repeat_item_grammar_bad6));
	ASSERT_NOT_NULL((grammar = srgs_parse(parser, repeat_item_grammar)));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "1111#", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "1111", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "1234#", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "1234", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "11115#", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "11115", &interpretation));
	ASSERT_NOT_NULL((grammar = srgs_parse(parser, repeat_item_range_grammar)));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "1111#", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "1111", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "1234#", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "1234", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "11115#", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "11115", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "111156#", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "111156", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "1111567#", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "1111567", &interpretation));
	ASSERT_NOT_NULL((grammar = srgs_parse(parser, repeat_item_optional_grammar)));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "1111#", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "1111", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "1234#", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "1234", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "11115#", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "11115", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "111156#", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "111156", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "1111567#", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "1111567", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "1#", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "1", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "#", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "A#", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "A", &interpretation));
	ASSERT_NOT_NULL((grammar = srgs_parse(parser, repeat_item_plus_grammar)));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "1111#", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "1111", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "1234#", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "1234", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "11115#", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "11115", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "111156#", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "111156", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "111157#", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "111157", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "1#", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "1", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "#", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "A#", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "A", &interpretation));
	ASSERT_NOT_NULL((grammar = srgs_parse(parser, repeat_item_star_grammar)));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "1111#", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "1111", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "1234#", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "1234", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "11115#", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "11115", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "111156#", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "111156", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "111157#", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "111157", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "1#", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_PARTIAL, srgs_grammar_match(grammar, "1", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "#", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "A#", &interpretation));
	ASSERT_EQUALS(SMT_NO_MATCH, srgs_grammar_match(grammar, "A", &interpretation));

	srgs_parser_destroy(parser);
}

static const char *repeat_item_range_ambiguous_grammar =
	"<grammar mode=\"dtmf\" version=\"1.0\""
	"    xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
	"    xsi:schemaLocation=\"http://www.w3.org/2001/06/grammar\n"
	"                        http://www.w3.org/TR/speech-grammar/grammar.xsd\""
	"    xmlns=\"http://www.w3.org/2001/06/grammar\">\n"
	"\n"
	"    <rule id=\"digit\">\n"
	"    <one-of>\n"
	"       <item> 0 </item>\n"
	"       <item> 1 </item>\n"
	"       <item> 2 </item>\n"
	"       <item> 3 </item>\n"
	"       <item> 4 </item>\n"
	"       <item> 5 </item>\n"
	"       <item> 6 </item>\n"
	"       <item> 7 </item>\n"
	"       <item> 8 </item>\n"
	"       <item> 9 </item>\n"
	"    </one-of>\n"
	"    </rule>\n"
	"\n"
	"    <rule id=\"pin\" scope=\"public\">\n"
	"         <item repeat=\"1-3\"><ruleref uri=\"#digit\"/></item>\n"
	"    </rule>\n"
	"</grammar>\n";

static void test_repeat_item_range_ambiguous_grammar(void)
{
	struct srgs_parser *parser;
	struct srgs_grammar *grammar;
	const char *interpretation;

	parser = srgs_parser_new("1234");
	ASSERT_NOT_NULL((grammar = srgs_parse(parser, repeat_item_range_ambiguous_grammar)));
	ASSERT_EQUALS(SMT_MATCH, srgs_grammar_match(grammar, "1", &interpretation));
	ASSERT_EQUALS(SMT_MATCH, srgs_grammar_match(grammar, "12", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "123", &interpretation));
}

static const char *repeat_item_range_optional_pound_grammar =
	"<grammar mode=\"dtmf\" version=\"1.0\""
	"    xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
	"    xsi:schemaLocation=\"http://www.w3.org/2001/06/grammar\n"
	"                        http://www.w3.org/TR/speech-grammar/grammar.xsd\""
	"    xmlns=\"http://www.w3.org/2001/06/grammar\">\n"
	"\n"
	"    <rule id=\"digit\">\n"
	"    <one-of>\n"
	"       <item> 0 </item>\n"
	"       <item> 1 </item>\n"
	"       <item> 2 </item>\n"
	"       <item> 3 </item>\n"
	"       <item> 4 </item>\n"
	"       <item> 5 </item>\n"
	"       <item> 6 </item>\n"
	"       <item> 7 </item>\n"
	"       <item> 8 </item>\n"
	"       <item> 9 </item>\n"
	"    </one-of>\n"
	"    </rule>\n"
	"\n"
	"    <rule id=\"pin\" scope=\"public\">\n"
	"       <one-of>\n"
	"         <item>\n"
	"            <item repeat=\"1-2\"><ruleref uri=\"#digit\"/></item>\n"
	"            <item repeat=\"0-1\">#</item>\n"
	"         </item>\n"
	"         <item repeat=\"3\"><ruleref uri=\"#digit\"/></item>\n"
	"       </one-of>\n"
	"    </rule>\n"
	"</grammar>\n";
static void test_repeat_item_range_optional_pound_grammar(void)
{
	struct srgs_parser *parser;
	struct srgs_grammar *grammar;
	const char *interpretation;

	parser = srgs_parser_new("1234");
	ASSERT_NOT_NULL((grammar = srgs_parse(parser, repeat_item_range_optional_pound_grammar)));
	ASSERT_EQUALS(SMT_MATCH, srgs_grammar_match(grammar, "1", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "1#", &interpretation));
	ASSERT_EQUALS(SMT_MATCH, srgs_grammar_match(grammar, "12", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "12#", &interpretation));
	ASSERT_EQUALS(SMT_MATCH_END, srgs_grammar_match(grammar, "123", &interpretation));
}

/*
<polite> = please | kindly | oh mighty computer;
public <command> = [ <polite> ] don't crash;
*/
static const char *voice_srgs1 =
	"<grammar mode=\"voice\" version=\"1.0\"\n"
	"    xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
	"    xsi:schemaLocation=\"http://www.w3.org/2001/06/grammar\n"
	"                        http://www.w3.org/TR/speech-grammar/grammar.xsd\"\n"
	"    language\"en-US\"\n"
	"    xmlns=\"http://www.w3.org/2001/06/grammar\">\n"
	"\n"
	"    <rule id=\"polite\">\n"
	"    <one-of>\n"
	"      <item>please</item>\n"
	"      <item>kindly</item>\n"
	"      <item> oh mighty computer</item>\n"
	"    </one-of>\n"
	"    </rule>\n"
	"\n"
	"    <rule id=\"command\" scope=\"public\">\n"
	"       <item repeat=\"0-1\"><ruleref uri=\"#polite\"/></item>\n"
	"       <item>don't crash</item>\n"
	"    </rule>\n"
	"</grammar>\n";

static const char *voice_jsgf =
	"#JSGF V1.0;\n"
	"grammar org.freeswitch.srgs_to_jsgf;\n"
	"public <command> = [ <polite> ] don't crash;\n"
	"<polite> = ( ( please ) | ( kindly ) | ( oh mighty computer ) );\n";

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

static void test_jsgf(void)
{
	struct srgs_parser *parser;
	struct srgs_grammar *grammar;
	const char *jsgf;
	parser = srgs_parser_new("1234");

	ASSERT_NOT_NULL((grammar = srgs_parse(parser, adhearsion_ask_grammar)));
	ASSERT_NOT_NULL((jsgf = srgs_grammar_to_jsgf(grammar)));
	ASSERT_NOT_NULL((grammar = srgs_parse(parser, voice_srgs1)));
	ASSERT_NOT_NULL((jsgf = srgs_grammar_to_jsgf(grammar)));
	ASSERT_STRING_EQUALS(voice_jsgf, jsgf);
	ASSERT_NOT_NULL((grammar = srgs_parse(parser, multi_rule_grammar)));
	ASSERT_NOT_NULL((jsgf = srgs_grammar_to_jsgf(grammar)));
	ASSERT_NOT_NULL((grammar = srgs_parse(parser, rayo_test_srgs)));
	ASSERT_NOT_NULL((jsgf = srgs_grammar_to_jsgf(grammar)));
	ASSERT_NULL(srgs_grammar_to_jsgf(NULL));
	srgs_parser_destroy(parser);
}

/* removed the ruleref to URL from example */
static const char *w3c_example_grammar =
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"

	"<!DOCTYPE grammar PUBLIC \"-//W3C//DTD GRAMMAR 1.0//EN\""
	"                  \"http://www.w3.org/TR/speech-grammar/grammar.dtd\">\n"
	"\n"
	"<grammar xmlns=\"http://www.w3.org/2001/06/grammar\" xml:lang=\"en\"\n"
	"         xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
	"         xsi:schemaLocation=\"http://www.w3.org/2001/06/grammar\n"
	"                             http://www.w3.org/TR/speech-grammar/grammar.xsd\"\n"
	"         version=\"1.0\" mode=\"voice\" root=\"basicCmd\">\n"
	"\n"
	"<meta name=\"author\" content=\"Stephanie Williams\"/>\n"
	"\n"
	"<rule id=\"basicCmd\" scope=\"public\">\n"
	"  <example> please move the window </example>\n"
	"  <example> open a file </example>\n"
	"\n"
	"  <!--ruleref uri=\"http://grammar.example.com/politeness.grxml#startPolite\"/-->\n"
	"\n"
	"  <ruleref uri=\"#command\"/>\n"
	"  <!--ruleref uri=\"http://grammar.example.com/politeness.grxml#endPolite\"/-->\n"
	"\n"
	"</rule>\n"
	"\n"
	"<rule id=\"command\">\n"
	"  <ruleref uri=\"#action\"/> <ruleref uri=\"#object\"/>\n"
	"</rule>\n"
	"\n"
	"<rule id=\"action\">\n"
	"   <one-of>\n"
	"      <item weight=\"10\"> open   <tag>TAG-CONTENT-1</tag> </item>\n"
	"      <item weight=\"2\">  close  <tag>TAG-CONTENT-2</tag> </item>\n"
	"      <item weight=\"1\">  delete <tag>TAG-CONTENT-3</tag> </item>\n"
	"      <item weight=\"1\">  move   <tag>TAG-CONTENT-4</tag> </item>\n"
	"    </one-of>\n"
	"</rule>\n"
	"\n"
	"<rule id=\"object\">\n"
	"  <item repeat=\"0-1\">\n"
	"    <one-of>\n"
	"      <item> the </item>\n"
	"      <item> a </item>\n"
	"    </one-of>\n"
	"  </item>\n"
	"\n"
	"  <one-of>\n"
	"      <item> window </item>\n"
	"      <item> file </item>\n"
	"      <item> menu </item>\n"
	"  </one-of>\n"
	"</rule>\n"
	"\n"
	"</grammar>";

static void test_w3c_example_grammar(void)
{
	struct srgs_parser *parser;
	struct srgs_grammar *grammar;
	parser = srgs_parser_new("1234");

	ASSERT_NOT_NULL((grammar = srgs_parse(parser, w3c_example_grammar)));
	ASSERT_NOT_NULL(srgs_grammar_to_jsgf(grammar));
}

static const char *metadata_grammar =
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"

	"<!DOCTYPE grammar PUBLIC \"-//W3C//DTD GRAMMAR 1.0//EN\""
	"                  \"http://www.w3.org/TR/speech-grammar/grammar.dtd\">\n"
	"\n"
	"<grammar xmlns=\"http://www.w3.org/2001/06/grammar\" xml:lang=\"en\"\n"
	"         xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
	"         xsi:schemaLocation=\"http://www.w3.org/2001/06/grammar\n"
	"                             http://www.w3.org/TR/speech-grammar/grammar.xsd\"\n"
	"         version=\"1.0\" mode=\"voice\" root=\"basicCmd\">\n"
	"\n"
	"<meta name=\"author\" content=\"Stephanie Williams\"/>\n"
	"<metadata>\n"
	"  <foo>\n"
	"   <bar>\n"
	"    <luser/>\n"
	"   </bar>\n"
	"  </foo>\n"
	"</metadata>\n"
	"\n"
	"<rule id=\"basicCmd\" scope=\"public\">\n"
	"  <example> please move the window </example>\n"
	"  <example> open a file </example>\n"
	"\n"
	"  <!--ruleref uri=\"http://grammar.example.com/politeness.grxml#startPolite\"/-->\n"
	"\n"
	"  <ruleref uri=\"#command\"/>\n"
	"  <!--ruleref uri=\"http://grammar.example.com/politeness.grxml#endPolite\"/-->\n"
	"\n"
	"</rule>\n"
	"\n"
	"<rule id=\"command\">\n"
	"  <ruleref uri=\"#action\"/> <ruleref uri=\"#object\"/>\n"
	"</rule>\n"
	"\n"
	"<rule id=\"action\">\n"
	"   <one-of>\n"
	"      <item weight=\"10\"> open   <tag>TAG-CONTENT-1</tag> </item>\n"
	"      <item weight=\"2\">  close  <tag>TAG-CONTENT-2</tag> </item>\n"
	"      <item weight=\"1\">  delete <tag>TAG-CONTENT-3</tag> </item>\n"
	"      <item weight=\"1\">  move   <tag>TAG-CONTENT-4</tag> </item>\n"
	"    </one-of>\n"
	"</rule>\n"
	"\n"
	"<rule id=\"object\">\n"
	"  <item repeat=\"0-1\">\n"
	"    <one-of>\n"
	"      <item> the </item>\n"
	"      <item> a </item>\n"
	"    </one-of>\n"
	"  </item>\n"
	"\n"
	"  <one-of>\n"
	"      <item> window </item>\n"
	"      <item> file </item>\n"
	"      <item> menu </item>\n"
	"  </one-of>\n"
	"</rule>\n"
	"\n"
	"</grammar>";

static void test_metadata_grammar(void)
{
	struct srgs_parser *parser;
	struct srgs_grammar *grammar;
	parser = srgs_parser_new("1234");

	ASSERT_NOT_NULL((grammar = srgs_parse(parser, metadata_grammar)));
	ASSERT_NOT_NULL(srgs_grammar_to_jsgf(grammar));
}

/**
 * main program
 */
int main(int argc, char **argv)
{
	const char *err;
	TEST_INIT
	srgs_init();
	TEST(test_parse_grammar);
	TEST(test_match_adhearsion_menu_grammar);
	TEST(test_match_adhearsion_ask_grammar);
	TEST(test_match_multi_digit_grammar);
	TEST(test_match_multi_rule_grammar);
	TEST(test_match_rayo_example_grammar);
	TEST(test_repeat_item_grammar);
	TEST(test_jsgf);
	TEST(test_w3c_example_grammar);
	TEST(test_metadata_grammar);
	TEST(test_repeat_item_range_ambiguous_grammar);
	TEST(test_repeat_item_range_optional_pound_grammar);
	return 0;
}

