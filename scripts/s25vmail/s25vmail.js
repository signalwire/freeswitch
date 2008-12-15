/*
 *  File:    s25vmail.js
 *  Purpose: Invoke voicemail based on AT&T System 25 PBX voicemail mode codes
 *  Machine:                      OS:
 *  Author:  John Wehle           Date: June 24, 2008
 *
 *  Copyright (c)  2008  Feith Systems and Software, Inc.
 *                      All Rights Reserved
 *
 *  The message waiting indicator is handled by a separate program.
 */


var id_digits_required = 3;

var digitTimeOut = 3000;
var interDigitTimeOut = 1000;
var absoluteTimeOut = 10000;


var dtmf_digits = "";
 
function on_dtmf (session, type, obj, arg)
  {

  if (type == "dtmf") {
    dtmf_digits += obj.digit;
    }

  return true;
  }


function prompt_for_id ()
  {
  var id;
  var index;
  var repeat;
  
  dtmf_digits = "";
  id = "";
  repeat = 0;

  while (session.ready () && repeat < 3) {
    session.flushDigits ();
  
    /* play phrase - if digit keyed while playing callback will catch them*/
    session.sayPhrase ("voicemail_enter_id", "#", "", on_dtmf, "");

    if (! session.ready ())
      return "";

    id = dtmf_digits;

    if (id.indexOf ('#') == -1) {
      dtmf_digits = session.getDigits (5, '#', digitTimeOut,
                                       interDigitTimeOut, absoluteTimeOut);
      id += dtmf_digits;
      id += '#';
      }

    /* a valid id must meet the minimum length requirements */
    if ((index = id.indexOf ('#')) >= id_digits_required) {
      id = id.substring (0,index);
      break;
      }

    dtmf_digits = "";
    id = "";
    repeat++;
    }

  return id;
  }


var start = "";
var mode = "";
var from = "";
var to = "";

var domain = session.getVariable ("domain");

session.answer ();

start = session.getDigits (1, '', digitTimeOut,
                           interDigitTimeOut, absoluteTimeOut);

if (start != "#") {
  console_log ("err", "Invalid VMAIL start code from PBX\n");
  exit();
  }

mode = session.getDigits (5, '#', digitTimeOut,
                          interDigitTimeOut, absoluteTimeOut);

from = session.getDigits (5, '#', digitTimeOut,
                          interDigitTimeOut, absoluteTimeOut);

to = session.getDigits (5, '#', digitTimeOut,
                        interDigitTimeOut, absoluteTimeOut);

session.execute("sleep", "1000");

// Verify that the proper parameters are present
switch (mode) {

  // Direct Inside Access
  case "00":
    if (isNaN (parseInt (from, 10))) {
      console_log ("err", "Invalid VMAIL calling PDC from PBX\n");
      break;
      }

    session.setVariable ("voicemail_authorized", "false");
    session.execute ("voicemail", "check default " + domain + " " + from);
    break;

  // Direct Dial Access
  case "01":
    from = prompt_for_id ();

    if (! session.ready ()) {
      session.hangup();
      exit();
      }

    if (isNaN (parseInt (from, 10))) {
      console_log ("err", "Invalid VMAIL mailbox from caller\n");
      break;
      }

    session.setVariable ("voicemail_authorized", "false");
    session.execute ("voicemail", "check default " + domain + " " + from);
    break;

  // Coverage - caller is inside
  case "02":
    if (isNaN (parseInt (from, 10)) || isNaN (parseInt (to, 10))) {
      console_log ("err", "Invalid VMAIL calling or called PDC from PBX\n");
      break;
      }

    session.setVariable ("effective_caller_id_name", "inside caller");
    session.setVariable ("effective_caller_id_number", from);

    session.execute ("voicemail", "default " + domain + " " + to);
    break;

  // Coverage - caller is dial
  case "03":
    if (isNaN (parseInt (to, 10))) {
      console_log ("err", "Invalid VMAIL called PDC from PBX\n");
      break;
      }

    session.setVariable ("effective_caller_id_name", "outside caller");
    session.setVariable ("effective_caller_id_number", "Unknown");

    session.execute ("voicemail", "default " + domain + " " + to);
    break;

  // Coverage - not yet defined
  case "04":
    break;

  // Leave Word Calling
  case "05":
    if (isNaN (parseInt (from, 10)) || isNaN (parseInt (to, 10))) {
      console_log ("err", "Invalid VMAIL calling or called PDC from PBX\n");
      break;
      }
    break;

  // Refresh MW lamps
  case "06":
    break;

  // Voice Port failed to answer
  case "08":
    if (isNaN (parseInt (to, 10))) {
      console_log ("err", "Invalid VMAIL PDC from PBX\n");
      break;
      }
    break;

  // Unknown
  default:
    console_log ("err", "Invalid VMAIL mode code from PBX\n");
    break;
  }

exit();
