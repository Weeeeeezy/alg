#! /bin/bash
#============================================================================#
#               "TradingStrats/MM-Single-ML/MM-Single-ML.sh":              #
#============================================================================#
if [ "$1" == "" ]
then
  Date=`date +"%Y-%m-%d"`
else
  Date="$1"
fi

cd /opt/MAQUETTE/log/MM_Single_ML

grep $Date MM_Single_ML.*txt | awk -f /home/admin/Bin/MM_Single_ML.awk

