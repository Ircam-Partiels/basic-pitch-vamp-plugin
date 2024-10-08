cmake_minimum_required(VERSION 3.18)

set(IGNORE_VAMP_PLUGIN_TESTER OFF CACHE STRING "Disables the tests with vamp plugin tester")
set(PARTIELS_EXE_HINT_PATH "/Applications" CACHE PATH "")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_XCODE_GENERATE_SCHEME ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3" CACHE STRING "Minimum OS X deployment version")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "Default value for MSVC_RUNTIME_LIBRARY of targets" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE CACHE BOOL "Default value for POSITION_INDEPENDENT_CODE of targets" FORCE)

### Include Vamp (IVE) ###
set(IVE_BUILD_HOST_LIBRARY OFF)
set(IVE_BUILD_PLUGIN_LIBRARY ON)
add_subdirectory(ircam-vamp-extension EXCLUDE_FROM_ALL)

### Include TensorFlow ###
set(SYSTEM_FARMHASH OFF)
set(SYSTEM_PTHREADPOOL OFF)
set(TFLITE_ENABLE_XNNPACK ON)
set(TFLITE_ENABLE_RUY ON)
set(BUILD_SHARED_LIBS OFF)
set(ABSL_PROPAGATE_CXX_STD ON)

set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS_BACKUP "$CACHE{CMAKE_SUPPRESS_DEVELOPER_WARNINGS}")
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON CACHE INTERNAL "" FORCE)
add_subdirectory(tensorflow/tensorflow/lite EXCLUDE_FROM_ALL)
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ${CMAKE_SUPPRESS_DEVELOPER_WARNINGS_BACKUP} CACHE INTERNAL "" FORCE)
set_target_properties(tensorflow-lite PROPERTIES CXX_STANDARD 20)
set_target_properties(XNNPACK PROPERTIES CXX_STANDARD 20)

if(APPLE)
  target_compile_options(pthreadpool PRIVATE -Wno-shorten-64-to-32)
  target_compile_options(eight_bit_int_gemm PRIVATE -Wno-shorten-64-to-32)
  target_compile_options(farmhash PRIVATE -Wno-shorten-64-to-32)
  target_compile_options(microkernels-prod PRIVATE -Wno-shorten-64-to-32)
  target_compile_options(jit PRIVATE -Wno-shorten-64-to-32)
  target_compile_options(operators PRIVATE -Wno-shorten-64-to-32)
  target_compile_options(operator-run PRIVATE -Wno-shorten-64-to-32)
  target_compile_options(operator-utils PRIVATE -Wno-shorten-64-to-32)
  target_compile_options(subgraph PRIVATE -Wno-shorten-64-to-32)
  target_compile_options(XNNPACK PRIVATE -Wno-shorten-64-to-32)
  target_compile_options(cpuinfo PRIVATE -Wno-shorten-64-to-32)
  target_compile_options(ruy_prepacked_cache PRIVATE -Wno-shorten-64-to-32)
  target_compile_options(ruy_thread_pool PRIVATE -Wno-shorten-64-to-32)
  target_compile_options(ruy_ctx PRIVATE -Wno-shorten-64-to-32)
  target_compile_options(ruy_context PRIVATE -Wno-shorten-64-to-32)
  target_compile_options(ruy_context_get_ctx PRIVATE -Wno-shorten-64-to-32)
  target_compile_options(ruy_trmul PRIVATE -Wno-shorten-64-to-32)
  target_compile_options(ruy_prepare_packed_matrices PRIVATE -Wno-shorten-64-to-32)
  target_compile_options(ruy_pack_arm PRIVATE -Wno-shorten-64-to-32)
  target_compile_options(ruy_frontend PRIVATE -Wno-shorten-64-to-32)
  target_compile_options(indirection PRIVATE -Wno-shorten-64-to-32)
  target_compile_options(tensorflow-lite PRIVATE -Wno-shorten-64-to-32)
endif()

### Project ###
project(BasicPitchVampPlugin VERSION 1.0.0 LANGUAGES C CXX)

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

### Source ###
set(BPVP_MODEL_CPP ${CMAKE_CURRENT_BINARY_DIR}/source/bpvp_model.cpp)
set(BPVP_MODEL_H ${CMAKE_CURRENT_SOURCE_DIR}/source/bpvp_model.h)
if(NOT EXISTS ${BPVP_MODEL_CPP})
  set(BPVP_MODEL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/basic-pitch/basic_pitch/saved_models/icassp_2022/nmp.tflite")
  file(RELATIVE_PATH  BPVP_MODEL_HREL "${CMAKE_CURRENT_BINARY_DIR}/source" ${BPVP_MODEL_H})
  file(WRITE ${BPVP_MODEL_CPP} "#include \"${BPVP_MODEL_HREL}\"\n\n")

  if(EXISTS ${BPVP_MODEL_PATH})
    message(STATUS "Generating default model ${BPVP_MODEL_PATH}")
    file(READ ${BPVP_MODEL_PATH} BPVP_HEX_DATA HEX)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," BPVP_HEX_DATA ${BPVP_HEX_DATA})
    file(APPEND ${BPVP_MODEL_CPP} "namespace Bpvp\n{\n")
    file(APPEND ${BPVP_MODEL_CPP} "static const unsigned char binary_data[] =\n{\n")
    file(APPEND ${BPVP_MODEL_CPP} "${BPVP_HEX_DATA}")
    file(APPEND ${BPVP_MODEL_CPP} "};\n}\n\n")
    file(APPEND ${BPVP_MODEL_CPP} "const void* Bpvp::model = (const void*)Bpvp::binary_data;\n")
    file(APPEND ${BPVP_MODEL_CPP} "const size_t Bpvp::model_size = sizeof(Bpvp::binary_data);\n")
  else()
    message(WARNING "Default model ${BPVP_MODEL_PATH} invalid")
    file(APPEND ${BPVP_MODEL_CPP} "const void* Bpvp::model = nullptr;\n")
    file(APPEND ${BPVP_MODEL_CPP} "const size_t Bpvp::model_size = 0;\n")
  endif()
    file(APPEND ${BPVP_MODEL_CPP} "\n")
endif()

file(GLOB BPVP_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/source/bpvp.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source/bpvp.h
  ${CMAKE_CURRENT_SOURCE_DIR}/source/bpvp_convert.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source/bpvp_convert.h
  ${BPVP_MODEL_H}
)
source_group("sources" FILES ${BPVP_SOURCES})

### Target ###
add_library(bpvp SHARED ${BPVP_SOURCES} ${BPVP_MODEL_CPP})
ive_prepare_plugin_target(bpvp)
target_compile_definitions(bpvp PRIVATE BPVP_PLUGIN_VERSION=${PROJECT_VERSION_MAJOR})
target_link_libraries(bpvp PRIVATE tensorflow-lite)

add_custom_command(TARGET bpvp POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/resource/ircambasicpitch.cat "$<IF:$<CONFIG:Debug>,${CMAKE_CURRENT_BINARY_DIR}/Debug/ircambasicpitch.cat,${CMAKE_CURRENT_BINARY_DIR}/Release/ircambasicpitch.cat>")
set_target_properties(bpvp PROPERTIES LIBRARY_OUTPUT_NAME ircambasicpitch)
vpp_add_plugin(bpvp)

find_program(PARTIELS_EXE "Partiels" HINTS ${PARTIELS_EXE_HINT_PATH} NO_CACHE)
if(PARTIELS_EXE)
  if(NOT IS_DIRECTORY ${PARTIELS_EXE}) 
      cmake_path(GET PARTIELS_EXE PARENT_PATH PARTIELS_EXE_PARENT)
      cmake_path(SET PARTIELS_EXE NORMALIZE "${PARTIELS_EXE_PARENT}/../../")
  endif()
  message(STATUS "Debugging: " ${PARTIELS_EXE})
  set_target_properties(bpvp PROPERTIES XCODE_SCHEME_EXECUTABLE ${PARTIELS_EXE})
  set_target_properties(bpvp PROPERTIES XCODE_SCHEME_ARGUMENTS "${CMAKE_CURRENT_SOURCE_DIR}/test/basicpitch.ptldoc")
  set_target_properties(bpvp PROPERTIES XCODE_SCHEME_ENVIRONMENT "VAMP_PATH=${CMAKE_CURRENT_BINARY_DIR}/Debug")
endif()

### Format ###
find_program(CLANG_FORMAT_EXE "clang-format" HINTS "C:/Program Files/LLVM/bin")
if(CLANG_FORMAT_EXE)
    add_custom_target(bpvp_check_format ${CLANG_FORMAT_EXE} --Werror --dry-run --verbose -style=file ${BPVP_SOURCES})
    add_custom_target(bpvp_apply_format ${CLANG_FORMAT_EXE} -i -style=file ${BPVP_SOURCES})
else()
    message(STATUS "Clang Format targets cannot be generated because clang-format is not found")
endif()

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
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/basicpitch-release.md "- [Basic-Pitch-Linux.tar.gz](${PROJECT_URL}/releases/download/${BPVP_BUILD_TAG}/Basic-Pitch-Linux.tar.gz)\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/basicpitch-release.md "- [Basic-Pitch-MacOS.pkg](${PROJECT_URL}/releases/download/${BPVP_BUILD_TAG}/Basic-Pitch-MacOS.pkg)\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/basicpitch-release.md "- [Basic-Pitch-Windows.exe](${PROJECT_URL}/releases/download/${BPVP_BUILD_TAG}/Basic-Pitch-Windows.exe)\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/basicpitch-release.md "- [Basic-Pitch-Manual.zip](${PROJECT_URL}/releases/download/${BPVP_BUILD_TAG}/Basic-Pitch-Manual.zip)\n") 

### Install ###
if(APPLE)
  install(TARGETS bpvp RUNTIME LIBRARY DESTINATION "~/Library/Audio/Plug-Ins/Vamp/")
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resource/ircambasicpitch.cat DESTINATION "~/Library/Audio/Plug-Ins/Vamp/")
elseif(UNIX)
  install(TARGETS bpvp RUNTIME LIBRARY DESTINATION "~/vamp/")
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resource/ircambasicpitch.cat DESTINATION "~/vamp/")
elseif(WIN32)
  install(TARGETS bpvp RUNTIME DESTINATION "$ENV{PROGRAMFILES}/Vamp Plugins/" PERMISSIONS OWNER_WRITE)
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resource/ircambasicpitch.cat DESTINATION "$ENV{PROGRAMFILES}/Vamp Plugins/")
endif()

### Testing ###
if(NOT IGNORE_VAMP_PLUGIN_TESTER)
  enable_testing()
  if(APPLE)
    if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/vamp-plugin-tester/vamp-plugin-tester)
      file(DOWNLOAD "https://github.com/pierreguillot/vamp-plugin-tester/releases/download/1.1/vamp-plugin-tester-1.1-osx-arm.zip" "${CMAKE_CURRENT_BINARY_DIR}/vamp-plugin-tester.tar.gz")
      file(ARCHIVE_EXTRACT INPUT "${CMAKE_CURRENT_BINARY_DIR}/vamp-plugin-tester.tar.gz" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    endif()
  elseif(UNIX)
    if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/vamp-plugin-tester/vamp-plugin-tester)
      file(DOWNLOAD "https://github.com/pierreguillot/vamp-plugin-tester/releases/download/1.1/vamp-plugin-tester-1.1-linux64.tar.gz" "${CMAKE_CURRENT_BINARY_DIR}/vamp-plugin-tester.tar.gz")
      file(ARCHIVE_EXTRACT INPUT "${CMAKE_CURRENT_BINARY_DIR}/vamp-plugin-tester.tar.gz" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    endif()
  elseif(WIN32)
    if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/vamp-plugin-tester/vamp-plugin-tester.exe)
      file(DOWNLOAD "https://github.com/pierreguillot/vamp-plugin-tester/releases/download/1.1/vamp-plugin-tester-1.1a-win64.zip" "${CMAKE_CURRENT_BINARY_DIR}/vamp-plugin-tester.zip")
      file(ARCHIVE_EXTRACT INPUT "${CMAKE_CURRENT_BINARY_DIR}/vamp-plugin-tester.zip" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    endif()
  endif()

  add_test(NAME VampPluginTester COMMAND ${CMAKE_CURRENT_BINARY_DIR}/vamp-plugin-tester/vamp-plugin-tester -a)
  set_tests_properties(VampPluginTester PROPERTIES ENVIRONMENT "$<IF:$<CONFIG:Debug>,VAMP_PATH=${CMAKE_CURRENT_BINARY_DIR}/Debug,VAMP_PATH=${CMAKE_CURRENT_BINARY_DIR}/Release>")
endif()
