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
 * ChannelTimetable.cs -- 
 *
 */
using System;
using System.Runtime.InteropServices;

namespace FreeSwitch.NET.Types
{
    public class ChannelTimetable
    {
        internal HandleRef marshaledObject;

        public Int64 Created
        {
            get { throw new NotImplementedException(); }
        }

        public Int64 Answered
        {
            get { throw new NotImplementedException(); }
        }

        public Int64 Hungup
        {
            get { throw new NotImplementedException(); }
        }
    }
}
