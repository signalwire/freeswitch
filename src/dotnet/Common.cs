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
 * Common.cs -- 
 *
 */
using System;
using System.Reflection;

namespace FreeSwitch
{
    public class Common
    {
        public static void DumpHex(string label, IntPtr pointer, int length)
        {
            Console.WriteLine("DUMP-{0}:", label);

            DumpHex(pointer, length);
        }

        public static void DumpProperties(string label, object dumpObject)
        {
            Type type = dumpObject.GetType();
            PropertyInfo[] properties = type.GetProperties();

            foreach (PropertyInfo p in properties)
            {
                Console.WriteLine("%%% - {0}: {1}:{2}", label, p.Name, p.GetValue(dumpObject, null));
            }
        }

        public static void DumpHex(IntPtr pointer, int length)
        {
            if (pointer == IntPtr.Zero)
                throw new NullReferenceException();

            for (int i = 0; i < length; i++)
            {
                IntPtr offset = new IntPtr(pointer.ToInt32() + i);

                if (i % 20 == 0)
                    Console.Write("\n0x{0:x}: ", offset.ToInt32());

                Console.Write("{0:x2} ", System.Runtime.InteropServices.Marshal.ReadByte(offset));
            }

            Console.WriteLine("\n");
        }
    }
}
