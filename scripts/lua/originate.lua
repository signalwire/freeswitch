-- Example Lua script to originate. luarun
freeswitch.console_log("info", "Lua in da house!!!\n");

local session = freeswitch.Session("sofia/10.0.1.100/1001");
session:execute("playback", "/sr8k.wav");
session:hangup();
