function all_done(s, how)
   io.write("done: " .. how .. "\n");
end

function my_cb(s, type, obj, arg)
   if (arg) then
      io.write("type: " .. type .. "\n" .. "arg: " .. arg .. "\n");
   else
      io.write("type: " .. type .. "\n");
   end

   if (type == "dtmf") then
      io.write("digit: [" .. obj['digit'] .. "]\nduration: [" .. obj['duration'] .. "]\n"); 

      if (obj['digit'] == "1") then
         return "pause";
      end

      if (obj['digit'] == "2") then
         return "seek:+3000";
      end

      if (obj['digit'] == "3") then
         return "seek:-3000";
      end

      if (obj['digit'] == "4") then
         return "seek:+3000";
      end

      if (obj['digit'] == "5") then
         return "speed:+1";
      end
      if (obj['digit'] == "6") then
         return "speed:0";
      end
      if (obj['digit'] == "7") then
         return "speed:-1";
      end

      if (obj['digit'] == "8") then
         return "stop";
      end

      if (obj['digit'] == "9") then
         return "break";
      end
   else
      io.write(obj:serialize("xml"));

   end
end

blah = "args";
session:setHangupHook("all_done");
session:setInputCallback("my_cb", "blah");
session:streamFile("/tmp/swimp.raw");
