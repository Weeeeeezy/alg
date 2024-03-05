# vim:ts=2:et
# Various Specific Tests / Executables. XXX: Currently, even crypto-tests are
# specified at (NOT CRYPTO_ONLY), to avoid building them every time:
#
IF (NOT CRYPTO_ONLY)
  LIST(APPEND TEST_SRCS
    MisAligned.cpp
    Base64Test.cpp
    SeqNumTests.cpp
    TimeStampTest.cpp
    MICEX_Test.cpp
    FORTS_Test.cpp
    Cumberland_Test.cpp
    Currenex_Test.cpp
    HotSpot_Test.cpp
    KrakenSpot_Test.cpp
    LMAX_Test.cpp
    TT_Test.cpp
    SpecialFPValsTest.cpp
  )
  IF (WITH_RAPIDJSON)
    LIST(APPEND TEST_SRCS
      TDAmeritrade_Test.cpp)
  ENDIF()
  IF (WITH_HDF5)
    LIST(APPEND TEST_SRCS
      BLS_parallel.cpp
    # BLS_selector.cpp
    # BLS_pso.cpp
    )
  ENDIF()
  IF (WITH_VMIME)
    LIST(APPEND TEST_SRCS
      TestEmail.cpp)
  ENDIF()
ENDIF()

FOREACH(TEST_SRC ${TEST_SRCS})
  GET_FILENAME_COMPONENT( TEST  ${TEST_SRC} NAME_WE)
  ADD_EXECUTABLE       (${TEST} ${TEST_SRC})
  SET_TARGET_PROPERTIES(${TEST} PROPERTIES OUTPUT_NAME ${TEST})
  TARGET_LINK_LIBRARIES(${TEST} MAQUETTE)
ENDFOREACH(TEST_SRC)

IF (WITH_HDF5)
  TARGET_LINK_LIBRARIES(BLS_parallel ${BLS_STRAT_LIB})
# TARGET_LINK_LIBRARIES(BLS_selector ${BLS_STRAT_LIB})
# TARGET_LINK_LIBRARIES(BLS_pso      ${BLS_STRAT_LIB})
ENDIF()
