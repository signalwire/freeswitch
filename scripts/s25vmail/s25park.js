/*
 *  File:    s25park.js
 *  Purpose: Implement AT&T System 25 PBX style parking.
 *  Machine:                      OS:
 *  Author:  John Wehle           Date: June 9, 2009
 */

/*
 *  Copyright (c)  2009  Feith Systems and Software, Inc.
 *                      All Rights Reserved
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/* RE to sanity check that the caller id is a valid extension */
var extRE = /^[0-9]{3,4}$/g;


var dtmf_digits;

function on_dtmf (session, type, obj, arg)
  {

  if (type == "dtmf") {
    dtmf_digits += obj.digit;
    return false;
    }

  return true;
  }


function normalize_channel_name (name, direction, ip_addr)
  {
  var re = /^sofia\//g;
  var length = name.search (re);
  var new_name = name;
  
  if (length == -1)
    return new_name;

  if (direction == "inbound") {
    re = /@.*$/g;

    new_name = name.replace (re, "@" + ip_addr);
    }
  else if (direction == "outbound") {
    re = /\/sip:(.*@[^:]*):.*$/g;

    new_name = name.replace (re, "/$1");
    }

  return new_name;
  }


session.answer ();

session.execute ("sleep", "1000");

/*
 * Figure out the normalized form of the requester's channel name.
 */

var requester_channel_name = normalize_channel_name (
  session.getVariable ("channel_name"), "inbound",
  session.getVariable ("network_addr"));

/*
 * Find the uuid for a call on the requester's phone.
 */

var channels = apiExecute ("show", "channels as xml");
var re = /\s+$/g;
var length = channels.search (re);

if (length == -1)
  length = channels.length;

channels = channels.substring (0, length);

var xchannels = new XML (channels);
var our_uuid = session.getVariable ("uuid");
var requester_uuid = "";

for each (var channel in xchannels.row) {
  if (channel.uuid.toString () == our_uuid)
    continue;

  var channel_name =  normalize_channel_name (channel.name.toString (),
             channel.direction.toString (), channel.ip_addr.toString ());

  if (channel_name == requester_channel_name) {
    requester_uuid = channel.uuid.toString ();
    break;
    }
  }

if (requester_uuid == "") {
  session.sayPhrase ("voicemail_invalid_extension", "#", "", on_dtmf, "");
  session.hangup ();
  exit ();
  }

/*
 * Find the peer uuid.
 */

var udump = apiExecute ("uuid_dump", requester_uuid + " xml");
var re = /\s+$/g;
var length = udump.search (re);

if (length == -1)
  length = udump.length;

udump = udump.substring (0, length);

var xudump = new XML (udump);
var uuid = xudump.headers['Other-Leg-Unique-ID'].toString ();

if (uuid == "") {
  session.sayPhrase ("voicemail_invalid_extension", "#", "", on_dtmf, "");
  session.hangup ();
  exit ();
  }

var requester_id_number = session.getVariable ("caller_id_number");

if (requester_id_number.search (extRE) == -1) {
  session.sayPhrase ("voicemail_invalid_extension", "#", "", on_dtmf, "");
  session.hangup ();
  exit ();
  }

apiExecute ("uuid_setvar", uuid + " hangup_after_bridge false");
apiExecute ("uuid_transfer", uuid + " *5" + requester_id_number + " XML default");

/*
 * Provide confirmation beeps followed by some silence.
 */

var confirmation = "tone_stream://L=3;%(100,100,350,440)";

session.execute ("playback", confirmation);

var i;

for (i = 0; session.ready () && i < 100; i++)
  session.execute("sleep", "100");

exit ();
