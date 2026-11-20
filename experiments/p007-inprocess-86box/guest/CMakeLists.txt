cmake_minimum_required(VERSION 3.25)
project(P007_Win95_Creative_Relay LANGUAGES C)
if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "P007 guest helper requires 32-bit MSVC for original Win95")
endif()
add_executable(P007_Win95_Original_Creative_Relay P007_Win95_NoCRT.c)
target_compile_options(P007_Win95_Original_Creative_Relay PRIVATE /W4 /WX /GS- /Oi- /arch:IA32)
target_link_options(P007_Win95_Original_Creative_Relay PRIVATE
    /NODEFAULTLIB /ENTRY:P007GuestMain /SUBSYSTEM:CONSOLE,5.0)
target_link_libraries(P007_Win95_Original_Creative_Relay PRIVATE kernel32 winmm)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_custom_command(TARGET P007_Win95_Original_Creative_Relay POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/patch_win95_pe.py"
            "$<TARGET_FILE:P007_Win95_Original_Creative_Relay>"
    VERBATIM)
