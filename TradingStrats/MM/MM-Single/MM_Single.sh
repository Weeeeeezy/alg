#! /bin/bash
#============================================================================#
#                   "TradingStrats/MM-Single/MM-Single.sh":                  #
#============================================================================#
if [ "$1" == "" ]
then
  Date=`date +"%Y-%m-%d"`
else
  Date="$1"
fi

cd /opt/MAQUETTE/log/MM_Single

grep $Date MM_Single.*txt | awk -f /home/admin/Bin/MM_Single.awk

