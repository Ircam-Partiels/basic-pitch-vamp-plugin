cmake_minimum_required(VERSION 3.18)

set(BPVP_TESTER_ENABLE ON CACHE STRING "Enables the tests with the vamp plugin tester")
set(BPVP_TESTER_TIMEOUT 600 CACHE STRING "The timeout in seconds of the test with the vamp plugin tester (zero for no timeout)")

option(BPVP_ENABLE_DEBUG "Enables the debug scheme using the Partiels application" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_XCODE_GENERATE_SCHEME ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3" CACHE STRING "Minimum OS X deployment version")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "Default value for MSVC_RUNTIME_LIBRARY of targets" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE CACHE BOOL "Default value for POSITION_INDEPENDENT_CODE of targets" FORCE)

### Project ###
project(BasicPitchVampPlugin VERSION 1.0.0 LANGUAGES C CXX)

### Models only ###
set(BPVP_MODELS_ONLY OFF CACHE BOOL "Only downloads the tflite model (used by the CI cache, without TensorFlow and the other dependencies)")

### Manual only ###
set(BPVP_MANUAL_ONLY OFF CACHE BOOL "Only generates the documentation (without the models, TensorFlow and the other dependencies)")

### Sccache ###
set(BPVP_USE_SCCACHE OFF CACHE BOOL "Uses sccache as the compiler launcher")
if(BPVP_USE_SCCACHE AND NOT BPVP_MODELS_ONLY)
  if(NOT CMAKE_C_COMPILER_LAUNCHER AND NOT CMAKE_CXX_COMPILER_LAUNCHER)
    find_program(BPVP_SCCACHE_PROGRAM sccache)
    if(BPVP_SCCACHE_PROGRAM)
      set(CMAKE_C_COMPILER_LAUNCHER "${BPVP_SCCACHE_PROGRAM}" CACHE STRING "C compiler launcher" FORCE)
      set(CMAKE_CXX_COMPILER_LAUNCHER "${BPVP_SCCACHE_PROGRAM}" CACHE STRING "C++ compiler launcher" FORCE)
      set(BPVP_SCCACHE_LAUNCHER_MANAGED ON CACHE INTERNAL "Whether the Basic Pitch Vamp Plugin enabled the compiler launcher with sccache")
      message(VERBOSE "sccache found, compiler launcher enabled")
    else()
      message(VERBOSE "sccache not found, continuing without compiler cache")
    endif()
  endif()
elseif(BPVP_SCCACHE_LAUNCHER_MANAGED)
  unset(CMAKE_C_COMPILER_LAUNCHER CACHE)
  unset(CMAKE_CXX_COMPILER_LAUNCHER CACHE)
  unset(BPVP_SCCACHE_LAUNCHER_MANAGED CACHE)
  message(VERBOSE "sccache compiler launcher disabled")
endif()

if(NOT BPVP_MODELS_ONLY AND NOT BPVP_MANUAL_ONLY)
### Include Vamp (IVE) ###
set(IVE_BUILD_HOST_LIBRARY OFF)
set(IVE_BUILD_PLUGIN_LIBRARY ON)
add_subdirectory(ircam-vamp-extension EXCLUDE_FROM_ALL)

### Include TensorFlow ###
find_package(Python COMPONENTS Interpreter)
if(NOT Python_FOUND)
  message(FATAL_ERROR "Python not found: please install Python that is required by TensorFlow")
endif()

set(TENSORFLOW_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tensorflow" CACHE PATH "" FORCE)
set(ABSL_MSVC_STATIC_RUNTIME ON CACHE BOOL "Link Abseil with the static MSVC runtime (/MT) like the rest of the project" FORCE)
set(TFLITE_ENABLE_BENCHMARK OFF)
set(TFLITE_ENABLE_TESTS OFF)
set(TFLITE_ENABLE_INSTALL OFF)

set(SYSTEM_FARMHASH OFF)
set(SYSTEM_PTHREADPOOL OFF)
set(TFLITE_ENABLE_XNNPACK ON)
set(TFLITE_ENABLE_RUY ON)
set(BUILD_SHARED_LIBS OFF)
set(ABSL_PROPAGATE_CXX_STD ON)

add_subdirectory(tensorflow/tensorflow/lite EXCLUDE_FROM_ALL)

set_target_properties(tensorflow-lite PROPERTIES CXX_STANDARD 20)
set_target_properties(XNNPACK PROPERTIES CXX_STANDARD 20)

### TensorFlow compiler settings (the compiler is only known after project()) ###
if(APPLE)
  add_dependencies(profiling_info_proto model_runtime_info_proto)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # model_building.cc is only used by TensorFlow tests and fails to compile with MSVC (C2248 on a friend class)
  set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/tensorflow/tensorflow/lite/core/model_building.cc TARGET_DIRECTORY tensorflow-lite PROPERTIES HEADER_FILE_ONLY ON)
endif()

foreach(TFLIB IN ITEMS absl_debugging_internal absl_base kleidiai absl_synchronization pthreadpool eight_bit_int_gemm farmhash cpuinfo XNNPACK xnnpack-delegate xnnpack-allocator xnnpack-base xnnpack-cache xnnpack-datatype xnnpack-fingerprint-cache xnnpack-fingerprint-check xnnpack-fingerprint-id xnnpack-hardware-config xnnpack-indirection xnnpack-logging xnnpack-memory xnnpack-microkernel-utils xnnpack-microkernels-all xnnpack-microkernels-prod xnnpack-microparams-init xnnpack-sanitizers xnnpack-mutex xnnpack-normalization xnnpack-operator-delete xnnpack-operator-run xnnpack-operator-utils xnnpack-operators xnnpack-pack-lh xnnpack-packing xnnpack-reference-ukernels xnnpack-subgraph ruy_prepacked_cache ruy_thread_pool ruy_profiler_instrumentation ruy_have_built_path_for_avx ruy_have_built_path_for_avx2_fma ruy_have_built_path_for_avx512 ruy_ctx ruy_context ruy_kernel_arm ruy_context_get_ctx ruy_trmul ruy_prepare_packed_matrices ruy_pack_arm ruy_frontend tensorflow-lite ruy_kernel_avx512 flatc flatbuffers-flatc)
  set(TFLIB_TYPE "")
  if(TARGET ${TFLIB})
    get_property(TFLIB_TYPE TARGET ${TFLIB} PROPERTY TYPE)
  endif()
  if(TFLIB_TYPE MATCHES "_LIBRARY$|EXECUTABLE" AND NOT TFLIB_TYPE STREQUAL "INTERFACE_LIBRARY")
    if(CMAKE_GENERATOR STREQUAL "Xcode")
      target_compile_options(${TFLIB} PRIVATE -Wno-shorten-64-to-32 -Wno-deprecated-this-capture)
      set_target_properties(${TFLIB} PROPERTIES STATIC_LIBRARY_OPTIONS "-no_warning_for_no_symbols")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_options(${TFLIB} PRIVATE -Wno-comment -Wno-unknown-pragmas -Wno-deprecated -Wno-deprecated-declarations -Wno-return-type)  
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
      target_compile_options(${TFLIB} PRIVATE /wd4006 /wd4068 /wd4098 /wd4190 /wd4244 /wd4267 /wd4273 /wd4319 /wd4530 /wd4715 /wd4804 /wd4834 /wd4996 /wd5287)
    endif()
  endif()
endforeach()
endif()

### Version ###
execute_process(COMMAND git log -1 --format=%h WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR} OUTPUT_VARIABLE GIT_COMMIT_ID OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND git status --porcelain WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR} OUTPUT_VARIABLE GIT_HAS_DIFF OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT GIT_HAS_DIFF STREQUAL "")
  set(GIT_COMMIT_ID "${GIT_COMMIT_ID}-head")
endif()

if(NOT BPVP_BUILD_TAG OR BPVP_BUILD_TAG STREQUAL "")
  set(BPVP_BUILD_TAG ${CMAKE_PROJECT_VERSION})
endif()
message(STATUS "Basic Pitch Vamp Plugin: " ${BPVP_BUILD_TAG} " (" ${GIT_COMMIT_ID} ")")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/version.txt ${BPVP_BUILD_TAG})

if(NOT BPVP_MODELS_ONLY AND NOT BPVP_MANUAL_ONLY)
### Package ###
set(VPP_NAME "Basic-Pitch" CACHE PATH "The name of the project" FORCE)
set(VPP_COMPANY "Ircam" CACHE PATH "The name of the company" FORCE)
set(VPP_URL "www.ircam.fr" CACHE PATH "The URL of the project")
set(VPP_APPID "9538f07d-8bf4-460b-ac1c-920465ff316a" CACHE PATH "The unique ID of the project")
set(VPP_BUILD_TAG "${BPVP_BUILD_TAG}" CACHE STRING "The current tag of the project" FORCE)
set(VPP_ABOUT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resource/About.txt" CACHE PATH "The about file to add to the distribution" FORCE)
set(VPP_CHANGELOG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resource/ChangeLog.txt" CACHE PATH "The changelog file to add to the distribution" FORCE)
set(VPP_ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resource/Ircam-logo-noir-RS.png" CACHE PATH "The icon file to add to the distribution" FORCE)
set(VPP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/package CACHE PATH "The directory of the package" FORCE)
include(vamp-plugin-packager/vamp-plugin-packager.cmake)
endif()

### Models ###
# The tflite model is not compiled into the plugin: it is downloaded here
# and copied into the Resources folder of the Vamp bundle (see the POST_BUILD commands).
if(NOT BPVP_MANUAL_ONLY)
set(BPVP_MODELS_DIR "${CMAKE_CURRENT_BINARY_DIR}/models")
file(GLOB BPVP_MODEL_TFLITE_LIST ${BPVP_MODELS_DIR}/*.tflite)
list(LENGTH BPVP_MODEL_TFLITE_LIST BPVP_MODEL_TFLITE_LIST_SIZE)
if(NOT BPVP_MODEL_TFLITE_LIST_SIZE EQUAL 1)
  file(REMOVE_RECURSE "${BPVP_MODELS_DIR}")
  file(MAKE_DIRECTORY "${BPVP_MODELS_DIR}")

  message(STATUS "Downloading model nmp.tflite")
  set(BPVP_MODEL_PATH "${BPVP_MODELS_DIR}/nmp.tflite")
  file(DOWNLOAD "https://github.com/spotify/basic-pitch/raw/v0.4.0/basic_pitch/saved_models/icassp_2022/nmp.tflite" ${BPVP_MODEL_PATH} STATUS DOWNLOAD_STATUS)
  list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
  list(GET DOWNLOAD_STATUS 1 ERROR_MESSAGE)
  if(${STATUS_CODE} EQUAL 0)
    message(STATUS "Download nmp.tflite completed successfully!")
  else()
    message(FATAL_ERROR "Error occurred during download: ${ERROR_MESSAGE}")
  endif()
endif()
endif()

file(GLOB BPVP_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/source/bpvp.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source/bpvp.h
  ${CMAKE_CURRENT_SOURCE_DIR}/source/bpvp_convert.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source/bpvp_convert.h
)
source_group("sources" FILES ${BPVP_SOURCES})

if(NOT BPVP_MODELS_ONLY AND NOT BPVP_MANUAL_ONLY)
### Target ###
add_library(bpvp MODULE ${BPVP_SOURCES})
ive_prepare_plugin_target(bpvp)
target_link_libraries(bpvp PRIVATE tensorflow-lite)

if(BPVP_TESTER_ENABLE)
  enable_testing()
endif()

vpp_add_plugin(bpvp
  OUTPUT_NAME ircambasicpitch
  CATFILE ${CMAKE_CURRENT_SOURCE_DIR}/resource/ircambasicpitch.cat
  COPYRIGHT "Copyright 2024 Ircam. All rights reserved"
  VERSION ${PROJECT_VERSION_MAJOR}
  INSTALL ON
  DEBUG ${BPVP_ENABLE_DEBUG}
  DEBUG_DOCUMENT ${CMAKE_CURRENT_SOURCE_DIR}/test/basicpitch.ptldoc
  # The full test (input A) checks that the plugin loads (no dependency issues),
  # that the tflite model of the bundle is loaded and that the inference runs.
  TESTER ${BPVP_TESTER_ENABLE}
  TESTER_ARGUMENTS -a
  TESTER_TIMEOUT ${BPVP_TESTER_TIMEOUT}
)

add_custom_command(TARGET bpvp POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${BPVP_MODELS_DIR}" "$<TARGET_FILE_DIR:bpvp>/../Resources/models"
)

vpp_create_clang_format_targets(bpvp ${BPVP_SOURCES})
endif()

if(NOT BPVP_MODELS_ONLY)
### Manual ###
set(BPVP_MANUAL_DIR ${CMAKE_CURRENT_BINARY_DIR}/Manual)
file(MAKE_DIRECTORY ${BPVP_MANUAL_DIR})

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/docs/basicpitch-manual.md MANUAL_CONTENT)
string(REPLACE "APPVERSION" "${BPVP_BUILD_TAG} (${GIT_COMMIT_ID})" MANUAL_CONTENT ${MANUAL_CONTENT})
string(REPLACE "src=\"../resource/" "src=\"${CMAKE_CURRENT_SOURCE_DIR}/resource/" MANUAL_CONTENT ${MANUAL_CONTENT})
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/basicpitch-manual.md ${MANUAL_CONTENT})
find_program(MDPDF_EXE "mdpdf")
if(MDPDF_EXE)
  add_custom_target(bpvp_manual COMMAND ${MDPDF_EXE} ${CMAKE_CURRENT_BINARY_DIR}/basicpitch-manual.md ${BPVP_MANUAL_DIR}/basicpitch-manual.pdf VERBATIM)
else()
  message(STATUS "Manual target cannot be generated because mdpdf is not found")
endif()

### Publishing ###
set(PROJECT_URL "https://github.com/Ircam-Partiels/basic-pitch-vamp-plugin")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/basicpitch-release.md "## Downloads\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/basicpitch-release.md "- [Basic-Pitch-Linux.zip](${PROJECT_URL}/releases/download/${BPVP_BUILD_TAG}/Basic-Pitch-Linux.zip)\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/basicpitch-release.md "- [Basic-Pitch-MacOS.pkg](${PROJECT_URL}/releases/download/${BPVP_BUILD_TAG}/Basic-Pitch-MacOS.pkg)\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/basicpitch-release.md "- [Basic-Pitch-Windows.exe](${PROJECT_URL}/releases/download/${BPVP_BUILD_TAG}/Basic-Pitch-Windows.exe)\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/basicpitch-release.md "- [Basic-Pitch-Manual.zip](${PROJECT_URL}/releases/download/${BPVP_BUILD_TAG}/Basic-Pitch-Manual.zip)\n")
endif() 
