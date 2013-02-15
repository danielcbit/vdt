#!/bin/bash

cat /var/lib/libvirt/dnsmasq/default.leases | awk '{print $3}' > hosts
