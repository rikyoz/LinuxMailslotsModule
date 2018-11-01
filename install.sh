#!/bin/bash
insmod mailslot.ko

#reading major number of the mailslot driver
mailslot_major=$( cat /proc/devices | grep mailslot | cut -d " " -f 1 )

#inserting device file in /dev/
mknod -m 666 /dev/mailslot0 c $mailslot_major 0
mknod -m 666 /dev/mailslot1 c $mailslot_major 1
mknod -m 666 /dev/test_mailslot c $mailslot_major 2