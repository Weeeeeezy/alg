# vim:ts=2:et

SET(VENUE_SRCS
  Binance/SecDefs.cpp
  Binance/Configs_H2WS.cpp
  Binance/SecDefs_Spt.cpp
  Binance/SecDefs_FutT.cpp
  Binance/SecDefs_FutC.cpp

  BitMEX/SecDefs.cpp
  BitMEX/Configs_H2WS.cpp

  OKEX/SecDefs.cpp
  OKEX/Configs_WS.cpp

  BitFinex/SecDefs.cpp
  BitFinex/Configs_WS.cpp

  Huobi/Configs_H2WS.cpp
  Huobi/SecDefs.cpp
  Huobi/SecDefs_Spt.cpp
  Huobi/SecDefs_Fut.cpp
  Huobi/SecDefs_CSwp.cpp
  Huobi/SecDefs_USwp.cpp

  KrakenSpot/SecDefs.cpp
  KrakenSpot/Configs_WS.cpp

  FTX/SecDefs.cpp
  FTX/Configs_WS.cpp

  TDAmeritrade/SecDefs.cpp
  TDAmeritrade/Configs_H2WS.cpp
)

# Special Compilation Options: Compile all:
IF (NOT CRYPTO_ONLY)
  LIST(APPEND VENUE_SRCS
    AlfaECN/SecDefs.cpp
    AlfaECN/Configs_FIX.cpp
    AlfaECN/Accounts_FIX.cpp

    AlfaFIX2/SecDefs.cpp
    AlfaFIX2/Configs_FIX.cpp
    AlfaFIX2/Accounts_FIX.cpp

    EBS/SecDefs.cpp
    EBS/Configs_FIX.cpp
    EBS/Accounts_FIX.cpp

    FastMatch/SecDefs.cpp

    FXBA/SecDefs.cpp
    FXBA/Configs_FIX.cpp
    FXBA/Accounts_FIX.cpp

    FORTS/SecDefs.cpp
    FORTS/SecDefs_Fut.cpp
    FORTS/SecDefs_Opt.cpp
    FORTS/SecDefs_Ind.cpp
    FORTS/Configs_FAST.cpp
    FORTS/Configs_FIX.cpp
    FORTS/Accounts_FIX.cpp
    FORTS/Configs_TWIME.cpp
    FORTS/Accounts_TWIME.cpp

    HotSpotFX/SecDefs.cpp
    HotSpotFX/Configs_ITCH.cpp
    HotSpotFX/Configs_FIX.cpp
    HotSpotFX/Accounts_FIX.cpp

    Cumberland/SecDefs.cpp
    Cumberland/Configs_FIX.cpp
    Cumberland/Accounts_FIX.cpp

    Currenex/SecDefs.cpp
    Currenex/Configs_FIX.cpp
    Currenex/Accounts_FIX.cpp

    LMAX/SecDefs.cpp
    LMAX/Configs_FIX.cpp
    LMAX/Accounts_FIX.cpp

    TT/SecDefs.cpp
    TT/Configs_FIX.cpp
    TT/Accounts_FIX.cpp

    MICEX/SecDefs.cpp
    MICEX/SecDefs_FX.cpp
    MICEX/SecDefs_EQ.cpp
    MICEX/Configs_FAST.cpp
    MICEX/Configs_FIX.cpp
    MICEX/Accounts_FIX.cpp

    NTProgress/SecDefs.cpp
    NTProgress/Configs_FIX.cpp
    NTProgress/Accounts_FIX.cpp

    # XXX: LATOKEN is incomplete, so also configured in !CRYPTO_ONLY mode only:
    LATOKEN/SecDefs.cpp
    LATOKEN/Configs_WS.cpp
  )
ENDIF()

ADD_LIBRARY(MQTVenues ${VENUE_SRCS})
