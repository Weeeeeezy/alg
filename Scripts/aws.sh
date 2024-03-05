#!/bin/bash
# ./iproutes.sh <interface name> <ip> <ip> <ip> ...
i=0
nif=eth0
default="$(ip route list default | awk '{print $3}')"
while (( "$#" )); do
  if [ $i -eq 0 ]
  then
    nif=$1
  else
    echo $i $1 >> /etc/iproute2/rt_tables
    echo ip address add $1/20 dev $nif
    ip address add $1/20 dev $nif
    echo ip route add table $1 default via $default dev $nif src $1
    ip route add table $1 default via $default dev $nif src $1
    echo ip rule add fwmark $i from 0.0.0.0 table $1
    ip rule add fwmark $i from 0.0.0.0 table $1
  fi
  ((i+=1))
  shift
done

