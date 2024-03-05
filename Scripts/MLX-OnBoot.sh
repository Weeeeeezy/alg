#!/bin/bash
# vim: ts=2:et
#==============================================================================
# Description:   Performs MAQUETTE host initialization on boot
# Author:        Dr Leonid Merkin
# Created:       2015-04-05
#==============================================================================
# CONFIG: We can use hardware-based Receive Side Scaling (RSS) or software-based
# Receive Packet Steering (RPS). Currently, RSS is used:
UseRSS=1

# Run all Mellanox kernel processes at the highest RT priority, and on the
# specified CPU set:
for p in $(pgrep mlx4);
do
  /usr/bin/chrt -f -p 100 $p
done

# Map Mellanox IRQs to system CPUs: in our case (consistent with "isolcpus"
# kernel option):
# (*) system processes run on Cores 0..3 (CPUs: 0-3, 6- 9);
# (*) user-level processes on Cores 4..5 (CPUs: 4-5,10-11);
# "show_irq_affinity.sh mlx0" --> IRQs=65-89: map to: 0-3,6-9 (mask: 0x3cf)
# "show_irq_affinity.sh mlx1" --> IRQs=78-85: map to: 0-3,6-9 (mask: 0x3cf)
#
for irq in 65 \
           66 67 68 69 70 71 72 73 74 75 76 77 \
           78 79 80 81 82 83 84 85 \
           86 87 88 89
do
  echo 3cf > /proc/irq/"$irq"/smp_affinity
done

# Setting for Mellanox adaptors using "ethtool":
# NB: This is applicable to physical adapters only, not VLANs:
#
for interface in mlx0 mlx1
do
  # Request low-latency mode (XXX: this is related to power consumption; the
  # kernel already has power management for peripheral devices switched off,
  # so would the following make any difference? But it would not harm in any
  # case):
  # XXX: Not used with newer OFED FW?
# /usr/bin/ethtool --set-priv-flags $interface pm_qos_request_low_latency on

  # Switch interrupt moderation OFF, so interrupts on RX will be delivered as
  # soon as data are available, without coalescing. This improves latencty at
  # the expense of throughput:
  /usr/bin/ethtool -C $interface adaptive-rx off rx-usecs 0 rx-frames 0

  # The following are optiised RSS settings
  # (see also /etc/modprobe.d/MP.conf):
  # XXX: Not used with newer OFED FW?
# if [ $UseRSS -eq 1 ]
# then
#   /usr/bin/ethtool --set-priv-flags $interface mlx4_rss_xor_hash_function on
# fi
done

# XXX: RPS currently remains disabled. If we decide to enable it, specify same
# CPU masks as above to indicate the CPUs which can be used for RX queues pro-
# cessing for each Mellanox device.
if [ $UseRSS -eq 0 ]
then
  for f in /sys/class/net/mlx0/queues/rx-*/rps_cpus
  do
    echo 11 > $f
  done

  for f in /sys/class/net/mlx1/queues/rx-*/rps_cpus
  do
    echo 22 > $f
  done
fi

# NB: The following firmware optimisations need to be done ONLY ONCE, so they
# are commented out.    They apply to the whole Mellanox card (mlx4_0) rather
# than to invividual ports:
#
#/opt/bin/mstconfig -d mlx4_0 -y set SRIOV_EN=0 NUM_OF_VFS=0 LOG_BAR_SIZE=4

