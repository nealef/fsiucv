fsiucv_mod-objs := fsiucv.o
VERSION=3.0
RELEASE=1

obj-m := fsiucv.o
LEVEL := `uname -r`
KDIR  := /usr/src/linux-headers-$(LEVEL)
PWD   := $(shell pwd)
DIST_FILES := ChangeLog fsiucv.c fsiucv.h fsiucv.spec HOW-TO \
		iucv.h Makefile propjr.c setup.sh LICENSE.TXT

prefix =
bindir = /usr/sbin

COMMAND =

all:	modules tools

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

clean:
	rm -rf *.o *~ core *.ko* *.cmd .tmp_versions propjr *.mod.c \
	    fsiucv-${VERSION}.${RELEASE} fsiucv-${VERSION}.${RELEASE}.tar.gz \
	    modules.order Module.symvers .fsiucv.*.cmd vmevents fsiucv.mod

dist:	$(DIST_FILES)
	mkdir -p fsiucv-$(VERSION).$(RELEASE)
	cp $(DIST_FILES) fsiucv-$(VERSION).$(RELEASE)
	tar -czf fsiucv-$(VERSION).$(RELEASE).tar.gz fsiucv-$(VERSION).$(RELEASE)

tools: propjr vmevents

propjr: propjr.o
	$(CC) -o $@ $^

propjr.o: examples/propjr.c
	$(CC) -O2 -o propjr.o -I. -c examples/propjr.c

vmevents: vmevents.o
	$(CC) -o $@ $^

vmevents.o: examples/vmevents.c
	$(CC) -O2 -o vmevents.o -I. -c examples/vmevents.c
