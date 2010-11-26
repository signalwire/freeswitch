/*
 * ossp-alsap - ossp DSP slave which forwards to alsa
 *
 * Copyright (C)      2009 Maarten Lankhorst <m.b.lankhorst@gmail.com>
 *
 * This file is released under the GPLv2.
 *
 * Why an alsa plugin as well? Just to show how much
 * the alsa userspace api sucks ;-)
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <alsa/asoundlib.h>
#include <sys/soundcard.h>

#include "ossp-slave.h"

enum {
	AFMT_FLOAT		= 0x00004000,
	AFMT_S32_LE		= 0x00001000,
	AFMT_S32_BE		= 0x00002000,
};

static size_t page_size;

/* alsa structures */
static snd_pcm_t *pcm[2];
static snd_pcm_hw_params_t *hw_params;
static snd_pcm_sw_params_t *sw_params;
static int block;

static unsigned int byte_counter[2];
static snd_pcm_uframes_t mmap_pos[2];
static int stream_corked[2];
static int stream_notify;

static struct format {
	snd_pcm_format_t format;
	snd_pcm_sframes_t rate;
	int channels;
} hw_format = { SND_PCM_FORMAT_U8, 8000, 1 };

#if 0
/* future mmap stuff */
static size_t mmap_raw_size, mmap_size;
static int mmap_fd[2] = { -1, -1 };
static void *mmap_map[2];
static uint64_t mmap_idx[2];		/* mmap pointer */
static uint64_t mmap_last_idx[2];	/* last idx for get_ptr */
static struct ring_buf mmap_stg[2];	/* staging ring buffer */
static size_t mmap_lead[2];		/* lead bytes */
static int mmap_sync[2];		/* sync with backend stream */
#endif

static snd_pcm_format_t fmt_oss_to_alsa(int fmt)
{
	switch (fmt) {
	case AFMT_U8:			return SND_PCM_FORMAT_U8;
	case AFMT_A_LAW:		return SND_PCM_FORMAT_A_LAW;
	case AFMT_MU_LAW:		return SND_PCM_FORMAT_MU_LAW;
	case AFMT_S16_LE:		return SND_PCM_FORMAT_S16_LE;
	case AFMT_S16_BE:		return SND_PCM_FORMAT_S16_BE;
	case AFMT_FLOAT:		return SND_PCM_FORMAT_FLOAT;
	case AFMT_S32_LE:		return SND_PCM_FORMAT_S32_LE;
	case AFMT_S32_BE:		return SND_PCM_FORMAT_S32_BE;
	default:			return SND_PCM_FORMAT_U8;
	}
}

static int fmt_alsa_to_oss(snd_pcm_format_t fmt)
{
	switch (fmt) {
	case SND_PCM_FORMAT_U8:		return AFMT_U8;
	case SND_PCM_FORMAT_A_LAW:	return AFMT_A_LAW;
	case SND_PCM_FORMAT_MU_LAW:	return AFMT_MU_LAW;
	case SND_PCM_FORMAT_S16_LE:	return AFMT_S16_LE;
	case SND_PCM_FORMAT_S16_BE:	return AFMT_S16_BE;
	case SND_PCM_FORMAT_FLOAT:	return AFMT_FLOAT;
	case SND_PCM_FORMAT_S32_LE:	return AFMT_S32_LE;
	case SND_PCM_FORMAT_S32_BE:	return AFMT_S32_BE;
	default:			return AFMT_U8;
	}
}

static void flush_streams(int drain)
{
	/* FIXME: snd_pcm_drain appears to be able to deadlock,
	 * always drop or check state? */
	if (drain) {
		if (pcm[PLAY])
			snd_pcm_drain(pcm[PLAY]);
		if (pcm[REC])
			snd_pcm_drain(pcm[REC]);
	} else {
		if (pcm[PLAY])
			snd_pcm_drop(pcm[PLAY]);
		if (pcm[REC])
			snd_pcm_drop(pcm[REC]);
	}

	/* XXX: Really needed? */
#if 0
	if (pcm[PLAY]) {
		snd_pcm_close(pcm[PLAY]);
		snd_pcm_open(&pcm[PLAY], "default",
			     SND_PCM_STREAM_PLAYBACK, block);
	}
	if (pcm[REC]) {
		snd_pcm_close(pcm[REC]);
		snd_pcm_open(&pcm[REC], "default",
			     SND_PCM_STREAM_CAPTURE, block);
	}
#endif
}

static void kill_streams(void)
{
	flush_streams(0);
}

static int trigger_streams(int play, int rec)
{
	int ret = 0;

	if (pcm[PLAY] && play >= 0) {
		ret = snd_pcm_sw_params_set_start_threshold(pcm[PLAY], sw_params,
							   play ? 1 : -1);
		if (ret >= 0)
			snd_pcm_sw_params(pcm[PLAY], sw_params);
	}
	if (ret >= 0 && pcm[REC] && rec >= 0) {
		ret = snd_pcm_sw_params_set_start_threshold(pcm[REC], sw_params,
							    rec ? 1 : -1);
		if (ret >= 0)
			snd_pcm_sw_params(pcm[REC], sw_params);
	}

	return ret;
}

static ssize_t alsap_mixer(enum ossp_opcode opcode,
			   void *carg, void *din, size_t din_sz,
			   void *rarg, void *dout, size_t *dout_szp, int tfd)
{
return -EBUSY;
}

static int set_hw_params(snd_pcm_t *pcm)
{
	int ret;
	unsigned rate;

	ret = snd_pcm_hw_params_any(pcm, hw_params);
	if (ret >= 0)
		ret = snd_pcm_hw_params_set_access(pcm, hw_params,
						   SND_PCM_ACCESS_RW_INTERLEAVED);
	rate = hw_format.rate;
	if (ret >= 0)
		ret = snd_pcm_hw_params_set_rate_minmax(pcm, hw_params,
							&rate, NULL,
							&rate, NULL);
	if (ret >= 0)
		ret = snd_pcm_hw_params_set_format(pcm, hw_params, hw_format.format);
	if (ret >= 0)
		ret = snd_pcm_hw_params_set_channels(pcm, hw_params,
						     hw_format.channels);
	if (ret >= 0)
		ret = snd_pcm_hw_params(pcm, hw_params);
	if (ret >= 0)
		ret = snd_pcm_sw_params_current(pcm, sw_params);
	if (ret >= 0)
		ret = snd_pcm_sw_params(pcm, sw_params);
	return ret;
}

static ssize_t alsap_open(enum ossp_opcode opcode,
			  void *carg, void *din, size_t din_sz,
			  void *rarg, void *dout, size_t *dout_szp, int tfd)
{
	struct ossp_dsp_open_arg *arg = carg;
	int ret;
	block = arg->flags & O_NONBLOCK ? SND_PCM_NONBLOCK : 0;
	int access;
//	block |= SND_PCM_ASYNC;
	/* Woop dee dooo.. I love handling things in SIGIO (PAIN!!)
	 * Probably needed for MMAP
	 */

	if (!hw_params)
		ret = snd_pcm_hw_params_malloc(&hw_params);
	if (ret < 0)
		return ret;

	if (!sw_params)
		ret = snd_pcm_sw_params_malloc(&sw_params);
	if (ret < 0)
		return ret;

	if (pcm[PLAY])
		snd_pcm_close(pcm[PLAY]);
	if (pcm[REC])
		snd_pcm_close(pcm[REC]);
	pcm[REC] = pcm[PLAY] = NULL;

	access = arg->flags & O_ACCMODE;
	if (access == O_WRONLY || access == O_RDWR) {
		ret = snd_pcm_open(&pcm[PLAY], "default",
				   SND_PCM_STREAM_PLAYBACK, block);
		if (ret >= 0)
			ret = set_hw_params(pcm[PLAY]);
	}

	if (ret >= 0 && (access == O_RDONLY || access == O_RDWR)) {
		ret = snd_pcm_open(&pcm[REC], "default",
				   SND_PCM_STREAM_CAPTURE, block);
		if (ret >= 0)
			ret = set_hw_params(pcm[REC]);
	}

	if (ret < 0) {
		if (pcm[PLAY])
			snd_pcm_close(pcm[PLAY]);
		if (pcm[REC])
			snd_pcm_close(pcm[REC]);
		pcm[REC] = pcm[PLAY] = NULL;
		return ret;
	}
	return 0;
}

#define GIOVANNI
#ifdef GIOVANNI

#define GIOVA_SLEEP 40000
#define GIOVA_BLK 3840
static ssize_t alsap_write(enum ossp_opcode opcode,
			   void *carg, void *din, size_t din_sz,
			   void *rarg, void *dout, size_t *dout_szp, int tfd)
{
	usleep((GIOVA_SLEEP/GIOVA_BLK)* din_sz);
	return	din_sz;
}
static ssize_t alsap_read(enum ossp_opcode opcode,
			  void *carg, void *din, size_t din_sz,
			  void *rarg, void *dout, size_t *dout_szp, int tfd)
{
	usleep((GIOVA_SLEEP/GIOVA_BLK)* *dout_szp);
	return	*dout_szp;
}
#else// GIOVANNI
static ssize_t alsap_write(enum ossp_opcode opcode,
			   void *carg, void *din, size_t din_sz,
			   void *rarg, void *dout, size_t *dout_szp, int tfd)
{
//	struct ossp_dsp_rw_arg *arg = carg;
	int ret, insize;

	insize = snd_pcm_bytes_to_frames(pcm[PLAY], din_sz);

	if (snd_pcm_state(pcm[PLAY]) == SND_PCM_STATE_SETUP)
		snd_pcm_prepare(pcm[PLAY]);

//	snd_pcm_start(pcm[PLAY]);
	ret = snd_pcm_writei(pcm[PLAY], din, insize);
	if (ret < 0)
		ret = snd_pcm_recover(pcm[PLAY], ret, 1);

	if (ret >= 0)
		return snd_pcm_frames_to_bytes(pcm[PLAY], ret);
	else
		return ret;
}

static ssize_t alsap_read(enum ossp_opcode opcode,
			  void *carg, void *din, size_t din_sz,
			  void *rarg, void *dout, size_t *dout_szp, int tfd)
{
//	struct ossp_dsp_rw_arg *arg = carg;
	int ret, outsize;

	outsize = snd_pcm_bytes_to_frames(pcm[REC], *dout_szp);

	if (snd_pcm_state(pcm[REC]) == SND_PCM_STATE_SETUP)
		snd_pcm_prepare(pcm[REC]);

	ret = snd_pcm_readi(pcm[REC], dout, outsize);
	if (ret < 0)
		ret = snd_pcm_recover(pcm[REC], ret, 1);
	if (ret >= 0)
		*dout_szp = ret = snd_pcm_frames_to_bytes(pcm[REC], ret);
	else
		*dout_szp = 0;

	return ret;
}
#endif// GIOVANNI

static ssize_t alsap_poll(enum ossp_opcode opcode,
			  void *carg, void *din, size_t din_sz,
			  void *rarg, void *dout, size_t *dout_szp, int tfd)
{
	unsigned revents = 0;

	stream_notify |= *(int *)carg;

	if (pcm[PLAY])
		revents |= POLLOUT;
	if (pcm[REC])
		revents |= POLLIN;

	*(unsigned *)rarg = revents;
	return 0;
}


static ssize_t alsap_flush(enum ossp_opcode opcode,
			   void *carg, void *din, size_t din_sz,
			   void *rarg, void *dout, size_t *dout_szp, int tfd)
{
	flush_streams(opcode == OSSP_DSP_SYNC);
	return 0;
}

static ssize_t alsap_post(enum ossp_opcode opcode,
			  void *carg, void *din, size_t din_sz,
			  void *rarg, void *dout, size_t *dout_szp, int tfd)
{
	int ret;

	ret = trigger_streams(1, 1);
	if (ret >= 0 && pcm[PLAY])
		ret = snd_pcm_start(pcm[PLAY]);
	if (pcm[REC])
		ret = snd_pcm_start(pcm[REC]);
	return ret;
}

static ssize_t alsap_get_param(enum ossp_opcode opcode,
			       void *carg, void *din, size_t din_sz,
			       void *rarg, void *dout, size_t *dout_szp,
			       int tfd)
{
	int v = 0;

	switch (opcode) {
	case OSSP_DSP_GET_RATE:
		return hw_format.rate;

	case OSSP_DSP_GET_CHANNELS:
		return hw_format.channels;

	case OSSP_DSP_GET_FORMAT: {
		v = fmt_alsa_to_oss(hw_format.format);
		break;
	}

	case OSSP_DSP_GET_BLKSIZE: {
		snd_pcm_uframes_t psize;
		snd_pcm_hw_params_get_period_size(hw_params, &psize, NULL);
		v = psize;
		break;
	}

	case OSSP_DSP_GET_FORMATS:
		v = AFMT_U8 | AFMT_A_LAW | AFMT_MU_LAW | AFMT_S16_LE |
			AFMT_S16_BE | AFMT_FLOAT | AFMT_S32_LE | AFMT_S32_BE;
		break;

	case OSSP_DSP_GET_TRIGGER:
		if (!stream_corked[PLAY])
			v |= PCM_ENABLE_OUTPUT;
		if (!stream_corked[REC])
			v |= PCM_ENABLE_INPUT;
		break;

	default:
		assert(0);
	}

	*(int *)rarg = v;

	return 0;
}

static ssize_t alsap_set_param(enum ossp_opcode opcode,
			       void *carg, void *din, size_t din_sz,
			       void *rarg, void *dout, size_t *dout_szp,
			       int tfd)
{
	int v = *(int *)carg;
	int ret = 0;

	/* kill the streams before changing parameters */
	kill_streams();

	switch (opcode) {
	case OSSP_DSP_SET_RATE: {
		hw_format.rate = v;
		break;
	}

	case OSSP_DSP_SET_CHANNELS: {
		hw_format.channels = v;
		break;
	}

	case OSSP_DSP_SET_FORMAT: {
		snd_pcm_format_t format = fmt_oss_to_alsa(v);
		hw_format.format = format;
		break;
	}

	case OSSP_DSP_SET_SUBDIVISION:
		if (!v)
			v = 1;
#if 0
		if (!v) {
			v = user_subdivision ?: 1;
			break;
		}
		user_frag_size = 0;
		user_subdivision = v;
		break;

	case OSSP_DSP_SET_FRAGMENT:
		user_subdivision = 0;
		user_frag_size = 1 << (v & 0xffff);
		user_max_frags = (v >> 16) & 0xffff;
		if (user_frag_size < 4)
			user_frag_size = 4;
		if (user_max_frags < 2)
			user_max_frags = 2;
#else
	case OSSP_DSP_SET_FRAGMENT:
#endif
		break;
	default:
		assert(0);
	}

	if (pcm[PLAY])
		ret = set_hw_params(pcm[PLAY]);
	if (ret >= 0 && pcm[REC])
		ret = set_hw_params(pcm[REC]);

	if (rarg)
		*(int *)rarg = v;
	return 0;
}

static ssize_t alsap_set_trigger(enum ossp_opcode opcode,
				 void *carg, void *din, size_t din_sz,
				 void *rarg, void *dout, size_t *dout_szp,
				 int fd)
{
	int enable = *(int *)carg;

	stream_corked[PLAY] = !!(enable & PCM_ENABLE_OUTPUT);
	stream_corked[REC] = !!(enable & PCM_ENABLE_INPUT);

	return trigger_streams(enable & PCM_ENABLE_OUTPUT,
			       enable & PCM_ENABLE_INPUT);
}

static ssize_t alsap_get_space(enum ossp_opcode opcode,
			       void *carg, void *din, size_t din_sz,
			       void *rarg, void *dout, size_t *dout_szp, int tfd)
{
	int dir = (opcode == OSSP_DSP_GET_OSPACE) ? PLAY : REC;
	int underrun = 0;
	struct audio_buf_info info = { };
	unsigned long bufsize;
	snd_pcm_uframes_t avail, fragsize;
	snd_pcm_state_t state;

	if (!pcm[dir])
		return -EINVAL;

	state = snd_pcm_state(pcm[dir]);
	if (state == SND_PCM_STATE_XRUN) {
		snd_pcm_recover(pcm[dir], -EPIPE, 0);
		underrun = 1;
	} else if (state == SND_PCM_STATE_SUSPENDED) {
		snd_pcm_recover(pcm[dir], -ESTRPIPE, 0);
		underrun = 1;
	}

	snd_pcm_hw_params_current(pcm[dir], hw_params);
	snd_pcm_hw_params_get_period_size(hw_params, &fragsize, NULL);
	snd_pcm_hw_params_get_buffer_size(hw_params, &bufsize);
	info.fragsize = snd_pcm_frames_to_bytes(pcm[dir], fragsize);
	info.fragstotal = bufsize / fragsize;
	if (!underrun) {
		avail = snd_pcm_avail_update(pcm[dir]);
		info.fragments = avail / fragsize;
	} else
		info.fragments = info.fragstotal;

	info.bytes = info.fragsize * info.fragments;

	*(struct audio_buf_info *)rarg = info;
	return 0;
}

static ssize_t alsap_get_ptr(enum ossp_opcode opcode,
			     void *carg, void *din, size_t din_sz,
			     void *rarg, void *dout, size_t *dout_szp, int tfd)
{
	int dir = (opcode == OSSP_DSP_GET_OPTR) ? PLAY : REC;
	struct count_info info = { };

	if (!pcm[dir])
		return -EIO;

	snd_pcm_hw_params_current(pcm[dir], hw_params);
	info.bytes = byte_counter[dir];
	snd_pcm_hw_params_get_periods(hw_params, (unsigned int *)&info.blocks, NULL);
	info.ptr = mmap_pos[dir];

	*(struct count_info *)rarg = info;
	return 0;
}

static ssize_t alsap_get_odelay(enum ossp_opcode opcode,
				void *carg, void *din, size_t din_sz,
				void *rarg, void *dout, size_t *dout_szp,
				int fd)
{
	snd_pcm_sframes_t delay;

	if (!pcm[PLAY])
		return -EIO;

	if (snd_pcm_delay(pcm[PLAY], &delay) < 0)
		return -EIO;

	*(int *)rarg = snd_pcm_frames_to_bytes(pcm[PLAY], delay);
	return 0;
}

static ossp_action_fn_t action_fn_tbl[OSSP_NR_OPCODES] = {
	[OSSP_MIXER]		= alsap_mixer,
	[OSSP_DSP_OPEN]		= alsap_open,
	[OSSP_DSP_READ]		= alsap_read,
	[OSSP_DSP_WRITE]	= alsap_write,
	[OSSP_DSP_POLL]		= alsap_poll,
#if 0
	[OSSP_DSP_MMAP]		= alsap_mmap,
	[OSSP_DSP_MUNMAP]	= alsap_munmap,
#endif
	[OSSP_DSP_RESET]	= alsap_flush,
	[OSSP_DSP_SYNC]		= alsap_flush,
	[OSSP_DSP_POST]		= alsap_post,
	[OSSP_DSP_GET_RATE]	= alsap_get_param,
	[OSSP_DSP_GET_CHANNELS]	= alsap_get_param,
	[OSSP_DSP_GET_FORMAT]	= alsap_get_param,
	[OSSP_DSP_GET_BLKSIZE]	= alsap_get_param,
	[OSSP_DSP_GET_FORMATS]	= alsap_get_param,
	[OSSP_DSP_SET_RATE]	= alsap_set_param,
	[OSSP_DSP_SET_CHANNELS]	= alsap_set_param,
	[OSSP_DSP_SET_FORMAT]	= alsap_set_param,
	[OSSP_DSP_SET_SUBDIVISION] = alsap_set_param,
	[OSSP_DSP_SET_FRAGMENT]	= alsap_set_param,
	[OSSP_DSP_GET_TRIGGER]	= alsap_get_param,
	[OSSP_DSP_SET_TRIGGER]	= alsap_set_trigger,
	[OSSP_DSP_GET_OSPACE]	= alsap_get_space,
	[OSSP_DSP_GET_ISPACE]	= alsap_get_space,
	[OSSP_DSP_GET_OPTR]	= alsap_get_ptr,
	[OSSP_DSP_GET_IPTR]	= alsap_get_ptr,
	[OSSP_DSP_GET_ODELAY]	= alsap_get_odelay,
};

static int action_pre(void)
{
	return 0;
}

static void action_post(void)
{
}

int main(int argc, char **argv)
{
	int rc;

	ossp_slave_init(argc, argv);

	page_size = sysconf(_SC_PAGE_SIZE);

	/* Okay, now we're open for business */
	rc = 0;
	do {
		rc = ossp_slave_process_command(ossp_cmd_fd, action_fn_tbl,
						action_pre, action_post);
	} while (rc > 0);

	return rc ? 1 : 0;
}
