# vim:ts=2:et

IF (NOT CRYPTO_ONLY)
  SET(ProtocolSrcs
    FAST/Decoder_MICEX_Curr.cpp
    FAST/Decoder_FORTS_Curr.cpp
    TWIME/Msgs.cpp
  )
  IF (WITH_RDKAFKA)
    LIST(APPEND ProtocolSrcs
      H2WS/LATOKEN-OMC.cpp)
  ENDIF()

  ADD_LIBRARY(MQTProtocols ${ProtocolSrcs})
ENDIF()
