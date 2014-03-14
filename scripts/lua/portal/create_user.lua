user_id = argv[1];
if (user_id == nil or user_id == "") then os.exit() end

conf = "/usr/local/freeswitch/conf"
dir  = conf .. "/directory/default"
template = dir .. "/1001.xml"
dest     = dir .. "/" .. user_id .. ".xml"

template_file = io.open(template, "r")
dest_file = io.open(dest, "a+")
print(dest_file)
while true do
	line = template_file:read("*line")
    if line == nil then break end
    line = line:gsub("1001", user_id)
    print(line)
    dest_file:write(line .. "\n")
end

api = freeswitch.API()
api:execute("reloadxml")
