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
 * ManagedSession.cs -- ManagedSession additional functions
 *
 */
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Runtime.InteropServices;

namespace FreeSWITCH.Native
{
    // switch_status_t ManagedSession::run_dtmf_callback(void *input, switch_input_type_t itype)
    // But, process_callback_result is used to turn a string into a switch_status_t
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    delegate string DtmfCallback(IntPtr input, Native.switch_input_type_t itype);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    delegate void CdeclAction();

    // This callback is used for originate
    [UnmanagedFunctionPointerAttribute(CallingConvention.Cdecl)]
    public delegate switch_status_t switch_state_handler_t_delegate(IntPtr sessionPtr); 

    public partial class ManagedSession
    {
        // SWITCH_DECLARE(void) InitManagedSession(ManagedSession *session, MonoObject *dtmfDelegate, MonoObject *hangupDelegate)
        [DllImport("mod_managed", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        static extern void InitManagedSession(IntPtr sessionPtr, DtmfCallback dtmfDelegate, CdeclAction hangupDelegate);

        /// <summary>Initializes the native ManagedSession. Called after Originate completes successfully .</summary>
        internal void Initialize()
        {
            if (allocated == 0) {
                throw new InvalidOperationException("Cannot initialize a ManagedSession until it is allocated (originated successfully).");
            }
            // P/Invoke generated function pointers stick around until the delegate is collected
            // By sticking the delegates in fields, their lifetime won't be less than the session
            // So we don't need to worry about GCHandles and all that....
            // Info here: http://blogs.msdn.com/cbrumme/archive/2003/05/06/51385.aspx
            this._inputCallbackRef = inputCallback;
            this._hangupCallbackRef = hangupCallback;
            InitManagedSession(ManagedSession.getCPtr(this).Handle, this._inputCallbackRef, this._hangupCallbackRef);
            this._variables = new ChannelVariables(this);
        }
        DtmfCallback _inputCallbackRef;
        CdeclAction _hangupCallbackRef;

        /// <summary>Function to execute when this session hangs up.</summary>
        public Action HangupFunction { get; set; }

        void hangupCallback()
        {
            Log.WriteLine(LogLevel.Debug, "AppFunction is in hangupCallback.");
            try {
                var f = HangupFunction;
                if (f != null) f();
            }
            catch (Exception ex) {
                Log.WriteLine(LogLevel.Warning, "Exception in hangupCallback: {0}", ex.ToString());
            }
        }

        public Func<Char, TimeSpan, string> DtmfReceivedFunction { get; set; }

        public Func<Native.Event, string> EventReceivedFunction { get; set; }

        string inputCallback(IntPtr input, Native.switch_input_type_t inputType)
        {
            try {
                switch (inputType) {
                    case FreeSWITCH.Native.switch_input_type_t.SWITCH_INPUT_TYPE_DTMF:
                        using (var dtmf = new Native.switch_dtmf_t(input, false)) {
                            return dtmfCallback(dtmf);
                        }
                    case FreeSWITCH.Native.switch_input_type_t.SWITCH_INPUT_TYPE_EVENT:
                        using (var swevt = new Native.switch_event(input, false)) {
                            return eventCallback(swevt);
                        }
                    default:
                        return "";
                }
            } catch (Exception ex) {
                Log.WriteLine(LogLevel.Error, "InputCallback threw exception: " + ex.ToString());
                return "-ERR InputCallback Exception: " + ex.Message;
            }
        }

        string dtmfCallback(Native.switch_dtmf_t dtmf) {
            var f = DtmfReceivedFunction;
            return f == null ? "" 
                : f(((char)(byte)dtmf.digit), TimeSpan.FromMilliseconds(dtmf.duration));
        }

        string eventCallback(Native.switch_event swevt) {
            using (var evt = new FreeSWITCH.Native.Event(swevt, 0)) {
                var f = EventReceivedFunction;
                return f == null ? "" 
                    : f(evt);
            }
        }

        [Obsolete("Use static Originate method.", false)]
        public bool Originate(CoreSession aLegSession, string destination, TimeSpan timeout) {
            var res = 0 == this.originate(aLegSession, destination, (int)timeout.TotalMilliseconds, null);
            if (res) {
                this.Initialize();
            }
            return res;
        }


        // Creating these function pointers is a two-stage process. 
        // The delegate needs to be stored so it doesn't get GC'd, so we can't just return GetFunctionPointerForDelegate right away.

        /// <summary>Wraps a nice handler into a delegate suitable for reverse P/Invoke. This only currently works well for hangup/reporting handlers.</summary>
		public static switch_state_handler_t_delegate CreateStateHandlerDelegate(ManagedSession sess, Action<ManagedSession> handler)
		{
            // We create a ManagedSession on top of the session so callbacks can use it "nicely"
            // Then we sort of dispose it.
            switch_state_handler_t_delegate del = ptr => {
                    handler(sess);
                    return switch_status_t.SWITCH_STATUS_SUCCESS;
            };
            return del;
        }
        public static SWIGTYPE_p_f_p_switch_core_session__switch_status_t WrapStateHandlerDelegate(switch_state_handler_t_delegate del) {
            return new SWIGTYPE_p_f_p_switch_core_session__switch_status_t(Marshal.GetFunctionPointerForDelegate(del), false);
        }


        // These are needed on the ManagedSession bleg, so they don't get GC'd
        // while the B Leg is still around
        switch_state_handler_t_delegate originate_onhangup_delegate;
        switch_state_handler_t_delegate originate_ondestroy_delegate;
        switch_state_handler_table originate_table;
        GCHandle originate_keepalive_handle; // Make sure the B Leg is not collected and disposed until we run ondestroy

        switch_status_t originate_ondestroy_method(IntPtr channelPtr) {
            // CS_DESTROY lets the bleg be collected
            // and frees originate_table memory
            // Note that this (bleg ManagedSession) is invalid 
            // to touch right now - the unmanaged memory has already been free'd
            if (this.originate_keepalive_handle.IsAllocated) {
                this.originate_keepalive_handle.Free(); // GC can now collect this bleg
            }
            if (this.originate_table != null) {
                this.originate_table.Dispose();
                this.originate_table = null;
            }
            return switch_status_t.SWITCH_STATUS_SUCCESS;
        }

        /// <summary>
        /// Performs originate. Returns ManagedSession on success, null on failure.
        /// onHangup is called as a state handler, after the channel is truly hungup (CS_REPORTING).
        /// </summary>
        public static ManagedSession OriginateHandleHangup(CoreSession aLegSession, string destination, TimeSpan timeout, Action<ManagedSession> onHangup) {
            var bleg = new ManagedSession();

            bleg.originate_ondestroy_delegate = bleg.originate_ondestroy_method;
            bleg.originate_onhangup_delegate = CreateStateHandlerDelegate(bleg, sess_b => {
                if (onHangup != null) {
                    onHangup(sess_b);
                }
            });
            bleg.originate_table = new switch_state_handler_table();
            bleg.originate_table.on_reporting = WrapStateHandlerDelegate(bleg.originate_onhangup_delegate);
            bleg.originate_table.on_destroy = WrapStateHandlerDelegate(bleg.originate_ondestroy_delegate);
            bleg.originate_table.flags = (int)switch_state_handler_flag_t.SSH_FLAG_STICKY;
            var res = 0 == bleg.originate(aLegSession, destination, (int)timeout.TotalSeconds, bleg.originate_table);
            bleg.originate_keepalive_handle = GCHandle.Alloc(bleg, GCHandleType.Normal); // Prevent GC from eating the bleg
            if (res) {
                bleg.Initialize();
                return bleg;
            } else {
                // Dispose to free the lock
                // The bleg lives on with its unmanaged memory freed 
                // Until CS_DESTROY gets called
                bleg.Dispose(); 
                return null;
            }
        }

        // Convenience
        public bool IsAvailable {
            get { return this.Ready(); }
        }

        public Guid Uuid {
            get {
                if (allocated == 0) throw new InvalidOperationException("Session has not been initialized.");
                return new Guid(this.GetUuid());
            }
        }

        public switch_call_cause_t CallCause {
            get {
                if (allocated == 0) throw new InvalidOperationException("Session has not been initialized.");
                return freeswitch.switch_channel_get_cause(this.channel);
            }
        }
    }
}
