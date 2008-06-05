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
 * 
 * AppFunction.cs -- Base class for applications
 *
 */

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;

namespace FreeSWITCH
{
    public abstract class AppFunction
    {
        protected static bool Load() { return true; }

        protected static void Unload() { }

        void hangupCallback()
        {
            Log.WriteLine(LogLevel.Debug, "AppFunction is in hangupCallback.");
            abortRun();
            var f = HangupFunction;
            if (f != null) f();
        }

        protected Action HangupFunction { get; set; }

        protected Func<Char, TimeSpan, string> DtmfReceivedFunction { get; set; }

        protected Func<Native.Event, string> EventReceivedFunction { get; set; }

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

        protected Native.MonoSession Session { get; private set; }

        protected string Arguments { get; private set; }

        public bool IsAvailable
        {
            get
            {
                if (this.Session == null) return false;
                return this.Session.Ready();
            }
        }

        protected virtual bool AbortOnHangup { get { return false; } }
        bool abortable = false;
        readonly object abortLock = new object();
        Thread runThread;
        void abortRun()
        {
            if (!AbortOnHangup) return;
            lock (abortLock) {
                if (abortable) runThread.Abort();
            }
        }

        protected Guid Uuid { get; private set; }

        internal void RunInternal(FreeSWITCH.Native.MonoSession session, string args)
        {
            this.Session = session;
            this.Arguments = args;
            Session.SetDelegates(this.inputCallback, this.hangupCallback);
            try { this.Uuid = new Guid(Session.GetUuid()); }
            catch { }
            try {
                runThread = Thread.CurrentThread;
                lock (abortLock) abortable = true;
                Run();
            }
            catch (ThreadAbortException) {
                Log.WriteLine(LogLevel.Debug, "Run thread aborted.");
                Thread.ResetAbort();
            }
            finally {
                lock (abortLock) { abortable = false; }
                Thread.ResetAbort();
            }
        }

        protected abstract void Run();
    }
}
