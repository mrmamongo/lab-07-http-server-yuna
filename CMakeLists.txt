cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTS "Build tests" ON)
option(BUILD_DOCS "Build documentation" OFF)
option(BUILD_COVERAGE "Build code coverage" OFF)

#set(
#  HUNTER_CACHE_SERVERS
#  "https://github.com/bmstu-iu8-cpp-sem-3/hunter-cache"
#  CACHE STRING "Default cache server"
#)
#
#include("cmake/HunterGate.cmake")
#
#HunterGate(
#  URL "https://github.com/cpp-pm/hunter/archive/v0.23.278.tar.gz"
#  SHA1 "35fa55bc1dfcf9f4d7264f8bb26cd1759ce1ae07"
#  LOCAL
#)

project(http-server)
string(APPEND CMAKE_CXX_FLAGS " -pedantic -Wall -Wextra")
#string(APPEND CMAKE_CXX_FLAGS " -Wno-unused-command-line-argument")
string(APPEND CMAKE_CXX_FLAGS " -Wshadow -Wnon-virtual-dtor")

set(Boost_INCLUDE_DIR /home/lamp/boost_1_75_0)
add_definitions(-DBOOST_LOG_DYN_LINK)

find_package(Boost COMPONENTS system thread filesystem log program_options date_time REQUIRED)
add_subdirectory(third-party/json)
#set(nlohmann_json_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third-party/json)
#find_package(nlohmann_json REQUIRED)
message(nlohmann_json_DIR)
find_package(Threads)

set(Boost_L Boost::log Boost::log_setup Boost::thread Boost::system Boost::filesystem Boost::date_time nlohmann_json::nlohmann_json)

#===================================== SERVER =====================================
add_library(server INTERFACE)

target_include_directories(server INTERFACE
        include/server
        )

target_link_libraries(server INTERFACE ${Boost_L} pthread
        )
#===================================== SERVER =====================================

#===================================== SERVER_EXE =================================
add_executable(server_exe
        sources/server_main.cpp
        )

target_link_libraries(server_exe server Boost::program_options ${Boost_L} pthread
        )
#===================================== SERVER_EXE =================================

#===================================== SERVER_EXAMPLE =============================
add_executable(server_example
        demo/server_example.cpp
        )

target_link_libraries(server_example Boost::headers ${Boost_L} )
#===================================== SERVER_EXAMPLE =============================
