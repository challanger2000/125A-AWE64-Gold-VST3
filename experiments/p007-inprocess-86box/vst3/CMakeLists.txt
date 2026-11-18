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
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES ON CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VSTGUI_SUPPORT ON CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
add_subdirectory("${P007_SDK_ROOT}" "${CMAKE_BINARY_DIR}/vst3sdk")
smtg_add_vst3plugin(AWE64_Gold_P007
  P007Native.hpp P007Native.cpp P007CreativeNrpn.hpp P007Factory.cpp
  # P007Z: owner-approved COMPLETE P165 original, not P007 minimalist stub.
  p165/PresetBrowserView.h p165/PresetBrowserView.cpp
  p165/PresetCatalog.h p165/PresetBrowserModel.h p165/IbkBank.h
  "${P007_SDK_ROOT}/public.sdk/source/vst/vstsinglecomponenteffect.cpp"
  "${P007_SDK_ROOT}/public.sdk/source/main/dllmain.cpp")
target_include_directories(AWE64_Gold_P007 PRIVATE "${P007_SDK_ROOT}")
target_link_libraries(AWE64_Gold_P007 PRIVATE sdk vstgui_support comdlg32)
target_compile_features(AWE64_Gold_P007 PRIVATE cxx_std_23)
smtg_target_configure_version_file(AWE64_Gold_P007)

# Import the exact user's EXISTING P165 frontplate, not an approximation.
# This artwork exists at pinned commit of old 125A-AWE-Synthesizer; Git blob
# SHA-1 is validated before compiling or packaging the VST3. Source and
# target are the user's public Github repos. No other previous DSP is used.
set(P007_FACEPLATE "${CMAKE_CURRENT_SOURCE_DIR}/resource/125A_P165_Original_Faceplate_1000x708.png")
if(NOT EXISTS "${P007_FACEPLATE}")
  file(DOWNLOAD
    "https://raw.githubusercontent.com/challanger2000/125A-AWE-Synthesizer/8e8315c2ecab9f4a8bdbd415c1b7af5a8485a3c1/src/vst3/resource/125A_P165_Original_Faceplate_1000x708.png"
    "${P007_FACEPLATE}" STATUS P007_P165_DOWNLOAD TLS_VERIFY ON TIMEOUT 60)
  list(GET P007_P165_DOWNLOAD 0 P007_P165_DL_CODE)
  if(NOT P007_P165_DL_CODE EQUAL 0)
    message(FATAL_ERROR "Original P165 faceplate import unavailable: ${P007_P165_DOWNLOAD}")
  endif()
endif()
execute_process(COMMAND git hash-object "${P007_FACEPLATE}"
  OUTPUT_VARIABLE P007_P165_BLOB_HASH OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE P007_P165_HASH_CODE)
if(NOT P007_P165_HASH_CODE EQUAL 0 OR
   NOT P007_P165_BLOB_HASH STREQUAL "7e57ef8e0c90d84fa4c951bea7815b0233762ca7")
  message(FATAL_ERROR "Exact original P165 image SHA mismatch; refuse substitution")
endif()
smtg_target_add_plugin_resources(AWE64_Gold_P007 RESOURCES
    resource/p007_p165.uidesc
    resource/125A_P165_Original_Faceplate_1000x708.png
    resource/125A_Button_Atlas_Hardware_Normal_Pressed.png
    resource/125A_Knob_Standard_33frames_58px.png
    resource/125A_Knob_Master_33frames_134px.png)
# Original P165 uses Win32 PNG resources for CBitmap(name), in addition
# to the Resources files required by Steinberg's bundle structure.
if(WIN32)
  set(P007_P165_RC "${CMAKE_CURRENT_BINARY_DIR}/p007_p165_assets.rc")
  file(WRITE "${P007_P165_RC}" "")
  foreach(P007_ASSET IN ITEMS
      "125A_P165_Original_Faceplate_1000x708.png"
      "125A_Button_Atlas_Hardware_Normal_Pressed.png"
      "125A_Knob_Standard_33frames_58px.png"
      "125A_Knob_Master_33frames_134px.png")
    file(APPEND "${P007_P165_RC}"
      "\"${P007_ASSET}\" PNG DISCARDABLE \"${CMAKE_CURRENT_SOURCE_DIR}/resource/${P007_ASSET}\"\n")
  endforeach()
  target_sources(AWE64_Gold_P007 PRIVATE "${P007_P165_RC}")
endif()

# Explicitly test original Creative bank/program/master DAW project state.
add_executable(P007StateCodecTest
  "${CMAKE_CURRENT_SOURCE_DIR}/../tests/P007StateCodecTest.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/P007StateCodec.hpp")
target_include_directories(P007StateCodecTest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(P007StateCodecTest PRIVATE cxx_std_20)


# P007W: independent actual Windows VST3 host, not codec-only regression.
# The optional --private-audio mode runs ONLY on the owner's private Windows
# machine with their original Win95/Creative guest assets. Public CI must never
# upload private images or misrepresent a synthetic run as original PCM.
add_executable(P007ActualHostQA
  "${CMAKE_CURRENT_SOURCE_DIR}/../tests/P007ActualHostQA.cpp"
  "${P007_SDK_ROOT}/public.sdk/source/vst/hosting/module_win32.cpp"
  "${P007_SDK_ROOT}/public.sdk/source/common/memorystream.cpp")
target_include_directories(P007ActualHostQA PRIVATE "${P007_SDK_ROOT}")
target_link_libraries(P007ActualHostQA PRIVATE sdk_hosting)
target_compile_features(P007ActualHostQA PRIVATE cxx_std_17)


# CI-only fake core for an ephemeral copy of the actual Windows VST3.
# This must NEVER be embedded in the private product or uploaded as a
# Creative/EMU8000 test result. Distinct markers explicitly say SYNTHETIC.
add_library(P007SyntheticCoreMock SHARED
  "${CMAKE_CURRENT_SOURCE_DIR}/../tests/P007SyntheticCoreMock.cpp")
target_compile_features(P007SyntheticCoreMock PRIVATE cxx_std_20)
set_target_properties(P007SyntheticCoreMock PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/synthetic-core")


# P007Y: strict evidence threshold for PRIVATE original Creative PCM.
# Public synthetic audio must fail the original-guest proof gate.
add_executable(P007OriginalAudioProofTest
  "${CMAKE_CURRENT_SOURCE_DIR}/../tests/P007OriginalAudioProofTest.cpp")
target_include_directories(P007OriginalAudioProofTest PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../tests")
target_compile_features(P007OriginalAudioProofTest PRIVATE cxx_std_17)

# P007AA: exact twelve Creative NRPN generator mapping and rejection tests
add_executable(P007CreativeNrpnTest
  "${CMAKE_CURRENT_SOURCE_DIR}/../tests/P007CreativeNrpnTest.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/P007CreativeNrpn.hpp")
target_include_directories(P007CreativeNrpnTest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(P007CreativeNrpnTest PRIVATE cxx_std_20)
