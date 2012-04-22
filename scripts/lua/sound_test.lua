--
-- sound_test.lua
--
-- accepts two args and then rolls through the sound files
-- arg 1: type 
-- arg 2: rate
--  
--[[    Use a dialplan entry like this:
  
<extension name="sound file tester">
  <condition field="destination_number" expression="^((8|16|32|48)000)(.*)$">
    <action application="lua" data="sound_test.lua $3 $1"/>
    <action application="hangup"/>
  </condition>
</extension>

Note the syntax of the destination number: <rate><type>
Rate can be 8000, 16000, 32000, or 48000
Type can be ivr, conference, voicemail, misc, digits, etc.

Using the extension listed above you could call it with mod_portaudio from fs_cli:

 pa call 16000ivr
 pa call 8000conference
 pa call 32000conference

 etc.

]]

-- Create tables that hold our rates and types

tbl_types = {
		['ascii'] = 1,
	    ['base256'] = 1,
	    ['conference'] = 1,
	    ['currency'] = 1,
	    ['digits'] = 1,
	    ['directory'] = 1,
	    ['ivr'] = 1,
	    ['misc'] = 1,
	    ['phonetic-ascii'] = 1,
	    ['time'] = 1,
	    ['voicemail'] = 1,
	    ['zrtp'] = 1
};

tbl_rates = {['8000'] = 1 ,['16000'] = 1, ['32000'] = 1, ['48000'] = 1};

stype = argv[1];
srate = argv[2];

freeswitch.consoleLog("INFO","Args: Type = " .. argv[1] .. ', Rate = ' .. argv[2] .. "\n");

if ( tbl_types[stype] == nil ) then 
	freeswitch.consoleLog("ERR","Type '" .. stype .. "' is not valid.\n");
elseif ( tbl_rates[srate] == nil ) then 
	freeswitch.consoleLog("ERR","Rate '" .. srate .. "' is not valid.\n");
else 
	-- Looks good, let's play some sound files
	sound_base = session:getVariable('sounds_dir') .. '/en/us/callie/' .. stype .. '/' .. srate;
	input_file = '/tmp/filez.txt';
	res = os.execute('ls -1 ' .. sound_base  .. ' > ' .. input_file);
	freeswitch.consoleLog("INFO","Result of system call: " .. res .. "\n");
	if ( res == 0 ) then
		for fname in io.lines(input_file) do
			freeswitch.consoleLog("NOTICE","Playing file: " .. fname .. "\n");
			session:streamFile(sound_base .. '/' .. fname);
			session:sleep(100);
		end
	else
		freeswitch.consoleLog("ERR","Result of system call: " .. res .. "\n");			
	end
end