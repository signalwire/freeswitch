/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2006, James Martelletti <james@nerdc0re.com>
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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * James Martelletti <james@nerdc0re.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * James Martelletti <james@nerdc0re.com>
 *
 *
 * Module.cs -- 
 *
 */
using System;
using System.Collections;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using FreeSwitch.NET;
using FreeSwitch.Types;
using FreeSwitch.Modules;
using FreeSwitch.Marshaling.Types;

namespace FreeSwitch.NET
{
    /// <summary>
    /// Base class for all Freeswitch.NET modules
    /// </summary>
    /// <example>
    /// public class Example : Module
    /// {
    ///     public Example()
    ///     {
    ///         AddApiInterface(new ExampleApi());
    ///         AddApplicationInterface(new ExampleApplication());
    ///                  
    ///         Register();
    ///     }    
    /// }
    /// </example>
	public class Module
	{
        private LoadableModuleInterfaceMarshal module_interface = new LoadableModuleInterfaceMarshal();
        private LoadableModuleMarshal          module           = new LoadableModuleMarshal();

        private ArrayList applicationInterfaces = new ArrayList();
        private ArrayList apiInterfaces         = new ArrayList();

        private ModuleLoad load;
        private string     name;
        private string     filename = Assembly.GetCallingAssembly().GetName().Name;
        
        /// <summary>
        /// Module constructor
        /// </summary>
		public Module()
		{
            Console.WriteLine("*** Creating new module object");

            load = new ModuleLoad(Load);
		}

        /// <summary>
        /// Implementation of ModuleLoad Delegate
        /// </summary>
        /// <param name="module"></param>
        /// <param name="name"></param>
        /// <returns></returns>
        public Status Load(ref IntPtr module, string name)
        {
            /* Allocate some unmanaged mem for the ModuleInterface */
            module = Marshal.AllocHGlobal(Marshal.SizeOf(module_interface));
            
            /* Set the module_name field of the LoadableModuleInterface */
            module_interface.module_name = Marshal.StringToHGlobalAnsi(filename);

            /* Grab the first ApiInterface in our array and add a pointer to this in our ModuleInterface*/
            Api apiIndex = (Api)apiInterfaces[0];
            
            module_interface.api_interface = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(ApiInterfaceMarshal)));
            
            Marshal.StructureToPtr(apiIndex.handle.Wrapper, module_interface.api_interface, true);
            
            /* For each Application interface */
            Application applicationIndex = (Application)applicationInterfaces[0];
            
            module_interface.application_interface = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(ApplicationInterfaceMarshal)));
            
            Marshal.StructureToPtr(applicationIndex.handle.Wrapper, module_interface.application_interface, true);                

            /* Finally, marshal the moduleinterface class and return */
            Marshal.StructureToPtr(module_interface, module, true);
                        
            return Status.Success;
        }

        /// <summary>
        /// AddApiInterface
        /// </summary>
        /// <param name="apiInterface"></param>
        public void AddApiInterface(Api apiInterface)
        {
            /* Create a new ApiInterface type and allocate unmanaged mem */
            ApiInterfaceMarshal apiInterfaceMarshal = new ApiInterfaceMarshal();
            IntPtr              apiInterfacePtr     = Marshal.AllocHGlobal(Marshal.SizeOf(apiInterfaceMarshal));

            /* Allocate umanaged mem to to interface fields so GC doesn't disturb them */
            apiInterfaceMarshal.interface_name = Marshal.StringToHGlobalAnsi(apiInterface.Name);
            apiInterfaceMarshal.desc           = Marshal.StringToHGlobalAnsi(apiInterface.Description);
            apiInterfaceMarshal.function       = new ApiFunction(apiInterface.ApiFunction);
            
            /* Set the handle of the managed object */
            apiInterface.handle = new HandleRef(apiInterfaceMarshal, apiInterfacePtr);

            Marshal.StructureToPtr(apiInterfaceMarshal, apiInterface.handle.Handle, true);

            /* Check to see whether there is already an interface defined, if there is then
             * we want to set the *next pointer of the last element before adding the new
             * interface to the array
             */
            if (apiInterfaces.Count > 0)
            {
                Api                 apiInterfaceElement = (Api)apiInterfaces[apiInterfaces.Count - 1];
                ApiInterfaceMarshal apiInterfacePrev    = (ApiInterfaceMarshal)apiInterfaceElement.handle.Wrapper;

                apiInterfacePrev.next = apiInterface.handle.Handle;
            }

            /* Finally, add our new interface to the array */
            apiInterfaces.Add(apiInterface);
        }

        /// <summary>
        /// AddApplicationInterface
        /// </summary>
        /// <param name="applicationInterface"></param>
        public void AddApplicationInterface(Application applicationInterface)
        {
            /* Create a new ApplicationInterface type and allocate unmanaged mem */
            ApplicationInterfaceMarshal applicationInterfaceMarshal = new ApplicationInterfaceMarshal();
            IntPtr                      applicationInterfacePtr     = Marshal.AllocHGlobal(Marshal.SizeOf(applicationInterfaceMarshal));

            /* Allocate umanaged mem to to interface fields so GC doesn't disturb them */
            applicationInterfaceMarshal.interface_name = Marshal.StringToHGlobalAnsi(applicationInterface.Name);
            applicationInterfaceMarshal.syntax         = Marshal.StringToHGlobalAnsi(applicationInterface.Syntax);
            applicationInterfaceMarshal.long_desc      = Marshal.StringToHGlobalAnsi(applicationInterface.LongDescription);
            applicationInterfaceMarshal.short_desc     = Marshal.StringToHGlobalAnsi(applicationInterface.ShortDescription);

            applicationInterfaceMarshal.application_function = new ApplicationFunction(applicationInterface.ApplicationFunction);

            /* Set the handle of the managed object */
            applicationInterface.handle = new HandleRef(applicationInterfaceMarshal, applicationInterfacePtr);

            Marshal.StructureToPtr(applicationInterfaceMarshal, applicationInterface.handle.Handle, true);

            /* Check to see whether there is already an interface defined, if there is then
             * we want to set the *next pointer of the last element before adding the new
             * interface to the array
             */
            if (applicationInterfaces.Count > 0)
            {
                Application                 applicationInterfaceElement = (Application)applicationInterfaces[applicationInterfaces.Count - 1];
                ApplicationInterfaceMarshal applicationInterfacePrev    = (ApplicationInterfaceMarshal)applicationInterfaceElement.handle.Wrapper;

                applicationInterfacePrev.next = applicationInterface.handle.Handle;
            }

            /* Finally, add our new interface to the array */
            applicationInterfaces.Add(applicationInterface);
        }

        /// <summary>
        /// Register
        /// </summary>
        public void Register()
        {
            module.module_load = new ModuleLoad(Load);

            Switch.switch_loadable_module_build_dynamic(filename,
                                                        module.module_load,
                                                        module.module_runtime,
                                                        module.module_shutdown);
        }

        
	}
}	
