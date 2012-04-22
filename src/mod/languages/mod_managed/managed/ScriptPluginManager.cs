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
 * ScriptPluginManager.cs -- Dynamic compilation and script execution
 *
 */

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.CodeDom;
using System.CodeDom.Compiler;
using System.IO;
using System.Reflection;
using System.Reflection.Emit;


namespace FreeSWITCH {

    public enum ScriptContextType {
        None,
        App,
        Api,
        ApiBackground,
    }

    public static class Script {

        [ThreadStatic]
        internal static ScriptContextType contextType;
        [ThreadStatic]
        internal static object context;

        public static ScriptContextType ContextType { get { return contextType; } }

        public static ApiContext GetApiContext() {
            return getContext<ApiContext>(ScriptContextType.Api);
        }
        public static ApiBackgroundContext GetApiBackgroundContext() {
            return getContext<ApiBackgroundContext>(ScriptContextType.ApiBackground);
        }
        public static AppContext GetAppContext() {
            return getContext<AppContext>(ScriptContextType.App);
        }

        public static T getContext<T>(ScriptContextType sct) {
            var ctx = context;
            if (ctx == null) throw new InvalidOperationException("Current context is null.");
            if (contextType != sct) throw new InvalidOperationException("Current ScriptContextType is not " + sct.ToString() + ".");
            return (T)ctx;
        }

    }

    internal class ScriptPluginManager : PluginManager {

        protected override bool LoadInternal(string fileName) {
            Assembly asm;
            if (Path.GetExtension(fileName).ToLowerInvariant() == ".exe") {
                asm = Assembly.LoadFrom(fileName);
            } else {
                asm = compileAssembly(fileName);
            }
            if (asm == null) return false;

            return processAssembly(fileName, asm);
        }

        Assembly compileAssembly(string fileName) {
            var comp = new CompilerParameters();
            var mainRefs = new List<string> { 
                Path.Combine(Native.freeswitch.SWITCH_GLOBAL_dirs.mod_dir, "FreeSWITCH.Managed.dll"),
                "System.dll", "System.Xml.dll", "System.Data.dll" 
            };
            var extraRefs = new List<string> {
                "System.Core.dll", 
                "System.Xml.Linq.dll",
            };
            comp.ReferencedAssemblies.AddRange(mainRefs.ToArray());
            comp.ReferencedAssemblies.AddRange(extraRefs.ToArray());
            CodeDomProvider cdp;
            var ext = Path.GetExtension(fileName).ToLowerInvariant();
            switch (ext) {
                case ".fsx":
                    cdp = CodeDomProvider.CreateProvider("f#");
                    break;
                case ".csx":
#if (CLR_VERSION40)
                    cdp = new Microsoft.CSharp.CSharpCodeProvider(new Dictionary<string, string> { { "CompilerVersion", "v4.0" } });
#else
                    cdp = new Microsoft.CSharp.CSharpCodeProvider(new Dictionary<string, string> { { "CompilerVersion", "v3.5" } });
#endif
                    break;
                case ".vbx":
#if (CLR_VERSION40)
                    cdp = new Microsoft.VisualBasic.VBCodeProvider(new Dictionary<string, string> { { "CompilerVersion", "v4.0" } });
#else
                    cdp = new Microsoft.VisualBasic.VBCodeProvider(new Dictionary<string, string> { { "CompilerVersion", "v3.5" } });
#endif
                    break;
                case ".jsx":
                    // Have to figure out better JS support
                    cdp = CodeDomProvider.CreateProvider("js");
                    extraRefs.ForEach(comp.ReferencedAssemblies.Remove);
                    break;
                default:
                    if (CodeDomProvider.IsDefinedExtension(ext)) {
                        cdp = CodeDomProvider.CreateProvider(CodeDomProvider.GetLanguageFromExtension(ext));
                    } else {
                        return null;
                    }
                    break;
            }

            comp.GenerateInMemory = true;
            comp.GenerateExecutable = true;

            Log.WriteLine(LogLevel.Info, "Compiling {0}", fileName);
            var res = cdp.CompileAssemblyFromFile(comp, fileName);

            var errors = res.Errors.Cast<CompilerError>().Where(x => !x.IsWarning).ToList();
            if (errors.Count > 0) {
                Log.WriteLine(LogLevel.Error, "There were {0} errors compiling {1}.", errors.Count, fileName);
                foreach (var err in errors) {
                    if (string.IsNullOrEmpty(err.FileName)) {
                        Log.WriteLine(LogLevel.Error, "{0}: {1}", err.ErrorNumber, err.ErrorText);
                    } else {
                        Log.WriteLine(LogLevel.Error, "{0}: {1}:{2}:{3} {4}", err.ErrorNumber, err.FileName, err.Line, err.Column, err.ErrorText);
                    }
                }
                return null;
            }
            Log.WriteLine(LogLevel.Info, "File {0} compiled successfully.", fileName);
            return res.CompiledAssembly;
        }

        bool processAssembly(string fileName, Assembly asm) {
            // Call the entrypoint once, to initialize apps that need their main called
            var entryPoint = getEntryDelegate(asm.EntryPoint);
            try { entryPoint(); } catch {  }

            // Check for loading
            var allTypes = asm.GetExportedTypes();
            if (!RunLoadNotify(allTypes)) return false;

            // Scripts can specify classes too
            var opts = GetOptions(allTypes);
            AddApiPlugins(allTypes, opts);
            AddAppPlugins(allTypes, opts);

            // Add the script executors
            var name = Path.GetFileName(fileName);
            var aliases = new List<string> { name };
            this.ApiExecutors.Add(new ApiPluginExecutor(name, aliases, () => new ScriptApiWrapper(entryPoint), opts));
            this.AppExecutors.Add(new AppPluginExecutor(name, aliases, () => new ScriptAppWrapper(entryPoint), opts));

            return true;
        }

        class ScriptApiWrapper : IApiPlugin {

            readonly Action entryPoint;
            public ScriptApiWrapper(Action entryPoint) {
                this.entryPoint = entryPoint;
            }

            public void Execute(ApiContext context) {
                Script.contextType = ScriptContextType.Api;
                Script.context = context;
                try {
                    entryPoint();
                } finally {
                    Script.contextType = ScriptContextType.None;
                    Script.context = null;
                }
            }

            public void ExecuteBackground(ApiBackgroundContext context) {
                Script.contextType = ScriptContextType.ApiBackground;
                Script.context = context;
                try {
                    entryPoint();
                } finally {
                    Script.contextType = ScriptContextType.None;
                    Script.context = null;
                }
            }

        }

        class ScriptAppWrapper : IAppPlugin {
            
            readonly Action entryPoint;
            public ScriptAppWrapper(Action entryPoint) {
                this.entryPoint = entryPoint;
            }

            public void Run(AppContext context) {
                Script.contextType = ScriptContextType.App;
                Script.context = context;
                try {
                    entryPoint();
                } finally {
                    Script.contextType = ScriptContextType.None;
                    Script.context = null;
                }
            }

        }

        static Action getEntryDelegate(MethodInfo entryPoint) {
            if (!entryPoint.IsPublic || !entryPoint.DeclaringType.IsPublic) {
                Log.WriteLine(LogLevel.Error, "Entry point: {0}.{1} is not public. This may cause errors with Mono.",
                    entryPoint.DeclaringType.FullName, entryPoint.Name);
            }
            var dm = new DynamicMethod(entryPoint.DeclaringType.Assembly.GetName().Name + "_entrypoint_" + entryPoint.DeclaringType.FullName + entryPoint.Name, null, null, true);
            var il = dm.GetILGenerator();
            var args = entryPoint.GetParameters();
            if (args.Length > 1) throw new ArgumentException("Cannot handle entry points with more than 1 parameter.");
            if (args.Length == 1) {
                if (args[0].ParameterType != typeof(string[])) throw new ArgumentException("Entry point paramter must be a string array.");
                il.Emit(OpCodes.Ldc_I4_0);
                il.Emit(OpCodes.Newarr, typeof(string));
            }
            il.EmitCall(OpCodes.Call, entryPoint, null);
            if (entryPoint.ReturnType != typeof(void)) {
                il.Emit(OpCodes.Pop);
            }
            il.Emit(OpCodes.Ret);
            return (Action)dm.CreateDelegate(typeof(Action));
        }

    }
}
