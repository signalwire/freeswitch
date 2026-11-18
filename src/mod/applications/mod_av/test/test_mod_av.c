/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2018, Anthony Minessale II <anthm@freeswitch.org>
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
 * Seven Du <dujinfang@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * mod_av_test -- avcodec tests
 *
 */

#include <test/switch_test.h>

int loop = 0;

/* Add our command line options. */
static fctcl_init_t my_cl_options[] = {
	{"--disable-hw",                 /* long_opt */
	 NULL,                           /* short_opt (optional) */
	 FCTCL_STORE_TRUE  ,             /* action */
	 "disable hardware encoder"     /* help */
	 },

	{"--loop",                       /* long_opt */
	 NULL,                           /* short_opt (optional) */
	 FCTCL_STORE_VALUE ,             /* action */
	 "loops to encode a picture"     /* help */
	 },
	FCTCL_INIT_NULL /* Sentinel */
};

/* payload bytes per FU-A fragment when the round-trip test re-fragments a NAL unit */
#define FU_A_TEST_PAYLOAD 32

/* Feed one RTP payload to the decoder the way the media layer does: a fresh frame per packet,
 * the marker on the last packet of a picture and the timestamp advancing per picture. */
static switch_status_t decode_rtp_payload(switch_codec_t *codec, switch_frame_t *frame, void *data, uint32_t datalen, switch_bool_t m)
{
	switch_status_t status;

	frame->data = data;
	frame->datalen = datalen;
	frame->m = m;
	frame->flags = 0;
	frame->img = NULL;
	frame->seq++;

	status = switch_core_codec_decode_video(codec, frame);

	if (m) frame->timestamp += 3000;

	return status;
}

FST_CORE_BEGIN("conf")
{
	const char *loop_;
	fctcl_install(my_cl_options);

	loop_ = fctcl_val("--loop");
	if (loop_) loop = atoi(loop_);

	FST_MODULE_BEGIN(mod_av, mod_av_test)
	{
		FST_SETUP_BEGIN()
		{
			// fst_requires_module("mod_av");
		}
		FST_SETUP_END()

		FST_TEST_BEGIN(encoder_test)
		{
			switch_status_t status;
			switch_codec_t codec = { 0 };
			switch_codec_settings_t codec_settings = {{ 0 }};
			switch_image_t *img;
			uint8_t buf[SWITCH_DEFAULT_VIDEO_SIZE + 12];
			switch_frame_t frame = { 0 };
			int packets = 0;
			int frames = 0;
			int last_key_frame = 0;
			int key_frames = 0;
			switch_status_t encode_status;
			int debug_level = 9;

			switch_set_string(codec_settings.video.config_profile_name, "conference");

			if (!fctcl_is("--disable-hw")) {
				codec_settings.video.try_hardware_encoder = 1;
			}

			status = switch_core_codec_init(&codec,
							   "H264",
							   NULL,
							   NULL,
							   0,
							   0,
							   1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   &codec_settings, fst_pool);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, 1280, 720, 1);
			fst_requires(img);


			frame.packet = buf;
			frame.packetlen = SWITCH_DEFAULT_VIDEO_SIZE + 12;
			frame.data = buf + 12;
			frame.datalen = SWITCH_DEFAULT_VIDEO_SIZE;
			frame.payload = 96;
			frame.m = 0;
			frame.seq = 0;
			frame.timestamp = 0;
			frame.img = img;

			switch_core_codec_control(&codec, SCC_DEBUG, SCCT_NONE, &debug_level, SCCT_INT, NULL, NULL, NULL);

			do {
				frame.datalen = SWITCH_DEFAULT_VIDEO_SIZE;
				encode_status = switch_core_codec_encode_video(&codec, &frame);

				if (encode_status == SWITCH_STATUS_SUCCESS || encode_status == SWITCH_STATUS_MORE_DATA) {

					fst_requires((encode_status == SWITCH_STATUS_SUCCESS && frame.m) || !frame.m);

					if (frame.flags & SFF_PICTURE_RESET) {
						frame.flags &= ~SFF_PICTURE_RESET;
						fst_check(0);
					}

					if (frame.datalen == 0) break;

					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[%d]: %02x %02x | m=%d | %d\n", frames, buf[12], buf[13], frame.m, frame.datalen);
					packets++;

					if (frame.m) frames++;

					if (frames % 20 == 2) {
						switch_core_codec_control(&codec, SCC_VIDEO_GEN_KEYFRAME, SCCT_NONE, NULL, SCCT_NONE, NULL, NULL, NULL);
					}

					if (buf[12] == 0x67) {
						key_frames++;
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Key Frame %d last=%d diff=%d\n",
							key_frames, last_key_frame, frames - last_key_frame);
						last_key_frame = frames;
					}
				}

			} while(encode_status == SWITCH_STATUS_MORE_DATA || loop-- > 1);

			fst_check(frame.m == 1);
			fst_check(packets > 0);

			switch_core_codec_destroy(&codec);
			switch_img_free(&img);
		}
		FST_TEST_END()

		/* RTP payloads shorter than the headers their NAL type implies are rejected by the depacketizer,
		 * while FU-A fragments with an empty payload, which RFC 6184 allows, are accepted */
		FST_TEST_BEGIN(decoder_short_packet_test)
		{
			switch_status_t status;
			switch_codec_t codec = { 0 };
			switch_codec_settings_t codec_settings = {{ 0 }};
			switch_frame_t frame = { 0 };
			/* each packet buffer is exactly the length it claims */
			uint8_t fu_a_indicator_only[] = { 0x7c };
			uint8_t fu_a_start_empty[] = { 0x7c, 0x85 };
			uint8_t fu_a_middle_empty[] = { 0x7c, 0x05 };
			uint8_t fu_a_end_empty[] = { 0x7c, 0x45 };
			uint8_t fu_a_start[] = { 0x7c, 0x85, 0x01, 0x02 };
			uint8_t fu_a_end[] = { 0x7c, 0x45, 0x03, 0x04 };
			uint8_t stap_a_indicator_only[] = { 0x78 };
			uint8_t stap_a_half_size[] = { 0x78, 0x00 };
			uint8_t single_nalu_header_only[] = { 0x65 };
			int i;

			switch_set_string(codec_settings.video.config_profile_name, "conference");

			status = switch_core_codec_init(&codec,
							   "H264",
							   NULL,
							   NULL,
							   0,
							   0,
							   1, SWITCH_CODEC_FLAG_DECODE,
							   &codec_settings, fst_pool);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			frame.payload = 96;
			frame.timestamp = 3000;

			/* an empty payload adds nothing */
			status = decode_rtp_payload(&codec, &frame, fu_a_indicator_only, 0, SWITCH_FALSE);
			fst_check(status == SWITCH_STATUS_SUCCESS);
			fst_check(frame.img == NULL);

			/* an FU-A without the FU header is dropped and a key frame requested, an FU-A with an empty payload is accepted */
			for (i = 0; i < 2; i++) {
				switch_bool_t m = i ? SWITCH_TRUE : SWITCH_FALSE;

				status = decode_rtp_payload(&codec, &frame, fu_a_indicator_only, sizeof(fu_a_indicator_only), m);
				fst_check(status == SWITCH_STATUS_MORE_DATA);
				fst_check(switch_test_flag(&frame, SFF_WAIT_KEY_FRAME));
				fst_check(frame.img == NULL);

				status = decode_rtp_payload(&codec, &frame, fu_a_start_empty, sizeof(fu_a_start_empty), m);
				fst_check(status == SWITCH_STATUS_SUCCESS);
				fst_check(frame.img == NULL);
			}

			/* the same packets in the middle of a fragment being reassembled, then empty middle and end fragments completing it */
			status = decode_rtp_payload(&codec, &frame, fu_a_start, sizeof(fu_a_start), SWITCH_FALSE);
			fst_check(status == SWITCH_STATUS_SUCCESS);
			status = decode_rtp_payload(&codec, &frame, fu_a_indicator_only, sizeof(fu_a_indicator_only), SWITCH_FALSE);
			fst_check(status == SWITCH_STATUS_MORE_DATA);
			status = decode_rtp_payload(&codec, &frame, fu_a_start, sizeof(fu_a_start), SWITCH_FALSE);
			fst_check(status == SWITCH_STATUS_SUCCESS);
			status = decode_rtp_payload(&codec, &frame, fu_a_middle_empty, sizeof(fu_a_middle_empty), SWITCH_FALSE);
			fst_check(status == SWITCH_STATUS_SUCCESS);
			status = decode_rtp_payload(&codec, &frame, fu_a_end_empty, sizeof(fu_a_end_empty), SWITCH_TRUE);
			fst_check(status == SWITCH_STATUS_SUCCESS);
			fst_check(frame.img == NULL);

			/* an end fragment whose start fragment was lost is dropped */
			status = decode_rtp_payload(&codec, &frame, fu_a_end, sizeof(fu_a_end), SWITCH_TRUE);
			fst_check(status == SWITCH_STATUS_MORE_DATA);
			fst_check(switch_test_flag(&frame, SFF_WAIT_KEY_FRAME));
			fst_check(frame.img == NULL);

			/* an empty packet carrying the marker closes the picture: the fragment started before it is
			 * handed to the decoder, so a later end fragment has no start to attach to */
			status = decode_rtp_payload(&codec, &frame, fu_a_start, sizeof(fu_a_start), SWITCH_FALSE);
			fst_check(status == SWITCH_STATUS_SUCCESS);
			status = decode_rtp_payload(&codec, &frame, fu_a_indicator_only, 0, SWITCH_TRUE);
			fst_check(status == SWITCH_STATUS_SUCCESS);
			fst_check(frame.img == NULL);
			status = decode_rtp_payload(&codec, &frame, fu_a_end, sizeof(fu_a_end), SWITCH_TRUE);
			fst_check(status == SWITCH_STATUS_MORE_DATA);
			fst_check(switch_test_flag(&frame, SFF_WAIT_KEY_FRAME));

			/* a STAP-A with no room for a NALU size, and a single NALU of only a header, are accepted */
			status = decode_rtp_payload(&codec, &frame, stap_a_indicator_only, sizeof(stap_a_indicator_only), SWITCH_TRUE);
			fst_check(status == SWITCH_STATUS_SUCCESS);
			status = decode_rtp_payload(&codec, &frame, stap_a_half_size, sizeof(stap_a_half_size), SWITCH_TRUE);
			fst_check(status == SWITCH_STATUS_SUCCESS);
			status = decode_rtp_payload(&codec, &frame, single_nalu_header_only, sizeof(single_nalu_header_only), SWITCH_TRUE);
			fst_check(status == SWITCH_STATUS_SUCCESS);
			fst_check(frame.img == NULL);

			switch_core_codec_destroy(&codec);
		}
		FST_TEST_END()

		/* Encode a few pictures, re-fragment every NAL unit into FU-A packets and make sure the
		 * depacketizer reassembles them into pictures */
		FST_TEST_BEGIN(decoder_fu_a_roundtrip_test)
		{
			switch_status_t status;
			switch_codec_t codec = { 0 };
			switch_codec_settings_t codec_settings = {{ 0 }};
			switch_image_t *img;
			uint8_t buf[SWITCH_DEFAULT_VIDEO_SIZE + 12];
			uint8_t fragment[FU_A_TEST_PAYLOAD + 2];
			switch_frame_t enc_frame = { 0 };
			switch_frame_t dec_frame = { 0 };
			int pictures;
			int encoded = 0;
			int split_fragments = 0;
			int decoded = 0;

			switch_set_string(codec_settings.video.config_profile_name, "conference");

			if (!fctcl_is("--disable-hw")) {
				codec_settings.video.try_hardware_encoder = 1;
			}

			status = switch_core_codec_init(&codec,
							   "H264",
							   NULL,
							   NULL,
							   0,
							   0,
							   1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   &codec_settings, fst_pool);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, 320, 240, 1);
			fst_check(img != NULL);

			enc_frame.packet = buf;
			enc_frame.packetlen = sizeof(buf);
			enc_frame.data = buf + 12;
			enc_frame.payload = 96;
			enc_frame.img = img;

			dec_frame.payload = 96;
			dec_frame.timestamp = 3000;

			for (pictures = 0; pictures < 10; pictures++) {
				switch_status_t encode_status;

				do {
					uint8_t *nalu = buf + 12;
					uint8_t nalu_type;

					enc_frame.datalen = SWITCH_DEFAULT_VIDEO_SIZE;
					encode_status = switch_core_codec_encode_video(&codec, &enc_frame);

					if (encode_status != SWITCH_STATUS_SUCCESS && encode_status != SWITCH_STATUS_MORE_DATA) break;
					if (enc_frame.datalen == 0) break;
					if (enc_frame.m) encoded++;

					nalu_type = nalu[0] & 0x1f;

					if (nalu_type >= 1 && nalu_type <= 23 && enc_frame.datalen > FU_A_TEST_PAYLOAD) {
						/* a single NAL unit packet: split it into FU-A fragments as RFC 6184 section 5.8 describes */
						uint32_t offset = 1; /* the NAL header travels in the FU indicator and FU header instead */

						while (offset < enc_frame.datalen) {
							uint32_t chunk = enc_frame.datalen - offset;
							switch_bool_t last;

							if (chunk > FU_A_TEST_PAYLOAD) chunk = FU_A_TEST_PAYLOAD;
							last = (offset + chunk == enc_frame.datalen) ? SWITCH_TRUE : SWITCH_FALSE;

							fragment[0] = (nalu[0] & 0xe0) | 28;
							fragment[1] = (nalu[0] & 0x1f) | (offset == 1 ? 0x80 : 0) | (last ? 0x40 : 0);
							memcpy(fragment + 2, nalu + offset, chunk);
							offset += chunk;

							status = decode_rtp_payload(&codec, &dec_frame, fragment, chunk + 2, (last && enc_frame.m) ? SWITCH_TRUE : SWITCH_FALSE);
							fst_check(status == SWITCH_STATUS_SUCCESS || status == SWITCH_STATUS_MORE_DATA);
							split_fragments++;

							if (dec_frame.img) {
								fst_check(dec_frame.img->d_w == 320 && dec_frame.img->d_h == 240);
								decoded++;
							}
						}
					} else {
						status = decode_rtp_payload(&codec, &dec_frame, nalu, enc_frame.datalen, enc_frame.m);
						fst_check(status == SWITCH_STATUS_SUCCESS || status == SWITCH_STATUS_MORE_DATA);

						if (dec_frame.img) {
							fst_check(dec_frame.img->d_w == 320 && dec_frame.img->d_h == 240);
							decoded++;
						}
					}
				} while (encode_status == SWITCH_STATUS_MORE_DATA);
			}

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%d pictures encoded, %d FU-A fragments made by the test, %d pictures decoded\n", encoded, split_fragments, decoded);
			fst_check(encoded > 0);
			fst_check(split_fragments > 0);
			fst_check(decoded > 0);
			fst_check(decoded + 2 >= encoded);

			switch_core_codec_destroy(&codec);
			switch_img_free(&img);
		}
		FST_TEST_END()

		/* H.263 payload headers are checked against the packet length before they are read. The decoder
		 * reports success for a packet its depacketizer rejected, so these only make sure nothing is
		 * read past a short packet and no picture comes out of it. */
		FST_TEST_BEGIN(decoder_h263_short_packet_test)
		{
			const char *names[] = { "H263", "H263-1998" };
			int c;

			for (c = 0; c < 2; c++) {
				switch_status_t status;
				switch_codec_t codec = { 0 };
				switch_codec_settings_t codec_settings = {{ 0 }};
				switch_frame_t frame = { 0 };
				/* each packet buffer is exactly the length it claims */
				uint8_t one[] = { 0x00 };
				uint8_t two[] = { 0x00, 0x00 };
				uint8_t three[] = { 0x00, 0x00, 0x00 };
				int i;

				switch_set_string(codec_settings.video.config_profile_name, "conference");

				status = switch_core_codec_init(&codec,
								   names[c],
								   NULL,
								   NULL,
								   0,
								   0,
								   1, SWITCH_CODEC_FLAG_DECODE,
								   &codec_settings, fst_pool);
				fst_check(status == SWITCH_STATUS_SUCCESS);

				frame.payload = 96;
				frame.timestamp = 3000;

				for (i = 0; i < 2; i++) {
					switch_bool_t m = i ? SWITCH_TRUE : SWITCH_FALSE;

					status = decode_rtp_payload(&codec, &frame, one, 0, m);
					fst_check(status == SWITCH_STATUS_SUCCESS);
					status = decode_rtp_payload(&codec, &frame, one, sizeof(one), m);
					fst_check(status == SWITCH_STATUS_SUCCESS);
					status = decode_rtp_payload(&codec, &frame, two, sizeof(two), m);
					fst_check(status == SWITCH_STATUS_SUCCESS);
					status = decode_rtp_payload(&codec, &frame, three, sizeof(three), m);
					fst_check(status == SWITCH_STATUS_SUCCESS);
					fst_check(frame.img == NULL);
				}

				switch_core_codec_destroy(&codec);
			}
		}
		FST_TEST_END()

		FST_TEARDOWN_BEGIN()
		{
			/* the module stays loaded across the tests, FST_MODULE_END unloads it after the last one */
			switch_sleep(1000000);
		}
		FST_TEARDOWN_END()
	}
	FST_MODULE_END()
}
FST_CORE_END()
