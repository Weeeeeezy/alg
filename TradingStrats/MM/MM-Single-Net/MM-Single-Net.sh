#! /bin/bash
#============================================================================#
#               "TradingStrats/MM-Single-Net/MM-Single-Net.sh":              #
#============================================================================#
if [ "$1" == "" ]
then
  Date=`date +"%Y-%m-%d"`
else
  Date="$1"
fi

cd /opt/MAQUETTE/log/MM_Single_Net

grep $Date MM_Single_Net.*txt | awk -f /home/admin/Bin/MM_Single_Net.awk

