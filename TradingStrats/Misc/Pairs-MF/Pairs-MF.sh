#! /bin/bash
#============================================================================#
#                   "TradingStrats/Pairs-MF/Pairs-MF.sh":                    #
#============================================================================#
if [ "$1" == "" ]
then
  Date=`date +"%Y-%m-%d"`
else
  Date="$1"
fi

cd /opt/MAQUETTE/log/Pairs-MF

grep $Date Pairs-MF.*txt | awk -f /home/admin/Bin/Pairs-MF.awk

