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
 * CoreSessionMarshaler.cs -- 
 *
 */
using System;
using System.Runtime.InteropServices;
using FreeSwitch.Types;
using FreeSwitch.Marshaling.Types;

namespace FreeSwitch.Marshaling
{
    class CoreSessionMarshaler : ICustomMarshaler
    {
        private static CoreSessionMarshaler Instance = new CoreSessionMarshaler();

        public static ICustomMarshaler GetInstance(string s)
        {
            return Instance;
        }

        public void CleanUpManagedData(object o)
        {
        }

        public void CleanUpNativeData(IntPtr pNativeData)
        {
        }

        public int GetNativeDataSize()
        {
            return IntPtr.Size;
        }

        public IntPtr MarshalManagedToNative(object obj)
        {
            CoreSession coreSession = (CoreSession)obj;

            Console.WriteLine("CoreSession: Marshalling Managed to Native");

            if (coreSession.marshaledObject.Handle != IntPtr.Zero)
            {

                Console.WriteLine("Returning: 0x{0:x}", coreSession.marshaledObject.Handle.ToInt32());
                return coreSession.marshaledObject.Handle;
            }
            else
            {
                CoreSessionMarshal coreSessionMarshal = new CoreSessionMarshal();
                IntPtr coreSessionPtr = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(CoreSessionMarshal)));

                Marshal.StructureToPtr(coreSessionMarshal, coreSessionPtr, true);

                coreSession.marshaledObject = new HandleRef(coreSessionMarshal, coreSessionPtr);

                Console.WriteLine("CoreSession: NO OBJECT EXISTS OMG");
                Console.WriteLine("Returning: 0x{0:x}", coreSession.marshaledObject.Handle.ToInt32());
                return coreSession.marshaledObject.Handle;
            }
        }

        public object MarshalNativeToManaged(IntPtr coreSessionPtr)
        {
            CoreSessionMarshal coreSessionMarshal = new CoreSessionMarshal();
            CoreSession        coreSession        = new CoreSession();

            Console.WriteLine("CoreSession: Marshalling Native to Managed");

            Marshal.PtrToStructure(coreSessionPtr, coreSessionMarshal);

            coreSession.marshaledObject = new HandleRef(coreSessionMarshal, coreSessionPtr);

            return coreSession;
        }
    }
}
