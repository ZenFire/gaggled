################################################################################

cmake_minimum_required (VERSION 2.6)
if(EXISTS ${CMAKE_SOURCE_DIR}/local.cmake.txt)
  include(${CMAKE_SOURCE_DIR}/local.cmake.txt)
endif()

project(gaggled)
set(CPACK_PACKAGE_VENDOR "BigWells Technology")
set(gaggled_VERSION_MAJOR "0")
set(gaggled_VERSION_MINOR "1")
set(gaggled_VERSION_PATCH "0")

set(gaggled_VERSION "${gaggled_VERSION_MAJOR}.${gaggled_VERSION_MINOR}.${gaggled_VERSION_PATCH}")
set(PACKAGE_VERSION ${gaggled_VERSION})

# set up compiler
if(CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -pthread -lzmq -Wuninitialized")
  add_definitions(-g -Wall)

  set(CMAKE_CXX_FLAGS_DEBUG "-g")
endif(CMAKE_COMPILER_IS_GNUCXX)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/contrib/gaggled.spec.in ${CMAKE_BINARY_DIR}/gaggled.spec)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(src/)

# boost ########################################################################
set(Boost_USE_MULTITHREADED ON)
find_package(Boost 1.44.0 REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

set (gaggled_SOURCES
  src/Gaggled.cpp
  src/Event.cpp
  src/Program.cpp
  src/Dependency.cpp
  )

add_library(gaggled_lib ${gaggled_SOURCES})

add_executable(gaggled src/gaggled_main.cpp)
target_link_libraries(gaggled gaggled_lib)

# once test cases for communication are working/shipping, this will be enabled
#add_executable(gaggled_test_ps src/gaggled_events_tests.cpp)
#add_executable(gaggled_test_rr src/gaggled_control_tests.cpp)

add_executable(gaggled_listener src/gaggled_listener.cpp)
add_executable(gaggled_controller src/gaggled_controller.cpp)

install(TARGETS gaggled DESTINATION bin)

