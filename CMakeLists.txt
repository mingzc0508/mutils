CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
project(ming_utils)
set(VERSION 1.1)

# CMake policies: enable MACOSX_RPATH by default
if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" )
  set(COMPILER_IS_CLANG true)
else()
  set(COMPILER_IS_CLANG false)
endif()

option(BUILD_DEBUG "debug or release" OFF)
option(BUILD_DEMO "build demo" OFF)

if (BUILD_DEBUG)
  set (common_cxxflags "-g -O0")
else()
  set (common_cxxflags -O3)
endif()
if (NOT ${COMPILER_IS_CLANG})
  set (common_ldflags "${common_ldflags} -pthread")
endif()
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 ${common_cxxflags}")
set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${common_ldflags}")
set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${common_ldflags}")

file(GLOB src_files
  include/*.h
  src/*.cpp
  src/log/*.cpp
)
add_library(mutils SHARED
  ${src_files}
)
target_include_directories(mutils PUBLIC
  include
)
add_library(mutils_static STATIC
  ${src_files}
)
set_target_properties(mutils_static PROPERTIES
  OUTPUT_NAME mutils
  POSITION_INDEPENDENT_CODE ON
)
target_include_directories(mutils_static PUBLIC
  include
)
install(DIRECTORY include/
  DESTINATION include/mutils
)
install (TARGETS mutils mutils_static
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
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
