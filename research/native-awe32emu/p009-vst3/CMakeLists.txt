cmake_minimum_required(VERSION 3.25)
project(P009_AWE32Emu_Native_VST3 LANGUAGES C CXX)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "P009 experiment is Windows x64 only")
endif()
set(P009_SDK_ROOT "" CACHE PATH "Exact Steinberg VST3 SDK")
set(P009_UPSTREAM "" CACHE PATH "Pinned upstream AWE32Emu checkout")
if(NOT EXISTS "\${P009_SDK_ROOT}/public.sdk/source/vst/vstsinglecomponenteffect.cpp")
  message(FATAL_ERROR "P009 missing Steinberg SDK")
endif()
if(NOT EXISTS "\${P009_UPSTREAM}/AWE32Emu/src/Synth.cpp")
  message(FATAL_ERROR "P009 missing pinned native AWE32Emu sources")
endif()
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES ON CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
add_subdirectory("\${P009_SDK_ROOT}" "\${CMAKE_BINARY_DIR}/vst3sdk")
set(NATIVE "\${P009_UPSTREAM}/AWE32Emu/src")
smtg_add_vst3plugin(AWE64_Gold_Native_P009
    P009Native.hpp P009Native.cpp P009Factory.cpp
    "\${NATIVE}/Synth.cpp"
    "\${NATIVE}/SoundFont.cpp"
    "\${NATIVE}/Emu8000.cpp"
    "\${NATIVE}/Emu8000Box.cpp"
    "\${NATIVE}/I18n.cpp"
    "\${NATIVE}/86box/snd_emu8k.c"
    "\${P009_SDK_ROOT}/public.sdk/source/vst/vstsinglecomponenteffect.cpp"
    "\${P009_SDK_ROOT}/public.sdk/source/main/dllmain.cpp")
target_include_directories(AWE64_Gold_Native_P009 PRIVATE
   "\${P009_SDK_ROOT}" "\${NATIVE}" "\${NATIVE}/86box/include")
target_compile_definitions(AWE64_Gold_Native_P009 PRIVATE
   AWE32EMU_WITH_86BOX _CRT_SECURE_NO_WARNINGS)
target_link_libraries(AWE64_Gold_Native_P009 PRIVATE sdk winmm)
target_compile_features(AWE64_Gold_Native_P009 PRIVATE cxx_std_17)
smtg_target_configure_version_file(AWE64_Gold_Native_P009)
