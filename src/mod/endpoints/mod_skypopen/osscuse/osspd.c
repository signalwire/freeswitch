/*
 * osspd - OSS Proxy Daemon: emulate OSS device using CUSE
 *
 * Copyright (C) 2008-2010  SUSE Linux Products GmbH
 * Copyright (C) 2008-2010  Tejun Heo <tj@kernel.org>
 *
 * This file is released under the GPLv2.
 */
#undef GIOVANNI

#define FUSE_USE_VERSION 28
#define _GNU_SOURCE

#include <assert.h>
#include <cuse_lowlevel.h>
#include <fcntl.h>
#include <fuse_opt.h>
#include <libgen.h>
#include <limits.h>
#include <pthread.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/soundcard.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ossp.h"
#include "ossp-util.h"

/*
 * MMAP support needs to be updated to the new fuse MMAP API.  Disable
 * it for the time being.
 */
#warning mmap support disabled for now
/* #define OSSP_MMAP */

#define DFL_MIXER_NAME		"mixer"
#define DFL_DSP_NAME		"dsp"
#define DFL_ADSP_NAME		"adsp"
#define STRFMT			"S[%u/%d]"
#define STRID(os)		os->id, os->pid

#define dbg1_os(os, fmt, args...)	dbg1(STRFMT" "fmt, STRID(os) , ##args)
#define dbg0_os(os, fmt, args...)	dbg0(STRFMT" "fmt, STRID(os) , ##args)
#define warn_os(os, fmt, args...)	warn(STRFMT" "fmt, STRID(os) , ##args)
#define err_os(os, fmt, args...)	err(STRFMT" "fmt, STRID(os) , ##args)
#define warn_ose(os, err, fmt, args...)	\
	warn_e(err, STRFMT" "fmt, STRID(os) , ##args)
#define err_ose(os, err, fmt, args...)	\
	err_e(err, STRFMT" "fmt, STRID(os) , ##args)

enum {
	SNDRV_OSS_VERSION	= ((3<<16)|(8<<8)|(1<<4)|(0)),	/* 3.8.1a */
	DFL_MIXER_MAJOR		= 14,
	DFL_MIXER_MINOR		= 0,
	DFL_DSP_MAJOR		= 14,
	DFL_DSP_MINOR		= 3,
	DFL_ADSP_MAJOR		= 14,
	DFL_ADSP_MINOR		= 12,
	DFL_MAX_STREAMS		= 128,
	MIXER_PUT_DELAY		= 600,			/* 10 mins */
	/* DSPS_MMAP_SIZE / 2 must be multiple of SHMLBA */
	DSPS_MMAP_SIZE		= 2 * (512 << 10),	/* 512k for each dir */
};

struct ossp_uid_cnt {
	struct list_head	link;
	uid_t			uid;
	unsigned		nr_os;
};

struct ossp_mixer {
	pid_t			pgrp;
	struct list_head	link;
	struct list_head	delayed_put_link;
	unsigned		refcnt;
	/* the following two fields are protected by mixer_mutex */
	int			vol[2][2];
	int			modify_counter;
	time_t			put_expires;
};

struct ossp_mixer_cmd {
	struct ossp_mixer	*mixer;
	struct ossp_mixer_arg	set;
	int			out_dir;
	int			rvol;
};

#define for_each_vol(i, j)						\
	for (i = 0, j = 0; i < 2; j += i << 1, j++, i = j >> 1, j &= 1)

struct ossp_stream {
	unsigned		id;	/* stream ID */
	struct list_head	link;
	struct list_head	pgrp_link;
	struct list_head	notify_link;
	unsigned		refcnt;
	pthread_mutex_t		cmd_mutex;
	pthread_mutex_t		mmap_mutex;
	struct fuse_pollhandle	*ph;

	/* stream owner info */
	pid_t			pid;
	pid_t			pgrp;
	uid_t			uid;
	gid_t			gid;

	/* slave info */
	pid_t			slave_pid;
	int			cmd_fd;
	int			notify_tx;
	int			notify_rx;

	/* the following dead flag is set asynchronously, keep it separate. */
	int			dead;

	/* stream mixer state, protected by mixer_mutex */
	int			mixer_pending;
	int			vol[2][2];
	int			vol_set[2][2];

	off_t			mmap_off;
	size_t			mmap_size;

	struct ossp_uid_cnt	*ucnt;
	struct fuse_session	*se;	/* associated fuse session */
	struct ossp_mixer	*mixer;
};

struct ossp_dsp_stream {
	struct ossp_stream	os;
	unsigned		rw;
	unsigned		mmapped;
	int			nonblock;
};

#define os_to_dsps(_os)		container_of(_os, struct ossp_dsp_stream, os)

static unsigned max_streams;
static unsigned umax_streams;
static unsigned hashtbl_size;
static char dsp_slave_path[PATH_MAX];

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mixer_mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned long *os_id_bitmap;
static unsigned nr_mixers;
static struct list_head *mixer_tbl;	/* indexed by PGRP */
static struct list_head *os_tbl;	/* indexed by ID */
static struct list_head *os_pgrp_tbl;	/* indexed by PGRP */
static struct list_head *os_notify_tbl;	/* indexed by notify fd */
static LIST_HEAD(uid_cnt_list);
static int notify_epfd;			/* epoll used to monitor notify fds */
static pthread_t notify_poller_thread;
static pthread_t slave_reaper_thread;
static pthread_t mixer_delayed_put_thread;
static pthread_t cuse_mixer_thread;
static pthread_t cuse_adsp_thread;
static pthread_cond_t notify_poller_kill_wait = PTHREAD_COND_INITIALIZER;
static pthread_cond_t slave_reaper_wait = PTHREAD_COND_INITIALIZER;
static LIST_HEAD(slave_corpse_list);
static LIST_HEAD(mixer_delayed_put_head); /* delayed reference */
static pthread_cond_t mixer_delayed_put_cond = PTHREAD_COND_INITIALIZER;

static int init_wait_fd = -1;
static int exit_on_idle;
static struct fuse_session *mixer_se;
static struct fuse_session *dsp_se;
static struct fuse_session *adsp_se;

static void put_os(struct ossp_stream *os);


/***************************************************************************
 * Accessors
 */

static struct list_head *mixer_tbl_head(pid_t pid)
{
	return &mixer_tbl[pid % hashtbl_size];
}

static struct list_head *os_tbl_head(uint64_t id)
{
	return &os_tbl[id % hashtbl_size];
}

static struct list_head *os_pgrp_tbl_head(pid_t pgrp)
{
	return &os_pgrp_tbl[pgrp % hashtbl_size];
}

static struct list_head *os_notify_tbl_head(int notify_rx)
{
	return &os_notify_tbl[notify_rx % hashtbl_size];
}

static struct ossp_mixer *find_mixer_locked(pid_t pgrp)
{
	struct ossp_mixer *mixer;

	list_for_each_entry(mixer, mixer_tbl_head(pgrp), link)
		if (mixer->pgrp == pgrp)
			return mixer;
	return NULL;
}

static struct ossp_mixer *find_mixer(pid_t pgrp)
{
	struct ossp_mixer *mixer;

	pthread_mutex_lock(&mutex);
	mixer = find_mixer_locked(pgrp);
	pthread_mutex_unlock(&mutex);
	return mixer;
}

static struct ossp_stream *find_os(unsigned id)
{
	struct ossp_stream *os, *found = NULL;

	pthread_mutex_lock(&mutex);
	list_for_each_entry(os, os_tbl_head(id), link)
		if (os->id == id) {
			found = os;
			break;
		}
	pthread_mutex_unlock(&mutex);
	return found;
}

static struct ossp_stream *find_os_by_notify_rx(int notify_rx)
{
	struct ossp_stream *os, *found = NULL;

	pthread_mutex_lock(&mutex);
	list_for_each_entry(os, os_notify_tbl_head(notify_rx), notify_link)
		if (os->notify_rx == notify_rx) {
			found = os;
			break;
		}
	pthread_mutex_unlock(&mutex);
	return found;
}


/***************************************************************************
 * Command and ioctl helpers
 */

static ssize_t exec_cmd_intern(struct ossp_stream *os, enum ossp_opcode opcode,
	const void *carg, size_t carg_size, const void *din, size_t din_size,
	void *rarg, size_t rarg_size, void *dout, size_t *dout_sizep, int fd)
{
	size_t dout_size = dout_sizep ? *dout_sizep : 0;
	struct ossp_cmd cmd = { .magic = OSSP_CMD_MAGIC, .opcode = opcode,
				 .din_size = din_size,
				 .dout_size = dout_size };
	struct iovec iov = { &cmd, sizeof(cmd) };
	struct msghdr msg = { .msg_iov = &iov, .msg_iovlen = 1 };
	struct ossp_reply reply = { };
	char cmsg_buf[CMSG_SPACE(sizeof(fd))];
	char reason[512];
	int rc;

	if (os->dead)
		return -EIO;

	//dbg1_os(os, "opcode %s=%d carg=%zu din=%zu rarg=%zu dout=%zu",
		//ossp_cmd_str[opcode], opcode, carg_size, din_size, rarg_size,
		//dout_size);
#ifndef GIOVANNI
memset(dout, 255, dout_size);
memset(din, 255, din_size);

#define GIOVA_BLK 3840
#define GIOVA_SLEEP 40000
switch(opcode){

	case 1: //OPEN
		reply.result = 0;
		break;
	case 2: //READ
		usleep((GIOVA_SLEEP/GIOVA_BLK)* *dout_sizep);
		reply.result = *dout_sizep;
		break;
	case 3: //WRITE
		usleep((GIOVA_SLEEP/GIOVA_BLK)* din_size);
		reply.result = din_size;
		break;
	case 9: //POST
		reply.result = -32;
		break;
	case 13: //GET_BLKSIZE
		reply.result = 0;
		*(int *)rarg = GIOVA_BLK;
		break;
	case 14: //GET_FORMATS
		reply.result = 0;
		*(int *)rarg = 28731;
		break;
	case 15: //SET_RATE
		reply.result = 0;
		*(int *)rarg = *(int *) carg;
		break;
	case 16: //SET_CHANNELS
		reply.result = 0;
		*(int *)rarg = *(int *) carg;
		break;
	case 17: //SET_FORMAT
		reply.result = 0;
		*(int *)rarg = *(int *) carg;
		break;
	case 19: //SET_FRAGMENT
		reply.result = 0;
		break;
	default: 
		reply.result = 0;
		break;
}
#endif // GIOVANNI

#ifdef GIOVANNI
	if (fd >= 0) {
		struct cmsghdr *cmsg;

		msg.msg_control = cmsg_buf;
		msg.msg_controllen = sizeof(cmsg_buf);
		cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
		*(int *)CMSG_DATA(cmsg) = fd;
		msg.msg_controllen = cmsg->cmsg_len;
	}

	if (sendmsg(os->cmd_fd, &msg, 0) <= 0) {
		rc = -errno;
		snprintf(reason, sizeof(reason), "command sendmsg failed: %s",
			 strerror(-rc));
		goto fail;
	}

	if ((rc = write_fill(os->cmd_fd, carg, carg_size)) < 0 ||
	    (rc = write_fill(os->cmd_fd, din, din_size)) < 0) {
		snprintf(reason, sizeof(reason),
			 "can't tranfer command argument and/or data: %s",
			 strerror(-rc));
		goto fail;
	}
	if ((rc = read_fill(os->cmd_fd, &reply, sizeof(reply))) < 0) {
		snprintf(reason, sizeof(reason), "can't read reply: %s",
			 strerror(-rc));
		goto fail;
	}

	if (reply.magic != OSSP_REPLY_MAGIC) {
		snprintf(reason, sizeof(reason),
			 "reply magic mismatch %x != %x",
			 reply.magic, OSSP_REPLY_MAGIC);
		rc = -EINVAL;
		goto fail;
	}

	if (reply.result < 0)
		goto out_unlock;

	if (reply.dout_size > dout_size) {
		snprintf(reason, sizeof(reason),
			 "data out size overflow %zu > %zu",
			 reply.dout_size, dout_size);
		rc = -EINVAL;
		goto fail;
	}

	dout_size = reply.dout_size;
	if (dout_sizep)
		*dout_sizep = dout_size;

	if ((rc = read_fill(os->cmd_fd, rarg, rarg_size)) < 0 ||
	    (rc = read_fill(os->cmd_fd, dout, dout_size)) < 0) {
		snprintf(reason, sizeof(reason), "can't read data out: %s",
			 strerror(-rc));
		goto fail;
	}

#endif // GIOVANNI

out_unlock:
	//dbg1_os(os, "  completed, result=%d dout=%zu",
		//reply.result, dout_size);

//if(rarg)
	//dbg1_os(os, "  2 %s=%d completed, result=%d dout=%zu carg=%d rarg=%d", ossp_cmd_str[opcode], opcode,
		//reply.result, dout_size, carg ? *(int *) carg : 666, *(int *)rarg);
	return reply.result;

fail:
	warn_os(os, "communication with slave failed (%s)", reason);
	os->dead = 1;
	return rc;
}

static ssize_t exec_cmd(struct ossp_stream *os, enum ossp_opcode opcode,
	const void *carg, size_t carg_size, const void *din, size_t din_size,
	void *rarg, size_t rarg_size, void *dout, size_t *dout_sizep, int fd)
{
	int is_mixer;
	int i, j;
	ssize_t ret, mret;

	/* mixer command is handled exlicitly below */
	is_mixer = opcode == OSSP_MIXER;
	if (is_mixer) {
		ret = -pthread_mutex_trylock(&os->cmd_mutex);
		if (ret)
			return ret;
	} else {
		pthread_mutex_lock(&os->cmd_mutex);

		ret = exec_cmd_intern(os, opcode, carg, carg_size,
				      din, din_size, rarg, rarg_size,
				      dout, dout_sizep, fd);
	}

	/* lazy mixer handling */
	pthread_mutex_lock(&mixer_mutex);

	if (os->mixer_pending) {
		struct ossp_mixer_arg marg;
	repeat_mixer:
		/* we have mixer command pending */
		memcpy(marg.vol, os->vol_set, sizeof(os->vol_set));
		memset(os->vol_set, -1, sizeof(os->vol_set));

		pthread_mutex_unlock(&mixer_mutex);
		mret = exec_cmd_intern(os, OSSP_MIXER, &marg, sizeof(marg),
				       NULL, 0, &marg, sizeof(marg), NULL, NULL,
				       -1);
		pthread_mutex_lock(&mixer_mutex);

		/* was there mixer set request while executing mixer command? */
		for_each_vol(i, j)
			if (os->vol_set[i][j] >= 0)
				goto repeat_mixer;

		/* update internal mixer state */
		if (mret == 0) {
			for_each_vol(i, j) {
				if (marg.vol[i][j] >= 0) {
					if (os->vol[i][j] != marg.vol[i][j])
						os->mixer->modify_counter++;
					os->vol[i][j] = marg.vol[i][j];
				}
			}
		}
		os->mixer_pending = 0;
	}

	pthread_mutex_unlock(&os->cmd_mutex);

	/*
	 * mixer mutex must be released after cmd_mutex so that
	 * exec_mixer_cmd() can guarantee that mixer_pending flags
	 * will be handled immediately or when the currently
	 * in-progress command completes.
	 */
	pthread_mutex_unlock(&mixer_mutex);

	return is_mixer ? mret : ret;
}

static ssize_t exec_simple_cmd(struct ossp_stream *os,
			       enum ossp_opcode opcode, void *carg, void *rarg)
{
	return exec_cmd(os, opcode,
			carg, ossp_arg_sizes[opcode].carg_size, NULL, 0,
			rarg, ossp_arg_sizes[opcode].rarg_size, NULL, NULL, -1);
}

static int ioctl_prep_uarg(fuse_req_t req, void *in, size_t in_sz, void *out,
			   size_t out_sz, void *uarg, const void *in_buf,
			   size_t in_bufsz, size_t out_bufsz)
{
	struct iovec in_iov = { }, out_iov = { };
	int retry = 0;

	if (in) {
		if (!in_bufsz) {
			in_iov.iov_base = uarg;
			in_iov.iov_len = in_sz;
			retry = 1;
		} else {
			assert(in_bufsz == in_sz);
			memcpy(in, in_buf, in_sz);
		}
	}

	if (out) {
		if (!out_bufsz) {
			out_iov.iov_base = uarg;
			out_iov.iov_len = out_sz;
			retry = 1;
		} else
			assert(out_bufsz == out_sz);
	}

	if (retry)
		fuse_reply_ioctl_retry(req, &in_iov, 1, &out_iov, 1);

	return retry;
}

#define PREP_UARG(inp, outp) do {					\
	if (ioctl_prep_uarg(req, (inp), sizeof(*(inp)),			\
			    (outp), sizeof(*(outp)), uarg,		\
			    in_buf, in_bufsz, out_bufsz))		\
		return;							\
} while (0)

#define IOCTL_RETURN(result, outp) do {					\
	if ((outp) != NULL)						\
		fuse_reply_ioctl(req, result, (outp), sizeof(*(outp)));	\
	else								\
		fuse_reply_ioctl(req, result, NULL, 0);			\
	return;								\
} while (0)


/***************************************************************************
 * Mixer implementation
 */

static void put_mixer_real(struct ossp_mixer *mixer)
{
	if (!--mixer->refcnt) {
		dbg0("DESTROY mixer(%d)", mixer->pgrp);
		list_del_init(&mixer->link);
		list_del_init(&mixer->delayed_put_link);
		free(mixer);
		nr_mixers--;

		/*
		 * If exit_on_idle, mixer for pgrp0 is touched during
		 * init and each stream has mixer attached.  As mixers
		 * are destroyed after they have been idle for
		 * MIXER_PUT_DELAY seconds, we can use it for idle
		 * detection.  Note that this might race with
		 * concurrent open.  The race is inherent.
		 */
		if (exit_on_idle && !nr_mixers) {
			info("idle, exiting");
			exit(0);
		}
	}
}

static struct ossp_mixer *get_mixer(pid_t pgrp)
{
	struct ossp_mixer *mixer;

	pthread_mutex_lock(&mutex);

	/* is there a matching one? */
	mixer = find_mixer_locked(pgrp);
	if (mixer) {
		if (list_empty(&mixer->delayed_put_link))
			mixer->refcnt++;
		else
			list_del_init(&mixer->delayed_put_link);
		goto out_unlock;
	}

	/* reap delayed put list if there are too many mixers */
	while (nr_mixers > 2 * max_streams &&
	       !list_empty(&mixer_delayed_put_head)) {
		struct ossp_mixer *mixer =
			list_first_entry(&mixer_delayed_put_head,
					 struct ossp_mixer, delayed_put_link);

		assert(mixer->refcnt == 1);
		put_mixer_real(mixer);
	}

	/* create a new one */
	mixer = calloc(1, sizeof(*mixer));
	if (!mixer) {
		warn("failed to allocate mixer for %d", pgrp);
		mixer = NULL;
		goto out_unlock;
	}

	mixer->pgrp = pgrp;
	INIT_LIST_HEAD(&mixer->link);
	INIT_LIST_HEAD(&mixer->delayed_put_link);
	mixer->refcnt = 1;
	memset(mixer->vol, -1, sizeof(mixer->vol));

	list_add(&mixer->link, mixer_tbl_head(pgrp));
	nr_mixers++;
	dbg0("CREATE mixer(%d)", pgrp);

out_unlock:
	pthread_mutex_unlock(&mutex);
	return mixer;
}

static void put_mixer(struct ossp_mixer *mixer)
{
	pthread_mutex_lock(&mutex);

	if (mixer) {
		if (mixer->refcnt == 1) {
			struct timespec ts;

			clock_gettime(CLOCK_REALTIME, &ts);
			mixer->put_expires = ts.tv_sec + MIXER_PUT_DELAY;
			list_add_tail(&mixer->delayed_put_link,
				      &mixer_delayed_put_head);
			pthread_cond_signal(&mixer_delayed_put_cond);
		} else
			put_mixer_real(mixer);
	}

	pthread_mutex_unlock(&mutex);
}

static void *mixer_delayed_put_worker(void *arg)
{
	struct ossp_mixer *mixer;
	struct timespec ts;
	time_t now;

	pthread_mutex_lock(&mutex);
again:
	clock_gettime(CLOCK_REALTIME, &ts);
	now = ts.tv_sec;

	mixer = NULL;
	while (!list_empty(&mixer_delayed_put_head)) {
		mixer = list_first_entry(&mixer_delayed_put_head,
					 struct ossp_mixer, delayed_put_link);

		if (now <= mixer->put_expires)
			break;

		assert(mixer->refcnt == 1);
		put_mixer_real(mixer);
		mixer = NULL;
	}

	if (mixer) {
		ts.tv_sec = mixer->put_expires + 1;
		pthread_cond_timedwait(&mixer_delayed_put_cond, &mutex, &ts);
	} else
		pthread_cond_wait(&mixer_delayed_put_cond, &mutex);

	goto again;
}

static void init_mixer_cmd(struct ossp_mixer_cmd *mxcmd,
			   struct ossp_mixer *mixer)
{
	memset(mxcmd, 0, sizeof(*mxcmd));
	memset(&mxcmd->set.vol, -1, sizeof(mxcmd->set.vol));
	mxcmd->mixer = mixer;
	mxcmd->out_dir = -1;
}

static int exec_mixer_cmd(struct ossp_mixer_cmd *mxcmd, struct ossp_stream *os)
{
	int i, j, rc;

	/*
	 * Set pending flags before trying to execute mixer command.
	 * Combined with lock release order in exec_cmd(), this
	 * guarantees that the mixer command will be executed
	 * immediately or when the current command completes.
	 */
	pthread_mutex_lock(&mixer_mutex);
	os->mixer_pending = 1;
	for_each_vol(i, j)
		if (mxcmd->set.vol[i][j] >= 0)
			os->vol_set[i][j] = mxcmd->set.vol[i][j];
	pthread_mutex_unlock(&mixer_mutex);

	rc = exec_simple_cmd(os, OSSP_MIXER, NULL, NULL);
	if (rc >= 0) {
		dbg0_os(os, "volume set=%d/%d:%d/%d get=%d/%d:%d/%d",
			mxcmd->set.vol[PLAY][LEFT], mxcmd->set.vol[PLAY][RIGHT],
			mxcmd->set.vol[REC][LEFT], mxcmd->set.vol[REC][RIGHT],
			os->vol[PLAY][LEFT], os->vol[PLAY][RIGHT],
			os->vol[REC][LEFT], os->vol[REC][RIGHT]);
	} else if (rc != -EBUSY)
		warn_ose(os, rc, "mixer command failed");

	return rc;
}

static void finish_mixer_cmd(struct ossp_mixer_cmd *mxcmd)
{
	struct ossp_mixer *mixer = mxcmd->mixer;
	struct ossp_stream *os;
	int dir = mxcmd->out_dir;
	int vol[2][2] = { };
	int cnt[2][2] = { };
	int i, j;

	pthread_mutex_lock(&mixer_mutex);

	/* get volume of all streams attached to this mixer */
	pthread_mutex_lock(&mutex);
	list_for_each_entry(os, os_pgrp_tbl_head(mixer->pgrp), pgrp_link) {
		if (os->pgrp != mixer->pgrp)
			continue;
		for_each_vol(i, j) {
			if (os->vol[i][j] < 0)
				continue;
			vol[i][j] += os->vol[i][j];
			cnt[i][j]++;
		}
	}
	pthread_mutex_unlock(&mutex);

	/* calculate the summary volume values */
	for_each_vol(i, j) {
		if (mxcmd->set.vol[i][j] >= 0)
			vol[i][j] = mxcmd->set.vol[i][j];
		else if (cnt[i][j])
			vol[i][j] = vol[i][j] / cnt[i][j];
		else if (mixer->vol[i][j] >= 0)
			vol[i][j] = mixer->vol[i][j];
		else
			vol[i][j] = 100;

		vol[i][j] = min(max(0, vol[i][j]), 100);
	}

	if (dir >= 0)
		mxcmd->rvol = vol[dir][LEFT] | (vol[dir][RIGHT] << 8);

	pthread_mutex_unlock(&mixer_mutex);
}

static void mixer_simple_ioctl(fuse_req_t req, struct ossp_mixer *mixer,
			       unsigned cmd, void *uarg, const void *in_buf,
			       size_t in_bufsz, size_t out_bufsz,
			       int *not_minep)
{
	const char *id = "OSS Proxy", *name = "Mixer";
	int i;

	switch (cmd) {
	case SOUND_MIXER_INFO: {
		struct mixer_info info = { };

		PREP_UARG(NULL, &info);
		strncpy(info.id, id, sizeof(info.id) - 1);
		strncpy(info.name, name, sizeof(info.name) - 1);
		info.modify_counter = mixer->modify_counter;
		IOCTL_RETURN(0, &info);
	}

	case SOUND_OLD_MIXER_INFO: {
		struct _old_mixer_info info = { };

		PREP_UARG(NULL, &info);
		strncpy(info.id, id, sizeof(info.id) - 1);
		strncpy(info.name, name, sizeof(info.name) - 1);
		IOCTL_RETURN(0, &info);
	}

	case OSS_GETVERSION:
		i = SNDRV_OSS_VERSION;
		goto puti;
	case SOUND_MIXER_READ_DEVMASK:
	case SOUND_MIXER_READ_STEREODEVS:
		i = SOUND_MASK_PCM | SOUND_MASK_IGAIN;
		goto puti;
	case SOUND_MIXER_READ_CAPS:
		i = SOUND_CAP_EXCL_INPUT;
		goto puti;
	case SOUND_MIXER_READ_RECMASK:
	case SOUND_MIXER_READ_RECSRC:
		i = SOUND_MASK_IGAIN;
		goto puti;
	puti:
		PREP_UARG(NULL, &i);
		IOCTL_RETURN(0, &i);

	case SOUND_MIXER_WRITE_RECSRC:
		IOCTL_RETURN(0, NULL);

	default:
		*not_minep = 1;
		return;
	}
	assert(0);
}

static void mixer_do_ioctl(fuse_req_t req, struct ossp_mixer *mixer,
			   unsigned cmd, void *uarg, const void *in_buf,
			   size_t in_bufsz, size_t out_bufsz)
{
	struct ossp_mixer_cmd mxcmd;
	struct ossp_stream *os, **osa;
	int not_mine = 0;
	int slot = cmd & 0xff, dir;
	int nr_os;
	int i, rc;

	mixer_simple_ioctl(req, mixer, cmd, uarg, in_buf, in_bufsz, out_bufsz,
			   &not_mine);
	if (!not_mine)
		return;

	rc = -ENXIO;
	if (!(cmd & (SIOC_IN | SIOC_OUT)))
		goto err;

	/*
	 * Okay, it's not one of the easy ones.  Build mxcmd for
	 * actual volume control.
	 */
	if (cmd & SIOC_IN)
		PREP_UARG(&i, &i);
	else
		PREP_UARG(NULL, &i);

	switch (slot) {
	case SOUND_MIXER_PCM:
		dir = PLAY;
		break;
	case SOUND_MIXER_IGAIN:
		dir = REC;
		break;
	default:
		i = 0;
		IOCTL_RETURN(0, &i);
	}

	init_mixer_cmd(&mxcmd, mixer);

	if (cmd & SIOC_IN) {
		unsigned l, r;

		rc = -EINVAL;
		l = i & 0xff;
		r = (i >> 8) & 0xff;
		if (l > 100 || r > 100)
			goto err;

		mixer->vol[dir][LEFT] = mxcmd.set.vol[dir][LEFT] = l;
		mixer->vol[dir][RIGHT] = mxcmd.set.vol[dir][RIGHT] = r;
	}
	mxcmd.out_dir = dir;

	/*
	 * Apply volume conrol
	 */
	/* acquire target streams */
	pthread_mutex_lock(&mutex);
	osa = calloc(max_streams, sizeof(osa[0]));
	if (!osa) {
		pthread_mutex_unlock(&mutex);
		rc = -ENOMEM;
		goto err;
	}

	nr_os = 0;
	list_for_each_entry(os, os_pgrp_tbl_head(mixer->pgrp), pgrp_link) {
		if (os->pgrp == mixer->pgrp) {
			osa[nr_os++] = os;
			os->refcnt++;
		}
 	}

	pthread_mutex_unlock(&mutex);

	/* execute mxcmd for each stream and put it */
	for (i = 0; i < nr_os; i++) {
		exec_mixer_cmd(&mxcmd, osa[i]);
		put_os(osa[i]);
	}

	finish_mixer_cmd(&mxcmd);
	free(osa);

	IOCTL_RETURN(0, out_bufsz ? &mxcmd.rvol : NULL);

err:
	fuse_reply_err(req, -rc);
}

static void mixer_open(fuse_req_t req, struct fuse_file_info *fi)
{
	pid_t pid = fuse_req_ctx(req)->pid, pgrp;
	struct ossp_mixer *mixer;
	int rc;

	rc = get_proc_self_info(pid, &pgrp, NULL, 0);
	if (rc) {
		err_e(rc, "get_proc_self_info(%d) failed", pid);
		fuse_reply_err(req, -rc);
		return;
	}

	mixer = get_mixer(pgrp);
	fi->fh = pgrp;

	if (mixer)
		fuse_reply_open(req, fi);
	else
		fuse_reply_err(req, ENOMEM);
}

static void mixer_ioctl(fuse_req_t req, int signed_cmd, void *uarg,
			struct fuse_file_info *fi, unsigned int flags,
			const void *in_buf, size_t in_bufsz, size_t out_bufsz)
{
	struct ossp_mixer *mixer;

	mixer = find_mixer(fi->fh);
	if (!mixer) {
		fuse_reply_err(req, EBADF);
		return;
	}

	mixer_do_ioctl(req, mixer, signed_cmd, uarg, in_buf, in_bufsz,
		       out_bufsz);
}

static void mixer_release(fuse_req_t req, struct fuse_file_info *fi)
{
	struct ossp_mixer *mixer;

	mixer = find_mixer(fi->fh);
	if (mixer) {
		put_mixer(mixer);
		fuse_reply_err(req, 0);
	} else
		fuse_reply_err(req, EBADF);
}


/***************************************************************************
 * Stream implementation
 */

static int alloc_os(size_t stream_size, size_t mmap_size, pid_t pid, uid_t pgrp,
		    uid_t uid, gid_t gid, int cmd_sock,
		    const int *notify, struct fuse_session *se,
		    struct ossp_stream **osp)
{
	struct ossp_uid_cnt *tmp_ucnt, *ucnt = NULL;
	struct ossp_stream *os;
	int rc;

	assert(stream_size >= sizeof(struct ossp_stream));
	os = calloc(1, stream_size);
	if (!os)
		return -ENOMEM;

	INIT_LIST_HEAD(&os->link);
	INIT_LIST_HEAD(&os->pgrp_link);
	INIT_LIST_HEAD(&os->notify_link);
	os->refcnt = 1;

	rc = -pthread_mutex_init(&os->cmd_mutex, NULL);
	if (rc)
		goto err_free;

	rc = -pthread_mutex_init(&os->mmap_mutex, NULL);
	if (rc)
		goto err_destroy_cmd_mutex;

	pthread_mutex_lock(&mutex);

	list_for_each_entry(tmp_ucnt, &uid_cnt_list, link)
		if (tmp_ucnt->uid == uid) {
			ucnt = tmp_ucnt;
			break;
		}
	if (!ucnt) {
		rc = -ENOMEM;
		ucnt = calloc(1, sizeof(*ucnt));
		if (!ucnt)
			goto err_unlock;
		ucnt->uid = uid;
		list_add(&ucnt->link, &uid_cnt_list);
	}

	rc = -EBUSY;
	if (ucnt->nr_os + 1 > umax_streams)
		goto err_unlock;

	/* everything looks fine, allocate id and init stream */
	rc = -EBUSY;
	os->id = find_next_zero_bit(os_id_bitmap, max_streams, 0);
	if (os->id >= max_streams)
		goto err_unlock;
	__set_bit(os->id, os_id_bitmap);

	os->cmd_fd = cmd_sock;
	os->notify_tx = notify[1];
	os->notify_rx = notify[0];
	os->pid = pid;
	os->pgrp = pgrp;
	os->uid = uid;
	os->gid = gid;
	if (mmap_size) {
		os->mmap_off = os->id * mmap_size;
		os->mmap_size = mmap_size;
	}
	os->ucnt = ucnt;
	os->se = se;

	memset(os->vol, -1, sizeof(os->vol));
	memset(os->vol_set, -1, sizeof(os->vol));

	list_add(&os->link, os_tbl_head(os->id));
	list_add(&os->pgrp_link, os_pgrp_tbl_head(os->pgrp));

	ucnt->nr_os++;
	*osp = os;
	pthread_mutex_unlock(&mutex);
	return 0;

err_unlock:
	pthread_mutex_unlock(&mutex);
	pthread_mutex_destroy(&os->mmap_mutex);
err_destroy_cmd_mutex:
	pthread_mutex_destroy(&os->cmd_mutex);
err_free:
	free(os);
	return rc;
}

static void shutdown_notification(struct ossp_stream *os)
{
	struct ossp_notify obituary = { .magic = OSSP_NOTIFY_MAGIC,
					.opcode = OSSP_NOTIFY_OBITUARY };
	ssize_t ret;

	/*
	 * Shutdown notification for this stream.  We politely ask
	 * notify_poller to shut the receive side down to avoid racing
	 * with it.
	 */
	while (os->notify_rx >= 0) {
		ret = write(os->notify_tx, &obituary, sizeof(obituary));
		if (ret <= 0) {
			if (ret == 0)
				warn_os(os, "unexpected EOF on notify_tx");
			else if (errno != EPIPE)
				warn_ose(os, -errno,
					 "unexpected error on notify_tx");
			close(os->notify_rx);
			os->notify_rx = -1;
			break;
		}

		if (ret != sizeof(obituary))
			warn_os(os, "short transfer on notify_tx");
		pthread_cond_wait(&notify_poller_kill_wait, &mutex);
	}
}

static void put_os(struct ossp_stream *os)
{
	if (!os)
		return;

	pthread_mutex_lock(&mutex);

	assert(os->refcnt);
	if (--os->refcnt) {
		pthread_mutex_unlock(&mutex);
		return;
	}

	os->dead = 1;
	shutdown_notification(os);

	dbg0_os(os, "DESTROY");

	list_del_init(&os->link);
	list_del_init(&os->pgrp_link);
	list_del_init(&os->notify_link);
	os->ucnt->nr_os--;

	pthread_mutex_unlock(&mutex);

	close(os->cmd_fd);
	close(os->notify_tx);
	put_mixer(os->mixer);
	pthread_mutex_destroy(&os->cmd_mutex);
	pthread_mutex_destroy(&os->mmap_mutex);

	pthread_mutex_lock(&mutex);
	dbg1_os(os, "stream dead, requesting reaping");
	list_add_tail(&os->link, &slave_corpse_list);
	pthread_cond_signal(&slave_reaper_wait);
	pthread_mutex_unlock(&mutex);
}

static void set_extra_env(pid_t pid)
{
	char procenviron[32];
	const int step = 1024;
	char *data = malloc(step + 1);
	int ofs = 0;
	int fd;
	int ret;

	if (!data)
		return;

	sprintf(procenviron, "/proc/%d/environ", pid);
	fd = open(procenviron, O_RDONLY);
	if (fd < 0)
		return;

	/*
	 * There should really be a 'read whole file to a newly allocated
	 * buffer' function.
	 */
	while ((ret = read(fd, data + ofs, step)) > 0) {
		char *newdata;
		ofs += ret;
		newdata = realloc(data, ofs + step + 1);
		if (!newdata) {
			ret = -1;
			break;
		}
		data = newdata;
	}
	if (ret == 0) {
		char *ptr = data;
		/* Append the extra 0 for end condition */
		data[ofs] = 0;

		while ((ret = strlen(ptr)) > 0) {
			/*
			 * Copy all PULSE variables and DISPLAY so that
			 * ssh -X remotehost 'mplayer -ao oss' will work
			 */
			if (!strncmp(ptr, "DISPLAY=", 8) ||
			    !strncmp(ptr, "PULSE_", 6))
				putenv(ptr);
			ptr += ret + 1;
		}
	}

	free(data);
	close(fd);
}

#ifndef GIOVANNI
int contapid = 13000;
#endif// GIOVANNI

static int create_os(const char *slave_path,
		     size_t stream_size, size_t mmap_size,
		     pid_t pid, pid_t pgrp, uid_t uid, gid_t gid,
		     struct fuse_session *se, struct ossp_stream **osp)
{
	static pthread_mutex_t create_mutex = PTHREAD_MUTEX_INITIALIZER;
	int cmd_sock[2] = { -1, -1 };
	int notify_sock[2] = { -1, -1 };
	struct ossp_stream *os = NULL;
	struct epoll_event ev = { };
	int i, rc;

	/*
	 * Only one thread can be creating a stream.  This is to avoid
	 * leaking unwanted fds into slaves.
	 */
	pthread_mutex_lock(&create_mutex);

	/* prepare communication channels */
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, cmd_sock) ||
	    socketpair(AF_UNIX, SOCK_STREAM, 0, notify_sock)) {
		rc = -errno;
		warn_e(rc, "failed to create slave command channel");
		goto close_all;
	}

	if (fcntl(notify_sock[0], F_SETFL, O_NONBLOCK) < 0) {
		rc = -errno;
		warn_e(rc, "failed to set NONBLOCK on notify sock");
		goto close_all;
	}

	/*
	 * Alloc stream which will be responsible for all server side
	 * resources from now on.
	 */
	rc = alloc_os(stream_size, mmap_size, pid, pgrp, uid, gid, cmd_sock[0],
		      notify_sock, se, &os);
	if (rc) {
		warn_e(rc, "failed to allocate stream for %d", pid);
		goto close_all;
	}

	rc = -ENOMEM;
	os->mixer = get_mixer(pgrp);
	if (!os->mixer)
		goto put_os;

	/*
	 * Register notification.  If successful, notify_poller has
	 * custody of notify_rx fd.
	 */
	pthread_mutex_lock(&mutex);
	list_add(&os->notify_link, os_notify_tbl_head(os->notify_rx));
	pthread_mutex_unlock(&mutex);

#ifndef GIOVANNI
	os->slave_pid = contapid;
	contapid++;
	if(contapid > 30000)
		contapid=13000;
#endif //GIOVANNI

//#ifdef GIOVANNI
	ev.events = EPOLLIN;
	ev.data.fd = notify_sock[0];
	if (epoll_ctl(notify_epfd, EPOLL_CTL_ADD, notify_sock[0], &ev)) {
		/*
		 * Without poller watching this notify sock, poller
		 * shutdown sequence in shutdown_notification() can't
		 * be used.  Kill notification rx manually.
		 */
		rc = -errno;
		warn_ose(os, rc, "failed to add notify epoll");
		close(os->notify_rx);
		os->notify_rx = -1;
		goto put_os;
	}

	/* start slave */
	os->slave_pid = fork();
	if (os->slave_pid < 0) {
		rc = -errno;
		warn_ose(os, rc, "failed to fork slave");
		goto put_os;
	}

	if (os->slave_pid == 0) {
		/* child */
		char id_str[2][16], fd_str[3][16];
		char mmap_off_str[32], mmap_size_str[32];
		char log_str[16], slave_path_copy[PATH_MAX];
		char *argv[] = { slave_path_copy, "-u", id_str[0],
				 "-g", id_str[1], "-c", fd_str[0],
				 "-n", fd_str[1], "-m", fd_str[2],
				 "-o", mmap_off_str, "-s", mmap_size_str,
				 "-l", log_str, NULL, NULL };
		struct passwd *pwd;

		/* drop stuff we don't need */
		if (close(cmd_sock[0]) || close(notify_sock[0]))
			fatal_e(-errno, "failed to close server pipe fds");

#ifdef OSSP_MMAP
		if (!mmap_size)
			close(fuse_mmap_fd(se));
#endif

		clearenv();
		pwd = getpwuid(os->uid);
		if (pwd) {
			setenv("LOGNAME", pwd->pw_name, 1);
			setenv("USER", pwd->pw_name, 1);
			setenv("HOME", pwd->pw_dir, 1);
		}
		/* Set extra environment variables from the caller */
		set_extra_env(pid);

		/* prep and exec */
		slave_path_copy[sizeof(slave_path_copy) - 1] = '\0';
		strncpy(slave_path_copy, slave_path, sizeof(slave_path_copy) - 1);
		if (slave_path_copy[sizeof(slave_path_copy) - 1] != '\0') {
			rc = -errno;
			err_ose(os, rc, "slave path too long");
			goto child_fail;
		}

		snprintf(id_str[0], sizeof(id_str[0]), "%d", os->uid);
		snprintf(id_str[1], sizeof(id_str[0]), "%d", os->gid);
		snprintf(fd_str[0], sizeof(fd_str[0]), "%d", cmd_sock[1]);
		snprintf(fd_str[1], sizeof(fd_str[1]), "%d", notify_sock[1]);
		snprintf(fd_str[2], sizeof(fd_str[2]), "%d",
#ifdef OSSP_MMAP
			 mmap_size ? fuse_mmap_fd(se) :
#endif
			 -1);
		snprintf(mmap_off_str, sizeof(mmap_off_str), "0x%llx",
			 (unsigned long long)os->mmap_off);
		snprintf(mmap_size_str, sizeof(mmap_size_str), "0x%zx",
			 mmap_size);
		snprintf(log_str, sizeof(log_str), "%d", ossp_log_level);
		if (ossp_log_timestamp)
			argv[ARRAY_SIZE(argv) - 2] = "-t";

		execv(slave_path, argv);
		rc = -errno;
		err_ose(os, rc, "execv failed for <%d>", pid);
	child_fail:
		_exit(1);
	}
//#endif //GIOVANNI

	/* turn on CLOEXEC on all server side fds */
	if (fcntl(os->cmd_fd, F_SETFD, FD_CLOEXEC) < 0 ||
	    fcntl(os->notify_tx, F_SETFD, FD_CLOEXEC) < 0 ||
	    fcntl(os->notify_rx, F_SETFD, FD_CLOEXEC) < 0) {
		rc = -errno;
		err_ose(os, rc, "failed to set CLOEXEC on server side fds");
		goto put_os;
	}

	dbg0_os(os, "CREATE slave=%d %s", os->slave_pid, slave_path);
	dbg0_os(os, "  client=%d cmd=%d:%d notify=%d:%d mmap=%d:0x%llx:%zu",
		pid, cmd_sock[0], cmd_sock[1], notify_sock[0], notify_sock[1],
#ifdef OSSP_MMAP
		os->mmap_size ? fuse_mmap_fd(se) :
#endif
		-1,
		(unsigned long long)os->mmap_off, os->mmap_size);

	*osp = os;
	rc = 0;
	goto close_client_fds;

put_os:
	put_os(os);
close_client_fds:
	close(cmd_sock[1]);
	pthread_mutex_unlock(&create_mutex);
	return rc;

close_all:
	for (i = 0; i < 2; i++) {
		close(cmd_sock[i]);
		close(notify_sock[i]);
	}
	pthread_mutex_unlock(&create_mutex);
	return rc;
}

static void dsp_open_common(fuse_req_t req, struct fuse_file_info *fi,
			    struct fuse_session *se)
{
	const struct fuse_ctx *fuse_ctx = fuse_req_ctx(req);
	struct ossp_dsp_open_arg arg = { };
	struct ossp_stream *os = NULL;
	struct ossp_mixer *mixer;
	struct ossp_dsp_stream *dsps;
	struct ossp_mixer_cmd mxcmd;
	pid_t pgrp;
	ssize_t ret;

	ret = get_proc_self_info(fuse_ctx->pid, &pgrp, NULL, 0);
	if (ret) {
		err_e(ret, "get_proc_self_info(%d) failed", fuse_ctx->pid);
		goto err;
	}

	ret = create_os(dsp_slave_path, sizeof(*dsps), DSPS_MMAP_SIZE,
			fuse_ctx->pid, pgrp, fuse_ctx->uid, fuse_ctx->gid,
			se, &os);
	if (ret)
		goto err;
	dsps = os_to_dsps(os);
	mixer = os->mixer;

	switch (fi->flags & O_ACCMODE) {
	case O_WRONLY:
		dsps->rw |= 1 << PLAY;
		break;
	case O_RDONLY:
		dsps->rw |= 1 << REC;
		break;
	case O_RDWR:
		dsps->rw |= (1 << PLAY) | (1 << REC);
		break;
	default:
		assert(0);
	}

	arg.flags = fi->flags;
	arg.opener_pid = os->pid;
	ret = exec_simple_cmd(&dsps->os, OSSP_DSP_OPEN, &arg, NULL);
	if (ret < 0) {
		put_os(os);
		goto err;
	}

	memcpy(os->vol, mixer->vol, sizeof(os->vol));
	if (os->vol[PLAY][0] >= 0 || os->vol[REC][0] >= 0) {
		init_mixer_cmd(&mxcmd, mixer);
		memcpy(mxcmd.set.vol, os->vol, sizeof(os->vol));
		exec_mixer_cmd(&mxcmd, os);
		finish_mixer_cmd(&mxcmd);
	}

	fi->direct_io = 1;
	fi->nonseekable = 1;
	fi->fh = os->id;

	fuse_reply_open(req, fi);
	return;

err:
	fuse_reply_err(req, -ret);
}

static void dsp_open(fuse_req_t req, struct fuse_file_info *fi)
{
	dsp_open_common(req, fi, dsp_se);
}

static void adsp_open(fuse_req_t req, struct fuse_file_info *fi)
{
	dsp_open_common(req, fi, adsp_se);
}

static void dsp_release(fuse_req_t req, struct fuse_file_info *fi)
{
	struct ossp_stream *os;

	os = find_os(fi->fh);
	if (os) {
		put_os(os);
		fuse_reply_err(req, 0);
	} else
		fuse_reply_err(req, EBADF);
}

static void dsp_read(fuse_req_t req, size_t size, off_t off,
		     struct fuse_file_info *fi)
{
	struct ossp_dsp_rw_arg arg = { };
	struct ossp_stream *os;
	struct ossp_dsp_stream *dsps;
	void *buf = NULL;
	ssize_t ret;

	ret = -EBADF;
	os = find_os(fi->fh);
	if (!os)
		goto out;
	dsps = os_to_dsps(os);

	ret = -EINVAL;
	if (!(dsps->rw & (1 << REC)))
		goto out;

	ret = -ENXIO;
	if (dsps->mmapped)
		goto out;

	ret = -ENOMEM;
	buf = malloc(size);
	if (!buf)
		goto out;

	arg.nonblock = (fi->flags & O_NONBLOCK) || dsps->nonblock;

	ret = exec_cmd(os, OSSP_DSP_READ, &arg, sizeof(arg),
		       NULL, 0, NULL, 0, buf, &size, -1);
out:
	if (ret >= 0)
		fuse_reply_buf(req, buf, size);
	else
		fuse_reply_err(req, -ret);

	free(buf);
}

static void dsp_write(fuse_req_t req, const char *buf, size_t size, off_t off,
		      struct fuse_file_info *fi)
{
	struct ossp_dsp_rw_arg arg = { };
	struct ossp_stream *os;
	struct ossp_dsp_stream *dsps;
	ssize_t ret;

	ret = -EBADF;
	os = find_os(fi->fh);
	if (!os)
		goto out;
	dsps = os_to_dsps(os);

	ret = -EINVAL;
	if (!(dsps->rw & (1 << PLAY)))
		goto out;

	ret = -ENXIO;
	if (dsps->mmapped)
		goto out;

	arg.nonblock = (fi->flags & O_NONBLOCK) || dsps->nonblock;

	ret = exec_cmd(os, OSSP_DSP_WRITE, &arg, sizeof(arg),
		       buf, size, NULL, 0, NULL, NULL, -1);
out:
	if (ret >= 0)
		fuse_reply_write(req, ret);
	else
		fuse_reply_err(req, -ret);
}

static void dsp_poll(fuse_req_t req, struct fuse_file_info *fi,
		     struct fuse_pollhandle *ph)
{
	int notify = ph != NULL;
	unsigned revents = 0;
	struct ossp_stream *os;
	ssize_t ret;

	ret = -EBADF;
	os = find_os(fi->fh);
	if (!os)
		goto out;

	if (ph) {
		pthread_mutex_lock(&mutex);
		if (os->ph)
			fuse_pollhandle_destroy(os->ph);
		os->ph = ph;
		pthread_mutex_unlock(&mutex);
	}

	ret = exec_simple_cmd(os, OSSP_DSP_POLL, &notify, &revents);
out:
	if (ret >= 0)
		fuse_reply_poll(req, revents);
	else
		fuse_reply_err(req, -ret);
}

static void dsp_ioctl(fuse_req_t req, int signed_cmd, void *uarg,
		      struct fuse_file_info *fi, unsigned int flags,
		      const void *in_buf, size_t in_bufsz, size_t out_bufsz)
{
	/* some ioctl constants are long and has the highest bit set */
	unsigned cmd = signed_cmd;
	struct ossp_stream *os;
	struct ossp_dsp_stream *dsps;
	enum ossp_opcode op;
	ssize_t ret;
	int i;

	ret = -EBADF;
	os = find_os(fi->fh);
	if (!os)
		goto err;
	dsps = os_to_dsps(os);

	/* mixer commands are allowed on DSP devices */
	if (((cmd >> 8) & 0xff) == 'M') {
		mixer_do_ioctl(req, os->mixer, cmd, uarg, in_buf, in_bufsz,
			       out_bufsz);
		return;
	}

	/* and the rest */
	switch (cmd) {
	case OSS_GETVERSION:
		i = SNDRV_OSS_VERSION;
		PREP_UARG(NULL, &i);
		IOCTL_RETURN(0, &i);

	case SNDCTL_DSP_GETCAPS:
		i = DSP_CAP_DUPLEX | DSP_CAP_REALTIME | DSP_CAP_TRIGGER |
#ifdef OSSP_MMAP
			DSP_CAP_MMAP |
#endif
			DSP_CAP_MULTI;
		PREP_UARG(NULL, &i);
		IOCTL_RETURN(0, &i);

	case SNDCTL_DSP_NONBLOCK:
		dsps->nonblock = 1;
		ret = 0;
		IOCTL_RETURN(0, NULL);

	case SNDCTL_DSP_RESET:		op = OSSP_DSP_RESET;		goto nd;
	case SNDCTL_DSP_SYNC:		op = OSSP_DSP_SYNC;		goto nd;
	case SNDCTL_DSP_POST:		op = OSSP_DSP_POST;		goto nd;
	nd:
		ret = exec_simple_cmd(&dsps->os, op, NULL, NULL);
		if (ret)
			goto err;
		IOCTL_RETURN(0, NULL);

	case SOUND_PCM_READ_RATE:	op = OSSP_DSP_GET_RATE;		goto ri;
	case SOUND_PCM_READ_BITS:	op = OSSP_DSP_GET_FORMAT;	goto ri;
	case SOUND_PCM_READ_CHANNELS:	op = OSSP_DSP_GET_CHANNELS;	goto ri;
	case SNDCTL_DSP_GETBLKSIZE:	op = OSSP_DSP_GET_BLKSIZE;	goto ri;
	case SNDCTL_DSP_GETFMTS:	op = OSSP_DSP_GET_FORMATS;	goto ri;
	case SNDCTL_DSP_GETTRIGGER:	op = OSSP_DSP_GET_TRIGGER;	goto ri;
	ri:
		PREP_UARG(NULL, &i);
		ret = exec_simple_cmd(&dsps->os, op, NULL, &i);
		if (ret)
			goto err;
		IOCTL_RETURN(0, &i);

	case SNDCTL_DSP_SPEED:		op = OSSP_DSP_SET_RATE;		goto wi;
	case SNDCTL_DSP_SETFMT:		op = OSSP_DSP_SET_FORMAT;	goto wi;
	case SNDCTL_DSP_CHANNELS:	op = OSSP_DSP_SET_CHANNELS;	goto wi;
	case SNDCTL_DSP_SUBDIVIDE:	op = OSSP_DSP_SET_SUBDIVISION;	goto wi;
	wi:
		PREP_UARG(&i, &i);
		ret = exec_simple_cmd(&dsps->os, op, &i, &i);
		if (ret)
			goto err;
		IOCTL_RETURN(0, &i);

	case SNDCTL_DSP_STEREO:
		PREP_UARG(NULL, &i);
		i = 2;
		ret = exec_simple_cmd(&dsps->os, OSSP_DSP_SET_CHANNELS, &i, &i);
		i--;
		if (ret)
			goto err;
		IOCTL_RETURN(0, &i);

	case SNDCTL_DSP_SETFRAGMENT:
		PREP_UARG(&i, NULL);
		ret = exec_simple_cmd(&dsps->os,
				      OSSP_DSP_SET_FRAGMENT, &i, NULL);
		if (ret)
			goto err;
		IOCTL_RETURN(0, NULL);

	case SNDCTL_DSP_SETTRIGGER:
		PREP_UARG(&i, NULL);
		ret = exec_simple_cmd(&dsps->os,
				      OSSP_DSP_SET_TRIGGER, &i, NULL);
		if (ret)
			goto err;
		IOCTL_RETURN(0, NULL);

	case SNDCTL_DSP_GETOSPACE:
	case SNDCTL_DSP_GETISPACE: {
		struct audio_buf_info info;

		ret = -EINVAL;
		if (cmd == SNDCTL_DSP_GETOSPACE) {
			if (!(dsps->rw & (1 << PLAY)))
				goto err;
			op = OSSP_DSP_GET_OSPACE;
		} else {
			if (!(dsps->rw & (1 << REC)))
				goto err;
			op = OSSP_DSP_GET_ISPACE;
		}

		PREP_UARG(NULL, &info);
		ret = exec_simple_cmd(&dsps->os, op, NULL, &info);
		if (ret)
			goto err;
		IOCTL_RETURN(0, &info);
	}

	case SNDCTL_DSP_GETOPTR:
	case SNDCTL_DSP_GETIPTR: {
		struct count_info info;

		op = cmd == SNDCTL_DSP_GETOPTR ? OSSP_DSP_GET_OPTR
					       : OSSP_DSP_GET_IPTR;
		PREP_UARG(NULL, &info);
		ret = exec_simple_cmd(&dsps->os, op, NULL, &info);
		if (ret)
			goto err;
		IOCTL_RETURN(0, &info);
	}

	case SNDCTL_DSP_GETODELAY:
		PREP_UARG(NULL, &i);
		i = 0;
		ret = exec_simple_cmd(&dsps->os, OSSP_DSP_GET_ODELAY, NULL, &i);
		IOCTL_RETURN(ret, &i);	/* always copy out result, 0 on err */

	case SOUND_PCM_WRITE_FILTER:
	case SOUND_PCM_READ_FILTER:
		ret = -EIO;
		goto err;

	case SNDCTL_DSP_MAPINBUF:
	case SNDCTL_DSP_MAPOUTBUF:
		ret = -EINVAL;
		goto err;

	case SNDCTL_DSP_SETSYNCRO:
	case SNDCTL_DSP_SETDUPLEX:
	case SNDCTL_DSP_PROFILE:
		IOCTL_RETURN(0, NULL);

	default:
		warn_os(os, "unknown ioctl 0x%x", cmd);
		ret = -EINVAL;
		goto err;
	}
	assert(0);	/* control shouldn't reach here */
err:
	fuse_reply_err(req, -ret);
}

#ifdef OSSP_MMAP
static int dsp_mmap_dir(int prot)
{
	if (!(prot & PROT_WRITE))
		return REC;
	return PLAY;
}

static void dsp_mmap(fuse_req_t req, void *addr, size_t len, int prot,
		     int flags, off_t offset, struct fuse_file_info *fi,
		     uint64_t mh)
{
	int dir = dsp_mmap_dir(prot);
	struct ossp_dsp_mmap_arg arg = { };
	struct ossp_stream *os;
	struct ossp_dsp_stream *dsps;
	ssize_t ret;

	os = find_os(fi->fh);
	if (!os) {
		fuse_reply_err(req, EBADF);
		return;
	}
	dsps = os_to_dsps(os);

	if (!os->mmap_off || len > os->mmap_size / 2) {
		fuse_reply_err(req, EINVAL);
		return;
	}

	pthread_mutex_lock(&os->mmap_mutex);

	ret = -EBUSY;
	if (dsps->mmapped & (1 << dir))
		goto out_unlock;

	arg.dir = dir;
	arg.size = len;

	ret = exec_simple_cmd(os, OSSP_DSP_MMAP, &arg, NULL);
	if (ret == 0)
		dsps->mmapped |= 1 << dir;

out_unlock:
	pthread_mutex_unlock(&os->mmap_mutex);

	if (ret == 0)
		fuse_reply_mmap(req, os->mmap_off + dir * os->mmap_size / 2, 0);
	else
		fuse_reply_err(req, -ret);
}

static void dsp_munmap(fuse_req_t req, size_t len, struct fuse_file_info *fi,
		       off_t offset, uint64_t mh)
{
	struct ossp_stream *os;
	struct ossp_dsp_stream *dsps;
	int dir, rc;

	os = find_os(fi->fh);
	if (!os)
		goto out;
	dsps = os_to_dsps(os);

	pthread_mutex_lock(&os->mmap_mutex);

	for (dir = 0; dir < 2; dir++)
		if (offset == os->mmap_off + dir * os->mmap_size / 2)
			break;
	if (dir == 2 || len > os->mmap_size / 2) {
		warn_os(os, "invalid munmap request "
			"offset=%llu len=%zu mmapped=0x%x",
			(unsigned long long)offset, len, dsps->mmapped);
		goto out_unlock;
	}

	rc = exec_simple_cmd(os, OSSP_DSP_MUNMAP, &dir, NULL);
	if (rc)
		warn_ose(os, rc, "MUNMAP failed for dir=%d", dir);

	dsps->mmapped &= ~(1 << dir);

out_unlock:
	pthread_mutex_unlock(&os->mmap_mutex);
out:
	fuse_reply_none(req);
}
#endif


/***************************************************************************
 * Notify poller
 */

static void *notify_poller(void *arg)
{
	struct epoll_event events[1024];
	int i, nfds;

repeat:
	nfds = epoll_wait(notify_epfd, events, ARRAY_SIZE(events), -1);
	for (i = 0; i < nfds; i++) {
		int do_notify = 0;
		struct ossp_stream *os;
		struct ossp_notify notify;
		ssize_t ret;

		os = find_os_by_notify_rx(events[i].data.fd);
		if (!os) {
			err("can't find stream for notify_rx fd %d",
			    events[i].data.fd);
			epoll_ctl(notify_epfd, EPOLL_CTL_DEL, events[i].data.fd,
				  NULL);
			/* we don't know what's going on, don't close the fd */
			continue;
		}

		while ((ret = read(os->notify_rx,
				   &notify, sizeof(notify))) > 0) {
			if (os->dead)
				continue;
			if (ret != sizeof(notify)) {
				warn_os(os, "short read on notify_rx (%zu, "
					"expected %zu), killing the stream",
					ret, sizeof(notify));
				os->dead = 1;
				break;
			}
			if (notify.magic != OSSP_NOTIFY_MAGIC) {
				warn_os(os, "invalid magic on notification, "
					"killing the stream");
				os->dead = 1;
				break;
			}

			if (notify.opcode >= OSSP_NR_NOTIFY_OPCODES)
				goto unknown;

			dbg1_os(os, "NOTIFY %s", ossp_notify_str[notify.opcode]);

			switch (notify.opcode) {
			case OSSP_NOTIFY_POLL:
				do_notify = 1;
				break;
			case OSSP_NOTIFY_OBITUARY:
				os->dead = 1;
				break;
			case OSSP_NOTIFY_VOLCHG:
				pthread_mutex_lock(&mixer_mutex);
				os->mixer->modify_counter++;
				pthread_mutex_unlock(&mixer_mutex);
				break;
			default:
			unknown:
				warn_os(os, "unknown notification %d",
					notify.opcode);
			}
		}
		if (ret == 0)
			os->dead = 1;
		else if (ret < 0 && errno != EAGAIN) {
			warn_ose(os, -errno, "read fail on notify fd");
			os->dead = 1;
		}

		if (!do_notify && !os->dead)
			continue;

		pthread_mutex_lock(&mutex);

		if (os->ph) {
			fuse_lowlevel_notify_poll(os->ph);
			fuse_pollhandle_destroy(os->ph);
			os->ph = NULL;
		}

		if (os->dead) {
			dbg0_os(os, "removing %d from notify poll list",
				os->notify_rx);
			epoll_ctl(notify_epfd, EPOLL_CTL_DEL, os->notify_rx,
				  NULL);
			close(os->notify_rx);
			os->notify_rx = -1;
			pthread_cond_broadcast(&notify_poller_kill_wait);
		}

		pthread_mutex_unlock(&mutex);
	}
	goto repeat;
}


/***************************************************************************
 * Slave corpse reaper
 */

static void *slave_reaper(void *arg)
{
	struct ossp_stream *os;
	int status;
	pid_t pid;

	pthread_mutex_lock(&mutex);
repeat:
	while (list_empty(&slave_corpse_list))
		pthread_cond_wait(&slave_reaper_wait, &mutex);

	os = list_first_entry(&slave_corpse_list, struct ossp_stream, link);
	list_del_init(&os->link);

	pthread_mutex_unlock(&mutex);

	do {
		pid = waitpid(os->slave_pid, &status, 0);
	} while (pid < 0 && errno == EINTR);

	if (pid < 0) {
		if (errno == ECHILD)
			warn_ose(os, -errno, "slave %d already gone?",
				 os->slave_pid);
		else
			fatal_e(-errno, "waitpid(%d) failed", os->slave_pid);
	}

	pthread_mutex_lock(&mutex);

	dbg1_os(os, "slave %d reaped", os->slave_pid);
	__clear_bit(os->id, os_id_bitmap);
	free(os);

	goto repeat;
}


/***************************************************************************
 * Stuff to bind and start everything
 */

static void ossp_daemonize(void)
{
	int fd, pfd[2];
	pid_t pid;
	ssize_t ret;
	int err;

	fd = open("/dev/null", O_RDWR);
	if (fd >= 0) {
		dup2(fd, 0);
		dup2(fd, 1);
		dup2(fd, 2);
		if (fd > 2)
			close(fd);
	}

	if (pipe(pfd))
		fatal_e(-errno, "failed to create pipe for init wait");

	if (fcntl(pfd[0], F_SETFD, FD_CLOEXEC) < 0 ||
	    fcntl(pfd[1], F_SETFD, FD_CLOEXEC) < 0)
		fatal_e(-errno, "failed to set CLOEXEC on init wait pipe");

	pid = fork();
	if (pid < 0)
		fatal_e(-errno, "failed to fork for daemon");

	if (pid == 0) {
		close(pfd[0]);
		init_wait_fd = pfd[1];

		/* be evil, my child */
		chdir("/");
		setsid();
		return;
	}

	/* wait for init completion and pass over success indication */
	close(pfd[1]);

	do {
		ret = read(pfd[0], &err, sizeof(err));
	} while (ret < 0 && errno == EINTR);

	if (ret == sizeof(err) && err == 0)
		exit(0);

	fatal("daemon init failed ret=%zd err=%d", ret, err);
	exit(1);
}

static void ossp_init_done(void *userdata)
{
	/* init complete, notify parent if it's waiting */
	if (init_wait_fd >= 0) {
		ssize_t ret;
		int err = 0;

		ret = write(init_wait_fd, &err, sizeof(err));
		if (ret != sizeof(err))
			fatal_e(-errno, "failed to notify init completion, "
				"ret=%zd", ret);
		close(init_wait_fd);
		init_wait_fd = -1;
	}
}

static const struct cuse_lowlevel_ops mixer_ops = {
	.open			= mixer_open,
	.release		= mixer_release,
	.ioctl			= mixer_ioctl,
};

static const struct cuse_lowlevel_ops dsp_ops = {
	.init_done		= ossp_init_done,
	.open			= dsp_open,
	.release		= dsp_release,
	.read			= dsp_read,
	.write			= dsp_write,
	.poll			= dsp_poll,
	.ioctl			= dsp_ioctl,
#ifdef OSSP_MMAP
	.mmap			= dsp_mmap,
	.munmap			= dsp_munmap,
#endif
};

static const struct cuse_lowlevel_ops adsp_ops = {
	.open			= adsp_open,
	.release		= dsp_release,
	.read			= dsp_read,
	.write			= dsp_write,
	.poll			= dsp_poll,
	.ioctl			= dsp_ioctl,
#ifdef OSSP_MMAP
	.mmap			= dsp_mmap,
	.munmap			= dsp_munmap,
#endif
};

static const char *usage =
"usage: osspd [options]\n"
"\n"
"options:\n"
"    --help            print this help message\n"
"    --dsp=NAME        DSP device name (default dsp)\n"
"    --dsp-maj=MAJ     DSP device major number (default 14)\n"
"    --dsp-min=MIN     DSP device minor number (default 3)\n"
"    --adsp=NAME       Aux DSP device name (default adsp, blank to disable)\n"
"    --adsp-maj=MAJ    Aux DSP device major number (default 14)\n"
"    --adsp-min=MIN    Aux DSP device minor number (default 12)\n"
"    --mixer=NAME      mixer device name (default mixer, blank to disable)\n"
"    --mixer-maj=MAJ   mixer device major number (default 14)\n"
"    --mixer-min=MIN   mixer device minor number (default 0)\n"
"    --max=MAX         maximum number of open streams (default 256)\n"
"    --umax=MAX        maximum number of open streams per UID (default --max)\n"
"    --exit-on-idle    exit if idle\n"
"    --dsp-slave=PATH  DSP slave (default ossp-padsp in the same dir)\n"
"    --log=LEVEL       log level (0..6)\n"
"    --timestamp       timestamp log messages\n"
"    -v                increase verbosity, can be specified multiple times\n"
"    -f                Run in foreground (don't daemonize)\n"
"\n";

struct ossp_param {
	char			*dsp_name;
	unsigned		dsp_major;
	unsigned		dsp_minor;
	char			*adsp_name;
	unsigned		adsp_major;
	unsigned		adsp_minor;
	char			*mixer_name;
	unsigned		mixer_major;
	unsigned		mixer_minor;
	unsigned		max_streams;
	unsigned		umax_streams;
	char			*dsp_slave_path;
	unsigned		log_level;
	int			exit_on_idle;
	int			timestamp;
	int			fg;
	int			help;
};

#define OSSP_OPT(t, p) { t, offsetof(struct ossp_param, p), 1 }

static const struct fuse_opt ossp_opts[] = {
	OSSP_OPT("--dsp=%s",		dsp_name),
	OSSP_OPT("--dsp-maj=%u",	dsp_major),
	OSSP_OPT("--dsp-min=%u",	dsp_minor),
	OSSP_OPT("--adsp=%s",		adsp_name),
	OSSP_OPT("--adsp-maj=%u",	adsp_major),
	OSSP_OPT("--adsp-min=%u",	adsp_minor),
	OSSP_OPT("--mixer=%s",		mixer_name),
	OSSP_OPT("--mixer-maj=%u",	mixer_major),
	OSSP_OPT("--mixer-min=%u",	mixer_minor),
	OSSP_OPT("--max=%u",		max_streams),
	OSSP_OPT("--umax=%u",		umax_streams),
	OSSP_OPT("--exit-on-idle",	exit_on_idle),
	OSSP_OPT("--dsp-slave=%s",	dsp_slave_path),
	OSSP_OPT("--timestamp",		timestamp),
	OSSP_OPT("--log=%u",		log_level),
	OSSP_OPT("-f",			fg),
	FUSE_OPT_KEY("-h",		0),
	FUSE_OPT_KEY("--help",		0),
	FUSE_OPT_KEY("-v",		1),
	FUSE_OPT_END
};

static struct fuse_session *setup_ossp_cuse(const struct cuse_lowlevel_ops *ops,
					    const char *name, int major,
					    int minor, int argc, char **argv)
{
	char name_buf[128];
	const char *bufp = name_buf;
	struct cuse_info ci = { .dev_major = major, .dev_minor = minor,
				.dev_info_argc = 1, .dev_info_argv = &bufp,
				.flags = CUSE_UNRESTRICTED_IOCTL };
	struct fuse_session *se;
	int fd;

	snprintf(name_buf, sizeof(name_buf), "DEVNAME=%s", name);

	se = cuse_lowlevel_setup(argc, argv, &ci, ops, NULL, NULL);
	if (!se) {
		err("failed to setup %s CUSE", name);
		return NULL;
	}

	fd = fuse_chan_fd(fuse_session_next_chan(se, NULL));
	if (
#ifdef OSSP_MMAP
		fd != fuse_mmap_fd(se) &&
#endif
		fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
		err_e(-errno, "failed to set CLOEXEC on %s CUSE fd", name);
		cuse_lowlevel_teardown(se);
		return NULL;
	}

	return se;
}

static void *cuse_worker(void *arg)
{
	struct fuse_session *se = arg;
	int rc;

	rc = fuse_session_loop_mt(se);
	cuse_lowlevel_teardown(se);

	return (void *)(unsigned long)rc;
}

static int process_arg(void *data, const char *arg, int key,
		       struct fuse_args *outargs)
{
	struct ossp_param *param = data;

	switch (key) {
	case 0:
		fprintf(stderr, usage);
		param->help = 1;
		return 0;
	case 1:
		param->log_level++;
		return 0;
	}
	return 1;
}

int main(int argc, char **argv)
{
	static struct ossp_param param = {
		.dsp_name = DFL_DSP_NAME,
		.dsp_major = DFL_DSP_MAJOR, .dsp_minor = DFL_DSP_MINOR,
		.adsp_name = DFL_ADSP_NAME,
		.adsp_major = DFL_ADSP_MAJOR, .adsp_minor = DFL_ADSP_MINOR,
		.mixer_name = DFL_MIXER_NAME,
		.mixer_major = DFL_MIXER_MAJOR, .mixer_minor = DFL_MIXER_MINOR,
		.max_streams = DFL_MAX_STREAMS,
	};
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	char path_buf[PATH_MAX], *dir;
	char adsp_buf[64] = "", mixer_buf[64] = "";
	struct sigaction sa;
	struct stat stat_buf;
	ssize_t ret;
	unsigned u;

	snprintf(ossp_log_name, sizeof(ossp_log_name), "osspd");
	param.log_level = ossp_log_level;

	if (fuse_opt_parse(&args, &param, ossp_opts, process_arg))
		fatal("failed to parse arguments");

	if (param.help)
		return 0;

	max_streams = param.max_streams;
	hashtbl_size = max_streams / 2 + 13;

	umax_streams = max_streams;
	if (param.umax_streams)
		umax_streams = param.umax_streams;
	if (param.log_level > OSSP_LOG_MAX)
		param.log_level = OSSP_LOG_MAX;
	if (!param.fg)
		param.log_level = -param.log_level;
	ossp_log_level = param.log_level;
	ossp_log_timestamp = param.timestamp;

	if (!param.fg)
		ossp_daemonize();

	/* daemonization already handled, prevent forking inside FUSE */
	fuse_opt_add_arg(&args, "-f");

	info("OSS Proxy v%s (C) 2008-2010 by Tejun Heo <teheo@suse.de>",
	     OSSP_VERSION);

	/* ignore stupid SIGPIPEs */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &sa, NULL))
		fatal_e(-errno, "failed to ignore SIGPIPE");

//#ifdef GIOVANNI
	/* determine slave path and check for availability */
	ret = readlink("/proc/self/exe", path_buf, PATH_MAX - 1);
	if (ret < 0)
		fatal_e(-errno, "failed to determine executable path");
	path_buf[ret] = '\0';
	dir = dirname(path_buf);

	if (param.dsp_slave_path) {
		strncpy(dsp_slave_path, param.dsp_slave_path, PATH_MAX - 1);
		dsp_slave_path[PATH_MAX - 1] = '\0';
	} else {
		ret = snprintf(dsp_slave_path, PATH_MAX, "%s/%s",
			       dir, "ossp-padsp");
		if (ret >= PATH_MAX)
			fatal("dsp slave pathname too long");
	}

	if (stat(dsp_slave_path, &stat_buf))
		fatal_e(-errno, "failed to stat %s", dsp_slave_path);
	if (!S_ISREG(stat_buf.st_mode) || !(stat_buf.st_mode & 0444))
		fatal("%s is not executable", dsp_slave_path);

//#endif// GIOVANNI
	/* allocate tables */
	os_id_bitmap = calloc(BITS_TO_LONGS(max_streams), sizeof(long));
	mixer_tbl = calloc(hashtbl_size, sizeof(mixer_tbl[0]));
	os_tbl = calloc(hashtbl_size, sizeof(os_tbl[0]));
	os_pgrp_tbl = calloc(hashtbl_size, sizeof(os_pgrp_tbl[0]));
	os_notify_tbl = calloc(hashtbl_size, sizeof(os_notify_tbl[0]));
	if (!os_id_bitmap || !mixer_tbl || !os_tbl || !os_pgrp_tbl ||
	    !os_notify_tbl)
		fatal("failed to allocate stream hash tables");
	for (u = 0; u < hashtbl_size; u++) {
		INIT_LIST_HEAD(&mixer_tbl[u]);
		INIT_LIST_HEAD(&os_tbl[u]);
		INIT_LIST_HEAD(&os_pgrp_tbl[u]);
		INIT_LIST_HEAD(&os_notify_tbl[u]);
	}
	__set_bit(0, os_id_bitmap);	/* don't use id 0 */

	/* create mixer delayed reference worker */
	ret = -pthread_create(&mixer_delayed_put_thread, NULL,
			      mixer_delayed_put_worker, NULL);
	if (ret)
		fatal_e(ret, "failed to create mixer delayed put worker");

	/* if exit_on_idle, touch mixer for pgrp0 */
	exit_on_idle = param.exit_on_idle;
	if (exit_on_idle) {
		struct ossp_mixer *mixer;

		mixer = get_mixer(0);
		if (!mixer)
			fatal("failed to touch idle mixer");
		put_mixer(mixer);
	}

	/* create notify epoll and kick off watcher thread */
	notify_epfd = epoll_create(max_streams);
	if (notify_epfd < 0)
		fatal_e(-errno, "failed to create notify epoll");
	if (fcntl(notify_epfd, F_SETFD, FD_CLOEXEC) < 0)
		fatal_e(-errno, "failed to set CLOEXEC on notify epfd");

	ret = -pthread_create(&notify_poller_thread, NULL, notify_poller, NULL);
	if (ret)
		fatal_e(ret, "failed to create notify poller thread");

	/* create reaper for slave corpses */
	ret = -pthread_create(&slave_reaper_thread, NULL, slave_reaper, NULL);
	if (ret)
		fatal_e(ret, "failed to create slave reaper thread");

#ifdef GIOVANNI
	/* we're set, let's setup fuse structures */
	if (strlen(param.mixer_name))
		mixer_se = setup_ossp_cuse(&mixer_ops, param.mixer_name,
					   param.mixer_major, param.mixer_minor,
					   args.argc, args.argv);
	if (strlen(param.adsp_name))
		adsp_se = setup_ossp_cuse(&dsp_ops, param.adsp_name,
					  param.adsp_major, param.adsp_minor,
					  args.argc, args.argv);

#endif// GIOVANNI
	dsp_se = setup_ossp_cuse(&dsp_ops, param.dsp_name,
				 param.dsp_major, param.dsp_minor,
				 args.argc, args.argv);
	if (!dsp_se)
		fatal("can't create dsp, giving up");

#ifdef GIOVANNI
	if (mixer_se)
		snprintf(mixer_buf, sizeof(mixer_buf), ", %s (%d:%d)",
			 param.mixer_name, param.mixer_major, param.mixer_minor);
	if (adsp_se)
		snprintf(adsp_buf, sizeof(adsp_buf), ", %s (%d:%d)",
			 param.adsp_name, param.adsp_major, param.adsp_minor);

#endif// GIOVANNI
	info("Creating %s (%d:%d)%s%s", param.dsp_name, param.dsp_major,
	     param.dsp_minor, adsp_buf, mixer_buf);

#ifdef GIOVANNI
	/* start threads for mixer and adsp */
	if (mixer_se) {
		ret = -pthread_create(&cuse_mixer_thread, NULL,
				      cuse_worker, mixer_se);
		if (ret)
			err_e(ret, "failed to create mixer worker");
	}
	if (adsp_se) {
		ret = -pthread_create(&cuse_adsp_thread, NULL,
				      cuse_worker, adsp_se);
		if (ret)
			err_e(ret, "failed to create adsp worker");
	}
#endif// GIOVANNI

	/* run CUSE for /dev/dsp in the main thread */
	ret = (ssize_t)cuse_worker(dsp_se);
	if (ret < 0)
		fatal("dsp worker failed");
	return 0;
}
