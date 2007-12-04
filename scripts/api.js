/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 *
 * api.js Demo javascript FSAPI Interface
 *
 * To use this script:
 * 1) Put it in $prefix/scripts. (eg /usr/local/freeswitch/scripts)
 * 2) Load mod_xml_rpc and point a browser to your FreeSWITCH machine.
 *    http://your.freeswitch.box:8080/api/jsapi?api.js
 */

/* Other possible js commands */
//env = request.dumpENV("text");
//xmlenv = new XML(request.dumpENV("xml"));
//request.addHeader("js-text", "You were in a javascript script");


if (session) {
	request.write("Don't call me from the dialplan silly! I'm a web interface today.\n");
	consoleLog("err", "Invalid usage!\n");
	exit();
}

request.write("Content-Type: text/html\n\n");
request.write("<title>FreeSWITCH Command Portal</title>");
request.write("<h2>FreeSWITCH Command Portal</h2>");
request.write("<form method=post><input name=command size=40> ");
request.write("<input type=submit value=\"Execute\">");
request.write("</form><hr noshade size=1><br>");

if ((command = request.getHeader("command"))) {
	cmd_list = command.split(" ");
    cmd = cmd_list.shift();
    args = cmd_list.join(" ");

	if ((reply = apiExecute(cmd, args))) {
		request.write("<br><B>Command Result</b><br><pre>" + reply + "\n</pre>");
	}
}


