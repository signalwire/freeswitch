
local count = 0;
local tddstring = {};


function my_cb(s, type, obj, arg)
   if (arg) then
      freeswitch.console_log("info", "\ntype: " .. type .. "\n" .. "arg: " .. arg .. "\n");
   else
      freeswitch.console_log("info", "\ntype: " .. type .. "\n");
   end


   tdddata = obj:getHeader("TDD-Data");
   count = 0;
   table.insert(tddstring, tdddata);
   
   
   freeswitch.console_log("info", obj:serialize("xml"));
end

function all_done(s, how)
   freeswitch.console_log("info", "done: " .. how .. "\n");
end

function tablelength(T)
   local count = 0
   for _ in pairs(T) do count = count + 1 end
   return count
end


blah = "args";
session:setHangupHook("all_done");
session:setInputCallback("my_cb", "blah");
session:answer();

session:execute("playback", "silence_stream://2000");
session:execute("spandsp_detect_tdd");
session:execute("spandsp_send_tdd", "Welcome to FreeSWITCH");

while session:ready() do
   session:streamFile("silence_stream://10000");
   if (count > 0) then
      count = 0;
      if (tablelength(tddstring) > 0) then
	 session:execute("spandsp_send_tdd", "You said: " .. table.concat(tddstring));
	 tddstring = {};
      end
   end
   count = count + 1;
end
