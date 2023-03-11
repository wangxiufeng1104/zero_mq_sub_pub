SUBDIRLIST(SUBDIRS ${CMAKE_CURRENT_SOURCE_DIR})
foreach(child ${SUBDIRS})
    add_subdirectory(${child})
endforeach()
