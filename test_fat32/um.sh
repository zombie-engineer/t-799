#!/bin/bash
sudo umount /tmp/bb
sudo losetup -d /dev/loop30
ls -la /tmp/bb
losetup | grep loop30
