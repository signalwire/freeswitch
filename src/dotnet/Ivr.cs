using System;
using System.Runtime.InteropServices;
using System.Text;
using FreeSwitch.Types;
using FreeSwitch.Marshaling.Types;

namespace FreeSwitch
{
    public class Ivr
    {
        public static Status MultiThreadedBridge(CoreSession session, CoreSession peerSession, InputCallbackFunction dtmfCallback)
        {
            return Switch.switch_ivr_multi_threaded_bridge(session, peerSession, dtmfCallback, IntPtr.Zero, IntPtr.Zero);
        }

        public static Status Originate(CoreSession session, CoreSession peerSession, CallCause callCause, string data, uint timelimit)
        {
            IntPtr callCausePtr = Marshal.AllocHGlobal(4);
            IntPtr dataPtr = Marshal.StringToHGlobalAnsi(data);

            Marshal.StructureToPtr(callCause, callCausePtr, true);

            return Switch.switch_ivr_originate(session, ref peerSession, callCausePtr, dataPtr, timelimit, null, null, null, null);
        }

        public static Status RecordFile(CoreSession coreSession, FileHandle fileHandle, string file, DtmfCallbackFunction dtmfCallbackFunction)
        {
            Byte[] filename = Encoding.Default.GetBytes(file);

            Console.WriteLine("File: {0}", file);

            dtmfCallbackFunction(coreSession, "1");
            dtmfCallbackFunction(coreSession, "2");
            dtmfCallbackFunction(coreSession, "3");

            Encoding ansiEncoding = Encoding.GetEncoding(1252);
            //filename = Encoding.Convert(Encoding.Default, ansiEncoding, filename);

            Console.WriteLine("Filename: {0}", file);
            //Console.WriteLine("Status record: {0}", status.ToString());
            return Status.Success;
        }
    }
}