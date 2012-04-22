function all_done(s, how)
   freeswitch.console_log("info", "done: " .. how .. "\n");
end

function my_cb(s, type, obj, arg)
   if (arg) then
      freeswitch.console_log("info", "\ntype: " .. type .. "\n" .. "arg: " .. arg .. "\n");
   else
      freeswitch.console_log("info", "\ntype: " .. type .. "\n");
   end

   if (type == "dtmf") then
      freeswitch.console_log("info", "\ndigit: [" .. obj['digit'] .. "]\nduration: [" .. obj['duration'] .. "]\n"); 


      if (obj['digit'] == "1") then
	 --session:speak("seek backwards");
         return "seek:-9000";
      end

      if (obj['digit'] == "2") then
	 --session:speak("start over");
         return "seek:0";
      end

      if (obj['digit'] == "3") then
	 --session:speak("seek forward");
         return "seek:+9000";
      end

      if (obj['digit'] == "4") then
	 --session:speak("speed faster");
         return "speed:+1";
      end

      if (obj['digit'] == "5") then
	 --session:speak("speed normal");
         return "speed:0";
      end

      if (obj['digit'] == "6") then
	 --session:speak("speed slower");
         return "speed:-1";
      end

      if (obj['digit'] == "7") then
	 --session:speak("volume up");
         return "volume:+1";
      end

      if (obj['digit'] == "8") then
	 --session:speak("volume normal");
         return "volume:0";
      end

      if (obj['digit'] == "9") then
	 --session:speak("volume down");
         return "volume:-1";
      end

      if (obj['digit'] == "*") then
	 --session:speak("stop");
         return "stop";
      end

      if (obj['digit'] == "0") then
	 --session:speak("pause");
         return "pause";
      end

      if (obj['digit'] == "#") then
         return "break";
      end

   else
      freeswitch.console_log("info", obj:serialize("xml"));

   end
end

blah = "args";
---session:set_tts_parms("flite", "kal");
session:setHangupHook("all_done");
session:setInputCallback("my_cb", "blah");
session:streamFile("/ram/swimp.raw");
--session:speak("Thank you, good bye!");
