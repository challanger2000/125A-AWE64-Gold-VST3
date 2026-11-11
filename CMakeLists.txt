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
