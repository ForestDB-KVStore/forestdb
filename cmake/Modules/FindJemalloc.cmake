# Locate jemalloc libraries on a host OS.

if(UNIX)
    find_path(JEMALLOC_INCLUDE_DIR jemalloc/jemalloc.h
        PATH_SUFFIXES include
        PATHS
        ~/Library/Frameworks
        /Library/Frameworks
        /usr/local
        /opt/local
        /opt/csw
        /opt)

    find_library(JEMALLOC_LIBRARIES
        NAMES jemalloc
        PATHS
        ~/Library/Frameworks
        /Library/Frameworks
        /usr/local
        /opt/local
        /opt/csw
        /opt)
elseif(WIN32)
endif()

if(JEMALLOC_LIBRARIES)
    message(STATUS "Found jemalloc libraries in ${JEMALLOC_INCLUDE_DIR} :
            ${JEMALLOC_LIBRARIES}")
    add_compile_definitions(HAVE_JEMALLOC=1)
    set(MALLOC_LIBRARIES ${JEMALLOC_LIBRARIES})
    include_directories(AFTER ${JEMALLOC_INCLUDE_DIR})
    mark_as_advanced(MALLOC_INCLUDE_DIR JEMALLOC_LIBRARIES)
else()
    message(FATAL_ERROR "Can't find jemalloc libraries")
endif(JEMALLOC_LIBRARIES)
