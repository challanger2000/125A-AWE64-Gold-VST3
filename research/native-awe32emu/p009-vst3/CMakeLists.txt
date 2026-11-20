cmake_minimum_required(VERSION 3.25)
project(P009_AWE32Emu_Native_VST3 VERSION 0.0.1 LANGUAGES C CXX)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "P009 experiment is Windows x64 only")
endif()
set(P009_SDK_ROOT "" CACHE PATH "Exact Steinberg VST3 SDK")
set(P009_UPSTREAM "" CACHE PATH "Pinned upstream AWE32Emu checkout")
if(NOT EXISTS "${P009_SDK_ROOT}/public.sdk/source/vst/vstsinglecomponenteffect.cpp")
  message(FATAL_ERROR "P009 missing Steinberg SDK")
endif()
if(NOT EXISTS "${P009_UPSTREAM}/AWE32Emu/src/Synth.cpp")
  message(FATAL_ERROR "P009 missing pinned native AWE32Emu sources")
endif()
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES ON CACHE BOOL "" FORCE)
# The SDK post-build validator output contains harmless EXEC:error diagnostics
# for unsupported sample rates even when ALL 47 tests pass. VS/MSBuild treats
# these as compile errors. Execute validator separately, capture and verify
# its true exit status and exact test summary.
set(SMTG_RUN_VST_VALIDATOR OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
add_subdirectory("${P009_SDK_ROOT}" "${CMAKE_BINARY_DIR}/vst3sdk")
set(NATIVE "${P009_UPSTREAM}/AWE32Emu/src")
smtg_add_vst3plugin(AWE64_Gold_Native_P009
    P009Native.hpp P009Native.cpp P009Factory.cpp
    "${NATIVE}/Synth.cpp"
    "${NATIVE}/SoundFont.cpp"
    "${NATIVE}/Emu8000.cpp"
    "${NATIVE}/Emu8000Box.cpp"
    "${NATIVE}/I18n.cpp"
    "${NATIVE}/86box/snd_emu8k.c"
    "${P009_SDK_ROOT}/public.sdk/source/vst/vstsinglecomponenteffect.cpp"
    "${P009_SDK_ROOT}/public.sdk/source/main/dllmain.cpp")
target_include_directories(AWE64_Gold_Native_P009 PRIVATE
   "${P009_SDK_ROOT}" "${NATIVE}" "${NATIVE}/86box/include")
target_compile_definitions(AWE64_Gold_Native_P009 PRIVATE
   AWE32EMU_WITH_86BOX _CRT_SECURE_NO_WARNINGS)
target_link_libraries(AWE64_Gold_Native_P009 PRIVATE sdk winmm)
target_compile_features(AWE64_Gold_Native_P009 PRIVATE cxx_std_17)
smtg_target_configure_version_file(AWE64_Gold_Native_P009)

# P009: standalone concurrency regression uses SAME original C EMU8000 and
# reconstructed Win95 driver from this plugin, but WITHOUT Steinberg host.
add_executable(P009NativeInstanceProbe
   tests/P009NativeInstanceProbe.cpp
   "${NATIVE}/Synth.cpp" "${NATIVE}/SoundFont.cpp"
   "${NATIVE}/Emu8000.cpp" "${NATIVE}/Emu8000Box.cpp"
   "${NATIVE}/I18n.cpp" "${NATIVE}/86box/snd_emu8k.c")
target_include_directories(P009NativeInstanceProbe PRIVATE
   "${NATIVE}" "${NATIVE}/86box/include")
target_compile_definitions(P009NativeInstanceProbe PRIVATE
   AWE32EMU_WITH_86BOX _CRT_SECURE_NO_WARNINGS)
target_link_libraries(P009NativeInstanceProbe PRIVATE winmm)
target_compile_features(P009NativeInstanceProbe PRIVATE cxx_std_17)

# P009RT: linked against the EXACT same chip/driver sources as the VST3.
# Private original assets are supplied only for local offline QA.
add_executable(P009RealtimeAllocationProbe
   tests/P009RealtimeAllocationProbe.cpp
   "${NATIVE}/Synth.cpp" "${NATIVE}/SoundFont.cpp"
   "${NATIVE}/Emu8000.cpp" "${NATIVE}/Emu8000Box.cpp"
   "${NATIVE}/I18n.cpp" "${NATIVE}/86box/snd_emu8k.c")
target_include_directories(P009RealtimeAllocationProbe PRIVATE
   "${NATIVE}" "${NATIVE}/86box/include")
target_compile_definitions(P009RealtimeAllocationProbe PRIVATE
   AWE32EMU_WITH_86BOX _CRT_SECURE_NO_WARNINGS)
target_link_libraries(P009RealtimeAllocationProbe PRIVATE winmm)
target_compile_features(P009RealtimeAllocationProbe PRIVATE cxx_std_17)

# P010: test ACTUAL compiled Windows VST3 using Steinberg's SDK hosting API.
add_executable(P010NativeHostQA
  tests/P010NativeHostQA.cpp
  "${P009_SDK_ROOT}/public.sdk/source/vst/hosting/module_win32.cpp"
  "${P009_SDK_ROOT}/public.sdk/source/vst/hosting/parameterchanges.cpp"
  "${P009_SDK_ROOT}/public.sdk/source/common/memorystream.cpp")
target_include_directories(P010NativeHostQA PRIVATE "${P009_SDK_ROOT}")
target_link_libraries(P010NativeHostQA PRIVATE sdk_hosting)
target_compile_features(P010NativeHostQA PRIVATE cxx_std_17)
 
# P016: PUBLIC two-layer SF2 -> exact same pinned Creative Win95 driver and
# 86Box-derived EMU8000 chip as the VST3, with shared production reset code.
# Only synthetic ROM is used. Not original-ROM fidelity evidence.
add_executable(P016PublicRecallPCM
   tests/P016PublicRecallPCM.cpp
   "${NATIVE}/Synth.cpp" "${NATIVE}/SoundFont.cpp"
   "${NATIVE}/Emu8000.cpp" "${NATIVE}/Emu8000Box.cpp"
   "${NATIVE}/I18n.cpp" "${NATIVE}/86box/snd_emu8k.c")
target_include_directories(P016PublicRecallPCM PRIVATE
   "${CMAKE_CURRENT_SOURCE_DIR}" "${NATIVE}" "${NATIVE}/86box/include")
target_compile_definitions(P016PublicRecallPCM PRIVATE
   AWE32EMU_WITH_86BOX _CRT_SECURE_NO_WARNINGS)
target_link_libraries(P016PublicRecallPCM PRIVATE winmm)
target_compile_features(P016PublicRecallPCM PRIVATE cxx_std_17)
