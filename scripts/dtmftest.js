function onPlayFile(s, type, obj, arg)
  {
    try {
      if (type == "dtmf") {
	console_log("info", "DTMF digit: " + s.name + " [" + obj.digit + "] len [" + obj.duration + "]\n\n");
	session.execute("phrase", "spell," + obj.digit);
      }

    }  catch (e) {
      console_log("err", e + "\n");
    }

    return true;

  }

session.answer();

while(session.ready()) {
  session.streamFile(argv[0], onPlayFile);
}
