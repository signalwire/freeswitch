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
 * David Brazier <David.Brazier@360crm.co.uk>
 * 
 * Loader.cs -- mod_mono managed loader
 *
 */

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;

namespace FreeSWITCH
{
    internal static class Loader
    {
        // Stores a list of the loaded function types so we can instantiate them as needed
        static Dictionary<string, Type> functions = new Dictionary<string, Type>(StringComparer.OrdinalIgnoreCase);
        // Only class name. Last in wins.
        static Dictionary<string, Type> shortFunctions = new Dictionary<string, Type>(StringComparer.OrdinalIgnoreCase);

        #region Load/Unload

        static string managedDir;

        public static bool Load()
        {
            managedDir = Path.Combine(Native.freeswitch.SWITCH_GLOBAL_dirs.mod_dir, "managed");
            Log.WriteLine(LogLevel.Debug, "FreeSWITCH.Managed loader is starting with directory '{0}'.", managedDir);
            if (!Directory.Exists(managedDir)) {
                Log.WriteLine(LogLevel.Error, "Managed directory not found: {0}", managedDir);
                return false;
            }

            AppDomain.CurrentDomain.AssemblyResolve += (_, rargs) => {
                Log.WriteLine(LogLevel.Debug, "Trying to resolve assembly '{0}'.", rargs.Name);
                if (rargs.Name == Assembly.GetExecutingAssembly().FullName) return Assembly.GetExecutingAssembly();
                var path = Path.Combine(managedDir, rargs.Name + ".dll");
                Log.WriteLine(LogLevel.Debug, "Resolving to: '" + path + "'.");
                return File.Exists(path) ? Assembly.LoadFile(path) : null;
            };

            InitManagedDelegates(_run, _execute, _executeBackground, _loadAssembly);

            // This is a simple one-time loader to get things in memory
            // Some day we should allow reloading of modules or something
            var allTypes = loadAssemblies(managedDir).SelectMany(a => a.GetExportedTypes());
            loadFunctions(allTypes);

            return true;
        }

        public static bool LoadAssembly(string filename) {
            try {
                string path = Path.Combine(managedDir, filename);
                if (!File.Exists(path)) {
                    Log.WriteLine(LogLevel.Error, "File not found: '{0}'.", path);
                    return false;
                }
                var asm = Assembly.LoadFile(path);
                loadFunctions(asm.GetExportedTypes());
                return true;
            } catch (Exception ex) {
                Log.WriteLine(LogLevel.Error, "Exception in LoadAssembly('{0}'): {1}", filename, ex.ToString());
                return false;
            }
        }

        delegate bool ExecuteDelegate(string cmd, IntPtr streamH, IntPtr eventH);
        delegate bool ExecuteBackgroundDelegate(string cmd);
        delegate bool RunDelegate(string cmd, IntPtr session);
        delegate bool LoadAssemblyDelegate(string filename);
        static readonly ExecuteDelegate _execute = Execute;
        static readonly ExecuteBackgroundDelegate _executeBackground = ExecuteBackground;
        static readonly RunDelegate _run = Run;
        static readonly LoadAssemblyDelegate _loadAssembly = LoadAssembly;
        //SWITCH_MOD_DECLARE(void) InitManagedDelegates(runFunction run, executeFunction execute, executeBackgroundFunction executeBackground, loadAssemblyFunction loadAssembly)  
        [DllImport("mod_managed", CharSet = CharSet.Ansi)]
        static extern void InitManagedDelegates(RunDelegate run, ExecuteDelegate execute, ExecuteBackgroundDelegate executeBackground, LoadAssemblyDelegate loadAssembly);

        // Be rather lenient in finding the Load and Unload methods
        static readonly BindingFlags methodBindingFlags =
            BindingFlags.Static | // Required
            BindingFlags.Public | BindingFlags.NonPublic | // Implementors might decide to make the load method private
            BindingFlags.IgnoreCase | // Some case insensitive languages?
            BindingFlags.FlattenHierarchy; // Allow inherited methods for hierarchies

        static void loadFunctions(IEnumerable<Type> allTypes)
        {
            var functions = new Dictionary<string, Type>(Loader.functions, StringComparer.OrdinalIgnoreCase);
            var shortFunctions = new Dictionary<string, Type>(Loader.shortFunctions, StringComparer.OrdinalIgnoreCase);
            var filteredTypes = allTypes
                .Where(t => !t.IsAbstract)
                .Where(t => t.IsSubclassOf(typeof(AppFunction)) || t.IsSubclassOf(typeof(ApiFunction)));
            foreach (var t in filteredTypes) {
                try {
                    if (functions.ContainsKey(t.FullName)) {
                        functions.Remove(t.FullName);
                        Log.WriteLine(LogLevel.Warning, "Replacing function {0}.", t.FullName);
                    }
                    var loadMethod = t.GetMethod("Load", methodBindingFlags, null, Type.EmptyTypes, null);
                    var shouldLoad = Convert.ToBoolean(loadMethod.Invoke(null, null)); // We don't require the Load method to return a bool exactly
                    if (shouldLoad) {
                        Log.WriteLine(LogLevel.Notice, "Function {0} loaded.", t.FullName);
                        functions.Add(t.FullName, t);
                        shortFunctions[t.Name] = t;
                    }
                    else {
                        Log.WriteLine(LogLevel.Notice, "Function {0} requested not to be loaded.", t.FullName);
                    }
                }
                catch (Exception ex) {
                    logException("Load", t.FullName, ex);
                }
            } 
            Loader.functions = functions;
            Loader.shortFunctions = shortFunctions;
        }

        static Assembly[] loadAssemblies(string managedDir)
        {
            // load the modules in the mod/managed directory
            Log.WriteLine(LogLevel.Notice, "loadAssemblies: {0}", managedDir);
            foreach (string s in Directory.GetFiles(managedDir, "*.dll", SearchOption.AllDirectories))
            {
                string f = Path.Combine(managedDir, s);
                try {
                    Log.WriteLine(LogLevel.Debug, "Loading '{0}'.", f);
                    System.Reflection.Assembly.LoadFile(f);
                }
                catch (Exception ex) {
                    Log.WriteLine(LogLevel.Notice, "Assembly.LoadFile failed; skipping {0} ({1})", f, ex.Message);
                }
            }

            return AppDomain.CurrentDomain.GetAssemblies();  // Includes anything else already loaded
        }

        public static void Unload()
        {
            Log.WriteLine(LogLevel.Debug, "FreeSWITCH.Managed Loader is unloading.");
            foreach (var t in functions.Values) {
                try {
                    var unloadMethod = t.GetMethod("Unload", methodBindingFlags, null, Type.EmptyTypes, null);
                    unloadMethod.Invoke(null, null);
                }
                catch (Exception ex) {
                    logException("Unload", t.FullName, ex);
                }
            }
        }

        #endregion

        #region Execution

        static Type getFunctionType<TFunction>(string fullName)
        {
            Type t;
            if (!functions.TryGetValue(fullName, out t) || !t.IsSubclassOf(typeof(TFunction))) {
                if (!shortFunctions.TryGetValue(fullName, out t) || !t.IsSubclassOf(typeof(TFunction))) {
                    Log.WriteLine(LogLevel.Error, "Could not find function {0}.", fullName);
                    return null;
                }
            }
            return t;
        }

        static readonly char[] spaceArray = new[] { ' ' };
        /// <summary>Returns a string couple containing the module name and arguments</summary>
        static string[] parseCommand(string command)
        {
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
            }
            else {
                return args;
            }
        }

        public static bool ExecuteBackground(string command)
        {
            try {
                var parsed = parseCommand(command);
                if (parsed == null) return false;
                var fullName = parsed[0];
                var args = parsed[1];

                var fType = getFunctionType<ApiFunction>(fullName);
                if (fType == null) return false;

                new System.Threading.Thread(() => {
                    try {
                        var f = (ApiFunction)Activator.CreateInstance(fType);
                        f.ExecuteBackground(args);
                        Log.WriteLine(LogLevel.Debug, "ExecuteBackground in {0} completed.", fullName);
                    } catch (Exception ex) {
                        logException("ExecuteBackground", fullName, ex);
                    }
                }).Start();
                return true;
            } catch (Exception ex) {
                Log.WriteLine(LogLevel.Error, "Exception in ExecuteBackground({0}): {1}", command, ex.ToString());
                return false;
            }
        }

        public static bool Execute(string command, IntPtr streamHandle, IntPtr eventHandle)
        {
            System.Diagnostics.Debug.Assert(streamHandle != IntPtr.Zero, "streamHandle is null.");
            var parsed = parseCommand(command);
            if (parsed == null) return false;
            var fullName = parsed[0];
            var args = parsed[1];
            
            var fType = getFunctionType<ApiFunction>(fullName);
            if (fType == null) return false;

            using (var stream = new Native.Stream(new Native.switch_stream_handle(streamHandle, false)))
            using (var evt = eventHandle == IntPtr.Zero ? null : new Native.Event(new Native.switch_event(eventHandle, false), 0)) {
                
                try {
                    var f = (ApiFunction)Activator.CreateInstance(fType);
                    f.Execute(stream, evt, args);
                    return true;
                }
                catch (Exception ex) {
                    logException("Execute", fullName, ex);
                    return false;
                }
            }
        }

        /// <summary>Runs an application function.</summary>
        public static bool Run(string command, IntPtr sessionHandle)
        {
            Log.WriteLine(LogLevel.Debug, "FreeSWITCH.Managed: attempting to run application '{0}'.", command);
            System.Diagnostics.Debug.Assert(sessionHandle != IntPtr.Zero, "sessionHandle is null.");
            var parsed = parseCommand(command);
            if (parsed == null) return false;
            var fullName = parsed[0];
            var args = parsed[1];

            var fType = getFunctionType<AppFunction>(fullName);
            if (fType == null) return false;

            using (var session = new Native.ManagedSession(new Native.SWIGTYPE_p_switch_core_session(sessionHandle, false))) {
                session.Initialize();
                session.SetAutoHangup(false);
                try {
                    var f = (AppFunction)Activator.CreateInstance(fType);
                    f.RunInternal(session, args);
                    return true;
                }
                catch (Exception ex) {
                    logException("Run", fullName, ex);
                    return false;
                }
            }
        }

        static void logException(string action, string moduleName, Exception ex)
        {
            Log.WriteLine(LogLevel.Error, "{0} exception in {1}: {2}", action, moduleName, ex.Message);
            Log.WriteLine(LogLevel.Debug, "{0} exception: {1}", moduleName, ex.ToString());
        }

        #endregion
    }

}
 