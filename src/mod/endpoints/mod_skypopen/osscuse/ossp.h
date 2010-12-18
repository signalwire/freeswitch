/*
 * ossp - OSS Proxy: emulate OSS device using CUSE
 *
 * Copyright (C) 2008-2010  SUSE Linux Products GmbH
 * Copyright (C) 2008-2010  Tejun Heo <tj@kernel.org>
 *
 * This file is released under the GPLv2.
 */

#ifndef _OSSP_H
#define _OSSP_H

#include <sys/types.h>
#include <inttypes.h>
#include <sys/soundcard.h>

#define OSSP_VERSION		"1.3.2"
#define OSSP_CMD_MAGIC		0xdeadbeef
#define OSSP_REPLY_MAGIC	0xbeefdead
#define OSSP_NOTIFY_MAGIC	0xbebebebe

#define PLAY			0
#define REC			1
#define LEFT			0
#define RIGHT			1

enum ossp_opcode {
	OSSP_MIXER,

	OSSP_DSP_OPEN,
	OSSP_DSP_READ,
	OSSP_DSP_WRITE,
	OSSP_DSP_POLL,
	OSSP_DSP_MMAP,
	OSSP_DSP_MUNMAP,

	OSSP_DSP_RESET,
	OSSP_DSP_SYNC,
	OSSP_DSP_POST,

	OSSP_DSP_GET_RATE,
	OSSP_DSP_GET_CHANNELS,
	OSSP_DSP_GET_FORMAT,
	OSSP_DSP_GET_BLKSIZE,
	OSSP_DSP_GET_FORMATS,
	OSSP_DSP_SET_RATE,
	OSSP_DSP_SET_CHANNELS,
	OSSP_DSP_SET_FORMAT,
	OSSP_DSP_SET_SUBDIVISION,

	OSSP_DSP_SET_FRAGMENT,
	OSSP_DSP_GET_TRIGGER,
	OSSP_DSP_SET_TRIGGER,
	OSSP_DSP_GET_OSPACE,
	OSSP_DSP_GET_ISPACE,
	OSSP_DSP_GET_OPTR,
	OSSP_DSP_GET_IPTR,
	OSSP_DSP_GET_ODELAY,

	OSSP_NR_OPCODES,
};

enum ossp_notify_opcode {
	OSSP_NOTIFY_POLL,
	OSSP_NOTIFY_OBITUARY,
	OSSP_NOTIFY_VOLCHG,

	OSSP_NR_NOTIFY_OPCODES,
};

struct ossp_mixer_arg {
	int			vol[2][2];
};

struct ossp_dsp_open_arg {
	int			flags;
	pid_t			opener_pid;
};

struct ossp_dsp_rw_arg {
	unsigned		nonblock:1;
};

struct ossp_dsp_mmap_arg {
	int			dir;
	size_t			size;
};

struct ossp_cmd {
	unsigned		magic;
	enum ossp_opcode	opcode;
	size_t			din_size;
	size_t			dout_size;
};

struct ossp_reply {
	unsigned		magic;
	int			result;
	size_t			dout_size;	/* <= cmd.data_in_size */
};

struct ossp_notify {
	unsigned		magic;
	enum ossp_notify_opcode opcode;
};

struct ossp_arg_size {
	ssize_t			carg_size;
	ssize_t			rarg_size;
	unsigned		has_fd:1;
};

extern const struct ossp_arg_size ossp_arg_sizes[OSSP_NR_OPCODES];
extern const char *ossp_cmd_str[OSSP_NR_OPCODES];
extern const char *ossp_notify_str[OSSP_NR_NOTIFY_OPCODES];

#endif /* _OSSP_H */
