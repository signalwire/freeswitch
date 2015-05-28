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
 * Bret McDanel <trixter AT 0xdecafbad.com>
 *
 *
 * mod_rss.c -- RSS Browser
 *
 */
#include <switch.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_rss_load);
SWITCH_MODULE_DEFINITION(mod_rss, mod_rss_load, NULL, NULL);

typedef enum {
	SFLAG_INSTRUCT = (1 << 0),
	SFLAG_INFO = (1 << 1),
	SFLAG_MAIN = (1 << 2)
} SFLAGS;

/* helper object */
struct dtmf_buffer {
	int32_t index;
	uint32_t flags;
	int32_t speed;
	char voice[80];
	switch_speech_handle_t *sh;
};

#define TTS_MEAN_SPEED 170
#define TTS_MAX_ENTRIES 99
#define TTS_DEFAULT_ENGINE "flite"
#define TTS_DEFAULT_VOICE "slt"

#define MATCH_COUNT

struct rss_entry {
	uint8_t inuse;
	char *title_txt;
	char *description_txt;
	char *subject_txt;
	char *dept_txt;
};

#ifdef MATCH_COUNT
static uint32_t match_count(char *str, uint32_t max)
{
	char tstr[80] = "";
	uint32_t matches = 0, x = 0;
	uint32_t len = (uint32_t) strlen(str);

	for (x = 0; x < max; x++) {
		switch_snprintf(tstr, sizeof(tstr), "%u", x);
		if (!strncasecmp(str, tstr, len)) {
			matches++;
		}
	}
	return matches;
}
#endif


/*
  dtmf handler function you can hook up to be executed when a digit is dialed during playback 
  if you return anything but SWITCH_STATUS_SUCCESS the playback will stop.
*/
static switch_status_t on_dtmf(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen)
{
	switch (itype) {
	case SWITCH_INPUT_TYPE_DTMF:{
			switch_dtmf_t *dtmf = (switch_dtmf_t *) input;
			struct dtmf_buffer *dtb;
			dtb = (struct dtmf_buffer *) buf;

			switch (dtmf->digit) {
			case '#':
				switch_set_flag(dtb, SFLAG_MAIN);
				return SWITCH_STATUS_BREAK;
			case '6':
				dtb->index++;
				return SWITCH_STATUS_BREAK;
			case '4':
				dtb->index--;
				return SWITCH_STATUS_BREAK;
			case '*':
				if (switch_test_flag(dtb->sh, SWITCH_SPEECH_FLAG_PAUSE)) {
					switch_clear_flag(dtb->sh, SWITCH_SPEECH_FLAG_PAUSE);
				} else {
					switch_set_flag(dtb->sh, SWITCH_SPEECH_FLAG_PAUSE);
				}
				break;
			case '5':
				switch_core_speech_text_param_tts(dtb->sh, "voice", "next");
				switch_set_flag(dtb, SFLAG_INFO);
				return SWITCH_STATUS_BREAK;
			case '9':
				switch_core_speech_text_param_tts(dtb->sh, "voice", dtb->voice);
				switch_set_flag(dtb, SFLAG_INFO);
				return SWITCH_STATUS_BREAK;
			case '2':
				if (dtb->speed < 260) {
					dtb->speed += 30;
					switch_core_speech_numeric_param_tts(dtb->sh, "speech/rate", dtb->speed);
					switch_set_flag(dtb, SFLAG_INFO);
					return SWITCH_STATUS_BREAK;
				}
				break;
			case '7':
				dtb->speed = TTS_MEAN_SPEED;
				switch_core_speech_numeric_param_tts(dtb->sh, "speech/rate", dtb->speed);
				switch_set_flag(dtb, SFLAG_INFO);
				return SWITCH_STATUS_BREAK;
			case '8':
				if (dtb->speed > 80) {
					dtb->speed -= 30;
					switch_core_speech_numeric_param_tts(dtb->sh, "speech/rate", dtb->speed);
					switch_set_flag(dtb, SFLAG_INFO);
					return SWITCH_STATUS_BREAK;
				}
				break;
			case '0':
				switch_set_flag(dtb, SFLAG_INSTRUCT);
				return SWITCH_STATUS_BREAK;
			}
		}
		break;
	default:
		break;
	}
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_APP(rss_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status;
	const char *err = NULL;
	struct dtmf_buffer dtb = { 0 };
	switch_xml_t xml = NULL, item, xchannel = NULL;
	struct rss_entry entries[TTS_MAX_ENTRIES] = { {0} };
	uint32_t i = 0;
	char *title_txt = "", *description_txt = "", *rights_txt = "";
	switch_codec_t speech_codec;
	char *engine = TTS_DEFAULT_ENGINE;
	char *voice = TTS_DEFAULT_VOICE;
	char *timer_name = NULL;
	switch_speech_handle_t sh;
	switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_NONE;
	switch_timer_t timer = { 0 }, *timerp = NULL;
	uint32_t last;
	char *mydata = NULL;
	char *filename = NULL;
	char *argv[3], *feed_list[TTS_MAX_ENTRIES] = { 0 }, *feed_names[TTS_MAX_ENTRIES] = {
	0};
	int feed_index = 0;
	const char *cf = "rss.conf";
	switch_xml_t cfg, cxml, feeds, feed;
	char buf[1024] = "";
	int32_t jumpto = -1;
	uint32_t matches = 0;
	switch_input_args_t args = { 0 };
	const char *vcf = NULL;
	char *chanvars = switch_channel_build_param_string(channel, NULL, NULL);
	switch_codec_implementation_t read_impl = { 0 };
	uint32_t rate, interval, channels;
	switch_core_session_get_read_impl(session, &read_impl);
	interval = read_impl.microseconds_per_packet / 1000;

	if ((vcf = switch_channel_get_variable(channel, "rss_alt_config"))) {
		cf = vcf;
	}

	if (!(cxml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return;
	}
	switch_safe_free(chanvars);

	if ((feeds = switch_xml_child(cfg, "feeds"))) {
		for (feed = switch_xml_child(feeds, "feed"); feed; feed = feed->next) {
			char *name = (char *) switch_xml_attr_soft(feed, "name");
			char *expanded = NULL;
			char *idx = feed->txt;

			if ((expanded = switch_channel_expand_variables(channel, idx)) == idx) {
				expanded = NULL;
			} else {
				idx = expanded;
			}

			if (!name) {
				name = "Error No Name.";
			}

			feed_list[feed_index] = switch_core_session_strdup(session, idx);
			switch_safe_free(expanded);

			if ((expanded = switch_channel_expand_variables(channel, name)) == name) {
				expanded = NULL;
			} else {
				name = expanded;
			}
			feed_names[feed_index] = switch_core_session_strdup(session, name);
			switch_safe_free(expanded);
			feed_index++;

		}
	}

	switch_xml_free(cxml);

	switch_channel_answer(channel);

	if (!zstr(data)) {
		if ((mydata = switch_core_session_strdup(session, data))) {
			switch_separate_string(mydata, ' ', argv, sizeof(argv) / sizeof(argv[0]));

			if (argv[0]) {
				engine = argv[0];
				if (argv[1]) {
					voice = argv[1];
					if (argv[2]) {
						jumpto = atoi(argv[2]);
					}
				}
			}
		}
	}

	if (!feed_index) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "No Feeds Specified!\n");
		return;
	}

	if (switch_channel_media_ready(channel)) {
		rate = read_impl.actual_samples_per_second;
		channels = read_impl.number_of_channels;
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Codec Error!\n");
		return;
	}

	memset(&sh, 0, sizeof(sh));
	if (switch_core_speech_open(&sh, engine, voice, rate, interval, channels, &flags, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid TTS module!\n");
		return;
	}

	if (switch_core_codec_init(&speech_codec,
							   "L16",
							   NULL,
							   NULL,
							   (int) rate,
							   interval,
							   1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,
							   switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Raw Codec Activated\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Raw Codec Activation Failed L16@%uhz 1 channel %dms\n", rate, interval);
		flags = 0;
		switch_core_speech_close(&sh, &flags);
		return;
	}

	if (timer_name) {
		if (switch_core_timer_init(&timer, timer_name, interval, (int) (rate / 50), switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Setup timer failed!\n");
			switch_core_codec_destroy(&speech_codec);
			flags = 0;
			switch_core_speech_close(&sh, &flags);
			return;
		}
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Setup timer success %u bytes per %d ms!\n", (rate / 50) * 2, interval);

		/* start a thread to absorb incoming audio */
		switch_core_service_session(session);
		timerp = &timer;
	}

	while (switch_channel_ready(channel)) {
		int32_t len = 0, idx = 0;
		char cmd[3];
	  main_menu:
		filename = NULL;
		len = idx = 0;
		*cmd = '\0';
		title_txt = description_txt = rights_txt = "";

		if (jumpto > -1) {
			switch_snprintf(cmd, sizeof(cmd), "%d", jumpto);
			jumpto = -1;
		} else {
			switch_core_speech_flush_tts(&sh);
			switch_ivr_sleep(session, 500, SWITCH_FALSE, NULL);

#ifdef MATCH_COUNT
			switch_snprintf(buf + len, sizeof(buf) - len, "%s",
							", Main Menu."
							"Select one of the following news sources, or press 0 to exit. ");
#else
			switch_snprintf(buf + len, sizeof(buf) - len, "%s",
							",Main Menu. "
							"Select one of the following news sources, followed by the pound key or press 0 to exit. ");
#endif
			len = (int32_t) strlen(buf);

			for (idx = 0; idx < feed_index; idx++) {
				switch_snprintf(buf + len, sizeof(buf) - len, "%d: %s. />", idx + 1, feed_names[idx]);
				len = (int32_t) strlen(buf);
			}

			args.input_callback = NULL;
			args.buf = cmd;
			args.buflen = sizeof(cmd);
			status = switch_ivr_speak_text_handle(session, &sh, &speech_codec, timerp, buf, &args);
			if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
				goto finished;
			}
		}
		if (*cmd != '\0') {
			int32_t x;
			char *p;

			if (strchr(cmd, '0')) {
				break;
			}

			if ((p = strchr(cmd, '#'))) {
				*p = '\0';
#ifdef MATCH_COUNT
				/* Hmmm... I know there are no more matches so I don't *need* them to press pound but 
				   I already told them to press it.  Will this confuse people or not?  Let's make em press 
				   pound unless this define is enabled for now.
				 */
			} else if (match_count(cmd, feed_index) > 1) {
#else
			} else {
#endif
				char term;
				char *cp;
				switch_size_t blen = sizeof(cmd) - strlen(cmd);

				cp = cmd + blen;
				switch_ivr_collect_digits_count(session, cp, blen, blen, "#", &term, 5000, 0, 0);
			}

			x = atoi(cmd) - 1;

			if (x > -1 && x < feed_index) {
				filename = feed_list[x];
			} else if (matches > 1) {

			} else {
				args.input_callback = NULL;
				args.buf = NULL;
				args.buflen = 0;
				status = switch_ivr_speak_text_handle(session, &sh, &speech_codec, timerp, "I'm sorry. That is an Invalid Selection. ", &args);
				if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
					goto finished;
				}
			}
		}

		if (!filename) {
			continue;
		}

		if (!(xml = switch_xml_parse_file(filename))) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Open of %s failed\n", filename);
			goto finished;
		}

		err = switch_xml_error(xml);

		if (!zstr(err)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Error [%s]\n", err);
			goto finished;
		}

		if ((xchannel = switch_xml_child(xml, "channel"))) {
			switch_xml_t title, description, rights;

			if ((title = switch_xml_child(xchannel, "title"))) {
				title_txt = title->txt;
			}

			if ((description = switch_xml_child(xchannel, "description"))) {
				description_txt = description->txt;
			}

			if ((rights = switch_xml_child(xchannel, "dc:rights"))) {
				rights_txt = rights->txt;
			}
		}


		if (!(item = switch_xml_child(xml, "item"))) {
			if (xchannel) {
				item = switch_xml_child(xchannel, "item");
			}
		}

		memset(entries, 0, sizeof(entries));

		for (i = 0; item; item = item->next) {
			switch_xml_t title, description, subject, dept;
			char *p;

			entries[i].inuse = 1;
			entries[i].title_txt = NULL;
			entries[i].description_txt = NULL;
			entries[i].subject_txt = NULL;
			entries[i].dept_txt = NULL;

			if ((title = switch_xml_child(item, "title"))) {
				entries[i].title_txt = title->txt;
			}

			if ((description = switch_xml_child(item, "description"))) {
				char *t, *e;
				entries[i].description_txt = description->txt;
				for (;;) {
					if (!(t = strchr(entries[i].description_txt, '<'))) {
						break;
					}
					if (!(e = strchr(t, '>'))) {
						break;
					}

					memset(t, 32, ++e - t);
				}
			}

			if ((subject = switch_xml_child(item, "dc:subject"))) {
				entries[i].subject_txt = subject->txt;
			}

			if ((dept = switch_xml_child(item, "slash:department"))) {
				entries[i].dept_txt = dept->txt;
			}

			if (entries[i].description_txt && (p = strchr(entries[i].description_txt, '<'))) {
				*p = '\0';
			}
#ifdef _STRIP_SOME_CHARS_
			for (p = entries[i].description_txt; *p; p++) {
				if (*p == '\'' || *p == '"' || *p == ':') {
					*p = ' ';
				}
			}
#endif
			i++;
		}

		if (switch_channel_ready(channel)) {
			switch_time_exp_t tm;
			char date[80] = "";
			switch_size_t retsize;
			char dtmf[5] = "";

			switch_time_exp_lt(&tm, switch_micro_time_now());
			switch_strftime_nocheck(date, &retsize, sizeof(date), "%I:%M %p", &tm);

			switch_ivr_sleep(session, 500, SWITCH_FALSE, NULL);

			switch_snprintf(buf, sizeof(buf),
							",%s. %s. %s. local time: %s, Press 0 for options, 5 to change voice, or pound to return to the main menu. ",
							title_txt, description_txt, rights_txt, date);
			args.input_callback = NULL;
			args.buf = dtmf;
			args.buflen = sizeof(dtmf);
			status = switch_ivr_speak_text_handle(session, &sh, &speech_codec, timerp, buf, &args);
			if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
				goto finished;
			}
			if (*dtmf != '\0') {
				switch (*dtmf) {
				case '0':
					switch_set_flag(&dtb, SFLAG_INSTRUCT);
					break;
				case '#':
					goto main_menu;
				}
			}
		}

		for (last = 0; last < TTS_MAX_ENTRIES; last++) {
			if (!entries[last].inuse) {
				last--;
				break;
			}
		}

		dtb.index = 0;
		dtb.sh = &sh;
		dtb.speed = TTS_MEAN_SPEED;
		//switch_set_flag(&dtb, SFLAG_INFO);
		switch_copy_string(dtb.voice, voice, sizeof(dtb.voice));
		while (entries[0].inuse && switch_channel_ready(channel)) {
			while (switch_channel_ready(channel)) {
				uint8_t cont = 0;

				if (dtb.index >= TTS_MAX_ENTRIES) {
					dtb.index = 0;
				}
				if (dtb.index < 0) {
					dtb.index = last;
				}

				if (!entries[dtb.index].inuse) {
					dtb.index = 0;
					continue;
				}
				if (switch_channel_ready(channel)) {
					char tmpbuf[1024] = "";
					uint32_t tmplen = 0;

					if (switch_test_flag(&dtb, SFLAG_MAIN)) {
						switch_clear_flag(&dtb, SFLAG_MAIN);
						goto main_menu;
					}
					if (switch_test_flag(&dtb, SFLAG_INFO)) {
						switch_clear_flag(&dtb, SFLAG_INFO);
						switch_snprintf(tmpbuf + tmplen, sizeof(tmpbuf) - tmplen, "%s %s. I am speaking at %u words per minute. ", sh.engine, sh.voice,
										dtb.speed);
						tmplen = (uint32_t) strlen(tmpbuf);
					}

					if (switch_test_flag(&dtb, SFLAG_INSTRUCT)) {
						switch_clear_flag(&dtb, SFLAG_INSTRUCT);
						cont = 1;
						switch_snprintf(tmpbuf + tmplen, sizeof(tmpbuf) - tmplen, "%s",
										"Press star to pause or resume speech. "
										"To go to the next item, press six. "
										"To go back, press 4. "
										"Press two to go faster, eight to slow down, or 7 to resume normal speed. "
										"To change voices, press five. To restore the original voice press 9. "
										"To hear these options again, press zero or press pound to return to the main menu. ");
					} else {
						switch_snprintf(tmpbuf + tmplen, sizeof(tmpbuf) - tmplen, "Story %d. ", dtb.index + 1);
						tmplen = (uint32_t) strlen(tmpbuf);

						if (entries[dtb.index].subject_txt) {
							switch_snprintf(tmpbuf + tmplen, sizeof(tmpbuf) - tmplen, "Subject: %s. ", entries[dtb.index].subject_txt);
							tmplen = (uint32_t) strlen(tmpbuf);
						}

						if (entries[dtb.index].dept_txt) {
							switch_snprintf(tmpbuf + tmplen, sizeof(tmpbuf) - tmplen, "From the %s department. ", entries[dtb.index].dept_txt);
							tmplen = (uint32_t) strlen(tmpbuf);
						}

						if (entries[dtb.index].title_txt) {
							switch_snprintf(tmpbuf + tmplen, sizeof(tmpbuf) - tmplen, "%s", entries[dtb.index].title_txt);
							tmplen = (uint32_t) strlen(tmpbuf);
						}
					}
					switch_core_speech_flush_tts(&sh);
					args.input_callback = on_dtmf;
					args.buf = &dtb;
					args.buflen = sizeof(dtb);
					status = switch_ivr_speak_text_handle(session, &sh, &speech_codec, timerp, tmpbuf, &args);
					if (status == SWITCH_STATUS_BREAK) {
						continue;
					} else if (status != SWITCH_STATUS_SUCCESS) {
						goto finished;
					}

					if (cont) {
						cont = 0;
						continue;
					}

					if (entries[dtb.index].description_txt) {
						args.input_callback = on_dtmf;
						args.buf = &dtb;
						args.buflen = sizeof(dtb);
						status = switch_ivr_speak_text_handle(session, &sh, &speech_codec, timerp, entries[dtb.index].description_txt, &args);
					}
					if (status == SWITCH_STATUS_BREAK) {
						continue;
					} else if (status != SWITCH_STATUS_SUCCESS) {
						goto finished;
					}
				}

				dtb.index++;
			}
		}
	}

  finished:
	switch_core_speech_close(&sh, &flags);
	switch_core_codec_destroy(&speech_codec);

	if (timerp) {
		/* End the audio absorbing thread */
		switch_core_thread_session_end(session);
		switch_core_timer_destroy(&timer);
	}

	switch_xml_free(xml);
	switch_core_session_reset(session, SWITCH_TRUE, SWITCH_TRUE);
}


SWITCH_MODULE_LOAD_FUNCTION(mod_rss_load)
{
	switch_application_interface_t *app_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_APP(app_interface, "rss", NULL, NULL, rss_function, NULL, SAF_NONE);

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
