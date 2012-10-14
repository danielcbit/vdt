#!/bin/bash
#
# This is executed in a chroot inside the mounted image
#
# Image wil be mounted to <setup>/mnt
#
# This sets IP address and hostname
#
# This file is for the node setup, i.e. it will be excuted once for EVERY
# image
#
# DO NOT edit this file. Do have additonal setup code put it in  a
# modify_image_node.sh image in your <setup> directory
#
# Parameter <image> <setup> <ip> <nodename>  <gateway> <dns> <ssh public key>
#
# (c) 2010 IBR

SETUP="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
NODENAME=$1
IMAGE=$2
IPADDR=$3
NETMASK="255.255.255.0"

echo "------- Node setup ---------------------------------------------------"

echo "Preparing image in $IMAGE with"
echo "Node name     : $NODENAME"
echo "IP Address    : $IPADDR"
echo "Netmask	    : $NETMASK"


if [ ! -e "$IMAGE" ]
then
   echo "$0: Can not find image  $IMAGE"
   exit -1
fi

mkdir -p "$SETUP/mnt/"
bash $SETUP/magicmount.sh "$IMAGE" "$SETUP/mnt/"

# switch to chroot
chroot "$SETUP/mnt/" /bin/sh <<EOF

uci set system.@system[0].hostname=$NODENAME
uci commit system.@system[0].hostname

# network configuration
/sbin/uci set network.lan.ipaddr=$IPADDR
/sbin/uci set network.lan.netmask=$NETMASK
/sbin/uci set network.lan.proto=static

/sbin/uci commit

# close chroot
EOF


#custom setup preparation
echo ">>>>> Custom image setup"
cp $SETUP/modify_image_node.sh "$SETUP/mnt"
chroot "$SETUP/mnt/" /bin/sh /modify_image_node.sh "$NODENAME"
echo ">>>>> Custom image setup done."

umount "$SETUP/mnt/"

until [ $? -eq 0 ]; do
	sleep 1
	umount "$SETUP/mnt/"
done

echo "------- Node setup done ------------------------------------------------"
