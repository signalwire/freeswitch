/*
 * main.c -- the bare scull char module
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

#include <asm/system.h>		/* cli(), *_flags */
#include <asm/uaccess.h>	/* copy_*_user */

#include <linux/soundcard.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>

#include "scull.h"		/* local definitions */

/*
 * Our parameters which can be set at load time.
 */

int scull_major =   SCULL_MAJOR;
int scull_minor =   3;
int scull_nr_devs = SCULL_NR_DEVS;	/* number of bare scull devices */

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_nr_devs, int, S_IRUGO);

MODULE_AUTHOR("Original: Alessandro Rubini, Jonathan Corbet. Heavy modified by: Giovanni Maruzzelli");
MODULE_LICENSE("Dual BSD/GPL");

static struct scull_dev *scull_devices;	/* allocated in scull_init_module */

#define GIOVA_BLK 1920
#define GIOVA_SLEEP 20000

void my_timer_callback_inq( unsigned long data )
{
	struct scull_dev *dev = (void *)data;

	wake_up_interruptible(&dev->inq);
	mod_timer( &dev->timer_inq, jiffies + msecs_to_jiffies(GIOVA_SLEEP/1000) );

}

void my_timer_callback_outq( unsigned long data )
{
	struct scull_dev *dev = (void *)data;

	wake_up_interruptible(&dev->outq);
	mod_timer( &dev->timer_outq, jiffies + msecs_to_jiffies(GIOVA_SLEEP/1000) );
}

/* The clone-specific data structure includes a key field */

struct scull_listitem {
	struct scull_dev device;
	dev_t key;
	struct list_head list;

};

/* The list of devices, and a lock to protect it */
static LIST_HEAD(scull_c_list);
static spinlock_t scull_c_lock = SPIN_LOCK_UNLOCKED;

/* Look for a device or create one if missing */
static struct scull_dev *scull_c_lookfor_device(dev_t key)
{
	struct scull_listitem *lptr;

	list_for_each_entry(lptr, &scull_c_list, list) {
		if (lptr->key == key)
			return &(lptr->device);
	}

	/* not found */
	lptr = kmalloc(sizeof(struct scull_listitem), GFP_KERNEL);
	if (!lptr)
		return NULL;

	/* initialize the device */
	memset(lptr, 0, sizeof(struct scull_listitem));
	lptr->key = key;

	init_waitqueue_head(&lptr->device.inq);
	init_waitqueue_head(&lptr->device.outq);
	printk(" Timer installing\n");
	setup_timer( &lptr->device.timer_inq, my_timer_callback_inq, (long int)lptr );
	setup_timer( &lptr->device.timer_outq, my_timer_callback_outq, (long int)lptr );
	printk( "Starting timer to fire in %dms (%ld)\n", GIOVA_SLEEP/1000, jiffies );
	mod_timer( &lptr->device.timer_inq, jiffies + msecs_to_jiffies(GIOVA_SLEEP/1000) );
	mod_timer( &lptr->device.timer_outq, jiffies + msecs_to_jiffies(GIOVA_SLEEP/1000) );
	/* place it in the list */
	list_add(&lptr->list, &scull_c_list);

	return &(lptr->device);
}
static int scull_c_open(struct inode *inode, struct file *filp)
{
	struct scull_dev *dev;
	dev_t key;

	if (!current->tgid) { 
		printk("Process \"%s\" has no tgid\n", current->comm);
		return -EINVAL;
	}
	key = current->tgid;

	/* look for a scullc device in the list */
	spin_lock(&scull_c_lock);
	dev = scull_c_lookfor_device(key);
	spin_unlock(&scull_c_lock);

	if (!dev)
		return -ENOMEM;

	/* then, everything else is copied from the bare scull device */
	filp->private_data = dev;
	return 0;          /* success */
}

static int scull_c_release(struct inode *inode, struct file *filp)
{
	/*
	 * Nothing to do, because the device is persistent.
	 * A `real' cloned device should be freed on last close
	 */
	return 0;
}



/*************************************************************/
/*
 * Open and close
 */

ssize_t scull_read(struct file *filp, char __user *buf, size_t count,
		loff_t *f_pos)
{
	struct scull_dev *dev = filp->private_data;

		DEFINE_WAIT(wait);
		prepare_to_wait(&dev->inq, &wait, TASK_INTERRUPTIBLE);
			schedule();
		finish_wait(&dev->inq, &wait);
		//memset(buf, 255, count);

	return count;

}

ssize_t scull_write(struct file *filp, const char __user *buf, size_t count,
		loff_t *f_pos)
{
	struct scull_dev *dev = filp->private_data;
		DEFINE_WAIT(wait);
		prepare_to_wait(&dev->outq, &wait, TASK_INTERRUPTIBLE);
			schedule();
		finish_wait(&dev->outq, &wait);

	return count;

}
/*
 * The ioctl() implementation
 */

int scull_ioctl(struct inode *inode, struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int __user *p = argp;

	switch (cmd) {
		case OSS_GETVERSION:
			return put_user(SOUND_VERSION, p);
		case SNDCTL_DSP_GETBLKSIZE:
			return put_user(GIOVA_BLK, p);
		case SNDCTL_DSP_GETFMTS:
			return put_user(28731, p);

		default:
			return 0;
	}

}

struct file_operations scull_fops = {
	.owner =    THIS_MODULE,
	.llseek =   no_llseek,
	.read =     scull_read,
	.write =    scull_write,
	.ioctl =    scull_ioctl,
	.open =     scull_c_open,
	.release =  scull_c_release,
};

/*
 * Finally, the module stuff
 */

/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */

void scull_cleanup_module(void)
{
	int i;
	int ret;
	struct scull_listitem *lptr, *next;
	dev_t devno = MKDEV(scull_major, scull_minor);

	/* Get rid of our char dev entries */
	if (scull_devices) {
		for (i = 0; i < scull_nr_devs; i++) {
			cdev_del(&scull_devices[i].cdev);
		}
		kfree(scull_devices);
	}


    	/* And all the cloned devices */
	list_for_each_entry_safe(lptr, next, &scull_c_list, list) {
		ret= del_timer( &lptr->device.timer_inq );
		if (ret) printk("The inq timer was still in use...\n");
		ret= del_timer( &lptr->device.timer_outq );
		if (ret) printk("The outq timer was still in use...\n");
		list_del(&lptr->list);
		kfree(lptr);
	}
		printk("Timer uninstalling\n");
	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, scull_nr_devs);

}


/*
 * Set up the char_dev structure for this device.
 */
static void scull_setup_cdev(struct scull_dev *dev, int index)
{
	int err, devno = MKDEV(scull_major, scull_minor + index);

	cdev_init(&dev->cdev, &scull_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &scull_fops;
	err = cdev_add (&dev->cdev, devno, 1);
	/* Fail gracefully if need be */
	if (err)
		printk(KERN_NOTICE "Error %d adding scull%d", err, index);
}



int scull_init_module(void)
{
	int result, i;
	dev_t dev = 0;

	/*
	 * Get a range of minor numbers to work with, asking for a dynamic
	 * major unless directed otherwise at load time.
	 */
	if (scull_major) {
		dev = MKDEV(scull_major, scull_minor);
		result = register_chrdev_region(dev, scull_nr_devs, "dsp");
	} else {
		result = alloc_chrdev_region(&dev, scull_minor, scull_nr_devs,
				"dsp");
		scull_major = MAJOR(dev);
	}
	if (result < 0) {
		printk(KERN_WARNING "scull: can't get major %d\n", scull_major);
		return result;
	}

	/* 
	 * allocate the devices -- we can't have them static, as the number
	 * can be specified at load time
	 */
	scull_devices = kmalloc(scull_nr_devs * sizeof(struct scull_dev), GFP_KERNEL);
	if (!scull_devices) {
		result = -ENOMEM;
		goto fail;  /* Make this more graceful */
	}
	memset(scull_devices, 0, scull_nr_devs * sizeof(struct scull_dev));

	/* Initialize each device. */
	for (i = 0; i < scull_nr_devs; i++) {
		scull_setup_cdev(&scull_devices[i], i);
	}

	/* At this point call the init function for any friend device */
	dev = MKDEV(scull_major, scull_minor + scull_nr_devs);
	return 0; /* succeed */

fail:
	scull_cleanup_module();
	return result;
}

module_init(scull_init_module);
module_exit(scull_cleanup_module);
