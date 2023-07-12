#!/bin/sh
echo "*VMEVENTS" >/sys/devices/iucv/iucv0/target_user
./vmevents
