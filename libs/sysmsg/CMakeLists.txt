string(REGEX REPLACE "/$" "" CURRENT_FOLDER_ABSOLUTE ${CMAKE_CURRENT_SOURCE_DIR})
string(REGEX REPLACE ".*/(.*)" "\\1" CURRENT_FOLDER ${CURRENT_FOLDER_ABSOLUTE})
set (VCAR_LIB ${CURRENT_FOLDER})
aux_source_directory (${CMAKE_CURRENT_SOURCE_DIR}/src SRC)
link_directories(BEFORE ${PROJ_CMAKE_DIR}/share/lib)
add_library(${VCAR_LIB} STATIC ${SRC})
SET_TARGET_PROPERTIES(${VCAR_LIB}  PROPERTIES  OUTPUT_NAME  "sysmsg")
target_link_libraries(${VCAR_LIB} zmq czmq hiredis)

target_include_directories(${VCAR_LIB}
                            PUBLIC
                            ${CMAKE_CURRENT_SOURCE_DIR}/src
                            ${PROJ_CMAKE_DIR}/share/inc
                            ${PROJ_CMAKE_DIR}/share/inc/czmq)
target_compile_options(${VCAR_LIB} PRIVATE -Wno-unused-result)