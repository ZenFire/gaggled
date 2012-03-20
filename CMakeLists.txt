################################################################################

cmake_minimum_required (VERSION 2.6)
if(EXISTS ${CMAKE_SOURCE_DIR}/local.cmake.txt)
  include(${CMAKE_SOURCE_DIR}/local.cmake.txt)
endif()

project(gaggled)
set(CPACK_PACKAGE_VENDOR "BigWells Technology")

set(CMAKE_MODULE_PATH
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/
  ${CMAKE_MODULE_PATH}
  )

## begin version code ##########################################################
include(read_version)
read_version(gaggled)
set(PACKAGE_VERSION ${gaggled_VERSION})
## end version code ############################################################

# set up compiler
if(CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -pthread -lzmq -Wuninitialized -Wimplicit -Wall -pedantic -pedantic-errors -Wextra -Wcast-align -Wcast-qual  -Wchar-subscripts -Wno-unused-parameter -Wcomment -Wdisabled-optimization -Werror -Wfloat-equal -Wformat -Wformat=2 -Wformat-nonliteral -Wformat-security -Wformat-y2k -Wimport  -Winit-self  -Winline -Winvalid-pch -Wunsafe-loop-optimizations  -Wlong-long -Wmissing-braces -Wmissing-field-initializers -Wmissing-format-attribute")
  add_definitions(-g -Wall)

  set(CMAKE_CXX_FLAGS_DEBUG "-g")
endif(CMAKE_COMPILER_IS_GNUCXX)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/contrib/gaggled.spec.in ${CMAKE_BINARY_DIR}/gaggled.spec)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(src/)

# boost ########################################################################
set(Boost_USE_MULTITHREADED ON)
find_package(Boost 1.42.0 REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

configure_file(
  ${CMAKE_SOURCE_DIR}/src/gv.hpp.in
  ${CMAKE_BINARY_DIR}/gv.hpp
  COMMENT "generated gv.hpp."
  )

set (gaggled_SOURCES
  src/Gaggled.cpp
  src/Event.cpp
  src/Program.cpp
  src/Dependency.cpp
  )

set (gaggled_MAIL
  src/util/smtp.cpp
  )

add_library(gaggled_lib ${gaggled_SOURCES})
add_library(gaggled_maillib ${gaggled_MAIL})

add_executable(gaggled src/gaggled_main.cpp)
target_link_libraries(gaggled gaggled_lib)

# once test cases for communication are working/shipping, this will be enabled
#add_executable(gaggled_test_ps src/gaggled_events_tests.cpp)
#add_executable(gaggled_test_rr src/gaggled_control_tests.cpp)

add_executable(gaggled_listener src/gaggled_listener.cpp)
add_executable(gaggled_smtpgate src/gaggled_smtpgate.cpp)
add_executable(gaggled_controller src/gaggled_controller.cpp)

target_link_libraries(gaggled_smtpgate gaggled_maillib)

install(TARGETS gaggled gaggled_smtpgate gaggled_controller gaggled_listener DESTINATION bin)

