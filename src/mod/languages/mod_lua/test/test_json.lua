function serialize(o)
	s = ""

	if type(o) == "number" then
		s = s .. o
	elseif type(o) == "string" then
		s = s .. string.format("%q", o)
	elseif type(o) == "table" then
		s = s .. "{\n"
		for k, v in pairs(o) do
			s = s .. '  ' .. k .. ' = '
			s = s .. serialize(v)
			s = s .. ",\n"
		end
		s = s .. "}"
	elseif type(o) == "boolean" then
		if o then
			s = s .. "true"
		else
			s = s .. "false"
		end
	else
		s = s .. " [" .. type(o) .. "]"
	end

	return s
end

json = freeswitch.JSON()


str = '{"a": "中文"}'
x = json:decode(str)
assert(x.a == '中文')

str = '{"a": "1", "b": 2, "c": true, "d": false, "e": [], "f": {}, "g": [1, 2, "3"], "h": {"a": 1, "b": 2}}'
x = json:decode(str)

freeswitch.consoleLog("INFO", serialize(x) .. "\n")
freeswitch.consoleLog("INFO", json:encode(x) .. '\n')

assert(x.a == "1")
assert(x.b == 2)

x = json:decode('["a", "b", true, false, null]')
freeswitch.consoleLog("INFO", serialize(x) .. "\n")

assert(x[1] == "a")

x = json:decode('[]')
assert(x)
x = json:decode('{}')
assert(x)
x = json:decode('blah')
assert(x == nil)

s = json:encode({hello = "blah", seven="7", aa = {bb = "cc", ee="ff", more = {deep = "yes"}}, last="last", empty={}})
freeswitch.consoleLog("INFO", s .. "\n")

s = json:encode({"a", "b", "c"})
freeswitch.consoleLog("INFO", s .. "\n")

s = json:encode({a = 1, b = 2, c = 3, d=true, e=false, f=nil})
freeswitch.consoleLog("INFO", s .. "\n")

json:return_unformatted_json(true);
s = json:encode({})
freeswitch.consoleLog("INFO", s .. "\n")
assert(s == "{}")

json:encode_empty_table_as_object(false);
s = json:encode({})
freeswitch.consoleLog("INFO", s .. "\n")
assert(s == "[]")

s = json:encode({[1] = "a"})
freeswitch.consoleLog("INFO", s .. "\n")
assert(s == '["a"]')

s = json:encode({"a", "b", "c"})
freeswitch.consoleLog("INFO", s .. "\n")
assert(s == '["a","b","c"]')

-- sparse
s = json:encode({[3] = "c"})
freeswitch.consoleLog("INFO", s .. "\n")
assert(s == '{"3":"c"}')

s = json:encode({{name = "seven"}, {name="nine"}})
freeswitch.consoleLog("INFO", s .. "\n")
assert(s == '[{"name":"seven"},{"name":"nine"}]')

s = json:encode({{name = "中文"}, {["中文"]="也行"}})
freeswitch.consoleLog("INFO", s .. "\n")
assert(s == '[{"name":"中文"},{"中文":"也行"}]')

json:encode_empty_table_as_object(true);
cmd = {command="status", data={}}
ret = json:execute(cmd)
freeswitch.consoleLog("INFO", serialize(ret) .. "\n")

ret = json:execute(json:encode(cmd))
freeswitch.consoleLog("INFO", serialize(ret) .. "\n")

ret = json:execute2(cmd)
freeswitch.consoleLog("INFO", ret .. "\n")

ret = json:execute2(json:encode(cmd))
freeswitch.consoleLog("INFO", ret .. "\n")

-- assert(false)
stream:write("+OK")
