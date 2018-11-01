CONFIG_MODULE_SIG=n
WARN := -W -Wall -Wstrict-prototypes -Wmissing-prototypes
ccflags-y := -O2

obj-m += mailslot.o
mailslot-objs := ./src/mailslot_driver.o ./src/mailslot.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
