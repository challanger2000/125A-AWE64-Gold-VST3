cmake_minimum_required(VERSION 3.25)
project(P009_AWE32Emu_Native_VST3 VERSION 0.9.0 LANGUAGES C CXX)
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
set(SMTG_ENABLE_VSTGUI_SUPPORT ON CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
add_subdirectory("${P009_SDK_ROOT}" "${CMAKE_BINARY_DIR}/vst3sdk")
set(NATIVE "${P009_UPSTREAM}/AWE32Emu/src")
smtg_add_vst3plugin(AWE64_Gold_Native_P009
    P009Native.hpp P009Native.cpp P009NativeGui.cpp P009AssetFingerprint.hpp P009CreativeFx.hpp P009Factory.cpp
    p165/PresetBrowserView.h p165/PresetBrowserView.cpp
    p165/PresetBrowserModel.h p165/PresetCatalog.h p165/IbkBank.h
    "${NATIVE}/Synth.cpp"
    "${NATIVE}/SoundFont.cpp"
    "${NATIVE}/Emu8000.cpp"
    "${NATIVE}/Emu8000Box.cpp"
    "${NATIVE}/I18n.cpp"
    "${NATIVE}/86box/snd_emu8k.c"
    "${P009_SDK_ROOT}/public.sdk/source/vst/vstsinglecomponenteffect.cpp"
    "${P009_SDK_ROOT}/public.sdk/source/main/dllmain.cpp")
# The last complete original Win95 P007BW owner GUI (not stale P007Z).
# Preserve the P168 LCD x=370 inset and 73% knob-island shading.
# Faceplate is PUBLIC user artwork, never the private Creative ROM.
set(P009_P165_FACEPLATE "${CMAKE_CURRENT_SOURCE_DIR}/resource/125A_P165_Original_Faceplate_1000x708.png")
if(NOT EXISTS "${P009_P165_FACEPLATE}")
  file(DOWNLOAD
    "https://raw.githubusercontent.com/challanger2000/125A-AWE-Synthesizer/8e8315c2ecab9f4a8bdbd415c1b7af5a8485a3c1/src/vst3/resource/125A_P165_Original_Faceplate_1000x708.png"
    "${P009_P165_FACEPLATE}" STATUS P009_P165_DL TLS_VERIFY ON TIMEOUT 90)
  list(GET P009_P165_DL 0 P009_P165_DL_STATUS)
  if(NOT P009_P165_DL_STATUS EQUAL 0)
    message(FATAL_ERROR "Original approved P165 GUI asset missing: ${P009_P165_DL}")
  endif()
endif()
execute_process(COMMAND git hash-object "${P009_P165_FACEPLATE}"
  OUTPUT_VARIABLE P009_P165_SHA OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE P009_P165_RC)
if(NOT P009_P165_RC EQUAL 0 OR
   NOT P009_P165_SHA STREQUAL "7e57ef8e0c90d84fa4c951bea7815b0233762ca7")
  message(FATAL_ERROR "Owner P165 faceplate Git blob SHA mismatch")
endif()
smtg_target_add_plugin_resources(AWE64_Gold_Native_P009 RESOURCES
  resource/p007_p165.uidesc
  resource/125A_P165_Original_Faceplate_1000x708.png
  resource/125A_Button_Atlas_Hardware_Normal_Pressed.png
  resource/125A_Knob_Standard_33frames_58px.png
  resource/125A_Knob_Master_33frames_134px.png)
# Hardware sprites use exact Windows PNG resource names.
if(WIN32)
  set(P009_P165_ASSET_RC "${CMAKE_CURRENT_BINARY_DIR}/p009_p165_assets.rc")
  file(WRITE "${P009_P165_ASSET_RC}" "")
  foreach(P009_ASSET IN ITEMS
    "125A_P165_Original_Faceplate_1000x708.png"
    "125A_Button_Atlas_Hardware_Normal_Pressed.png"
    "125A_Knob_Standard_33frames_58px.png"
    "125A_Knob_Master_33frames_134px.png")
    file(APPEND "${P009_P165_ASSET_RC}"
      "\"${P009_ASSET}\" PNG DISCARDABLE \"${CMAKE_CURRENT_SOURCE_DIR}/resource/${P009_ASSET}\"\n")
  endforeach()
  target_sources(AWE64_Gold_Native_P009 PRIVATE "${P009_P165_ASSET_RC}")
endif()
target_link_libraries(AWE64_Gold_Native_P009 PRIVATE sdk vstgui_support comdlg32 bcrypt)
target_include_directories(AWE64_Gold_Native_P009 PRIVATE
   "${P009_SDK_ROOT}" "${NATIVE}" "${NATIVE}/86box/include")
# P020: only the native research VST3 adopts the 64-frame chip window.
# The pinned 86Box C core, driver logic, and all P019 980/128/64 probes
# remain unchanged. Fail CLOSED if CI forgot the reviewed P019 header patch.
file(READ "${NATIVE}/Emu8000Box.h" P020_UPSTREAM_BOX_HEADER)
if(NOT P020_UPSTREAM_BOX_HEADER MATCHES "P019_EMU_BLOCK_FRAMES")
  message(FATAL_ERROR
    "P020 requires exact pinned P019 render-window header patch before build")
endif()
unset(P020_UPSTREAM_BOX_HEADER)
target_compile_definitions(AWE64_Gold_Native_P009 PRIVATE
   AWE32EMU_WITH_86BOX _CRT_SECURE_NO_WARNINGS
   P019_EMU_BLOCK_FRAMES=64)
target_link_libraries(AWE64_Gold_Native_P009 PRIVATE sdk winmm)
target_compile_features(AWE64_Gold_Native_P009 PRIVATE cxx_std_23)
smtg_target_configure_version_file(AWE64_Gold_Native_P009)
# Generated SDK moduleinfo in this toolchain contains trailing commas,
# which strict JSON DAWs reject although the validator accepts them.
# Keep the exact Steinberg FUID and factory metadata, with deterministic
# valid JSON and plugin/factory version consistency. This is packaging only.
add_custom_command(TARGET AWE64_Gold_Native_P009 POST_BUILD
   COMMAND "${CMAKE_COMMAND}" -E copy_if_different
       "${CMAKE_CURRENT_SOURCE_DIR}/resource/moduleinfo.strict.json"
       "$<TARGET_FILE_DIR:AWE64_Gold_Native_P009>/../Resources/moduleinfo.json"
   COMMENT "Install strict VST3 moduleinfo.json (preserve FUID)")

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
   AWE32EMU_WITH_86BOX _CRT_SECURE_NO_WARNINGS
   P019_EMU_BLOCK_FRAMES=64)
target_link_libraries(P009NativeInstanceProbe PRIVATE winmm)
target_compile_features(P009NativeInstanceProbe PRIVATE cxx_std_17)

# P200: same production Synth/EMU8000 native source as the VST3.
# The test renders original 86Box chip PCM and measures 44.1k old/new CPU.
# Legacy macro restores only the pre-fix unused second DSP.
add_executable(P200StereoTriggerTrace
   tests/P200StereoTriggerTrace.cpp
   "${NATIVE}/Synth.cpp" "${NATIVE}/SoundFont.cpp"
   "${NATIVE}/Emu8000.cpp" "${NATIVE}/Emu8000Box.cpp"
   "${NATIVE}/I18n.cpp" "${NATIVE}/86box/snd_emu8k.c")
target_include_directories(P200StereoTriggerTrace PRIVATE
   "${NATIVE}" "${NATIVE}/86box/include")
target_compile_definitions(P200StereoTriggerTrace PRIVATE
   AWE32EMU_WITH_86BOX _CRT_SECURE_NO_WARNINGS
   P019_EMU_BLOCK_FRAMES=64)
target_link_libraries(P200StereoTriggerTrace PRIVATE winmm)
target_compile_features(P200StereoTriggerTrace PRIVATE cxx_std_17)

# P200: same production Synth/EMU8000 native source as the VST3.
# The test renders original 86Box chip PCM and measures 44.1k old/new CPU.
# Legacy macro restores only the pre-fix unused second DSP.
add_executable(P203VoiceStateNative
   tests/P203VxdVoiceStateProbe.cpp
   "${NATIVE}/Synth.cpp" "${NATIVE}/SoundFont.cpp"
   "${NATIVE}/Emu8000.cpp" "${NATIVE}/Emu8000Box.cpp"
   "${NATIVE}/I18n.cpp" "${NATIVE}/86box/snd_emu8k.c")
target_include_directories(P203VoiceStateNative PRIVATE
   "${NATIVE}" "${NATIVE}/86box/include")
target_compile_definitions(P203VoiceStateNative PRIVATE
   AWE32EMU_WITH_86BOX _CRT_SECURE_NO_WARNINGS
   P019_EMU_BLOCK_FRAMES=64)
target_link_libraries(P203VoiceStateNative PRIVATE winmm)
target_compile_features(P203VoiceStateNative PRIVATE cxx_std_17)

# P200: same production Synth/EMU8000 native source as the VST3.
# The test renders original 86Box chip PCM and measures 44.1k old/new CPU.
# Legacy macro restores only the pre-fix unused second DSP.
add_executable(P203VoiceStateNegative
   tests/P203VxdVoiceStateProbe.cpp
   "${NATIVE}/Synth.cpp" "${NATIVE}/SoundFont.cpp"
   "${NATIVE}/Emu8000.cpp" "${NATIVE}/Emu8000Box.cpp"
   "${NATIVE}/I18n.cpp" "${NATIVE}/86box/snd_emu8k.c")
target_include_directories(P203VoiceStateNegative PRIVATE
   "${NATIVE}" "${NATIVE}/86box/include")
target_compile_definitions(P203VoiceStateNegative PRIVATE
   AWE32EMU_WITH_86BOX _CRT_SECURE_NO_WARNINGS
   P019_EMU_BLOCK_FRAMES=64)
target_link_libraries(P203VoiceStateNegative PRIVATE winmm)
target_compile_definitions(P203VoiceStateNegative PRIVATE P203_NEGATIVE_BAD_PROTECTION)
target_compile_features(P203VoiceStateNegative PRIVATE cxx_std_17)

# P204: compiled actual original 86Box EMU8000 with original Win95 SF2.
add_executable(P204ReservationNative
   tests/P204OriginalReservationPortTrace.cpp
   "${NATIVE}/Synth.cpp" "${NATIVE}/SoundFont.cpp"
   "${NATIVE}/Emu8000.cpp" "${NATIVE}/Emu8000Box.cpp"
   "${NATIVE}/I18n.cpp" "${NATIVE}/86box/snd_emu8k.c")
target_include_directories(P204ReservationNative PRIVATE
   "${NATIVE}" "${NATIVE}/86box/include")
target_compile_definitions(P204ReservationNative PRIVATE
   AWE32EMU_WITH_86BOX _CRT_SECURE_NO_WARNINGS P019_EMU_BLOCK_FRAMES=64)
target_link_libraries(P204ReservationNative PRIVATE winmm)
target_compile_features(P204ReservationNative PRIVATE cxx_std_17)

# P204: compiled actual original 86Box EMU8000 with original Win95 SF2.
add_executable(P204ReservationNegative
   tests/P204OriginalReservationPortTrace.cpp
   "${NATIVE}/Synth.cpp" "${NATIVE}/SoundFont.cpp"
   "${NATIVE}/Emu8000.cpp" "${NATIVE}/Emu8000Box.cpp"
   "${NATIVE}/I18n.cpp" "${NATIVE}/86box/snd_emu8k.c")
target_include_directories(P204ReservationNegative PRIVATE
   "${NATIVE}" "${NATIVE}/86box/include")
target_compile_definitions(P204ReservationNegative PRIVATE
   AWE32EMU_WITH_86BOX _CRT_SECURE_NO_WARNINGS P019_EMU_BLOCK_FRAMES=64
   P204_LEGACY_INTERLEAVED_ALLOCATOR)
target_link_libraries(P204ReservationNegative PRIVATE winmm)
target_compile_features(P204ReservationNegative PRIVATE cxx_std_17)


# P205: original-chip allocation VTFT reset witness with causal mutant.
foreach(P205_KIND IN ITEMS Native Negative)
  add_executable(P205AllocatorReset${P205_KIND}
    tests/P204OriginalReservationPortTrace.cpp
    "${NATIVE}/Synth.cpp" "${NATIVE}/SoundFont.cpp"
    "${NATIVE}/Emu8000.cpp" "${NATIVE}/Emu8000Box.cpp"
    "${NATIVE}/I18n.cpp" "${NATIVE}/86box/snd_emu8k.c")
  target_include_directories(P205AllocatorReset${P205_KIND} PRIVATE
    "${NATIVE}" "${NATIVE}/86box/include")
  target_compile_definitions(P205AllocatorReset${P205_KIND} PRIVATE
    AWE32EMU_WITH_86BOX _CRT_SECURE_NO_WARNINGS
    P019_EMU_BLOCK_FRAMES=64 P205_VERIFY_ALLOC_RESET)
  target_link_libraries(P205AllocatorReset${P205_KIND} PRIVATE winmm)
  target_compile_features(P205AllocatorReset${P205_KIND} PRIVATE cxx_std_17)
endforeach()
target_compile_definitions(P205AllocatorResetNegative PRIVATE
  P205_NEGATIVE_SKIP_ALLOC_RESET)


# P206 Win95 original Creative CC120 / CC123 (sustain) true chip ports.
foreach(P206_KIND IN ITEMS Native Negative)
  add_executable(P206OriginalChannelCc${P206_KIND}
    tests/P206OriginalCreativeChannelCc.cpp
    "${NATIVE}/Synth.cpp" "${NATIVE}/SoundFont.cpp"
    "${NATIVE}/Emu8000.cpp" "${NATIVE}/Emu8000Box.cpp"
    "${NATIVE}/I18n.cpp" "${NATIVE}/86box/snd_emu8k.c")
  target_include_directories(P206OriginalChannelCc${P206_KIND} PRIVATE
    "${NATIVE}" "${NATIVE}/86box/include")
  target_compile_definitions(P206OriginalChannelCc${P206_KIND} PRIVATE
    AWE32EMU_WITH_86BOX _CRT_SECURE_NO_WARNINGS P019_EMU_BLOCK_FRAMES=64)
  target_link_libraries(P206OriginalChannelCc${P206_KIND} PRIVATE winmm)
  target_compile_features(P206OriginalChannelCc${P206_KIND} PRIVATE cxx_std_17)
endforeach()
target_compile_definitions(P206OriginalChannelCcNegative PRIVATE
  P206_NEGATIVE_LEGACY_CC)


# P207 genuine original 86Box chip VXD signed-bend unsigned saturation:
# fail-closed against both live CC pitch writes and pre-bent NoteOn.
foreach(P207_KIND IN ITEMS Native Negative)
  add_executable(P207OriginalPitch${P207_KIND}
    tests/P207OriginalCreativePitch.cpp
    "${NATIVE}/Synth.cpp" "${NATIVE}/SoundFont.cpp"
    "${NATIVE}/Emu8000.cpp" "${NATIVE}/Emu8000Box.cpp"
    "${NATIVE}/I18n.cpp" "${NATIVE}/86box/snd_emu8k.c")
  target_include_directories(P207OriginalPitch${P207_KIND} PRIVATE
    "${NATIVE}" "${NATIVE}/86box/include")
  target_compile_definitions(P207OriginalPitch${P207_KIND} PRIVATE
    AWE32EMU_WITH_86BOX _CRT_SECURE_NO_WARNINGS P019_EMU_BLOCK_FRAMES=64)
  target_link_libraries(P207OriginalPitch${P207_KIND} PRIVATE winmm)
  target_compile_features(P207OriginalPitch${P207_KIND} PRIVATE cxx_std_17)
endforeach()
target_compile_definitions(P207OriginalPitchNegative PRIVATE
  P207_NEGATIVE_OLD_SIGNED_CLAMP)


# P209: original 86Box chip and original VXD stereo allocator.
# Accept independent linked L/R PCM lengths + test-only old rejection.
foreach(P209_KIND IN ITEMS Native Negative)
  add_executable(P209UnequalStereo${P209_KIND}
    tests/P204OriginalReservationPortTrace.cpp
    "${NATIVE}/Synth.cpp" "${NATIVE}/SoundFont.cpp"
    "${NATIVE}/Emu8000.cpp" "${NATIVE}/Emu8000Box.cpp"
    "${NATIVE}/I18n.cpp" "${NATIVE}/86box/snd_emu8k.c")
  target_include_directories(P209UnequalStereo${P209_KIND} PRIVATE
    "${NATIVE}" "${NATIVE}/86box/include")
  target_compile_definitions(P209UnequalStereo${P209_KIND} PRIVATE
    AWE32EMU_WITH_86BOX _CRT_SECURE_NO_WARNINGS
    P019_EMU_BLOCK_FRAMES=64 P209_VERIFY_UNEQUAL_STEREO)
  target_link_libraries(P209UnequalStereo${P209_KIND} PRIVATE winmm)
  target_compile_features(P209UnequalStereo${P209_KIND} PRIVATE cxx_std_17)
endforeach()
target_compile_definitions(P209UnequalStereoNegative PRIVATE
  P209_NEGATIVE_REQUIRE_EQUAL_STEREO_LENGTH)

# P202 Legacy: same production Synth/EMU8000 native source as the VST3.
# The test renders original 86Box chip PCM and measures 44.1k old/new CPU.
# Legacy macro restores only the pre-fix unused second DSP.
add_executable(P202CpuLegacy
   tests/P202RealChipCpuProbe.cpp
   "${NATIVE}/Synth.cpp" "${NATIVE}/SoundFont.cpp"
   "${NATIVE}/Emu8000.cpp" "${NATIVE}/Emu8000Box.cpp"
   "${NATIVE}/I18n.cpp" "${NATIVE}/86box/snd_emu8k.c")
target_include_directories(P202CpuLegacy PRIVATE
   "${NATIVE}" "${NATIVE}/86box/include")
target_compile_definitions(P202CpuLegacy PRIVATE
   AWE32EMU_WITH_86BOX _CRT_SECURE_NO_WARNINGS
   P019_EMU_BLOCK_FRAMES=64)
target_link_libraries(P202CpuLegacy PRIVATE winmm)
target_compile_definitions(P202CpuLegacy PRIVATE P202_CPU_LEGACY_REFERENCE)
target_compile_features(P202CpuLegacy PRIVATE cxx_std_17)

# P202 Optimized: same production Synth/EMU8000 native source as the VST3.
# The test renders original 86Box chip PCM and measures 44.1k old/new CPU.
# Legacy macro restores only the pre-fix unused second DSP.
add_executable(P202CpuOptimized
   tests/P202RealChipCpuProbe.cpp
   "${NATIVE}/Synth.cpp" "${NATIVE}/SoundFont.cpp"
   "${NATIVE}/Emu8000.cpp" "${NATIVE}/Emu8000Box.cpp"
   "${NATIVE}/I18n.cpp" "${NATIVE}/86box/snd_emu8k.c")
target_include_directories(P202CpuOptimized PRIVATE
   "${NATIVE}" "${NATIVE}/86box/include")
target_compile_definitions(P202CpuOptimized PRIVATE
   AWE32EMU_WITH_86BOX _CRT_SECURE_NO_WARNINGS
   P019_EMU_BLOCK_FRAMES=64)
target_link_libraries(P202CpuOptimized PRIVATE winmm)
target_compile_features(P202CpuOptimized PRIVATE cxx_std_17)

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
   AWE32EMU_WITH_86BOX _CRT_SECURE_NO_WARNINGS
   P019_EMU_BLOCK_FRAMES=64)
target_link_libraries(P009RealtimeAllocationProbe PRIVATE winmm)
target_compile_features(P009RealtimeAllocationProbe PRIVATE cxx_std_17)

# P010: test ACTUAL compiled Windows VST3 using Steinberg's SDK hosting API.
add_executable(P010NativeHostQA
  tests/P010NativeHostQA.cpp
  "${P009_SDK_ROOT}/public.sdk/source/vst/hosting/module_win32.cpp"
  "${P009_SDK_ROOT}/public.sdk/source/vst/hosting/parameterchanges.cpp"
  "${P009_SDK_ROOT}/public.sdk/source/common/memorystream.cpp")
target_include_directories(P010NativeHostQA PRIVATE "${P009_SDK_ROOT}" "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(P010NativeHostQA PRIVATE sdk_hosting bcrypt psapi)
target_compile_features(P010NativeHostQA PRIVATE cxx_std_17)

# Browser model includes native std::span through IbkBank; compile it
# independently in C++20 so Steinberg's C++17 module_win32 stays untouched.
add_executable(P173BrowserContract tests/P173BrowserContract.cpp)
target_include_directories(P173BrowserContract PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(P173BrowserContract PRIVATE cxx_std_20)
 
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

# P019: diagnostic ONLY. The VST3 above always retains the pinned 980-frame
# chip clock window. Three separate standalone original-chip probes compare
# PCM after latency alignment; a short test window is NEVER shipped.
foreach(P019_WINDOW IN ITEMS 980 128 64)
  add_executable(P019Probe${P019_WINDOW}
     tests/P019ChipLatencyProbe.cpp
     "${NATIVE}/Synth.cpp" "${NATIVE}/SoundFont.cpp"
     "${NATIVE}/Emu8000.cpp" "${NATIVE}/Emu8000Box.cpp"
     "${NATIVE}/I18n.cpp" "${NATIVE}/86box/snd_emu8k.c")
  target_include_directories(P019Probe${P019_WINDOW} PRIVATE
     "${NATIVE}" "${NATIVE}/86box/include")
  target_compile_definitions(P019Probe${P019_WINDOW} PRIVATE
     AWE32EMU_WITH_86BOX _CRT_SECURE_NO_WARNINGS
     P019_EMU_BLOCK_FRAMES=${P019_WINDOW})
  target_link_libraries(P019Probe${P019_WINDOW} PRIVATE winmm)
  target_compile_features(P019Probe${P019_WINDOW} PRIVATE cxx_std_17)
endforeach()

# P019 original owner ROM/SBK/SF2 test package uses a self-contained
# Win64 native comparator; user Windows requires no Python or extra tools.
add_executable(P019ComparePCM tests/P019ComparePCM.cpp)
target_compile_features(P019ComparePCM PRIVATE cxx_std_17)

# P021: A VST3 at 48/96k must STILL play the real 86Box EMU8000 chip!
# Resampling the alternative C++ synth engine is a false sound PASS.
# Compare 8/48/96/192k chip output with original 44.1k PCM at exact timestamps.
file(READ "${NATIVE}/Emu8000.cpp" P021_CORE_SOURCE)
if(NOT P021_CORE_SOURCE MATCHES "P021_BOX86_ORIGINAL_CHIP_RESAMPLE")
  message(FATAL_ERROR
     "P021 original Box86 non-native-rate output fix is missing")
endif()
unset(P021_CORE_SOURCE)
add_executable(P021OriginalChipResamplingProbe
   tests/P021OriginalChipResamplingProbe.cpp
   "${NATIVE}/Synth.cpp" "${NATIVE}/SoundFont.cpp"
   "${NATIVE}/Emu8000.cpp" "${NATIVE}/Emu8000Box.cpp"
   "${NATIVE}/I18n.cpp" "${NATIVE}/86box/snd_emu8k.c")
target_include_directories(P021OriginalChipResamplingProbe PRIVATE
   "${NATIVE}" "${NATIVE}/86box/include")
target_compile_definitions(P021OriginalChipResamplingProbe PRIVATE
   AWE32EMU_WITH_86BOX _CRT_SECURE_NO_WARNINGS P019_EMU_BLOCK_FRAMES=64)
target_link_libraries(P021OriginalChipResamplingProbe PRIVATE winmm)
target_compile_features(P021OriginalChipResamplingProbe PRIVATE cxx_std_17)


# P210: real original 86Box chip/Win95 allocator and a test-only unsafe
# voice0 reuse mutant. Neither test executable is packaged in the VST3.
foreach(P210_KIND IN ITEMS Native Negative)
  add_executable(P210VoiceBudget${P210_KIND}
    tests/P210VoiceBudgetTrace.cpp
    "${NATIVE}/Synth.cpp" "${NATIVE}/SoundFont.cpp"
    "${NATIVE}/Emu8000.cpp" "${NATIVE}/Emu8000Box.cpp"
    "${NATIVE}/I18n.cpp" "${NATIVE}/86box/snd_emu8k.c")
  target_include_directories(P210VoiceBudget${P210_KIND} PRIVATE
    "${NATIVE}" "${NATIVE}/86box/include")
  target_compile_definitions(P210VoiceBudget${P210_KIND} PRIVATE
    AWE32EMU_WITH_86BOX _CRT_SECURE_NO_WARNINGS P019_EMU_BLOCK_FRAMES=64)
  target_link_libraries(P210VoiceBudget${P210_KIND} PRIVATE winmm)
  target_compile_features(P210VoiceBudget${P210_KIND} PRIVATE cxx_std_17)
endforeach()
target_compile_definitions(P210VoiceBudgetNegative PRIVATE
  P210_NEGATIVE_REUSE_RESERVED_VOICE0)


# P210: repeat the existing real 86Box heap counter under 37 simultaneous
# SF2 zones, both 44.1/48k and two concurrent synths. Test-only target.
add_executable(P210Realtime37LayerProbe
   tests/P009RealtimeAllocationProbe.cpp
   "${NATIVE}/Synth.cpp" "${NATIVE}/SoundFont.cpp"
   "${NATIVE}/Emu8000.cpp" "${NATIVE}/Emu8000Box.cpp"
   "${NATIVE}/I18n.cpp" "${NATIVE}/86box/snd_emu8k.c")
target_include_directories(P210Realtime37LayerProbe PRIVATE
   "${NATIVE}" "${NATIVE}/86box/include")
target_compile_definitions(P210Realtime37LayerProbe PRIVATE
   AWE32EMU_WITH_86BOX _CRT_SECURE_NO_WARNINGS P019_EMU_BLOCK_FRAMES=64
   P210_37_LAYER_RT_STRESS)
target_link_libraries(P210Realtime37LayerProbe PRIVATE winmm)
target_compile_features(P210Realtime37LayerProbe PRIVATE cxx_std_17)

# P213: in-memory PUBLIC synthetic SF2 instrument/preset converted-envelope
# merger proof; same patched source as production VST3. No private SF2/ROM.
add_executable(P213ConvertedScopeEnvelope
    tests/P213ConvertedScopeEnvelope.cpp
    "${NATIVE}/SoundFont.cpp"
    "${NATIVE}/I18n.cpp")
target_include_directories(P213ConvertedScopeEnvelope PRIVATE "${NATIVE}")
target_compile_features(P213ConvertedScopeEnvelope PRIVATE cxx_std_17)

# P214: authentic Creative Win95 SF2 preset decay, LFO template and DRAM
# loop-address calculations. Public synthetic note only; owner media absent.
add_executable(P214OriginalSf2ScopeDefaults
    tests/P214OriginalSf2ScopeDefaults.cpp
    "${NATIVE}/SoundFont.cpp"
    "${NATIVE}/I18n.cpp")
target_include_directories(P214OriginalSf2ScopeDefaults PRIVATE "${NATIVE}")
target_compile_features(P214OriginalSf2ScopeDefaults PRIVATE cxx_std_17)
