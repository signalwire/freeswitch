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
 * Jeff Lenk <jeff@jefflenk.com>
 * 
 * PluginInterfaces.cs -- Public interfaces for plugins
 *
 */

using System;
using System.Collections.Generic;
using System.Linq;

namespace FreeSWITCH {

    public class AppContext {
        readonly string arguments;
        readonly Native.ManagedSession session;

        public AppContext(string arguments, Native.ManagedSession session) {
            this.arguments = arguments;
            this.session = session;
        }

        public string Arguments { get { return arguments; } }
        public Native.ManagedSession Session { get { return session; } }
    }

    public class ApiContext {
        readonly string arguments;
        readonly Native.Stream stream;
        readonly Native.Event evt;

        public ApiContext(string arguments, Native.Stream stream, Native.Event evt) {
            this.arguments = arguments;
            this.stream = stream;
            this.evt = evt;
        }

        public string Arguments { get { return arguments; } }
        public Native.Stream Stream { get { return stream; } }
        public Native.Event Event { get { return evt; } }
    }

    public class ApiBackgroundContext {
        readonly string arguments;

        public ApiBackgroundContext(string arguments) {
            this.arguments = arguments;
        }

        public string Arguments { get { return arguments; } }
    }

    public interface IApiPlugin {
        void Execute(ApiContext context);
        void ExecuteBackground(ApiBackgroundContext context);
    }

    public interface IAppPlugin {
        void Run(AppContext context);
    }

    public interface ILoadNotificationPlugin {
        bool Load();
    }

    [Flags]
    public enum PluginOptions {
        None            = 0,
        NoAutoReload    = 1,
    }

    public interface IPluginOptionsProvider {
        PluginOptions GetOptions();
    }

}
