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
string(APPEND CMAKE_CXX_FLAGS " -pedantic -Werror -Wall -Wextra")
string(APPEND CMAKE_CXX_FLAGS " -Wno-unused-command-line-argument")
string(APPEND CMAKE_CXX_FLAGS " -Wshadow -Wnon-virtual-dtor")

set(Boost_INCLUDE_DIR /home/lamp/boost_1_75_0)
add_definitions(-DBOOST_LOG_DYN_LINK)

find_package(GTest CONFIG REQUIRED)

find_package(Boost COMPONENTS system thread filesystem log program_options date_time REQUIRED)

find_package(nlohmann_json)

find_package(Threads)

set(Boost_L Boost::log Boost::log_setup Boost::thread Boost::system Boost::filesystem Boost::date_time)

#===================================== LOGGER =====================================
add_library(logger INTERFACE)

target_include_directories(logger INTERFACE
        include/logger
        )

target_link_libraries(logger INTERFACE ${Boost_L})
#===================================== LOGGER =====================================


#===================================== SERVER =====================================
add_library(server INTERFACE)

target_include_directories(server INTERFACE
        include/server
        )

target_link_libraries(server INTERFACE logger ${Boost_L} nlohmann_json::nlohmann_json pthread
        )
#===================================== SERVER =====================================

#===================================== SERVER_EXE =================================
add_executable(server_exe
        sources/server_main.cpp
        )

target_link_libraries(server_exe server Boost::program_options logger ${Boost_L} nlohmann_json::nlohmann_json pthread
        )
#===================================== SERVER_EXE =================================

#===================================== SERVER_EXAMPLE =============================
add_executable(server_example
        demo/server_example.cpp
        )

target_link_libraries(server_example Boost::headers ${Boost_L} logger nlohmann_json::nlohmann_json)
#===================================== SERVER_EXAMPLE =============================
