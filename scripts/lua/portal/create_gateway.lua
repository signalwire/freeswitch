name = argv[1];
realm = argv[2];
username = argv[3];
password = argv[4];
register = argv[5];

conf = "/usr/local/freeswitch/conf"
dir  = conf .. "/sip_profiles/external/"
file_name = dir .. "/" .. name .. ".xml"

conf_file = io.open(file_name, "w+")

conf_file:write('<include>\n')
conf_file:write('	<gateway name="' .. name .. '">\n')
conf_file:write('		<param name="realm" value="' .. realm .. '"/>\n')
conf_file:write('		<param name="username" value="' .. username .. '"/>\n')
conf_file:write('		<param name="password" value="' .. password .. '"/>\n')
conf_file:write('		<param name="register" value="' .. register .. '"/>\n')
conf_file:write('	</gateway>\n')
conf_file:write('<include>\n')

api = freeswitch.API()
api:execute("sofia profile external rescan")
