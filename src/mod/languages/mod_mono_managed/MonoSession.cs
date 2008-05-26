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
 * 
 * MonoSession.cs -- MonoSession additional functions
 *
 */
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace FreeSWITCH.Native
{
    //switch_status_t MonoSession::run_dtmf_callback(void *input, switch_input_type_t itype)
    // But, process_callback_result is used to turn a string into a switch_status_t
    using DtmfCallback = Func<IntPtr, Native.switch_input_type_t, string>;
    public partial class MonoSession
    {
        // SWITCH_DECLARE(void) InitMonoSession(MonoSession *session, MonoObject *dtmfDelegate, MonoObject *hangupDelegate)
        [System.Runtime.CompilerServices.MethodImpl(System.Runtime.CompilerServices.MethodImplOptions.InternalCall)]
        static extern void InitMonoSession(IntPtr sessionPtr, DtmfCallback dtmfDelegate, Action hangupDelegate);

        internal void SetDelegates(DtmfCallback dtmfCallback, Action hangupHook)
        {
            InitMonoSession(MonoSession.getCPtr(this).Handle, dtmfCallback, hangupHook);
        }
    }
}
