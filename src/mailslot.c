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

#include "mailslot.h"

#include <linux/slab.h>    /* for kzalloc */
#include <linux/mutex.h>   /* for mutex */
#include <linux/uaccess.h> /* for copy_to_user and copy_from_user functions */
#include <linux/wait.h>    /* for wait_queue */

typedef struct message {
    char* content;
    size_t size;
    struct message* next;
} message_t;

struct mailslot {
    struct mutex mutex;
    wait_queue_head_t rd_queue, wr_queue;
    message_t* head;
    message_t* tail;
    size_t max_msg_size;
    int msg_count;
    int id; /* needed only to help debugging! */
};

mailslot_t* mailslot_alloc( void ) {
    return kzalloc( sizeof( mailslot_t ), GFP_KERNEL );
}

void mailslot_init( mailslot_t* slot, int id ) {
    mutex_init( &( slot->mutex ) );
    init_waitqueue_head( &( slot->rd_queue ) );
    init_waitqueue_head( &( slot->wr_queue ) );
    slot->head = NULL;
    slot->tail = NULL;
    slot->max_msg_size = DEFAULT_MAX_MSG_SIZE;
    slot->id = id;
}

ssize_t mailslot_enqueue( mailslot_t* slot, const char* content, size_t size, int non_blocking ) {
    int error;
    int count = slot->msg_count;
    message_t* msg = NULL;

    if ( count == MAX_SLOT_SIZE ) {
        printk( KERN_ERR "mailslot (id %d): cannot enqueue msg, slot is full\n", slot->id );
        return -ENOSPC;
    }

    if ( size > slot->max_msg_size ) { /* all or nothing */
        printk( KERN_ERR "mailslot (id %d): cannot write msg, size (%lu) greater than max allowed by the slot (%lu)\n", slot->id, size, slot->max_msg_size );
        return -EPERM;
    }

    msg = kzalloc( sizeof( message_t ), non_blocking ? GFP_ATOMIC : GFP_KERNEL );
    if ( msg == NULL ) {
        printk( KERN_ERR "mailslot (id %d): failed to allocate space for the new msg\n", slot->id );
        return non_blocking ? -EAGAIN : -ENOMEM;
    }

    msg->content = kzalloc( size, non_blocking ? GFP_ATOMIC : GFP_KERNEL );
    if ( msg->content == NULL ) {
        printk( KERN_ERR "mailslot (id %d): failed to allocate space for the new msg's content\n", slot->id );
        kfree( msg );
        return non_blocking ? -EAGAIN : -ENOMEM;
    }

    if ( non_blocking ) { /* disabling the pagefault handler, so that copy_from_user won't sleep */
        pagefault_disable();
    }
    error = copy_from_user( msg->content, content, size );
    if ( non_blocking ) {
        pagefault_enable();
    }
    if ( error ) {
        printk( KERN_ERR "mailslot (id %d): failed to copy msg from user space\n", slot->id );
        kfree( msg->content );
        kfree( msg );
        return -EFAULT;
    }
    msg->size = size;
    msg->next = NULL;

    if ( count == 0 ) {
        slot->head = msg;
    } else {
        slot->tail->next = msg;
    }
    slot->tail = msg;
    slot->msg_count++;
    mailslot_printqueue( slot ); /* debug help */

    return size;
}

ssize_t mailslot_dequeue( mailslot_t* slot, char* buffer, size_t size, int non_blocking ) {
    int res, error;
    int count = slot->msg_count;
    message_t* msg = NULL;

    if ( count == 0 ) { /* not an error */
        printk( KERN_INFO "mailslot (id %d): no msg to read, empty slot\n", slot->id );
        return 0;
    }

    msg = slot->head;
    if ( msg->size > size ) { /* all or nothing */
        printk( KERN_ERR "mailslot (id %d): user buffer too small for the msg\n", slot->id );
        return -EMSGSIZE;
    }

    if ( non_blocking ) {
        pagefault_disable();
    }
    error = copy_to_user( buffer, msg->content, msg->size );
    if ( non_blocking ) {
        pagefault_enable();
    }
    if ( error ) {
        printk( KERN_ERR "mailslot (id %d): failed to copy msg to user space\n", slot->id );
        return -EFAULT;
    }

    res = msg->size;
    slot->head = slot->head->next; /* if count == 1, here head becomes NULL! */
    kfree( msg->content );
    kfree( msg );
    if ( count == 1 ) {
        slot->tail = NULL; /* here head == NULL and tail == (deallocated) msg */
    }
    slot->msg_count--;
    mailslot_printqueue( slot ); /* debug help */

    return res;
}

int mailslot_lock( mailslot_t* slot, int non_blocking ) {
    printk( KERN_INFO "mailslot (id %d): pid %d wants to lock the slot", slot->id, current->pid );
    if ( non_blocking ) {
        printk( KERN_CONT " without blocking\n" );
        return mutex_trylock( &(slot->mutex) );
    } else {
        printk( KERN_CONT "\n" );
        return mutex_lock_interruptible( &(slot->mutex) ) == 0;
    }
}

void mailslot_unlock( mailslot_t* slot ) {
    mutex_unlock( &(slot->mutex) );
}

int mailslot_wait_msg( mailslot_t* slot ) {
    return wait_event_interruptible_exclusive( slot->rd_queue, slot->msg_count > 0 );
}

int mailslot_wait_space( mailslot_t* slot ) {
    return wait_event_interruptible_exclusive( slot->wr_queue, slot->msg_count < MAX_SLOT_SIZE );
}

void mailslot_notify_msg( mailslot_t* slot ) {
    wake_up_interruptible( &(slot->rd_queue) );
}

void mailslot_notify_space( mailslot_t* slot ) {
    wake_up_interruptible( &(slot->wr_queue) );
}

void mailslot_set_max_msg_size( mailslot_t* slot, size_t size ) {
    slot->max_msg_size = size;
}

void mailslot_free( mailslot_t* slot ) {
    int i;
    message_t* msg = NULL;
    for ( i = 0; i < slot->msg_count; i++ ) {
        msg = slot->head->next;
        kfree( slot->head->content );
        kfree( slot->head );
        slot->head = msg;
    }
    kfree( slot );
}

void mailslot_printqueue( mailslot_t* slot ) {
    message_t* msg = slot->head;
    printk( KERN_INFO "mailslot (id %d): (slot content) head = ", slot->id );
    while ( msg != NULL ) {
        printk( KERN_CONT "\"%s\"", msg->content );
        if ( msg->next != NULL ) {
            printk( KERN_CONT ", " );
        }
        msg = msg->next;
    }
    if ( msg == NULL ){
        printk( KERN_CONT " = tail\n" );
    }
}