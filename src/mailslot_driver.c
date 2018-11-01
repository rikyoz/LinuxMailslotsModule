/*
Copyright (C) 2017-2018  Riccardo Ostani.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "mailslot_driver.h"
#include "mailslot.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>      /* for char device functions */
#include <linux/cdev.h>    /* for cdev handling functions */
#include <linux/sched.h>   /* for current pointer */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Riccardo Ostani");
MODULE_DESCRIPTION("Mail slots for Linux");
MODULE_VERSION("1.0");

#define DEV_NAME     "mailslot" /* name of the device driver */
#define BASE_MINOR   0          /* base minor number */
#define INSTANCES    256        /* number of mailslot instances supported */

static int major; /* major number assigned to the mailslot device driver */
static dev_t dev;
static struct cdev* ms_cdev;

static mailslot_t* mailslot[ INSTANCES ] = { NULL };

static ssize_t ms_write( struct file* filp, const char __user* buffer, size_t size, loff_t* ofst ) {
    int result, has_lock;
    int non_blocking = filp->f_flags & O_NONBLOCK;
    int slot_id = iminor( filp->f_path.dentry->d_inode ) ;
    mailslot_t* slot = mailslot[ slot_id - BASE_MINOR ];

    if ( size == 0 ) {
        printk( KERN_INFO "mailslot (id %d): [write] pid %d tried to write a 0-size msg\n", slot_id, current->pid );
        return 0;
    }

    if ( buffer == NULL ) {
        printk( KERN_INFO "mailslot (id %d): [write] pid %d tried to write a NULL msg\n", slot_id, current->pid );
        return -EFAULT;
    }

write:
    has_lock = mailslot_lock( slot, non_blocking );
    if ( !has_lock ) {
        printk( KERN_ERR "mailslot (id %d): [write] pid %d couldn't acquire lock\n", slot_id, current->pid );
        return non_blocking ? -EAGAIN : -EINTR;
    }
    printk( KERN_INFO "mailslot (id %d): [write] lock acquired (pid %d)\n", slot_id, current->pid );

    result = mailslot_enqueue( slot, buffer, size, non_blocking );

    mailslot_unlock( slot );
    printk( KERN_INFO "mailslot (id %d): [write] slot unlocked (pid %d)\n", slot_id, current->pid );

    if ( result > 0 ) { /* the message was correctly enqueued! */
        mailslot_notify_msg( slot );
    } else if ( result == -ENOSPC ) { /* slot is full! */
        if ( non_blocking ) { /* the write would block but we must not! */
            result = -EAGAIN;
        } else {
            result = mailslot_wait_space( slot );
            if ( result == 0 ) { /* now there's space for the message */
                goto write; /* try again to write the message */
            } else { /* sleep was interrupted by a signal! */
                result = -EINTR;
            }
        }
    }
    return result;
}

static ssize_t ms_read( struct file* filp, char __user* buffer, size_t size, loff_t* ofst ) {
    int result, has_lock;
    int non_blocking = filp->f_flags & O_NONBLOCK;
    int slot_id = iminor( filp->f_path.dentry->d_inode );
    mailslot_t* slot = mailslot[ slot_id - BASE_MINOR ];

    if ( size == 0 ) {
        printk( KERN_INFO "mailslot (id %d): [read] pid %d tried to read to 0-size buffer\n", slot_id, current->pid );
        return 0;
    }

    if ( buffer == NULL ) {
        printk( KERN_INFO "mailslot (id %d): [read] pid %d tried to read to a NULL buffer\n", slot_id, current->pid );
        return 0;
    }

read:
    has_lock = mailslot_lock( slot, non_blocking );
    if ( !has_lock ) {
        printk( KERN_ERR "mailslot (id %d): [read] pid %d couldn't acquire lock\n", slot_id, current->pid );
        return non_blocking ? -EAGAIN : -EINTR;
    }
    printk( KERN_INFO "mailslot (id %d): [read] lock acquired (pid %d)\n", slot_id, current->pid );

    result = mailslot_dequeue( slot, buffer, size, non_blocking );

    mailslot_unlock( slot );
    printk( KERN_INFO "mailslot (id %d): [read] slot unlocked (pid %d)\n", slot_id, current->pid );

    if ( result > 0 ) { /* a message was correctly dequeued! */
        mailslot_notify_space( slot );
    } else if ( result == 0 ) { /* slot is empty! */
        if ( non_blocking ) { /* the read would block but we must not! */
            result = -EAGAIN;
        } else {
            result = mailslot_wait_msg( slot );
            if ( result == 0 ) { /* now there's a message to read! */
                goto read; /* try again to read a message */
            } else {
                result = -EINTR;
            }
        }
    }
    return result;
}

static long ms_unlocked_ioctl( struct file* filp, unsigned cmd, unsigned long arg ) {
    int slot_id = iminor( filp->f_path.dentry->d_inode );
    mailslot_t* slot = NULL;

    switch ( cmd ) {
        case MAILSLOT_SET_NONBLOCKING: /* per session setting */
            printk( KERN_INFO "mailslot (id %d): [ioctl] IO is now ", slot_id );
            if ( arg ) {
                filp->f_flags |= O_NONBLOCK;
                printk( KERN_CONT "non-" );
            } else {
                filp->f_flags &= ~O_NONBLOCK;
            }
            printk( KERN_CONT "blocking for pid %d\n", current->pid );
            break;

        case MAILSLOT_SET_MAX_MSG_SIZE: /* per slot setting */
            if ( arg == 0 || arg > LIMIT_MAX_MSG_SIZE ) {
                printk( KERN_ERR "mailslot (id %d): [ioctl] invalid max message size\n", slot_id );
                return -EINVAL;
            } else {
                slot = mailslot[ slot_id - BASE_MINOR ];
                mailslot_lock( slot, filp->f_flags & O_NONBLOCK );
                mailslot_set_max_msg_size( slot, arg );
                mailslot_unlock( slot );
                printk( KERN_INFO "mailslot (id %d): [ioctl] max msg size set to %lu chars\n", slot_id, arg );
            }
            break;

        default:
            printk( KERN_INFO "mailslot (id %d): [ioctl] invalid command code %u\n", slot_id, cmd );
            return -ENOTTY;
    }
    return 0;
}

static int ms_open( struct inode* inode, struct file* filp ) { return 0; }

static int ms_release( struct inode* inode, struct file* filp ) { return 0; }

static struct file_operations ms_fops = {
    .read           = ms_read,
    .write          = ms_write,
    .unlocked_ioctl = ms_unlocked_ioctl,
    .open           = ms_open,
    .release        = ms_release,
    .owner          = THIS_MODULE
};

void delete_slots( void ) {
    int i;
    for ( i = 0; i < INSTANCES; i++ ) {
        if ( mailslot[i] == NULL ) {
            return;
        }
        mailslot_free( mailslot[i] );
    }
}

int init_module( void ) {
    int i, error;

    /* allocating and initializing mailslots */
    for ( i = 0; i < INSTANCES; ++i ) {
        mailslot[i] = mailslot_alloc();
        if ( mailslot[i] == NULL ) {
            printk( KERN_ERR "mailslot: failed to allocate memory for slot %d\n", i + BASE_MINOR );
            error = -ENOMEM;
            goto fail_alloc;
        }
        /* the id of the slot is the minor number, not the index in the array of the instances! */
        mailslot_init( mailslot[i], i + BASE_MINOR );
    }

    /* allocating char device minor numbers in range [BASE_MINOR, BASE_MINOR + INSTANCES - 1] */
    error = alloc_chrdev_region( &dev, BASE_MINOR, INSTANCES, DEV_NAME );
    if ( error ) {
        printk( KERN_ERR "mailslot: failed to register char device numbers\n" );
        goto fail_alloc;
    }

    major = MAJOR( dev );

    /* registering the char device */
    ms_cdev = cdev_alloc();
    if ( !ms_cdev ) {
        printk( KERN_ERR "mailslot: failed to allocate cdev\n" );
        error = -ENOMEM;
        goto fail_cdev_alloc;
    }
    cdev_init( ms_cdev, &ms_fops );
    error = cdev_add( ms_cdev, dev, INSTANCES );
    if ( error ) {
        printk( KERN_ERR "mailslot: failed to add cdev to the system\n" );
        goto fail_cdev_add;
    }

    printk( KERN_INFO "mailslot: device registered successfully (major number: %d)\n", major );
    return 0;

fail_cdev_add: cdev_del( ms_cdev );
fail_cdev_alloc: unregister_chrdev_region( dev, INSTANCES );
fail_alloc: delete_slots();
    return error;
}

void cleanup_module( void ) {
    cdev_del( ms_cdev );
    unregister_chrdev_region( dev, INSTANCES );
    delete_slots();
    printk( KERN_INFO "mailslot: device unregistered successfully (major number: %d)\n", major );
}