add_subdirectory(core)

include_directories(
    ${CMAKE_SOURCE_DIR}/modules/charinterface
    ${CMAKE_SOURCE_DIR}/kame/graph 
    ${CMAKE_SOURCE_DIR}/modules/levelmeter/core)

########### next target ###############
set(levelmeter_SRCS
    userlevelmeter.cpp
 )

add_library(levelmeter MODULE ${levelmeter_SRCS})
target_link_libraries(levelmeter kame_levelmetercore kame_charinterface ${MODULE_LINKER_FLAGS})
########### install files ###############
install(TARGETS levelmeter LIBRARY  DESTINATION ${KAME_MODULE_INSTALL_DIR})
