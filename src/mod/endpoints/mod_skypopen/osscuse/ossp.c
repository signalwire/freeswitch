/*
 * ossp - OSS Proxy: emulate OSS device using CUSE
 *
 * Copyright (C) 2008-2010  SUSE Linux Products GmbH
 * Copyright (C) 2008-2010  Tejun Heo <tj@kernel.org>
 *
 * This file is released under the GPLv2.
 */

#include "ossp.h"

const struct ossp_arg_size ossp_arg_sizes[OSSP_NR_OPCODES] = {
	[OSSP_MIXER]		= { sizeof(struct ossp_mixer_arg),
				    sizeof(struct ossp_mixer_arg), 0 },

	[OSSP_DSP_OPEN]		= { sizeof(struct ossp_dsp_open_arg), 0, 0 },
	[OSSP_DSP_READ]		= { sizeof(struct ossp_dsp_rw_arg), 0, 0 },
	[OSSP_DSP_WRITE]	= { sizeof(struct ossp_dsp_rw_arg), 0, 0 },
	[OSSP_DSP_POLL]		= { sizeof(int), sizeof(unsigned), 0 },
	[OSSP_DSP_MMAP]		= { sizeof(struct ossp_dsp_mmap_arg), 0, 0 },
	[OSSP_DSP_MUNMAP]	= { sizeof(int), 0, 0 },

	[OSSP_DSP_RESET]	= { 0, 0, 0 },
	[OSSP_DSP_SYNC]		= { 0, 0, 0 },
	[OSSP_DSP_POST]		= { 0, 0, 0 },
	[OSSP_DSP_GET_RATE]	= { 0, sizeof(int), 0 },
	[OSSP_DSP_GET_CHANNELS]	= { 0, sizeof(int), 0 },
	[OSSP_DSP_GET_FORMAT]	= { 0, sizeof(int), 0 },
	[OSSP_DSP_GET_BLKSIZE]	= { 0, sizeof(int), 0 },
	[OSSP_DSP_GET_FORMATS]	= { 0, sizeof(int), 0 },
	[OSSP_DSP_SET_RATE]	= { sizeof(int), sizeof(int), 0 },
	[OSSP_DSP_SET_CHANNELS]	= { sizeof(int), sizeof(int), 0 },
	[OSSP_DSP_SET_FORMAT]	= { sizeof(int), sizeof(int), 0 },
	[OSSP_DSP_SET_SUBDIVISION] = { sizeof(int), sizeof(int), 0 },
	[OSSP_DSP_SET_FRAGMENT]	= { sizeof(int), 0, 0 },
	[OSSP_DSP_GET_TRIGGER]	= { 0, sizeof(int), 0 },
	[OSSP_DSP_SET_TRIGGER]	= { sizeof(int), 0, 0 },
	[OSSP_DSP_GET_OSPACE]	= { 0, sizeof(struct audio_buf_info), 0 },
	[OSSP_DSP_GET_ISPACE]	= { 0, sizeof(struct audio_buf_info), 0 },
	[OSSP_DSP_GET_OPTR]	= { 0, sizeof(struct count_info), 0 },
	[OSSP_DSP_GET_IPTR]	= { 0, sizeof(struct count_info), 0 },
	[OSSP_DSP_GET_ODELAY]	= { 0, sizeof(int), 0 },
};

const char *ossp_cmd_str[OSSP_NR_OPCODES] = {
	[OSSP_MIXER]		= "MIXER",

	[OSSP_DSP_OPEN]		= "OPEN",
	[OSSP_DSP_READ]		= "READ",
	[OSSP_DSP_WRITE]	= "WRITE",
	[OSSP_DSP_POLL]		= "POLL",
	[OSSP_DSP_MMAP]		= "MMAP",
	[OSSP_DSP_MUNMAP]	= "MUNMAP",

	[OSSP_DSP_RESET]	= "RESET",
	[OSSP_DSP_SYNC]		= "SYNC",
	[OSSP_DSP_POST]		= "POST",

	[OSSP_DSP_GET_RATE]	= "GET_RATE",
	[OSSP_DSP_GET_CHANNELS]	= "GET_CHANNELS",
	[OSSP_DSP_GET_FORMAT]	= "GET_FORMAT",
	[OSSP_DSP_GET_BLKSIZE]	= "GET_BLKSIZE",
	[OSSP_DSP_GET_FORMATS]	= "GET_FORMATS",
	[OSSP_DSP_SET_RATE]	= "SET_RATE",
	[OSSP_DSP_SET_CHANNELS]	= "SET_CHANNELS",
	[OSSP_DSP_SET_FORMAT]	= "SET_FORMAT",
	[OSSP_DSP_SET_SUBDIVISION] = "SET_BUSDIVISION",

	[OSSP_DSP_SET_FRAGMENT]	= "SET_FRAGMENT",
	[OSSP_DSP_GET_TRIGGER]	= "GET_TRIGGER",
	[OSSP_DSP_SET_TRIGGER]	= "SET_TRIGGER",
	[OSSP_DSP_GET_OSPACE]	= "GET_OSPACE",
	[OSSP_DSP_GET_ISPACE]	= "GET_ISPACE",
	[OSSP_DSP_GET_OPTR]	= "GET_OPTR",
	[OSSP_DSP_GET_IPTR]	= "GET_IPTR",
	[OSSP_DSP_GET_ODELAY]	= "GET_ODELAY",
};

const char *ossp_notify_str[OSSP_NR_NOTIFY_OPCODES] = {
	[OSSP_NOTIFY_POLL]	= "POLL",
	[OSSP_NOTIFY_OBITUARY]	= "OBITUARY",
	[OSSP_NOTIFY_VOLCHG]	= "VOLCHG",
};
