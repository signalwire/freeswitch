using System;
using System.Runtime.InteropServices;
using System.Text;
using FreeSwitch.Types;
using FreeSwitch.Marshaling.Types;

namespace FreeSwitch
{
    public class Log
    {
        public static void Printf(LogLevel level, string message)
        {
            Switch.switch_log_printf(null, "File", "Func.NET", 123, level, message);
        }
    }
}