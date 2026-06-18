CMAKE_MINIMUM_REQUIRED(VERSION 3.16)
project(ming_utils)
set(VERSION 1.1)

# CMake policies: enable MACOSX_RPATH by default
if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW)
endif()

option(BUILD_DEMO "build demo" OFF)

set(CMAKE_CXX_STANDARD 11)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

file(GLOB src_files
  src/*.cpp
  src/log/*.cpp
)
file(GLOB inc_files
  include/*.h
)
add_library(mutils SHARED
  ${src_files}
)
target_include_directories(mutils PRIVATE include)
target_include_directories(mutils INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/mutils>
)
set_target_properties(mutils PROPERTIES
  PUBLIC_HEADER "${inc_files}"
)
add_library(mutils_static STATIC
  ${src_files}
)
set_target_properties(mutils_static PROPERTIES
  OUTPUT_NAME mutils
  POSITION_INDEPENDENT_CODE ON
  PUBLIC_HEADER "${inc_files}"
)
target_include_directories(mutils_static PRIVATE include)
target_include_directories(mutils_static INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/mutils>
)
install (TARGETS mutils mutils_static
  EXPORT mutils-config
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  PUBLIC_HEADER DESTINATION include/mutils
)
install(EXPORT mutils-config
  DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/mutils"
  NAMESPACE rokid::
)

# build demo
if (BUILD_DEMO)
include(${CUSTOM_CMAKE_MODULES}/common.mk)
findPackage(gtest REQUIRED
  HINTS ${gtestPrefix}
  HEADERS gtest/gtest.h
  STATIC_LIBS gtest
)

add_executable (xmopt-demo demo/xmopt-demo.cpp)
target_include_directories(xmopt-demo PRIVATE include)
add_executable (vq-demo demo/vq-demo.cpp)
target_link_libraries (vq-demo mutils)
add_executable (uri-demo demo/uri-demo.cpp)
target_link_libraries (uri-demo mutils)
add_executable(rlog-demo demo/rlog-demo.cpp)
target_link_libraries(rlog-demo mutils)
add_executable(tcp-rlogcat demo/tcp-rlogcat.cpp)
add_executable(heapsort-demo demo/heapsort-demo.cpp)
target_include_directories(heapsort-demo PRIVATE include)
add_executable(thr-pool-demo demo/thr-pool-demo.cpp)
target_include_directories(thr-pool-demo PRIVATE include)
add_library(global-error1 SHARED tests/misc/global-error-shared1.cpp)
target_include_directories(global-error1 PRIVATE include)
target_link_libraries(global-error1 mutils_static)
add_library(global-error2 SHARED tests/misc/global-error-shared2.cpp)
target_include_directories(global-error2 PRIVATE include)
target_link_libraries(global-error2 mutils_static)

add_executable(tests
  tests/main.cpp
  tests/misc/test-circle-stream.cpp
  tests/misc/test-thr-pool.cpp
  tests/misc/test-global-error.cpp
)
target_include_directories(tests PRIVATE
  include
  ${gtest_INCLUDE_DIRS}
)
target_link_libraries(tests
  ${gtest_LIBRARIES}
  global-error1
  global-error2
)
endif(BUILD_DEMO)
