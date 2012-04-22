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
 * EventBinding.cs - Helpers for switch_event_bind function
 *
 */
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Runtime.InteropServices;
using System.Reflection;
using FreeSWITCH.Native;

namespace FreeSWITCH
{
    public class SwitchEventWrap : switch_event, IDisposable
    {
        private bool disposed;
        private SWIGTYPE_p_p_switch_event p_p_switch_event;
        private IntPtr native_ptr_ptr;

        internal SwitchEventWrap(IntPtr ptrPtr) : base((IntPtr)Marshal.PtrToStructure(ptrPtr, typeof(IntPtr)), false)
        {
            native_ptr_ptr = ptrPtr;
            p_p_switch_event = new SWIGTYPE_p_p_switch_event(ptrPtr, false);
        }

        ~SwitchEventWrap()
        {
            dispose();
        }

        public override void Dispose()
        {
            dispose();
            GC.SuppressFinalize(this);
        }

        internal void dispose()
        {
            if (disposed) return;
            disposed = true;
            freeswitch.switch_event_destroy(p_p_switch_event);
            Marshal.FreeCoTaskMem(native_ptr_ptr);
        }
    }

    public class EventBinding : IDisposable
    {

        public class EventBindingArgs : EventArgs
        {
            public switch_event EventObj { get; set; }
        }

        //typedef void (*switch_event_callback_t) (switch_event_t *);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        delegate void switch_event_callback_delegate(IntPtr event_data);

        readonly switch_event_callback_delegate del; // Prevent GC
        readonly SWIGTYPE_p_f_p_switch_event__void function;

        private EventBinding(SWIGTYPE_p_f_p_switch_event__void function, switch_event_callback_delegate origDelegate)
        {
            this.function = function;
            this.del = origDelegate;
        }
        bool disposed;
        public void Dispose()
        {
            dispose();
            GC.SuppressFinalize(this);
        }
        void dispose()
        {
            if (disposed) return;
            // HACK: FS crashes if we unbind after shutdown is pretty complete. This is still a race condition.
            if (freeswitch.switch_core_ready() == switch_bool_t.SWITCH_FALSE) return;
            freeswitch.switch_event_unbind_callback(this.function);
            disposed = true;
        }
        ~EventBinding()
        {
            dispose();
        }
        public static switch_event SwitchEventDupe(switch_event evt)
        {
            IntPtr clone_ptr_ptr = Marshal.AllocCoTaskMem(IntPtr.Size);
            freeswitch.switch_event_dup(new SWIGTYPE_p_p_switch_event(clone_ptr_ptr, false), evt);
            SwitchEventWrap dupe_evt = new SwitchEventWrap(clone_ptr_ptr);
            return dupe_evt;
        }
        public static IDisposable Bind(string id, switch_event_types_t event_types, string subclass_name, Action<EventBindingArgs> f, bool dupe)
        {
            switch_event_callback_delegate boundFunc;
            if (dupe)
            {
                boundFunc = (eventObj) =>
                {
                    var args = new EventBindingArgs { EventObj = SwitchEventDupe(new switch_event(eventObj,false)) };
                    f(args);
                };
            }
            else
            {
                boundFunc = (eventObj) =>
                {
                    var args = new EventBindingArgs { EventObj = new switch_event(eventObj, false) };
                    f(args);
                };
            }
            var fp = Marshal.GetFunctionPointerForDelegate(boundFunc);
            var swigFp = new SWIGTYPE_p_f_p_switch_event__void(fp, false);
            var res = freeswitch.switch_event_bind(id, event_types, subclass_name, swigFp, null);
            if (res != switch_status_t.SWITCH_STATUS_SUCCESS)
            {
                throw new InvalidOperationException("Call to switch_event_bind failed, result: " + res + ".");
            }
            return new EventBinding(swigFp, boundFunc);
        }
    }
}