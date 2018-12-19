<h1 align="center">
Linux Mailslots Module
</h1>

<h3 align="center">Windows mailslots for the Linux kernel</h3>

<p align="center">
  <a href="https://github.com/rikyoz/LinuxMailslotsModule/releases/latest"><img src="https://img.shields.io/github/release/rikyoz/LinuxMailslotsModule/all.svg?style=flat-square&logo=github&logoColor=white&colorB=blue" alt="GitHub release"></a>
  <img src="https://img.shields.io/badge/OS-Linux-red.svg?style=flat-square&logo=linux&logoColor=white" alt="MSVC version">
  <img src="https://img.shields.io/badge/arch-x86,%20x86__64-orange.svg?style=flat-square&logo=data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAQAAAAAYLlVAAAC1UlEQVR42u3WA9AjWRSA0bu2bdu2bdssrm3btm3btm3bmX+Ms7rLTiW9NUmvcsL7XuELu6Ojoz5DWcc5nvKp2kBdPvesi21m1Pgr7OxjrfWtgw0VZZjKM9rjXfNHM+bWWzutGo2YSH/tNm+jgNe1XzdDR322V41Tox5D6K4qY0WRtVRnjyhysercH0VeVJ13o8hXqvNNFOlSna4oUlOd2r8moBPwoQfd6THfoLweauqp6aJ8wInmMmjujWAFtwMeNJup5cXsVnWYDyDtajQjmMp7QOoypxGMbMtyAe+Ztf5/JTaJAkM6mjRXrj0KpE9zdZIyAV8bLX5lBIPlszXAVlGXMwAr5fwskL4wdPzAfGUC5o9kJy+o+dCVloiwJNg2907wimddZrqcB9GtNQF3RXI+kI5yCcgADwF6yvfLNa0JWD7n5dWXAa4lbZwrR7UioKdhc76vdEB+KxzbioAncxpGr9IBM+XKDa0IuCanaWkS8BzguEhqrQg4P6e5mgasbV+7WCySvWlFwIU5zdYooMhytCbghpzGLh9gAodCWjFXXwDSV4aJH5inWcBLkbzTOMBa9rWvk92jH5BWqBvwjSHKBfQ3as4HlvoSFq2b+zcB6bXIj6pZABvnPKzPgPSJlxV/hkUH5v7SUPiv2LN5wKuRjO82wDdON6xFSwW8XvhdcGYkrzUPYJf4lcktZh4jxg8sViqA9SKZxDo2NH0km1ImgE2jDjuBLXK6FPX1N1fUYQnKBnCeGeN3jGdPfUC+P27TyO7GjN8xoUMpHZCecKZ97etE9+hD6vKQOz1jgMa6u90J+VO9V//OaXnzgE5Al+p0iyLfqM63UeRV1Xk/ilylOo9Gkc1U55AoMrz+qjJJ1OMQ1bgq6jOYr1Rh9EgFZtd+q0QjVtFeW0UzFvGJ9uhhrSjDSE7UX6tdaMIoz0R2cbvXfKE2UJevvOEe+5kuOjr+qb4H0/HV/SQ0YjEAAAAASUVORK5CYII=" alt="Architectures">
  <a href="https://github.com/rikyoz/bit7z/blob/master/LICENSE"><img src="https://img.shields.io/badge/license-GNU%20GPL%20v2-lightgrey.svg?style=flat-square&logo=data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAQAAAAAYLlVAAAAvUlEQVR42u3Zt1EEURRE0Ye0gFwI4BWJoC0KGxftExk+RQATAN+nLpo1R0+v6NsJHPOLcO4vKkrPVexEcMFZN8AQ7UXwCBx0ART6VtiN4BCA1AHO+SnVAEg1AFINgFQDINUASDUAUg2AVAMg1QBINQBSDYBUAyDVAMhxAVfUdzkmYJ+7mj1xPQ7gjbWYJTmQbGsB77zy0mjPPQH9UwOKAQYYYIABBhhggAFzCTDAAAMMMGDS+v2a9V8Vzs1PH+dRolvEzoAoAAAAAElFTkSuQmCC" alt="License"></a>
</p>
<p align="center">
  <a href="#features">Features</a> •
  <a href="#building">Building</a> •
  <a href="#usage">Usage</a> •
  <a href="#license-gpl-v2">License</a>
</p>

This project provides a simple and easy to use Linux kernel module implementing a service similar to Windows mailslots, i.e. a driver for special device files accessible according to FIFO policy.

Developed for the course of *Advanced Operating Systems and Virtualization* (AOSV) taken in the A.Y. 2016/2017 for the *Master Degree in Engineering in Computer Science* (MSE-CS) at *Sapienza University of Rome*.
The original project specification can be found [here](https://www.dis.uniroma1.it/~quaglia/DIDATTICA/AOSV/examination.html).

## Features
The module is a Linux driver for special device files supporting the following features:

+ **FIFO** (**F**irst **I**n **F**irst **O**ut) access policy semantic (via *open/close/read/write* services).
+ **Atomic** message read/write, i.e. any segment read from or written to the file stream is seen as an independent data unit, a message, and it is posted/delivered atomically (all or nothing).
+ Support to **multiple instances** accessible concurrently by active processes/threads.
+ **Blocking/Non-Blocking** runtime behaviour of I/O sessions (tunable via *open* or *ioctl* commands)
+ Runtime configuration (via ioctl) of the following parameters:
  + *Maximum message size* (configurable up to an absolute upper limit).
  + *Maximum mailslot storage size* which is dynamically reserved to any individual mailslot.
+ Compile-time configuration of the following parameters:
  + *Range of device file minor numbers* supported by the driver (default: [0-255]).
  + *Number of mailslot instances* (default: 256).

## Building

The project provides a Makefile and can be compiled using the `make` command line utility.

## Usage

Once built, the module can be manually installed using the `insmod mailslot.ko` command (or via the `install.sh` utility shell script, which installs the module and creates also 3 mailslots files in the `/dev/` directory).
Mailslot files can be created using the `mknod` command (for example usages, please see the `install.sh` script),

In order to uninstall the module, the `rmmod mailslot` command must be used, as well as mailslot files can be removed using the `rm` command (if the installation script was used, the module can also be uninstalled using the provided `uninstall.sh` shell script, which removes also the 3 mailslots files created during the installation).

## License (GPL v2)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

<br/>
<div align="center">

Copyright &copy; 2017 - 2018 Riccardo Ostani ([@rikyoz](https://github.com/rikyoz))

</div>
