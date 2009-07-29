/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application - mod_managed
 * Copyright (C) 2008, Michael Giagnocavo <mgg@giagnocavo.net>
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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application - mod_managed
 *
 * The Initial Developer of the Original Code is
 * Michael Giagnocavo <mgg@giagnocavo.net>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Michael Giagnocavo <mgg@giagnocavo.net>
 * 
 * Demo.cs -- mod_mono demo classes
 *
 */

// How to test the demo (in the mod/managed directory):
// -- Compile to dll for "normal" loading
// -- Compile to exe for script EXE loading
// -- Copy to managed directory for dynamic compilation

using System;
using FreeSWITCH;
using FreeSWITCH.Native;

public class AppDemo : IAppPlugin {

    ManagedSession Session;
    public void Run(AppContext context) {
        Session = context.Session;
        Session.HangupFunction = hangupHook;
        Session.Answer();
        Session.DtmfReceivedFunction = (d, t) => {
            Log.WriteLine(LogLevel.Notice, "Received {0} for {1}.", d, t);
            return "";
        };
        Log.WriteLine(LogLevel.Notice, "Inside AppDemo.Run (args '{0}'); HookState is {1}. Now will collect digits.", context.Arguments, Session.HookState);
        Session.CollectDigits(5000); // Hanging up here will cause an abort and the next line won't be written
        Log.WriteLine(LogLevel.Notice, "AppDemo is finishing its run and will now hang up.");
        Session.Hangup("USER_BUSY");
    }

    void hangupHook() {
        Log.WriteLine(LogLevel.Notice, "AppDemo hanging up, UUID: {0}.", this.Session.Uuid);
    }

}

public class ApiDemo : IApiPlugin {

    public void Execute(ApiContext context) {
        context.Stream.Write(string.Format("ApiDemo executed with args '{0}' and event type {1}.",
            context.Arguments, context.Event == null ? "<none>" : context.Event.GetEventType()));
    }

    public void ExecuteBackground(ApiBackgroundContext context) {
        Log.WriteLine(LogLevel.Notice, "ApiDemo on a background thread #({0}), with args '{1}'.",
            System.Threading.Thread.CurrentThread.ManagedThreadId,
            context.Arguments);
    }

}

public class LoadDemo : ILoadNotificationPlugin {
    public bool Load() {
        Log.WriteLine(LogLevel.Notice, "LoadDemo running.");
        return true;
    }
}

public class ScriptDemo {

    public static void Main() {
        switch (FreeSWITCH.Script.ContextType) {
            case ScriptContextType.Api: {
                    var ctx = FreeSWITCH.Script.GetApiContext();
                    ctx.Stream.Write("Script executing as API with args: " + ctx.Arguments);
                    break;
                }
            case ScriptContextType.ApiBackground: {
                    var ctx = FreeSWITCH.Script.GetApiBackgroundContext();
                    Log.WriteLine(LogLevel.Notice, "Executing as APIBackground with args: " + ctx.Arguments);
                    break;
                }
            case ScriptContextType.App: {
                    var ctx = FreeSWITCH.Script.GetAppContext();
                    Log.WriteLine(LogLevel.Notice, "Executing as App with args: " + ctx.Arguments);
                    break;
                }
        }

    }

}

