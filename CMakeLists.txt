cmake_minimum_required (VERSION 2.6)
project (mimic)
find_package (Threads)

include_directories("include")
add_library(mimiclib STATIC src/pollHandler.cpp src/eventNotifier.cpp src/eventQueue.cpp src/event.cpp src/eventHandler.cpp src/mimic.cpp src/connections.cpp src/serverWorker.cpp src/fileWorker.cpp src/basicClient.cpp)
add_executable(mimic src/test.cpp)   
add_executable(basicClient src/basicClient.cpp)
add_executable(eventTest src/eventTest.cpp)
add_executable(performanceTest src/performanceTests.cpp)
target_link_libraries (performanceTest ${CMAKE_THREAD_LIBS_INIT} mimiclib) 
target_link_libraries (basicClient ${CMAKE_THREAD_LIBS_INIT} mimiclib) 
target_link_libraries (eventTest ${CMAKE_THREAD_LIBS_INIT} mimiclib)
target_link_libraries (mimic ${CMAKE_THREAD_LIBS_INIT} mimiclib)   

set (TestProj_VERSION_MAJOR 1)
set (TestProj_VERSION_MINOR 0)

set (CMAKE_CXX_COMPILER /tmp/clang+llvm-6.0.0-x86_64-linux-gnu-ubuntu-16.04/bin/clang++)
#set (CMAKE_CXX_COMPILER /usr/bin/g++)

set (CMAKE_CXX_FLAGS "-Wall -g --std=gnu++2a --stdlib=libc++ -DASIO_STANDALONE")
#set (CMAKE_CXX_FLAGS "-Wall -g -DASIO_STANDALONE  -std=gnu++17")
