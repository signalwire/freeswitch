package org.freeswitch.example;

import org.freeswitch.swig.freeswitch;

public class ApplicationLauncher {

	public static final void startup(String arg) {
		try {
			freeswitch.setOriginateStateHandler(OriginateStateHandler.getInstance());
		} catch (Exception e) {
			freeswitch.console_log("err", "Error registering originate state handler");
		}
	}

}
