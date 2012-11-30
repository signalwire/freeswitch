package org.freeswitch;

public interface StateHandler {

	public interface OnInitHandler extends StateHandler {
		public int onInit(String uuid);
	}

	public static interface OnRoutingHandler extends StateHandler {
		public int onRouting(String uuid);
	}

	public static interface OnExecuteHandler extends StateHandler {
		public int onExecute(String uuid);
	}

	public static interface OnHangupHandler extends StateHandler {
		public int onHangup(String uuid, String cause);
	}

	public static interface OnExchangeMediaHandler extends StateHandler {
		public int onExchangeMedia(String uuid);
	}

	public static interface OnSoftExecuteHandler extends StateHandler {
		public int onSoftExecute(String uuid);
	}

	public static interface OnConsumeMediaHandler extends StateHandler {
		public int onConsumeMedia(String uuid);
	}

	public static interface OnHibernateHandler extends StateHandler {
		public int onHibernate(String uuid);
	}

	public static interface OnResetHandler extends StateHandler {
		public int onReset(String uuid);
	}

	public static interface OnParkHandler extends StateHandler {
		public int onPark(String uuid);
	}

	public static interface OnReportingHandler extends StateHandler {
		public int onReporting(String uuid);
	}

	public static interface OnDestroyHandler extends StateHandler {
		public int onDestroy(String uuid);
	}

}

