/*
 * main.c -- the bare skypopen char module
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
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>

#include <asm/uaccess.h>	/* copy_*_user */

#include <linux/soundcard.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>

#include "skypopen.h"		/* local definitions */


/*
 * Our parameters which can be set at load time.
 */

int skypopen_major =   SKYPOPEN_MAJOR;
int skypopen_minor =   SKYPOPEN_MINOR;
int skypopen_nr_devs = SKYPOPEN_NR_DEVS;	/* number of bare skypopen devices */

module_param(skypopen_major, int, S_IRUGO);
module_param(skypopen_minor, int, S_IRUGO);
module_param(skypopen_nr_devs, int, S_IRUGO);

MODULE_AUTHOR("Original: Alessandro Rubini, Jonathan Corbet. Modified by: Giovanni Maruzzelli for FreeSWITCH skypopen");
MODULE_LICENSE("Dual BSD/GPL");

static struct skypopen_dev *skypopen_devices;	/* allocated in skypopen_init_module */

static int unload = 0;

#ifndef WANT_HRTIMER
void my_timer_callback_inq( unsigned long data )
{
	struct skypopen_dev *dev = (void *)data;

	wake_up_interruptible(&dev->inq);
	mod_timer( &dev->timer_inq, jiffies + msecs_to_jiffies(SKYPOPEN_SLEEP) );

}

void my_timer_callback_outq( unsigned long data )
{
	struct skypopen_dev *dev = (void *)data;

	wake_up_interruptible(&dev->outq);
	mod_timer( &dev->timer_outq, jiffies + msecs_to_jiffies(SKYPOPEN_SLEEP) );
}
#else// WANT_HRTIMER

#ifndef CENTOS_5
static enum hrtimer_restart my_hrtimer_callback_inq( struct hrtimer *timer_inq )
{
	struct skypopen_dev *dev = container_of(timer_inq, struct skypopen_dev, timer_inq);

	if(unload)
		return HRTIMER_NORESTART;

	hrtimer_forward(&dev->timer_inq, timer_inq->_softexpires, ktime_set(0, SKYPOPEN_SLEEP * 1000000));
	wake_up_interruptible(&dev->inq);

	return HRTIMER_RESTART;
}
static enum hrtimer_restart my_hrtimer_callback_outq( struct hrtimer *timer_outq )
{
	struct skypopen_dev *dev = container_of(timer_outq, struct skypopen_dev, timer_outq);

	if(unload)
		return HRTIMER_NORESTART;

	hrtimer_forward(&dev->timer_outq, timer_outq->_softexpires, ktime_set(0, SKYPOPEN_SLEEP * 1000000));
	wake_up_interruptible(&dev->outq);

	return HRTIMER_RESTART;
}
#else// CENTOS_5
static int my_hrtimer_callback_inq( struct hrtimer *timer_inq )
{
	struct skypopen_dev *dev = container_of(timer_inq, struct skypopen_dev, timer_inq);

	if(unload)
		return HRTIMER_NORESTART;

	hrtimer_forward(&dev->timer_inq, timer_inq->expires, ktime_set(0, SKYPOPEN_SLEEP * 1000000));
	wake_up_interruptible(&dev->inq);

	return HRTIMER_RESTART;
}
static int my_hrtimer_callback_outq( struct hrtimer *timer_outq )
{
	struct skypopen_dev *dev = container_of(timer_outq, struct skypopen_dev, timer_outq);

	if(unload)
		return HRTIMER_NORESTART;

	hrtimer_forward(&dev->timer_outq, timer_outq->expires, ktime_set(0, SKYPOPEN_SLEEP * 1000000));
	wake_up_interruptible(&dev->outq);

	return HRTIMER_RESTART;
}
#endif// CENTOS_5
#endif// WANT_HRTIMER

/* The clone-specific data structure includes a key field */

struct skypopen_listitem {
	struct skypopen_dev device;
	dev_t key;
	struct list_head list;

};

/* The list of devices, and a lock to protect it */
static LIST_HEAD(skypopen_c_list);
#ifdef WANT_DEFINE_SPINLOCK
static DEFINE_SPINLOCK(skypopen_c_lock);
#else // WANT_DEFINE_SPINLOCK
static spinlock_t skypopen_c_lock = SPIN_LOCK_UNLOCKED;
#endif // WANT_DEFINE_SPINLOCK

/* Look for a device or create one if missing */
static struct skypopen_dev *skypopen_c_lookfor_device(dev_t key)
{
	struct skypopen_listitem *lptr;
#ifdef WANT_HRTIMER
#endif// WANT_HRTIMER

	list_for_each_entry(lptr, &skypopen_c_list, list) {
		if (lptr->key == key)
			return &(lptr->device);
	}

	/* not found */
	lptr = kmalloc(sizeof(struct skypopen_listitem), GFP_KERNEL);
	if (!lptr)
		return NULL;

	/* initialize the device */
	memset(lptr, 0, sizeof(struct skypopen_listitem));
	lptr->key = key;

	init_waitqueue_head(&lptr->device.inq);
	init_waitqueue_head(&lptr->device.outq);

#ifndef WANT_HRTIMER
	setup_timer( &lptr->device.timer_inq, my_timer_callback_inq, (long int)lptr );
	setup_timer( &lptr->device.timer_outq, my_timer_callback_outq, (long int)lptr );
	printk( "Starting skypopen OSS driver read timer (%dms) skype client:(%d)\n", SKYPOPEN_SLEEP, current->tgid );
	mod_timer( &lptr->device.timer_inq, jiffies + msecs_to_jiffies(SKYPOPEN_SLEEP) );
	printk( "Starting skypopen OSS driver write timer (%dms) skype client:(%d)\n", SKYPOPEN_SLEEP, current->tgid );
	mod_timer( &lptr->device.timer_outq, jiffies + msecs_to_jiffies(SKYPOPEN_SLEEP) );
#endif// WANT_HRTIMER

	/* place it in the list */
	list_add(&lptr->list, &skypopen_c_list);

	return &(lptr->device);
}

/*
 * Open and close
 */
static int skypopen_c_open(struct inode *inode, struct file *filp)
{
	struct skypopen_dev *dev;
	dev_t key;

	key = current->tgid;

	/* look for a skypopenc device in the list */
	spin_lock(&skypopen_c_lock);
	dev = skypopen_c_lookfor_device(key);
	if (dev){
		dev->opened++;
	}
	spin_unlock(&skypopen_c_lock);

	if (!dev)
		return -ENOMEM;

	/* then, everything else is copied from the bare skypopen device */
	filp->private_data = dev;
	return 0;          /* success */
}

static int skypopen_c_release(struct inode *inode, struct file *filp)
{
	dev_t key;
	struct skypopen_dev *dev = filp->private_data;
	int ret;

	key = current->tgid;

	spin_lock(&skypopen_c_lock);
	dev->opened--;
	spin_unlock(&skypopen_c_lock);

	if(!dev->opened){
#ifdef WANT_HRTIMER
		if(dev->timer_inq_started){
			ret = hrtimer_cancel( &dev->timer_inq );
			//printk( "Stopped skypopen OSS driver read HRtimer skype client:(%d) ret=%d\n", key, ret);
			dev->timer_inq_started=0;
		}
		if(dev->timer_outq_started){
			ret = hrtimer_cancel( &dev->timer_outq );
			//printk( "Stopped skypopen OSS driver write HRtimer skype client:(%d) ret=%d\n", key, ret);
			dev->timer_outq_started=0;
		}
#endif// WANT_HRTIMER
	}
	return 0;
}



/*************************************************************/

static ssize_t skypopen_read(struct file *filp, char __user *buf, size_t count,
		loff_t *f_pos)
{
	DEFINE_WAIT(wait);
	struct skypopen_dev *dev = filp->private_data;
	dev_t key;

	key = current->tgid;

	if(unload)
		return -1;

#ifdef WANT_HRTIMER
	if(dev->timer_inq_started == 0){
		ktime_t ktime_inq;

		ktime_inq = ktime_set( 0, SKYPOPEN_SLEEP * 1000000);
		hrtimer_init( &dev->timer_inq, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
		dev->timer_inq.function = &my_hrtimer_callback_inq;
		hrtimer_start( &dev->timer_inq, ktime_inq, HRTIMER_MODE_REL );
		dev->timer_inq_started = 1;
		//printk( "Started skypopen OSS driver read HRtimer skype client:(%d) \n", key);
	}
#endif// WANT_HRTIMER

	//printk("READ\n");
	prepare_to_wait(&dev->inq, &wait, TASK_INTERRUPTIBLE);
	schedule();
	finish_wait(&dev->inq, &wait);
	return count;
}

static ssize_t skypopen_write(struct file *filp, const char __user *buf, size_t count,
		loff_t *f_pos)
{
	DEFINE_WAIT(wait);
	struct skypopen_dev *dev = filp->private_data;
	dev_t key;

	key = current->tgid;

	if(unload)
		return -1;

#ifdef WANT_HRTIMER
	if(dev->timer_outq_started == 0){
		ktime_t ktime_outq;

		ktime_outq = ktime_set( 0, SKYPOPEN_SLEEP * 1000000);
		hrtimer_init( &dev->timer_outq, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
		dev->timer_outq.function = &my_hrtimer_callback_outq;
		hrtimer_start( &dev->timer_outq, ktime_outq, HRTIMER_MODE_REL );
		dev->timer_outq_started = 1;
		//printk( "Started skypopen OSS driver write HRtimer skype client:(%d) \n", key);
	}
#endif// WANT_HRTIMER

	//printk("WRITE\n");
	prepare_to_wait(&dev->outq, &wait, TASK_INTERRUPTIBLE);
	schedule();
	finish_wait(&dev->outq, &wait);
	return count;

}
/*
 * The ioctl() implementation
 */

#ifndef HAVE_UNLOCKED_IOCTL
static int skypopen_ioctl(struct inode *inode, struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int __user *p = argp;

	switch (cmd) {
		case OSS_GETVERSION:
			return put_user(SOUND_VERSION, p);
		case SNDCTL_DSP_GETBLKSIZE:
			return put_user(SKYPOPEN_BLK, p);
		case SNDCTL_DSP_GETFMTS:
			return put_user(28731, p);

		default:
			return 0;
	}

}
#else// HAVE_UNLOCKED_IOCTL
static long skypopen_unlocked_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int __user *p = argp;

	switch (cmd) {
		case OSS_GETVERSION:
			return put_user(SOUND_VERSION, p);
		case SNDCTL_DSP_GETBLKSIZE:
			return put_user(SKYPOPEN_BLK, p);
		case SNDCTL_DSP_GETFMTS:
			return put_user(28731, p);

		default:
			return 0;
	}

}
#endif// HAVE_UNLOCKED_IOCTL

struct file_operations skypopen_fops = {
	.owner =    THIS_MODULE,
	.llseek =   no_llseek,
	.read =     skypopen_read,
	.write =    skypopen_write,
#ifndef HAVE_UNLOCKED_IOCTL
	.ioctl =    skypopen_ioctl,
#else// HAVE_UNLOCKED_IOCTL
	.unlocked_ioctl =    skypopen_unlocked_ioctl,
#endif// HAVE_UNLOCKED_IOCTL
	.open =     skypopen_c_open,
	.release =  skypopen_c_release,
};

/*
 * Finally, the module stuff
 */

/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */

void skypopen_cleanup_module(void)
{
	int i;
	int ret;
	struct skypopen_listitem *lptr, *next;
	dev_t devno = MKDEV(skypopen_major, skypopen_minor);


	unload = 1;

	msleep(100);

	/* Get rid of our char dev entries */
	if (skypopen_devices) {
		for (i = 0; i < skypopen_nr_devs; i++) {
			cdev_del(&skypopen_devices[i].cdev);
		}
		kfree(skypopen_devices);
	}

	/* And all the cloned devices */
	list_for_each_entry_safe(lptr, next, &skypopen_c_list, list) {
#ifndef WANT_HRTIMER
		ret= del_timer( &lptr->device.timer_inq );
		printk( "Stopped skypopen OSS driver read timer\n");
		ret= del_timer( &lptr->device.timer_outq );
		printk( "Stopped skypopen OSS driver write timer\n");
#else// WANT_HRTIMER
		if(lptr->device.timer_inq_started){
			ret = hrtimer_cancel( &lptr->device.timer_inq );
			printk( "Stopped skypopen OSS driver read HRtimer\n");
		}
		if(lptr->device.timer_outq_started){
			ret = hrtimer_cancel( &lptr->device.timer_outq );
			printk( "Stopped skypopen OSS driver write HRtimer\n");
		}

#endif// WANT_HRTIMER
		list_del(&lptr->list);
		kfree(lptr);
	}
	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, skypopen_nr_devs);
	printk("skypopen OSS driver unloaded\n");

}


/*
 * Set up the char_dev structure for this device.
 */
static void skypopen_setup_cdev(struct skypopen_dev *dev, int index)
{
	int err, devno = MKDEV(skypopen_major, skypopen_minor + index);

	cdev_init(&dev->cdev, &skypopen_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &skypopen_fops;
	err = cdev_add (&dev->cdev, devno, 1);
	/* Fail gracefully if need be */
	if (err)
		printk(KERN_NOTICE "Error %d adding skypopen%d", err, index);
}



int skypopen_init_module(void)
{
	int result, i;
	dev_t dev = 0;

	printk("skypopen OSS driver loading (www.freeswitch.org)\n");

	/*
	 * Get a range of minor numbers to work with, asking for a dynamic
	 * major unless directed otherwise at load time.
	 */
	if (skypopen_major) {
		dev = MKDEV(skypopen_major, skypopen_minor);
		result = register_chrdev_region(dev, skypopen_nr_devs, "dsp");
	} else {
		result = alloc_chrdev_region(&dev, skypopen_minor, skypopen_nr_devs,
				"dsp");
		skypopen_major = MAJOR(dev);
	}
	if (result < 0) {
		printk(KERN_WARNING "skypopen OSS driver: can't get major %d\n", skypopen_major);
		return result;
	}

	/* 
	 * allocate the devices -- we can't have them static, as the number
	 * can be specified at load time
	 */
	skypopen_devices = kmalloc(skypopen_nr_devs * sizeof(struct skypopen_dev), GFP_KERNEL);
	if (!skypopen_devices) {
		result = -ENOMEM;
		goto fail;  /* Make this more graceful */
	}
	memset(skypopen_devices, 0, skypopen_nr_devs * sizeof(struct skypopen_dev));

	/* Initialize each device. */
	for (i = 0; i < skypopen_nr_devs; i++) {
		skypopen_setup_cdev(&skypopen_devices[i], i);
	}

	/* At this point call the init function for any friend device */
	dev = MKDEV(skypopen_major, skypopen_minor + skypopen_nr_devs);
	return 0; /* succeed */

fail:
	skypopen_cleanup_module();
	return result;
}

module_init(skypopen_init_module);
module_exit(skypopen_cleanup_module);
