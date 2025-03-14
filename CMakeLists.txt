cmake_minimum_required(VERSION 3.20)
project(ForestDB VERSION 1.0.0 LANGUAGES CXX)

if(${CMAKE_MAJOR_VERSION} GREATER 2)
    cmake_policy(SET CMP0042 NEW)
endif(${CMAKE_MAJOR_VERSION} GREATER 2)


include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_STANDARD 11)
elseif(COMPILER_SUPPORTS_CXX0X)
    add_compile_options(-std=c++0x)
else()
    if(NOT WIN32)
        message(FATAL_ERROR "Compiler ${CMAKE_CXX_COMPILER} has no C++11 support.")
    endif()
endif()

include(CheckFunctionExists)
include(CheckIncludeFileCXX)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckTypeSize)
include(CTest) # automatically defines BUILD_TESTING option

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git)
    execute_process(COMMAND git describe --abbrev=7 --always --tags
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE FORESTDB_VERSION
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
else()
    set(FORESTDB_VERSION "unknown")
endif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git)

option(WITH_CONAN "Use dependences provide by Conan" OFF)
option(_JEMALLOC "Use dependences provide by Conan" OFF)
option(WITH_SNAPPY "Use Snappy Compression" OFF)
option(CODE_COVERAGE "Enable Coverage" OFF)
option(ADDRESS_SANITIZER "Enable address sanitizer" OFF)
option(THREAD_SANITIZER "Enable thread sanitizer" OFF)
option(LEAK_SANITIZER "Enable leak sanitizer" OFF)

include_directories(BEFORE
    ${PROJECT_SOURCE_DIR}/option
    ${PROJECT_SOURCE_DIR}/utils
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/src
)

CHECK_INCLUDE_FILES("sched.h" HAVE_SCHED_H)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/config.cmake.h
    ${CMAKE_CURRENT_BINARY_DIR}/src/config.h)

if(SNAPPY_OPTION AND NOT WIN32)
    if(WITH_CONAN)
        find_package(Snappy REQUIRED)
        set(LIBSNAPPY Snappy::snappy)
    else()
        include(cmake/Modules/FindSnappy.cmake)
        if(SNAPPY_FOUND)
            set(LIBSNAPPY ${SNAPPY_LIBRARIES})
        else()
            message(FATAL_ERROR "Can't find snappy, "
                "if you want to build without snappy set SNAPPY_OPTION=OFF")
        endif(SNAPPY_FOUND)

    endif(WITH_CONAN)
    message(STATUS "Snappy compression: ENABLED")
    add_compile_definitions(_DOC_COMP=1)
else()
    message(STATUS "Snappy compression: DISABLED")
endif()


if(_JEMALLOC)
    if(WITH_CONAN)
        find_package(jemalloc REQUIRED)
        set(MALLOC_LIBRARIES jemalloc::jemalloc)
        add_compile_definitions(HAVE_JEMALLOC=1)
    else()
        include(cmake/Modules/FindJemalloc)
    endif(WITH_CONAN)
endif(_JEMALLOC)

#include(include(FindAsyncIOLib)

if(NOT BTREE_SUBBLOCK)
    # Enable by default if not given.
    set(BTREE_SUBBLOCK "Enable")
endif()

if((BTREE_SUBBLOCK STREQUAL "Enable") OR
    (BTREE_SUBBLOCK STREQUAL "enable") OR
    (BTREE_SUBBLOCK STREQUAL "True") OR
    (BTREE_SUBBLOCK STREQUAL "true"))
    add_definitions(-D__BTREEBLK_SUBBLOCK=1)
    message(STATUS "B+tree sub-blocks: ENABLED")
else()
    message(STATUS "B+tree sub-blocks: DISABLED")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(DEFAULT_BUILD_TYPE "RelWithDebInfo")
    #set(DEFAULT_BUILD_TYPE "Debug")

    set(BUILD_TYPE_OPTIONS
        "Choose the type of build, "
        "options are: Debug Release RelWithDebInfo MinSizeRel.")
    set(CMAKE_BUILD_TYPE ${DEFAULT_BUILD_TYPE}
        CACHE ${BUILD_TYPE_OPTIONS} FORCE)
    message(STATUS "Build type is not given, use default.")
endif()
message(STATUS "Build type: " ${CMAKE_BUILD_TYPE})


if(NOT LIBRT)
    unset(LIBRT CACHE)
endif(NOT LIBRT)
if(NOT WIN32)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -Wunused-variable -Wall -pedantic -Wredundant-decls")
    set(PTHREAD_LIB pthread)
    set(LIBM m)
    set(FORESTDB_FILE_OPS "src/filemgr_ops_linux.cc")
    if(NOT APPLE)
        set(LIBRT rt)
    endif(NOT APPLE)
    find_library(DL_LIBRARY NAMES dl)
    if(DL_LIBRARY)
        set(DL_LIBRARIES "${DL_LIBRARY}")
        message(STATUS "DL library : ${DL_LIBRARY}")
    endif(DL_LIBRARY)

    find_library(DL_STATIC_LIBRARY NAMES libdl.a)
    if(DL_STATIC_LIBRARY)
        message(STATUS "DL static library : ${DL_STATIC_LIBRARY}")
    endif()

else()
    set(FORESTDB_FILE_OPS "src/filemgr_ops_windows.cc")
    set(GETTIMEOFDAY_VS "${PROJECT_SOURCE_DIR}/utils/gettimeofday_vs.cc")
endif(NOT WIN32)

if(_MVCC_WAL EQUAL 1)
    add_compile_definitions(_MVCC_WAL_ENABLE=1)
endif()

if(_MEMLEAK EQUAL 1)
    add_compile_definitions(_MEMLEAK_ENABLE=1)
elseif(_MEMLEAK EQUAL 2)
    add_compile_definitions(_MEMLEAK_ENABLE=1)
    add_compile_definitions(_CHK_MODIFY_AFTER_FREE=1)
endif()

# Comment the line below to disable compiling Copy-On-Write (COW) optimizations
add_compile_definitions(_COW_COMPACTION=1)

add_compile_definitions(_GNU_SOURCE __DEBUG)

# set required flags for the selected compiler.
if("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-Qunused-arguments -g -fomit-frame-pointer -pthread)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-g -fomit-frame-pointer -pthread -Wno-class-memaccess -Wno-int-in-bool-context -Wno-format-overflow)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    add_compile_options(/wd4716 /W0)
endif()

set(BREAKPAD_SRC src/breakpad_dummy.cc)


include(cmake/Modules/ThreadSanitizer.cmake)

if(_ENCRYPTION)
    include(cmake/Modules/FindEncryptionLib)
endif(_ENCRYPTION)

set(FORESTDB_CORE_SRC
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

set(FORESTDB_UTILS_SRC
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
target_link_libraries(FDB_TOOLS_CORE ${PTHREAD_LIB} ${LIBM} ${SNAPPY_LIBRARIES}
    ${ASYNC_IO_LIB} ${MALLOC_LIBRARIES} ${PLATFORM_LIBRARY}
    ${LIBRT} ${CRYPTO_LIB}
    ${DL_LIBRARIES} ${BREAKPAD_LIBRARIES})


add_library(FDB_TOOLS_UTILS OBJECT ${FORESTDB_UTILS_SRC})
set_target_properties(FDB_TOOLS_UTILS PROPERTIES COMPILE_FLAGS "-D_FDB_TOOLS")
target_link_libraries(FDB_TOOLS_UTILS ${PTHREAD_LIB} ${LIBM} ${SNAPPY_LIBRARIES}
    ${ASYNC_IO_LIB} ${MALLOC_LIBRARIES} ${PLATFORM_LIBRARY}
    ${LIBRT} ${CRYPTO_LIB}
    ${DL_LIBRARIES} ${BREAKPAD_LIBRARIES})

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

if((NOT COUCHBASE_SERVER_BUILD) OR (INSTALL_HEADER_FILES))
    install(FILES
        include/libforestdb/forestdb.h
        include/libforestdb/fdb_types.h
        include/libforestdb/fdb_errors.h
        ${CMAKE_CURRENT_BINARY_DIR}/src/config.h
        DESTINATION include/libforestdb)
endif((NOT COUCHBASE_SERVER_BUILD) OR (INSTALL_HEADER_FILES))

install(TARGETS static_lib
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)

install(TARGETS forestdb_dump
    RUNTIME DESTINATION bin)

if("${CMAKE_BUILD_TYPE}" STREQUAL "Coverage")
    if((NOT COUCHBASE_SERVER_BUILD) AND (NOT WIN32))
        include(cmake/Modules/CodeCoverage)
        SETUP_TARGET_FOR_COVERAGE(test_coverage ctest ${PROJECT_SOURCE_DIR}/coverage)
    endif((NOT COUCHBASE_SERVER_BUILD) AND (NOT WIN32))
endif()

if(("${CMAKE_BUILD_TYPE}" STREQUAL "Valgrind") AND (NOT WIN32))
    include(cmake/Modules/MemoryCheck)
    SETUP_TARGET_FOR_MEMCHECK(test_valgrind ${PROJECT_BINARY_DIR}/fdb_functional_test)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(COUCHBASE_SERVER_BUILD)
    ENABLE_CODE_COVERAGE_REPORT()
endif()
