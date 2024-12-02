cmake_minimum_required(VERSION 3.22)

project(mandelbrot
        VERSION 0.0.1
        DESCRIPTION "Mandelbrot set deep zoomer"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(UNIX)
  set(CMAKE_CXX_FLAGS_PROFILING
    "${CMAKE_CXX_FLAGS_DEBUG} -pg"
    CACHE STRING "Flags used by the C++ compiler during profiling builds."
    FORCE)
  set(CMAKE_CXX_FLAGS "-pthread -Wno-deprecated-declarations")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O0 -g3 -ggdb -DDEBUG -pedantic -Wall -Wextra -Wreorder -Wuninitialized -Wtype-limits -Wno-pragmas")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG -Wno-deprecated")
elseif(WIN32)
  set(CMAKE_CXX_FLAGS_DEBUG "/Od /Wall /Zi /wd5045 /wd5039 /wd5027 /wd4191 /wd4365 /wd4668 /wd4355 /wd4800 /wd4774 /wd4606 /wd4626 /wd4625 /wd4061 /wd4514 /wd4820 /D_CRT_SECURE_NO_WARNINGS /D__STDC_WANT_SECURE_LIB__")
  set(CMAKE_CXX_FLAGS_RELEASE "/O2 /W1 /DNDEBUG /D_CRT_SECURE_NO_WARNINGS /D__STDC_WANT_SECURE_LIB__")
endif(UNIX)

if (CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format-security")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -glldb")
endif()

find_path(GMP_INCLUDE_DIRS NAMES gmp.h)
find_library(GMP_LIBRARY NAMES gmp libgmp)

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
endif()
set(SFML_BUILD_AUDIO FALSE)
set(SFML_BUILD_NETWORK FALSE)

set(MANDELBROT_SOURCES 
  src/mandelbrot.cpp
  src/util.cpp
)

add_executable(mandelbrot ${MANDELBROT_SOURCES})

target_compile_features(mandelbrot PRIVATE cxx_std_17)

target_include_directories(mandelbrot
  PRIVATE ${PROJECT_INCLUDE_DIRS}
  build
  PUBLIC ${SFML_INCLUDE_DIRS}
  ${GMP_INCLUDE_DIRS}
)

target_link_libraries(mandelbrot 
  PUBLIC 
  ${GMP_LIBRARY}
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
