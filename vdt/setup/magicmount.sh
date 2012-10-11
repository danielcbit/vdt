#!/bin/bash
# Maaaaaaaaaaaagic mounter
#
# Usage: magicmount.sh openwrt.imagefile mountpoint

echo "Mounting $1 to $2"
if [ ! -e "$1" ]
then
   echo "No file $1"
   exit -1
fi

if [ ! -d "$2" ]
then
   echo "Not a directory  $2"
   exit -1
fi

res=`fdisk -u -l $1 | grep $1 | tail -n 1`
echo "res: "
echo $res
offset=`echo $res  | awk '{print $2}'`
echo $offset
let offset=$offset*512
echo $offset

mount -o loop,offset=$offset "$1" "$2"
res=$?

if [ $res != 0 ]
then
    echo "Error mounting image: $res"
    exit $res
fi

echo "Success!"
