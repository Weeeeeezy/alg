# vim:ts=2:et

SET(INFRASTRUCT_SRCS
  PersistMgr.cpp
  RiskMgrTypes.cpp
  RiskMgr.cpp
  RiskMgr-DB.cpp
  SecDefsMgr.cpp
)

# "StratMonitor" requires "Wt":
IF (WITH_WT)
  LIST(APPEND INFRASTRUCT_SRCS StratMonitor.cpp)
ENDIF()

ADD_LIBRARY(MQTInfraStruct ${INFRASTRUCT_SRCS})
