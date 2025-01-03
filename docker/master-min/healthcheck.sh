#!/bin/sh
#
# FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
# Copyright (C) 2005-2016, Anthony Minessale II <anthm@freeswitch.org>
#
# Version: MPL 1.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/F
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
#
# The Initial Developer of the Original Code is
# Michael Jerris <mike@jerris.com>
# Portions created by the Initial Developer are Copyright (C)
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
#  Sergey Safarov <s.safarov@gmail.com>
#

# Check FreeSwitch status
fs_cli -x status | grep -q ^UP || exit 1

# Check erlang related modules is registered on epmd daemon
KAZOO_EXIST=$(fs_cli -x "module_exists mod_kazoo")
ERLANG_EXITS=$(fs_cli -x "module_exists mod_erlang_event")

if [ "$KAZOO_EXIST" == "true" -o "$ERLANG_EXITS" == "true" ]; then
    /usr/bin/epmd -names | grep -qE "^name freeswitch at port" || exit 1
fi

exit 0
