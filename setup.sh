#!/bin/sh
rmmod fsiucv_mod 2>/dev/null
cp *.ko /lib/modules/`uname -r`/kernel/drivers/s390/char/
modprobe iucv
modprobe fsiucv_mod
sleep 1
echo "*MSG" >/sys/devices/iucv/iucv0/target_user
