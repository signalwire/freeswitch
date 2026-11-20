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
 * Pax Cao <yiping.cao@msn.com>
 *
 * mod_openh264_test -- mod_openh264 tests
 *
 */

#include <test/switch_test.h>

#define 	TEST_SIMU_RTP_MAX_LEN 		4200

/* Add our command line options. */
static fctcl_init_t my_cl_options[] = {
	{"--disable-hw",                 /* long_opt */
	 NULL,                           /* short_opt (optional) */
	 FCTCL_STORE_TRUE  ,             /* action */
	 "disable hardware encoder"     /* help */
	 },

	FCTCL_INIT_NULL /* Sentinel */
};

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
	fctcl_install(my_cl_options);

	FST_MODULE_BEGIN(mod_openh264, mod_openh264_test)
	{
		FST_SETUP_BEGIN()
		{
			fst_requires_module("mod_openh264");
		}
		FST_SETUP_END()

		FST_TEST_BEGIN(decoder_testcase)
		{
			switch_status_t status;
			switch_codec_t codec = { 0 };
			switch_codec_settings_t codec_settings = {{ 0 }};
			switch_image_t *img;
			uint8_t buf[TEST_SIMU_RTP_MAX_LEN + 12] = {0};
			switch_frame_t frame = { 0 };
			int packets = 0;
			switch_status_t decode_status;
			FILE *fp = NULL;
			long file_size = 0;
			switch_bool_t		fh_status = SWITCH_FALSE;
			int   file_count = 1;
			switch_set_string(codec_settings.video.config_profile_name, "test-decoder-1");

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
			
			frame.packet = buf;
			frame.packetlen = TEST_SIMU_RTP_MAX_LEN + 12;
			frame.data = buf + 12;
			frame.datalen = TEST_SIMU_RTP_MAX_LEN;
			frame.payload = 96;
			frame.m = SWITCH_FALSE;
			frame.seq = 0;
			frame.timestamp = 375233;
			frame.img = (switch_image_t*)NULL;
			do {
				switch_size_t len = TEST_SIMU_RTP_MAX_LEN;
				char  file_name[128] = { 0 };
				frame.seq++;
				switch_snprintf(file_name, 128, "./data/case1.packet%d.264", file_count);

				fp = fopen(file_name, "rb");
				if (!fp) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
						 "read video file %s failed \n", file_name);
					break;
				}
				fseek(fp, 0, SEEK_END);
				file_size = ftell(fp);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
						 "file %s length: %ld \n", file_name, file_size);
				fseek(fp, 0, SEEK_SET);	 
				
				frame.datalen = fread(frame.data, sizeof(char), file_size, fp);
				fst_check(frame.datalen == file_size);

				if (4 == file_count ) {
					frame.m = SWITCH_TRUE;
				} 
				if (5 == file_count) {
					frame.m = SWITCH_TRUE;
					frame.timestamp = 380633;
				}
				decode_status = switch_core_codec_decode_video(&codec, &frame);
				fst_check(decode_status == SWITCH_STATUS_SUCCESS || decode_status == SWITCH_STATUS_MORE_DATA);
				
				if (frame.img != NULL) {
					// write down the decoded
					status = switch_img_write_png(frame.img, (char *)"case1.output.qcif.png");
					fst_check(status == SWITCH_STATUS_SUCCESS);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
						 "write output file done! \n");
					
				}
				fclose(fp);
				file_count++;
			} while (decode_status == SWITCH_STATUS_MORE_DATA && file_count < 6);
			
			switch_core_codec_destroy(&codec);

			file_count = 1;
			status = switch_core_codec_init(&codec,
							   "H264",
							   NULL,
							   NULL,
							   0,
							   0,
							   1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   &codec_settings, fst_pool);
			fst_check(status == SWITCH_STATUS_SUCCESS);
			
			frame.packet = buf;
			frame.packetlen = TEST_SIMU_RTP_MAX_LEN + 12;
			frame.data = buf + 12;
			frame.datalen = TEST_SIMU_RTP_MAX_LEN;
			frame.payload = 96;
			frame.m = SWITCH_FALSE;
			frame.seq = 0;
			frame.timestamp = 375233;
			frame.img = (switch_image_t*)NULL;
			do {
				switch_size_t len = TEST_SIMU_RTP_MAX_LEN;
				char  file_name[128] = { 0 };
				frame.seq++;
				switch_snprintf(file_name, 128, "./data/case2.packet%d.264", file_count);

				fp = fopen(file_name, "rb");
				if (!fp) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
						 "read video file %s failed \n", file_name);
					break;
				}
				fseek(fp, 0, SEEK_END);
				file_size = ftell(fp);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
						 "file %s length: %ld \n", file_name, file_size);
				fseek(fp, 0, SEEK_SET);	 
				
				frame.datalen = fread(frame.data, sizeof(char), file_size, fp);
				fst_check(frame.datalen == file_size);

				if (3 == file_count) {
					frame.m = SWITCH_TRUE;
				} 
				if (4 == file_count) {
					frame.m = SWITCH_TRUE;
					frame.timestamp = 380633;
				} 
				decode_status = switch_core_codec_decode_video(&codec, &frame);
				fst_check(decode_status == SWITCH_STATUS_SUCCESS || decode_status == SWITCH_STATUS_MORE_DATA);
				if (frame.img != NULL) {
					// write down the decoded
					status = switch_img_write_png(frame.img, (char *)"case2.output.qcif.png");
					fst_check(status == SWITCH_STATUS_SUCCESS);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
						 "write output file done! \n");
					
				}
				fclose(fp);
				file_count++;
			} while (decode_status == SWITCH_STATUS_MORE_DATA && file_count < 5);
			
			switch_core_codec_destroy(&codec);

		}
		FST_TEST_END()


		/* RTP payloads shorter than the headers their NAL type implies are rejected by the depacketizer,
		 * while FU-A fragments with an empty payload, which RFC 6184 allows, are accepted. Without a
		 * picture to return the decoder reports SWITCH_STATUS_MORE_DATA, so a dropped packet shows as
		 * the key frame request it raises. */
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

			switch_set_string(codec_settings.video.config_profile_name, "test-decoder-1");

			status = switch_core_codec_init(&codec,
							   "H264",
							   NULL,
							   NULL,
							   0,
							   0,
							   1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   &codec_settings, fst_pool);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			frame.payload = 96;
			frame.timestamp = 3000;

			/* an empty payload adds nothing */
			status = decode_rtp_payload(&codec, &frame, fu_a_indicator_only, 0, SWITCH_FALSE);
			fst_check(status == SWITCH_STATUS_MORE_DATA);
			fst_check(frame.img == NULL);

			/* an FU-A without the FU header is dropped and a key frame requested */
			status = decode_rtp_payload(&codec, &frame, fu_a_indicator_only, sizeof(fu_a_indicator_only), SWITCH_FALSE);
			fst_check(status == SWITCH_STATUS_MORE_DATA);
			fst_check(switch_test_flag(&frame, SFF_WAIT_KEY_FRAME));
			fst_check(frame.img == NULL);
			status = decode_rtp_payload(&codec, &frame, fu_a_indicator_only, sizeof(fu_a_indicator_only), SWITCH_TRUE);
			fst_check(status == SWITCH_STATUS_MORE_DATA);
			fst_check(switch_test_flag(&frame, SFF_WAIT_KEY_FRAME));
			fst_check(frame.img == NULL);

			/* an FU-A start fragment with an empty payload is accepted */
			status = decode_rtp_payload(&codec, &frame, fu_a_start_empty, sizeof(fu_a_start_empty), SWITCH_FALSE);
			fst_check(status == SWITCH_STATUS_MORE_DATA);
			fst_check(!switch_test_flag(&frame, SFF_WAIT_KEY_FRAME));
			fst_check(frame.img == NULL);

			/* a fragment without the FU header in the middle of a reassembly is dropped, then empty middle and end fragments complete one */
			status = decode_rtp_payload(&codec, &frame, fu_a_start, sizeof(fu_a_start), SWITCH_FALSE);
			fst_check(status == SWITCH_STATUS_MORE_DATA);
			status = decode_rtp_payload(&codec, &frame, fu_a_indicator_only, sizeof(fu_a_indicator_only), SWITCH_FALSE);
			fst_check(status == SWITCH_STATUS_MORE_DATA);
			fst_check(switch_test_flag(&frame, SFF_WAIT_KEY_FRAME));
			status = decode_rtp_payload(&codec, &frame, fu_a_start, sizeof(fu_a_start), SWITCH_FALSE);
			fst_check(status == SWITCH_STATUS_MORE_DATA);
			fst_check(!switch_test_flag(&frame, SFF_WAIT_KEY_FRAME));
			status = decode_rtp_payload(&codec, &frame, fu_a_middle_empty, sizeof(fu_a_middle_empty), SWITCH_FALSE);
			fst_check(status == SWITCH_STATUS_MORE_DATA);
			fst_check(!switch_test_flag(&frame, SFF_WAIT_KEY_FRAME));
			status = decode_rtp_payload(&codec, &frame, fu_a_end_empty, sizeof(fu_a_end_empty), SWITCH_TRUE);
			fst_check(status == SWITCH_STATUS_MORE_DATA);
			fst_check(frame.img == NULL);

			/* an end fragment whose start fragment was lost is dropped */
			status = decode_rtp_payload(&codec, &frame, fu_a_end, sizeof(fu_a_end), SWITCH_TRUE);
			fst_check(status == SWITCH_STATUS_MORE_DATA);
			fst_check(switch_test_flag(&frame, SFF_WAIT_KEY_FRAME));
			fst_check(frame.img == NULL);

			/* an empty packet carrying the marker closes the picture: the fragment started before it is
			 * handed to the decoder, so a later end fragment has no start to attach to */
			status = decode_rtp_payload(&codec, &frame, fu_a_start, sizeof(fu_a_start), SWITCH_FALSE);
			fst_check(status == SWITCH_STATUS_MORE_DATA);
			status = decode_rtp_payload(&codec, &frame, fu_a_indicator_only, 0, SWITCH_TRUE);
			fst_check(status == SWITCH_STATUS_MORE_DATA);
			fst_check(frame.img == NULL);
			status = decode_rtp_payload(&codec, &frame, fu_a_end, sizeof(fu_a_end), SWITCH_TRUE);
			fst_check(status == SWITCH_STATUS_MORE_DATA);
			fst_check(switch_test_flag(&frame, SFF_WAIT_KEY_FRAME));

			/* a STAP-A with no room for a NALU size, and a single NALU of only a header, are accepted */
			status = decode_rtp_payload(&codec, &frame, stap_a_indicator_only, sizeof(stap_a_indicator_only), SWITCH_TRUE);
			fst_check(status == SWITCH_STATUS_MORE_DATA);
			status = decode_rtp_payload(&codec, &frame, stap_a_half_size, sizeof(stap_a_half_size), SWITCH_TRUE);
			fst_check(status == SWITCH_STATUS_MORE_DATA);
			status = decode_rtp_payload(&codec, &frame, single_nalu_header_only, sizeof(single_nalu_header_only), SWITCH_TRUE);
			fst_check(status == SWITCH_STATUS_MORE_DATA);
			fst_check(frame.img == NULL);

			switch_core_codec_destroy(&codec);
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
