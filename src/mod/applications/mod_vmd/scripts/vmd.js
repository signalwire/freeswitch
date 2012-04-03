function onInput(s, type, obj, arg)
{
	try
	{
		if(type == "dtmf")
		{
			console_log("info", "DTMF digit: "+s.name+" ["+obj.digit+"] len ["+obj.duration+"]\n");
		}
		else if(type == "event" && session.getVariable("vmd_detect") == "TRUE")
		{
			console_log("info", "Voicemail Detected\n");
		}
		
	}
	catch(e)
	{
		console_log("err", e + "\n");
	}
	return true;
}

session.answer();
session.execute("vmd", "start");
while(session.ready())
{
	session.streamFile(argv[0], onInput);
}
