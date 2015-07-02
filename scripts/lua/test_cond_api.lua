-- Test various cond api to verify consistency.
-- string compared to string (eg. 'string1' == 'string2') compares by value
-- string compared to number (eg. 'string' == 5) compares its string length
-- string compared to any type with 'lt(<) gt(>) ge(>=) le(<=)' operators are compared to their legnth (even if both values are strings).
-- number compared to number works just like you'd expect :)

-- only print failed tests
local FAILED_ONLY = true;

local ops = {"==", "!=", ">=",  "<=",  ">",  "<"};
local tests = {
   -- cmp value,                 expected results for each operator, MAPPINGS[idx] 
   { {"1", "1"},                {1, 2, 1, 1, 2, 2}, 'test 2 equal integers' },
   { {"1", "2"},                {2, 1, 2, 1, 2, 1}, 'test 2 non equal integers' },
   { {"1.000001", "1.000001"},  {1, 2, 1, 1, 2, 2}, 'test 2 equal float' },
   { {"1.000001", "1.000002"},  {2, 1, 2, 1, 2, 1}, 'test 2 non equal float' },
   { {"'hello'", "'hello'"},    {1, 2, 1, 1, 2, 2}, 'test 2 equal quoted strings' },
   { {"hello", "hello"},        {1, 2, 1, 1, 2, 2}, 'test 2 equal unquoted strings' },
   { {"hello", "HELLO"},        {2, 1, 1, 1, 2, 2}, 'test 2 non equal unquoted strings' },
   { {"hello", "5"},            {1, 2, 1, 1, 2, 2}, 'test lenght of unquoted string with equal number' },
   { {"'hello'", "5"},          {1, 2, 1, 1, 2, 2}, 'test length of quoted string with equal number' },
   { {"' hello'", "5"},         {2, 1, 1, 2, 1, 2}, 'test length of quoted string includes preceding space' },
   { {" hello", "5"},           {1, 2, 1, 1, 2, 2}, 'test length of unquoted string excludes preceding space' },
   { {"'hello'", "6"},          {2, 1, 2, 1, 2, 1}, 'test length of quoted string is against non equal number' },
   { {"'01'", "01"},            {2, 1, 1, 2, 1, 2}, 'test number quoted (as string) against number' },
   { {"''", "''"},              {1, 2, 1, 1, 2, 2}, 'test quoted empty strings' },
   { {"' '", "''"},             {2, 1, 1, 2, 1, 2}, 'test quoted space against empty string' },
   { {"", " "},                 {3, 3, 3, 3, 3, 3},    'test unquoted empty values returns ERR' },
   
   { {"'Isn\\'t it \"great\"?!\\t'", "'Isn\\'t it \"great\"?!\\t'"}, {1, 2, 1, 1, 2, 2}, 'test quoted string with special escaped chars' },
   { {"'Isn't it \"great\"?!\\t'", "'Isn't it \"great\"?!\\t'"}, {3, 3, 3, 3, 3, 3}, 'test quoted string with unescaped single quote returns ERR' },
};

stream:write("Testing cond api\n");

local commands = {
   -- command,		description,		truth val, false val, err val
   {" ? true : false", "command with spaces", {"true", "false", "-ERR"}},
   {" ? true:false", "command without spaces", {"true", "false", "-ERR"}},
   {" ? true :", "command with missing ternary false value", {"true", "", "-ERR"}},
   {"?true:false", "command with no spaces between values", {"-ERR", "-ERR", "-ERR"}},
}

local num_tests=0;
local num_passed=0;

local api = freeswitch.API();

-- do for each command
for _, cmd in pairs(commands) do
   for ti, test in pairs(tests) do
      if (not FAILED_ONLY) then
	 stream:write(string.format("\nTesting #[%d]: `%s` (%s)\n", ti, test[3], cmd[2]));
      end
      for i , op in pairs(ops) do
	 command = "cond " .. test[1][1] .. " " .. op .. " " .. test[1][2] .. cmd[1];
	 reply = api:executeString(command);
	 expected = cmd[3][test[2][i]];
	 if (reply ~= nil) then
	    passed = (reply == expected);
	    if (passed) then
	       num_passed=num_passed+1;
	       pass_text = "PASSED";
	    else
	       pass_text = "FAILED"
	    end
	    -- print runned test
	    if (not FAILED_ONLY or not passed) then
	       stream:write(string.format("%s:\tTest #[%d]: [%s (%s)] \t--- expected: [%s], actual: [%s]\n", pass_text, ti, command, cmd[2], expected, reply));
	    end
	 else
	    stream:write("FAILED!\t" .. command .. "\n");
	 end
	 num_tests=num_tests+1;
      end
   end
end

stream:write(string.format("\nRAN: [%d], PASSED: [%d], FAILED: [%d]", num_tests, num_passed, num_tests-num_passed));
