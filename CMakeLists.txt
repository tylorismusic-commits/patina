cmake_minimum_required(VERSION 3.16)
project(patina VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)

set(HEARTH_DSP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/hearth-dsp"
    CACHE PATH "Path to the hearth-dsp checkout")
if(NOT EXISTS "${HEARTH_DSP_DIR}/include/hdsp/hdsp.hpp")
  message(FATAL_ERROR "hearth-dsp not found at ${HEARTH_DSP_DIR}")
endif()
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src"
                    "${HEARTH_DSP_DIR}/include")

enable_testing()

add_library(patina_ui src/ui/panel.cpp)
target_link_libraries(patina_ui PUBLIC PkgConfig::CAIRO PkgConfig::FREETYPE)
target_compile_features(patina_ui PUBLIC cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(patina_ui PRIVATE -Wall -Wextra)
endif()

add_executable(patina_preview test/ui_preview.cpp)
target_link_libraries(patina_preview PRIVATE patina_ui)
target_compile_features(patina_preview PRIVATE cxx_std_17)

add_executable(patina_dsp_test test/test_saturation.cpp)
target_compile_features(patina_dsp_test PRIVATE cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(patina_dsp_test PRIVATE -Wall -Wextra)
endif()
add_test(NAME patina_dsp COMMAND patina_dsp_test)
