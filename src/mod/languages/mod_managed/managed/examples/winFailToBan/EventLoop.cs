using System;
using System.Threading;
using FreeSWITCH.Native;
using winFailToBan.Internal;
using FreeSWITCH;

namespace winFailToBan
{
    public static class EventLoop
    {
        public static Boolean Running = true;
        private static Thread _eventThread;

        public static void StartEvents()
        {
            if (_eventThread != null)
                return;
            _eventThread = new Thread(EventMainLoop);
            Running = true;
            _eventThread.Start();
        }

        public static void StopEvents()
        {
            Running = false;
        }

        public static void EventMainLoop()
        {
            EventConsumer ec = null;
            try
            {

                ec = new EventConsumer("CUSTOM", "sofia::register_attempt", 100);
                ec.bind("SHUTDOWN", String.Empty);
                ec.bind("HEARTBEAT", String.Empty);
                while (Running)
                {
                    var evt = ec.pop(1, 0);
                    if (evt == null)
                        continue;

                    var en = evt.InternalEvent.GetValueOfHeader("Event-Name");
                    if (en == "CUSTOM")
                        en = evt.InternalEvent.GetValueOfHeader("Event-SubClass");
                    switch (en)
                    {
                        case @"sofia::register_attempt":
                            {
                                var iev = evt.InternalEvent;
                                var ar = iev.GetValueOfHeader("auth-result");  // get the value of the result to see if it's the case we want
                                var ip = iev.GetValueOfHeader("network-ip");  // and the ip address the register came from
                                
                                if (ar == "FORBIDDEN")
                                {
                                    BanTracker.TrackFailure(ip);
                                }
                            }
                            break;

                        case "SHUTDOWN":
                            Log.WriteLine(LogLevel.Critical,"FTB: Processing Shutdown event");
                            Running = false;
                            break;

                        case "HEARTBEAT":
                            BanTracker.CleanUp();
                            break;

                        default:
                            break;
                    }
                }
            }
            catch (Exception exx)
            {
                Log.WriteLine(LogLevel.Critical, "FailToBan -- Exception in event loop {0}", exx.Message);
            }
            finally
            {
                if(ec != null)
                    ec.Dispose();
                _eventThread = null;
            }
        }
    }
}
