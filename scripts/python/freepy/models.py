""" 
FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>

Version: MPL 1.1

The contents of this file are subject to the Mozilla Public License Version
1.1 (the "License"); you may not use this file except in compliance with
the License. You may obtain a copy of the License at
http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS IS" basis,
WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
for the specific language governing rights and limitations under the
License.

The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application

The Initial Developer of the Original Code is
Anthony Minessale II <anthmct@yahoo.com>
Portions created by the Initial Developer are Copyright (C)
the Initial Developer. All Rights Reserved.

Contributor(s): Traun Leyden <tleyden@branchcut.com>
"""
 
"""
Data models for objects inside freeswitch
"""

import re

class ConfMember:

    def __init__(self, rawstring):
        self.rawstring = rawstring
        self.member_id = None
        self.member_uri = None
        self.uuid = None
        self.caller_id_name = None
        self.caller_id_number = None
        self.flags = None
        self.volume_in = None
        self.volume_out = None
        self.energy_level = None

        self.parse(self.rawstring)
        
    def parse(self, rawstring):
        """
        1;sofia/mydomain.com/user@somewhere.com;898e6552-24ab-11dc-9df7-9fccd4095451;FreeSWITCH;0000000000;hear|speak;0;0;300
        """
        fields = rawstring.split(";")
        self.member_id = fields[0]
        self.member_uri = fields[1]
        self.uuid = fields[2]
        self.caller_id_name = fields[3]
        self.caller_id_number = fields[4]
        self.flags = fields[5]
        self.volume_in = fields[6]
        self.volume_out = fields[7]
        self.energy_level = fields[8]

    def brief_member_uri(self):
        """
        if self.member_uri is sofia/mydomain.com/foo@bar.com
        return foo@bar.com
        """
        if not self.member_uri:
            return None

        if self.member_uri.find("/") == -1:
            return self.member_uri
        r = self.member_uri.split("/")[-1]  # tokenize on "/" and return last item
        return r

    def __repr__(self):
        return self.__str__()
    
    def __str__(self):
        return "%s (%s)" % (self.member_id, self.member_uri)
