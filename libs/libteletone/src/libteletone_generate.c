/* 
 * libteletone
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
 * The Original Code is libteletone
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
 * libteletone.c -- Tone Generator
 *
 *
 *
 * Exception:
 * The author hereby grants the use of this source code under the 
 * following license if and only if the source code is distributed
 * as part of the OpenZAP or FreeTDM library.	Any use or distribution of this
 * source code outside the scope of the OpenZAP or FreeTDM library will nullify the
 * following license and reinact the MPL 1.1 as stated above.
 *
 * Copyright (c) 2007, Anthony Minessale II
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.	 IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <libteletone.h>

#define SMAX 32767
#define SMIN -32768
#define normalize_to_16bit(n) if (n > SMAX) n = SMAX; else if (n < SMIN) n = SMIN;

#ifdef _MSC_VER
#pragma warning(disable:4706)
#endif

TELETONE_API_DATA int16_t TELETONE_SINES[SINE_TABLE_MAX] = {
	0x00c9, 0x025b, 0x03ed, 0x057f, 0x0711, 0x08a2, 0x0a33, 0x0bc4,
	0x0d54, 0x0ee4, 0x1073, 0x1201, 0x138f, 0x151c, 0x16a8, 0x1833,
	0x19be, 0x1b47, 0x1cd0, 0x1e57, 0x1fdd, 0x2162, 0x22e5, 0x2467,
	0x25e8, 0x2768, 0x28e5, 0x2a62, 0x2bdc, 0x2d55, 0x2ecc, 0x3042,
	0x31b5, 0x3327, 0x3497, 0x3604, 0x3770, 0x38d9, 0x3a40, 0x3ba5,
	0x3d08, 0x3e68, 0x3fc6, 0x4121, 0x427a, 0x43d1, 0x4524, 0x4675,
	0x47c4, 0x490f, 0x4a58, 0x4b9e, 0x4ce1, 0x4e21, 0x4f5e, 0x5098,
	0x51cf, 0x5303, 0x5433, 0x5560, 0x568a, 0x57b1, 0x58d4, 0x59f4,
	0x5b10, 0x5c29, 0x5d3e, 0x5e50, 0x5f5e, 0x6068, 0x616f, 0x6272,
	0x6371, 0x646c, 0x6564, 0x6657, 0x6747, 0x6832, 0x691a, 0x69fd,
	0x6add, 0x6bb8, 0x6c8f, 0x6d62, 0x6e31, 0x6efb, 0x6fc2, 0x7083,
	0x7141, 0x71fa, 0x72af, 0x735f, 0x740b, 0x74b3, 0x7556, 0x75f4,
	0x768e, 0x7723, 0x77b4, 0x7840, 0x78c8, 0x794a, 0x79c9, 0x7a42,
	0x7ab7, 0x7b27, 0x7b92, 0x7bf9, 0x7c5a, 0x7cb7, 0x7d0f, 0x7d63,
	0x7db1, 0x7dfb, 0x7e3f, 0x7e7f, 0x7eba, 0x7ef0, 0x7f22, 0x7f4e,
	0x7f75, 0x7f98, 0x7fb5, 0x7fce, 0x7fe2, 0x7ff1, 0x7ffa, 0x7fff
};


TELETONE_API(int) teletone_set_tone(teletone_generation_session_t *ts, int index, ...)
{
	va_list ap;
	int i = 0;
	teletone_process_t x = 0;

	va_start(ap, index);
	while (i < TELETONE_MAX_TONES && (x = va_arg(ap, teletone_process_t))) {
		ts->TONES[index].freqs[i++] = x;
	}
	va_end(ap);

	return (i > TELETONE_MAX_TONES) ? -1 : 0;
	
}

TELETONE_API(int) teletone_set_map(teletone_tone_map_t *map, ...)
{
	va_list ap;
	int i = 0;
	teletone_process_t x = 0;

	va_start(ap, map);
	while (i < TELETONE_MAX_TONES && (x = va_arg(ap, teletone_process_t))) {
		map->freqs[i++] = x;
	}
	va_end(ap);

	return (i > TELETONE_MAX_TONES) ? -1 : 0;
	
}

TELETONE_API(int) teletone_init_session(teletone_generation_session_t *ts, int buflen, tone_handler handler, void *user_data)
{
	memset(ts, 0, sizeof(*ts));
	ts->rate = 8000;
	ts->channels = 1;
	ts->duration = 2000;
	ts->wait = 500;
	ts->tmp_duration = -1;
	ts->tmp_wait = -1;
	ts->handler = handler;
	ts->user_data = user_data;
	ts->volume = -7;
	ts->decay_step = 0;
	ts->decay_factor = 1;
	if (buflen) {
		if ((ts->buffer = calloc(buflen, sizeof(teletone_audio_t))) == 0) {
			return -1;
		}
		ts->datalen = buflen;
	} else {
		ts->dynamic = 1024;
	}
	/* Add Standard DTMF Tones */
	teletone_set_tone(ts, '1', 697.0, 1209.0, 0.0);
	teletone_set_tone(ts, '2', 697.0, 1336.0, 0.0);
	teletone_set_tone(ts, '3', 697.0, 1477.0, 0.0);
	teletone_set_tone(ts, 'A', 697.0, 1633.0, 0.0);
	teletone_set_tone(ts, '4', 770.0, 1209.0, 0.0);
	teletone_set_tone(ts, '5', 770.0, 1336.0, 0.0);
	teletone_set_tone(ts, '6', 770.0, 1477.0, 0.0);
	teletone_set_tone(ts, 'B', 770.0, 1633.0, 0.0);
	teletone_set_tone(ts, '7', 859.0, 1209.0, 0.0);
	teletone_set_tone(ts, '8', 859.0, 1336.0, 0.0);
	teletone_set_tone(ts, '9', 859.0, 1477.0, 0.0);
	teletone_set_tone(ts, 'C', 859.0, 1633.0, 0.0);
	teletone_set_tone(ts, '*', 941.0, 1209.0, 0.0);
	teletone_set_tone(ts, '0', 941.0, 1336.0, 0.0);
	teletone_set_tone(ts, '#', 941.0, 1477.0, 0.0);
	teletone_set_tone(ts, 'D', 941.0, 1633.0, 0.0);
	
	return 0;
}

TELETONE_API(int) teletone_destroy_session(teletone_generation_session_t *ts)
{
	if (ts->buffer) {
		free(ts->buffer);
		ts->buffer = NULL;
		ts->samples = 0;
	}
	return 0;
}

static int ensure_buffer(teletone_generation_session_t *ts, int need)
{
	need += ts->samples;
	need *= sizeof(teletone_audio_t);
	need *= ts->channels;

	if (need > ts->datalen) {
		teletone_audio_t *tmp;
		ts->datalen = need + ts->dynamic;
		tmp = realloc(ts->buffer, ts->datalen);
		if (!tmp) {
			return -1;
		}
		ts->buffer = tmp;
	}

	return 0;
}

TELETONE_API(int) teletone_mux_tones(teletone_generation_session_t *ts, teletone_tone_map_t *map)
{
	/*teletone_process_t period = (1.0 / ts->rate) / ts->channels;*/
	int i, c;
	int freqlen = 0;
	teletone_dds_state_t tones[TELETONE_MAX_TONES+1];
	//int decay = 0;
	int duration;
	int wait = 0;
	int32_t sample;
	int32_t dc = 0;
	float vol = ts->volume;
	ts->samples = 0;
	memset(tones, 0, sizeof(tones[0]) * TELETONE_MAX_TONES);
	duration = (ts->tmp_duration > -1) ? ts->tmp_duration : ts->duration;
	wait = (ts->tmp_wait > -1) ? ts->tmp_wait : ts->wait;

	if (map->freqs[0] > 0) {
		for (freqlen = 0; freqlen < TELETONE_MAX_TONES && map->freqs[freqlen]; freqlen++) {
			teletone_dds_state_set_tone(&tones[freqlen], map->freqs[freqlen], ts->rate, 0);
			teletone_dds_state_set_tx_level(&tones[freqlen], vol);
		}
	
		if (ts->channels > 1) {
			duration *= ts->channels;
		}

		if (ts->dynamic) {
			if (ensure_buffer(ts, duration)) {
				return -1;
			}
		}

		for (ts->samples = 0; ts->samples < ts->datalen && ts->samples < duration; ts->samples++) {
			if (ts->decay_direction && ++dc >= ts->decay_step) {
				float nvol = vol + ts->decay_direction * ts->decay_factor;
				int j;

				if (nvol <= TELETONE_VOL_DB_MAX && nvol >= TELETONE_VOL_DB_MIN) {
					vol = nvol;
					for (j = 0; j < TELETONE_MAX_TONES && map->freqs[j]; j++) {					
						teletone_dds_state_set_tx_level(&tones[j], vol);
					}
					dc = 0;
				}
			}

			sample = 128;

			for (i = 0; i < freqlen; i++) {
				int32_t s = teletone_dds_state_modulate_sample(&tones[i], 0);
				sample += s;
			}
			sample /= freqlen;
			ts->buffer[ts->samples] = (teletone_audio_t)sample;
			
			for (c = 1; c < ts->channels; c++) {
				ts->buffer[ts->samples+1] = ts->buffer[ts->samples];
				ts->samples++;
			}
			
		}
	}
	if (ts->dynamic) {
		if (ensure_buffer(ts, wait)) {
			return -1;
		}
	}
	for (c = 0; c < ts->channels; c++) {
		for (i = 0; i < wait && ts->samples < ts->datalen; i++) {
			ts->buffer[ts->samples++] = 0;
		}
	}

	if (ts->debug && ts->debug_stream) {
		if (map->freqs[0] <= 0) {
			fprintf(ts->debug_stream, "wait %d (%dms)\n", wait, wait / (ts->rate / 1000));
		} else {
			fprintf(ts->debug_stream, "Generate: (");

			for (i = 0; i < TELETONE_MAX_TONES && map->freqs[i]; i++) {
				fprintf(ts->debug_stream, "%s%0.2f", i == 0 ? "" : "+",map->freqs[i]);
			}
			 
			fprintf(ts->debug_stream, 
					") [volume %0.2fdB; samples %d(%dms) x %d channel%s; wait %d(%dms); decay_factor %0.2fdB; decay_step %d(%dms); wrote %d bytes]\n",
					ts->volume,
					duration,
					duration / (ts->rate / 1000), 
					ts->channels,
					ts->channels == 1 ? "" : "s",
					wait,
					wait / (ts->rate / 1000),
					ts->decay_factor,
					ts->decay_step,
					ts->decay_step / (ts->rate / 1000),					
					ts->samples * 2);
		}
	}	
	return ts->samples / ts->channels;
}

/* don't ask */
static char *my_strdup (const char *s)
{
	size_t len = strlen (s) + 1;
	void *new = malloc (len);
	
	if (new == NULL) {
		return NULL;
	}

	return (char *) memcpy (new, s, len);
}

TELETONE_API(int) teletone_run(teletone_generation_session_t *ts, const char *cmd)
{
	char *data = NULL, *cur = NULL, *end = NULL;
	int LOOPING = 0;
	
	if (!cmd) {
		return -1;
	}

	do {
		if (!(data = my_strdup(cmd))) {
			return -1;
		}

		cur = data;

		while (*cur) {
			if (*cur == ' ' || *cur == '\r' || *cur == '\n') {
				cur++;
				continue;
			}

			if ((end = strchr(cur, ';')) != 0) {
				*end++ = '\0';
			}
			
			if (*(cur + 1) == '=') {
				switch(*cur) {
				case 'c':
					ts->channels = atoi(cur + 2);
					break;
				case 'r':
					ts->rate = atoi(cur + 2);
					break;
				case 'd':
					ts->duration = atoi(cur + 2) * (ts->rate / 1000);
					break;
				case 'v':
					{
						float vol = (float)atof(cur + 2);
						if (vol <= TELETONE_VOL_DB_MAX && vol >= TELETONE_VOL_DB_MIN) {
							ts->volume = vol;
						}
					}
					break;
				case '>':
					ts->decay_step = atoi(cur + 2) * (ts->rate / 1000);
					ts->decay_direction = -1;
					break;
				case '<':
					ts->decay_step = atoi(cur + 2) * (ts->rate / 1000);
					ts->decay_direction = 1;
					break;
				case '+':
					ts->decay_factor = (float)atof(cur + 2);
					break;
				case 'w':
					ts->wait = atoi(cur + 2) * (ts->rate / 1000);
					break;
				case 'l':
					ts->loops = atoi(cur + 2); 
					break;
				case 'L':
					if (!LOOPING) {
						int L;
						if ((L = atoi(cur + 2)) > 0) {
							ts->LOOPS = L;
							LOOPING++;
						}
					}
					break;
				}
			} else {
				while (*cur) {
					char *p = NULL, *e = NULL;
					teletone_tone_map_t mymap, *mapp = NULL;

					if (*cur == ' ' || *cur == '\r' || *cur == '\n') {
						cur++;
						continue;
					}
					
					ts->tmp_duration = -1;
					ts->tmp_wait = -1;

					memset(&mymap, 0, sizeof(mymap));

					if (*(cur + 1) == '(') {
						p = cur + 2;
						if (*cur) {
							char *next;
							int i = 0;
							if ((e = strchr(p, ')')) != 0) {
								*e++ = '\0';
							}
							do {
								if ((next = strchr(p, ',')) != 0) {
									*next++ = '\0';
								}
								if (i == 0) {
									ts->tmp_duration = atoi(p) * (ts->rate / 1000);
									i++;
								} else if (i == 1) {
									ts->tmp_wait = atoi(p) * (ts->rate / 1000);
									i++;
								} else {
									mymap.freqs[i++ - 2] = atof(p);
								}
								p = next;

							} while (next && (i-2) < TELETONE_MAX_TONES);
							if (i > 2 && *cur == '%') {
								mapp = &mymap;
							} else if ((i != 2 || *cur == '%')) { 
								if (ts->debug && ts->debug_stream) {
									fprintf(ts->debug_stream, "Syntax Error!\n");
								}
								goto bottom;
							}
						} 
					}

					if (*cur && !mapp) {
						if (*cur > 0 && *cur < TELETONE_TONE_RANGE) { 
							mapp = &ts->TONES[(int)*cur];
						} else if (ts->debug && ts->debug_stream) {
							fprintf(ts->debug_stream, "Map [%c] Out Of Range!\n", *cur);
						}
					}

					if (mapp) {
						if (mapp->freqs[0]) {
							if (ts->handler) {
								do {
									ts->handler(ts, mapp);
									if (ts->loops > 0) {
										ts->loops--;
									}
								} while (ts->loops);
							}
						} else if (ts->debug && ts->debug_stream) {
							fprintf(ts->debug_stream, "Ignoring Empty Map [%c]!\n", *cur);
						}
					}
					
					if (e) {
						cur = e;
					} else {
						cur++;
					}
				}
			}

			if (end) {
				cur = end;
			} else if (*cur){
				cur++;
			}
		}
	bottom:
		free(data);
		data = NULL;

		if (ts->LOOPS > 0) {
			ts->LOOPS--;
		}

	} while (ts->LOOPS);

	return 0;
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
