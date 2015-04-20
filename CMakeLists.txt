cmake_minimum_required(VERSION 2.8)
set (ASYNC async)
set (ASYNCERR asyncerr)
set (TEST testing)
set (SYNC sync)
set (UNIXSOCK unix)
set (THREADEDPOOL threadedpool)

set(PROJECT librediscluster)

project (${PROJECT})

string(REGEX MATCH ".*clang.*" USE_CLANG ${CMAKE_CXX_COMPILER})
IF (NOT USE_CLANG)
   IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
      SET(USE_CLANG TRUE)
   ENDIF()
ENDIF(NOT USE_CLANG)

if(USE_CLANG)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -stdlib=libc++")
else(USE_CLANG)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
endif(USE_CLANG)

set(HEADERS
	include/asynchirediscommand.h
	include/cluster.h
	include/container.h
	include/hirediscommand.h
	include/hiredisprocess.h
	include/slothash.h
	include/clusterexception.h)

include_directories(include)

set(ASYNC_SOURCES
        src/examples/asyncexample.cpp)

set(SYNC_SOURCES
        src/examples/example.cpp)

set(UNIX_SOURCES
        src/examples/unixsocketexample.cpp)

set(TEST_SOURCES
        src/testing/testing.cpp)

set(ASYNCERR_SOURCES
	src/examples/asyncerrorshandling.cpp)

set(THREADEDPOOL_SOURCES
        src/examples/threadpool.cpp)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin/)

add_executable (${ASYNC} ${HEADERS} ${ASYNC_SOURCES})
add_executable (${SYNC} ${HEADERS} ${SYNC_SOURCES})
add_executable (${TEST} ${HEADERS} ${TEST_SOURCES})
add_executable (${UNIXSOCK} ${HEADERS} ${UNIX_SOURCES})
add_executable (${ASYNCERR} ${HEADERS} ${ASYNCERR_SOURCES})
add_executable (${THREADEDPOOL} ${HEADERS} ${THREADEDPOOL_SOURCES})

if(USE_CLANG)
target_link_libraries (${ASYNC} libhiredis.dylib libevent.dylib)
target_link_libraries (${TEST} libhiredis.dylib libevent.dylib)
target_link_libraries (${ASYNCERR} libhiredis.dylib libevent.dylib)
target_link_libraries (${THREADEDPOOL} libhiredis.dylib)
else(USE_CLANG)
target_link_libraries (${ASYNC} libhiredis.so libevent.so librt.so)
target_link_libraries (${TEST} libhiredis.so libevent.so librt.so)
target_link_libraries (${ASYNCERR} libhiredis.so libevent.so librt.so)
target_link_libraries (${THREADEDPOOL} libhiredis.so libpthread.so)
endif(USE_CLANG)

target_link_libraries (${SYNC} libhiredis.a)
target_link_libraries (${UNIXSOCK} libhiredis.a)
