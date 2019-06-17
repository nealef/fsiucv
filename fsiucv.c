/************************************************************/
/*                                                          */
/* Module ID  - fsiucv.c                                    */
/*                                                          */
/* Function   - Provide a file I/O based interface for IUCV */
/*              communications.                             */
/*                                                          */
/* Called By  -                                             */
/*                                                          */
/* Calling To -                                             */
/*                                                          */
/* Parameters - (1)                                         */
/*              DESCRIPTION:                                */
/*                                                          */
/* Notes      - (1) ....................................... */
/*                                                          */
/*              (2) ....................................... */
/*                                                          */
/*                                                          */
/* Name       - Neale Ferguson.                             */
/*                                                          */
/* Date       - June, 2005.                                 */
/*                                                          */
/*                                                          */
/* Associated    - (1) Refer To ........................... */
/* Documentation                                            */
/*                 (2) Refer To ........................... */
/*                                                          */
/************************************************************/

/************************************************************/
/*                                                          */
/*                     DEFINES                              */
/*                     -------                              */
/*                                                          */
/************************************************************/

#ifndef __KERNEL__
#  define __KERNEL__
#endif
#ifndef MODULE
#  define MODULE
#endif

#define FSIUCV_DEBUG    0
#define NAME_SIZE       7

#ifndef FSIUCV_MAJOR
# define FSIUCV_MAJOR 109	/* dynamic major by default     */
#endif

#ifndef FSIUCV_NR_DEVS
# define FSIUCV_NR_DEVS 255	/* fsiucv0 through fsiucv255    */
#endif

#define PROGNAME	0xc6e2c9e4e54040

/*
 * Macros to help debugging
 */

#undef PDEBUG			/* undef it, just in case */
#ifdef FSIUCV_DEBUG
#  ifdef __KERNEL__
     /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_DEBUG "fsiucv: " fmt, ## args)
#  else
     /* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...)	/* not debugging: nothing */
#endif

#undef PDEBUGG
#define PDEBUGG(fmt, args...)	/* nothing: it's a placeholder */

/*----------------------------------------------------------*/
/* Finite state machine state definitions                   */
/*----------------------------------------------------------*/

#define O_LOCK	0
#define O_CONN	1
#define O_CW8T	2
#define O_PW8T	3
#define O_ACCP	4
#define O_AW8T	5
#define O_KILL	6
#define O_EXIT	-1

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

/*=============== End of Defines ===========================*/

/************************************************************/
/*                                                          */
/*              INCLUDE STATEMENTS                          */
/*              ------------------                          */
/*                                                          */
/************************************************************/

#include <linux/version.h>
#include <linux/kmod.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/init.h>
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/ctype.h>
#include <linux/string.h>

#include <net/iucv/iucv.h>

#include <asm/segment.h>	/* memcpy and such */
#include <asm/ebcdic.h>		/* ebcdic stuff */
#include <asm/uaccess.h>	/* copy to/from user space */
#include <asm/atomic.h>		/* atomic operations */

#include "iucv.h"		/* IUCV definitions */
#include "fsiucv.h"		/* Local definitions */

/*================== End of Include Statements =============*/

/************************************************************/
/*                                                          */
/*              TYPE DEFINITIONS                            */
/*              ----------------                            */
/*                                                          */
/************************************************************/

typedef struct EIB_t {
	void *next;
	struct iucv_message msg;
} EIB_t;

typedef struct FSIUCV_Dev {
	uid_t devOwner;		/* Current owner of device  */
	spinlock_t lock;	/* Lock word for device     */
	wait_queue_head_t waitq;/* Wait queue for device    */
	struct iucv_path *path;	/* IUCV path                */
	struct device *clDev;	/* Class device		    */
	struct device *device;	/* Device information       */
	u16 msglimit;		/* Message limit            */
	u32 msgid;		/* Target class             */
	u32 trgcls;		/* Target class             */
	u32 srccls;		/* Source class             */
	u32 msgtag;		/* Message tag              */
	u32 msglen;		/* Length of message read   */
	int rc;			/* IUCV return code         */
	int inSize;		/* Size of incoming data    */
	int outSize;		/* Size of outgoing data    */
	char priority;		/* Priority msg support     */
	char parmdata;		/* Parmdata msg support     */
	char local;		/* Local connection only    */
	char flag;		/* Flags                    */
#define FS_INUSE  0x80		/* This path is in use      */
#define FS_ACTIV  0x40		/* Active connection        */
#define FS_CWAIT  0x20		/* Active connection        */
	char *inData;		/* Buffer for incoming data */
	char *outData;		/* Buffer for outgoing data */
        char *answer;           /* Answer buffer if two-way */
        size_t ansSize;         /* Size of answer buffer    */
        size_t residual;        /* Residual count           */
	atomic_t *intType;	/* *Type of interrupt in EIB*/
	atomic_t intData;	/* Type of interrupt in EIB */
#define CPEND 0x01		/* Connection pending       */
#define CCOMP 0x02		/* Connection complete      */
#define CQSCD 0x04		/* Connection quiesced      */
#define CRSMD 0x08		/* Connection resumed       */
#define MPEND 0x10		/* Message pending          */
#define MCOMP 0x20		/* Message complete         */
#define CSVRD 0x40		/* Connection severed       */
	void *eib;		/* External Interrupt Buffer*/
	EIB_t *rcvq;		/* Queue of receive EIBs    */
	char name[8];		/* Device name              */
	char user[9];		/* Target virtual machine id*/
	char node[9];		/* Target VM node           */
	char prog[17];		/* Program name             */
} FSIUCV_Dev;

#define count_t unsigned long

/*================== End of Type Definitions ===============*/

/************************************************************/
/*                                                          */
/*             FUNCTION PROTOTYPES                          */
/*             -------------------                          */
/*                                                          */
/************************************************************/

/*
 * I/O request handlers
 */
int fsiucv_open(struct inode *, struct file *);
int fsiucv_release(struct inode *, struct file *);
ssize_t fsiucv_read(struct file *, char *, size_t, loff_t *);
static ssize_t fsiucv_receive(struct file *, char *, size_t, loff_t *);
static ssize_t fsiucv_getreply(struct file *, char *, size_t, loff_t *);
ssize_t fsiucv_readv(struct file *, const struct iovec *,
		     unsigned long, loff_t *);
ssize_t fsiucv_write(struct file *, const char *, size_t, loff_t *);
ssize_t fsiucv_writev(struct file *, const struct iovec *,
		      unsigned long, loff_t *);
int fsiucv_ioctl(struct file *, unsigned int, unsigned long);
long fsiucv_compat_ioctl(struct file *, unsigned int, unsigned long);

/*
 * I/O module routines
 */
static int __init fsiucv_init(void);
static void __exit fsiucv_exit(void);

/*
 * sysfs support routines
 */
static ssize_t prog_show(struct device *, struct device_attribute *, char *);
static ssize_t prog_set(struct device *, struct device_attribute *, const char *, size_t);
static ssize_t user_show(struct device *, struct device_attribute *, char *);
static ssize_t user_set(struct device *, struct device_attribute *, const char *, size_t);
static ssize_t iucvnode_show(struct device *, struct device_attribute *, char *);
static ssize_t iucvnode_set(struct device *, struct device_attribute *, const char *, size_t);
static ssize_t priority_show(struct device *, struct device_attribute *, char *);
static ssize_t priority_set(struct device *, struct device_attribute *, const char *, size_t);
static ssize_t parmdata_show(struct device *, struct device_attribute *, char *);
static ssize_t parmdata_set(struct device *, struct device_attribute *, const char *, size_t);
static ssize_t msglimit_show(struct device *, struct device_attribute *, char *);
static ssize_t msglimit_set(struct device *, struct device_attribute *, const char *, size_t);
static ssize_t lokal_show(struct device *, struct device_attribute *, char *);
static ssize_t lokal_set(struct device *, struct device_attribute *, const char *, size_t);
static ssize_t path_show(struct device *, struct device_attribute *, char *);
static ssize_t status_show(struct device *, struct device_attribute *, char *);

/*
 * Miscellaneous
 */
static inline void cleanup_eibq(FSIUCV_Dev *);
static int  fsiucv_register_device(FSIUCV_Dev *, int);
static int  fsiucv_unregister_device(FSIUCV_Dev *, int);
static int  fsiucv_register_driver(void);
static void fsiucv_unregister_driver(void);
static int  fsiucv_register_cdev(dev_t);
static void fsiucv_cleanup(void);

/*
 * Callbacks
 */
static int  fsiucv_callback_connreq(struct iucv_path *, u8 x[8], u8 y[16]);
static void fsiucv_callback_connack(struct iucv_path *, u8 x[16]);
static void fsiucv_callback_connrej(struct iucv_path *, u8 x[16]);
static void fsiucv_callback_connsusp(struct iucv_path *, u8 x[16]);
static void fsiucv_callback_connres(struct iucv_path *, u8 x[16]);
static void fsiucv_callback_rx(struct iucv_path *, struct iucv_message *);
static void fsiucv_callback_txdone(struct iucv_path *, struct iucv_message *);

/*================== End of Prototypes =====================*/

/************************************************************/
/*                                                          */
/*           GLOBAL VARIABLE DECLARATIONS                   */
/*           ----------------------------                   */
/*                                                          */
/************************************************************/

extern int fsiucv_nr_devs;
extern struct device *iucv_root;
extern struct bus_type iucv_bus;

extern FSIUCV_Dev *fsiucv_devices;

/*
 * sysfs file attributes
 */
DEVICE_ATTR(program_name, 0666, prog_show, prog_set);
DEVICE_ATTR(target_user, 0666, user_show, user_set);
DEVICE_ATTR(target_node, 0666, iucvnode_show, iucvnode_set);
DEVICE_ATTR(priority, 0666, priority_show, priority_set);
DEVICE_ATTR(parmdata, 0666, parmdata_show, parmdata_set);
DEVICE_ATTR(msglimit, 0666, msglimit_show, msglimit_set);
DEVICE_ATTR(local, 0666, lokal_show, lokal_set);
DEVICE_ATTR(path, 0444, path_show, NULL);
DEVICE_ATTR(status, 0444, status_show, NULL);

static struct attribute *fsiucv_attrs[] = {
	&dev_attr_program_name.attr,
	&dev_attr_target_user.attr,
	&dev_attr_target_node.attr,
	&dev_attr_path.attr,
	&dev_attr_priority.attr,
	&dev_attr_parmdata.attr,
	&dev_attr_msglimit.attr,
	&dev_attr_local.attr,
	&dev_attr_status.attr,
	NULL
};

static struct attribute_group fsiucv_attr_group = {
	.attrs = fsiucv_attrs,
};

/*
 * Module parameters
 */
static int fsiucv_major = 0;
static int maxconn = 0;

/*
 * IUCV Device Control Blocks
 */
FSIUCV_Dev *iucvDev;

/*
 * Device driver data areas
 */
static struct cdev *fsiucv_cdev = NULL;
static struct class *fsiucv_class;
static struct device_driver fsiucv_driver = {
	.name = "fsiucv",
	.bus  = &iucv_bus,
};

/*
 * IUCV Operations
 */
static struct iucv_handler fsiucv_ops = {
	.path_pending = fsiucv_callback_connreq,
	.path_complete = fsiucv_callback_connack,
	.path_severed = fsiucv_callback_connrej,
	.path_quiesced = fsiucv_callback_connsusp,
	.path_resumed = fsiucv_callback_connres,
	.message_pending = fsiucv_callback_rx,
	.message_complete = fsiucv_callback_txdone
};

/*----------------------------------------------------------*/
/* Map the IPRCODE to something this module will use        */
/*----------------------------------------------------------*/

static int mapIPRC[97] = {
	0, 2, 2, 2, 3, 2, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2
};

/*----------------------------------------------------------*/
/* Map the IUCV interrupt to a state return code            */
/*----------------------------------------------------------*/

static int mapInt[112] = {
	0, 1, 2, 0, 3, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0,
	5, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	7, 7, 7, 0, 7, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0,
	7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/*----------------------------------------------------------*/
/* The finite state machines                                */
/*----------------------------------------------------------*/

static const int openFSM[7][8] = {
/* LOCK	*/ {O_CONN, O_PW8T, O_EXIT, O_EXIT, O_EXIT, O_EXIT, O_EXIT, O_EXIT},
/* CONN	*/ {O_CW8T, O_EXIT, O_KILL, O_KILL, O_EXIT, O_EXIT, O_EXIT, O_EXIT},
/* CW8T	*/ {O_KILL, O_KILL, O_EXIT, O_KILL, O_KILL, O_EXIT, O_KILL, O_KILL},
/* PW8T */ {O_KILL, O_ACCP, O_EXIT, O_EXIT, O_EXIT, O_EXIT, O_EXIT, O_EXIT},
/* ACCP */ {O_AW8T, O_KILL, O_EXIT, O_EXIT, O_EXIT, O_EXIT, O_EXIT, O_EXIT},
/* AW8T	*/ {O_KILL, O_KILL, O_EXIT, O_KILL, O_KILL, O_EXIT, O_KILL, O_KILL},
/* KILL */ {O_EXIT, O_EXIT, O_EXIT, O_EXIT, O_EXIT, O_EXIT, O_EXIT, O_EXIT}
};

/*----------------------------------------------------------*/
/* The different file operations                            */
/*----------------------------------------------------------*/

static struct file_operations fsiucv_fops = {
      read:    		fsiucv_read,
      write:   		fsiucv_write,
      open:   		fsiucv_open,
      unlocked_ioctl:	fsiucv_ioctl,
      compat_ioctl:	fsiucv_compat_ioctl,
      release: 		fsiucv_release,
};

MODULE_AUTHOR("Neale Ferguson <neale@sinenomine.net>");
MODULE_DESCRIPTION("Linux on System z interface to IUCV."
		   " Copyright 2005-2016 Neale Ferguson");
module_param(maxconn, int, S_IRUSR | S_IWUSR);
MODULE_PARM_DESC(maxconn, "Maximum number of connections (1-255)");
MODULE_LICENSE("GPL");

/*================== End of Global Variables ===============*/

/************************************************************/
/*                                                          */
/* Name       - fsiucv_init.                                */
/*                                                          */
/* Function   - Initialize this device driver.              */
/*                                                          */
/************************************************************/

static int __init
fsiucv_init(void)
{
    int rc, i_dev;
    union {
            long long cpuid;
            char cpu[8];
    } id;
    dev_t dev;

    asm("STIDP\t%0\n"
        : "=m"(id.cpuid)
       );

    if (id.cpu[0] != 0xff)
        return -EACCES;

    if (maxconn < 1 || maxconn > FSIUCV_NR_DEVS)
        maxconn = 1;

    PDEBUG("Driver loaded for %d connections\n", maxconn);
    /*------------------------------------------------------*/
    /* Register your major, and accept a dynamic number     */
    /*------------------------------------------------------*/
    rc = alloc_chrdev_region(&dev, 0, maxconn, "fsiucv");
    if (rc != 0) {
        printk(KERN_ERR "fsiucv: can't allocate chrdev region\n");
        return rc;
    }

    fsiucv_major = MAJOR(dev);

    PDEBUG("was allocated major number %d\n",
           fsiucv_major);

    rc = fsiucv_register_driver();
    if (rc == 0) {

        /*----------------------------------------------*/
        /* allocate the devices -- we can't have them   */
        /* static as the # can be specified at load time*/
        /*----------------------------------------------*/
        iucvDev = kzalloc(maxconn * sizeof (FSIUCV_Dev),
                          GFP_KERNEL);
        if (!iucvDev) {
            rc = -ENOMEM;
            unregister_chrdev(fsiucv_major, "fsiucv");
            return (rc);
        }

        PDEBUG("iucv device control block: %p.%lx\n",
               iucvDev,(maxconn * sizeof (FSIUCV_Dev)));
        /*----------------------------------------------*/
        /* Register these devices under the IUCV bus    */
        /*----------------------------------------------*/
        memset(iucvDev, 0, maxconn * sizeof (FSIUCV_Dev));
        for (i_dev = 0; i_dev < maxconn; i_dev++) {
            rc = fsiucv_register_device(&iucvDev[i_dev], i_dev);
        }
    }

    if (rc == 0)
        rc = fsiucv_register_cdev(dev);

    if (rc != 0)
        fsiucv_cleanup();

    return (rc);

}

module_init(fsiucv_init);

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - fsiucv_register_driver.                     */
/*                                                          */
/* Function   - Register device driver.                     */
/*                                                          */
/************************************************************/

static int
fsiucv_register_driver(void)
{
    int rc;

    rc = iucv_register(&fsiucv_ops, 1);
    if (rc == 0) {
        rc = driver_register(&fsiucv_driver);
        if (rc == 0) {
            fsiucv_class = class_create(THIS_MODULE,
                                    "fsiucv");
            if (IS_ERR(fsiucv_class)) {
                rc = PTR_ERR(fsiucv_class);
                fsiucv_class = NULL;
                driver_unregister(&fsiucv_driver);
                iucv_unregister(&fsiucv_ops, 1);
            }
        } else {
            iucv_unregister(&fsiucv_ops, 1);
        }
    }
    return(rc);
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - fsiucv_unregister_driver.                   */
/*                                                          */
/* Function   - Unregister device driver.                   */
/*                                                          */
/************************************************************/

static void
fsiucv_unregister_driver(void)
{
    class_destroy(fsiucv_class);
    fsiucv_class = NULL;
    driver_unregister(&fsiucv_driver);
    PDEBUG("unregistering from iucv service - %p (%p,%p)\n",
           &fsiucv_ops,fsiucv_ops.list.prev,fsiucv_ops.list.next);
    iucv_unregister(&fsiucv_ops, 1);
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - fsiucv_register_device.                     */
/*                                                          */
/* Function   - Register device.                            */
/*                                                          */
/************************************************************/

static int
fsiucv_register_device(FSIUCV_Dev *pFS, int minor)
{
    struct device *pDev;
    int rc = 0;

    init_waitqueue_head(&pFS->waitq);

    spin_lock_init(&pFS->lock);
    snprintf(pFS->name, sizeof(pFS->name), "iucv%d", minor);
    pFS->device = pDev = kzalloc(sizeof (struct device),
                                 GFP_KERNEL);
    if (pDev) {
        dev_set_name(pDev, pFS->name);
        PDEBUG("kernel device %d: %p.%lx\n",
               minor,pDev,sizeof(struct device));
        pDev->bus = &iucv_bus;
        pDev->parent = iucv_root;
        pDev->driver = &fsiucv_driver;
        dev_set_drvdata(pDev, pFS);
        pDev->release = (void (*)(struct device *)) kfree;
        pFS->intType = &pFS->intData;
        rc = device_register(pDev);
        if (rc != 0) {
            printk(KERN_ERR "can't register device: %d\n", rc);
            return(rc);
        } 

        rc = sysfs_create_group(&pDev->kobj, &fsiucv_attr_group);
        if (rc != 0) {
            device_unregister(pDev);
            return(rc);
        }

        pFS->clDev = device_create(fsiucv_class, pDev,
                                   MKDEV(fsiucv_major, minor),
                                   pFS, "%s", dev_name(pDev));

        if (IS_ERR(pFS->clDev)) {
            rc = PTR_ERR(pFS->clDev);
            pFS->clDev = NULL;
            sysfs_remove_group(&pDev->kobj, &fsiucv_attr_group);
            return(rc);
        }
    } else 
        rc = -ENOMEM;

    return (rc);
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - fsiucv_unregister_device.                   */
/*                                                          */
/* Function   - Unregister a device.                        */
/*                                                          */
/************************************************************/

static int
fsiucv_unregister_device(FSIUCV_Dev *pFS, int minor)
{
    static __u8 user_data[16] = { 0xe2, 0xc8, 0xe4, 0xe3,
            0xc4, 0xd6, 0xe6, 0xd5,
            0x40, 0x40, 0x40, 0x40,
            0x40, 0x40, 0x40, 0x40
    };

    PDEBUG("Destroying device\n");

    device_destroy(fsiucv_class, MKDEV(fsiucv_major, minor));
    if (pFS->device != NULL) {
        PDEBUG("removing group\n");

        sysfs_remove_group(&pFS->device->kobj,
                           &fsiucv_attr_group);

        if (pFS->flag & FS_INUSE)
            iucv_path_sever(pFS->path, user_data);

        PDEBUG("freeing path\n");

        kfree(pFS->path);

        PDEBUG("freeing input buffer\n");

        if (pFS->inData != NULL)
            kfree(pFS->inData);

        PDEBUG("freeing output buffer\n");

        if (pFS->outData != NULL)
            kfree(pFS->outData);

        PDEBUG("freeing answer buffer\n");

        if (pFS->answer != NULL) 
            kfree(pFS->answer);

        PDEBUG("cleaning up EIB queue\n");

        cleanup_eibq(pFS);

        PDEBUG("unregistering device\n");

        device_unregister(pFS->device);

        PDEBUG("freeing iucv path\n");

        if (pFS->path != NULL)
            iucv_path_free(pFS->path);
    }	
    return(0);
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - fsiucv_register_cdev.                       */
/*                                                          */
/* Function   - Register character device.                  */
/*                                                          */
/************************************************************/

static int
fsiucv_register_cdev(dev_t dev)
{
    int rc = 0;
    
    fsiucv_cdev = cdev_alloc();
    if (fsiucv_cdev == NULL) {
        rc = -ENOMEM;
    } else {
        fsiucv_cdev->owner = THIS_MODULE;
        fsiucv_cdev->ops   = &fsiucv_fops;
        fsiucv_cdev->dev   = dev;
        rc = cdev_add(fsiucv_cdev, fsiucv_cdev->dev, maxconn);
        if (rc != 0) {
            kobject_put(&fsiucv_cdev->kobj);
            fsiucv_cdev = NULL;
        }
    }
    return (rc);
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - fsiucv_cleanup.                             */
/*                                                          */
/* Function   - Unload this device driver.                  */
/*                                                          */
/************************************************************/

static void
fsiucv_cleanup()
{
    int i_dev;

    PDEBUG("Deleting class device\n");

    if (fsiucv_cdev) {
        cdev_del(fsiucv_cdev);
        fsiucv_cdev = NULL;
    }

    PDEBUG("Unregistering devices\n");

    for (i_dev = 0; i_dev < maxconn; i_dev++) {
        fsiucv_unregister_device(&iucvDev[i_dev], i_dev);
    }

    PDEBUG("Unregistering driver\n");

    fsiucv_unregister_driver();
    if (fsiucv_major != 0) {
        unregister_chrdev_region(MKDEV(fsiucv_major, 0), maxconn);
        fsiucv_major = 0;
    }

    PDEBUG("freeing IUCV device control block\n");

    kfree(iucvDev);
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - fsiucv_exit.                                */
/*                                                          */
/* Function   - Terminate this device driver.               */
/*                                                          */
/************************************************************/

static void __exit
fsiucv_exit(void)
{
    fsiucv_cleanup();
}

module_exit(fsiucv_exit);

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - fsiucv_open.                                */
/*                                                          */
/* Function   - Open the IUCV device.                       */
/*                                                          */
/* Parameters - inode -                                     */
/*              filp  -                                     */
/*                                                          */
/************************************************************/

int
fsiucv_open(struct inode *inode, struct file *filp)
{
    int num = MINOR(inode->i_rdev), iField, state, err = 0, rc = 0;
    FSIUCV_Dev *dev;
    u8 userid[8],
       system[8],
       userData[16];
    int flags1 = 0;

    if (num >= maxconn)
            return -ENODEV;

    dev = &iucvDev[num];

    PDEBUG("Device %d at %p\n",num,dev);

    /*--------------------------------------------------*/
    /* use filp->private_data to point to device data   */
    /*--------------------------------------------------*/
    filp->private_data = dev;
    filp->f_pos = 0;
    dev->ansSize = 0;
    dev->answer = NULL;

    spin_lock(&dev->lock);

    state = O_LOCK;
    while (state != O_EXIT) {
        switch (state) {
        case O_LOCK:
            if (!(dev->flag & FS_INUSE)) {
                dev->flag = FS_INUSE;
                if (dev->user[0] == 0) {
                    rc = 1;
                } else
                    rc = 0;
            } else {
                err = -EACCES;
                rc = 2;
            }
            break;

        case O_CONN:
            memset(&userid, ' ', sizeof (userid));
            memset(&system, ' ', sizeof (system));
            memset(&userData, 0, sizeof (userData));

            for (iField = 0;
                 iField < sizeof (userid) &&
                 dev->user[iField] != 0 &&
                 dev->user[iField] != '\n'; iField++)
                userid[iField] = dev->user[iField];

            for (iField = 0;
                 iField < sizeof (system) &&
                 dev->node[iField] != 0 &&
                 dev->node[iField] != '\n'; iField++)
                system[iField] = dev->node[iField];

            for (iField = 0;
                 iField < sizeof (userData) &&
                 dev->prog[iField] != 0 &&
                 dev->prog[iField] != '\n'; iField++)
                userData[iField] = dev->prog[iField];

            printk(KERN_ERR "fsiucv - user: %s node: %s data: %s\n",dev->user,dev->node,dev->prog);
            if (dev->priority)
                flags1 |= IUCV_IPPRTY;
            if (dev->parmdata)
                flags1 |= IUCV_IPRMDATA;
            if (dev->local)
                flags1 |= IUCV_IPLOCAL;

            dev->flag |= FS_ACTIV;

            dev->path = iucv_path_alloc(dev->msglimit,
                                        flags1,
                                        GFP_KERNEL);
            if (dev->path == NULL) {
                printk(KERN_ERR
                       "fsiucv: unable to allocate path\n");
                rc = 2;
            } else {
                dev->path->private = dev;

                rc = iucv_path_connect(dev->path, &fsiucv_ops,
                                       (u8 *) &userid, (u8 *) &system, 
                                       (u8 *) &userData, dev);
                if (rc != 0) {
                    printk(KERN_ERR
                           "fsiucv: unexpected rc %d from connect\n",
                           rc);
                    rc = mapIPRC[rc];
                    dev->flag &= ~FS_ACTIV;
                }
            }
            break;

        case O_PW8T:
            dev->flag |= FS_CWAIT;
        case O_CW8T:
        case O_AW8T:
            PDEBUG("waiting\n");
            wait_event_interruptible(dev->waitq,
                                     (atomic_read(dev->intType) !=
                                      0));
            rc = mapInt[atomic_read(dev->intType)];
            PDEBUG("%d got: %d %d\n", state, rc,
                   atomic_read(dev->intType));
            atomic_clear_mask((CPEND | CCOMP), dev->intType);
            break;

        case O_ACCP:
            flags1 &= ~IUCV_IPLOCAL;
            dev->flag |= FS_ACTIV;
            rc = iucv_path_accept(dev->path, &fsiucv_ops, NULL, dev);
            if (rc != 0) {
                printk(KERN_ERR
                       "fsiucv: unexpected rc %d from accept\n",
                       rc);
                rc = mapIPRC[rc];
                dev->flag &= ~FS_ACTIV;
            }
            break;

        case O_KILL:
            PDEBUG("Connection severed\n");
            dev->flag = 0;
            err = -EACCES;
            rc = 0;
            break;
        }
        state = openFSM[state][rc];
    }
    spin_unlock(&dev->lock);
    PDEBUG("open complete - err: %d\n",err);

    if (err != 0) {
        if (dev->path != NULL) {
            iucv_path_free(dev->path);
            dev->path = NULL;
        }
        atomic_set(dev->intType, 0);
        dev->flag = 0;
        memset(dev->user, 0, sizeof(dev->user));
        memset(dev->node, 0, sizeof(dev->node));
        memset(dev->prog, 0, sizeof(dev->prog));
    }

    return (err);
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - fsiucv_release.                             */
/*                                                          */
/* Function   - Close a device.                             */
/*                                                          */
/* Parameters - inode -                                     */
/*              filp  -                                     */
/*                                                          */
/************************************************************/

int
fsiucv_release(struct inode *inode, struct file *filp)
{
    FSIUCV_Dev *dev = filp->private_data;
    static __u8 user_data[16] = { 0xd9, 0xc5, 0xd3, 0xc5,
            0xc1, 0xe2, 0xc5, 0x40,
            0x40, 0x40, 0x40, 0x40,
            0x40, 0x40, 0x40, 0x40
    };

    PDEBUG("releasing %s - %02x\n",dev->name,dev->flag);
    spin_lock(&dev->lock);
    if (dev->flag & FS_INUSE) {
        if (dev->path != NULL) {
            iucv_path_sever(dev->path, user_data);
            cleanup_eibq(dev);
            if (dev->path != NULL) {
                iucv_path_free(dev->path);
                dev->path = NULL;
            }
        }
        memset(dev->user, 0, sizeof(dev->user));
        memset(dev->node, 0, sizeof(dev->node));
        atomic_set(dev->intType, 0);
        dev->flag    = 0;
    }
    spin_unlock(&dev->lock);

    return (0);
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - fsiucv_read.                                */
/*                                                          */
/* Function   - Read an IUCV message.                       */
/*                                                          */
/* Parameters - inode -                                     */
/*              filp  -                                     */
/*              buf   - Pointer to read buffer.             */
/*              count - Size of read buffer.                */
/*                                                          */
/************************************************************/

ssize_t
fsiucv_read(struct file * filp, char *buf, size_t count, loff_t * ppos)
{
    FSIUCV_Dev *dev = filp->private_data;

    if (dev->answer == NULL)
        return(fsiucv_receive(filp, buf, count, ppos));
    else
        return(fsiucv_getreply(filp, buf, count, ppos));
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - fsiucv_receive.                             */
/*                                                          */
/* Function   - Use the iucv_message_receive call to get    */
/*              the data.                                   */
/*                                                          */
/* Parameters - inode -                                     */
/*              filp  -                                     */
/*              buf   - Pointer to read buffer.             */
/*              count - Size of read buffer.                */
/*                                                          */
/************************************************************/

static ssize_t
fsiucv_receive(struct file * filp, char *buf, size_t count, loff_t * ppos)
{
    FSIUCV_Dev *dev = filp->private_data;
    loff_t f_pos = filp->f_pos;
    ulong msgLen, bufSize, resLen;
    struct iucv_message *mpEIB;
    EIB_t *rcvq;
    int rc, flags;

    PDEBUG("In read: dev: %p\n",dev);
    PDEBUG("*rcvq 1: %p\n",&dev->rcvq);
    PDEBUG("event 1: %02x\n", atomic_read(dev->intType));

    if (atomic_read(dev->intType) & CSVRD)
        return(-EIO);

    if (f_pos == 0) {
        PDEBUG("rcvq 2: %p\n",dev->rcvq);
        if (!(atomic_read(dev->intType) & MPEND) && dev->rcvq == NULL)
            wait_event_interruptible(dev->waitq,
                         ((atomic_read(dev->intType) & MPEND) | 
                          (atomic_read(dev->intType)
                                     & CSVRD)));

        PDEBUG("event 2: %02x\n", atomic_read(dev->intType));

        if (atomic_read(dev->intType) & CSVRD)
            return(-EIO);

        if ((atomic_read(dev->intType) & MPEND) || dev->rcvq != NULL) {
            atomic_clear_mask(MPEND, dev->intType);
            mpEIB = &dev->rcvq->msg;
            PDEBUG("eib=%p next=%p\n", dev->rcvq, dev->rcvq->next);
            dev->msglen = mpEIB->length;
            dev->msgid  = mpEIB->id;
            dev->trgcls = mpEIB->class;

            bufSize = MAX(count, dev->msglen);

            if (dev->inSize < bufSize) {
                if (dev->inData)
                    kfree(dev->inData);
                dev->inData = kmalloc(bufSize, GFP_KERNEL);
                dev->inSize = bufSize;
            }

            flags = 0;

            PDEBUG("reading message: %d length: %d\n",
                dev->msgid, dev->msglen);

            rc = iucv_message_receive(dev->path, mpEIB,
                              flags, (void *) dev->inData,
                              dev->msglen, &resLen);

            if (rc != 0) {
                printk(KERN_ERR "fsiucv: rc %d from receive\n",rc);
                return (-EIO);
            }

            PDEBUG("receive - count=%d\n",
            dev->msglen);

        } else
            return (-EIO);
    }

    msgLen = MIN(count, (dev->msglen - f_pos));

    if (copy_to_user(buf, &dev->inData[f_pos], msgLen) != 0)
        return (-EIO);

    *ppos = f_pos + msgLen;

    if (*ppos >= dev->msglen) {
        dev->msglen = 0;
        rcvq        = dev->rcvq;
        dev->rcvq   = dev->rcvq->next;
        kfree(rcvq);
        *ppos = 0;
    }

    PDEBUG("leaving read %ld\n", msgLen);

    return (msgLen);
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - fsiucv_getreply.                            */
/*                                                          */
/* Function   - Extract reply from answer buffer.           */
/*                                                          */
/* Parameters - inode -                                     */
/*              filp  -                                     */
/*              buf   - Pointer to read buffer.             */
/*              count - Size of read buffer.                */
/*                                                          */
/************************************************************/

static ssize_t
fsiucv_getreply(struct file * filp, char *buf, size_t count, loff_t * ppos)
{
    FSIUCV_Dev *dev = filp->private_data;
    loff_t f_pos = filp->f_pos;
    ulong msgLen;

    PDEBUG("In getreply: dev: %p\n",dev);

    if (atomic_read(dev->intType) & CSVRD)
        return(-EIO);

    if (dev->residual != 0) {
        printk(KERN_ERR "fsiucv: Reply buffer requires %ld additional bytes\n",
               dev->residual);
        return(-EIO);
    }

    msgLen = MIN(count, (dev->msglen - f_pos));

    if (copy_to_user(buf, &dev->answer[f_pos], msgLen) != 0)
        return (-EIO);

    *ppos = f_pos + msgLen;

    if (*ppos >= dev->msglen) {
        dev->msglen = 0;
        *ppos = 0;
    }

    PDEBUG("leaving getreply %ld\n", msgLen);

    return (msgLen);
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - fsiucv_readv.                               */
/*                                                          */
/* Function   - Read an array of IUCV buffers.              */
/*                                                          */
/* Parameters - inode -                                     */
/*              filp  -                                     */
/*              buf   - Pointer to read buffer.             */
/*              count - Size of read buffer.                */
/*                                                          */
/************************************************************/

ssize_t
fsiucv_readv(struct file * filp, const struct iovec * iov,
	     unsigned long count, loff_t * ppos)
{
    FSIUCV_Dev *dev = filp->private_data;
    unsigned long f_pos = (unsigned long) (filp->f_pos);

    if (atomic_read(dev->intType) & CSVRD)
        return(-EIO);

    return (count);
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - fsiucv_write.                               */
/*                                                          */
/* Function   - Send a message via IUCV to the target.      */
/*                                                          */
/* Parameters - inode -                                     */
/*              filp  -                                     */
/*              buf   - Pointer to read buffer.             */
/*              count - Size of read buffer.                */
/*                                                          */
/************************************************************/

ssize_t
fsiucv_write(struct file * filp, const char *buf, size_t count, loff_t * ppos)
{
    FSIUCV_Dev *dev = filp->private_data;
    int rc, flags = 0;
    struct iucv_message *msg;

    PDEBUG("write request\n");

    if (atomic_read(dev->intType) & CSVRD)
        return(-EIO);

    if (dev->outSize < count) {
        if (dev->outData)
            kfree(dev->outData);
        dev->outData = kmalloc(count, GFP_KERNEL);
        dev->outSize = count;
    }

    filp->f_pos = 0;
    if (copy_from_user(&dev->outData[filp->f_pos], buf, count) != 0)
        return(-EFAULT);

    msg = kzalloc(sizeof(struct iucv_message), GFP_KERNEL);
    msg->id = dev->msgid;
    msg->class = dev->trgcls;

    if (dev->answer == NULL) {
        rc = iucv_message_send(dev->path, msg, flags,
                               dev->srccls, dev->outData, count);
    } else {
        rc = iucv_message_send2way(dev->path, msg, flags,
                                   dev->srccls, dev->outData, count,
                                   dev->answer, dev->ansSize, &dev->residual);
                                   dev->msglen = dev->ansSize;
    }

    if (rc == 0) {
        wait_event_interruptible(dev->waitq,
                    ((atomic_read(dev->intType) & MCOMP) |
                     (atomic_read(dev->intType) & CSVRD)));

        if (atomic_read(dev->intType) & CSVRD)
            count = -EIO;

        atomic_clear_mask(MCOMP, dev->intType);
    } else
        count = -EIO;

    return (count);
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - fsiucv_writev.                              */
/*                                                          */
/* Function   - Send an array of messages via IUCV to the   */
/*              target.                                     */
/*                                                          */
/* Parameters - inode -                                     */
/*              filp  -                                     */
/*              buf   - Pointer to read buffer.             */
/*              count - Size of read buffer.                */
/*                                                          */
/************************************************************/

ssize_t
fsiucv_writev(struct file * filp, const struct iovec * iov,
	      unsigned long count, loff_t * ppos)
{
    FSIUCV_Dev *dev = filp->private_data;

    if (atomic_read(dev->intType) & CSVRD)
        return(-EIO);

    filp->f_pos = 0;
    return (count);
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - fsiucv_ioctl.                               */
/*                                                          */
/* Function   - Perform IOCTL functions for IUCV interface: */
/*                                                          */
/* Parameters - inode -                                     */
/*              filp  -                                     */
/*              cmd   - Command to be issued.               */
/*              arg   - Argument.                           */
/*                                                          */
/************************************************************/

int
fsiucv_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    FSIUCV_Dev *dev = filp->private_data;

    if (dev != NULL) {
        switch (cmd) {
            case FSIUCVTCS:	/* Set target class   */
                if (copy_from_user(&dev->trgcls,
                    (void __user *) arg, sizeof (dev->trgcls)))
                     return -EFAULT;
                else
                     return 0;
                break;

            case FSIUCVTCG:	/* Get target class   */
                if (copy_to_user((void __user *) arg,
                    &dev->trgcls, sizeof (dev->trgcls)))
                    return -EFAULT;
                else
                    return 0;
                break;

            case FSIUCVSCS:	/* Set source class   */
                if (copy_from_user(&dev->srccls,
                    (void __user *) arg, sizeof (dev->srccls)))
                    return -EFAULT;
                else
                    return 0;
                break;

            case FSIUCVSCG:	/* Get source class   */
                if (copy_to_user((void __user *) arg,
                    &dev->srccls, sizeof (dev->srccls)))
                    return -EFAULT;
                else
                    return 0;
                break;

            case FSIUCVMTS:	/* Set message tag    */
                if (copy_from_user(&dev->msgtag,
                    (void __user *) arg, sizeof (dev->msgtag)))
                    return -EFAULT;
                else
                    return 0;
                break;

            case FSIUCVMTG:	/* Get message tag    */
                if (copy_to_user((void __user *) arg,
                    &dev->msgtag, sizeof (dev->msgtag)))
                    return -EFAULT;
                else
                    return 0;
                break;

            case FSIUCVANS:	/* Set answer buffer size for 2way reply */
                if (copy_from_user(&dev->ansSize, (void __user *) arg,
                    sizeof (dev->ansSize)))
                    return -EFAULT;
                else {
                    if (dev->answer != NULL)
                        kfree(dev->answer);

                    dev->answer = kmalloc(dev->ansSize, GFP_KERNEL);
                    if (dev->answer == NULL) {
                        dev->ansSize = 0;
                        return -ENOMEM;
                    }
                return 0;
                }
            break;

            case FSIUCVVMG:	/* Get name of partner user */
                if (copy_to_user((void __user *) arg,
                    &dev->user, sizeof (dev->user)))
                    return -EFAULT;
                else
                    return 0;
                break;

            case FSIUCVNDG:	/* Get name of partner node */
                if (copy_to_user((void __user *) arg,
                    &dev->node, sizeof (dev->node)))
                    return -EFAULT;
                else
                    return 0;
                break;

            default:
                return -EINVAL;
        }
    } else
        return -ENODEV;

}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - fsiucv_compat_ioctl.                        */
/*                                                          */
/* Function   - Perform IOCTL functions for IUCV interface: */
/*              for 32-bit apps.                            */
/*                                                          */
/* Parameters - inode -                                     */
/*              filp  -                                     */
/*              cmd   - Command to be issued.               */
/*              arg   - Argument.                           */
/*                                                          */
/*                                                          */
/************************************************************/

long
fsiucv_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    return ((long) fsiucv_ioctl(filp, cmd, arg));
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - cleanup_eibq.                               */
/*                                                          */
/* Function   - Get rid of any remaining entries on the eib */
/*              queue.                                      */
/*                                                          */
/* Parameters - *dev     - Device entry                     */
/*                                                          */
/************************************************************/

static inline void
cleanup_eibq(FSIUCV_Dev * dev)
{
    EIB_t *next, *cur;

    if (dev->rcvq != NULL) {

        cur = dev->rcvq;
        dev->rcvq = NULL;

        while (cur != NULL) {
            next = cur->next;
            kfree(cur);
            cur = next;
        }
    }
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - prog_show.                                  */
/*                                                          */
/* Function   - Show the prog name associated with this dev */
/*                                                          */
/* Parameters - *dev  - A(struct device)                    */
/*              *buf  - Buffer to place result              */
/*                                                          */
/************************************************************/

static ssize_t
prog_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    FSIUCV_Dev *priv = (FSIUCV_Dev *) dev_get_drvdata(dev);

    return (snprintf(buf, sizeof (priv->prog), "%s", priv->prog));
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - prog_set.                                   */
/*                                                          */
/* Function   - Set the prog name associated with this dev  */
/*                                                          */
/* Parameters - *dev  - A(struct device)                    */
/*              *buf  - Source data                         */
/*              count - Size of source data                 */
/*                                                          */
/************************************************************/

static ssize_t
prog_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    int lProg;
    FSIUCV_Dev *priv = (FSIUCV_Dev *) dev_get_drvdata(dev);

    PDEBUG("prog_set - fsdev: %p count: %ld\n",priv,count);
    if (!(priv->flag & FS_INUSE)) {
            lProg = MIN(count, sizeof (priv->prog) - 1);
            memcpy(priv->prog, buf, lProg);
            priv->prog[lProg] = 0;
            return (count);
    } else {
        return (-EACCES);
    }
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - user_show.                                  */
/*                                                          */
/* Function   - Show the user name associated with this dev */
/*                                                          */
/* Parameters - *dev  - A(struct device)                    */
/*              *buf  - Buffer to place result              */
/*                                                          */
/************************************************************/

static ssize_t
user_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    FSIUCV_Dev *priv = (FSIUCV_Dev *) dev_get_drvdata(dev);

    return snprintf(buf, sizeof (priv->user), "%s", priv->user);
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - user_set.                                   */
/*                                                          */
/* Function   - Set the user name associated with this dev  */
/*                                                          */
/* Parameters - *dev  - A(struct device)                    */
/*              *buf  - Source data                         */
/*              count - Size of source data                 */
/*                                                          */
/************************************************************/

static ssize_t
user_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    int lUser;
    FSIUCV_Dev *priv = (FSIUCV_Dev *) dev_get_drvdata(dev);

    PDEBUG("user_set - fsdev: %p count: %ld\n",priv,count);
    if (!(priv->flag & FS_INUSE)) {
        lUser = MIN(count, sizeof (priv->user) - 1);
        memcpy(priv->user, buf, lUser);
        priv->user[lUser] = 0;
        return (count);
    } else {
        return (-EACCES);
    }
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - iucvnode_show.                              */
/*                                                          */
/* Function   - Show the node name associated with this dev */
/*                                                          */
/* Parameters - *dev  - A(struct device)                    */
/*              *buf  - Buffer to place result              */
/*                                                          */
/************************************************************/

static ssize_t
iucvnode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    FSIUCV_Dev *priv = (FSIUCV_Dev *) dev_get_drvdata(dev);

    return snprintf(buf, sizeof (priv->node), "%s", priv->node);
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - iucvnode_set.                               */
/*                                                          */
/* Function   - Set the node name associated with this dev  */
/*                                                          */
/* Parameters - *dev  - A(struct device)                    */
/*              *buf  - Source data                         */
/*              count - Size of source data                 */
/*                                                          */
/************************************************************/

static ssize_t
iucvnode_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    int lNode;
    FSIUCV_Dev *priv = (FSIUCV_Dev *) dev_get_drvdata(dev);

    if (!(priv->flag & FS_INUSE)) {
        lNode = min(count, sizeof (priv->node) - 1);
        memcpy(priv->node, buf, lNode);
        priv->node[lNode] = 0;
        return (count);
    } else {
        return (-EACCES);
    }
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - path_show.                                  */
/*                                                          */
/* Function   - Show the path number in use.                */
/*                                                          */
/* Parameters - *dev  - A(struct device)                    */
/*              *buf  - Buffer to place result              */
/*                                                          */
/************************************************************/

static ssize_t
path_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    FSIUCV_Dev *priv = (FSIUCV_Dev *) dev_get_drvdata(dev);

    if (priv->flag && FS_INUSE)
        return (snprintf(buf, sizeof (priv->user), "%d\n", 
                priv->path->pathid));
    else
        return (sprintf(buf, "No connection active\n"));
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - priority_show.                              */
/*                                                          */
/* Function   - Show whether priority messages are supported*/
/*                                                          */
/* Parameters - *dev  - A(struct device)                    */
/*              *buf  - Buffer to place result              */
/*                                                          */
/************************************************************/

static ssize_t
priority_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    FSIUCV_Dev *priv = (FSIUCV_Dev *) dev_get_drvdata(dev);

    return (sprintf(buf, "%d\n", priv->priority));
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - priority_set.                               */
/*                                                          */
/* Function   - Set whether the connection will support     */
/*              priority messages.                          */
/*                                                          */
/* Parameters - *dev  - A(struct device)                    */
/*              *buf  - Source data                         */
/*              count - Size of source data                 */
/*                                                          */
/************************************************************/

static ssize_t
priority_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    FSIUCV_Dev *priv = (FSIUCV_Dev *) dev_get_drvdata(dev);
    char *err;
    unsigned long val;

    if (!(priv->flag && FS_INUSE)) {
        val = simple_strtoul(buf, &err, 0);
        if ((err && !isspace(*err)) || (val > 1))
                return -EINVAL;

        priv->priority = val;

        return (count);
    } else {
        return (-EACCES);
    }
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - parmdata_show.                              */
/*                                                          */
/* Function   - Show whether parameter data messages are    */
/*              supported.                                  */
/*                                                          */
/* Parameters - *dev  - A(struct device)                    */
/*              *buf  - Buffer to place result              */
/*                                                          */
/************************************************************/

static ssize_t
parmdata_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    FSIUCV_Dev *priv = (FSIUCV_Dev *) dev_get_drvdata(dev);

    return (sprintf(buf, "%d\n", priv->parmdata));
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - parmdata_set.                               */
/*                                                          */
/* Function   - Set whether the connection will support     */
/*              parmdata messages.                          */
/*                                                          */
/* Parameters - *dev  - A(struct device)                    */
/*              *buf  - Source data                         */
/*              count - Size of source data                 */
/*                                                          */
/************************************************************/

static ssize_t
parmdata_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    FSIUCV_Dev *priv = (FSIUCV_Dev *) dev_get_drvdata(dev);
    char *err;
    unsigned long val;

    if (!(priv->flag && FS_INUSE)) {
        val = simple_strtoul(buf, &err, 0);
        if ((err && !isspace(*err)) || (val > 1))
            return -EINVAL;

        priv->parmdata = val;

        return (count);
    } else {
        return (-EACCES);
    }
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - msglimit_show.                              */
/*                                                          */
/* Function   - Show then message limit setting.            */
/*                                                          */
/* Parameters - *dev  - A(struct device)                    */
/*              *buf  - Buffer to place result              */
/*                                                          */
/************************************************************/

static ssize_t
msglimit_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	FSIUCV_Dev *priv = (FSIUCV_Dev *) dev_get_drvdata(dev);

	return (sprintf(buf, "%d\n", priv->msglimit));
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - msglimit_set.                               */
/*                                                          */
/* Function   - Set the maximum number of outstanding  	    */
/*              messages.                          	    */
/*                                                          */
/* Parameters - *dev  - A(struct device)                    */
/*              *buf  - Source data                         */
/*              count - Size of source data                 */
/*                                                          */
/************************************************************/

static ssize_t
msglimit_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	FSIUCV_Dev *priv = (FSIUCV_Dev *) dev_get_drvdata(dev);
	char *err;
	unsigned long val;

	if (!(priv->flag && FS_INUSE)) {
		val = simple_strtoul(buf, &err, 0);
		if ((err && !isspace(*err)) || (val > 255))
			return -EINVAL;

		priv->msglimit = val;

		return (count);
	} else {
		return (-EACCES);
	}
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - lokal_show.                                 */
/*                                                          */
/* Function   - Show whether only local connections are     */
/*              supported.                                  */
/*                                                          */
/* Parameters - *dev  - A(struct device)                    */
/*              *buf  - Buffer to place result              */
/*                                                          */
/************************************************************/

static ssize_t
lokal_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	FSIUCV_Dev *priv = (FSIUCV_Dev *) dev_get_drvdata(dev);

	return (sprintf(buf, "%d\n", priv->local));
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - lokal_set.                                  */
/*                                                          */
/* Function   - Set whether the connection will support     */
/*              local only connections.                     */
/*                                                          */
/* Parameters - *dev  - A(struct device)                    */
/*              *buf  - Source data                         */
/*              count - Size of source data                 */
/*                                                          */
/************************************************************/

static ssize_t
lokal_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	FSIUCV_Dev *priv = (FSIUCV_Dev *) dev_get_drvdata(dev);
	char *err;
	unsigned long val;

	if (!(priv->flag && FS_INUSE)) {
		val = simple_strtoul(buf, &err, 0);
		if ((err && !isspace(*err)) || (val > 1))
			return -EINVAL;

		priv->local = val;

		return (count);
	} else {
		return (-EACCES);
	}
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - status_show.                                */
/*                                                          */
/* Function   - Show status of device.                      */
/*                                                          */
/* Parameters - *dev  - A(struct device)                    */
/*              *buf  - Buffer to place result              */
/*                                                          */
/************************************************************/

static ssize_t
status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	FSIUCV_Dev *priv = (FSIUCV_Dev *) dev_get_drvdata(dev);

	return (sprintf(buf, "%02x\n", priv->flag));
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - fsiucv_callback_connreq.                    */
/*                                                          */
/* Function   - Process the IUCV connection request inter-  */
/*              rupt.                                       */
/*                                                          */
/* Parameters - *iucvPath - IUCV path control block         */
/*              vmId - Name of virtual machine connecting   */
/*              userd - User data associated with connection*/
/*                                                          */
/************************************************************/

static int
fsiucv_callback_connreq(struct iucv_path *iucvPath, u8 vmId[8], u8 userd[16])
{
	FSIUCV_Dev *pDev = &iucvDev[0];
	int iDev;

	PDEBUG("connection pending\n");
	for (iDev = 0; iDev < maxconn; iDev++, pDev++) {
		if ((pDev->flag & FS_CWAIT) &&
		    (memcmp(userd, pDev->prog, sizeof(userd)) == 0)) {
			PDEBUG("connection is for us from %s\n",vmId);
			pDev->path = iucvPath;
			iucvPath->private = pDev;
			memcpy(pDev->user, vmId, sizeof(pDev->user)-1);
			pDev->flag &= ~FS_CWAIT;
			atomic_set_mask(CPEND, pDev->intType);
			wake_up(&pDev->waitq);
			return(0);
		}
	}
	return (1);
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - fsiucv_callback_connrej.                    */
/*                                                          */
/* Function   - Process the IUCV severed interrupt.         */
/*                                                          */
/* Parameters - *iucvPath - IUCV path control block         */
/*              userd - User data associated with connection*/
/*                                                          */
/************************************************************/

static void
fsiucv_callback_connrej(struct iucv_path *iucvPath,  u8 userd[16])
{
	FSIUCV_Dev *dev = iucvPath->private;

	PDEBUG("Path severed - %p\n",dev);
	if (dev != NULL) {
		atomic_set_mask(CSVRD, dev->intType);
		wake_up(&dev->waitq);
	}
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - fsiucv_callback_connsusp.                   */
/*                                                          */
/* Function   - Process the IUCV quiesced interrupt.        */
/*                                                          */
/* Parameters - *iucvPath - IUCV path control block         */
/*              userd - User data associated with connection*/
/*                                                          */
/************************************************************/

static void
fsiucv_callback_connsusp(struct iucv_path *iucvPath, u8 userd[16])
{
	FSIUCV_Dev *dev = iucvPath->private;

	atomic_set_mask(CQSCD, dev->intType);
	wake_up(&dev->waitq);
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - fsiucv_callback_connres.                    */
/*                                                          */
/* Function   - Process the IUCV resumed interrupt.         */
/*                                                          */
/* Parameters - *iucvPath - IUCV path control block         */
/*              userd - User data associated with connection*/
/*                                                          */
/************************************************************/

static void
fsiucv_callback_connres(struct iucv_path *iucvPath, u8 userd[16])
{
	FSIUCV_Dev *dev = iucvPath->private;

	atomic_set_mask(CRSMD, dev->intType);
	wake_up(&dev->waitq);
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - fsiucv_callback_rx.                         */
/*                                                          */
/* Function   - Process the IUCV receive pending interrupt. */
/*                                                          */
/* Parameters - *iucvPath - IUCV path control block         */
/*              *msg - IUCV message control block           */
/*                                                          */
/************************************************************/

static void
fsiucv_callback_rx(struct iucv_path *iucvPath, struct iucv_message *msg)
{
	FSIUCV_Dev *dev = iucvPath->private;

	PDEBUG("message pending\n");
	if (dev->rcvq == NULL) {
		PDEBUG("1st eib\n");
		dev->rcvq = kmalloc(sizeof (EIB_t), GFP_KERNEL);
		if (dev->rcvq) {
			dev->rcvq->next = NULL;
			memcpy(&dev->rcvq->msg, msg,
			       sizeof (struct iucv_message));
		} else
			PDEBUG("No memory for EIB\n");
	} else {
		EIB_t *last = dev->rcvq;

		while (last->next != NULL)
			last = last->next;
		last->next = kmalloc(sizeof (EIB_t), GFP_KERNEL);
		if (last->next) {
			PDEBUG("add eib to q\n");
			last = last->next;
			last->next = NULL;
			memcpy(&last->msg, msg, sizeof (struct iucv_message));
		}
	}
	atomic_set_mask(MPEND, dev->intType);
	PDEBUG("mask=%08x\n", atomic_read(dev->intType));
	wake_up(&dev->waitq);
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - fsiucv_callback_connack.                    */
/*                                                          */
/* Function   - Process the IUCV connection complete 	    */
/* 		interrupt. 				    */
/*                                                          */
/* Parameters - *iucvPath - IUCV path control block         */
/*              userd - User data associated with connection*/
/*                                                          */
/************************************************************/

static void
fsiucv_callback_connack(struct iucv_path *iucvPath, u8 userd[16])
{
	FSIUCV_Dev *dev = iucvPath->private;

	PDEBUG("connection complete\n");
	atomic_set_mask(CCOMP, dev->intType);
	PDEBUG("mask=%08x\n", atomic_read(dev->intType));
	wake_up(&dev->waitq);
}

/*===================== End of Function ====================*/

/************************************************************/
/*                                                          */
/* Name       - fsiucv_callback_txdone.                     */
/*                                                          */
/* Function   - Process the IUCV send complete interrupt.   */
/*                                                          */
/* Parameters - *iucvPath - IUCV path control block         */
/*              *msg - IUCV message control block           */
/*                                                          */
/************************************************************/

static void
fsiucv_callback_txdone(struct iucv_path *iucvPath, struct iucv_message *msg)
{
	FSIUCV_Dev *dev = iucvPath->private;

	PDEBUG("txdone\n");
	PDEBUG(" ... dev - %p\n",dev);
        if (dev != NULL) {
                atomic_set_mask(MCOMP, dev->intType);
                wake_up(&dev->waitq);
        }
}

/*===================== End of Function ====================*/
