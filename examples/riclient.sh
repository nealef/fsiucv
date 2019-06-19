#!/bin/sh
echo "RISERVER" >/sys/devices/iucv/iucv0/program_name
echo "NEALE" >/sys/devices/iucv/iucv0/target_user
echo "IBMISV" >/sys/devices/iucv/iucv0/target_node
./riclient
