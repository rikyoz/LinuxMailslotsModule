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

#ifndef MAILSLOT_H
#define MAILSLOT_H

#include <linux/kernel.h>

#define DEFAULT_MAX_MSG_SIZE 256 /* default max size of a message data-unit */
#define LIMIT_MAX_MSG_SIZE   512 /* upper limit to the max size of a message data-unit */
#define MAX_SLOT_SIZE        64  /* max number of messages storable in a mailslot */

typedef struct mailslot mailslot_t;

/* Allocates a mailslot struct. */
mailslot_t* mailslot_alloc( void );

/* Initilizes the fields of a mailslot. */
void mailslot_init( mailslot_t* slot, int id );

/* Enqueues a message in a slot.
 * Note: it must be called in a critical section (e.g. after a mailslot_lock) */
ssize_t mailslot_enqueue( mailslot_t* slot, const char* content, size_t size, int non_blocking );

/* Dequeues the oldest message in the slot.
 * Note: it must be called in a critical section (e.g. after a mailslot_lock) */
ssize_t mailslot_dequeue( mailslot_t* slot, char* buffer, size_t size, int non_blocking );

/* Locks the access to the slot.
 * It returns 1 if lock was acquired, 0 otherwise. */
int mailslot_lock( mailslot_t* slot, int non_blocking );

/* Unlocks the access to the slot. */
void mailslot_unlock( mailslot_t* slot );

/* Makes the caller sleep and wait for a message to be written in the slot. */
int mailslot_wait_msg( mailslot_t* slot );

/* Makes the caller sleep and wait for space availability in the slot. */
int mailslot_wait_space( mailslot_t* slot );

/* Wakes up all processes waiting for new messages in the slot. */
void mailslot_notify_msg( mailslot_t* slot );

/* Wakes up all processes waiting for space in the slot. */
void mailslot_notify_space( mailslot_t* slot );

/* Sets the max message size allowed in the slot. */
void mailslot_set_max_msg_size( mailslot_t* slot, size_t size );

/* Frees the mailslot memory. */
void mailslot_free( mailslot_t* slot );

/* Prints the content of the slot fifo queue. */
void mailslot_printqueue( mailslot_t* slot );

#endif