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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / DAHDI codec module
 *
 * The Initial Developer of the Original Code is
 * Moises Silva <moy@sangoma.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Moises Silva <moy@sangoma.com>
 *
 * mod_dahdi_codec -- DAHDI Codecs (G729A 8.0kbit, G723.1 5.3kbit)
 *
 * Thanks to Voiceway for sponsoring this module and Neocenter for providing the DAHDI hardware to test
 *
 */

#include <switch.h>
#include <g711.h>
#include <linux/types.h> /* __u32 */
#include <sys/ioctl.h>

/*#define DEBUG_DAHDI_CODEC 1*/

#define CODEC_G729_IANA_CODE 18
#define CODEC_G723_IANA_CODE 4

switch_mutex_t *transcoder_counter_mutex;
static uint32_t total_encoders = 0;
static uint32_t total_encoders_usage = 0;
static uint32_t total_decoders = 0;
static uint32_t total_decoders_usage = 0;

/* 
   Zaptel/DAHDI definitions to not require the headers installed
   Zaptel and DAHDI are binary compatible (at least in the transcoder interface)
 */

#define DAHDI_TC_CODE                   'T'
#define DAHDI_TC_ALLOCATE               _IOW(DAHDI_TC_CODE, 1, struct dahdi_transcoder_formats)
#define DAHDI_TC_GETINFO                _IOWR(DAHDI_TC_CODE, 2, struct dahdi_transcoder_info)

#define DAHDI_FORMAT_G723_1  (1 << 0)
#define DAHDI_FORMAT_ULAW    (1 << 2)
#define DAHDI_FORMAT_SLINEAR (1 << 6)
#define DAHDI_FORMAT_G729A   (1 << 8)

struct dahdi_transcoder_formats {
	__u32   srcfmt;
	__u32   dstfmt;
};

struct dahdi_transcoder_info {
	__u32 tcnum;
	char name[80];
	__u32 numchannels;
	__u32 dstfmts;
	__u32 srcfmts;
};

static const char transcoding_device_dahdi[] = "/dev/dahdi/transcode";
static const char transcoder_name_dahdi[] = "DAHDI";
static const char transcoding_device_zap[] = "/dev/zap/transcode";
static const char transcoder_name_zap[] = "Zap";
static const char *transcoding_device = NULL;
static const char *transcoder_name = NULL;

SWITCH_MODULE_LOAD_FUNCTION(mod_dahdi_codec_load);
SWITCH_MODULE_DEFINITION(mod_dahdi_codec, mod_dahdi_codec_load, NULL, NULL);

struct dahdi_context {
	int32_t encoding_fd;
	int32_t decoding_fd;
	uint8_t codec_r;
};

static int32_t switch_dahdi_get_transcoder(struct dahdi_transcoder_formats *fmts)
{
	int32_t fd = open(transcoding_device, O_RDWR);
	if (fd < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
				"Failed to open %s transcoder device: %s.\n", 
				transcoder_name, strerror(errno));
		return -1;
	}
	if (ioctl(fd, DAHDI_TC_ALLOCATE, fmts)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
				"Failed to attach to transcoder: %s.\n", 
				strerror(errno));
		close(fd);
		return -1;
	}
	if (fmts->srcfmt & DAHDI_FORMAT_ULAW) {
		switch_mutex_lock(transcoder_counter_mutex);
		total_encoders_usage++;
		switch_mutex_unlock(transcoder_counter_mutex);
	} else {
		switch_mutex_lock(transcoder_counter_mutex);
		total_decoders_usage++;
		switch_mutex_unlock(transcoder_counter_mutex);
	}
	return fd;
}

static switch_status_t switch_dahdi_init(switch_codec_t *codec, switch_codec_flag_t flags, const switch_codec_settings_t *codec_settings)
{
	uint32_t encoding, decoding;
	struct dahdi_context *context = NULL;
	struct dahdi_transcoder_formats fmts;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Switch DAHDI init called.\n");

	encoding = (flags & SWITCH_CODEC_FLAG_ENCODE);
	decoding = (flags & SWITCH_CODEC_FLAG_DECODE);

	if (!(encoding || decoding)) {
#ifdef DEBUG_DAHDI_CODEC
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No encoding or decoding requested for DAHDI transcoder?.\n");
#endif
		return SWITCH_STATUS_FALSE;
	}

	if (!(context = switch_core_alloc(codec->memory_pool, sizeof(*context)))) {
#ifdef DEBUG_DAHDI_CODEC
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to allocate memory for dahdi codec context.\n");
#endif
		return SWITCH_STATUS_FALSE;
	}

	if (encoding) {
		fmts.srcfmt = DAHDI_FORMAT_ULAW;
		fmts.dstfmt = (codec->implementation->ianacode == CODEC_G729_IANA_CODE) 
			? DAHDI_FORMAT_G729A : DAHDI_FORMAT_G723_1;
		context->encoding_fd = switch_dahdi_get_transcoder(&fmts);
		if (context->decoding_fd < 0) {
#ifdef DEBUG_DAHDI_CODEC
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "encoding requested and denied with %d/%d.\n",
				fmts.srcfmt, fmts.dstfmt);
#endif
			return SWITCH_STATUS_FALSE;
		}
		/* ulaw requires 8 times more storage than g729 and 12 times more than G723, right? */
		context->codec_r = (codec->implementation->ianacode == CODEC_G729_IANA_CODE) 
			? 8 : 12;
#ifdef DEBUG_DAHDI_CODEC
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Encoding requested and granted with %d/%d.\n",
				fmts.srcfmt, fmts.dstfmt);
#endif
	}
	if (decoding) {
		fmts.dstfmt = DAHDI_FORMAT_ULAW;
		fmts.srcfmt = (codec->implementation->ianacode == CODEC_G729_IANA_CODE) 
			? DAHDI_FORMAT_G729A : DAHDI_FORMAT_G723_1;
		context->decoding_fd = switch_dahdi_get_transcoder(&fmts);
		if (context->decoding_fd < 0) {
#ifdef DEBUG_DAHDI_CODEC
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Decoding requested and denied with %d/%d.\n",
				fmts.srcfmt, fmts.dstfmt);
#endif
			return SWITCH_STATUS_FALSE;
		}
#ifdef DEBUG_DAHDI_CODEC
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Decoding requested and granted with %d/%d.\n",
				fmts.srcfmt, fmts.dstfmt);
#endif
	}
	codec->private_info = context;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_dahdi_encode(switch_codec_t *codec,
					   switch_codec_t *other_codec,
					   void *decoded_data,
					   uint32_t decoded_data_len,
					   uint32_t decoded_rate, void *encoded_data, uint32_t *encoded_data_len, uint32_t *encoded_rate,
					   unsigned int *flag)
{
	int32_t res;
	short *dbuf_linear;
	unsigned char *ebuf_g729;
	unsigned char ebuf_ulaw[decoded_data_len/2];
	uint32_t i;
	struct dahdi_context *context = NULL;
#ifdef DEBUG_DAHDI_CODEC
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Switch DAHDI encode called to encode %d bytes.\n", decoded_data_len);
#endif
	context = codec->private_info;
	dbuf_linear = decoded_data;
	ebuf_g729 = encoded_data;
	for (i = 0; i < decoded_data_len / sizeof(short); i++) {
		ebuf_ulaw[i] = linear_to_ulaw(dbuf_linear[i]);
	}
#ifdef DEBUG_DAHDI_CODEC
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Writing %d bytes of decoded ulaw data.\n", i);
#endif
	res = write(context->encoding_fd, ebuf_ulaw, i);
	if (-1 == res) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to write to %s encoder device.\n", transcoder_name);
		return SWITCH_STATUS_FALSE;
	}
	if (i != res) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Requested to write %d bytes to %s encoder device, but only wrote %d bytes.\n", i, transcoder_name, res);
		return SWITCH_STATUS_FALSE;
	}
#ifdef DEBUG_DAHDI_CODEC
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Wrote %d bytes of decoded ulaw data.\n", res);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Attempting to read %d bytes of encoded data.\n", i/context->codec_r);
#endif
	res = read(context->encoding_fd, encoded_data, i/context->codec_r);
	if (-1 == res) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to read from %s encoder device: %s.\n", transcoder_name, strerror(errno));
		return SWITCH_STATUS_FALSE;
	}
	*encoded_data_len = res;
#ifdef DEBUG_DAHDI_CODEC
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Read %d bytes of encoded data.\n", res);
#endif
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_dahdi_decode(switch_codec_t *codec,
										   switch_codec_t *other_codec,
										   void *encoded_data,
										   uint32_t encoded_data_len,
										   uint32_t encoded_rate, void *decoded_data, uint32_t *decoded_data_len, uint32_t *decoded_rate,
										   unsigned int *flag)
{
	int32_t res;
	short *dbuf_linear;
	unsigned char dbuf_ulaw[encoded_data_len * ((struct dahdi_context*)codec->private_info)->codec_r]; 
	unsigned char *ebuf_g729;
	uint32_t i;
	struct dahdi_context *context;
#ifdef DEBUG_DAHDI_CODEC
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Switch DAHDI decode called to decode %d bytes.\n", encoded_data_len);
#endif

	context = codec->private_info;
	dbuf_linear = decoded_data;
	ebuf_g729 = encoded_data;

	if (*flag & SWITCH_CODEC_FLAG_SILENCE) {
		memset(dbuf_linear, 0, codec->implementation->decoded_bytes_per_packet);
		*decoded_data_len = codec->implementation->decoded_bytes_per_packet;
#ifdef DEBUG_DAHDI_CODEC
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Switch DAHDI decode in silence returned %d bytes.\n", *decoded_data_len);
#endif
		return SWITCH_STATUS_SUCCESS;
	} 
#ifdef DEBUG_DAHDI_CODEC
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Writing %d bytes to decode.\n", encoded_data_len);
#endif
	res = write(context->decoding_fd, ebuf_g729, encoded_data_len);
	if (-1 == res) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to write to %s decoder device.\n", transcoder_name);
		return SWITCH_STATUS_FALSE;
	}
	if (encoded_data_len != res) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Requested to write %d bytes to %s decoder device, but only wrote %d bytes.\n", encoded_data_len, transcoder_name, res);
		return SWITCH_STATUS_FALSE;
	}
#ifdef DEBUG_DAHDI_CODEC
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Wrote %d bytes to decode.\n", res);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Attempting to read from device %d bytes of decoded ulaw data.\n", 
			encoded_data_len*context->codec_r);
#endif
	res = read(context->decoding_fd, dbuf_ulaw, encoded_data_len*context->codec_r);
	if (-1 == res) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to read from %s encoder device: %s.\n", transcoder_name, strerror(errno));
		return SWITCH_STATUS_FALSE;
	}
	for (i = 0; i < res; i++) {
		dbuf_linear[i] = ulaw_to_linear(dbuf_ulaw[i]);
	}
	*decoded_data_len = i * 2;
#ifdef DEBUG_DAHDI_CODEC
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Switch DAHDI decode returned %d bytes.\n", *decoded_data_len);
#endif
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t switch_dahdi_destroy(switch_codec_t *codec)
{
	/* memory pool takes care of the private_info memory */
	struct dahdi_context *context = codec->private_info;
	if (context->encoding_fd >= 0) {
		switch_mutex_lock(transcoder_counter_mutex);
		total_encoders_usage--;
		switch_mutex_unlock(transcoder_counter_mutex);
		close(context->encoding_fd);
	}
	if (context->decoding_fd >= 0) {
		switch_mutex_lock(transcoder_counter_mutex);
		total_decoders_usage--;
		switch_mutex_unlock(transcoder_counter_mutex);
		close(context->decoding_fd);
	}
	codec->private_info = NULL;
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(dahdi_transcode_usage)
{
	if (!total_encoders && !total_decoders) {
		stream->write_function(stream, "No DAHDI transcoding hardware found.\n");
		return SWITCH_STATUS_SUCCESS;
	}
	switch_mutex_lock(transcoder_counter_mutex);
	stream->write_function(stream, "Using %d encoders of a total of %d available.\n",
			total_encoders_usage, total_encoders);
	stream->write_function(stream, "Using %d decoders of a total of %d available.\n",
			total_decoders_usage, total_decoders);
	switch_mutex_unlock(transcoder_counter_mutex);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_dahdi_codec_load)
{
	switch_api_interface_t *api_interface;
	switch_codec_interface_t *codec_interface;
	struct stat statbuf;
	struct dahdi_transcoder_info info = { 0 };
	int32_t fd, res;

	total_encoders = 0;
	total_encoders_usage = 0;
	total_decoders = 0;
	total_decoders_usage = 0;

	/* Let's check if DAHDI or Zaptel device should be used */
	if (!stat(transcoding_device_dahdi, &statbuf)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "DAHDI transcoding device found.\n");
		transcoding_device = transcoding_device_dahdi;
		transcoder_name = transcoder_name_dahdi;
	} else if (!stat(transcoding_device_zap, &statbuf)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Zap transcoding device found.\n");
		transcoding_device = transcoding_device_zap;
		transcoder_name = transcoder_name_zap;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No DAHDI or Zap transcoder device was found in /dev/.\n");
		return SWITCH_STATUS_FALSE;
	}

	fd = open(transcoding_device, O_RDWR);
	if (fd < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to open %s transcoder device: %s.\n", 
				transcoder_name, strerror(errno));
		return SWITCH_STATUS_FALSE;
	}

	for (info.tcnum = 0; !(res = ioctl(fd, DAHDI_TC_GETINFO, &info)); info.tcnum++) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Found dahdi transcoder name: %s\n", info.name);
		if ((info.srcfmts & DAHDI_FORMAT_ULAW) && (info.dstfmts & (DAHDI_FORMAT_G729A | DAHDI_FORMAT_G723_1))) {
			total_encoders += info.numchannels;
			continue;
		}
		if ((info.dstfmts & DAHDI_FORMAT_ULAW) && (info.srcfmts & (DAHDI_FORMAT_G729A | DAHDI_FORMAT_G723_1))) {
			total_decoders  += info.numchannels;
			continue;
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, 
				"Not using transcoder %s, we just support ULAW and G723.1/G729A", info.name);
	}
	close(fd);
	
	if (!total_encoders && !total_decoders) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "No DAHDI transcoders found.\n");
		return SWITCH_STATUS_FALSE;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Found %d ULAW to G729A/G723.1 encoders.\n", total_encoders);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Found %d G729A/G723.1 to ULAW decoders.\n", total_decoders);

	switch_mutex_init(&transcoder_counter_mutex, SWITCH_MUTEX_UNNESTED, pool);

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	
	SWITCH_ADD_CODEC(codec_interface, "DAHDI G.729A 8.0k"); /* 8.0kbit */
	int mpf = 20000; /* Algorithmic delay of 15ms with 5ms of look-ahead delay */
	int spf = 160; 
	int bpfd = 320; 
	int bpfc = 20; 
	int fpnp = 10; 
	switch_core_codec_add_implementation(pool,
	 codec_interface,
	 SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
	 18,				/* the IANA code number */
	 "G729",  			/* the IANA code name */
	 NULL,				/* default fmtp to send (can be overridden by the init function) */
	 8000,				/* samples transferred per second */
	 8000,				/* actual samples transferred per second */
	 8000,				/* bits transferred per second */
	 mpf,				/* number of microseconds per frame (10ms frames) */
	 spf,				/* number of samples per frame */
	 bpfd,				/* number of bytes per frame decompressed */
	 bpfc,				/* number of bytes per frame compressed */
	 1,				/* number of channels represented */
	 fpnp,				/* number of frames per network packet */
	 switch_dahdi_init, 		/* function to initialize a codec handle using this implementation */
	 switch_dahdi_encode,		/* function to encode raw data into encoded data */
	 switch_dahdi_decode,		/* function to decode encoded data into raw data */
	 switch_dahdi_destroy);		/* deinitalize a codec handle using this implementation */

	SWITCH_ADD_CODEC(codec_interface, "DAHDI G.723.1 5.3k"); /* 5.3kbit */
	mpf = 30000; /* Algorithmic delay of 37.5ms with 7.5ms of look-ahead delay */
	spf = 240;
	bpfd = 480;
	bpfc = 20;
	fpnp = 10;
	switch_core_codec_add_implementation(pool,
	 codec_interface,
	 SWITCH_CODEC_TYPE_AUDIO,	/* enumeration defining the type of the codec */
	 4,				/* the IANA code number */
	 "G723",  			/* the IANA code name */
	 NULL,				/* default fmtp to send (can be overridden by the init function) */
	 8000,				/* samples transferred per second */
	 8000,				/* actual samples transferred per second */
	 8000,				/* bits transferred per second */
	 mpf,				/* number of microseconds per frame (10ms frames) */
	 spf,				/* number of samples per frame */
	 bpfd,				/* number of bytes per frame decompressed */
	 bpfc,				/* number of bytes per frame compressed */
	 1,				/* number of channels represented */
	 fpnp,				/* number of frames per network packet */
	 switch_dahdi_init, 		/* function to initialize a codec handle using this implementation */
	 switch_dahdi_encode,		/* function to encode raw data into encoded data */
	 switch_dahdi_decode,		/* function to decode encoded data into raw data */
	 switch_dahdi_destroy);		/* deinitalize a codec handle using this implementation */

	SWITCH_ADD_API(api_interface, "dahdi_transcode", "DAHDI Transcode", dahdi_transcode_usage, NULL);
	switch_console_set_complete("add dahdi_transcode");
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
