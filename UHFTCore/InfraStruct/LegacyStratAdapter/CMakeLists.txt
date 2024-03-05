# vim:ts=2:et
#
# The following is for compilation of ODB code only:
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/StrategyAdaptor)

LIST(APPEND MAQUETTE_SRCS
  StrategyAdaptor/EventTypes.cpp
  StrategyAdaptor/OrderBook.cpp
# StrategyAdaptor/MSEnv.cpp
  StrategyAdaptor/StratEnv_Subscr.cpp
  StrategyAdaptor/StratEnv_Events.cpp
  StrategyAdaptor/StratEnv_Orders.cpp
  StrategyAdaptor/StratEnv_Analyt.cpp
# StrategyAdaptor/ConnEmu.cpp
  StrategyAdaptor/TradeBooksShM.cpp
  StrategyAdaptor/OrderBooksShM.cpp
  StrategyAdaptor/InitMargins.cpp
)

LIST(APPEND MAQUETTE_ODB_HDRS
  StrategyAdaptor/SecDefsShM.h
)
