#! /bin/bash
#=============================================================================#
#                    "TradingStrats/Pairs-MF/WT-CAESAR.sh":                   #
#       Script for Starting and Stopping the WT-Pairs-MF(CAESAR) Server       #
#=============================================================================#
if [ "$2" == "Debug" ]
then
  Server=WT-Pairs-MF.d
  Port=443
else
  # Generic Case:
  Server=WT-Pairs-MF
  Port=443
fi
#=============================================================================#
if [ $EUID -ne 0 ]
then
  echo "ERROR: Must be root"
  exit 1
fi
MQTDir=/opt/MAQUETTE
LogDir="$MQTDir"/log/CAESAR

# Cretae the "run" dir if it does not exist ($MQTDir/run is a symlink to it):
mkdir -p /run/MAQUETTE

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
      --config            $MQTDir/share/WT-CAESAR.xml \
      --servername        $Server          \
      --docroot           $MQTDir/share/Wt \
      --approot           $MQTDir/share \
      --accesslog         $LogDir/WT-CAESAR.acclog    \
      --pid-file          $MQTDir/run/WT-CAESAR.pid   \
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
      $Server $Args >& $LogDir/WT-CAESAR.out &
    fi
    ;;
  stop)
    killall -2 "$Server"
    ;;
  *)
    echo "Arg must be {start|stop}"
    exit 1
esac
