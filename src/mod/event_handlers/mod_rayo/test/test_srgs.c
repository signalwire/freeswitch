

#include <switch.h>
#include <test/switch_test.h>
#include <srgs.h>


static const char *adhearsion_menu_grammar =
	"<grammar xmlns=\"http://www.w3.org/2001/06/grammar\" version=\"1.0\" xml:lang=\"en-US\" mode=\"dtmf\" root=\"options\" tag-format=\"semantics/1.0-literals\">"
	"  <rule id=\"options\" scope=\"public\">\n"
	"    <one-of>\n"
	"      <item><tag>0</tag>1</item>\n"
	"      <item><tag>1</tag>5</item>\n"
	"      <item><tag>7</tag>7</item>\n"
	"      <item><tag>3</tag>9</item>\n"
	"      <item><tag>4</tag>715</item>\n"
	"    </one-of>\n"
	"  </rule>\n"
	"</grammar>\n";

static const char *adhearsion_large_menu_grammar =
	"<grammar xmlns=\"http://www.w3.org/2001/06/grammar\" version=\"1.0\" xml:lang=\"en-US\" mode=\"dtmf\" root=\"options\" tag-format=\"semantics/1.0-literals\">"
	"  <rule id=\"options\" scope=\"public\">\n"
	"    <one-of>\n"
	"      <item><tag>0</tag>1</item>\n"
	"      <item><tag>1</tag>5</item>\n"
	"      <item><tag>2</tag>7</item>\n"
	"      <item><tag>3</tag>9</item>\n"
	"      <item><tag>4</tag>715</item>\n"
	"      <item><tag>5</tag>716</item>\n"
	"      <item><tag>6</tag>717</item>\n"
	"      <item><tag>7</tag>718</item>\n"
	"      <item><tag>8</tag>719</item>\n"
	"      <item><tag>9</tag>720</item>\n"
	"      <item><tag>10</tag>721</item>\n"
	"      <item><tag>11</tag>722</item>\n"
	"      <item><tag>12</tag>723</item>\n"
	"      <item><tag>13</tag>724</item>\n"
	"      <item><tag>14</tag>725</item>\n"
	"      <item><tag>15</tag>726</item>\n"
	"      <item><tag>16</tag>727</item>\n"
	"      <item><tag>17</tag>728</item>\n"
	"      <item><tag>18</tag>729</item>\n"
	"      <item><tag>19</tag>730</item>\n"
	"      <item><tag>20</tag>731</item>\n"
	"      <item><tag>21</tag>732</item>\n"
	"      <item><tag>22</tag>733</item>\n"
	"      <item><tag>23</tag>734</item>\n"
	"      <item><tag>24</tag>735</item>\n"
	"      <item><tag>25</tag>736</item>\n"
	"      <item><tag>26</tag>737</item>\n"
	"      <item><tag>27</tag>738</item>\n"
	"      <item><tag>28</tag>739</item>\n"
	"      <item><tag>29</tag>740</item>\n"
	"      <item><tag>30</tag>741</item>\n"
	"      <item><tag>31</tag>742</item>\n"
	"      <item><tag>32</tag>743</item>\n"
	"      <item><tag>33</tag>744</item>\n"
	"      <item><tag>34</tag>745</item>\n"
	"      <item><tag>35</tag>746</item>\n"
	"      <item><tag>36</tag>747</item>\n"
	"      <item><tag>37</tag>748</item>\n"
	"      <item><tag>38</tag>749</item>\n"
	"      <item><tag>39</tag>750</item>\n"
	"      <item><tag>40</tag>751</item>\n"
	"      <item><tag>41</tag>752</item>\n"
	"      <item><tag>42</tag>753</item>\n"
	"      <item><tag>43</tag>754</item>\n"
	"      <item><tag>44</tag>755</item>\n"
	"      <item><tag>45</tag>756</item>\n"
	"      <item><tag>46</tag>757</item>\n"
	"      <item><tag>47</tag>758</item>\n"
	"      <item><tag>48</tag>759</item>\n"
	"      <item><tag>49</tag>760</item>\n"
	"      <item><tag>50</tag>761</item>\n"
	"      <item><tag>51</tag>762</item>\n"
	"      <item><tag>52</tag>763</item>\n"
	"      <item><tag>53</tag>764</item>\n"
	"      <item><tag>54</tag>765</item>\n"
	"      <item><tag>55</tag>766</item>\n"
	"      <item><tag>56</tag>767</item>\n"
	"      <item><tag>57</tag>768</item>\n"
	"      <item><tag>58</tag>769</item>\n"
	"      <item><tag>59</tag>770</item>\n"
	"      <item><tag>60</tag>771</item>\n"
	"      <item><tag>61</tag>772</item>\n"
	"      <item><tag>62</tag>773</item>\n"
	"      <item><tag>63</tag>774</item>\n"
	"      <item><tag>64</tag>775</item>\n"
	"    </one-of>\n"
	"  </rule>\n"
	"</grammar>\n";

static const char *duplicate_tag_grammar =
	"<grammar xmlns=\"http://www.w3.org/2001/06/grammar\" version=\"1.0\" xml:lang=\"en-US\" mode=\"dtmf\" root=\"options\" tag-format=\"semantics/1.0-literals\">"
	"  <rule id=\"options\" scope=\"public\">\n"
	"    <one-of>\n"
	"      <item><tag>2</tag>1</item>\n"
	"      <item><tag>2</tag>5</item>\n"
	"      <item><tag>4</tag>7</item>\n"
	"      <item><tag>4</tag>9</item>\n"
	"    </one-of>\n"
	"  </rule>\n"
	"</grammar>\n";


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


FST_BEGIN()

FST_SUITE_BEGIN(srgs)

FST_SETUP_BEGIN()
{
	fst_requires(srgs_init());
}
FST_SETUP_END()

FST_TEARDOWN_BEGIN()
{
}
FST_TEARDOWN_END()

/**
 * Test matching against adhearsion menu grammar
 */
FST_TEST_BEGIN(match_adhearsion_menu_grammar)
{
	struct srgs_parser *parser;
	struct srgs_grammar *grammar;
	const char *interpretation;

	parser = srgs_parser_new("1234");
	fst_requires(parser);
	fst_requires((grammar = srgs_parse(parser, adhearsion_menu_grammar)));

	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "0", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "1", &interpretation));
	fst_check_string_equals("0", interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "2", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "3", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "4", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "5", &interpretation));
	fst_check_string_equals("1", interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "6", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_MATCH == srgs_grammar_match(grammar, "7", &interpretation));
	fst_check_string_equals("7", interpretation);
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "715", &interpretation));
	fst_check_string_equals("4", interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "8", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "9", &interpretation));
	fst_check_string_equals("3", interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "#", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "*", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "A", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "27", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "223", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "0123456789*#", &interpretation));
	fst_check(NULL == interpretation);

	srgs_parser_destroy(parser);
}
FST_TEST_END()

/**
 * Test matching against adhearsion menu grammar
 */
FST_TEST_BEGIN(match_adhearsion_large_menu_grammar)
{
	struct srgs_parser *parser;
	struct srgs_grammar *grammar;
	const char *interpretation;

	parser = srgs_parser_new("1234");
	fst_requires(parser);
	fst_requires((grammar = srgs_parse(parser, adhearsion_large_menu_grammar)));

	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "0", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "1", &interpretation));
	fst_check_string_equals("0", interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "2", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "3", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "4", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "5", &interpretation));
	fst_check_string_equals("1", interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "6", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_MATCH == srgs_grammar_match(grammar, "7", &interpretation));
	fst_check_string_equals("2", interpretation);
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "715", &interpretation));
	fst_check_string_equals("4", interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "8", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "9", &interpretation));
	fst_check_string_equals("3", interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "#", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "*", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "A", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "27", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "223", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "0123456789*#", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "761", &interpretation));
	fst_check_string_equals("50", interpretation);

	srgs_parser_destroy(parser);
}
FST_TEST_END()


/**
 * Test matching with duplicate tags
 */
FST_TEST_BEGIN(match_duplicate_tag_grammar)
{
	struct srgs_parser *parser;
	struct srgs_grammar *grammar;
	const char *interpretation;

	parser = srgs_parser_new("1234");
	fst_requires(parser);
	fst_requires((grammar = srgs_parse(parser, duplicate_tag_grammar)));

	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "0", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "1", &interpretation));
	fst_check_string_equals("2", interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "2", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "3", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "4", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "5", &interpretation));
	fst_check_string_equals("2", interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "6", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "7", &interpretation));
	fst_check_string_equals("4", interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "8", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "9", &interpretation));
	fst_check_string_equals("4", interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "#", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "*", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "A", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "27", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "223", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "0123456789*#", &interpretation));
	fst_check(NULL == interpretation);

	srgs_parser_destroy(parser);
}
FST_TEST_END()



/**
 * Test matching against adhearsion ask grammar
 */
FST_TEST_BEGIN(match_adhearsion_ask_grammar)
{
	struct srgs_parser *parser;
	struct srgs_grammar *grammar;
	const char *interpretation;

	parser = srgs_parser_new("1234");
	fst_requires(parser);
	fst_requires((grammar = srgs_parse(parser, adhearsion_ask_grammar)));

	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "0", &interpretation));
	fst_check(NULL == interpretation);
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "1", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "2", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "3", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "4", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "5", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "6", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "7", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "8", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "9", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "#", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "*", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "A", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "27", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "223", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "0123456789*#", &interpretation));

	srgs_parser_destroy(parser);
}
FST_TEST_END()


/**
 * Test matching against grammar with multiple digits per item
 */
FST_TEST_BEGIN(match_multi_digit_grammar)
{
	struct srgs_parser *parser;
	struct srgs_grammar *grammar;
	const char *interpretation;

	parser = srgs_parser_new("1234");
	fst_requires(parser);
	fst_requires((grammar = srgs_parse(parser, multi_digit_grammar)));

	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "0", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "1", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "2", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "3", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "4", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "5", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "6", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "7", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "8", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "9", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "#", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "*", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "A", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "27", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "223", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "0123456789*#", &interpretation));

	srgs_parser_destroy(parser);
}
FST_TEST_END()


FST_TEST_BEGIN(match_multi_rule_grammar)
{
	struct srgs_parser *parser;
	struct srgs_grammar *grammar;
	const char *interpretation;

	parser = srgs_parser_new("1234");
	fst_requires(parser);
	fst_requires((grammar = srgs_parse(parser, multi_rule_grammar)));

	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "0", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "1", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "2", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "3", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "4", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "5", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "6", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "7", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "8", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "9", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "#", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "*", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "A", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "27", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "223", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "0123456789*#", &interpretation));

	srgs_parser_destroy(parser);
}
FST_TEST_END()


FST_TEST_BEGIN(match_rayo_example_grammar)
{
	struct srgs_parser *parser;
	struct srgs_grammar *grammar;
	const char *interpretation;

	parser = srgs_parser_new("1234");
	fst_requires(parser);
	fst_requires((grammar = srgs_parse(parser, rayo_example_grammar)));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "0", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "1", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "2", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "3", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "4", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "5", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "6", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "7", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "8", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "9", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "#", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "*", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "A", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "*9", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "1234#", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "2321#", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "27", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "223", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "0123456789*#", &interpretation));

	srgs_parser_destroy(parser);
}
FST_TEST_END()



FST_TEST_BEGIN(parse_grammar)
{
	struct srgs_parser *parser;

	parser = srgs_parser_new("1234");
	fst_requires(parser);

	fst_check(srgs_parse(parser, adhearsion_ask_grammar));
	fst_check(NULL == srgs_parse(parser, adhearsion_ask_grammar_bad));
	fst_check(NULL == srgs_parse(parser, NULL));
	fst_check(NULL == srgs_parse(NULL, adhearsion_ask_grammar));
	fst_check(NULL == srgs_parse(NULL, adhearsion_ask_grammar_bad));
	fst_check(NULL == srgs_parse(parser, bad_ref_grammar));

	srgs_parser_destroy(parser);
}
FST_TEST_END()

FST_TEST_BEGIN(repeat_item_grammar)
{
	struct srgs_parser *parser;
	struct srgs_grammar *grammar;
	const char *interpretation;

	parser = srgs_parser_new("1234");
	fst_requires(parser);
	fst_check(NULL == srgs_parse(parser, repeat_item_grammar_bad));
	fst_check(NULL == srgs_parse(parser, repeat_item_grammar_bad2));
	fst_check(NULL == srgs_parse(parser, repeat_item_grammar_bad3));
	fst_check(NULL == srgs_parse(parser, repeat_item_grammar_bad4));
	fst_check(NULL == srgs_parse(parser, repeat_item_grammar_bad5));
	fst_check(NULL == srgs_parse(parser, repeat_item_grammar_bad6));
	fst_requires((grammar = srgs_parse(parser, repeat_item_grammar)));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "1111#", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "1111", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "1234#", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "1234", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "11115#", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "11115", &interpretation));
	fst_requires((grammar = srgs_parse(parser, repeat_item_range_grammar)));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "1111#", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "1111", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "1234#", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "1234", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "11115#", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "11115", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "111156#", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "111156", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "1111567#", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "1111567", &interpretation));
	fst_requires((grammar = srgs_parse(parser, repeat_item_optional_grammar)));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "1111#", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "1111", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "1234#", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "1234", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "11115#", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "11115", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "111156#", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "111156", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "1111567#", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "1111567", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "1#", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "1", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "#", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "A#", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "A", &interpretation));
	fst_requires((grammar = srgs_parse(parser, repeat_item_plus_grammar)));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "1111#", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "1111", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "1234#", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "1234", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "11115#", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "11115", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "111156#", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "111156", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "111157#", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "111157", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "1#", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "1", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "#", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "A#", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "A", &interpretation));
	fst_requires((grammar = srgs_parse(parser, repeat_item_star_grammar)));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "1111#", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "1111", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "1234#", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "1234", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "11115#", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "11115", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "111156#", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "111156", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "111157#", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "111157", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "1#", &interpretation));
	fst_check(SMT_MATCH_PARTIAL == srgs_grammar_match(grammar, "1", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "#", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "A#", &interpretation));
	fst_check(SMT_NO_MATCH == srgs_grammar_match(grammar, "A", &interpretation));

	srgs_parser_destroy(parser);
}
FST_TEST_END()


FST_TEST_BEGIN(repeat_item_range_ambiguous_grammar)
{
	struct srgs_parser *parser;
	struct srgs_grammar *grammar;
	const char *interpretation;

	parser = srgs_parser_new("1234");
	fst_requires(parser);
	fst_requires((grammar = srgs_parse(parser, repeat_item_range_ambiguous_grammar)));
	fst_check(SMT_MATCH == srgs_grammar_match(grammar, "1", &interpretation));
	fst_check(SMT_MATCH == srgs_grammar_match(grammar, "12", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "123", &interpretation));
}
FST_TEST_END()

FST_TEST_BEGIN(repeat_item_range_optional_pound_grammar)
{
	struct srgs_parser *parser;
	struct srgs_grammar *grammar;
	const char *interpretation;

	parser = srgs_parser_new("1234");
	fst_requires(parser);
	fst_requires((grammar = srgs_parse(parser, repeat_item_range_optional_pound_grammar)));
	fst_check(SMT_MATCH == srgs_grammar_match(grammar, "1", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "1#", &interpretation));
	fst_check(SMT_MATCH == srgs_grammar_match(grammar, "12", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "12#", &interpretation));
	fst_check(SMT_MATCH_END == srgs_grammar_match(grammar, "123", &interpretation));
}
FST_TEST_END()


FST_TEST_BEGIN(jsgf)
{
	struct srgs_parser *parser;
	struct srgs_grammar *grammar;
	const char *jsgf;
	parser = srgs_parser_new("1234");
	fst_requires(parser);

	fst_requires((grammar = srgs_parse(parser, adhearsion_ask_grammar)));
	fst_check((jsgf = srgs_grammar_to_jsgf(grammar)));
	fst_requires((grammar = srgs_parse(parser, voice_srgs1)));
	fst_check((jsgf = srgs_grammar_to_jsgf(grammar)));
	fst_check_string_equals(voice_jsgf, jsgf);
	fst_requires((grammar = srgs_parse(parser, multi_rule_grammar)));
	fst_check((jsgf = srgs_grammar_to_jsgf(grammar)));
	fst_requires((grammar = srgs_parse(parser, rayo_test_srgs)));
	fst_check((jsgf = srgs_grammar_to_jsgf(grammar)));
	fst_check(NULL == srgs_grammar_to_jsgf(NULL));
	srgs_parser_destroy(parser);
}
FST_TEST_END()


FST_TEST_BEGIN(w3c_example_grammar)
{
	struct srgs_parser *parser;
	struct srgs_grammar *grammar;
	parser = srgs_parser_new("1234");
	fst_requires(parser);

	fst_requires((grammar = srgs_parse(parser, w3c_example_grammar)));
	fst_check(srgs_grammar_to_jsgf(grammar));
}
FST_TEST_END()


FST_TEST_BEGIN(metadata_grammar)
{
	struct srgs_parser *parser;
	struct srgs_grammar *grammar;
	parser = srgs_parser_new("1234");
	fst_requires(parser);

	fst_requires((grammar = srgs_parse(parser, metadata_grammar)));
	fst_check(srgs_grammar_to_jsgf(grammar));
}
FST_TEST_END()

FST_SUITE_END()

FST_END()

