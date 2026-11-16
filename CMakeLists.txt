cmake_minimum_required(VERSION 3.16)
project(patina VERSION 0.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
include(GNUInstallDirs)

set(CLAP_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/clap"
    CACHE PATH "Directory where CLAP bundles are installed")
set(LV2_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/lv2"
    CACHE PATH "Directory where LV2 bundles are installed")

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
pkg_check_modules(X11 REQUIRED IMPORTED_TARGET x11)
pkg_check_modules(LV2 REQUIRED IMPORTED_TARGET lv2)
pkg_check_modules(LILV QUIET IMPORTED_TARGET lilv-0)

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
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(patina_preview PRIVATE -Wall -Wextra)
endif()

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

# Release tarballs: CLAP, LV2, and a combined Linux bundle.
add_custom_target(dist-clap
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist"
  COMMAND ${CMAKE_COMMAND} -E tar czf
          "${CMAKE_BINARY_DIR}/dist/Patina-${PROJECT_VERSION}-clap.tar.gz"
          Patina.clap Patina.resources
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/clap"
  DEPENDS patina_bundle
  COMMENT "Packing Patina-${PROJECT_VERSION} CLAP")

add_custom_target(dist-lv2
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist"
  COMMAND ${CMAKE_COMMAND} -E tar czf
          "${CMAKE_BINARY_DIR}/dist/patina-${PROJECT_VERSION}-lv2.tar.gz"
          patina.lv2
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/lv2"
  DEPENDS patina_lv2_bundle
  COMMENT "Packing Patina-${PROJECT_VERSION} LV2")

add_custom_target(dist
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist"
  COMMAND ${CMAKE_COMMAND} -E tar czf
          "${CMAKE_BINARY_DIR}/dist/patina-${PROJECT_VERSION}-linux-x86_64.tar.gz"
          clap/Patina.clap clap/Patina.resources lv2/patina.lv2
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  DEPENDS patina_bundle patina_lv2_bundle
  COMMENT "Packing Patina-${PROJECT_VERSION} Linux bundle")

find_library(XTST_LIBRARY Xtst)
find_program(XVFB_RUN xvfb-run)
add_executable(patina_clap_host_test test/clap_host_test.cpp)
target_include_directories(patina_clap_host_test PRIVATE
  "${CMAKE_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/third_party/clap/include")
target_link_libraries(patina_clap_host_test PRIVATE PkgConfig::X11 ${CMAKE_DL_LIBS})
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(patina_clap_host_test PRIVATE -Wall -Wextra)
endif()
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

# ---------------------------------------------------------------------------
# LV2 build: same DSP and panel as CLAP, plus the LV2 UI and generated TTL.
# ---------------------------------------------------------------------------
set(LV2_BUNDLE_DIR "${CMAKE_BINARY_DIR}/lv2/patina.lv2")

add_library(patina_lv2 MODULE src/patina_lv2.cpp)
target_include_directories(patina_lv2 PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(patina_lv2 PRIVATE PkgConfig::LV2)
target_compile_features(patina_lv2 PRIVATE cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(patina_lv2 PRIVATE -Wall -Wextra -fvisibility=hidden)
endif()
set_target_properties(patina_lv2 PROPERTIES
  PREFIX ""
  OUTPUT_NAME "patina"
  LIBRARY_OUTPUT_DIRECTORY "${LV2_BUNDLE_DIR}")

add_library(patina_lv2_ui MODULE src/ui/lv2_ui.cpp src/ui/panel.cpp)
target_include_directories(patina_lv2_ui PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(patina_lv2_ui PRIVATE
  PkgConfig::CAIRO PkgConfig::FREETYPE PkgConfig::X11 PkgConfig::LV2
  Threads::Threads)
target_compile_features(patina_lv2_ui PRIVATE cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(patina_lv2_ui PRIVATE -Wall -Wextra -fvisibility=hidden)
endif()
set_target_properties(patina_lv2_ui PROPERTIES
  PREFIX ""
  OUTPUT_NAME "patina_ui"
  LIBRARY_OUTPUT_DIRECTORY "${LV2_BUNDLE_DIR}")

add_custom_target(patina_lv2_bundle ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory "${LV2_BUNDLE_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_SOURCE_DIR}/lv2/manifest.ttl"
          "${CMAKE_SOURCE_DIR}/lv2/patina.ttl"
          "${CMAKE_SOURCE_DIR}/lv2/presets.ttl"
          "${CMAKE_SOURCE_DIR}/assets/fonts/Jost-ExtraBold.ttf"
          "${CMAKE_SOURCE_DIR}/assets/fonts/OFL.txt"
          "${LV2_BUNDLE_DIR}/"
  DEPENDS patina_lv2 patina_lv2_ui)

install(TARGETS patina_lv2 patina_lv2_ui
        LIBRARY DESTINATION "${LV2_DIR}/patina.lv2")
install(FILES
  lv2/manifest.ttl
  lv2/patina.ttl
  lv2/presets.ttl
  assets/fonts/Jost-ExtraBold.ttf
  assets/fonts/OFL.txt
  DESTINATION "${LV2_DIR}/patina.lv2")

# ---------------------------------------------------------------------------
# Metadata generator: emits lv2/*.ttl from src/params.hpp and src/presets.hpp.
# The test fails if the committed TTL files are stale.
# ---------------------------------------------------------------------------
add_executable(patina_ttl_gen tools/gen_ttl.cpp)
target_include_directories(patina_ttl_gen PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_compile_features(patina_ttl_gen PRIVATE cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(patina_ttl_gen PRIVATE -Wall -Wextra)
endif()
add_test(NAME patina_ttl
  COMMAND patina_ttl_gen --check --root "${CMAKE_SOURCE_DIR}")

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
add_executable(patina_dsp_test test/test_saturation.cpp)
target_compile_features(patina_dsp_test PRIVATE cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(patina_dsp_test PRIVATE -Wall -Wextra)
endif()
add_test(NAME patina_dsp COMMAND patina_dsp_test)

if(LILV_FOUND)
  add_executable(patina_lv2_host_test test/lv2_host_test.cpp)
  target_include_directories(patina_lv2_host_test PRIVATE
    "${CMAKE_SOURCE_DIR}/src" ${PKG_CONFIG_LV2_INCLUDE_DIRS})
  target_link_libraries(patina_lv2_host_test PRIVATE PkgConfig::LILV)
  target_compile_features(patina_lv2_host_test PRIVATE cxx_std_17)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(patina_lv2_host_test PRIVATE -Wall -Wextra)
  endif()
  add_dependencies(patina_lv2_host_test patina_lv2_bundle)
  add_test(NAME patina_lv2_host
    COMMAND patina_lv2_host_test "${LV2_BUNDLE_DIR}")
endif()

add_executable(patina_lv2_ui_host_test test/lv2_ui_host_test.cpp)
target_include_directories(patina_lv2_ui_host_test PRIVATE
  "${CMAKE_SOURCE_DIR}/src" ${PKG_CONFIG_LV2_INCLUDE_DIRS})
target_link_libraries(patina_lv2_ui_host_test PRIVATE PkgConfig::X11 ${CMAKE_DL_LIBS})
target_compile_features(patina_lv2_ui_host_test PRIVATE cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(patina_lv2_ui_host_test PRIVATE -Wall -Wextra)
endif()
add_dependencies(patina_lv2_ui_host_test patina_lv2_bundle)
if(XVFB_RUN)
  add_test(NAME patina_lv2_ui_host
    COMMAND "${XVFB_RUN}" -a -s "-screen 0 1600x1000x24"
      "$<TARGET_FILE:patina_lv2_ui_host_test>" "${LV2_BUNDLE_DIR}")
else()
  add_test(NAME patina_lv2_ui_host
    COMMAND patina_lv2_ui_host_test "${LV2_BUNDLE_DIR}")
endif()
