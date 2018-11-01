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

#ifndef MAILSLOT_DRIVER_H
#define MAILSLOT_DRIVER_H

#include <linux/ioctl.h>

#define MAILSLOT_IOCTL_MAGIC 'x' /* unused 8-bit number in ioctl-number.txt */

#define MAILSLOT_SET_NONBLOCKING  _IOW( MAILSLOT_IOCTL_MAGIC, 0, unsigned int )
#define MAILSLOT_SET_MAX_MSG_SIZE _IOW( MAILSLOT_IOCTL_MAGIC, 1, unsigned int )

#endif