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
 * Event.cs -- 
 *
 */
using System;
using System.Collections;
using System.Text;
using System.Runtime.InteropServices;

namespace FreeSwitch.NET.Types
{
    public enum Priority
    {
        SWITCH_PRIORITY_NORMAL,
        SWITCH_PRIORITY_LOW,
        SWITCH_PRIORITY_HIGH
    }

    public class Event
    {
        internal HandleRef marshaledObject;

        public EventType EventId
        {
            get { throw new NotImplementedException(); }
        }

        public Priority Priority
        {
            get { throw new NotImplementedException(); }
        }

        public string Owner
        {
            get { throw new NotImplementedException(); }
        }

        public IntPtr Subclass
        {
            get { throw new NotImplementedException(); }
        }

        public ArrayList Headers
        {
            get { throw new NotImplementedException(); }
        }

        public string Body
        {
            get { throw new NotImplementedException(); }
        }

        public IntPtr BindUserData
        {
            get { throw new NotImplementedException(); }
        }

        public IntPtr EventUserData
        {
            get { throw new NotImplementedException(); }
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public class EventSubclass
    {
        private string owner;
        private string name;
    }

    [StructLayout(LayoutKind.Sequential)]
    public class EventHeader
    {
        public string Name;
        public string Value;
    }

    public enum EventType
    {
        SWITCH_EVENT_CUSTOM,
        SWITCH_EVENT_CHANNEL_STATE,
        SWITCH_EVENT_CHANNEL_ANSWER,
        SWITCH_EVENT_CHANNEL_HANGUP,
        SWITCH_EVENT_API,
        SWITCH_EVENT_LOG,
        SWITCH_EVENT_INBOUND_CHAN,
        SWITCH_EVENT_OUTBOUND_CHAN,
        SWITCH_EVENT_STARTUP,
        SWITCH_EVENT_SHUTDOWN,
        SWITCH_EVENT_ALL
    }

}
