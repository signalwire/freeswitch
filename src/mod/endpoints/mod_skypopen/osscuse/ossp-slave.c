/*
 * ossp-slave - OSS Proxy: Common codes for slaves
 *
 * Copyright (C) 2008-2010  SUSE Linux Products GmbH
 * Copyright (C) 2008-2010  Tejun Heo <tj@kernel.org>
 *
 * This file is released under the GPLv2.
 */

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>

#include "ossp-slave.h"

static const char *usage =
"usage: ossp-SLAVE [options]\n"
"\n"
"proxies commands from osspd to pulseaudio\n"
"\n"
"options:\n"
"    -u UID            uid to use\n"
"    -g GID            gid to use\n"
"    -c CMD_FD         fd to receive commands from osspd\n"
"    -n NOTIFY_FD      fd to send async notifications to osspd\n"
"    -m MMAP_FD        fd to use for mmap\n"
"    -o MMAP_OFFSET    mmap offset\n"
"    -s MMAP_SIZE      mmap size\n"
"    -l LOG_LEVEL      set log level\n"
"    -t                enable log timestamps\n";

char ossp_user_name[OSSP_USER_NAME_LEN];
int ossp_cmd_fd = -1, ossp_notify_fd = -1;
void *ossp_mmap_addr[2];

void ossp_slave_init(int argc, char **argv)
{
	int have_uid = 0, have_gid = 0;
	uid_t uid;
	gid_t gid;
	int mmap_fd = -1;
	off_t mmap_off = 0;
	size_t mmap_size = 0;
	int opt;
	struct passwd *pw, pw_buf;
	struct sigaction sa;
	char pw_sbuf[sysconf(_SC_GETPW_R_SIZE_MAX)];

	while ((opt = getopt(argc, argv, "u:g:c:n:m:o:s:l:t")) != -1) {
		switch (opt) {
		case 'u':
			have_uid = 1;
			uid = strtol(optarg, NULL, 0);
			break;
		case 'g':
			have_gid = 1;
			gid = strtol(optarg, NULL, 0);
			break;
		case 'c':
			ossp_cmd_fd = strtol(optarg, NULL, 0);
			break;
		case 'n':
			ossp_notify_fd = strtol(optarg, NULL, 0);
			break;
		case 'm':
			mmap_fd = strtol(optarg, NULL, 0);
			break;
		case 'o':
			mmap_off = strtoull(optarg, NULL, 0);
			break;
		case 's':
			mmap_size = strtoul(optarg, NULL, 0);
			break;
		case 'l':
			ossp_log_level = strtol(optarg, NULL, 0);
			break;
		case 't':
			ossp_log_timestamp = 1;
			break;
		}
	}

	if (!have_uid || !have_gid || ossp_cmd_fd < 0 || ossp_notify_fd < 0) {
		fprintf(stderr, usage);
		_exit(1);
	}

	snprintf(ossp_user_name, sizeof(ossp_user_name), "uid%d", uid);
	if (getpwuid_r(uid, &pw_buf, pw_sbuf, sizeof(pw_sbuf), &pw) == 0)
		snprintf(ossp_user_name, sizeof(ossp_user_name), "%s",
			 pw->pw_name);

	snprintf(ossp_log_name, sizeof(ossp_log_name), "ossp-padsp[%s:%d]",
		 ossp_user_name, getpid());

	if (mmap_fd >= 0) {
		void *p;

		if (!mmap_off || !mmap_size) {
			fprintf(stderr, usage);
			_exit(1);
		}

		p = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED,
			 mmap_fd, mmap_off);
		if (p == MAP_FAILED)
			fatal_e(-errno, "mmap failed");

		ossp_mmap_addr[PLAY] = p;
		ossp_mmap_addr[REC] = p + mmap_size / 2;
		close(mmap_fd);
	}

	/* mmap done, drop privileges */
	if (setresgid(gid, gid, gid) || setresuid(uid, uid, uid))
		fatal_e(-errno, "failed to drop privileges");

	/* block SIGPIPE */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &sa, NULL))
		fatal_e(-errno, "failed to ignore SIGPIPE");
}

int ossp_slave_process_command(int cmd_fd,
			       ossp_action_fn_t const *action_fn_tbl,
			       int (*action_pre_fn)(void),
			       void (*action_post_fn)(void))
{
	static struct sized_buf carg_sbuf = { }, rarg_sbuf = { };
	static struct sized_buf din_sbuf = { }, dout_sbuf = { };
	struct ossp_cmd cmd;
	int fd = -1;
	char cmsg_buf[CMSG_SPACE(sizeof(fd))];
	struct iovec iov = { &cmd, sizeof(cmd) };
	struct msghdr msg = { .msg_iov = &iov, .msg_iovlen = 1,
			      .msg_control = cmsg_buf,
			      .msg_controllen = sizeof(cmsg_buf) };
	struct cmsghdr *cmsg;
	size_t carg_size, din_size, rarg_size, dout_size;
	char *carg = NULL, *din = NULL, *rarg = NULL, *dout = NULL;
	struct ossp_reply reply = { .magic = OSSP_REPLY_MAGIC };
	ssize_t ret;

	ret = recvmsg(cmd_fd, &msg, 0);
	if (ret == 0)
		return 0;
	if (ret < 0) {
		ret = -errno;
		err_e(ret, "failed to read command channel");
		return ret;
	}

	if (ret != sizeof(cmd)) {
		err("command struct size mismatch (%zu, should be %zu)",
		    ret, sizeof(cmd));
		return -EINVAL;
	}

	if (cmd.magic != OSSP_CMD_MAGIC) {
		err("illegal command magic 0x%x", cmd.magic);
		return -EINVAL;
	}

	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg;
	     cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_level == SOL_SOCKET &&
		    cmsg->cmsg_type == SCM_RIGHTS)
			fd = *(int *)CMSG_DATA(cmsg);
		else {
			err("unknown cmsg %d:%d received (opcode %d)",
			    cmsg->cmsg_level, cmsg->cmsg_type, cmd.opcode);
			return -EINVAL;
		}
	}

	if (cmd.opcode >= OSSP_NR_OPCODES) {
		err("unknown opcode %d", cmd.opcode);
		return -EINVAL;
	}

	carg_size = ossp_arg_sizes[cmd.opcode].carg_size;
	din_size = cmd.din_size;
	rarg_size = ossp_arg_sizes[cmd.opcode].rarg_size;
	dout_size = cmd.dout_size;

	if ((fd >= 0) != ossp_arg_sizes[cmd.opcode].has_fd) {
		err("fd=%d unexpected for opcode %d", fd, cmd.opcode);
		return -EINVAL;
	}

	if (ensure_sbuf_size(&carg_sbuf, carg_size) ||
	    ensure_sbuf_size(&din_sbuf, din_size) ||
	    ensure_sbuf_size(&rarg_sbuf, rarg_size) ||
	    ensure_sbuf_size(&dout_sbuf, dout_size)) {
		err("failed to allocate command buffers");
		return -ENOMEM;
	}

	if (carg_size) {
		carg = carg_sbuf.buf;
		ret = read_fill(cmd_fd, carg, carg_size);
		if (ret < 0)
			return ret;
	}
	if (din_size) {
		din = din_sbuf.buf;
		ret = read_fill(cmd_fd, din, din_size);
		if (ret < 0)
			return ret;
	}
	if (rarg_size)
		rarg = rarg_sbuf.buf;
	if (dout_size)
		dout = dout_sbuf.buf;

	ret = -EINVAL;
	if (action_fn_tbl[cmd.opcode]) {
		ret = action_pre_fn();
		if (ret == 0) {
			ret = action_fn_tbl[cmd.opcode](cmd.opcode, carg,
							din, din_size, rarg,
							dout, &dout_size, fd);
			action_post_fn();
		}
	}

	reply.result = ret;
	if (ret >= 0)
		reply.dout_size = dout_size;
	else {
		rarg_size = 0;
		dout_size = 0;
	}

	if (write_fill(cmd_fd, &reply, sizeof(reply)) < 0 ||
	    write_fill(cmd_fd, rarg, rarg_size) < 0 ||
	    write_fill(cmd_fd, dout, dout_size) < 0)
		return -EIO;

	return 1;
}
