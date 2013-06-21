/*
 * skypopen.h -- definitions for the char module
 *
 * Copyright (C) 2010 Giovanni Maruzzelli
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
 * $Id: skypopen.h,v 1.15 2004/11/04 17:51:18 rubini Exp $
 */

#ifndef _SKYPOPEN_H_
#define _SKYPOPEN_H_

#include <linux/version.h>
#include <linux/ioctl.h> /* needed for the _IOW etc stuff used later */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
#include <asm/switch_to.h>		/* cli(), *_flags */
#else
#include <asm/system.h>		/* cli(), *_flags */
#endif //LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)


#if LINUX_VERSION_CODE == KERNEL_VERSION(2, 6, 18)
#define CENTOS_5 
#define WANT_HRTIMER /* undef this only if you don't want to use High Resolution Timers (why?) */
#endif /* CentOS 5.x */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
#define WANT_HRTIMER 
#endif /* HRTIMER */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
#define WANT_DEFINE_SPINLOCK 
#endif /* DEFINE_SPINLOCK */

#define SKYPOPEN_BLK 1920
#define SKYPOPEN_SLEEP 20


#define SKYPOPEN_MAJOR 14   /* dynamic major by default */
#define SKYPOPEN_MINOR 3   /* dynamic major by default */
#define SKYPOPEN_NR_DEVS 1    /* not useful, I'm too lazy to remove it */

#ifdef CENTOS_5
#define HRTIMER_MODE_REL HRTIMER_REL
#endif// CENTOS_5

struct skypopen_dev {
	struct cdev cdev;	  /* Char device structure		*/
	wait_queue_head_t inq; /* read and write queues */
	wait_queue_head_t outq; /* read and write queues */
#ifndef WANT_HRTIMER 
	struct timer_list timer_inq;
	struct timer_list timer_outq;
#else// WANT_HRTIMER 
	struct hrtimer timer_inq;
	struct hrtimer timer_outq;
#endif// WANT_HRTIMER 
	int timer_inq_started;
	int timer_outq_started;
	int opened;
};


/*
 * The different configurable parameters
 */
extern int skypopen_major;     /* main.c */
extern int skypopen_nr_devs;

#endif /* _SKYPOPEN_H_ */
