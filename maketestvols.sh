#!/bin/bash
set -x

sudo vgcreate --verbose vg0 $1
# make sure kernel isn't first
sudo lvcreate --verbose --extent 1 --name One vg0
# establish the initial kernel partiton
sudo lvcreate --verbose --size 16M --name kernel vg0
#  force fragmentation
sudo lvcreate --verbose --extent 1 --name Two vg0
#  extend kernel volume (fragmented)
sudo lvextend --verbose --size +12M vg0/kernel
#  force more fragmentation
sudo lvcreate --verbose --extent 1 --name Three vg0
#  extend kernel volume (fragmented)
sudo lvextend --verbose --size +8M vg0/kernel
#  force more fragmentation
sudo lvcreate --verbose --extent 1 --name Four vg0
#  extend kernel volume (fragmented)
sudo lvextend --verbose --size +12M vg0/kernel
#  force more fragmentation
sudo lvcreate --verbose --extent 1 --name Five vg0
#  extend kernel volume (fragmented)
sudo lvextend --verbose --size +16M vg0/kernel
