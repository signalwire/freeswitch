var dtmftimeout = 40000;
var digitmaxlength = 0;

function mycb(session, type, data, arg) {
  if (type == "dtmf") {
    if (data.digit == "#") {
      return true;
    }
    dtmf.digits += data.digit;

    if (dtmf.digits.length < digitmaxlength) {
      return true;
    }
  }
  return false;
}

let dtmf = new Object();
dtmf.digits = "";
if (session.ready()) {
  session.answer();

  digitmaxlength = 1;
  session.streamFile("ivr/ivr-welcome.wav", mycb, "dtmf 2000");
  session.collectInput(mycb, dtmf, dtmftimeout);
  console_log("console", "IVR Digit Pressed: " + dtmf.digits + "\n");

  if (dtmf.digits == "1") {
    session.execute("javascript", "queueManager.js");
  }
  // else if (dtmf.digits == "2") {
  //     session.execute("transfer", "5551234 XML default"); //transfer to external number
  //   } else if (dtmf.digits == "3") {
  //     session.execute("transfer", "9999 XML default"); //transfer to 9999 music on hold
  //   } else if (dtmf.digits == "4") {
  //     session.execute("transfer", "5000 XML default"); //transfer to example IVR extension
  //   } else {
  //     //transfer to voicemail
  //   }
}
