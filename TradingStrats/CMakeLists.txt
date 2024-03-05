# NB: Implicitly, each STRAT has its main cpp file as:
#     TradingStrats/STRAT/STRAT.cpp:
#
SET(MQT_STRATS
  3Arb/TriArb2S
  3Arb/TriArbC0
  MM/MM-NTProgress
  MM/MM-Single
  MM/MM-Single-Net
  MM/MM-Single-ML
  BLS/BLS_serial
# BLS/BLS-Helix-Selector
  Misc/OpeningGap
  Misc/Pairs-MF
)
IF (WITH_RAPIDJSON)
  LIST(APPEND MQT_STRATS BLS/BLS-Multi-Instr)
ENDIF()

FOREACH(StratDir ${MQT_STRATS})
  GET_FILENAME_COMPONENT( STRAT  ${StratDir} NAME_WE)
  ADD_EXECUTABLE(       ${STRAT} ${StratDir}/${STRAT}.cpp)
  SET_TARGET_PROPERTIES(${STRAT} PROPERTIES OUTPUT_NAME ${STRAT})
  TARGET_LINK_LIBRARIES(${STRAT} MAQUETTE)

  # Build the WebServer controlling that Strategy (if required) and link it
  # with Wt libraries (if exist!):
  IF (WITH_WT)
    SET(WTStrat    "WT-${STRAT}")
    SET(WTStratSrc "${StratDir}/${WTStrat}.cpp")

    # NB: Need a full path here, or "WTStratSrc" would not be found!
    IF (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${WTStratSrc}")
      ADD_EXECUTABLE(       ${WTStrat} ${WTStratSrc})
      SET_TARGET_PROPERTIES(${WTStrat} PROPERTIES OUTPUT_NAME ${WTStrat})
      TARGET_LINK_LIBRARIES(${WTStrat} MAQUETTE)
    ENDIF()
  ENDIF()
ENDFOREACH(StratDir)

TARGET_LINK_LIBRARIES(BLS_serial ${BLS_STRAT_LIB})

IF (WITH_RAPIDJSON)
  TARGET_LINK_LIBRARIES(BLS-Multi-Instr ${BLS_STRAT_LIB})
ENDIF()

#TARGET_LINK_LIBRARIES(BLS-Helix-Selector
#  ${BLS_STRAT_LIB} ${BLS_SELECTOR_LIB}
