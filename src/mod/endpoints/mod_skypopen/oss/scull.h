/*
 * scull.h -- definitions for the char module
 *
 * Copyright (C) 2001 Alessandro Rubini and Jonathan Corbet
 * Copyright (C) 2001 O'Reilly & Associates
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.  The citation
 * should list that the code comes from the book "Linux Device
 * Drivers" by Alessandro Rubini and Jonathan Corbet, published
 * by O'Reilly & Associates.   No warranty is attached;
 * we cannot take responsibility for errors or fitness for use.
 *
 * $Id: scull.h,v 1.15 2004/11/04 17:51:18 rubini Exp $
 */

#ifndef _SCULL_H_
#define _SCULL_H_

#include <linux/ioctl.h> /* needed for the _IOW etc stuff used later */

#ifndef SCULL_MAJOR
#define SCULL_MAJOR 14   /* dynamic major by default */
#endif

#ifndef SCULL_NR_DEVS
#define SCULL_NR_DEVS 1    /* scull0 through scull3 */
#endif

struct scull_dev {
	struct cdev cdev;	  /* Char device structure		*/
	wait_queue_head_t inq; /* read and write queues */
	wait_queue_head_t outq; /* read and write queues */
	struct timer_list timer_inq;
	struct timer_list timer_outq;
	int readable;
	int writable;
	//unsigned long read_howmany;
	//unsigned long write_howmany;
	//unsigned long read_sleeped_acc;
	//unsigned long write_sleeped_acc;
	//double read_delay; /* how much delay last time */
	//double write_delay; /* how much delay last time */
};


/*
 * The different configurable parameters
 */
extern int scull_major;     /* main.c */
extern int scull_nr_devs;


/*
 * Prototypes for shared functions
 */

ssize_t scull_read(struct file *filp, char __user *buf, size_t count,
                   loff_t *f_pos);
ssize_t scull_write(struct file *filp, const char __user *buf, size_t count,
                    loff_t *f_pos);
int     scull_ioctl(struct inode *inode, struct file *filp,
                    unsigned int cmd, unsigned long arg);

#endif /* _SCULL_H_ */
