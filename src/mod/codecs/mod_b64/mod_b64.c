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
 * Brian K. West <brian@freeswitch.org>
 *
 * mod_b64.c -- The B64 ultra-low delay audio codec (http://www.b64-codec.org/)
 *
 */

#include "switch.h"

SWITCH_MODULE_LOAD_FUNCTION(mod_b64_load);
SWITCH_MODULE_DEFINITION(mod_b64, mod_b64_load, NULL, NULL);

#define HEADER "----Come to ClueCon Every August!    "
#define HLEN strlen(HEADER)

static switch_status_t switch_codec_b64_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_codec_b64_destroy(switch_codec_t *codec)
{

	codec->private_info = NULL;
	return SWITCH_STATUS_SUCCESS;
}

static int rot(char *p, char min, char max, int i)
{
	int r = 0;
	int c;
	
	if (*p >= min && *p <= max) {
		if ((c = *p + i) <= max) {
			*p = c;
		} else {
			*p = *p - i;
		}
		r++;
	}

	return r;
}

static void rot13_buffer(char *buf, size_t len)
{
	char *p = buf;
	char *e = buf + len;

	while(p < e) {
		if (!rot(p, 'a', 'z', 13)) {
			rot(p, 'A', 'Z', 13);
		}
		p++;
	}
}

static switch_status_t switch_codec_b64_encode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *decoded_data,
										  uint32_t decoded_data_len,
										  uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate,
										  unsigned int *flag)
{
	char *p = encoded_data;
	strncpy(p, HEADER, *encoded_data_len - HLEN);
	p += HLEN;
	encoded_data = p;
	*encoded_data_len -= HLEN;

	switch_b64_encode((unsigned char *) decoded_data, decoded_data_len, (unsigned char *) encoded_data, *encoded_data_len);
	*encoded_data_len = strlen(encoded_data);
	rot13_buffer(encoded_data, *encoded_data_len);
	*encoded_data_len += HLEN;

	return SWITCH_STATUS_SUCCESS;

}

static switch_status_t switch_codec_b64_decode(switch_codec_t *codec,
										  switch_codec_t *other_codec,
										  void *encoded_data,
										  uint32_t encoded_data_len,
										  uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate,
										  unsigned int *flag)
{

	char *p = encoded_data;

	if (strncmp(p, HEADER, HLEN)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "HEADER CHECK ERROR!");
		return SWITCH_STATUS_FALSE;
	}

	p += HLEN;
	encoded_data = p;
	encoded_data_len -= HLEN;
	
	rot13_buffer(encoded_data, encoded_data_len);
	switch_b64_decode(encoded_data, decoded_data, *decoded_data_len);
	*decoded_data_len = codec->implementation->decoded_bytes_per_packet;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_b64_load)
{
	switch_codec_interface_t *codec_interface;
	int samples = 160;
	int bytes = 320;
	int mss = 20000;
	int x = 0;
	int rate = 8000;
	int bits = 171200;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_CODEC(codec_interface, "B64 (STANDARD)");

	for (x = 0; x < 3; x++) {
		switch_core_codec_add_implementation(pool, codec_interface, SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
											 116,	/* the IANA code number */
											 "b64",/* the IANA code name */
											 NULL,	/* default fmtp to send (can be overridden by the init function) */
											 rate,	/* samples transferred per second */
											 rate,	/* actual samples transferred per second */
											 bits,	/* bits transferred per second */
											 mss,	/* number of microseconds per frame */
											 samples,	/* number of samples per frame */
											 bytes,	/* number of bytes per frame decompressed */
											 0,	/* number of bytes per frame compressed */
											 1,	/* number of channels represented */
											 1,	/* number of frames per network packet */
											 switch_codec_b64_init,	/* function to initialize a codec handle using this implementation */
											 switch_codec_b64_encode,	/* function to encode raw data into encoded data */
											 switch_codec_b64_decode,	/* function to decode encoded data into raw data */
											 switch_codec_b64_destroy);	/* deinitalize a codec handle using this implementation */
		
		bytes *= 2;
		samples *= 2;
		rate *= 2;
		bits *= 2;
		
	}
	
	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

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

