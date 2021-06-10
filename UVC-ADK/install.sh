#!/bin/bash

mkdir install
mkdir -p install/lib/uclibc
mkdir -p install/lib/glibc

cp -r include/ install/

make LIBTYPE=muclibc
cp libadk_usbcamera.a install/lib/uclibc/
make clean
make
cp libadk_usbcamera.a install/lib/glibc/
