/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application Call Detail Recorder module
 * Copyright 2006, Author: Yossi Neiman of Cartis Solutions, Inc. <freeswitch AT cartissolutions.com>
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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application Call Detail Recorder module
 *
 * The Initial Developer of the Original Code is
 * Chris Parker <cparker AT segv.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Chris Parker <cparker AT segv.org>
 *
 * Description: Contains definitions and structs used by the radius cdr module.
 *
 * mod_radius_cdr.h
 *
 */

#ifndef MODRADIUSCDR
#define MODRADIUSCDR

#define	PW_FS_PEC		27880

#define PW_FS_AVPAIR		1
#define PW_FS_CLID		2
#define PW_FS_DIALPLAN		3
#define PW_FS_SRC		4
#define PW_FS_DST		5
#define PW_FS_SRC_CHANNEL	6
#define PW_FS_DST_CHANNEL	7
#define PW_FS_ANI		8
#define PW_FS_ANIII		9
#define PW_FS_LASTAPP		10
#define PW_FS_LASTDATA		11
#define PW_FS_DISPOSITION	12
#define PW_FS_HANGUPCAUSE	13
#define PW_FS_BILLUSEC		15
#define PW_FS_AMAFLAGS		16
#define PW_FS_RDNIS		17
#define PW_FS_CONTEXT		18
#define PW_FS_SOURCE		19
#define PW_FS_CALLSTARTDATE	20
#define PW_FS_CALLANSWERDATE	21
#define PW_FS_CALLTRANSFERDATE	22
#define PW_FS_CALLENDDATE	23
#define PW_FS_DIRECTION	24
#define PW_FS_OTHER_LEG_ID 25


#endif
