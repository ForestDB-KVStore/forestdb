cmake_minimum_required(VERSION 3.4.0)
project (ForestDB)

IF (${CMAKE_MAJOR_VERSION} GREATER 2)
    CMAKE_POLICY(SET CMP0042 NEW)
ENDIF (${CMAKE_MAJOR_VERSION} GREATER 2)

INCLUDE(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
IF (COMPILER_SUPPORTS_CXX11)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++11")
    SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -std=c++11")
ELSEIF(COMPILER_SUPPORTS_CXX0X)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
    SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++0x")
    SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -std=c++0x")
ELSE()
    IF (NOT WIN32)
        MESSAGE(FATAL_ERROR "Compiler ${CMAKE_CXX_COMPILER} has no C++11 support.")
    ENDIF()
ENDIF()

INCLUDE(CheckFunctionExists)
INCLUDE(CheckIncludeFileCXX)
INCLUDE(CheckIncludeFiles)
INCLUDE(CheckLibraryExists)
INCLUDE(CheckSymbolExists)
INCLUDE(CheckTypeSize)
INCLUDE(CTest)

IF (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git)
    EXECUTE_PROCESS(COMMAND git describe --abbrev=7 --always --tags
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    OUTPUT_VARIABLE FORESTDB_VERSION
                    ERROR_QUIET
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
ELSE (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git)
    SET(FORESTDB_VERSION "unknown")
ENDIF (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git)

include_directories(BEFORE ${PROJECT_SOURCE_DIR}/option)
include_directories(BEFORE ${PROJECT_SOURCE_DIR}/utils)
include_directories(BEFORE ${PROJECT_SOURCE_DIR}/src)
include_directories(BEFORE ${PROJECT_SOURCE_DIR}/include)
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR}/src)

CHECK_INCLUDE_FILES("sched.h" HAVE_SCHED_H)

CONFIGURE_FILE (${CMAKE_CURRENT_SOURCE_DIR}/src/config.cmake.h
                ${CMAKE_CURRENT_BINARY_DIR}/src/config.h)

LIST(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/Modules")

if (NOT SNAPPY_OPTION)
    # Disable snappy by default if not given.
    set(SNAPPY_OPTION "Disable")
endif ()


if (COUCHBASE_SERVER_BUILD)
    ADD_DEFINITIONS(-D_DOC_COMP=1)
    include_directories(AFTER ${SNAPPY_INCLUDE_DIR})

    if (WIN32)
       # The rest of Couchbase server use tcmalloc as the memory
       # allocator for WIN32. Override the allocator to jemalloc
       ADD_DEFINITIONS(-UHAVE_TCMALLOC)
       SET(COUCHBASE_MEMORY_ALLOCATOR "jemalloc")
       # Make sure that we define all of the variables
       include(CouchbaseMemoryAllocator)
    endif (WIN32)

    if (MEMORY_ALLOCATOR)
        include_directories(AFTER ${MALLOC_INCLUDE_DIR})
    endif (MEMORY_ALLOCATOR)
    set(DIRUTILS_LIBRARY dirutils)
    set(GTEST_LIBRARY gtest)

else(COUCHBASE_SERVER_BUILD)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GNU_SOURCE -D__DEBUG")
    if (NOT WIN32)
        if (NOT(SNAPPY_OPTION STREQUAL "Disable"))
            INCLUDE(FindSnappy)
            ADD_DEFINITIONS(-D_DOC_COMP=1)
            MESSAGE(STATUS "Snappy compression: ENABLED")
        else ()
            MESSAGE(STATUS "Snappy compression: DISABLED")
        endif (NOT(SNAPPY_OPTION STREQUAL "Disable"))
    endif(NOT WIN32)
    if (_JEMALLOC EQUAL 1)
        INCLUDE(FindJemalloc)
    endif(_JEMALLOC EQUAL 1)
endif(COUCHBASE_SERVER_BUILD)

#INCLUDE(FindAsyncIOLib)

if (NOT BTREE_SUBBLOCK)
    # Enable by default if not given.
    set(BTREE_SUBBLOCK "Enable")
endif ()

if ( (BTREE_SUBBLOCK STREQUAL "Enable") OR
     (BTREE_SUBBLOCK STREQUAL "enable") OR
     (BTREE_SUBBLOCK STREQUAL "True") OR
     (BTREE_SUBBLOCK STREQUAL "true")  )
    ADD_DEFINITIONS(-D__BTREEBLK_SUBBLOCK=1)
    MESSAGE(STATUS "B+tree sub-blocks: ENABLED")
else ()
    MESSAGE(STATUS "B+tree sub-blocks: DISABLED")
endif()

if (NOT CMAKE_BUILD_TYPE)
     set(DEFAULT_BUILD_TYPE "RelWithDebInfo")
    #set(DEFAULT_BUILD_TYPE "Debug")

    set(BUILD_TYPE_OPTIONS
        "Choose the type of build, "
        "options are: Debug Release RelWithDebInfo MinSizeRel.")
    set(CMAKE_BUILD_TYPE ${DEFAULT_BUILD_TYPE}
        CACHE ${BUILD_TYPE_OPTIONS} FORCE)
    message(STATUS "Build type is not given, use default.")
endif ()
message(STATUS "Build type: " ${CMAKE_BUILD_TYPE})


if(NOT LIBRT)
    unset(LIBRT CACHE)
endif(NOT LIBRT)
if (NOT WIN32)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -Wunused-variable -Wall -pedantic -Wredundant-decls")
    set(PTHREAD_LIB pthread)
    set(LIBM m)
    set(FORESTDB_FILE_OPS "src/filemgr_ops_linux.cc")
    if (NOT APPLE)
        set(LIBRT rt)
    endif (NOT APPLE)
    FIND_LIBRARY(DL_LIBRARY NAMES dl)
    if (DL_LIBRARY)
        set(DL_LIBRARIES "${DL_LIBRARY}")
        message(STATUS "DL library : ${DL_LIBRARY}")
    endif (DL_LIBRARY)

    FIND_LIBRARY(DL_STATIC_LIBRARY NAMES libdl.a)
    if (DL_STATIC_LIBRARY)
        message(STATUS "DL static library : ${DL_STATIC_LIBRARY}")
    endif ()

else (NOT WIN32)
    set(FORESTDB_FILE_OPS "src/filemgr_ops_windows.cc")
    set(GETTIMEOFDAY_VS "${PROJECT_SOURCE_DIR}/utils/gettimeofday_vs.cc")
endif(NOT WIN32)

IF (_MVCC_WAL EQUAL 1)
    ADD_DEFINITIONS(-D_MVCC_WAL_ENABLE=1)
ENDIF()

IF (_MEMLEAK EQUAL 1)
    ADD_DEFINITIONS(-D_MEMLEAK_ENABLE=1)
ELSEIF (_MEMLEAK EQUAL 2)
    ADD_DEFINITIONS(-D_MEMLEAK_ENABLE=1)
    ADD_DEFINITIONS(-D_CHK_MODIFY_AFTER_FREE=1)
ENDIF()

# Comment the line below to disable compiling Copy-On-Write (COW) optimizations
ADD_DEFINITIONS (-D_COW_COMPACTION=1)

if (COUCHBASE_SERVER_BUILD)
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
       # using /W0 cause msvc to print out a warning that it overrides
       # previous settings (/W3 is added by default).
       # Let's just disable the ones we're "violating"
       # (we should probably fix them at some point?)
       set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4716 /wd4244 /wd4267 /wd4101 /wd4800 /wd4700 /wd4018 /wd4102")
   endif()
else()
  # If building outside CB server build, set required flags for the selected compiler.
    if ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -Qunused-arguments -g -fomit-frame-pointer -pthread")
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -g -fomit-frame-pointer -pthread -Wno-class-memaccess -Wno-int-in-bool-context -Wno-format-overflow")
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
       set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4716 /W0")
    endif()
endif()

# Breakpad support with Couchbase Server Build only when on Windows/Linux.
if (COUCHBASE_SERVER_BUILD)
    if (BREAKPAD_FOUND)
        INCLUDE_DIRECTORIES(AFTER ${BREAKPAD_INCLUDE_DIR})
        if (WIN32)
            SET(BREAKPAD_SRC src/breakpad_win32.cc)
        elseif ("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
            SET(BREAKPAD_SRC src/breakpad_linux.cc)
            # Breakpad has headers (which are dependencies for our breakpad
            # integration code) that don't compile cleanly as C++11, so the
            # GNU standard is used instead.
            SET_SOURCE_FILES_PROPERTIES(src/breakpad_linux.cc
                                        PROPERTIES
                                        COMPILE_FLAGS ${CB_GNU_CXX11_OPTION})
        else()
            SET(BREAKPAD_SRC src/breakpad_dummy.cc)
        endif()
    else (BREAKPAD_FOUND)
        SET(BREAKPAD_SRC src/breakpad_dummy.cc)
    endif(BREAKPAD_FOUND)
else()
    SET(BREAKPAD_SRC src/breakpad_dummy.cc)
endif()

if (NOT COUCHBASE_SERVER_BUILD)
   INCLUDE(ThreadSanitizer)
endif()

IF (_ENCRYPTION)
    INCLUDE(FindEncryptionLib)
ENDIF (_ENCRYPTION)

SET(FORESTDB_CORE_SRC
    ${PROJECT_SOURCE_DIR}/src/api_wrapper.cc
    ${PROJECT_SOURCE_DIR}/src/avltree.cc
    ${PROJECT_SOURCE_DIR}/src/bgflusher.cc
    ${PROJECT_SOURCE_DIR}/src/blockcache.cc
    ${PROJECT_SOURCE_DIR}/${BREAKPAD_SRC}
    ${PROJECT_SOURCE_DIR}/src/btree.cc
    ${PROJECT_SOURCE_DIR}/src/btree_kv.cc
    ${PROJECT_SOURCE_DIR}/src/btree_str_kv.cc
    ${PROJECT_SOURCE_DIR}/src/btree_fast_str_kv.cc
    ${PROJECT_SOURCE_DIR}/src/btreeblock.cc
    ${PROJECT_SOURCE_DIR}/src/checksum.cc
    ${PROJECT_SOURCE_DIR}/src/compactor.cc
    ${PROJECT_SOURCE_DIR}/src/configuration.cc
    ${PROJECT_SOURCE_DIR}/src/docio.cc
    ${PROJECT_SOURCE_DIR}/src/encryption.cc
    ${PROJECT_SOURCE_DIR}/src/encryption_aes.cc
    ${PROJECT_SOURCE_DIR}/src/encryption_bogus.cc
    ${PROJECT_SOURCE_DIR}/src/fdb_errors.cc
    ${PROJECT_SOURCE_DIR}/src/filemgr.cc
    ${PROJECT_SOURCE_DIR}/src/filemgr_ops.cc
    ${PROJECT_SOURCE_DIR}/src/forestdb.cc
    ${PROJECT_SOURCE_DIR}/src/hash.cc
    ${PROJECT_SOURCE_DIR}/src/hash_functions.cc
    ${PROJECT_SOURCE_DIR}/src/hbtrie.cc
    ${PROJECT_SOURCE_DIR}/src/iterator.cc
    ${PROJECT_SOURCE_DIR}/src/kv_instance.cc
    ${PROJECT_SOURCE_DIR}/src/list.cc
    ${PROJECT_SOURCE_DIR}/src/log_message.cc
    ${PROJECT_SOURCE_DIR}/src/staleblock.cc
    ${PROJECT_SOURCE_DIR}/src/superblock.cc
    ${PROJECT_SOURCE_DIR}/src/transaction.cc
    ${PROJECT_SOURCE_DIR}/src/version.cc
    ${PROJECT_SOURCE_DIR}/src/wal.cc)

SET(FORESTDB_UTILS_SRC
    ${PROJECT_SOURCE_DIR}/utils/crc32.cc
    ${PROJECT_SOURCE_DIR}/utils/debug.cc
    ${PROJECT_SOURCE_DIR}/utils/memleak.cc
    ${PROJECT_SOURCE_DIR}/utils/partiallock.cc
    ${PROJECT_SOURCE_DIR}/utils/system_resource_stats.cc
    ${PROJECT_SOURCE_DIR}/utils/time_utils.cc
    ${PROJECT_SOURCE_DIR}/utils/timing.cc)

add_library(forestdb SHARED
            ${FORESTDB_FILE_OPS}
            ${GETTIMEOFDAY_VS}
            ${FORESTDB_CORE_SRC}
            ${FORESTDB_UTILS_SRC})
target_link_libraries(forestdb ${PTHREAD_LIB} ${LIBM} ${SNAPPY_LIBRARIES}
                      ${ASYNC_IO_LIB} ${MALLOC_LIBRARIES} ${LIBRT}
                      ${CRYPTO_LIB}
                      ${DL_LIBRARIES} ${BREAKPAD_LIBRARIES})

add_library(static_lib
            ${FORESTDB_FILE_OPS}
            ${GETTIMEOFDAY_VS}
            ${FORESTDB_CORE_SRC}
            ${FORESTDB_UTILS_SRC})
target_link_libraries(static_lib
                      ${PTHREAD_LIB}
                      ${LIBM}
                      ${SNAPPY_LIBRARIES}
                      ${ASYNC_IO_LIB}
                      ${MALLOC_LIBRARIES}
                      ${LIBRT}
                      ${CRYPTO_LIB}
                      ${DL_LIBRARIES}
                      ${BREAKPAD_LIBRARIES})
set_target_properties(static_lib PROPERTIES OUTPUT_NAME forestdb CLEAN_DIRECT_OUTPUT 1)

# Create objects libraries for the different "tools" parts forestdb, which can be reused
# by various targets.
add_library(FDB_TOOLS_CORE OBJECT ${FORESTDB_CORE_SRC})
set_target_properties(FDB_TOOLS_CORE PROPERTIES COMPILE_FLAGS "-D_FDB_TOOLS")

add_library(FDB_TOOLS_UTILS OBJECT ${FORESTDB_UTILS_SRC})
set_target_properties(FDB_TOOLS_UTILS PROPERTIES COMPILE_FLAGS "-D_FDB_TOOLS")

add_executable(forestdb_dump
               tools/dump_common.cc
               tools/forestdb_dump.cc
               ${FORESTDB_FILE_OPS}
               ${GETTIMEOFDAY_VS}
               $<TARGET_OBJECTS:FDB_TOOLS_CORE>
               $<TARGET_OBJECTS:FDB_TOOLS_UTILS>)
target_link_libraries(forestdb_dump ${PTHREAD_LIB} ${LIBM} ${SNAPPY_LIBRARIES}
                      ${ASYNC_IO_LIB} ${MALLOC_LIBRARIES} ${PLATFORM_LIBRARY}
                      ${LIBRT} ${CRYPTO_LIB}
                      ${DL_LIBRARIES} ${BREAKPAD_LIBRARIES})
set_target_properties(forestdb_dump PROPERTIES COMPILE_FLAGS "-D_FDB_TOOLS")

add_executable(forestdb_hexamine
               tools/dump_common.cc
               tools/forestdb_hexamine.cc
               ${FORESTDB_FILE_OPS}
               ${GETTIMEOFDAY_VS}
               $<TARGET_OBJECTS:FDB_TOOLS_CORE>
               $<TARGET_OBJECTS:FDB_TOOLS_UTILS>)
target_link_libraries(forestdb_hexamine ${PTHREAD_LIB} ${LIBM} ${SNAPPY_LIBRARIES}
                      ${ASYNC_IO_LIB} ${MALLOC_LIBRARIES} ${PLATFORM_LIBRARY}
                      ${LIBRT} ${CRYPTO_LIB}
                      ${DL_LIBRARIES} ${BREAKPAD_LIBRARIES})
set_target_properties(forestdb_hexamine PROPERTIES COMPILE_FLAGS "-D_FDB_TOOLS")

if ((NOT COUCHBASE_SERVER_BUILD) OR (INSTALL_HEADER_FILES))
    INSTALL(FILES
            include/libforestdb/forestdb.h
            include/libforestdb/fdb_types.h
            include/libforestdb/fdb_errors.h
            DESTINATION include/libforestdb)
endif((NOT COUCHBASE_SERVER_BUILD) OR (INSTALL_HEADER_FILES))

INSTALL(TARGETS forestdb
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

INSTALL(TARGETS forestdb_dump
        RUNTIME DESTINATION bin)


if ("${CMAKE_BUILD_TYPE}" STREQUAL "Coverage")
    if ((NOT COUCHBASE_SERVER_BUILD) AND (NOT WIN32))
        INCLUDE(CodeCoverage)
        SETUP_TARGET_FOR_COVERAGE(test_coverage ctest ${PROJECT_SOURCE_DIR}/coverage)
    endif((NOT COUCHBASE_SERVER_BUILD) AND (NOT WIN32))
endif()

if (("${CMAKE_BUILD_TYPE}" STREQUAL "Valgrind") AND (NOT WIN32))
    INCLUDE(MemoryCheck)
    SETUP_TARGET_FOR_MEMCHECK(test_valgrind ${PROJECT_BINARY_DIR}/fdb_functional_test)
endif()

# add top-level testing dir
enable_testing()
add_subdirectory("${PROJECT_SOURCE_DIR}/tests")

if (COUCHBASE_SERVER_BUILD)
    ENABLE_CODE_COVERAGE_REPORT()
endif()
