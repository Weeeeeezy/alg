#!/bin/bash
# ./iproutes.sh <interface name> <ip> <ip> <ip> ...

i=0
nif=ens5
default="$(ip route list default | awk '{print $3}')"
while (( "$#" )); do
  if [ $i -eq 0 ]
  then
    nif=$1
  else
    ip=`echo $1 | sed 's/,//g'`
    echo $i $ip >> /etc/iproute2/rt_tables
    echo ip address add $ip/20 dev $nif
    ip address add $ip/20 dev $nif
    echo ip route add table $ip default via $default dev $nif src $ip
    ip route add table $ip default via $default dev $nif src $ip
    echo ip rule add fwmark $i from 0.0.0.0 table $ip
    ip rule add fwmark $i from 0.0.0.0 table $ip
  fi
  ((i+=1))
  shift
done
