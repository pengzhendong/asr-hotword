cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(asr_hotword VERSION 0.1)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
get_filename_component(fc_base
  "fc_base-${CMAKE_CXX_COMPILER_ID}"
  REALPATH BASE_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}"
)
set(FETCHCONTENT_BASE_DIR ${fc_base})
option(BUILD_TESTING "whether to build unit test" OFF)
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include_directories(${PROJECT_SOURCE_DIR})

if(NOT MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -pthread -fPIC")
else()
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
  add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
endif()
if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
  set(CMAKE_MACOSX_RPATH 1)
endif()

include(gflags)
set(WITH_GFLAGS OFF CACHE BOOL "whether build glog with gflags" FORCE)
include(glog)
include(openfst)

add_library(hotword
  hotword/context_graph.cc
  utils/string.cc
)
if(MSVC)
  target_link_libraries(hotword PUBLIC fst)
else()
  target_link_libraries(hotword PUBLIC dl fst)
endif()

add_executable(hotword_main bin/hotword_main.cc)
target_link_libraries(hotword_main PUBLIC hotword gflags)

if(BUILD_TESTING)
  include(gtest)
  add_subdirectory(test)
endif()
