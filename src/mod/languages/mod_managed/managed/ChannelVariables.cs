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
 * ChannelVariables.cs -- Strongly typed channel variables for ManagedSession
 *
 */
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Runtime.InteropServices;

namespace FreeSWITCH.Native {
    public partial class ManagedSession {

        // Need to find a better place to put these - then make them public
        static readonly DateTime epoch = new DateTime(1970, 1, 1);
        static DateTime epochUsToDateTime(long us){
            return us == 0 ? 
                DateTime.MinValue :
                epoch.AddMilliseconds((double)((decimal)us / 1000m));
        }
        static bool strToBool(string s) {
            if (string.IsNullOrEmpty(s)) return false;
            switch (s.ToLowerInvariant()) {
                case "true":
                case "yes":
                case "on":
                case "enable":
                case "enabled":
                case "active":
                case "allow":
                    return true;
                default:
                    // Numbers are true
                    long tmp;
                    return long.TryParse(s, out tmp);
            }
        }
        static string boolToStr(bool b) {
            return b ? "true" : "false";
        }

        ChannelVariables _variables; // Set on ManagedSession init
        public ChannelVariables Variables {
            get {
                if (_variables == null) {
                    _variables = new ChannelVariables(this);
                }
                return _variables;
            }
        }

        /// <summary>Strongly typed access to common variables</summary>
        public class ChannelVariables {
            readonly ManagedSession sess;
            internal ChannelVariables(ManagedSession session) {
                this.sess = session;
            }

            public IDictionary<string, string> GetAllVariables() {
                var dic = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                var evt = Native.freeswitch.switch_channel_variable_first(sess.channel);
                while(evt != null) {
                    dic.Add(evt.name, evt.value);
                    evt = evt.next;
                }
                Native.freeswitch.switch_channel_variable_last(sess.channel);
                return dic;
            }

            /*** Settings ***/
            const string bypass_media = "bypass_media";
            public bool BypassMedia {
                get { return strToBool(sess.GetVariable(bypass_media)); }
                set { sess.SetVariable(bypass_media, boolToStr(value)); }
            }

            /*** Times ***/
            const string created_time = "created_time";
            const string answered_time = "answered_time";
            const string hangup_time = "hungup_time";
            const string progress_time = "progress_time";
            const string progress_media_time = "progress_media_time";
            const string transfer_time = "transfer_time";

            public DateTime CreatedTime {
                get { return epochUsToDateTime(long.Parse(sess.GetVariable(created_time))); }
            }

            DateTime? readUsecsDateTime(string varName) {
                var v = sess.GetVariable(varName);
                if (string.IsNullOrEmpty(v)) return null;
                else {
                    var d = epochUsToDateTime(long.Parse(v));
                    if (d == DateTime.MinValue) return null;
                    return d;
                }
            }

            public DateTime? AnsweredTime {
                get { return readUsecsDateTime(answered_time); }
            }
            public DateTime? HangupTime {
                get { return readUsecsDateTime(hangup_time); }
            }
            public DateTime? ProgressTime {
                get { return readUsecsDateTime(progress_time); }
            }
            public DateTime? ProgressMediaTime {
                get { return readUsecsDateTime(progress_media_time); }
            }
            public DateTime? TransferTime {
                get { return readUsecsDateTime(transfer_time); }
            }

        
            /*** SIP Variables ***/
            const string sofia_profile_name = "sofia_profile_name";
            const string sip_received_ip = "sip_received_ip";
            const string sip_received_port = "sip_received_port";
            const string sip_via_protocol = "sip_via_protocol";
            const string sip_from_user = "sip_from_user";
            const string sip_from_uri = "sip_from_uri";
            const string sip_from_host = "sip_from_host";
            const string sip_req_user = "sip_req_user";
            const string sip_req_uri = "sip_req_uri";
            const string sip_req_host = "sip_req_host";
            const string sip_to_user = "sip_to_user";
            const string sip_to_uri = "sip_to_uri";
            const string sip_to_host = "sip_to_host";
            const string sip_contact_user = "sip_contact_user";
            const string sip_contact_port = "sip_contact_port";
            const string sip_contact_uri = "sip_contact_uri";
            const string sip_contact_host = "sip_contact_host";
            const string sip_call_id = "sip_call_id";
            const string sip_destination_url = "sip_destination_url";
            const string sip_term_status = "sip_term_status";
            const string sip_term_cause = "sip_term_status";
            const string switch_r_sdp = "switch_r_sdp";
            const string switch_m_sdp = "switch_m_sdp";
            const string sip_hangup_phrase = "sip_hangup_phrase";


            short? readShort(string varName) {
                var s = sess.GetVariable(varName);
                if (string.IsNullOrEmpty(s)) return null;
                short res;
                if (short.TryParse(s, out res)) return res;
                else return null;
            }
            int? readInt(string varName) {
                var s = sess.GetVariable(varName);
                if (string.IsNullOrEmpty(s)) return null;
                int res;
                if (int.TryParse(s, out res)) return res;
                else return null;
            }

            // String suffix is added when the var is better represented as 
            // a different data type, but for now we return string

            public string SofiaProfileName { get { return sess.GetVariable(sofia_profile_name); } }
            public string SipCallID { get { return sess.GetVariable(sip_call_id); } }
            public string SipReceivedIP { get { return sess.GetVariable(sip_received_ip); } }
            public short? SipReceivedPort { get { return readShort(sip_received_port); } }
            public string SipViaProtocolString { get { return sess.GetVariable(sip_via_protocol); } }
            public string SipFromUser { get { return sess.GetVariable(sip_from_user); } }
            public string SipFromUriString { get { return sess.GetVariable(sip_from_uri); } }
            public string SipFromHost { get { return sess.GetVariable(sip_from_host); } }
            public string SipReqUser { get { return sess.GetVariable(sip_req_user); } }
            public string SipReqUriString { get { return sess.GetVariable(sip_req_uri); } }
            public string SipReqHost { get { return sess.GetVariable(sip_req_host); } }
            public string SipToUser { get { return sess.GetVariable(sip_to_user); } }
            public string SipToUriString { get { return sess.GetVariable(sip_to_uri); } }
            public string SipToHost { get { return sess.GetVariable(sip_to_host); } }
            public string SipContactUser { get { return sess.GetVariable(sip_contact_user); } }
            public string SipContactUriString { get { return sess.GetVariable(sip_contact_uri); } }
            public string SipContactHost { get { return sess.GetVariable(sip_contact_host); } }
            public short? SipContactPort { get { return readShort(sip_contact_port); } }
            public string SipDestinationUrlString { get { return sess.GetVariable(sip_destination_url); } }
            public int? SipTermStatus { get { return readInt(sip_term_status); } }
            public int? SipTermCause { get { return readInt(sip_term_cause); } }
            public string SwitchRSdp { get { return sess.GetVariable(switch_r_sdp); } }
            public string SwitchMSdp { get { return sess.GetVariable(switch_m_sdp); } }
            public string SipHangupPhrase { get { return sess.GetVariable(sip_hangup_phrase); } }

            /*** Other ***/

            const string proto_specific_hangup_cause = "proto_specific_hangup_cause";
            const string hangup_cause = "hangup_cause";
            const string hangup_cause_q850 = "hangup_cause_q850";
            const string originate_disposition = "originate_disposition";
            const string direction = "direction";

            public string ProtoSpecificHangupCause { get { return sess.GetVariable(proto_specific_hangup_cause); } }
            public string HangupCauseString { get { return sess.GetVariable(hangup_cause); } }
            public int? HangupCauseQ850 { get { return readInt(hangup_cause_q850); } }
            public string OriginateDispositionString { get { return sess.GetVariable(originate_disposition); } }

            public Native.switch_call_direction_t CallDirection {
                get {
                    var s = sess.GetVariable(direction);
                    if (string.IsNullOrEmpty(s)) return switch_call_direction_t.SWITCH_CALL_DIRECTION_INBOUND; // I guess
                    return s.ToLowerInvariant() == "inbound" ? switch_call_direction_t.SWITCH_CALL_DIRECTION_INBOUND : switch_call_direction_t.SWITCH_CALL_DIRECTION_OUTBOUND;
                }

            }
        }
    }
}