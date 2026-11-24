/*
 * Copyright (C) 2018, Signalwire, Inc. ALL RIGHTS RESERVED
 *
 * switch_vpx.c -- Tests vpx functions
 */
#include <test/switch_test.h>

/* Waits for the next soft timer tick (the time switch_micro_time_now() returns),
 * then for offset_us more of real time. Fails when the timer does not tick within
 * a second or ticks again before offset_us pass */
static switch_status_t wait_into_tick(switch_time_t offset_us)
{
	switch_time_t deadline = switch_time_now() + 1000000;
	switch_time_t tick = switch_micro_time_now();

	while (switch_micro_time_now() == tick) {
		if (switch_time_now() > deadline) {
			return SWITCH_STATUS_TIMEOUT;
		}

		switch_os_yield();
	}

	tick = switch_micro_time_now();

	while (switch_time_now() - tick < offset_us) {
		switch_os_yield();
	}

	return switch_micro_time_now() == tick ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}

/* Encodes frame->img and reads out all of its packets */
static switch_status_t encode_picture(switch_codec_t *codec, switch_frame_t *frame)
{
	switch_status_t status;

	do {
		frame->datalen = SWITCH_DEFAULT_VIDEO_SIZE;
		status = switch_core_codec_encode_video(codec, frame);
	} while (status == SWITCH_STATUS_MORE_DATA);

	return status;
}

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

		/* A bitrate change reconfigures the running VP8 encoder. The first picture
		 * is encoded 5 ms into a soft timer tick, the picture after the bitrate
		 * change right at the start of a tick, and both must encode */
		FST_TEST_BEGIN(vp8_bandwidth_change)
		{
			uint8_t buf[SWITCH_DEFAULT_VIDEO_SIZE + 12];
			switch_codec_t codec = { 0 };
			switch_codec_settings_t codec_settings = { { 0 } };
			switch_frame_t frame = { 0 };
			switch_image_t *img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, 640, 360, 1);
			switch_rgb_color_t color = { 0 };
			switch_status_t status;
			int bandwidth = 500;

			codec_settings.video.width = 640;
			codec_settings.video.height = 360;

			status = switch_core_codec_init(&codec, "VP8", NULL, NULL, 0, 0, 1, SWITCH_CODEC_FLAG_ENCODE, &codec_settings, fst_pool);
			fst_check(status == SWITCH_STATUS_SUCCESS);
			fst_check(img != NULL);

			if (status == SWITCH_STATUS_SUCCESS && img) {
				color.r = 255;
				switch_img_fill(img, 0, 0, img->d_w, img->d_h, &color);

				frame.packet = buf;
				frame.data = buf + 12;
				frame.img = img;

				status = wait_into_tick(5000);
				fst_check(status == SWITCH_STATUS_SUCCESS);
				status = encode_picture(&codec, &frame);
				fst_check(status == SWITCH_STATUS_SUCCESS);

				switch_sleep(40000);

				status = wait_into_tick(0);
				fst_check(status == SWITCH_STATUS_SUCCESS);
				status = switch_core_codec_control(&codec, SCC_VIDEO_BANDWIDTH, SCCT_INT, &bandwidth, SCCT_NONE, NULL, NULL, NULL);
				fst_check(status == SWITCH_STATUS_SUCCESS);
				status = encode_picture(&codec, &frame);
				fst_check(status == SWITCH_STATUS_SUCCESS);
			}

			if (switch_core_codec_ready(&codec)) {
				switch_core_codec_destroy(&codec);
			}

			switch_img_free(&img);
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
