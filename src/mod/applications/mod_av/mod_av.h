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
 * Ken Rice <krice@freeswitch.org>
 * Paul D. Tinsley <pdt at jackhammer.org>
 * Bret McDanel <trixter AT 0xdecafbad.com>
 * Marcel Barbulescu <marcelbarbulescu@gmail.com>
 * Raymond Chandler <intralanman@gmail.com>
 * Emmanuel Schmidbauer <e.schmidbauer@gmail.com>
 * Jakub Karolczyk <jakub.karolczyk@signalwire.com>
 *
 *
 * mod_av.h -- LibAV mod
 *
 */

#ifndef MOD_AV_H
#define MOD_AV_H

#define LIBAVCODEC_V 59 /* FFmpeg version >= 5.1 */
#define LIBAVCODEC_6_V 60 /* FFmpeg version >= 6.0 */
#define LIBAVCODEC_7_V 61 /* FFmpeg version >= 7.0 */
#define LIBAVCODEC_61_V 31 /* FFmpeg version >= 6.1 */
#define LIBAVFORMAT_V 59 /* FFmpeg version >= 5.1 */
#define LIBAVFORMAT_6_V 60 /* FFmpeg version >= 6.0 */
#define LIBAVFORMAT_7_V 61 /* FFmpeg version >= 7.0 */
#define LIBAVFORMAT_61_V 16 /* FFmpeg version >= 6.1 */
#define LIBAVUTIL_V 57 /* FFmpeg version >= 5.1 */

struct mod_av_globals {
	int debug;
};

extern struct mod_av_globals mod_av_globals;

void show_codecs(switch_stream_handle_t *stream);
void show_formats(switch_stream_handle_t *stream);

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

