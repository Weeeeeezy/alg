# vim:ts=2:et

SET(QUANT_SUPPORT_SRCS
  BT/DataProvider.cpp
  BT/MktData.cpp
  BT/OrdMgmt.cpp
  BT/SecDefs.cpp
  BT/BackTest.cpp
)

IF (NOT CRYPTO_ONLY)
  LIST(APPEND QUANT_SUPPORT_SRCS MDReader_HotSpotFX.cpp)
ENDIF()

ADD_LIBRARY(MQTQuantSupport ${QUANT_SUPPORT_SRCS})
