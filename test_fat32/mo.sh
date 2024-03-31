#!/bin/bash
DIR=/tmp/bb
[ $DIR -d ] || mkdir -pv $DIR && \
sudo losetup /dev/loop30 test.img && \
sudo mount /dev/loop30 $DIR && \
find $DIR
