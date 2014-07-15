/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 * Michael Jerris <mike@jerris.com>
 * Paul D. Tinsley <pdt at jackhammer.org>
 * Marcel Barbulescu <marcelbarbulescu@gmail.com>
 * Joseph Sullivan <jossulli@amazon.com>
 * Seven Du <dujinfang@gmail.com>
 *
 * switch_version.c -- Version Functions
 *
 */
#include <switch.h>
#include <switch_version.h>

const char *switch_version_major_str = SWITCH_VERSION_MAJOR;
const char *switch_version_minor_str = SWITCH_VERSION_MINOR;
const char *switch_version_micro_str = SWITCH_VERSION_MICRO;
const char *switch_version_revision_str = SWITCH_VERSION_REVISION;
const char *switch_version_revision_human_str = SWITCH_VERSION_REVISION_HUMAN;
const char *switch_version_full_str = SWITCH_VERSION_FULL;
const char *switch_version_full_human_str = SWITCH_VERSION_FULL_HUMAN;

SWITCH_DECLARE(const char *)switch_version_major(void) {return switch_version_major_str;}
SWITCH_DECLARE(const char *)switch_version_minor(void) {return switch_version_minor_str;}
SWITCH_DECLARE(const char *)switch_version_micro(void) {return switch_version_micro_str;}

SWITCH_DECLARE(const char *)switch_version_revision(void) {return switch_version_revision_str;}
SWITCH_DECLARE(const char *)switch_version_revision_human(void) {return switch_version_revision_human_str;}
SWITCH_DECLARE(const char *)switch_version_full(void) {return switch_version_full_str;}
SWITCH_DECLARE(const char *)switch_version_full_human(void) {return switch_version_full_human_str;}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
