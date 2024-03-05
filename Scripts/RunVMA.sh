#! /bin/bash
#============================================================================#
# "RunVMA.sh": Run an Application over Pre-Loaded libvma.so                  #
#============================================================================#
#----------------------------------------------------------------------------#
# Params:                                                                    #
#----------------------------------------------------------------------------#
# Run the internal VMA thread on the given CPU (eg an auxiliary one):
InternalThreadAffinity=7

# By default, we expect that the application to be invoked by this script  is
# Single-Threaded from the LibVMA point of view: there will be no concurrent
# calls to any Mellanox libraries. If this is not the case, the "-m" flag must
# be set:
MultiThreadedApp=0

# Do we want to run it under GDB and with extended VMA debugging?
Debug=0

# VMA Application ID (ConfigFile Section):
AppID=MOEX-10G

function usage() {
  echo "ERROR: Invalid option: $opt"
  echo "USAGE: [Option...] CommandLine"
  echo "Available Options:"
	echo "-p PollIterations"
  echo "-i InternalThreadCPU (default: $InternalThreadAffinity)"
  echo "-m (for multi-threaded apps; by default, off)"
  echo "-g (run under GDB)"
  exit 1
}

while getopts "i:p:mg" opt
do
  case $opt in
    i) InternalThreadAffinity="$OPTARG";;
		p) PollIters="$OPTARG";;
    m) MultiThreadedApp=1;;
    g) Debug=1;;
    *) usage;;
  esac
done

# Shoft to get to the Application Path:
shift $(($OPTIND-1))
ProgName=`which $1`
# Shift again to get to the Application Params:
shift

LSBase=/tmp/VMA-`basename "$ProgName"`-`date +'%Y%m%d-%H%M%S'`
StatsFile="$LSBase".stats
LogFile="$LSBase".log

#-----------------------------------------------------------------------------#
# VMA Settings in the Environment:                                            #
#-----------------------------------------------------------------------------#
if [ $Debug -eq 0 ]
then
	export VMA_TRACELEVEL=info
else
	export VMA_TRACELEVEL=debug
fi

export VMA_LOG_DETAILS=3
#Add details on each log line
#0 = Basic log line
#1 = ThreadId
#2 = ProcessId + ThreadId
#3 = Time + ProcessId + ThreadId
#   [Time is in milli-seconds from start of process]
#Default value is 0

export VMA_LOG_COLORS=0
export VMA_LOG_FILE="$LogFile"

#VMA_SPEC=latency
#VMA predefined specification profile for latency.
#Optimized for use cases that are keen on latency. i.e. Ping-Pong tests.
#
#Latency SPEC changes the following default configuration
# VMA_TX_WRE = 256                         (default: 3000)
# VMA_TX_WRE_BATCHING = 4                  (default: 64)
# VMA_RX_WRE = 256                         (default: 16000)
# VMA_RX_WRE_BATCHING = 4                  (default: 64)
# VMA_RX_POLL = -1                         (default: 100000)
# VMA_RX_PREFETCH_BYTES_BEFORE_POLL = 256  (default: 0)
# VMA_GRO_STREAMS_MAX = 0                  (default: 32)
# VMA_SELECT_POLL = -1                     (default: 100000)
# VMA_SELECT_POLL_OS_FORCE = Enable        (default: Disabled)
# VMA_SELECT_POLL_OS_RATIO = 1             (default: 10)
# VMA_SELECT_SKIP_OS = 1                   (default: 4)
# VMA_PROGRESS_ENGINE_INTERVAL = 100       (default: 10)
# VMA_CQ_MODERATION_ENABLE = Disable       (default: Enabled)
# VMA_CQ_AIM_MAX_COUNT = 128               (default: 560)
# VMA_CQ_AIM_INTERVAL_MSEC = Disable       (default: 250)
# VMA_CQ_KEEP_QP_FULL = Disable            (default: Enable)
# VMA_AVOID_SYS_CALLS_ON_TCP_FD = Enable   (default: Disable)
# VMA_INTERNAL_THREAD_AFFINITY = 0         (default: -1)
# VMA_THREAD_MODE = Single                 (default: Multi spin lock)
# VMA_MEM_ALLOC_TYPE = 2                   (default: 1 (Contig Pages))
# XXX: Still, we do NOT use this param; rather, select individual optimised vals
# by one...

#VMA_STATS_FILE=
#Redirect socket statistics to a specific user defined file.
#VMA will dump each socket's statistics into a file when closing the socket.

export VMA_STATS_SHMEM_DIR=/dev/shm/
#Set the directory path for vma to create the shared memory files for vma_stats.
#No files will be created when setting this value to empty string "".
#Default value is /tmp/

#VMA_STATS_FD_NUM
#Max number of sockets monitored by VMA statistic mechanism.
#Value range is 0 to 1024.
#Default value is 100

export VMA_CONFIG_FILE=/opt/etc/libvma.conf
#Sets the full path to the VMA configuration file.
#Default value is: /etc/libvma.conf

export VMA_APPLICATION_ID="$AppID"
#Specify a group of rules from libvma.conf for VMA to apply.
#Example: 'VMA_APPLICATION_ID=iperf_server'.
#Default is "VMA_DEFAULT_APPLICATION_ID" (match only the '*' group rule)

export VMA_CPU_USAGE_STATS=0
#Calculate VMA CPU usage during polling HW loops.
#This information is available through VMA stats utility.
#Default value is 0 (Disabled)

export VMA_HANDLE_SIGINTR=0
#When Enabled, VMA handler will be called when interrupt signal is sent to the
#process. VMA will also call to application's handler if exist.
#Value range is 0 to 1
#Default value is 0 (Disabled)

export VMA_HANDLE_SIGSEGV=1
#When Enabled, print backtrace if segmentation fault happens.
#Value range is 0 to 1
#Default value is 0 (Disabled)

#VMA_TX_SEGS_TCP
#Number of TCP LWIP segments allocation for each VMA process.
#Default value is 1000000

#VMA_TX_BUFS
#Number of global Tx data buffer elements allocation.
#Default value is 200000

export VMA_TX_WRE=256
#Number of Work Request Elements allocated in all transmit QP's.
#The number of QP's can change according to the number of network offloaded
#interfaces.
#Default value is 3000
#We use the Latency-Optimised value

export VMA_TX_WRE_BATCHING=64
#The number of Tx Work Request Elements used until a completion signal is
#requested.
#Tuning this parameter allows a better control of the jitter encountered from
#the Tx CQE handling. Setting a high batching value results in high PPS and
#lower average latency. Setting a low batching value results in lower latency
#std-dev.
#Value range is 1-64
#Default value is 64
#The recommended Low-Latency value was 4, but we found that 64 is much better

#VMA_TX_MAX_INLINE
#Max send inline data set for QP.
#Data copied into the INLINE space is at least 32 bytes of headers and
#the rest can be user datagram payload.
#VMA_TX_MAX_INLINE=0 disables INLINEing on the tx transmit path.
#In older releases this parameter was called: VMA_MAX_INLINE
#Default VMA_TX_MAX_INLINE is 220

export VMA_TX_MC_LOOPBACK=0
#This parameter sets the initial value used by VMA internally to controls the
#multicast loopback packets behavior during transmission.
#An application that calls setsockopt() with IP_MULTICAST_LOOP will run over
#the initial value set by this parameter.
#Read more in 'Multicast loopback behavior' in notes section below
#Default value is 1 (Enabled)
#XXX: We don't need multicast loopback???

export VMA_TX_NONBLOCKED_EAGAINS=1
#Return value 'OK' on all send operation done on a non-blocked udp sockets. This
#is the OS default behavior. The datagram sent is silently dropped inside VMA
#or the network stack.
#When set Enabled (set to 1), VMA will return with error EAGAIN if it was unable
#accomplish the send operation and the datagram was dropped.
#In both cases a dropped Tx statistical counter is incremented.
#Default value is 0 (Disabled)
#NB: Set it to 1, it is VERY important for us that such packets are NOT silently
#dropped

export VMA_TX_PREFETCH_BYTES=256
#Accelerate offloaded send operation by optimizing cache. Different values
#give optimized send rate on different machines. We recommend you tune this
#for your specific hardware.
#Value range is 0 to MTU size
#Disable with a value of 0
#Default value is 256 bytes
#XXX: Tune this!

export VMA_RING_ALLOCATION_LOGIC_TX=0
export VMA_RING_ALLOCATION_LOGIC_RX=0
#Ring allocation logic is used to separate the traffic to different rings.
#By default all sockets use the same ring for both RX and TX over the same
#interface.
#Even when specifing the logic to be per socket or thread, for different
#interfaces we use different rings. This is useful when tuning for a multi-
#threaded application and aiming for HW resource separation.
#Warning: This feature might hurt performance for applications which their main
#processing loop is based in select() and/or poll().
#The logic options are:
#0  - Ring per interface
#10 - Ring per socket (using socket fd as separator)
#20 - Ring per thread (using the id of the thread in which the socket was
#     created)
#30 - Ring per core (using cpu id)
#31 - Ring per core - attach threads : attach each thread to a cpu core
#Default value is 0

export VMA_RING_MIGRATION_RATIO_TX=-1
export VMA_RING_MIGRATION_RATIO_RX=-1
#Ring migration ratio is used with the "ring per thread" logic in order to
#decide when it is beneficial to replace the socket's ring with the ring
#allocated for the current thread.
#Each VMA_RING_MIGRATION_RATIO iterations (of accessing the ring) we check the
#current thread ID and see if our ring is matching the current thread.
#If not, we consider ring migration. If we keep accessing the ring from the same
#thread for some iterations, we migrate the socket to this thread ring.
#Use a value of -1 in order to disable migration.
#Default value is 100
#We set it to (-1) because Ring-Per-Thread is not used, anyway...

export VMA_RING_LIMIT_PER_INTERFACE=0
#Limit the number of rings that can be allocated per interface.
#For example, in ring allocation per socket logic, if the number of sockets
#using the same interface is larger than the limit, then several sockets will be
#sharing the same ring.
#[Note:VMA_RX_BUFS might need to be adjusted in order to have enough buffers for
#all rings in the system. Each ring consume VMA_RX_WRE buffers.]
#Use a value of 0 for unlimited number of rings.
#Default value is 0 (no limit)

#VMA_RX_BUFS
#Number Rx data buffer elements allocation for the processes. These data buffers
#will be used by all QPs on all HCAs as determined by the VMA_QP_LOGIC.
#Default value is 200000

export VMA_RX_WRE=256
#Number of Work Request Elements allocated in all receive QPs.
#The number of QP's can change according to the VMA_QP_LOGIC.
#Default value is 16000

export VMA_RX_WRE_BATCHING=64
#Number of Work Request Elements and RX buffers to batch before recycling.
#Batching decrease latency mean, but might increase latency STD.
#Value range is 1-1024.
#Default value is 64
#The recommended Low-Latency value was 4, but we found that 64 is much better

export VMA_RX_BYTES_MIN=24000000
#Minimum value in bytes that will be used per socket by VMA when applications
#call to setsockopt(SO_RCVBUF). If application tries to set a smaller value then
#configured in VMA_RX_BYTES_MIN, VMA will force this minimum limit value on the
#socket.VMA offloaded socket's receive max limit of ready bytes count. If the
#application does not drain a sockets and the byte limit is reached, new
#received datagrams will be dropped.
#Monitor of the applications socket's usage of current, max and dropped bytes
#and packet counters can be done with vma_stats.
#Default value is 2M, we use 24M

export VMA_RX_POLL=0
#The number of times to poll on Rx path for ready packets before going to sleep
#(wait for interrupt in blocked mode) or return -1 (in non-blocked mode).
#This Rx polling is done when the application is working with direct blocked
#calls to read(), recv(), recvfrom() & recvmsg().
#When Rx path has successfull poll hits (see performace monitoring) the latency
#is improved dramatically. This comes on account of CPU utilization.
#Value range is -1, 0 to 100,000,000
#Where value of -1 is used for infinite polling
#Default value is 100000
#XXX: We are NOT using blocked sockets, so the value is irrelevant...

export VMA_RX_POLL_INIT=0
#VMA maps all UDP sockets as potential offloaded capable. Only after the
#ADD_MEMBERSHIP does the offload start to work and the CQ polling kicks in VMA.
#This parameter control the polling count during this transition phase where the
#socket is a UDP unicast socket and no multicast addresses where added to it.
#Once the first ADD_MEMBERSHIP is called the above VMA_RX_POLL takes effect.
#Value range is similar to the above VMA_RX_POLL
#Default value is 0

export VMA_RX_UDP_POLL_OS_RATIO=1000
#The above param will define the ratio between VMA CQ poll and OS FD poll.
#This will result in a single poll of the not-offloaded sockets every
#VMA_RX_UDP_POLL_OS_RATIO offloaded socket (CQ) polls. No matter if the CQ poll
#was a hit or miss. No matter if the socket is blocking or non-blocking.
#When disabled, only offloaded sockets are polled.
#This parameter replaces the two old parameters: VMA_RX_POLL_OS_RATIO and
#VMA_RX_SKIP_OS
#Disable with 0
#Default value is 100
#XXX: Setting this param to 0 can be dangerous: Pending UDP multicast subscrs
#(managed by the OS) can be starved!

export VMA_RX_UDP_HW_TS_CONVERSION=3
#The above param will define the udp hardware receive time stamp conversion
#method. Experimental verbs is required for converting the time stamp from
#hardware time (Hz) to system time (seconds.nano_seconds). Hence, hardware
#support is not guaranteed.
#The value of VMA_RX_UDP_HW_TS_CONVERSION determined by all devices - i.e
#if the hardware of one device does not support the conversion, then it will
#be canceled for the other devices.
#Disable with 0
#Raw HW time with 1            - only convert the time stamp to
#                                seconds.nano_seconds time units (or disable if
#                                hardware does not support).
#Use best sync possible with 2 - Sync to system time, then Raw hardware time -
#                                disable if none of them are supported by
#                                hardware.
#Sync to system time with 3    - convert the time stamp to seconds.nano_seconds
#                                time units comparable to UDP receive software
#                                timestamp.
#                                disable if hardware does not support.
#Default value 3

export VMA_RX_POLL_YIELD=0
#When an application is running with multiple threads, on a limited number of
#cores, there is a need for each thread polling inside the VMA (read, readv,
#recv & recvfrom) to yield the CPU to other polling thread so not to starve
#them from processing incoming packets.
#Default value is 0 (Disable)

export VMA_RX_PREFETCH_BYTES=256
#Size of receive buffer to prefetch into cache while processing ingress packets.
#The default is a single cache line of 64 bytes which should be at least 32
#bytes to cover the IPoIB+IP+UDP headers and a small part of the users payload.
#Increasing this can help improve performance for larger user payload sizes.
#Value range is 32 bytes to MTU size
#Default value is 256 bytes

export VMA_RX_PREFETCH_BYTES_BEFORE_POLL=256
#Same as the above VMA_RX_PREFETCH_BYTES, only that prefetch is done before
#acutally getting the packets. This benefit low pps traffic latency.
#Disable with 0.
#Default value is 0

export VMA_RX_CQ_DRAIN_RATE_NSEC=0
#Socket's receive path CQ drain logic rate control.
#When disabled (Default) the socket's receive path will first try to return a
#ready packet from the socket's receive ready packet queue. Only if that queue
#is empty will the socket check the CQ for ready completions for processing.
#When enabled, even if the socket's receive ready packet queue is not empty it
#will still check the CQ for ready completions for processing. This CQ polling
#rate is controls in nano-second resolution to prevent CPU consumption because
#of over CQ polling. This will enable a more 'real time' monitoring of the
#sockets ready packet queue.
#Recommended value is 100-5000 (nsec)
#Default value is 0 (Disable)

export VMA_GRO_STREAMS_MAX=0
#Control the number of TCP streams to perform GRO (generic receive offload)
#simultaneously.
#Disable GRO with a value of 0.
#Default value is 32
#XXX: Why is GRO recommended to be disabled in Low-Latency setup?

#VMA_TCP_3T_RULES
#Use only 3 tuple rules for TCP, instead of using 5 tuple rules.
#This can improve performance for a server with listen socket which accept many
#connections.

#VMA_ETH_MC_L2_ONLY_RULES
#Use only L2 rules for Ethernet Multicast.
#All loopback traffic will be handled by VMA instead of OS.

export VMA_SELECT_POLL=100000
#The duration in micro-seconds (usec) in which to poll the hardware on Rx path
#before going to sleep (pending an interrupt) blocking on OS select(), poll() or
#epoll_wait().
#The max polling duration will be limited by the timeout the user is using when
#calling select(), poll() or epoll_wait().
#When select(), poll() or epoll_wait() path has successfull receive poll hits
#(see performace monitoring) the latency is improved dramatically. This comes
#on account of CPU utilization.
#Value range is -1, 0 to 100,000,000
#Where value of -1 is used for infinite polling
#Where value of 0 is used for no polling (interrupt driven)
#Default value is 100000
#XXX: Do NOT set an infinite period (-1) here: it results is starvation of some
#feeds, eg multicast subscrs... We currently use the default (100 msec) here...

export VMA_SELECT_POLL_OS_FORCE=0
#This flag forces to poll the OS file descriptors while user thread calls
#select(), poll() or epoll_wait() even when no offloaded sockets are mapped.
#Enabling this flag causes VMA to set VMA_SELECT_POLL_OS_RATIO and
#VMA_SELECT_SKIP_OS to 1. This will result in VMA_SELECT_POLL number of
#times VMA will poll the OS file descriptors, along side with offloaded
#sockets, if such sockets exists.
#Note that setting VMA_SELECT_SKIP_OS and VMA_SELECT_POLL_OS_RATIO
#directly will override the values these parameters gets while
#VMA_SELECT_POLL_OS_FORCE is enabled.
#Enable with 1
#Disable with 0
#Default value is 0
#XXX: Is 0 OK here? We still set VMA_SELECT_SKIP_OS and VMA_SELECT_POLL_OS_RATIO
#directly...

export VMA_SELECT_POLL_OS_RATIO=100
#This will enable polling of the OS file descriptors while user thread calls
#select(), poll() or epoll_wait() and the VMA is busy in the offloaded sockets
#polling loop. This will result in a signle poll of the not-offloaded sockets
#every VMA_SELECT_POLL_RATIO offlaoded sockets (CQ) polls.
#When disabled, only offlaoded sockets are polled.
#(See VMA_SELECT_POLL for more info)
#Disable with 0
#Default value is 10
#XXX: Setting this value to 0 is dangerous, because some activities (eg UDP
#multicast subsrcs) may not be offloaded, so still need to check the OS...

export VMA_SELECT_SKIP_OS=0
#Similar to VMA_RX_SKIP_OS, but in select(), poll() or epoll_wait() this will
#force the VMA to check the non offloaded fd even though an offloaded socket
#has ready packets found while polling.
#Default value is 4
#XXX: Is 0 OK here?

export VMA_PROGRESS_ENGINE_INTERVAL=0
#VMA Internal thread safe check that the CQ is drained at least onse
#every N milliseconds.
#This mechanism allows VMA to progress the TCP stack even when the application
#doesn't access its socket (so it doesn't provide a context to VMA).
#If CQ was already drained by the application receive socket API calls then this
#thread goes back to sleep without any processing.
#Disable with 0
#Default value is 10 msec
#XXX: Is 0 OK here (recommended for Low-Latency operations)

#VMA_PROGRESS_ENGINE_WCE_MAX
#Each time VMA's internal thread starts it's CQ draining, it will stop when
#reach this max value.
#The application is not limited by this value in the number of CQ elements
#it can ProcessId form calling any of the receive path socket APIs.
#Default value is 2048

export VMA_CQ_MODERATION_ENABLE=0
#Enable CQ interrupt moderation.
#Default value is 1 (Enabled)
#Why is the recommended Low-Latency value 0? -- Because we do NOT want to
#accumulate incoming pckgs...

#VMA_CQ_MODERATION_COUNT
#Number of packets to hold before generating interrupt.
#Default value is 48
#Irrelevant is moderation is disabled anyway

#VMA_CQ_MODERATION_PERIOD_USEC
#Period in micro-seconds for holding the packet before generating interrupt.
#Default value is 50
#Irrelevant is moderation is disabled anyway

export VMA_CQ_AIM_MAX_COUNT=128
#Maximum count value to use in the adaptive interrupt moderation algorithm.
#Default value is 560
#128 is for Low-Latency operations

#VMA_CQ_AIM_MAX_PERIOD_USEC
#Maximum period value to use in the adaptive interrupt moderation algorithm.
#Default value is 250

export VMA_CQ_AIM_INTERVAL_MSEC=0
#Frequency of interrupt moderation adaptation.
#Intervall in milli-seconds between adaptation attempts.
#Use value of 0 to disable adaptive interrupt moderation.
#Default value is 250
#Disabled for Low-Latency operations

#VMA_CQ_AIM_INTERRUPTS_RATE_PER_SEC
#Desired interrupts rate per second for each ring (CQ).
#The count and period parameters for CQ moderation will change automatically
#to achieve the desired interrupt rate for the current traffic rate.
#Default value is 5000
#(Disabled anyway)

#VMA_CQ_POLL_BATCH_MAX
#Max size of the array while polling the CQs in the VMA
#Default value is 8

export VMA_CQ_KEEP_QP_FULL=0
#If disabled, CQ will not try to compensate for each poll on the receive path.
#It will use a "depth" to remember how many WRE miss from each QP to fill it
#when buffers become avilable.
#If enabled, CQ will try to compensate QP for each polled receive completion. If
#buffers are short it will re-post a recently completed buffer. This causes a
#packet drop and will be monitored in the vma_stats.
#Default value is 1 (Enabled)
#Disabled for Low-Latency operations.

#VMA_QP_COMPENSATION_LEVEL
#Number of spare receive buffer CQ holds to allow for filling up QP while full
#receive buffers are being processes inside VMA.
#Default value is 256 buffers

export VMA_OFFLOADED_SOCKETS=1
#Create all sockets as offloaded/not-offloaded by default.
#Value of 1 is for offloaded, 0 for not-offloaded.
#Default value is 1 (Enabled)

#VMA_TIMER_RESOLUTION_MSEC
#Control VMA internal thread wakeup timer resolution (in milli seconds)
#Default value is 10 (milli-sec)

#VMA_TCP_TIMER_RESOLUTION_MSEC
#Control VMA internal TCP timer resolution (fast timer) (in milli seconds).
#Minimum value is the internal thread wakeup timer resolution
#(VMA_TIMER_RESOLUTION_MSEC).
#Default value is 100 (milli-sec)

export VMA_TCP_CTL_THREAD=0
#Do all tcp control flows in the internal thread.
#This feature should be kept disabled if using blocking poll/select (epoll is
#OK).
#Use value of 0 to disable.
#Use value of 1 for waking up the thread when there is work to do.
#Use value of 2 for waiting for thread timer to expire.
#Default value is disabled
#XXX: Synchronising with an internal thread would probably do more harm than
#good, hence disabled

export VMA_TCP_TIMESTAMP_OPTION=0
#Currently, LWIP is not supporting RTTM and PAWS mechanisms.
#See RFC1323 for info.
#Use value of 0 to disable.
#Use value of 1 for enable.
#Use value of 2 for OS follow up.
#Disabled by default (enabling causing a slight performance
#degradation of ~50-100 nano sec per half round trip)

export VMA_RX_SW_CSUM=1
#This parameter enables/disables software checksum validation for ingress
#TCP/UDP IP packets.
#Most Mellanox HCAs support hardware offload checksum validation. If the
#hardware does not support checksum validation offload, software checksum
#validation is required.
#When this parameter is enabled, software checksum validation is calculated only
#if hardware offload checksum validation is not performed.
#Performance degradation might occur if hardware offload fails to validate
#checksum and software calculation is used.
#Note that disabling software calculation might cause corrupt packets to be
#processed by VMA and the application, when the hardware does not perform this
#action.
#For further details on which adapter card supports hardware offload checksum
#validation, please refer to the VMA Release Notes.
#Valid Values are:
#Use value of 0 to disable.
#Use value of 1 for enable.
#Default value is Enabled.

export VMA_EXCEPTION_HANDLING=1
#Mode for handling missing support or error cases in Socket API or functionality
#by VMA.
#Useful for quickly identifying VMA unsupported Socket API or features
#Use value of -1 for just handling at DEBUG severity.
#Use value of 0 to log DEBUG message and try recovering via Kernel network stack
#(un-offloading the socket).
#Use value of 1 to log ERROR message and try recovering via Kernel network stack
#(un-offloading the socket).
#Use value of 2 to log ERROR message and return API respectful error code.
#Use value of 3 to log ERROR message and abort application (throw vma_error
#exception).
#Default value is -1 (notice, that in the future the default value will be
#changed to 0)

export VMA_AVOID_SYS_CALLS_ON_TCP_FD=1
#For TCP fd, avoid system calls for the supported options of:
#ioctl, fcntl, getsockopt, setsockopt.
#Non-supported options will go to OS.
#Default value is disabled
#We enable it for Low-Latency operations

export VMA_INTERNAL_THREAD_AFFINITY="$InternalThreadAffinity"
#Control which CPU core(s) the VMA internal thread is serviced on. The cpu set
#should be provided as *EITHER* a hexidecmal value that represents a bitmask.
#*OR* as a
#comma delimited of values (ranges are ok). Both the bitmask and comma delimited
#list methods are identical to what is supported by the taskset command.
#See the man page on taskset for additional information.
#Where value of -1 disables internal thread affinity setting by VMA
#Bitmask Examples:
#0x00000001 - Run on processor 0.
#0x00000007 - Run on processors 1,2, and 3.
#Comma Delimited Examples:
#0,4,8      - Run on processors 0,4, and 8.
#0,1,7-10   - Run on processors 0,1,7,8,9 and 10.
#Default value is -1 (Disabled).

#VMA_INTERNAL_THREAD_CPUSET
#Select a cpuset for VMA internal thread (see man page of cpuset).
#The value is the path to the cpuset (for example: /dev/cpuset/my_set), or an
#empty string to run it on the same cpuset the process runs on.
#Default value is an empty string.

export VMA_INTERNAL_THREAD_TCP_TIMER_HANDLING=0
#Select the internal thread policy when handling TCP timers
#Use value of 0 for deferred handling. The internal thread will NOT handle TCP
#timers upon timer expiration (once every 100ms) in order to let application
#threads handling it first
#Use value of 1 for immediate handling. The internal thread will try locking and
#handling TCP timers upon timer expiration (once every 100ms).  Application
#threads may be blocked till internal thread finishes handling TCP timers
#Default value is 0 (deferred handling)

export VMA_INTERNAL_THREAD_ARM_CQ=0
#Wakeup the internal thread for each packet that the CQ recieve.
#Poll and process the packet and bring it to the socket layer.
#This can minimize latency in case of a busy application which is not available
#to recieve the packet when it arrived.
#However, this might decrease performance in case of high pps rate application.
#Default value is 0 (Disabled)

export VMA_WAIT_AFTER_JOIN_MSEC=0
#This parameter indicates the time of delay the first packet send after
#receiving the multicast JOINED event from the SM
#This is helpful to over come loss of first few packets of an outgoing stream
#due to SM lengthy handling of MFT configuration on the switch chips
#Default value is 0 (milli-sec)
#NB: We are not sending data on the multicast interfaces...

export VMA_THREAD_MODE="$MultiThreadedApp"
#By default VMA is ready for multi-threaded applications, meaning it is thread
#safe.
#If the users application is a single threaded one, then using this
#configuration parameter you can help eliminate VMA locks and get even better
#performance.
#Single threaded application value is 0
#Multi threaded application using spin lock value is 1
#Multi threaded application using mutex lock value is 2
#Multi threaded application with more threads than cores using spin lock value
#is 3
#Default value is 1 (Multi with spin lock)
#NB: Our applications are normally Single-Threaded; at least, only 1 thread will
#be using LibvMA.

export VMA_BUFFER_BATCHING_MODE=1
#Batching of returning Rx buffers and pulling Tx buffers per socket.
#In case the value is 0 then VMA will not use buffer batching.
#In case the value is 1 then VMA will use buffer batching and will try to
#periodically reclaim unused buffers.
#In case the value is 2 then VMA will use buffer batching with no reclaim.
#[future: other values are reserved]
#Default value is 1
#XXX: Using the default here, but its latency impact is uncertain...

export VMA_MEM_ALLOC_TYPE=2
#This replaces the VMA_HUGETBL parameter logic.
#VMA will try to allocate data buffers as configured:
#	0 - "ANON" - using malloc
#	1 - "CONTIG" - using contiguous pages
#	2 - "HUGEPAGES" - using huge pages.
#OFED will also try to allocate QP & CQ memory accordingly:
#	0 - "ANON" - default - use current pages ANON small ones.
#	"HUGE" - force huge pages
#	"CONTIG" - force contig pages
#	1 - "PREFER_CONTIG" - try contig fallback to ANON small pages.
#	"PREFER_HUGE" - try huge fallback to ANON small pages.
#	2 - "ALL" - try huge fallback to contig if failed fallback to ANON small
#pages.
#To overrive OFED use: (MLX_QP_ALLOC_TYPE, MLX_CQ_ALLOC_TYPE)
#Default value is 1 (Contiguous pages)

#NB: The following VMA neigh parameters are for advanced users or Mellanox
#support only:
#
#VMA_NEIGH_UC_ARP_QUATA
#VMA will send UC ARP in case neigh state is NUD_STALE.
#In case that neigh state is still NUD_STALE VMA will try
#VMA_NEIGH_UC_ARP_QUATA retries to send UC ARP again and then will send BC ARP.
#
#VMA_NEIGH_UC_ARP_DELAY_MSEC
#This parameter indicates number of msec to wait betwen every UC ARP.
#
#VMA_NEIGH_NUM_ERR_RETRIES
#This number inidcates number of retries to restart neigh state machine in case
#neigh got ERROR event.
#Deafult value is 1

export VMA_BF=1
#This flag enables / disables BF (Blue Flame) usage of the ConnectX
#Deafult value is 1 (Enabled)

export VMA_FORK=0
#Control whether VMA should support fork. Setting this flag on will cause VMA to
#call ibv_fork_init() function. ibv_fork_init() initializes libibverbs's data
#structures to handle fork() function calls correctly and avoid data corruption.
#If ibv_fork_init() is not called or returns a non-zero status, then libibverbs
#data structures are not fork()-safe and the effect of an application calling
#fork() is undefined.
#ibv_fork_init() works on Linux kernels 2.6.17 and higher which support the
#MADV_DONTFORK flag for madvise().
#Note that VMA's default with huge pages enabled (VMA_HUGETBL) you should use an
#OFED stack version that support fork()ing of with huge pages (OFED 1.5 and
#higher).
#Default value is 0 (Disabled)

export VMA_CLOSE_ON_DUP2=1
#When this parameter is enabled, VMA will handle the dupped fd (oldfd),
#as if it was closed (clear internal data structures) and only then,
#will forward the call to the OS.
#This is, in practice, a very rudimentary dup2 support.
#It only supports the case, where dup2 is used to close file descriptors,
#Default value is 1 (Enabled)

export VMA_MTU=0
#Size of each Rx and Tx data buffer (Maximum Transfer Unit).
#This value sets the fragmentation size of the packets sent by the VMA library.
#If VMA_MTU is 0 then for each inteterface VMA will follow the actual MTU.
#If VMA_MTU is greater than 0 then this MTU value is applicable to all
#interfaces regardless of their actual MTU
#Default value is 0 (following interface actual MTU)

export VMA_MSS=0
#VMA_MSS define the max TCP payload size that can sent without IP fragmentation.
#Value of 0 will set VMA's TCP MSS to be aligned with VMA_MTU configuration
#(leaving 40 bytes room for IP + TCP headers; "TCP MSS = VMA_MTU - 40").
#Other VMA_MSS values will force VMA's TCP MSS to that specific value.
#Default value is 0 (following VMA_MTU)

export VMA_TCP_CC_ALGO=1
#TCP congestion control algorithm.
#The default algorithm coming with LWIP is a variation of Reno/New-Reno.
#The new Cubic algorithm was adapted from FreeBsd implementation.
#Use value of 0 for LWIP algorithm.
#Use value of 1 for the Cubic algorithm.
#Default value is 0 (LWIP).

export VMA_TCP_MAX_SYN_RATE=0
#Limit the number of TCP SYN packets that VMA will handle
#per second per listen socket.
#For example, in case you use 10 for this value than VMA will accept at most 10
#(could be less) new connections per second per listen socket.
#Use a value of 0 for un-limiting the number of TCP SYN packets that can be
#handled.
#Value range is 0 to 100000.
#Default value is 0 (no limit)

export VMA_IPERF=0
#Support iperf server default test which is multithreaded.
#In you run the iperf server with the additional flag -U and you feel that the
#VMA can do better, you can disable this function (VMA_IPERF=0)
#Default value is 1 (Enabled)

#-----------------------------------------------------------------------------#
# Pre-load libvma.so and run the Application:																	#
#-----------------------------------------------------------------------------#
LIBVMA=/opt/lib/libvma.so

if [ $Debug -eq 0 ]
then
	# Generic case:
	export LD_PRELOAD=$LIBVMA
  exec $ProgName $@
else
	# Run under GDB:
  exec gdb $ProgName \
		-ex "set env LD_PRELOAD=$LIBVMA" \
		-ex "set args $@"
fi
