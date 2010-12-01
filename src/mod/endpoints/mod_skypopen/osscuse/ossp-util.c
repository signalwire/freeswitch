/*
 * ossp-util - OSS Proxy: Common utilities
 *
 * Copyright (C) 2008-2010  SUSE Linux Products GmbH
 * Copyright (C) 2008-2010  Tejun Heo <tj@kernel.org>
 *
 * This file is released under the GPLv2.
 */

#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <syslog.h>
#include <unistd.h>
#include "ossp-util.h"

#define BIT(nr)			(1UL << (nr))
#define BIT_MASK(nr)		(1UL << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr)		((nr) / BITS_PER_LONG)
#define BITS_TO_LONGS(nr)	DIV_ROUND_UP(nr, BITS_PER_BYTE * sizeof(long))
#define BITOP_WORD(nr)		((nr) / BITS_PER_LONG)

char ossp_log_name[OSSP_LOG_NAME_LEN];
int ossp_log_level = OSSP_LOG_DFL;
int ossp_log_timestamp;

static const char *severity_strs[] = {
	[OSSP_LOG_CRIT]		= "CRIT",
	[OSSP_LOG_ERR]		= " ERR",
	[OSSP_LOG_WARN]		= "WARN",
	[OSSP_LOG_INFO]		= NULL,
	[OSSP_LOG_DBG0]		= "DBG0",
	[OSSP_LOG_DBG1]		= "DBG1",
};

static int severity_map[] = {
	[OSSP_LOG_CRIT]		= LOG_ERR,
	[OSSP_LOG_ERR]		= LOG_ERR,
	[OSSP_LOG_WARN]		= LOG_WARNING,
	[OSSP_LOG_INFO]		= LOG_INFO,
	[OSSP_LOG_DBG0]		= LOG_DEBUG,
	[OSSP_LOG_DBG1]		= LOG_DEBUG,
};

void log_msg(int severity, const char *fmt, ...)
{
	static int syslog_opened = 0;
	char buf[1024];
	size_t len = sizeof(buf), off = 0;
	va_list ap;

	if (severity > abs(ossp_log_level))
		return;

	if (ossp_log_level < 0 && !syslog_opened)
		openlog(ossp_log_name, 0, LOG_DAEMON);

	assert(severity >= 0 && severity < ARRAY_SIZE(severity_strs));

	if (ossp_log_timestamp) {
		static uint64_t start;
		uint64_t now;
		struct timeval tv;
		gettimeofday(&tv, NULL);
		now = tv.tv_sec * 1000 + tv.tv_usec / 1000;
		if (!start)
			start = now;

		off += snprintf(buf + off, len - off, "<%08"PRIu64"> ",
				now - start);
	}

	if (ossp_log_level > 0) {
		char sev_buf[16] = "";
		if (severity_strs[severity])
			snprintf(sev_buf, sizeof(sev_buf), " %s",
				 severity_strs[severity]);
		off += snprintf(buf + off, len - off, "%s%s: ",
				ossp_log_name, sev_buf);
	} else if (severity_strs[severity])
		off += snprintf(buf + off, len - off, "%s ",
				severity_strs[severity]);

	va_start(ap, fmt);
	off += vsnprintf(buf + off, len - off, fmt, ap);
	va_end(ap);

	off += snprintf(buf + off, len - off, "\n");

	if (ossp_log_level > 0)
		fputs(buf, stderr);
	else
		syslog(severity_map[severity], "%s", buf);
}

int read_fill(int fd, void *buf, size_t size)
{
	while (size) {
		ssize_t ret;
		int rc;

		ret = read(fd, buf, size);
		if (ret <= 0) {
			if (ret == 0)
				rc = -EIO;
			else
				rc = -errno;
			err_e(rc, "failed to read_fill %zu bytes from fd %d",
			      size, fd);
			return rc;
		}
		buf += ret;
		size -= ret;
	}
	return 0;
}

int write_fill(int fd, const void *buf, size_t size)
{
	while (size) {
		ssize_t ret;
		int rc;

		ret = write(fd, buf, size);
		if (ret <= 0) {
			if (ret == 0)
				rc = -EIO;
			else
				rc = -errno;
			err_e(rc, "failed to write_fill %zu bytes to fd %d",
			      size, fd);
			return rc;
		}
		buf += ret;
		size -= ret;
	}
	return 0;
}

void ring_fill(struct ring_buf *ring, const void *buf, size_t size)
{
	size_t tail;

	assert(ring_space(ring) >= size);

	tail = (ring->head + ring->size - ring->bytes) % ring->size;

	if (ring->head >= tail) {
		size_t todo = min(size, ring->size - ring->head);

		memcpy(ring->buf + ring->head, buf, todo);
		ring->head = (ring->head + todo) % ring->size;
		ring->bytes += todo;
		buf += todo;
		size -= todo;
	}

	assert(ring->size - ring->head >= size);
	memcpy(ring->buf + ring->head, buf, size);
	ring->head += size;
	ring->bytes += size;
}

void *ring_data(struct ring_buf *ring, size_t *sizep)
{
	size_t tail;

	if (!ring->bytes)
		return NULL;

	tail = (ring->head + ring->size - ring->bytes) % ring->size;

	*sizep = min(ring->bytes, ring->size - tail);
	return ring->buf + tail;
}

int ring_resize(struct ring_buf *ring, size_t new_size)
{
	struct ring_buf new_ring = { .size = new_size };
	void *p;
	size_t size;

	if (ring_bytes(ring) > new_size)
		return -ENOSPC;

	new_ring.buf = calloc(1, new_size);
	if (new_size && !new_ring.buf)
		return -ENOMEM;

	while ((p = ring_data(ring, &size))) {
		ring_fill(&new_ring, p, size);
		ring_consume(ring, size);
	}

	free(ring->buf);
	*ring = new_ring;
	return 0;
}

int ensure_sbuf_size(struct sized_buf *sbuf, size_t size)
{
	char *new_buf;

	if (sbuf->size >= size)
		return 0;

	new_buf = realloc(sbuf->buf, size);
	if (size && !new_buf)
		return -ENOMEM;

	sbuf->buf = new_buf;
	sbuf->size = size;
	return 0;
}

static unsigned long __ffs(unsigned long word)
{
	int num = 0;

	if (BITS_PER_LONG == 64) {
		if ((word & 0xffffffff) == 0) {
			num += 32;
			word >>= 32;
		}
	}

	if ((word & 0xffff) == 0) {
		num += 16;
		word >>= 16;
	}
	if ((word & 0xff) == 0) {
		num += 8;
		word >>= 8;
	}
	if ((word & 0xf) == 0) {
		num += 4;
		word >>= 4;
	}
	if ((word & 0x3) == 0) {
		num += 2;
		word >>= 2;
	}
	if ((word & 0x1) == 0)
		num += 1;
	return num;
}

#define ffz(x)  __ffs(~(x))

unsigned long find_next_zero_bit(const unsigned long *addr, unsigned long size,
				 unsigned long offset)
{
	const unsigned long *p = addr + BITOP_WORD(offset);
	unsigned long result = offset & ~(BITS_PER_LONG-1);
	unsigned long tmp;

	if (offset >= size)
		return size;
	size -= result;
	offset %= BITS_PER_LONG;
	if (offset) {
		tmp = *(p++);
		tmp |= ~0UL >> (BITS_PER_LONG - offset);
		if (size < BITS_PER_LONG)
			goto found_first;
		if (~tmp)
			goto found_middle;
		size -= BITS_PER_LONG;
		result += BITS_PER_LONG;
	}
	while (size & ~(BITS_PER_LONG-1)) {
		if (~(tmp = *(p++)))
			goto found_middle;
		result += BITS_PER_LONG;
		size -= BITS_PER_LONG;
	}
	if (!size)
		return result;
	tmp = *p;

found_first:
	tmp |= ~0UL << size;
	if (tmp == ~0UL)	/* Are any bits zero? */
		return result + size;	/* Nope. */
found_middle:
	return result + ffz(tmp);
}

void __set_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);

	*p  |= mask;
}

void __clear_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);

	*p &= ~mask;
}

int get_proc_self_info(pid_t pid, pid_t *ppid_r,
		       char *cmd_buf, size_t cmd_buf_sz)

{
	char path[64], buf[4096];
	int fd = -1;
	char *cmd_start, *cmd_end, *ppid_start, *end;
	ssize_t ret;
	pid_t ppid;
	int i, rc;

	snprintf(path, sizeof(path), "/proc/%ld/stat", (long)pid);
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		rc = -errno;
		goto out;
	}

	ret = read(fd, buf, sizeof(buf));
	if (ret < 0)
		goto out;
	if (ret == sizeof(buf)) {
		rc = -EOVERFLOW;
		goto out;
	}
	buf[ret] = '\0';

	rc = -EINVAL;
	cmd_start = strchr(buf, '(');
	cmd_end = strrchr(buf, ')');
	if (!cmd_start || !cmd_end)
		goto out;
	cmd_start++;

	ppid_start = cmd_end;
	for (i = 0; i < 3; i++) {
		ppid_start = strchr(ppid_start, ' ');
		if (!ppid_start)
			goto out;
		ppid_start++;
	}

	ppid = strtoul(ppid_start, &end, 10);
	if (end == ppid_start || *end != ' ')
		goto out;

	if (ppid_r)
		*ppid_r = ppid;
	if (cmd_buf) {
		size_t len = min_t(size_t, cmd_end - cmd_start, cmd_buf_sz - 1);
		memcpy(cmd_buf, cmd_start, len);
		cmd_buf[len] = '\0';
	}

	rc = 0;
 out:
	close(fd);

	return rc;
}
