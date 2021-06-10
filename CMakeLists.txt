cmake_minimum_required(VERSION 3.20)
set (CMAKE_CXX_STANDARD 20)

include(ExternalProject)

project (shelter)

ExternalProject_Add(yaml_cpp
  GIT_REPOSITORY "https://github.com/jbeder/yaml-cpp.git"
  SOURCE_DIR yaml-cpp
  BINARY_DIR yaml-cpp-build
  INSTALL_DIR "${CMAKE_BINARY_DIR}/prefix"
  CMAKE_ARGS
    -DYAML_CPP_BUILD_TESTS=OFF
    -DYAML_BUILD_SHARED_LIBS=OFF
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
)

add_library(yaml-cpp UNKNOWN IMPORTED)
set_target_properties(yaml-cpp PROPERTIES
	IMPORTED_LOCATION ${PROJECT_BINARY_DIR}/prefix/lib64/${CMAKE_STATIC_LIBRARY_PREFIX}yaml-cpp${CMAKE_STATIC_LIBRARY_SUFFIX}
)

include_directories(SYSTEM "${PROJECT_BINARY_DIR}/prefix/include")

add_executable(shelter main.cxx)

add_dependencies(shelter yaml_cpp)
target_link_libraries(shelter
  yaml-cpp
)
