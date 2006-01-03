/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 *
 * switch_config.h -- Configuration File Parser
 *
 */
/*! \file switch_config.h
    \brief Configuration File Parser
*/

#ifndef SWITCH_CONFIG_H
#define SWITCH_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <switch.h>

struct switch_config {
    FILE *file;
    char *path;
    char category[256];
    char buf[1024];
    int lineno;
};

typedef struct switch_config switch_config;

SWITCH_DECLARE(int) switch_config_open_file(switch_config *cfg, char *file_path);
SWITCH_DECLARE(void) switch_config_close_file(switch_config *cfg);
SWITCH_DECLARE(int) switch_config_next_pair(switch_config *cfg, char **var, char **val);

#ifdef __cplusplus
}
#endif


#endif
