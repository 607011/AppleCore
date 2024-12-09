cmake_minimum_required(VERSION 3.16)

project(mandelbrot
        VERSION 0.9.2
        DESCRIPTION "Mandelbrot set deep zoomer"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(BOOST_ROOT $ENV{BOOST_ROOT})
find_package(Boost 1.81.0 NO_MODULE REQUIRED)
message(STATUS "Boost include dirs: ${Boost_INCLUDE_DIRS}")

if(UNIX)
  set(CMAKE_CXX_FLAGS_PROFILING
    "${CMAKE_CXX_FLAGS_DEBUG} -pg"
    CACHE STRING "Flags used by the C++ compiler during profiling builds."
    FORCE)
  set(CMAKE_CXX_FLAGS "-pthread -Wno-deprecated-declarations")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O0 -g3 -ggdb -DDEBUG -pedantic -Wall -Wextra -Wreorder -Wuninitialized -Wtype-limits -Wno-pragmas")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG -Wno-deprecated -Wsign-compare")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG -Wno-deprecated -DDEBUG -ggdb")
endif(UNIX)

if (CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format-security")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -glldb")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG -Wno-deprecated -DDEBUG -glldb")
endif()

if (APPLE)
  enable_language(OBJCXX)
endif()

find_path(MPFR_INCLUDE_DIRS NAMES mpfr.h)
find_library(MPFR_LIBRARY NAMES mpfr libmpfr)

include(FetchContent)

FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG master
)
FetchContent_MakeAvailable(yaml-cpp)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 2.6.x
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(SFML)

add_definitions(-DPROJECT_NAME="${CMAKE_PROJECT_NAME}" -DPROJECT_VERSION="${CMAKE_PROJECT_VERSION}")
if (DEFINED HEADLESS)
  add_definitions(-DHEADLESS=1)
  set(SFML_BUILD_WINDOW FALSE)
endif()
set(SFML_BUILD_AUDIO FALSE)
set(SFML_BUILD_NETWORK FALSE)

set(MANDELBROT_SOURCES 
  src/main.cpp
  src/util.cpp
)

if (APPLE)
set(MANDELBROT_SOURCES ${MANDELBROT_SOURCES}
  src/mandelbrot_metal.mm
)
endif()

add_executable(mandelbrot ${MANDELBROT_SOURCES})

target_compile_features(mandelbrot PRIVATE cxx_std_17)

target_include_directories(mandelbrot
  PRIVATE ${PROJECT_INCLUDE_DIRS}
  build
  PUBLIC ${SFML_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS}
  ${MPRF_INCLUDE_DIRS}
)

target_link_libraries(mandelbrot 
  PUBLIC 
  ${MPFR_LIBRARY}
  yaml-cpp::yaml-cpp
  sfml-graphics
)

if(UNIX)
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET mandelbrot
      POST_BUILD
      COMMAND strip mandelbrot)
  endif()
endif(UNIX)
