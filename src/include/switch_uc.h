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
 *
 *
 * switch_uc.h -- Configuration File Parser
 *
 */
/**
 * @file switch_uc.h
 * @brief Basic Configuration File Parser
 * @see uc
 */


#ifndef SWITCH_UC_H
#define SWITCH_UC_H

#include <switch.h>

SWITCH_BEGIN_EXTERN_C 

//+++start+++ added by yy for auth modules,2019.04.04
typedef enum {
	MODULE_CRM = (1 << 0),
	MODULE_PMS = (1 << 1),
	MODULE_SCHEDULING = (1 << 2),
	MODULE_BILLING = (1 << 3),
	MODULE_TTS_ASR = (1 << 4),
	MODULE_FAX = (1 << 5),
	MODULE_SYN_RING = (1 << 6),
	MODULE_NUM_ATTR = (1 << 7),
	MODULE_LRIO = (1 << 8),
	MODULE_SR = (1 << 9),
	MODULE_CLOUD = (1 << 10),
	MODULE_SLA = (1 << 11),
	MODULE_HA = (1 << 12)
} device_modules_auth_sn_type_t;
//+++end+++ added by yy for auth modules,2019.04.04


SWITCH_DECLARE(switch_status_t) switch_core_at88_test(int seq,int id,int code_type,int rewrite);

SWITCH_DECLARE(switch_status_t) switch_core_encrypt(switch_core_flag_t flags, switch_bool_t console, const char **err);

SWITCH_DECLARE(int)  switch_core_get_ini_key_string(char *title,char *key,char *defaultstr,char *returnstr,char *filename);

SWITCH_DECLARE(switch_bool_t) switch_get_soft(void);
SWITCH_DECLARE(switch_bool_t) switch_get_work(void);


SWITCH_DECLARE(unsigned int) switch_get_maxch_num(void);
SWITCH_DECLARE(unsigned int) switch_get_maxext_num(void);
SWITCH_DECLARE(unsigned int) switch_get_maxapi_num(void);

SWITCH_DECLARE(unsigned int) switch_get_valid_period(void);

SWITCH_DECLARE(unsigned int) switch_get_remaining_valid_period(void);

SWITCH_DECLARE(unsigned long long) switch_core_get_device_modules_auth_sn(void);

SWITCH_DECLARE(unsigned long long) switch_core_get_device_features_auth_sn(void);

SWITCH_DECLARE(unsigned long long) switch_core_mem_free(void);

SWITCH_DECLARE(unsigned long long) switch_core_mem_total(void);

SWITCH_DECLARE(unsigned long long) switch_core_flash_size(void);

SWITCH_DECLARE(unsigned long long) switch_core_flash_use(void);

SWITCH_DECLARE(char *) switch_core_device_type(void);

SWITCH_DECLARE(char *) switch_core_device_sn(void);

SWITCH_DECLARE(char *) switch_core_soft_ver(void);

SWITCH_DECLARE(char *) switch_core_uboot_ver(void);

SWITCH_DECLARE(char *) switch_core_kernel_ver(void);

SWITCH_DECLARE(switch_status_t) switch_core_check_usb_key(const char **err);

SWITCH_DECLARE(void) switch_close_auth(void);


SWITCH_END_EXTERN_C
/** @} */
#endif
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
