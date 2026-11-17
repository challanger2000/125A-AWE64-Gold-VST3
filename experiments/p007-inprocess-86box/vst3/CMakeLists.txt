cmake_minimum_required(VERSION 3.25)
project(125A_AWE64_Gold_P007 VERSION 0.0.2 LANGUAGES C CXX)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "P007 native VST3 is Windows x64 only")
endif()
set(P007_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../external/vst3sdk"
    CACHE PATH "Steinberg VST3 SDK 3.7.12")
if(NOT EXISTS "${P007_SDK_ROOT}/public.sdk/source/vst/vstsinglecomponenteffect.cpp")
  message(FATAL_ERROR "Steinberg VST3 SDK missing")
endif()
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
add_subdirectory("${P007_SDK_ROOT}" "${CMAKE_BINARY_DIR}/vst3sdk")
smtg_add_vst3plugin(AWE64_Gold_P007
  P007Native.hpp P007Native.cpp P007Factory.cpp
  "${P007_SDK_ROOT}/public.sdk/source/vst/vstsinglecomponenteffect.cpp"
  "${P007_SDK_ROOT}/public.sdk/source/main/dllmain.cpp")
target_include_directories(AWE64_Gold_P007 PRIVATE "${P007_SDK_ROOT}")
target_link_libraries(AWE64_Gold_P007 PRIVATE sdk)
target_compile_features(AWE64_Gold_P007 PRIVATE cxx_std_23)
smtg_target_configure_version_file(AWE64_Gold_P007)
