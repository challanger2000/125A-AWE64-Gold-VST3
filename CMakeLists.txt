cmake_minimum_required(VERSION 3.25)
project(AWE64GoldP001 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()

set(FORK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/86box-awe32")
set(FORK_CHIP "${FORK_ROOT}/src/sound/snd_emu8k.c")
set(FORK_COMMIT "fee1e81977824fd2d5eed4d239589dc30d251dd7")
set(FORK_BLOB "e60eb10f00a8e1187380faa25989f6ba0433d99a")
if(NOT EXISTS "${FORK_CHIP}")
    message(FATAL_ERROR "Pinned fork submodule missing: run git submodule update --init")
endif()
find_package(Git REQUIRED)
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${FORK_ROOT}" rev-parse HEAD
    OUTPUT_VARIABLE FORK_HEAD OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE HEAD_RC)
execute_process(COMMAND "${GIT_EXECUTABLE}" hash-object "${FORK_CHIP}"
    OUTPUT_VARIABLE CHIP_BLOB OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE BLOB_RC)
if(NOT HEAD_RC EQUAL 0 OR NOT FORK_HEAD STREQUAL FORK_COMMIT)
    message(FATAL_ERROR "Fork commit mismatch: ${FORK_HEAD}; expected ${FORK_COMMIT}")
endif()
if(NOT BLOB_RC EQUAL 0 OR NOT CHIP_BLOB STREQUAL FORK_BLOB)
    message(FATAL_ERROR "Original chip source modified or incorrect: ${CHIP_BLOB}; expected ${FORK_BLOB}")
endif()
message(STATUS "P001 original EMU8000 fork verified: ${FORK_HEAD} / blob ${CHIP_BLOB}")

add_library(awe64_fork_chip STATIC
    "${FORK_CHIP}"
    src/chip_adapter/HostShim.cpp
    src/chip_adapter/Chip.cpp)
target_include_directories(awe64_fork_chip
    PRIVATE src/chip_adapter/shim_include
    PUBLIC "${FORK_ROOT}/src/include" "${CMAKE_CURRENT_SOURCE_DIR}/src/chip_adapter")
if(MSVC)
    target_compile_definitions(awe64_fork_chip PRIVATE _CRT_SECURE_NO_WARNINGS _USE_MATH_DEFINES)
    target_compile_options(awe64_fork_chip PRIVATE /W3)
else()
    target_link_libraries(awe64_fork_chip PUBLIC m)
endif()

add_executable(awe64_replay tests/replay.cpp)
target_link_libraries(awe64_replay PRIVATE awe64_fork_chip)
add_executable(awe64_chip_contract tests/chip_contract.cpp)
target_link_libraries(awe64_chip_contract PRIVATE awe64_fork_chip)
add_test(NAME p001_chip_ram_rom_contract COMMAND awe64_chip_contract)

# A second process drives the *unmodified* original fork C interface directly
# and records synthetic non-silent PCM; the adapter must replay its port writes
# against that reference, with positive and destructive negative controls.
add_executable(awe64_direct_fork_fixture tests/direct_fork_fixture.cpp)
target_link_libraries(awe64_direct_fork_fixture PRIVATE awe64_fork_chip)
target_include_directories(awe64_direct_fork_fixture PRIVATE src/chip_adapter/shim_include)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME p001_direct_fork_pcm_and_negative_controls
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_replay.py"
  "$<TARGET_FILE:awe64_direct_fork_fixture>" "$<TARGET_FILE:awe64_replay>")
# No claim of fork PCM parity is made until an independent 86Box AWE32_WAV
# capture, its register trace, and the exact private ROM are provided.

# P002: Creative 1996 public API factual identifiers and byte-level WinMM
# staging. No claim of original driver register/acoustic equivalence until
# independently captured original Creative traces are compared.
add_library(awe64_creative_midi STATIC src/creative_compat/CreativeMidi.cpp)
target_include_directories(awe64_creative_midi PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/creative_compat")
target_compile_features(awe64_creative_midi PUBLIC cxx_std_23)
add_executable(awe64_p002_midi_contract tests/p002_midi_contract.cpp)
target_link_libraries(awe64_p002_midi_contract PRIVATE awe64_creative_midi)
add_test(NAME p002_winmm_primary_api_16channel_contract COMMAND awe64_p002_midi_contract)
# P002 experimentally source-derived/common velocity + independent IFATN
# port writes. It is NOT silently wired into the approved P001 plugin.
add_library(awe64_p002_register_candidate STATIC src/creative_compat/CreativeCandidateRegister.cpp)
target_include_directories(awe64_p002_register_candidate PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/creative_compat")
target_compile_features(awe64_p002_register_candidate PUBLIC cxx_std_23)
target_link_libraries(awe64_p002_register_candidate PUBLIC awe64_creative_midi)
add_executable(awe64_p002_register_candidate_test tests/p002_candidate_register_test.cpp)
target_link_libraries(awe64_p002_register_candidate_test PRIVATE awe64_p002_register_candidate)
add_test(NAME p002_native_midi_to_ifatn_candidate_contract COMMAND awe64_p002_register_candidate_test)

# P001 only, activated after original-1MiB-ROM direct-core digital PCM parity.
# This VST3 is an instrument smoke target with fixed AWETEST ROM tone only.
option(AWE64_BUILD_P001_VST3 "Build experimental minimal VST3 instrument" OFF)
if(AWE64_BUILD_P001_VST3)
    if(NOT WIN32)
        message(FATAL_ERROR "125A AWE64 Gold VST3 targets Windows x64 only")
    endif()
    set(AWE64_VST3_SDK_ROOT "${CMAKE_SOURCE_DIR}/external/vst3sdk" CACHE PATH "Pinned VST3 SDK root")
    if(NOT EXISTS "${AWE64_VST3_SDK_ROOT}/public.sdk/source/vst/vstsinglecomponenteffect.cpp")
        message(FATAL_ERROR "Pinned recursive Steinberg SDK checkout missing")
    endif()
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${AWE64_VST3_SDK_ROOT}" rev-parse HEAD
        OUTPUT_VARIABLE SDK_HEAD OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE SDK_RC)
    if(NOT SDK_RC EQUAL 0 OR NOT SDK_HEAD STREQUAL "3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96")
        message(FATAL_ERROR "Unexpected Steinberg VST3 SDK revision: ${SDK_HEAD}")
    endif()
    set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
    set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
    add_subdirectory("${AWE64_VST3_SDK_ROOT}" "${CMAKE_BINARY_DIR}/vst3sdk")
    smtg_add_vst3plugin(AWE64_Gold_P001
        src/vst3/P001Minimal.h
        src/vst3/P001Minimal.cpp
        src/vst3/P001Factory.cpp
        "${AWE64_VST3_SDK_ROOT}/public.sdk/source/vst/vstsinglecomponenteffect.cpp"
        "${AWE64_VST3_SDK_ROOT}/public.sdk/source/main/dllmain.cpp")
    target_include_directories(AWE64_Gold_P001 PRIVATE
        "${AWE64_VST3_SDK_ROOT}")
    target_link_libraries(AWE64_Gold_P001 PRIVATE sdk awe64_fork_chip)
    target_compile_features(AWE64_Gold_P001 PRIVATE cxx_std_23)
    smtg_target_configure_version_file(AWE64_Gold_P001)
    add_executable(awe64_p001_vst3_host tests/p001_vst3_host.cpp)
    target_include_directories(awe64_p001_vst3_host PRIVATE "${AWE64_VST3_SDK_ROOT}")
    target_link_libraries(awe64_p001_vst3_host PRIVATE sdk sdk_hosting)
    target_compile_features(awe64_p001_vst3_host PRIVATE cxx_std_23)
    add_dependencies(awe64_p001_vst3_host AWE64_Gold_P001)

    # Test-only real Windows heap interception: downloaded/pinned MinHook
    # is linked into the independent host, NEVER into our VST3/product DLL.
    set(AWE64_MINHOOK_ROOT "${CMAKE_SOURCE_DIR}/external/minhook" CACHE PATH "Pinned test-only MinHook root")
    if(NOT EXISTS "${AWE64_MINHOOK_ROOT}/include/MinHook.h")
        message(FATAL_ERROR "P001 native heap audit requires pinned MinHook test dependency")
    endif()
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${AWE64_MINHOOK_ROOT}" rev-parse HEAD
        OUTPUT_VARIABLE MINHOOK_HEAD OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE MINHOOK_RC)
    if(NOT MINHOOK_RC EQUAL 0 OR NOT MINHOOK_HEAD STREQUAL "c3fcafdc10146beb5919319d0683e44e3c30d537")
        message(FATAL_ERROR "Unexpected pinned test-only MinHook commit: ${MINHOOK_HEAD}")
    endif()
    add_subdirectory("${AWE64_MINHOOK_ROOT}" "${CMAKE_BINARY_DIR}/minhook")
    add_executable(awe64_p001_vst3_rt_host tests/p001_vst3_rt_host.cpp)
    target_link_libraries(awe64_p001_vst3_rt_host PRIVATE sdk sdk_hosting minhook)
    target_include_directories(awe64_p001_vst3_rt_host PRIVATE "${AWE64_VST3_SDK_ROOT}")
    target_compile_features(awe64_p001_vst3_rt_host PRIVATE cxx_std_23)
    add_dependencies(awe64_p001_vst3_rt_host AWE64_Gold_P001)
endif()
