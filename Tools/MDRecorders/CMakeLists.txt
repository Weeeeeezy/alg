add_executable(MDRecorder MDRecorder.cpp)
target_link_libraries(MDRecorder MAQUETTE)

add_executable(ConvertMDStoreToH5 ConvertMDStoreToH5.cpp)
target_link_libraries(ConvertMDStoreToH5 MAQUETTE)

add_executable(ConvertToEdFormat ConvertToEdFormat.cpp)
target_link_libraries(ConvertToEdFormat MAQUETTE)

add_executable(MDAnalyzer MDAnalyzer.cpp)
target_link_libraries(MDAnalyzer MAQUETTE)

add_executable(ConvertToTS ConvertToTS.cpp)
target_link_libraries(ConvertToTS MAQUETTE)

add_executable(ConvertTXTToMDStore ConvertTXTToMDStore.cpp)
target_link_libraries(ConvertTXTToMDStore MAQUETTE)

if (WITH_CCTZ)
add_executable(ConvertTSDataToMDStore ConvertTSDataToMDStore.cpp)
target_link_libraries(ConvertTSDataToMDStore MAQUETTE)

add_executable(ConvertTSDataToVSPDat ConvertTSDataToVSPDat.cpp)
target_link_libraries(ConvertTSDataToVSPDat MAQUETTE)
endif()

add_executable(PrintVSPBin PrintVSPBin.cpp)
target_link_libraries(PrintVSPBin MAQUETTE ${BLS_STRAT_LIB})

add_executable(PrintMDStore PrintMDStore.cpp)
target_link_libraries(PrintMDStore MAQUETTE)

add_executable(VerifyH5MDStore VerifyH5MDStore.cpp)
target_link_libraries(VerifyH5MDStore MAQUETTE)

add_executable(VerifyH5AndRemoveDat VerifyH5AndRemoveDat.cpp)
target_link_libraries(VerifyH5AndRemoveDat MAQUETTE)

add_executable(PrintMDStoreH5 PrintMDStoreH5.cpp)
target_link_libraries(PrintMDStoreH5 MAQUETTE)
