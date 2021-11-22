cmake_minimum_required(VERSION 3.20)
set (CMAKE_CXX_STANDARD 20)

include(ExternalProject)

project (shelter)

set(VERSION 1.0.3)

option(USE_LTO "Use link time optimization" TRUE)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION USE_LTO)

if (MSVC)
  # warning level 4 and all warnings as errors
  add_compile_options(/W4 /WX)
else()
  # lots of warnings and all warnings as errors
  add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

# this needs for clang-tidy to get include paths etc
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

ExternalProject_Add(lyra
  GIT_REPOSITORY "https://github.com/bfgroup/Lyra.git"
  SOURCE_DIR lyra
  INSTALL_DIR "${CMAKE_BINARY_DIR}"
  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
)

ExternalProject_Add(yaml_cpp
  GIT_REPOSITORY "https://github.com/jbeder/yaml-cpp.git"
  SOURCE_DIR yaml-cpp
  BINARY_DIR yaml-cpp-build 
  INSTALL_DIR "${CMAKE_BINARY_DIR}"
  CMAKE_ARGS
    -DYAML_CPP_BUILD_TESTS=OFF
    -DYAML_BUILD_SHARED_LIBS=OFF
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_LIBDIR:PATH=<INSTALL_DIR>/lib
    -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
)

add_library(yaml-cpp UNKNOWN IMPORTED)

set_target_properties(yaml-cpp PROPERTIES
IMPORTED_LOCATION ${PROJECT_BINARY_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}yaml-cpp${CMAKE_STATIC_LIBRARY_SUFFIX})

include_directories(SYSTEM "${PROJECT_BINARY_DIR}/include")

# Get the current working branch
execute_process(
    COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE)

# Get the latest commit hash
execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE)

add_definitions( -DVERSION_CMAKE=${VERSION} )
add_definitions( -DBRANCH_CMAKE=${GIT_BRANCH} )
add_definitions( -DHASH_CMAKE=${GIT_COMMIT_HASH} )

add_executable(shelter main.cxx)

add_dependencies(shelter yaml_cpp lyra)
target_precompile_headers(shelter
  PRIVATE
    stdafx.hpp
)
target_link_libraries(shelter
  yaml-cpp
)
