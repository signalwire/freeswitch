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
 * Util.cs - misc functions
 *
 */
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Runtime.InteropServices;
using System.Reflection;

namespace FreeSWITCH {
    public static class FSUtil {
        // IntPtr cPtr, bool futureUse
        static readonly Type[] swigConstructorTypes = new [] { typeof(IntPtr), typeof(bool) };
        public static T CreateSwigTypePointer<T>(this IntPtr cPtr) {
            var ty = typeof(T);
            var bflags = BindingFlags.Default | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static | BindingFlags.Instance;
            var cons = ty.GetConstructor(bflags, null, swigConstructorTypes, null);
            if (cons == null) throw new ArgumentException(ty.Name + " constructor not found.");
            return (T)cons.Invoke(new object[] { cPtr, false });
        }
    }
}
