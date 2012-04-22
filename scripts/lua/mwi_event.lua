-- This is an example of sending an event via luarun from the cli
-- Edit to your liking.  luarun mwi_event.lua
freeswitch.console_log("info", "Lua in da house!!!\n");

local event = freeswitch.Event("message_waiting");
event:addHeader("MWI-Messages-Waiting", "no");
event:addHeader("MWI-Message-Account", "sip:1000@10.0.1.100");
-- event:addHeader("Sofia-Profile", "internal");
event:fire();
