cmake_minimum_required(VERSION 3.25)
project(AWE64_P006_86Box_Bridge VERSION 0.0.1 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()
add_executable(p006_audio_midi_bridge_contract tests/test_bridge.cpp)
target_include_directories(p006_audio_midi_bridge_contract PRIVATE bridge)
if(MSVC)
  target_compile_options(p006_audio_midi_bridge_contract PRIVATE /W4 /WX /permissive-)
else()
  target_compile_options(p006_audio_midi_bridge_contract PRIVATE -Wall -Wextra -Wconversion -Werror -pedantic)
endif()
add_test(NAME p006_spsc_audio_and_midi_wire_contract COMMAND p006_audio_midi_bridge_contract)
option(P006_BUILD_VST3 "Build experimental Windows-x64 VST3" OFF)
if(P006_BUILD_VST3)
  add_subdirectory(vst3)
endif()

# Win95 guest helper: PE32 subsystem 4.0 and ONLY kernel32/winmm imports.
# MSVC modern VCRUNTIME/UCRT is forbidden: /NODEFAULTLIB, direct ENTRY.
option(P006_BUILD_WIN95_GUEST "Build CRT-free Win95 guest relay" OFF)
if(P006_BUILD_WIN95_GUEST)
  if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "P006 Win95 helper needs native MSVC Win32")
  endif()
  add_executable(P006_Win95_Original_Creative_Relay
    guest/P006_Win95_NoCRT.c)
  target_include_directories(P006_Win95_Original_Creative_Relay
    PRIVATE bridge)
  target_compile_options(P006_Win95_Original_Creative_Relay
    PRIVATE /W4 /WX /GS- /Oi- /arch:IA32)
  target_link_options(P006_Win95_Original_Creative_Relay PRIVATE
    /NODEFAULTLIB /ENTRY:P006GuestMain /SUBSYSTEM:CONSOLE,5.0)
  target_link_libraries(P006_Win95_Original_Creative_Relay PRIVATE
    kernel32 winmm)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  add_custom_command(TARGET P006_Win95_Original_Creative_Relay POST_BUILD
    COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/guest/patch_win95_pe.py"
      "$<TARGET_FILE:P006_Win95_Original_Creative_Relay>"
    VERBATIM)
endif()
