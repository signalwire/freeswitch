/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application - mod_mono
 * Copyright (C) 2008, Michael Giagnocavo <mgg@packetrino.com>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application - mod_mono
 *
 * The Initial Developer of the Original Code is
 * Michael Giagnocavo <mgg@packetrino.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Michael Giagnocavo <mgg@packetrino.com>
 * Jeff Lenk <jlenk@frontiernet.net> - Modified class to support Dotnet
 * 
 * MonoSession.cs -- MonoSession additional functions
 *
 */
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Runtime.InteropServices;

namespace FreeSWITCH.Native
{
    public partial class MonoSession
    {
			public delegate void hangupDelegate();
			hangupDelegate hangupDel;
			System.Runtime.InteropServices.GCHandle gcHangup;
			IntPtr hangupFuncPtr;
			public delegate string dtmfDelegate(IntPtr input, Native.switch_input_type_t inputType);
			dtmfDelegate dtmfDel;
			System.Runtime.InteropServices.GCHandle gcDtmf;
			IntPtr dtmfFuncPtr;


		  [DllImport("mod_dotnet", EntryPoint = "InitDotnetSession")]
			public static extern void InitDotnetSession(IntPtr sessionPtr, IntPtr dtmfDelegate, IntPtr hangupDelegate);

        /// <summary>Initializes the native MonoSession. Must be called after Originate.</summary>
        public void Initialize()
        {
				hangupDel = new hangupDelegate(hangupCallback);
				//Calling the Alloc function to make sure the managed object is not garbage collected
				gcHangup = GCHandle.Alloc(hangupDel);
				hangupFuncPtr = System.Runtime.InteropServices.Marshal.GetFunctionPointerForDelegate(hangupDel);

				dtmfDel = new dtmfDelegate(inputCallback);
				//Calling the Alloc function to make sure the managed object is not garbage collected
				gcDtmf = GCHandle.Alloc(dtmfDel);
				dtmfFuncPtr = System.Runtime.InteropServices.Marshal.GetFunctionPointerForDelegate(dtmfDel);

			  InitDotnetSession(MonoSession.getCPtr(this).Handle, dtmfFuncPtr, hangupFuncPtr);
        }

        /// <summary>Function to execute when this session hangs up.</summary>
        public Action HangupFunction { get; set; }

        /// <summary>Sets the application that should have it's run thread aborted (if enabled) when this session is hungup.</summary>
        internal AppFunction AppToAbort { get; set; }

        void hangupCallback()
        {
            Log.WriteLine(LogLevel.Debug, "AppFunction is in hangupCallback.");
            try {
                if (AppToAbort != null) AppToAbort.AbortRun();
                var f = HangupFunction;
                if (f != null) f();
            }
            catch (Exception ex) {
                Log.WriteLine(LogLevel.Warning, "Exception in hangupCallback: {0}", ex.ToString());
                throw;
            }
        }

        public Func<Char, TimeSpan, string> DtmfReceivedFunction { get; set; }

        public Func<Native.Event, string> EventReceivedFunction { get; set; }

        string inputCallback(IntPtr input, Native.switch_input_type_t inputType)
        {
            switch (inputType) {
                case FreeSWITCH.Native.switch_input_type_t.SWITCH_INPUT_TYPE_DTMF:
                    using (var dtmf = new Native.switch_dtmf_t(input, false)) {
                        return dtmfCallback(dtmf);
                    }
                case FreeSWITCH.Native.switch_input_type_t.SWITCH_INPUT_TYPE_EVENT:
                    using (var swevt = new Native.switch_event(input, false)) {
                        return eventCallback(swevt);
                    }
                default:
                    return "";
            }
        }

        string dtmfCallback(Native.switch_dtmf_t dtmf)
        {
            var f = DtmfReceivedFunction;
            return f == null ?
                "-ERR No DtmfReceivedFunction set." :
                f(((char)(byte)dtmf.digit), TimeSpan.FromMilliseconds(dtmf.duration));
        }

        string eventCallback(Native.switch_event swevt)
        {
            using (var evt = new FreeSWITCH.Native.Event(swevt, 0)) {
                var f = EventReceivedFunction;
                return f == null ?
                    "-ERR No EventReceivedFunction set." :
                    f(evt);
            }
        }

    }
}
