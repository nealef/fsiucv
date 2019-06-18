#!/bin/sh
modprobe iucv
modprobe fsiucv
sleep 1
echo "*MSG" >/sys/devices/iucv/iucv0/target_user
