package org.freeswitch.example;

import org.freeswitch.StateHandler.OnHangupHandler;

public class OriginateStateHandler implements OnHangupHandler {

	private static OriginateStateHandler instance = null;

	public static final OriginateStateHandler getInstance() {
		if ( instance == null ) instance = new OriginateStateHandler();
		return instance;
	}

	private OriginateStateHandler() {
		// hide constructor
	}

	public int onHangup(String uuid, String cause) {
		return 1; // SWITCH_STATUS_FALSE
	}

}