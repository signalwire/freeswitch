-- This is an example of sending an event via luarun from the cli
-- Edit to your liking.  luarun mwi_event.lua
freeswitch.console_log("info", "Sending MWI Event using Lua\n");

local event = freeswitch.Event("message_waiting");
event:add_header("MWI-Messages-Waiting", "no");
event:add_header("MWI-Message-Account", "sip:1002@10.0.1.100");
event:fire();
