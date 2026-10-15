# Heaviside: OpenColorIO built for WebAssembly.
#
#   emcmake cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#
# Output: build/heaviside.mjs and build/heaviside.wasm

cmake_minimum_required(VERSION 3.20)
project(Heaviside LANGUAGES C CXX)

if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "Configure with emcmake (Emscripten).")
endif()

# OCIO uses C++ exceptions for control flow, not only errors: a LUT file is offered to
# each reader in turn and a failed read throws. Every object needs exception support or
# those catches are compiled out. These are the global flags, not add_compile_options,
# so they also reach the dependencies OCIO builds as external projects.
string(APPEND CMAKE_C_FLAGS " -fwasm-exceptions")
string(APPEND CMAKE_CXX_FLAGS " -fwasm-exceptions")
add_link_options(-fwasm-exceptions)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(OCIO_BUILD_APPS OFF CACHE BOOL "" FORCE)
set(OCIO_BUILD_PYTHON OFF CACHE BOOL "" FORCE)
set(OCIO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(OCIO_BUILD_GPU_TESTS OFF CACHE BOOL "" FORCE)
set(OCIO_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(OCIO_BUILD_OPENFX OFF CACHE BOOL "" FORCE)
set(OCIO_BUILD_JAVA OFF CACHE BOOL "" FORCE)
set(OCIO_USE_SIMD OFF CACHE BOOL "" FORCE)
set(OCIO_INSTALL_EXT_PACKAGES ALL CACHE STRING "" FORCE)
add_subdirectory(extern/OpenColorIO EXCLUDE_FROM_ALL)

# YY.MMDD.HHMM, the commit time in New York; CI sets it (.github/workflows/release.yml)
set(HEAVISIDE_VERSION "dev" CACHE STRING "Heaviside version, reported by build()")

add_executable(heaviside src/heaviside.cpp)
target_compile_features(heaviside PRIVATE cxx_std_17)
target_compile_definitions(heaviside PRIVATE HEAVISIDE_VERSION="${HEAVISIDE_VERSION}")
target_link_libraries(heaviside PRIVATE OpenColorIO embind)
set_target_properties(heaviside PROPERTIES SUFFIX ".mjs")
target_link_options(heaviside PRIVATE
  -sMODULARIZE=1
  -sEXPORT_ES6=1
  -sEXPORT_NAME=Heaviside
  -sENVIRONMENT=web,worker,node
  -sALLOW_MEMORY_GROWTH=1
  -sFORCE_FILESYSTEM=1
  -sEXPORTED_RUNTIME_METHODS=FS)
