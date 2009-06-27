/*
 *  File:    aadir.js
 *  Purpose: Auto Attendant directory.
 *  Machine:                      OS:
 *  Author:  John Wehle           Date: November 6, 2008
 *
 *  Copyright (c)  2008  Feith Systems and Software, Inc.
 *                      All Rights Reserved
 */


var digitTimeOut = 3000;
var interDigitTimeOut = 1000;
var absoluteTimeOut = 10000;


var base_dir = session.getVariable ("base_dir");
var domain = session.getVariable ("domain");
var voicemail_path = base_dir + "/storage/voicemail/default/" + domain + "/";

var file_exts = [ ".wav", ".mp3" ];

var extRE = /^1[0-9][0-9][0-9]$/g;
var operator = "operator";

var directory;
var directory_camelcase;

var translations = [ "0",
                     "QZ", "ABC", "DEF",
                     "GHI", "JKL", "MNO",
                     "PQRS", "TUV", "WXYZ" ];

var extension = "";
var dtmf_digits = "";


function load_directory ()
  {
  var i;
  var name;
  var number;

  var dir = apiExecute ("xml_locate", "directory domain name " + domain);
  var re = /\s+$/g;
  var length = dir.search (re);

  if (length == -1)
    length = dir.length;

  dir = dir.substring (0, length);

  var xdir = new XML (dir);

  directory = new Array ();
  i = 0;

  re = /[^A-Z0-9\s]/gi;

  for each (var variables in xdir.groups.group.users.user.variables) {
    name = "";
    number = "";

    for each (variable in variables.variable) {
      if (variable.@name.toString() == "effective_caller_id_name")
        name = variable.@value.toString();
      if (variable.@name.toString() == "effective_caller_id_number")
        number = variable.@value.toString();
      }

    if (name.length == 0 || number.length == 0 || number.search (extRE) == -1)
      continue;

    directory[i] = new Array (2);
    directory[i][0] = name.replace (re, "");
    directory[i][1] = number;

    i++;
    }
  }


function build_camelcase_directory ()
  {
  var i;
  var fname;
  var lname;
  var fre = /^[A-Z0-9]+/gi;
  var lre = /[A-Z0-9]+$/gi;

  directory_camelcase = new Array (directory.length);

  for (i = 0; i < directory.length; i++) {
    directory_camelcase[i] = new Array (2);

    directory_camelcase[i][0] = "";
    directory_camelcase[i][1] = 0;

    fname = directory[i][0].match (fre);
    lname = directory[i][0].match (lre);
    if (fname.length != 1 || lname.length != 1) {
      console_log ("err", "Can't parse " + directory[i][0] + " for directory\n");
      continue;
      }

    directory_camelcase[i][0] = lname[0] + fname[0];
    directory_camelcase[i][1] = directory[i][1];
    }
  }


function directory_lookup (digits)
  {
  var i;
  var match = "";
  var pattern = "^";
  var re;

  if (digits.length && digits[0] == 0)
    return 0;

  for (i = 0; i < digits.length; i++) {
    if (isNaN (parseInt (digits[i], 10)))
      return -1;
    pattern += "[" + translations[parseInt (digits[i], 10)] + "]";
    }

  re = new RegExp (pattern, "i");

  for (i = 0; i < directory_camelcase.length; i++)
    if (directory_camelcase[i][0].search (re) != -1) {
      if (! isNaN (parseInt (match, 10)))
        return "";
      match = directory_camelcase[i][1];
      }

  if (isNaN (parseInt (match, 10)))
    return -1;

  return match;
  }


function on_dtmf (session, type, obj, arg)
  {

  if (type == "dtmf") {
    dtmf_digits += obj.digit;
    extension = directory_lookup (dtmf_digits)
    return false;
    }

  return true;
  }


function directory_prompt ()
  {
  var choice;
  var index;
  var repeat;
  
  extension = "";
  choice = "";
  repeat = 0;

  while (session.ready () && repeat < 3) {
  
    /* play phrase - if digit keyed while playing callback will catch them*/
    session.sayPhrase ("feith_aa_directory", "#", "", on_dtmf, "");

    choice = dtmf_digits;

    while ( isNaN (parseInt (extension, 10)) ) {
      if (! session.ready ())
        return "";

      dtmf_digits = session.getDigits (1, '#', digitTimeOut,
                                       interDigitTimeOut, absoluteTimeOut);
      choice += dtmf_digits;

      extension = directory_lookup (choice);
      }

    if (parseInt (extension, 10) >= 0)
      break;

    session.sayPhrase ("voicemail_invalid_extension", "#", "", on_dtmf, "");

    dtmf_digits = "";
    extension = "";
    choice = "";
    repeat++;

    session.flushDigits ();
    }

  return extension;
  }


var choice = "";
var fd;
var i;
var recorded_name;

session.answer ();

session.execute("sleep", "1000");

load_directory ();

build_camelcase_directory ();

dtmf_digits = "";
session.flushDigits ();
choice = directory_prompt ();

if (! session.ready ()) {
  session.hangup();
  exit();
  }

if ( isNaN (parseInt (choice, 10)) || parseInt (choice, 10) <= 0) {
  session.execute ("transfer", operator + " XML default");
  exit();
  }

for (i = 0; i < file_exts.length; i++) {
  recorded_name = voicemail_path + choice + "/recorded_name" + file_exts[i];
  fd = new File (recorded_name);
  if (fd.exists) {
    session.streamFile (recorded_name);
    break;
    }
  }

session.execute ("phrase", "spell," + choice);

session.execute ("transfer", choice + " XML default");

exit();
