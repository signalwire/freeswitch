--[[
FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
Copyright (C) 2005/2006, Anthony Minessale II <anthm@freeswitch.org>

Version: MPL 1.1

The contents of this file are subject to the Mozilla Public License Version
1.1 (the "License"); you may not use this file except in compliance with
the License. You may obtain a copy of the License at
http://www.mozilla.org/MPL/

   Software distributed under the License is distributed on an "AS IS" basis,
WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
for the specific language governing rights and limitations under the
License.

The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application

The Initial Developer of the Original Code is
Anthony Minessale II <anthm@freeswitch.org>
   Portions created by the Initial Developer are Copyright (C)
the Initial Developer. All Rights Reserved.

Contributor(s):

Brian West <brian@freeswitch.org>

   Example for Speech Enabled LUA Applications.
]]

-- Used in parse_xml
function parseargs_xml(s)
   local arg = {}
   string.gsub(s, "(%w+)=([\"'])(.-)%2", function (w, _, a)
					    arg[w] = a
					 end)
   return arg
end

-- Turns XML into a lua table.
function parse_xml(s)
   local stack = {};
   local top = {};
   table.insert(stack, top);
   local ni,c,label,xarg, empty;
   local i, j = 1, 1;
   while true do
      ni,j,c,label,xarg, empty = string.find(s, "<(%/?)(%w+)(.-)(%/?)>", i);
      if not ni then
	 break
      end
      local text = string.sub(s, i, ni-1);
      if not string.find(text, "^%s*$") then
	 table.insert(top, text);
      end
      if empty == "/" then
	 table.insert(top, {label=label, xarg=parseargs_xml(xarg), empty=1});
      elseif c == "" then
	 top = {label=label, xarg=parseargs_xml(xarg)};
	 table.insert(stack, top);
      else
	 local toclose = table.remove(stack);
	 top = stack[#stack];
	 if #stack < 1 then
	    error("nothing to close with "..label);
	 end
	 if toclose.label ~= label then
	    error("trying to close "..toclose.label.." with "..label);
	 end
	 table.insert(top, toclose);
      end
      i = j+1;
   end
   local text = string.sub(s, i);
   if not string.find(text, "^%s*$") then
      table.insert(stack[stack.n], text);
   end
   if #stack > 1 then
      error("unclosed "..stack[stack.n].label);
   end
   return stack[1];
end

function dump(o)
   if type(o) == 'table' then
      local s = '{ '
      for k,v in pairs(o) do
	 if type(k) ~= 'number' then k = '"'..k..'"' end
	 s = s .. '['..k..'] = ' .. dump(v) .. ','
      end
      return s .. '} '
   else
      return tostring(o)
   end
end

-- Used to parse the XML results.
function getResults(s) 
   local xml = parse_xml(s);
   local stack = {}
   local top = {}

   -- freeswitch.consoleLog("crit", "\n" .. dump(xml) .. "\n");
   table.insert(stack, top)
   top = {grammar=xml[2].xarg.grammar, score=xml[2].xarg.confidence, text=xml[2][1][1][1]}
   table.insert(stack, top)
   return top;
end

-- This is the input callback used by dtmf or any other events on this session such as ASR.
function onInput(s, type, obj)
   freeswitch.consoleLog("info", "Callback with type " .. type .. "\n");
   if (type == "dtmf") then
      freeswitch.consoleLog("info", "DTMF Digit: " .. obj.digit .. "\n");
   else if (type == "event") then
	 local event = obj:getHeader("Speech-Type");
	 if (event == "begin-speaking") then
	    freeswitch.consoleLog("info", "\n" .. obj:serialize() .. "\n");
	    -- Return break on begin-speaking events to stop playback of the fire or tts.
	    return "break";
	 end
	 if (event == "detected-speech") then
	    freeswitch.consoleLog("info", "\n" .. obj:serialize() .. "\n");
	    if (obj:getBody()) then
	       -- Pause speech detection (this is on auto but pausing it just in case)
	       session:execute("detect_speech", "pause");
	       -- Parse the results from the event into the results table for later use.
	       results = getResults(obj:getBody());
	    end
	    return "break";
	 end
      end
   end
end


--Used to map returned names to extension numbers
extensions = {
   ["anthony"] = 3000,
   ["michael"] = 3001,
   ["brian"] = 3002
}

-- Create the empty results table.
results = {};
-- Answer the call.
session:answer();
-- Define TTS Engine
session:set_tts_params("flite", "slt");
-- Register the input callback 
session:setInputCallback("onInput");
-- Sleep a little bit to give media time to be fully up.
session:sleep(200);
session:speak("Welcome to the directory.");
-- Start the detect_speech app.  This attaches the bug to fire events
session:execute("detect_speech", "pocketsphinx directory directory");   

-- Magic happens here.
-- It would be ok to loop like 3 times and error to the operator if this doesn't work or revert to reading names off with TTS.
while (session:ready() == true) do 
   session:sleep(100);
   -- Who are they looking for?
   session:speak("Say the name of the person you're trying to reach.");
   -- This sleep is what blocks till the detected-speech event.  This has to give you enough time to speak plus get the results.
   session:sleep(3000);
   session:sleep(3000);
   -- If the results aren't null and we have an extension in the table.
   if (results.text ~= nil and extensions[results.text] ~= nil) then
      -- Letting the caller know we are trying.
      session:speak("Please hold while I transfer your call.");
      -- It's critical to stop the detect_detect otherwise it will continue to fire speech events and waste resources.
      session:execute("detect_speech", "stop"); 
      -- Transfer the call to the extension out of the lua table.
      session:execute("transfer", extensions[results.text] .. " XML default");
   end
   -- We didn't have them in our directory table.
   session:speak("Sorry, I don't have that person listed, please try again.");
   -- Clear any results we have just in case.
   results = {};
   -- Resume detect_speech.
   session:execute("detect_speech", "resume");
end
