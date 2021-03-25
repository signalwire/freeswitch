/*
 * Copyright (C) 2018, Signalwire, Inc. ALL RIGHTS RESERVED
 *
 * switch_vpx.c -- Tests vpx functions
 */
#include <test/switch_test.h>

FST_CORE_BEGIN("./conf")
{
	FST_SUITE_BEGIN()
	{
		FST_SETUP_BEGIN()
		{
			switch_stream_handle_t stream = { 0 };
			SWITCH_STANDARD_STREAM(stream);

			switch_api_execute("vpx", "debug on", NULL, &stream);
			switch_safe_free(stream.data);
		}
		FST_SETUP_END()

		FST_TEST_BEGIN(vp8_test)
		{
			switch_image_t *img;
			uint8_t buf[SWITCH_DEFAULT_VIDEO_SIZE + 12];
			switch_status_t status;
			switch_codec_t codec = { 0 };
			switch_frame_t frame = { 0 };
			switch_codec_settings_t codec_settings = {{ 0 }};
			int packets = 0;
			switch_status_t encode_status;

			// switch_set_string(codec_settings.video.config_profile_name, "conference");
			codec_settings.video.width = 1280;
			codec_settings.video.height = 720;

			status = switch_core_codec_init(&codec,
							   "VP8",
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

			do {
				frame.datalen = SWITCH_DEFAULT_VIDEO_SIZE;
				encode_status = switch_core_codec_encode_video(&codec, &frame);

				if (encode_status == SWITCH_STATUS_SUCCESS || encode_status == SWITCH_STATUS_MORE_DATA) {

					switch_assert((encode_status == SWITCH_STATUS_SUCCESS && frame.m) || !frame.m);

					if (frame.flags & SFF_PICTURE_RESET) {
						frame.flags &= ~SFF_PICTURE_RESET;
						// fst_check(0);
					}

					if (frame.datalen == 0) break;

					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%02x %02x | m=%d | %d\n", buf[12], buf[13], frame.m, frame.datalen);
					packets++;
				}

			} while(encode_status == SWITCH_STATUS_MORE_DATA);

			fst_check(frame.m == 1);
			fst_check(packets > 0);

			switch_img_free(&img);
			switch_core_codec_destroy(&codec);
		}
		FST_TEST_END()

		FST_TEARDOWN_BEGIN()
		{
			switch_sleep(1000000);
			// switch_core_destroy();
		}
		FST_TEARDOWN_END()
	}
	FST_SUITE_END()
}
FST_CORE_END()
