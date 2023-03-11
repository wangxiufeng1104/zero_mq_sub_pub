string(REGEX REPLACE "/$" "" CURRENT_FOLDER_ABSOLUTE ${CMAKE_CURRENT_SOURCE_DIR})
string(REGEX REPLACE ".*/(.*)" "\\1" CURRENT_FOLDER ${CURRENT_FOLDER_ABSOLUTE})
set (VCAR_APP ${CURRENT_FOLDER})
aux_source_directory (${CMAKE_CURRENT_SOURCE_DIR}/src APP_SRC)
add_executable(${VCAR_APP} ${APP_SRC})
target_include_directories(${VCAR_APP} PRIVATE
                    ${PROJ_CMAKE_DIR}/share/inc/czmq)
target_link_libraries(${VCAR_APP} zmq pthread sysmsg sys_until)
