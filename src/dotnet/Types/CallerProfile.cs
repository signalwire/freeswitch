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
 * CallerProfile.cs -- 
 *
 */
using System;
using System.Runtime.InteropServices;
using FreeSwitch.Marshaling.Types;

namespace FreeSwitch.Types
{
    public class CallerProfile
    {
        internal HandleRef marshaledObject;

        public static CallerProfile New(MemoryPool pool,
                             string username,
                             string dialplan,
                             string callerIdName,
                             string callerIdNumber,
                             string networkAddress,
                             string ani,
                             string ani2,
                             string rdnis,
                             string source,
                             string context,
                             string destinationNumber)
        {
            return Switch.switch_caller_profile_new(pool, username, dialplan, callerIdName, callerIdNumber, networkAddress, ani, ani2, rdnis, source, context, destinationNumber);
        }

        public string GetFieldByName(string field)
        {
            IntPtr ptr;
            
            ptr = Switch.switch_caller_get_field_by_name(this, field);

            if (ptr != IntPtr.Zero)
                return Marshal.PtrToStringAnsi(ptr);
            else
                return "";
        }


		/*
         * Properties
         */
        public string Dialplan
        {
            get { return GetFieldByName("dialplan"); }
        }

        public string CallerIdName
        {
            get { return GetFieldByName("caller_id_name"); }
        }

        public string CallerIdNumber
        {
            get { return GetFieldByName("caller_id_number"); }
        }

        public string NetworkAddress
        {
            get { return GetFieldByName("network_addr"); }
        }

        public string Ani
        {
            get { return GetFieldByName("ani"); }
        }

        public string Ani2
        {
            get { return GetFieldByName("ani2"); }
        }

        public string Rdnis
        {
            get { return GetFieldByName("rdnis"); }
        }

        public string Source
        {
            get { return GetFieldByName("source"); }
        }

        public string Context
        {
            get { return GetFieldByName("context"); }
        }

        public string DestinationNumber
        {
            get { return GetFieldByName("destination_number"); }
        }

        public string ChannelName
        {
            get { return GetFieldByName("chan_name"); }
        }

        public string Uuid
        {
            get{ return GetFieldByName("uuid"); }
        }

        public string Username
        {
            get { return GetFieldByName("username"); }
        }
    }
}
