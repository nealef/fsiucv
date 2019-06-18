fsiucv_mod-objs := fsiucv.o
VERSION=2.0
RELEASE=2

obj-m := fsiucv.o
LEVEL := `uname -r`
KDIR  := /usr/src/kernels/$(LEVEL)
PWD   := $(shell pwd)
DIST_FILES := ChangeLog fsiucv.c fsiucv.h fsiucv.spec HOW-TO \
		iucv.h Makefile propjr.c setup.sh LICENSE.TXT

prefix =
bindir = /usr/sbin

COMMAND =

all:	modules tools

modules:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

install:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules_install

clean:
	rm -rf *.o *~ core *.ko* *.cmd .tmp_versions propjr *.mod.c \
	    fsiucv-${VERSION}.${RELEASE} fsiucv-${VERSION}.${RELEASE}.tar.gz \
	    modules.order Modules.symvers

dist:	$(DIST_FILES)
	mkdir -p fsiucv-$(VERSION).$(RELEASE)
	cp $(DIST_FILES) fsiucv-$(VERSION).$(RELEASE)
	tar -czf fsiucv-$(VERSION).$(RELEASE).tar.gz fsiucv-$(VERSION).$(RELEASE)

tools: propjr

propjr: propjr.o
	$(CC) -o $@ $^

propjr.o: examples/propjr.c
	$(CC) -O2 -o propjr.o -I. -c examples/propjr.c
