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
 * Demo.cs -- mod_mono demo classes
 *
 */

#if DEBUG

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace FreeSWITCH.Demo
{
    public class AppDemo : AppFunction
    {
        new protected static bool Load()
        {
            Log.WriteLine(LogLevel.Info, "Inside AppDemo::Load.");
            return true;
        }

        protected override void Run() {
            bool isRecording = false;
            Session.DtmfReceivedFunction = (d, t) => {
                Log.WriteLine(LogLevel.Critical, "RECORDING is {0}", Session.GetVariable("RECORDING"));
                Log.WriteLine(LogLevel.Info, "Received {0} for {1}.", d, t);

                if (isRecording) {
                    Log.WriteLine(LogLevel.Info, "Recording: [TRUE]  Returning crap");
                    return "1";
                } else {
                    Log.WriteLine(LogLevel.Info, "Recording: [FALSE] Returning null");
                    return null;
                }
            };
            Session.StreamFile(@"C:\freeswitch\Debug\sounds\en\us\callie\voicemail\8000\vm-hello.wav", 0);
            var fn = @"C:\" + Session.GetHashCode() + ".wav";
            isRecording = true;
            Session.SetVariable("RECORDING", "true");
            Session.RecordFile(fn, 600, 500, 3);
            isRecording = false;
            Session.SetVariable("RECORDING", "false");
            Session.sleep(500);
            Log.WriteLine(LogLevel.Info, "WW GROUP: Finished Recording file");
            var res = Session.PlayAndGetDigits(1, 1, 3, 3000, "*", @"C:\freeswitch\libs\sounds\en\us\callie\ivr\8000\ivr-sample_submenu.wav", @"C:\freeswitch\libs\sounds\en\us\callie\ivr\8000\ivr-sample_submenu.wav", "1|2|3|9|#");
            Log.WriteLine(LogLevel.Info, "WW GROUP: Message Menu [" + res + "]");
        }
        
        void hangupHook()
        {
            Log.WriteLine(LogLevel.Debug, "AppDemo hanging up, UUID: {0}.", this.Uuid);
        }

        protected override bool AbortOnHangup { get { return true; } } 
    }

    public class ApiDemo : ApiFunction
    {
        new protected static bool Load()
        {
            Log.WriteLine(LogLevel.Debug, "Inside ApiDemo::Load.");
            return true;
        }

        public override void ExecuteBackground(string args)
        {
            Log.WriteLine(LogLevel.Debug, "ApiDemo on a background thread #({0}), with args '{1}'.",
                System.Threading.Thread.CurrentThread.ManagedThreadId,
                args);
        }

        public override void Execute(Native.Stream stream, Native.Event evt, string args)
        {
            stream.Write(string.Format("ApiDemo executed with args '{0}' and event type {1}.",
                args, evt == null ? "<none>" : evt.GetEventType()));
        }

    }

}

#endif