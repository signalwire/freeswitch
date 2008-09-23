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
 * Loader.cs -- mod_mono managed loader
 *
 */

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Linq;
using System.Reflection;

namespace FreeSWITCH
{
    internal static class Loader
    {
        // Stores a list of the loaded function types so we can instantiate them as needed
        static readonly Dictionary<string, Type> functions = new Dictionary<string, Type>(StringComparer.InvariantCultureIgnoreCase);
        // Only class name. Last in wins.
        static readonly Dictionary<string, Type> shortFunctions = new Dictionary<string, Type>(StringComparer.InvariantCultureIgnoreCase);

        #region Load/Unload

        public static bool Load()
        {
            string managedDir = Path.Combine(Native.freeswitch.SWITCH_GLOBAL_dirs.mod_dir, "dotnet");
            Log.WriteLine(LogLevel.Debug, "mod_mono_managed loader is starting with directory '{0}'.", managedDir);
            if (!Directory.Exists(managedDir)) {
                Log.WriteLine(LogLevel.Error, "Managed directory not found: {0}", managedDir);
                return false;
            }

            // This is a simple one-time loader to get things in memory
            // Some day we should allow reloading of modules or something
            loadAssemblies(managedDir)
                .SelectMany(a => a.GetExportedTypes())
                .Where(t => !t.IsAbstract)
                .Where(t => t.IsSubclassOf(typeof(AppFunction)) || t.IsSubclassOf(typeof(ApiFunction)))
                .ToList()
                .loadFunctions();

            return true;
        }

        // Be rather lenient in finding the Load and Unload methods
        static readonly BindingFlags methodBindingFlags =
            BindingFlags.Static | // Required
            BindingFlags.Public | BindingFlags.NonPublic | // Implementors might decide to make the load method private
            BindingFlags.IgnoreCase | // Some case insensitive languages?
            BindingFlags.FlattenHierarchy; // Allow inherited methods for hierarchies

        static void loadFunctions(this List<Type> allTypes)
        {
            foreach (var t in allTypes) {
                try {
                    if (functions.ContainsKey(t.FullName)) {
                        Log.WriteLine(LogLevel.Error, "Duplicate function {0}. Skipping.", t.FullName);
                        continue;
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
        }

        static List<Assembly> loadAssemblies(string managedDir)
        {
            return Directory.GetFiles(managedDir, "*.dll", SearchOption.AllDirectories)
                  .Select(f => Path.Combine(managedDir, f))
                  .Select(f => {
                      try {
                          return System.Reflection.Assembly.LoadFile(f);
                      }
                      catch (Exception ex) {
                          Log.WriteLine(LogLevel.Notice, "Assembly.LoadFile failed; skipping {0} ({1})", f, ex.Message);
                          return null;
                      }
                  })
                  .Where(a => a != null)
                  .Concat(new[] { System.Reflection.Assembly.GetExecutingAssembly() }) // Add in our own to load Demo or built-in things if added
                  .ToList();
        }

        public static void Unload()
        {
            Log.WriteLine(LogLevel.Debug, "mod_mono_managed Loader is unloading.");
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
                }
                catch (Exception ex) {
                    logException("ExecuteBackground", fullName, ex);
                }
            }).Start();
            return true;
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
            Log.WriteLine(LogLevel.Debug, "mod_mono attempting to run application '{0}'.", command);
            System.Diagnostics.Debug.Assert(sessionHandle != IntPtr.Zero, "sessionHandle is null.");
            var parsed = parseCommand(command);
            if (parsed == null) return false;
            var fullName = parsed[0];
            var args = parsed[1];

            var fType = getFunctionType<AppFunction>(fullName);
            if (fType == null) return false;

            using (var session = new Native.MonoSession(new Native.SWIGTYPE_p_switch_core_session(sessionHandle, false))) {
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
 