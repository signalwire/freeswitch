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
 * XmlSearchBinding.cs - Helpers for switch_xml_bind_search_function
 *
 */
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Runtime.InteropServices;
using System.Reflection;
using FreeSWITCH.Native;

namespace FreeSWITCH {

    public class SwitchXmlSearchBinding : IDisposable {

        public class XmlBindingArgs {
            public string Section { get; set; }
            public string TagName { get; set; }
            public string KeyName { get; set; }
            public string KeyValue { get; set; }
            public switch_event Parameters { get; set; }
        }

        //typedef switch_xml_t (*switch_xml_search_function_t) (const char *section,
        //                                              const char *tag_name, const char *key_name, const char *key_value, switch_event_t *params,
        //                                              void *user_data);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        delegate IntPtr switch_xml_search_function_delegate(string section, string tag_name, string key_name, string key_value, IntPtr param, IntPtr user_data);

        readonly switch_xml_search_function_delegate del; // Prevent GC
        readonly SWIGTYPE_p_f_p_q_const__char_p_q_const__char_p_q_const__char_p_q_const__char_p_switch_event_t_p_void__p_switch_xml function;

        private SwitchXmlSearchBinding(SWIGTYPE_p_f_p_q_const__char_p_q_const__char_p_q_const__char_p_q_const__char_p_switch_event_t_p_void__p_switch_xml function,
            switch_xml_search_function_delegate origDelegate) {
            this.function = function;
            this.del = origDelegate;
        }
        bool disposed;
        public void Dispose() {
            dispose();
            GC.SuppressFinalize(this);
        }
        void dispose() {
            if (disposed) return;
            // HACK: FS crashes if we unbind after shutdown is pretty complete. This is still a race condition.
            if (freeswitch.switch_core_ready() == switch_bool_t.SWITCH_FALSE) return; 
            freeswitch.switch_xml_unbind_search_function_ptr(this.function);
            disposed = true;
        }
        ~SwitchXmlSearchBinding() {
            dispose();
        }

        public static IDisposable Bind(Func<XmlBindingArgs, string> f, switch_xml_section_enum_t sections) {
            switch_xml_search_function_delegate boundFunc = (section, tag, key, keyval, param, userData) => {
                var args = new XmlBindingArgs { Section = section, TagName = tag, KeyName = key, KeyValue = keyval, Parameters = new switch_event(param, false) };
                var xmlStr = f(args);
                var fsxml = string.IsNullOrEmpty(xmlStr) ? null : freeswitch.switch_xml_parse_str_dynamic(xmlStr, switch_bool_t.SWITCH_TRUE);
                return switch_xml.getCPtr(fsxml).Handle;
            };
            var fp = Marshal.GetFunctionPointerForDelegate(boundFunc);
            var swigFp = new SWIGTYPE_p_f_p_q_const__char_p_q_const__char_p_q_const__char_p_q_const__char_p_switch_event_t_p_void__p_switch_xml(fp, false);
            var res = freeswitch.switch_xml_bind_search_function_ret(swigFp, (uint)sections, null, null);
            if (res != switch_status_t.SWITCH_STATUS_SUCCESS) {
                throw new InvalidOperationException("Call to switch_xml_bind_search_function_ret failed, result: " + res + ".");
            }
            return new SwitchXmlSearchBinding(swigFp, boundFunc);
        }
    }
}