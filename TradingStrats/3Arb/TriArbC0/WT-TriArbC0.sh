#! /bin/bash
#=============================================================================#
#                    "TradingStrats/TriArb2S/WT-TriArb2S.sh":                 #
#            Script for Starting and Stopping the WT-TriArb2S Server          #
#=============================================================================#
Server=WT-TriArb2S
Port=443

if [ $EUID -ne 0 ]
then
  echo "ERROR: Must be root"
  exit 1
fi
MQTDir=/opt/MAQUETTE
LogDir="$MQTDir"/log/TriArb2S

case "$1" in
  start)
    # Only start it if it is not running yet:
    pid=`pgrep -x $Server`
    if [ "$pid" != "" ]
    then
 .    echo "$Server server is already running (PID=$pid)"
      exit 1
    fi
    # Yes, start it:
    export \
      LD_LIBRARY_PATH=/opt/GCC/Curr/lib64:/opt/env-GCC/lib:"$LD_LIBRARY_PATH"
    ulimit -c unlimited
    $MQTDir/bin/GCC/$Server \
      --config            $MQTDir/share/WT-TriArb2S.xml \
      --servername        $Server          \
      --docroot           $MQTDir/share/Wt \
      --approot           $MQTDir/share \
      --accesslog         $LogDir/WT-TriArb2S.acclog   \
      --pid-file          $MQTDir/run/WT-TriArb2S.pid   \
      --threads           5       \
      --https-address     0.0.0.0 \
      --https-port        $Port   \
      --ssl-private-key   $MQTDir/share/MAQUETTE.key \
      --ssl-certificate   $MQTDir/share/MAQUETTE.crt \
      --ssl-tmp-dh        $MQTDir/share/MAQUETTE.dh  \
      >& $LogDir/WT-TriArb2S.out &
      ;;
  stop)
    killall -2 "$Server"
    ;;
  *)
    echo "Arg must be {start|stop}"
    exit 1
esac
