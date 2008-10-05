/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application - mod_cli
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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application - mod_cli
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

        protected Native.ManagedSession Session { get; private set; }

        protected string Arguments { get; private set; }

        public bool IsAvailable
        {
            get
            {
                if (this.Session == null) return false;
                return this.Session.Ready();
            }
        }

        /// <summary>Determines if the thread used for Run will have Abort called on it on hangup. Defaults to false.</summary>
        protected virtual bool AbortOnHangup { get { return false; } }
        bool abortable = false;
        readonly object abortLock = new object();
        Thread runThread;
        internal void AbortRun()
        {
            if (!AbortOnHangup) return;
            if (runThread == Thread.CurrentThread) {
                Log.WriteLine(LogLevel.Warning, "Thread will not be aborted because Hangup was called from the Run thread.");
                return;
            }
            lock (abortLock) {
                if (abortable) {
                    Log.WriteLine(LogLevel.Critical, "Aborting run thread.");
                    runThread.Abort();
                }
            }
        }

        protected Guid Uuid { get; private set; }

        internal void RunInternal(FreeSWITCH.Native.ManagedSession session, string args)
        {
            this.Session = session;
            this.Arguments = args;
            Session.AppToAbort = this;
            try { this.Uuid = new Guid(Session.GetUuid()); }
            catch { }
            try {
                runThread = Thread.CurrentThread;
                lock (abortLock) abortable = true;
                Run();
            }
            catch (ThreadAbortException) {
                Log.WriteLine(LogLevel.Critical, "Run thread aborted.");
                Thread.ResetAbort();
            }
            finally {
                lock (abortLock) { abortable = false; }
                if (runThread.ThreadState == ThreadState.AbortRequested) {
                    try { Thread.ResetAbort(); }
                    catch { }
                }
            }
        }

        protected abstract void Run();
    }
}
