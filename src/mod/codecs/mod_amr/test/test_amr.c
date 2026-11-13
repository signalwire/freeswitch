/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2021, Anthony Minessale II <anthm@freeswitch.org>
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
 * Dragos Oancea <dragos@signalwire.com>
 *
 *
 * test_amr.c -- tests mod_amr
 *
 */

#ifndef AMR_PASSTHROUGH
#include <switch.h>
#include <stdlib.h>

#include <test/switch_test.h>
#include <math.h>

/* speech bits per AMR-NB mode (RFC 4867 table 1), SID = index 8 */
static const int nb_bits[] = { 95, 103, 118, 134, 148, 159, 204, 244, 39 };

static switch_status_t amr_init(switch_codec_t *codec, const char *fmtp, switch_memory_pool_t *pool)
{
	switch_codec_settings_t cs = {{ 0 }};
	return switch_core_codec_init(codec, "AMR", "mod_amr", fmtp, 8000, 20, 1,
								  SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, &cs, pool);
}

static void tone_frame(int16_t *pcm, int samples, double freq, int rate, double *phase)
{
	int i;
	for (i = 0; i < samples; i++) {
		pcm[i] = (int16_t)(6000 * sin(*phase));
		*phase += 2 * M_PI * freq / rate;
	}
}

/* encode one 20 ms frame of a tone; returns encoded length (0 on error) */
static uint32_t enc_frame(switch_codec_t *c, double freq, double *phase, uint8_t *out)
{
	int16_t pcm[160];
	uint32_t len = SWITCH_RECOMMENDED_BUFFER_SIZE, rate = 8000;
	unsigned int flag = 0;
	tone_frame(pcm, 160, freq, 8000, phase);
	if (switch_core_codec_encode(c, NULL, pcm, sizeof(pcm), 8000, out, &len, &rate, &flag) != SWITCH_STATUS_SUCCESS) return 0;
	return len;
}

/* frame type (FT) of a single-frame packet */
static int packet_ft(const uint8_t *p, int octet_aligned)
{
	if (octet_aligned) return (p[1] >> 3) & 0x0f;
	return ((p[0] & 0x07) << 1) | (p[1] >> 7); /* BE: CMR(4) F(1) FT(4) Q(1) */
}


/* decode a packet; returns decoded bytes or -1 */
static int dec_packet(switch_codec_t *c, const uint8_t *pkt, int len, int16_t *pcm, int cap_bytes)
{
	uint32_t dlen = (uint32_t) cap_bytes, rate = 8000;
	unsigned int flag = 0;
	if (switch_core_codec_decode(c, NULL, (void *) pkt, (uint32_t) len, 8000, pcm, &dlen, &rate, &flag) != SWITCH_STATUS_SUCCESS) return -1;
	return (int) dlen;
}

/* power of `freq` relative to the total power of the signal (1.0 = pure tone) */
static double tone_ratio(const int16_t *pcm, int n, double freq, int rate)
{
	double w = 2 * M_PI * freq / rate, c = 2 * cos(w), s1 = 0, s2 = 0, s0, total = 0;
	int i;
	for (i = 0; i < n; i++) {
		s0 = pcm[i] + c * s1 - s2; s2 = s1; s1 = s0;
		total += (double) pcm[i] * pcm[i];
	}
	return total > 0 ? 2 * (s1 * s1 + s2 * s2 - c * s1 * s2) / (n * total) : 0;
}

/* single-frame OA packet -> its frame as TOC byte + speech bytes */
typedef struct { uint8_t toc; uint8_t data[64]; int bytes; } frame_t;

static int oa_frame(const uint8_t *pkt, int len, frame_t *f)
{
	if (len < 2) return -1;
	f->toc = pkt[1] & 0x7c; /* F=0, keep FT and Q */
	f->bytes = len - 2;
	memcpy(f->data, pkt + 2, f->bytes);
	return 0;
}

static const frame_t no_data_frame = { 0x7c, { 0 }, 0 }; /* FT=15, Q=1 */

FST_CORE_BEGIN(".")
{
	FST_SUITE_BEGIN(test_amr)
	{
		FST_SETUP_BEGIN()
		{
			fst_requires_module("mod_loopback");
			fst_requires_module("mod_amr");
		}
		FST_SETUP_END()

		FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()

		FST_TEST_BEGIN(amr_decode) 
		{
			switch_codec_t read_codec = { 0 };
			switch_status_t status;
			switch_codec_settings_t codec_settings = {{ 0 }};
			uint32_t flags = 0;
			uint32_t rate;
			/*amr frame types*/
			static char no_data[] = "\x77\xc0";
			static char fail[] = "\x76\xc0";
			/*decode*/
			uint32_t decoded_len;
			unsigned char decbuf[SWITCH_RECOMMENDED_BUFFER_SIZE] = { 0 };
			switch_stream_handle_t stream = { 0 };

			status = switch_core_codec_init(&read_codec,
			"AMR",
			"mod_amr",
			NULL,
			8000,
			20,
			1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
			&codec_settings, fst_pool);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			SWITCH_STANDARD_STREAM(stream);

			switch_api_execute("amr_debug", "on", NULL, &stream);

			switch_safe_free(stream.data);

			/*NO DATA = 0xf*/
			status = switch_core_codec_decode(&read_codec, NULL, &no_data, 2, 8000, &decbuf, &decoded_len, &rate, &flags);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			/*Invalid frame type*/
			status = switch_core_codec_decode(&read_codec, NULL, &fail, 2, 8000, &decbuf, &decoded_len, &rate, &flags);
			fst_check(status != SWITCH_STATUS_SUCCESS);

			switch_core_codec_destroy(&read_codec);
		}

		FST_TEST_END()

		FST_TEST_BEGIN(amr_be_packet_length)
		{
			/* RFC 4867 4.3: a bandwidth-efficient packet is CMR(4) + TOC(6) + speech bits, padded to a
			   whole octet. Every packet of a mode must have exactly that length. */
			int mode, i;
			char fmtp[64];

			for (mode = 0; mode < 8; mode++) {
				switch_codec_t c = { 0 };
				uint8_t out[SWITCH_RECOMMENDED_BUFFER_SIZE];
				double phase = 0;
				int expect = (10 + nb_bits[mode] + 7) / 8, bad = 0;

				switch_snprintf(fmtp, sizeof(fmtp), "mode-set=%d", mode);
				fst_requires(amr_init(&c, fmtp, fst_pool) == SWITCH_STATUS_SUCCESS);
				for (i = 0; i < 50; i++) {
					uint32_t len = enc_frame(&c, 300 + 37 * i, &phase, out);
					if ((int)len != expect || packet_ft(out, 0) != mode) bad++;
				}
				if (bad) printf("BE mode %d: %d of 50 packets had the wrong length or FT (expected %d bytes)\n", mode, bad, expect);
				fst_check_int_equals(bad, 0);
				switch_core_codec_destroy(&c);
			}
		}
		FST_TEST_END()

		FST_TEST_BEGIN(amr_oa_rejects_truncated_packet)
		{
			/* mode 7 OA packet = CMR + TOC + 31 speech bytes = 33; one byte short must be refused */
			switch_codec_t c = { 0 };
			uint8_t out[SWITCH_RECOMMENDED_BUFFER_SIZE], dec[SWITCH_RECOMMENDED_BUFFER_SIZE];
			uint32_t len, dlen = sizeof(dec), rate = 8000;
			unsigned int flag = 0;
			double phase = 0;

			fst_requires(amr_init(&c, "octet-align=1;mode-set=7", fst_pool) == SWITCH_STATUS_SUCCESS);
			len = enc_frame(&c, 440, &phase, out);
			fst_check_int_equals(len, 33);
			fst_check(switch_core_codec_decode(&c, NULL, out, len, 8000, dec, &dlen, &rate, &flag) == SWITCH_STATUS_SUCCESS);
			dlen = sizeof(dec);
			fst_check(switch_core_codec_decode(&c, NULL, out, len - 1, 8000, dec, &dlen, &rate, &flag) != SWITCH_STATUS_SUCCESS);
			dlen = sizeof(dec);
			fst_check(switch_core_codec_decode(&c, NULL, out, 1, 8000, dec, &dlen, &rate, &flag) != SWITCH_STATUS_SUCCESS);
			switch_core_codec_destroy(&c);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(amr_be_rejects_truncated_packet)
		{
			switch_codec_t c = { 0 };
			uint8_t out[SWITCH_RECOMMENDED_BUFFER_SIZE], dec[SWITCH_RECOMMENDED_BUFFER_SIZE];
			uint32_t len, dlen = sizeof(dec), rate = 8000;
			unsigned int flag = 0;
			double phase = 0;

			fst_requires(amr_init(&c, "mode-set=7", fst_pool) == SWITCH_STATUS_SUCCESS);
			len = enc_frame(&c, 440, &phase, out);
			fst_check(switch_core_codec_decode(&c, NULL, out, len, 8000, dec, &dlen, &rate, &flag) == SWITCH_STATUS_SUCCESS);
			dlen = sizeof(dec);
			fst_check(switch_core_codec_decode(&c, NULL, out, len - 2, 8000, dec, &dlen, &rate, &flag) != SWITCH_STATUS_SUCCESS);
			switch_core_codec_destroy(&c);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(amr_adjust_bitrate_stays_valid)
		{
			/* "increase" from the top mode or "decrease" from mode 1 must still encode a valid speech mode */
			switch_codec_t c = { 0 };
			uint8_t out[SWITCH_RECOMMENDED_BUFFER_SIZE];
			double phase = 0;
			uint32_t len;
			int ft;

			fst_requires(amr_init(&c, "octet-align=1;mode-set=7", fst_pool) == SWITCH_STATUS_SUCCESS);
			switch_core_codec_control(&c, SCC_AUDIO_ADJUST_BITRATE, SCCT_STRING, (void *)"increase", SCCT_NONE, NULL, NULL, NULL);
			len = enc_frame(&c, 440, &phase, out);
			ft = len ? packet_ft(out, 1) : -1;
			printf("after increase from mode 7: FT %d, len %u\n", ft, len);
			fst_check(len > 0 && ft == 7); /* already at the top: stay there */
			switch_core_codec_destroy(&c);

			memset(&c, 0, sizeof(c));
			fst_requires(amr_init(&c, "octet-align=1;mode-set=1", fst_pool) == SWITCH_STATUS_SUCCESS);
			switch_core_codec_control(&c, SCC_AUDIO_ADJUST_BITRATE, SCCT_STRING, (void *)"decrease", SCCT_NONE, NULL, NULL, NULL);
			len = enc_frame(&c, 440, &phase, out);
			ft = len ? packet_ft(out, 1) : -1;
			printf("after decrease from mode 1: FT %d, len %u\n", ft, len);
			fst_check(len > 0 && ft >= 0 && ft <= 1); /* must go down, never wrap to the top */
			switch_core_codec_destroy(&c);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(amr_fmtp_many_params)
		{
			/* VoLTE offers carry many parameters; octet-align must be honoured even as the 11th */
			switch_codec_t c = { 0 };
			const char *fmtp = "mode-set=7;mode-change-period=2;mode-change-capability=2;mode-change-neighbor=1;max-red=0;"
				"crc=0;robust-sorting=0;ptime=20;maxptime=40;channels=1;octet-align=1";

			fst_requires(amr_init(&c, fmtp, fst_pool) == SWITCH_STATUS_SUCCESS);
			printf("fmtp_out: %s\n", c.fmtp_out);
			fst_check(c.fmtp_out && strstr(c.fmtp_out, "octet-align=1"));
			switch_core_codec_destroy(&c);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(amr_multi_frame_packet)
		{
			/* RFC 4867 4.4: one packet may carry several frames (F bit set in all TOCs but the last) */
			switch_codec_t c = { 0 };
			uint8_t f1[64], f2[64], pkt[128], dec[SWITCH_RECOMMENDED_BUFFER_SIZE];
			uint32_t l1, l2, dlen = sizeof(dec), rate = 8000;
			unsigned int flag = 0;
			double phase = 0;
			int pl = 0;

			fst_requires(amr_init(&c, "octet-align=1;mode-set=7", fst_pool) == SWITCH_STATUS_SUCCESS);
			l1 = enc_frame(&c, 440, &phase, f1);
			l2 = enc_frame(&c, 440, &phase, f2);
			fst_requires(l1 == 33 && l2 == 33);
			pkt[pl++] = f1[0];             /* CMR */
			pkt[pl++] = f1[1] | 0x80;      /* TOC 1, F=1: another frame follows */
			pkt[pl++] = f2[1];             /* TOC 2, F=0 */
			memcpy(pkt + pl, f1 + 2, l1 - 2); pl += l1 - 2;
			memcpy(pkt + pl, f2 + 2, l2 - 2); pl += l2 - 2;
			fst_check(switch_core_codec_decode(&c, NULL, pkt, pl, 8000, dec, &dlen, &rate, &flag) == SWITCH_STATUS_SUCCESS);
			printf("2-frame packet (%d bytes) decoded to %u bytes\n", pl, dlen);
			fst_check_int_equals(dlen, 640);
			switch_core_codec_destroy(&c);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(amr_roundtrip_oa_be)
		{
			/* 1 s of a 1 kHz tone survives encode + decode, in both payload formats */
			const char *fmtps[2] = { "octet-align=1;mode-set=7", "mode-set=7" };
			int k, i;

			for (k = 0; k < 2; k++) {
				switch_codec_t c = { 0 };
				uint8_t pkt[256];
				int16_t pcm[8000], out[160];
				double phase = 0, r;
				int got = 0;

				fst_requires(amr_init(&c, fmtps[k], fst_pool) == SWITCH_STATUS_SUCCESS);
				for (i = 0; i < 50; i++) {
					uint32_t len = enc_frame(&c, 1000, &phase, pkt);
					int d = dec_packet(&c, pkt, (int) len, out, sizeof(out));
					fst_requires(d == 320);
					memcpy(pcm + got, out, 320);
					got += 160;
				}
				r = tone_ratio(pcm + 1600, 6400, 1000, 8000);
				printf("%s round trip: 1 kHz carries %.0f%% of the power\n", k ? "BE" : "OA", r * 100);
				fst_check(r > 0.8);
				switch_core_codec_destroy(&c);
			}
		}
		FST_TEST_END()

		FST_TEST_BEGIN(amr_40ms_packets)
		{
			/* a 40 ms implementation puts 2 frames in each packet and decodes them back */
			switch_codec_t c = { 0 };
			switch_codec_settings_t cs = {{ 0 }};
			int16_t in[320], out[640];
			uint8_t pkt[256];
			uint32_t len = sizeof(pkt), rate = 8000;
			unsigned int flag = 0;
			double phase = 0;

			fst_requires(switch_core_codec_init(&c, "AMR", "mod_amr", "octet-align=1;mode-set=7", 8000, 40, 1,
												SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, &cs, fst_pool) == SWITCH_STATUS_SUCCESS);
			fst_check_int_equals(c.implementation->microseconds_per_packet, 40000);
			tone_frame(in, 320, 1000, 8000, &phase);
			fst_requires(switch_core_codec_encode(&c, NULL, in, sizeof(in), 8000, pkt, &len, &rate, &flag) == SWITCH_STATUS_SUCCESS);
			/* CMR, TOC with F=1, TOC with F=0, 2 x 31 speech bytes */
			fst_check_int_equals(len, 1 + 2 + 2 * 31);
			fst_check((pkt[1] & 0x80) && !(pkt[2] & 0x80));
			fst_check_int_equals(dec_packet(&c, pkt, (int) len, out, sizeof(out)), 640);
			switch_core_codec_destroy(&c);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(amr_interleaving_negotiated_send)
		{
			/* with interleaving negotiated our packets carry ILL/ILP (ILL=0) and the answer echoes the parameter */
			switch_codec_t c = { 0 };
			uint8_t pkt[256];
			int16_t out[160];
			double phase = 0;
			uint32_t len;

			fst_requires(amr_init(&c, "octet-align=1;mode-set=7;interleaving=8", fst_pool) == SWITCH_STATUS_SUCCESS);
			printf("fmtp_out: %s\n", c.fmtp_out);
			fst_check(strstr(c.fmtp_out, "interleaving=8") != NULL);
			len = enc_frame(&c, 1000, &phase, pkt);
			fst_check_int_equals(len, 34); /* CMR + ILL/ILP + TOC + 31 */
			fst_check_int_equals(pkt[1], 0x00); /* ILL=0, ILP=0 */
			fst_check_int_equals((pkt[2] >> 3) & 0x0f, 7);
			fst_check_int_equals(dec_packet(&c, pkt, (int) len, out, sizeof(out)), 320);
			switch_core_codec_destroy(&c);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(amr_deinterleave_exact)
		{
			/* Receive side of RFC 4867 4.4.1. 3 groups of 8 frames, sent as 4 packets of 2 frames each
			   (ILL=3): packet ILP=p carries frames p and p+4 of its group. The decoded audio must be
			   bit-identical to a decoder given the same frames in time order (after the one-group delay).
			   Then the same with packet ILP=1 of the middle group lost: exactly frames 9 and 13 become lost. */
			const int N = 2, L = 3, G = N * (L + 1), GROUPS = 3, TOTAL = G * GROUPS;
			switch_codec_t enc = { 0 };
			frame_t frames[24];
			uint8_t pkt[256];
			double phase = 0;
			int i, g, p, k, pass;

			fst_requires(amr_init(&enc, "octet-align=1;mode-set=7", fst_pool) == SWITCH_STATUS_SUCCESS);
			for (i = 0; i < TOTAL; i++) {
				uint32_t len = enc_frame(&enc, 300 + 60 * i, &phase, pkt); /* a different tone per frame */
				fst_requires(oa_frame(pkt, (int) len, &frames[i]) == 0);
			}
			switch_core_codec_destroy(&enc);

			for (pass = 0; pass < 2; pass++) {
				switch_codec_t dil = { 0 }, ref = { 0 };
				int16_t got[24 * 160 + 4 * 160], want[24 * 160 + 4 * 160];
				int ngot = 0, nwant = 0, lost_group = 1, lost_ilp = 1, mismatch = 0;

				fst_requires(amr_init(&dil, "octet-align=1;mode-set=7;interleaving=8", fst_pool) == SWITCH_STATUS_SUCCESS);
				fst_requires(amr_init(&ref, "octet-align=1;mode-set=7", fst_pool) == SWITCH_STATUS_SUCCESS);

				for (g = 0; g < GROUPS; g++) {
					for (p = 0; p <= L; p++) {
						int pl = 0, d;
						if (pass == 1 && g == lost_group && p == lost_ilp) continue; /* lost on the network */
						pkt[pl++] = 0xf0;                                    /* CMR: no request */
						pkt[pl++] = (uint8_t)((L << 4) | p);                  /* ILL, ILP */
						for (k = 0; k < N; k++) pkt[pl++] = frames[g * G + p + k * (L + 1)].toc | (k < N - 1 ? 0x80 : 0);
						for (k = 0; k < N; k++) {
							const frame_t *f = &frames[g * G + p + k * (L + 1)];
							memcpy(pkt + pl, f->data, f->bytes); pl += f->bytes;
						}
						d = dec_packet(&dil, pkt, pl, got + ngot, (int) sizeof(got) - ngot * 2);
						fst_requires(d == N * 320);
						ngot += d / 2;
					}
				}

				/* reference: the frames in time order, behind the delay the de-interleaver adds (L packets of
				   N frames before the first group is complete), with the lost packet's frames as NO_DATA */
				for (i = 0; i < L * N + TOTAL; i++) {
					const frame_t *f = &no_data_frame;
					int fi = i - L * N, d, pl = 0;
					if (fi >= 0) {
						int pos = fi % G;
						if (!(pass == 1 && fi / G == lost_group && pos % (L + 1) == lost_ilp)) f = &frames[fi];
					}
					pkt[pl++] = 0xf0;
					pkt[pl++] = f->toc;
					memcpy(pkt + pl, f->data, f->bytes); pl += f->bytes;
					d = dec_packet(&ref, pkt, pl, want + nwant, (int) sizeof(want) - nwant * 2);
					fst_requires(d == 320);
					nwant += 160;
				}

				/* the last group is only complete when its last packet arrives, so compare what came out */
				for (i = 0; i < ngot && i < nwant; i++) mismatch += got[i] != want[i];
				printf("%s: %d samples decoded, %d differ from the in-order reference\n",
					   pass ? "with packet loss" : "no loss", ngot, mismatch);
				fst_check(ngot >= (TOTAL - N) * 160);
				fst_check_int_equals(mismatch, 0);

				switch_core_codec_destroy(&dil);
				switch_core_codec_destroy(&ref);
			}
		}
		FST_TEST_END()

		FST_TEST_BEGIN(amr_interleaving_rejects_bad_ilp)
		{
			/* 4.4.1: ILP greater than ILL -> discard */
			switch_codec_t c = { 0 };
			int16_t out[640];
			uint8_t pkt[] = { 0xf0, 0x12, 0x7c }; /* ILL=1, ILP=2, one NO_DATA frame */

			fst_requires(amr_init(&c, "octet-align=1;interleaving=4", fst_pool) == SWITCH_STATUS_SUCCESS);
			fst_check(dec_packet(&c, pkt, sizeof(pkt), out, sizeof(out)) < 0);
			pkt[1] = 0x11; /* ILP=1: fine */
			fst_check(dec_packet(&c, pkt, sizeof(pkt), out, sizeof(out)) == 320);
			switch_core_codec_destroy(&c);
		}
		FST_TEST_END()
	}
	FST_SUITE_END()
}
FST_CORE_END()
#endif 
