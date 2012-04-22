stream:write("Content-Type: text/html\n\n");
stream:write("<title>FreeSWITCH Command Portal</title>");
stream:write("<h2>FreeSWITCH Command Portal</h2>");
stream:write("<form method=post><input name=command size=40> ");
stream:write("<input type=submit value=\"Execute\">");
stream:write("</form><hr noshade size=1><br>");

command = env:getHeader("command");

if (command)  then
   api = freeswitch.API();
   reply = api:executeString(command);

   if (reply) then
      stream:write("<br><B>Command Result</b><br><pre>" .. reply .. "\n</pre>");
   end

end

env:addHeader("cool", "true");
stream:write("<pre>" .. env:serialize() .. "</pre>");



