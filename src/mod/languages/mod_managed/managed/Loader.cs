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
 * David Brazier <David.Brazier@360crm.co.uk>
 * Jeff Lenk <jeff@jefflenk.com>
 * Artur Kraev <ravenox@gmail.com>
 * 
 * Loader.cs -- mod_managed loader
 *
 */

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;

namespace FreeSWITCH {
    internal static class Loader {

        // Thunks
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate bool ExecuteDelegate(string cmd, IntPtr streamH, IntPtr eventH);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate bool ExecuteBackgroundDelegate(string cmd);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate bool RunDelegate(string cmd, IntPtr session);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate bool ReloadDelegate(string cmd);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate bool ListDelegate(string cmd, IntPtr streamH, IntPtr eventH);
        static readonly ExecuteDelegate _execute = Execute;
        static readonly ExecuteBackgroundDelegate _executeBackground = ExecuteBackground;
        static readonly RunDelegate _run = Run;
        static readonly ReloadDelegate _reload = Reload;
        static readonly ListDelegate _list = List;
        
        [DllImport("mod_managed", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        static extern void InitManagedDelegates(RunDelegate run, ExecuteDelegate execute, ExecuteBackgroundDelegate executeBackground, ReloadDelegate reload, ListDelegate list);

        static readonly object loaderLock = new object();

        static string managedDir;
        static string shadowDir;
        
        public static bool Load() {
            managedDir = Path.Combine(Native.freeswitch.SWITCH_GLOBAL_dirs.mod_dir, "managed");
            shadowDir = Path.Combine(managedDir, "shadow");
            if (Directory.Exists(shadowDir)) {
                try {
                    Directory.Delete(shadowDir, true);
                } catch (Exception ex) {
                    Log.WriteLine(LogLevel.Warning, "Cannot delete shadow directory: {0}", ex);
                }

                Directory.CreateDirectory(shadowDir);
            }

            Log.WriteLine(LogLevel.Debug, "FreeSWITCH.Managed loader is starting with directory '{0}'.", managedDir);
            if (!Directory.Exists(managedDir)) {
                Log.WriteLine(LogLevel.Error, "Managed directory not found: {0}", managedDir);
                return false;
            }

            AppDomain.CurrentDomain.AssemblyResolve += (_, rargs) => {
                Log.WriteLine(LogLevel.Info, "Resolving assembly '{0}'.", rargs.Name);
                if (rargs.Name == Assembly.GetExecutingAssembly().FullName) return Assembly.GetExecutingAssembly();
                var parts = rargs.Name.Split(',');
                var path = Path.Combine(managedDir, parts[0] + ".dll");
                Log.WriteLine(LogLevel.Info, "Resolving to: '" + path + "'.");
                return File.Exists(path) ? Assembly.LoadFile(path) : null;
            };

            InitManagedDelegates(_run, _execute, _executeBackground, _reload, _list);

            configureWatcher();

            // Initial load
            var allFiles = Directory.GetFiles(managedDir);
            foreach (var file in allFiles) {
                try {
                    loadFile(file);
                } catch (Exception ex) {
                    Log.WriteLine(LogLevel.Error, "Exception loading file {0}: " + ex.ToString(), file);
                }
            }
            initialLoadComplete = true;
            return true;
        }


        // *** File watcher for changes
        // Cheap queue hack is used because multiple related files can generate a bunch of changes
        // and a single file can also trigger a few notifications. With a simple queue, these get batched
        static readonly object watcherLock = new object();
        static FileSystemWatcher watcher;
        static System.Threading.Timer watcherTimer;
        static HashSet<string> watcherFiles = new HashSet<string>();
        static void configureWatcher() {
            watcher = new FileSystemWatcher(managedDir);
            watcher.IncludeSubdirectories = false;
            watcher.NotifyFilter = NotifyFilters.FileName | NotifyFilters.LastWrite;
            watcher.EnableRaisingEvents = true;
            watcher.Changed += watcher_Changed;
            watcher.Created += watcher_Changed;
            watcher.Deleted += watcher_Changed;
            watcher.Renamed += watcher_Changed;
            watcherTimer = new System.Threading.Timer(_ => {
                lock (watcherLock) {
                    foreach (var file in watcherFiles) {
                        try {
                            loadFile(file);
                        } catch (Exception ex) {
                            Log.WriteLine(LogLevel.Error, "Exception loading change from {0}: {1}", file, ex.ToString());
                        }
                    }
                    watcherFiles.Clear();
                }
            });
            watcherTimer.Change(1000, 1000);
        }
        
        static void watcher_Changed(object sender, FileSystemEventArgs e) {
            Action<string> queueFile = fileName => {
                var currentPi = pluginInfos
                    .Where(x => !string.IsNullOrEmpty(x.FileName))
                    .FirstOrDefault(p => string.Compare(fileName, p.FileName, StringComparison.OrdinalIgnoreCase) == 0);
                if (currentPi != null) {
                    var noReload = currentPi.Manager.ApiExecutors.Any(x => (x.PluginOptions & PluginOptions.NoAutoReload) == PluginOptions.NoAutoReload) ||
                                   currentPi.Manager.AppExecutors.Any(x => (x.PluginOptions & PluginOptions.NoAutoReload) == PluginOptions.NoAutoReload);
                    if (noReload) return;
                }
                lock (watcherLock) {
                    watcherFiles.Add(fileName); 
                } 
            };
            try {
                if (!initialLoadComplete) return;
                var file = e.FullPath;
                var isConfig = Path.GetExtension(file).ToLowerInvariant() == ".config";
                if (isConfig) {
                    queueFile(Path.ChangeExtension(file, null));
                } else {
                    queueFile(file);
                }
            } catch (Exception ex) {
                Log.WriteLine(LogLevel.Critical, "Unhandled exception watching for changes: " + ex.ToString());
            }
        }

        static volatile bool initialLoadComplete = false;
        static readonly List<PluginInfo> pluginInfos = new List<PluginInfo>(); 
        // Volatile cause we haven't streamlined all the locking
        // These dictionaries are never mutated; the reference is changed instead
        static volatile Dictionary<string, AppPluginExecutor> appExecs = new Dictionary<string, AppPluginExecutor>(StringComparer.OrdinalIgnoreCase);
        static volatile Dictionary<string, ApiPluginExecutor> apiExecs = new Dictionary<string, ApiPluginExecutor>(StringComparer.OrdinalIgnoreCase);

        static Dictionary<string, AppPluginExecutor> getAppExecs() {
            lock (loaderLock) {
                return appExecs;
            }
        }
        static Dictionary<string, ApiPluginExecutor> getApiExecs() {
            lock (loaderLock) {
                return apiExecs;
            }
        }

        class PluginInfo {
            public string FileName { get; set; }
            public AppDomain Domain { get; set; }
            public PluginManager Manager { get; set; }
        }

        static int appDomainCount = 0;
        static void loadFile(string fileName) {
            // Attempts to load the file. On failure, it will call unload.
            // Loading part does not take out a lock. 
            // Lock is only done after loading is finished and dictionaries need updating.

            // We might get a load for a file that's no longer there. Just unload the old one.
            if (!File.Exists(fileName)) {
                unloadFile(fileName);
                return;
            }

            Type pmType;
            switch (Path.GetExtension(fileName).ToLowerInvariant()) {
                case ".dll":
                    pmType = typeof(AsmPluginManager);
                    break;
                case ".exe": // TODO these need to come from config
                case ".fsx":
                case ".vbx":
                case ".csx":
                case ".jsx":
                    pmType = typeof(ScriptPluginManager);
                    break;
                default:
                    pmType = null;
                    break;
            }
            if (pmType == null) return;

            // App domain setup
            var setup = new AppDomainSetup();
            if (File.Exists(fileName + ".config")) {
                setup.ConfigurationFile = fileName + ".config";
            }
            setup.ApplicationBase = Native.freeswitch.SWITCH_GLOBAL_dirs.mod_dir;
            setup.LoaderOptimization = LoaderOptimization.MultiDomainHost; // TODO: would MultiDomain work better since FreeSWITCH.Managed isn't gac'd?
            setup.CachePath = shadowDir;
            setup.ShadowCopyFiles = "true";

            // computing private bin path
            var binPath = setup.PrivateBinPath ?? string.Empty;

            var binPaths = binPath.Split(new[] { ";" }, StringSplitOptions.RemoveEmptyEntries)
                .Select(x => x.Trim())
                .ToList();

            // adding "managed" (modules) directory
            if (!binPaths.Contains("managed")) {
                binPaths.Add("managed");
            }

            // adding "managed/<modulename>" directory for per-module references support
            var moduleRefsDir = Path.GetFileName(fileName);
            moduleRefsDir = Path.GetFileNameWithoutExtension(moduleRefsDir);

            if (moduleRefsDir != null && moduleRefsDir.Trim() != "") {
                moduleRefsDir = Path.Combine("managed", moduleRefsDir);
                if (!binPaths.Contains(moduleRefsDir, StringComparer.OrdinalIgnoreCase)) {
                    binPaths.Add(moduleRefsDir);
                }
            }

            // bringing all together
            setup.PrivateBinPath = string.Join(";", binPaths);

            // Create domain and load PM inside
            System.Threading.Interlocked.Increment(ref appDomainCount);
            setup.ApplicationName = Path.GetFileName(fileName) + "_" + appDomainCount;
            var domain = AppDomain.CreateDomain(setup.ApplicationName, null, setup);

            PluginManager pm;
            try {
                pm = (PluginManager)domain.CreateInstanceAndUnwrap(pmType.Assembly.FullName, pmType.FullName, null);
                if (!pm.Load(fileName)) {
                    AppDomain.Unload(domain);
                    unloadFile(fileName);
                    return;
                }
            } catch (Exception ex) {
                // On an exception, we will unload the current file so an old copy doesnt stay active
                Log.WriteLine(LogLevel.Alert, "Exception loading {0}: {1}", fileName, ex.ToString());
                AppDomain.Unload(domain);
                unloadFile(fileName);
                return;
            }

            addPlugin(fileName, domain, pm);
        }

        static void addPlugin(string fileName, AppDomain domain, PluginManager pm) {
            // Update dictionaries atomically
            lock (loaderLock) {
                if (!string.IsNullOrEmpty(fileName)) {
                    if (domain == null) throw new ApplicationException("File based plugins must specify an AppDomain.");
                    unloadFile(fileName);
                }
                if (domain == null) domain = AppDomain.CurrentDomain;

                var pi = new PluginInfo { FileName = fileName, Domain = domain, Manager = pm };
                pluginInfos.Add(pi);
                var newAppExecs = new Dictionary<string, AppPluginExecutor>(appExecs, StringComparer.OrdinalIgnoreCase);
                var newApiExecs = new Dictionary<string, ApiPluginExecutor>(apiExecs, StringComparer.OrdinalIgnoreCase);
                pm.AppExecutors.ForEach(x => x.Aliases.ForEach(y => newAppExecs[y] = x));
                pm.ApiExecutors.ForEach(x => x.Aliases.ForEach(y => newApiExecs[y] = x));
                appExecs = newAppExecs;
                apiExecs = newApiExecs;
                Action<PluginExecutor, string> printLoaded = (pe, type) => {
                    var aliases = pe.Aliases.Aggregate((acc, x) => acc += ", " + x);
                    Log.WriteLine(LogLevel.Notice, "Loaded {3} {0}, aliases '{1}', into domain {2}.", pe.Name, aliases, pi.Domain.FriendlyName, type);
                };
                pm.AppExecutors.ForEach(x => printLoaded(x, "App"));
                pm.ApiExecutors.ForEach(x => printLoaded(x, "Api"));
                Log.WriteLine(LogLevel.Info, "Finished loading {0} into domain {1}.", pi.FileName, pi.Domain.FriendlyName);
            }
        }

        public static void LoadEmbeddedPlugins(Assembly asm) {
            var pm = new EmbeddedPluginManager(asm);
            addPlugin(null, null, pm);
        }

        static void unloadFile(string fileName) {
            List<PluginInfo> pisToRemove;
            lock (loaderLock) {
                pisToRemove = pluginInfos.Where(x => string.Compare(fileName, x.FileName, StringComparison.OrdinalIgnoreCase) == 0).ToList();
                if (pisToRemove.Count == 0) return; // Done

                var apisToRemove = pisToRemove.SelectMany(x => x.Manager.ApiExecutors).ToList();
                var appsToRemove = pisToRemove.SelectMany(x => x.Manager.AppExecutors).ToList();
                pluginInfos.RemoveAll(pisToRemove.Contains);
                appExecs = appExecs.Where(x => !appsToRemove.Contains(x.Value)).ToDictionary(x => x.Key, x => x.Value, StringComparer.OrdinalIgnoreCase);
                apiExecs = apiExecs.Where(x => !apisToRemove.Contains(x.Value)).ToDictionary(x => x.Key, x => x.Value, StringComparer.OrdinalIgnoreCase);

                Action<PluginExecutor, string> printRemoved = (pe, type) => {
                    Log.WriteLine(LogLevel.Notice, "Unloaded {0} {1} (file {2}).", type, pe.Name, fileName);
                };
                apisToRemove.ForEach(x => printRemoved(x, "API"));
                appsToRemove.ForEach(x => printRemoved(x, "App"));
            }

            pisToRemove.ForEach(pi => {
                var t = new System.Threading.Thread(() => {
                    var friendlyName = pi.Domain.FriendlyName;
                    Log.WriteLine(LogLevel.Info, "Starting to unload {0}, domain {1}.", pi.FileName, friendlyName);
                    try {
                        var d = pi.Domain;
                        pi.Manager.BlockUntilUnloadIsSafe();
                        pi.Manager = null;
                        pi.Domain = null;
                        // This can crash if there is still sessions in the appdomain. Plugin code should use dispose properly.
                        AppDomain.Unload(d);
                        Log.WriteLine(LogLevel.Info, "Unloaded {0}, domain {1}.", pi.FileName, friendlyName);
                    } catch (Exception ex) {
                        Log.WriteLine(LogLevel.Alert, "Could not unload {0}, domain {1}: {2}", pi.FileName, friendlyName, ex.ToString());
                    }
                });
                t.Priority = System.Threading.ThreadPriority.BelowNormal;
                t.IsBackground = true;
                t.Start();
            });
        }

        static bool Reload(string cmd) {
            try {
                if (Path.IsPathRooted(cmd)) {  
                    loadFile(cmd);
                } else {
                    loadFile(Path.Combine(managedDir, cmd));
                }
                return true;
            } catch (Exception ex) {
                Log.WriteLine(LogLevel.Error, "Error reloading {0}: {1}", cmd, ex.ToString());
                return false;
            }
        }

        // ******* Execution

        static readonly char[] spaceArray = new[] { ' ' };
        /// <summary>Returns a string couple containing the module name and arguments</summary>
        static string[] parseCommand(string command) {
            if (string.IsNullOrEmpty(command)) {
                Log.WriteLine(LogLevel.Error, "No arguments supplied.");
                return null;
            }
            var args = command.Split(spaceArray, 2, StringSplitOptions.RemoveEmptyEntries);
            if (args.Length == 0 || string.IsNullOrEmpty(args[0]) || string.IsNullOrEmpty(args[0].Trim())) {
                Log.WriteLine(LogLevel.Error, "Module name not supplied.");
                return null;
            }
            if (args.Length == 1) {
                return new[] { args[0], "" };
            } else {
                return args;
            }
        }

        public static bool ExecuteBackground(string command) {
            try {
                var parsed = parseCommand(command);
                if (parsed == null) return false;
                var fullName = parsed[0];
                var args = parsed[1];
                var execs = getApiExecs();
                ApiPluginExecutor exec;
                if (!execs.TryGetValue(fullName, out exec)) {
                    Log.WriteLine(LogLevel.Error, "API plugin {0} not found.", fullName);
                    return false;
                }
                return exec.ExecuteApiBackground(args);
            } catch (Exception ex) {
                Log.WriteLine(LogLevel.Error, "Exception in ExecuteBackground({0}): {1}", command, ex.ToString());
                return false;
            }
        }

        public static bool Execute(string command, IntPtr streamHandle, IntPtr eventHandle) {
            try {
                System.Diagnostics.Debug.Assert(streamHandle != IntPtr.Zero, "streamHandle is null.");
                var parsed = parseCommand(command);
                if (parsed == null) return false;
                var fullName = parsed[0];
                var args = parsed[1];

                var execs = getApiExecs();
                ApiPluginExecutor exec;
                if (!execs.TryGetValue(fullName, out exec)) {
                    Log.WriteLine(LogLevel.Error, "API plugin {0} not found.", fullName);
                    return false;
                }
                var res = exec.ExecuteApi(args, streamHandle, eventHandle);
                return res;
            } catch (Exception ex) {
                Log.WriteLine(LogLevel.Error, "Exception in Execute({0}): {1}", command, ex.ToString());
                return false;
            }
        }

        public static bool Run(string command, IntPtr sessionHandle) {
            try {
                Log.WriteLine(LogLevel.Debug, "FreeSWITCH.Managed: attempting to run application '{0}'.", command);
                System.Diagnostics.Debug.Assert(sessionHandle != IntPtr.Zero, "sessionHandle is null.");
                var parsed = parseCommand(command);
                if (parsed == null) return false;
                var fullName = parsed[0];
                var args = parsed[1];

                AppPluginExecutor exec;
                var execs = getAppExecs();
                if (!execs.TryGetValue(fullName, out exec)) {
                    Log.WriteLine(LogLevel.Error, "App plugin {0} not found.", fullName);
                    return false;
                }
                return exec.Execute(args, sessionHandle);
            } catch (Exception ex) {
                Log.WriteLine(LogLevel.Error, "Exception in Run({0}): {1}", command, ex.ToString());
                return false;
            }
        }

        public static bool List(string command, IntPtr streamHandle, IntPtr eventHandle)
        {
            try {
                if (streamHandle != IntPtr.Zero) {
                    using (var stream = new Native.Stream(new Native.switch_stream_handle(streamHandle, false))) {
                        stream.Write("Available APIs:\n");

                        getApiExecs().Values.ForEach(x => stream.Write(string.Format("{0}: {1}\n", x.Name, String.Join(",", x.Aliases.ToArray()))));

                        stream.Write("Available Apps:\n");
                        getAppExecs().Values.ForEach(x => stream.Write(string.Format("{0}: {1}\n", x.Name, String.Join(",", x.Aliases.ToArray()))));
                    }
                } else {
                    Log.WriteLine(LogLevel.Info, "Available APIs:");
                    getApiExecs().Values.ForEach(x => Log.WriteLine(LogLevel.Info, "{0}: {1}", x.Name, String.Join(",", x.Aliases.ToArray())));
                    Log.WriteLine(LogLevel.Info, "Available Apps:");
                    getAppExecs().Values.ForEach(x => Log.WriteLine(LogLevel.Info, "{0}: {1}", x.Name, String.Join(",", x.Aliases.ToArray())));
                }
                return true;
            } catch (Exception ex) {
                Log.WriteLine(LogLevel.Error, "Exception listing managed modules: {0}", ex.ToString());
                return false;
            }
        }
    }

}
 
