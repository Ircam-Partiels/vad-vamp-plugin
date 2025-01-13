cmake_minimum_required(VERSION 3.18)
cmake_policy(SET CMP0091 NEW)

option(IGNORE_VAMP_PLUGIN_TESTER "Disables the tests with vamp plugin tester" OFF)
option(IGNORE_PARTIELS_DEBUG "Disables the debug scheme using the Partiels application" OFF)
set(PARTIELS_EXE_HINT_PATH "/Applications" CACHE PATH "")

set(CMAKE_XCODE_GENERATE_SCHEME ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3" CACHE STRING "Minimum OS X deployment version")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "Default value for MSVC_RUNTIME_LIBRARY of targets" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE CACHE BOOL "Default value for POSITION_INDEPENDENT_CODE of targets" FORCE)

### Include Vamp (IVE) ###
set(IVE_BUILD_HOST_LIBRARY OFF)
set(IVE_BUILD_PLUGIN_LIBRARY ON)
add_subdirectory(ircam-vamp-extension EXCLUDE_FROM_ALL)

### Project ###
project(VadVampPlugin VERSION 1.0.0 LANGUAGES C CXX)

### Version ###
execute_process(COMMAND git log -1 --format=%h WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR} OUTPUT_VARIABLE GIT_COMMIT_ID OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND git status --porcelain WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR} OUTPUT_VARIABLE GIT_HAS_DIFF OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT GIT_HAS_DIFF STREQUAL "")
  set(GIT_COMMIT_ID "${GIT_COMMIT_ID}-head")
endif()

if(NOT VAD_VAMP_PLUGIN_BUILD_TAG OR VAD_VAMP_PLUGIN_BUILD_TAG STREQUAL "")
  set(VAD_VAMP_PLUGIN_BUILD_TAG ${CMAKE_PROJECT_VERSION})
endif()
message(STATUS "Vad Vamp Plugin: " ${VAD_VAMP_PLUGIN_BUILD_TAG} " (" ${GIT_COMMIT_ID} ")")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/version.txt ${VAD_VAMP_PLUGIN_BUILD_TAG})

### Package ###
set(VPP_NAME "Vad" CACHE PATH "The name of the project" FORCE)
set(VPP_COMPANY "Ircam" CACHE PATH "The name of the company" FORCE)
set(VPP_URL "www.ircam.fr" CACHE PATH "The URL of the project")
set(VPP_APPID "f19f1fac-b588-4615-80cf-356ec2b917c1" CACHE PATH "The unique ID of the project")
set(VPP_BUILD_TAG "${VAD_VAMP_PLUGIN_BUILD_TAG}" CACHE STRING "The current tag of the project" FORCE)
set(VPP_ABOUT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resource/About.txt" CACHE PATH "The about file to add to the distribution" FORCE)
set(VPP_CHANGELOG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resource/ChangeLog.txt" CACHE PATH "The changelog file to add to the distribution" FORCE)
set(VPP_ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resource/Ircam-logo-noir-RS.png" CACHE PATH "The icon file to add to the distribution" FORCE)
set(VPP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/package CACHE PATH "The directory of the package" FORCE)
include(vamp-plugin-packager/vamp-plugin-packager.cmake)

### VAD ###
add_subdirectory(libfvad EXCLUDE_FROM_ALL)

### Source ###
file(GLOB VAD_VAMP_PLUGIN_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/source/vad.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source/vad.h
)
source_group("sources" FILES ${VAD_VAMP_PLUGIN_SOURCES})

### Target ###
add_library(vad_vamp_plugin SHARED ${VAD_VAMP_PLUGIN_SOURCES})
ive_prepare_plugin_target(vad_vamp_plugin)
target_link_libraries(vad_vamp_plugin PRIVATE fvad)
target_compile_definitions(vad_vamp_plugin PRIVATE VAD_VAMP_PLUGIN_PLUGIN_VERSION=${PROJECT_VERSION_MAJOR})
set_target_properties(vad_vamp_plugin PROPERTIES LIBRARY_OUTPUT_NAME ircamvad)

vpp_add_plugin(vad_vamp_plugin ${CMAKE_CURRENT_SOURCE_DIR}/resource/ircamvad.cat)
if(NOT IGNORE_PARTIELS_DEBUG)
  vpp_set_plugin_debug(vad_vamp_plugin "${CMAKE_CURRENT_SOURCE_DIR}/test/vad.ptldoc")
endif()
vpp_set_plugin_install(vad_vamp_plugin)
vpp_create_clang_format_targets(vad_vamp_plugin ${VAD_VAMP_PLUGIN_SOURCES})
vpp_create_manual_target(vad_vamp_plugin ${CMAKE_CURRENT_SOURCE_DIR}/docs/vad-manual.md "${VAD_VAMP_PLUGIN_BUILD_TAG} (${GIT_COMMIT_ID})")

### Publishing ###
set(PROJECT_URL "https://github.com/Ircam-Partiels/vad_vamp_plugin")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/vad-release.md "## Downloads\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/vad-release.md "- [Vad-Linux.tar.gz](${PROJECT_URL}/releases/download/${VAD_VAMP_PLUGIN_BUILD_TAG}/Vad-Linux.tar.gz)\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/vad-release.md "- [Vad-MacOS.pkg](${PROJECT_URL}/releases/download/${VAD_VAMP_PLUGIN_BUILD_TAG}/Vad-MacOS.pkg)\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/vad-release.md "- [Vad-Windows.exe](${PROJECT_URL}/releases/download/${VAD_VAMP_PLUGIN_BUILD_TAG}/Vad-Windows.exe)\n")
file(APPEND ${CMAKE_CURRENT_BINARY_DIR}/vad-release.md "- [Vad-Manual.zip](${PROJECT_URL}/releases/download/${VAD_VAMP_PLUGIN_BUILD_TAG}/Vad-Manual.zip)\n") 

### Testing ###
if(NOT IGNORE_VAMP_PLUGIN_TESTER)
  enable_testing()
  vpp_enable_vamp_plugin_tester()
endif()
