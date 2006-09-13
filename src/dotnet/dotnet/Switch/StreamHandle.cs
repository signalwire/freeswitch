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
 * LoadableModule.cs -- 
 *
 */
using System;
using System.Runtime.InteropServices;
using FreeSwitch.NET.Types;
using FreeSwitch.NET.Marshaling.Types;

namespace FreeSwitch.NET
{
    /*
     * SWITCH_DECLARE(switch_status_t) switch_loadable_module_load_module(char *dir, char *fname)
     * SWITCH_DECLARE(switch_status_t) switch_loadable_module_build_dynamic(char *filename,
     * SWITCH_DECLARE(switch_status_t) switch_loadable_module_init()
     * SWITCH_DECLARE(void) switch_loadable_module_shutdown(void)
     * SWITCH_DECLARE(switch_endpoint_interface_t *) switch_loadable_module_get_endpoint_interface(char *name)
     * SWITCH_DECLARE(switch_codec_interface_t *) switch_loadable_module_get_codec_interface(char *name)
     * SWITCH_DECLARE(switch_dialplan_interface_t *) switch_loadable_module_get_dialplan_interface(char *name)
     * SWITCH_DECLARE(switch_timer_interface_t *) switch_loadable_module_get_timer_interface(char *name)
     * SWITCH_DECLARE(switch_application_interface_t *) switch_loadable_module_get_application_interface(char *name)
     * SWITCH_DECLARE(switch_api_interface_t *) switch_loadable_module_get_api_interface(char *name)
     * SWITCH_DECLARE(switch_file_interface_t *) switch_loadable_module_get_file_interface(char *name)
     * SWITCH_DECLARE(switch_speech_interface_t *) switch_loadable_module_get_speech_interface(char *name)
     * SWITCH_DECLARE(switch_directory_interface_t *) switch_loadable_module_get_directory_interface(char *name)
     * SWITCH_DECLARE(int) switch_loadable_module_get_codecs(switch_memory_pool_t *pool, switch_codec_interface_t **array,
     * SWITCH_DECLARE(int) switch_loadable_module_get_codecs_sorted(switch_codec_interface_t **array,
     * SWITCH_DECLARE(switch_status_t) switch_api_execute(char *cmd, char *arg, char *retbuf, switch_size_t len)
     */
    public partial class Switch
    {

    }
}
