# vim:ts=2:et
# NB: The following settings are NOT directly usable -- install them in the resp
# mainline "CMakeLists.txt" files:
#-----------------------------------------------------------------------------#
# "Connectors/CmakeLists.txt":                                                #
#-----------------------------------------------------------------------------#
LIST(APPEND_MQT_SRCS
  Connectors/FeedOS/EConnector_FeedOS.cpp)

# Connectors/FeedOS/EConnector_FeedOS.cpp is to be compiled with relaxed
# warnings:
SET_SOURCE_FILES_PROPERTIES(
  Connectors/FeedOS/EConnector_FeedOS.cpp
  PROPERTIES COMPILE_FLAGS -Wno-deprecated)

#-----------------------------------------------------------------------------#
# "Venues/CMakeLists.txt":                                                    #
#-----------------------------------------------------------------------------#
LIST(APPEND MQT_SRCS
  Venues/FeedOS/SecDefs.cpp
  Venues/FeedOS/Configs.cpp

#-----------------------------------------------------------------------------#
# Top-Level "CMakeLists.txt":                                                 #
#-----------------------------------------------------------------------------#
SET(EXT_LIBS         ${MQT_LIBS} "feedosapi")
INCLUDE_DIRECTORIES("${PREFIX}/FeedOS/api/include")
LINK_DIRECTORIES   ("${PREFIX}/FeedOS/api/lib")
ADD_DEFINITIONS    (-isystem "${PREFIX}/FeedOS/api/include")
