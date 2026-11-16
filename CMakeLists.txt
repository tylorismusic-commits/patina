cmake_minimum_required(VERSION 3.16)
project(patina VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
include(GNUInstallDirs)

set(CLAP_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/clap"
    CACHE PATH "Directory where CLAP bundles are installed")

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
pkg_check_modules(X11 REQUIRED IMPORTED_TARGET x11)

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

# ---------------------------------------------------------------------------
# CLAP plugin: five saturation models, one X11/Cairo editor.
# ---------------------------------------------------------------------------
add_library(patina MODULE
  src/clap/patina_clap.cpp
  src/ui/panel.cpp)
target_include_directories(patina PRIVATE
  "${CMAKE_SOURCE_DIR}/src"
  "${CMAKE_SOURCE_DIR}/third_party/clap/include")
target_link_libraries(patina PRIVATE
  PkgConfig::CAIRO PkgConfig::FREETYPE PkgConfig::X11
  Threads::Threads ${CMAKE_DL_LIBS})
target_compile_features(patina PRIVATE cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(patina PRIVATE -Wall -Wextra -fvisibility=hidden)
endif()
set_target_properties(patina PROPERTIES
  PREFIX ""
  OUTPUT_NAME "Patina"
  SUFFIX ".clap"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/clap")

set(PATINA_RESOURCE_FILES
  "${CMAKE_SOURCE_DIR}/assets/fonts/Jost-ExtraBold.ttf"
  "${CMAKE_SOURCE_DIR}/assets/fonts/OFL.txt")

add_custom_target(patina_bundle ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/clap/Patina.resources"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PATINA_RESOURCE_FILES}
          "${CMAKE_BINARY_DIR}/clap/Patina.resources/"
  DEPENDS patina ${PATINA_RESOURCE_FILES})

install(TARGETS patina LIBRARY DESTINATION "${CLAP_DIR}")
install(FILES ${PATINA_RESOURCE_FILES}
        DESTINATION "${CLAP_DIR}/Patina.resources")

find_library(XTST_LIBRARY Xtst)
find_program(XVFB_RUN xvfb-run)
add_executable(patina_clap_host_test test/clap_host_test.cpp)
target_include_directories(patina_clap_host_test PRIVATE
  "${CMAKE_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/third_party/clap/include")
target_link_libraries(patina_clap_host_test PRIVATE PkgConfig::X11 ${CMAKE_DL_LIBS})
if(XTST_LIBRARY)
  target_link_libraries(patina_clap_host_test PRIVATE ${XTST_LIBRARY})
else()
  message(WARNING "libXtst not found: the UI drag test will be skipped")
endif()
add_dependencies(patina_clap_host_test patina_bundle)
if(XVFB_RUN)
  add_test(NAME patina_clap_host
    COMMAND "${XVFB_RUN}" -a -s "-screen 0 1600x1000x24"
      "$<TARGET_FILE:patina_clap_host_test>" "${CMAKE_BINARY_DIR}/clap/Patina.clap")
else()
  add_test(NAME patina_clap_host
    COMMAND patina_clap_host_test "${CMAKE_BINARY_DIR}/clap/Patina.clap")
endif()

add_executable(patina_dsp_test test/test_saturation.cpp)
target_compile_features(patina_dsp_test PRIVATE cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(patina_dsp_test PRIVATE -Wall -Wextra)
endif()
add_test(NAME patina_dsp COMMAND patina_dsp_test)
