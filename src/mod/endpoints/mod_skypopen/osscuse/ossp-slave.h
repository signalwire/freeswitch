/*
 * ossp-slave - OSS Proxy: Common codes for slaves
 *
 * Copyright (C) 2008-2010  SUSE Linux Products GmbH
 * Copyright (C) 2008-2010  Tejun Heo <tj@kernel.org>
 *
 * This file is released under the GPLv2.
 */

#ifndef _OSSP_SLAVE_H
#define _OSSP_SLAVE_H

#include "ossp.h"
#include "ossp-util.h"

#define OSSP_USER_NAME_LEN	128

extern char ossp_user_name[OSSP_USER_NAME_LEN];
extern int ossp_cmd_fd, ossp_notify_fd;
extern void *ossp_mmap_addr[2];

void ossp_slave_init(int argc, char **argv);
int ossp_slave_process_command(int cmd_fd,
			       ossp_action_fn_t const *action_fn_tbl,
			       int (*action_pre_fn)(void),
			       void (*action_post_fn)(void));

#endif /* _OSSP_SLAVE_H */
