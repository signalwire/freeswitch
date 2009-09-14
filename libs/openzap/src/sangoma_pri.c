/*****************************************************************************
 * sangoma_pri.c libpri Sangoma integration
 *
 * Author(s):	Anthony Minessale II <anthmct@yahoo.com>
 *              Nenad Corbic <ncorbic@sangoma.com>
 *
 * Copyright:	(c) 2005 Anthony Minessale II
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 * ============================================================================
 */

#include "openzap.h"
#include <sangoma_pri.h>
#ifndef HAVE_GETTIMEOFDAY

#ifdef WIN32
#include <mmsystem.h>

static __inline int gettimeofday(struct timeval *tp, void *nothing)
{
#ifdef WITHOUT_MM_LIB
	SYSTEMTIME st;
	time_t tt;
	struct tm tmtm;
	/* mktime converts local to UTC */
	GetLocalTime (&st);
	tmtm.tm_sec = st.wSecond;
	tmtm.tm_min = st.wMinute;
	tmtm.tm_hour = st.wHour;
	tmtm.tm_mday = st.wDay;
	tmtm.tm_mon = st.wMonth - 1;
	tmtm.tm_year = st.wYear - 1900;  tmtm.tm_isdst = -1;
	tt = mktime (&tmtm);
	tp->tv_sec = tt;
	tp->tv_usec = st.wMilliseconds * 1000;
#else
	/**
	 ** The earlier time calculations using GetLocalTime
	 ** had a time resolution of 10ms.The timeGetTime, part
	 ** of multimedia apis offer a better time resolution
	 ** of 1ms.Need to link against winmm.lib for this
	 **/
	unsigned long Ticks = 0;
	unsigned long Sec =0;
	unsigned long Usec = 0;
	Ticks = timeGetTime();

	Sec = Ticks/1000;
	Usec = (Ticks - (Sec*1000))*1000;
	tp->tv_sec = Sec;
	tp->tv_usec = Usec;
#endif /* WITHOUT_MM_LIB */
	(void)nothing;
	return 0;
}
#endif /* WIN32 */
#endif /* HAVE_GETTIMEOFDAY */

static struct sangoma_pri_event_list SANGOMA_PRI_EVENT_LIST[] = {
	{0, SANGOMA_PRI_EVENT_ANY, "ANY"},
	{1, SANGOMA_PRI_EVENT_DCHAN_UP, "DCHAN_UP"},
	{2, SANGOMA_PRI_EVENT_DCHAN_DOWN, "DCHAN_DOWN"},
	{3, SANGOMA_PRI_EVENT_RESTART, "RESTART"},
	{4, SANGOMA_PRI_EVENT_CONFIG_ERR, "CONFIG_ERR"},
	{5, SANGOMA_PRI_EVENT_RING, "RING"},
	{6, SANGOMA_PRI_EVENT_HANGUP, "HANGUP"},
	{7, SANGOMA_PRI_EVENT_RINGING, "RINGING"},
	{8, SANGOMA_PRI_EVENT_ANSWER, "ANSWER"},
	{9, SANGOMA_PRI_EVENT_HANGUP_ACK, "HANGUP_ACK"},
	{10, SANGOMA_PRI_EVENT_RESTART_ACK, "RESTART_ACK"},
	{11, SANGOMA_PRI_EVENT_FACNAME, "FACNAME"},
	{12, SANGOMA_PRI_EVENT_INFO_RECEIVED, "INFO_RECEIVED"},
	{13, SANGOMA_PRI_EVENT_PROCEEDING, "PROCEEDING"},
	{14, SANGOMA_PRI_EVENT_SETUP_ACK, "SETUP_ACK"},
	{15, SANGOMA_PRI_EVENT_HANGUP_REQ, "HANGUP_REQ"},
	{16, SANGOMA_PRI_EVENT_NOTIFY, "NOTIFY"},
	{17, SANGOMA_PRI_EVENT_PROGRESS, "PROGRESS"},
	{18, SANGOMA_PRI_EVENT_KEYPAD_DIGIT, "KEYPAD_DIGIT"}
};

#define LINE "--------------------------------------------------------------------------------"

char *sangoma_pri_event_str(sangoma_pri_event_t event_id)
{ 
	return SANGOMA_PRI_EVENT_LIST[event_id].name;
}

static int __pri_sangoma_read(struct pri *pri, void *buf, int buflen)
{
	struct sangoma_pri *spri = (struct sangoma_pri *) pri->userdata;
	zap_size_t len = buflen;
	int res;
	char bb[4096] = "";


	if (zap_channel_read(spri->zdchan, buf, &len) != ZAP_SUCCESS) {
		printf("D-READ FAIL! [%s]\n", spri->zdchan->last_error);
		return 0;
	}
	res = (int)len;
	memset(&((unsigned char*)buf)[res],0,2);
	res+=2;

	//print_bits(buf, res-2, bb, sizeof(bb), 1, 0);
	//zap_log(ZAP_LOG_DEBUG, "READ %d\n%s\n%s\n\n", res-2, LINE, bb);

	return res;
}

static int __pri_sangoma_write(struct pri *pri, void *buf, int buflen)
{
	struct sangoma_pri *spri = (struct sangoma_pri *) pri->userdata;
	int res;
	zap_size_t len = buflen -2;
	char bb[4096] = "";

	if (zap_channel_write(spri->zdchan, buf, buflen, &len) != ZAP_SUCCESS) {
		printf("D-WRITE FAIL! [%s]\n", spri->zdchan->last_error);
        return 0;
	}
	
	//print_bits(buf, (int)buflen-2, bb, sizeof(bb), 1, 0);
	//zap_log(ZAP_LOG_DEBUG, "WRITE %d\n%s\n%s\n\n", (int)buflen-2, LINE, bb);

	return (int) buflen;
}

int sangoma_init_pri(struct sangoma_pri *spri, int span, int dchan, int swtype, int node, int debug)
{
	int ret = -1;
	zap_socket_t dfd = 0;

	memset(spri, 0, sizeof(struct sangoma_pri));

	if (zap_channel_open(span, dchan, &spri->zdchan) != ZAP_SUCCESS) {
		fprintf(stderr, "Unable to open DCHAN %d for span %d (%s)\n", dchan, span, strerror(errno));
	} else {
		if ((spri->pri = pri_new_cb(spri->zdchan->sockfd, node, swtype, __pri_sangoma_read, __pri_sangoma_write, spri))){
			spri->span = span;
			pri_set_debug(spri->pri, debug);
			ret = 0;
		} else {
			fprintf(stderr, "Unable to create PRI\n");
		}
	}
	return ret;
}


int sangoma_one_loop(struct sangoma_pri *spri)
{
	fd_set rfds, efds;
	struct timeval now = {0,0}, *next;
	pri_event *event;
    int sel;
	
	if (spri->on_loop) {
		spri->on_loop(spri);
	}

	FD_ZERO(&rfds);
	FD_ZERO(&efds);

#ifdef _MSC_VER
	//Windows macro for FD_SET includes a warning C4127: conditional expression is constant
#pragma warning(push)
#pragma warning(disable:4127)
#endif

	FD_SET(spri->pri->fd, &rfds);
	FD_SET(spri->pri->fd, &efds);

#ifdef _MSC_VER
#pragma warning(pop)
#endif

	if ((next = pri_schedule_next(spri->pri))) {
		gettimeofday(&now, NULL);
		now.tv_sec = next->tv_sec - now.tv_sec;
		now.tv_usec = next->tv_usec - now.tv_usec;
		if (now.tv_usec < 0) {
			now.tv_usec += 1000000;
			now.tv_sec -= 1;
		}
		if (now.tv_sec < 0) {
			now.tv_sec = 0;
			now.tv_usec = 0;
		}
	}

	sel = select(spri->pri->fd + 1, &rfds, NULL, &efds, next ? &now : NULL);
	event = NULL;

	if (!sel) {
		event = pri_schedule_run(spri->pri);
	} else if (sel > 0) {
		event = pri_check_event(spri->pri);
	}

	if (event) {
		event_handler handler;
		/* 0 is catchall event handler */
		if ((handler = spri->eventmap[event->e] ? spri->eventmap[event->e] : spri->eventmap[0] ? spri->eventmap[0] : NULL)) {
			handler(spri, event->e, event);
		} else {
			fprintf(stderr,"No event handler found for event %d.\n", event->e);
		}
	}

	return sel;
}

int sangoma_run_pri(struct sangoma_pri *spri)
{
	int ret = 0;

	for (;;){
		ret=sangoma_one_loop(spri);
		if (ret < 0){

#ifndef WIN32 //This needs to be adressed fror WIN32 still
			if (errno == EINTR){
				/* Igonore an interrupted system call */
				continue;
			}
#endif	
			printf("Error = %i\n",ret);
			perror("Sangoma Run Pri: ");
			break;		
		}
	}

	return ret;

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

