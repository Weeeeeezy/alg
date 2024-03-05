#! /bin/bash
#=============================================================================#
#               "TradingStrats/MM-NTProgress/WT-MM-NTProgress.sh:"            #
#          Script for Starting and Stopping the WT-MM-NTProgress Server       #
#=============================================================================#
if [ "$2" == "Debug" ]
then
  Server=WT-MM-NTProgress.d
  Port=553
else
  # Generic Case:
  Server=WT-MM-NTProgress
  Port=543
fi
#=============================================================================#
if [ $EUID -ne 0 ]
then
  echo "ERROR: Must be root"
  exit 1
fi
MQTDir=/opt/MAQUETTE
LogDir="$MQTDir"/log/MM-NTProgress

case "$1" in
  start)
    # Only start it if it is not running yet:
    pid=`pgrep -x $Server`
    if [ "$pid" != "" ]
    then
      echo "$Server server is already running (PID=$pid)"
      exit 1
    fi
    # Yes, start it:
    export \
      LD_LIBRARY_PATH=/opt/GCC/Curr/lib64:/opt/env-GCC/lib:"$LD_LIBRARY_PATH"
    ulimit -c unlimited

    Server=$MQTDir/bin/GCC/$Server
    Args="\
      --config            $MQTDir/share/WT-MM-NTProgress.xml \
      --servername        $Server          \
      --docroot           $MQTDir/share/Wt \
      --approot           $MQTDir/share \
      --accesslog         $LogDir/WT-MM-NTProgress.acclog    \
      --pid-file          $MQTDir/run/WT-MM-NTProgress.pid   \
      --threads           5       \
      --https-address     0.0.0.0 \
      --https-port        $Port   \
      --ssl-private-key   $MQTDir/share/MAQUETTE.key \
      --ssl-certificate   $MQTDir/share/MAQUETTE.crt \
      --ssl-tmp-dh        $MQTDir/share/MAQUETTE.dh"

    if [ "$2" == "Debug" ]
    then
      # Run the Server under GDB:
      gdb $Server -ex "set args $Args"
    else
      # Generic Case:
      $Server $Args >& $LogDir/WT-MM-NTProgress.out &
    fi
    ;;
  stop)
    killall -2 "$Server"
    ;;
  *)
    echo "Arg must be {start|stop}"
    exit 1
esac
