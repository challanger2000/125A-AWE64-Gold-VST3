# P006 Windows only. Creative driver and 86Box live in private guest.
if(NOT WIN32)
  message(FATAL_ERROR "P006 VST3 targets Windows x64 only")
endif()
set(P006_SDK_ROOT "${CMAKE_SOURCE_DIR}/external/vst3sdk" CACHE PATH "Steinberg SDK 3.7.12")
if(NOT EXISTS "${P006_SDK_ROOT}/public.sdk/source/vst/vstsinglecomponenteffect.cpp")
  message(FATAL_ERROR "Steinberg VST3 SDK is missing")
endif()
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
add_subdirectory("${P006_SDK_ROOT}" "${CMAKE_BINARY_DIR}/vst3sdk")
smtg_add_vst3plugin(AWE64_Gold_P006
  P006Bridge.hpp P006Bridge.cpp P006WindowsBridge.hpp P006Factory.cpp
  "${P006_SDK_ROOT}/public.sdk/source/vst/vstsinglecomponenteffect.cpp"
  "${P006_SDK_ROOT}/public.sdk/source/main/dllmain.cpp")
target_include_directories(AWE64_Gold_P006 PRIVATE
  "${P006_SDK_ROOT}" "${CMAKE_CURRENT_SOURCE_DIR}/../bridge")
target_link_libraries(AWE64_Gold_P006 PRIVATE sdk)
target_compile_features(AWE64_Gold_P006 PRIVATE cxx_std_23)
smtg_target_configure_version_file(AWE64_Gold_P006)
