# vim:ts=2:et

SET(CONNECTOR_SRCS
  EConnector.cpp
  EConnector_MktData.cpp
  EConnector_OrdMgmt.cpp
  OrderBook.cpp

  H2WS/Binance/EConnector_H2WS_Binance_MDC.cpp
  H2WS/Binance/EConnector_H2WS_Binance_OMC.cpp

  H2WS/BitMEX/EConnector_WS_BitMEX_MDC.cpp
  H2WS/BitMEX/EConnector_H2WS_BitMEX_OMC.cpp
  H2WS/BitMEX/Sender.cpp

  H2WS/BitFinex/EConnector_WS_BitFinex_MDC.cpp
  H2WS/BitFinex/EConnector_WS_BitFinex_OMC.cpp
  H2WS/BitFinex/Sender.cpp

  H2WS/Huobi/EConnector_WS_Huobi_MDC.cpp
  H2WS/Huobi/EConnector_H2WS_Huobi_OMC.cpp

  H2WS/KrakenSpot/EConnector_WS_KrakenSpot_MDC.cpp

  H2WS/FTX/EConnector_WS_FTX_MDC.cpp
# TODO: OKEX
)

# Non-Crypto Connectors:
IF (NOT CRYPTO_ONLY)
  LIST(APPEND CONNECTOR_SRCS
    FAST/EConnector_FAST_MICEX.cpp
    FAST/EConnector_FAST_FORTS.cpp

    FIX/EConnector_FIX.cpp
    ITCH/EConnector_ITCH_HotSpotFX.cpp

    TWIME/EConnector_TWIME_FORTS.cpp
    TWIME/Sender.cpp
  )
  # NB: P2CGate MDC is only built if requested:
  IF (WITH_P2CGATE)
    LIST(APPEND CONNECTOR_SRCS
      P2CGate/EConnector_P2CGate_FORTS_MDC.cpp)
  ENDIF()

  # The following connectors require RapidJSON:
  IF (WITH_RAPIDJSON)
    LIST(APPEND CONNECTOR_SRCS
      H2WS/LATOKEN/EConnector_WS_LATOKEN_MDC.cpp
      H2WS/TDAmeritrade/EConnector_H1WS_TDAmeritrade.cpp
      H2WS/TDAmeritrade/Sender_OMC.cpp)

    # LATOKEN STP Connector also requires LibRDKafka:
    IF (WITH_RDKAFKA)
      LIST(APPEND CONNECTOR_SRCS
           Kafka/EConnector_LATOKEN_STP-Init.cpp
           Kafka/EConnector_LATOKEN_STP-Oper.cpp
           Kafka/EConnector_LATOKEN_STP-DB.cpp)
    ENDIF()
  ENDIF()
ENDIF()

ADD_LIBRARY(MQTConnectors ${CONNECTOR_SRCS})
