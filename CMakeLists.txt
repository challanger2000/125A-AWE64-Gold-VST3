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


# P002 bounded native Creative 1MGM zone-to-IFATN bridge.
# No original bank bytes shipped. No P001 chip or production VST3 changes.
add_library(awe64_p002_sbk_zone STATIC
    src/creative_compat/CreativeSbkDescriptor.cpp
    src/creative_compat/CreativeSbkCandidate.cpp)
target_include_directories(awe64_p002_sbk_zone PUBLIC src/creative_compat)
target_compile_features(awe64_p002_sbk_zone PUBLIC cxx_std_23)
target_link_libraries(awe64_p002_sbk_zone PUBLIC awe64_p002_register_candidate)
add_executable(awe64_p002_sbk_zone_test tests/p002_native_sbk_zone.cpp)
target_link_libraries(awe64_p002_sbk_zone_test PRIVATE awe64_p002_sbk_zone)
add_test(NAME p002_native_creative_sbk_key_zone_to_ifatn_contract
         COMMAND awe64_p002_sbk_zone_test)


# P002 raw original Creative 1MGM preset/instrument layering. Does not
# allocate physical voices or modify the approved P001 chip/VST3.
add_library(awe64_p002_sbk_layers STATIC
    src/creative_compat/CreativeSbkLayers.cpp)
target_include_directories(awe64_p002_sbk_layers PUBLIC src/creative_compat)
target_compile_features(awe64_p002_sbk_layers PUBLIC cxx_std_23)
add_executable(awe64_p002_sbk_layers_test tests/p002_native_sbk_layers.cpp)
target_link_libraries(awe64_p002_sbk_layers_test PRIVATE awe64_p002_sbk_layers)
add_test(NAME p002_native_creative_sbk_multilayer_contract
         COMMAND awe64_p002_sbk_layers_test)


# MIDI NoteOn + 16-channel program state -> original Creative bank layers
# -> genuine ROM word-address descriptors, without voice/allocation guesses.
add_library(awe64_p002_rom_note_plan STATIC
    src/creative_compat/CreativeMidiRomPlan.cpp)
target_include_directories(awe64_p002_rom_note_plan PUBLIC src/creative_compat)
target_compile_features(awe64_p002_rom_note_plan PUBLIC cxx_std_23)
target_link_libraries(awe64_p002_rom_note_plan
    PUBLIC awe64_p002_sbk_layers awe64_creative_midi)
add_executable(awe64_p002_rom_note_plan_test
    tests/p002_native_midi_rom_plan.cpp)
target_link_libraries(awe64_p002_rom_note_plan_test
    PRIVATE awe64_p002_rom_note_plan)
add_test(NAME p002_winmm_program_to_original_rom_address_contract
         COMMAND awe64_p002_rom_note_plan_test)


# Native P002 original-1MGM pitch candidate. Generates actual hardware IP
# E22/E20 port words from MIDI key, gen55 and pitchbend, with soundbank
# and physical-voice validation. Not an original-driver equivalence pass.
add_library(awe64_p002_pitch_candidate STATIC
    src/creative_compat/CreativePitchCandidate.cpp)
target_include_directories(awe64_p002_pitch_candidate PUBLIC src/creative_compat)
target_compile_features(awe64_p002_pitch_candidate PUBLIC cxx_std_23)
target_link_libraries(awe64_p002_pitch_candidate
    PUBLIC awe64_p002_rom_note_plan awe64_p002_register_candidate)
add_executable(awe64_p002_pitch_candidate_test
    tests/p002_native_pitch_candidate.cpp)
target_link_libraries(awe64_p002_pitch_candidate_test PRIVATE awe64_p002_pitch_candidate)
add_test(NAME p002_native_original_creative_pitch_bend_candidate
         COMMAND awe64_p002_pitch_candidate_test)


# Original Creative Windows95 SBAWE.VXD-derived CC7/CC11/velocity/ROM
# IFATN register candidate. Bank encoded attenuation still caller-owned.
add_library(awe64_p002_original_controller_volume STATIC
    src/creative_compat/CreativeControllerVolume.cpp)
target_include_directories(awe64_p002_original_controller_volume
    PUBLIC src/creative_compat)
target_compile_features(awe64_p002_original_controller_volume PUBLIC cxx_std_23)
target_link_libraries(awe64_p002_original_controller_volume
    PUBLIC awe64_p002_register_candidate)
add_executable(awe64_p002_original_controller_volume_test
    tests/p002_native_controller_volume.cpp)
target_link_libraries(awe64_p002_original_controller_volume_test
    PRIVATE awe64_p002_original_controller_volume)
add_test(NAME p002_native_real_win95_driver_cc7_cc11_ifatn
         COMMAND awe64_p002_original_controller_volume_test)


# P002 stateful ROM voice binding + live controller register words.
# Physical voice allocation and release/kill register encodings remain
# explicit inputs/semantic actions, never invented Creative driver parity.
add_library(awe64_p002_bound_voices STATIC
    src/creative_compat/CreativeBoundVoiceController.cpp)
target_include_directories(awe64_p002_bound_voices PUBLIC src/creative_compat)
target_compile_features(awe64_p002_bound_voices PUBLIC cxx_std_23)
target_link_libraries(awe64_p002_bound_voices
    PUBLIC awe64_p002_original_controller_volume awe64_p002_pitch_candidate)
add_executable(awe64_p002_bound_voices_test
    tests/p002_native_bound_voice_controller.cpp)
target_link_libraries(awe64_p002_bound_voices_test
    PRIVATE awe64_p002_bound_voices)
add_test(NAME p002_native_real_creative_live_voice_controllers
         COMMAND awe64_p002_bound_voices_test)


# Exact *idle* branch of Creative Win95 SBAWE.VXD mono-voice scan:
# 30 musical voices visited in original 3-lane order; no invented stealing.
add_library(awe64_p002_idle_allocator STATIC
    src/creative_compat/CreativeIdleVoiceAllocator.cpp)
target_include_directories(awe64_p002_idle_allocator PUBLIC src/creative_compat)
target_compile_features(awe64_p002_idle_allocator PUBLIC cxx_std_23)
add_executable(awe64_p002_idle_allocator_test
    tests/p002_native_idle_voice_allocator.cpp)
target_link_libraries(awe64_p002_idle_allocator_test PRIVATE awe64_p002_idle_allocator)
add_test(NAME p002_native_original_win95_idle_mono_voice_allocator
         COMMAND awe64_p002_idle_allocator_test)
target_link_libraries(awe64_p002_bound_voices PUBLIC awe64_p002_idle_allocator)


# P002 original Win95 SBAWE.VXD mode-0 mono dynamic selection PROBE.
# Requires complete live original driver + chip register snapshots.
# Selection returns physical voice; actual I/O and voice steal not invented.
add_library(awe64_p002_vxd_mono_priority STATIC
    src/creative_compat/CreativeVxdMonoVoiceRanking.cpp)
target_include_directories(awe64_p002_vxd_mono_priority PUBLIC src/creative_compat)
target_compile_features(awe64_p002_vxd_mono_priority PUBLIC cxx_std_23)
target_link_libraries(awe64_p002_vxd_mono_priority
    PUBLIC awe64_p002_idle_allocator awe64_p002_register_candidate)
target_link_libraries(awe64_p002_bound_voices PUBLIC awe64_p002_vxd_mono_priority)
add_executable(awe64_p002_vxd_mono_priority_test
    tests/p002_native_vxd_mono_ranking.cpp)
target_link_libraries(awe64_p002_vxd_mono_priority_test
    PRIVATE awe64_p002_vxd_mono_priority)
add_test(NAME p002_native_original_vxd_mono_voice_priority
         COMMAND awe64_p002_vxd_mono_priority_test)


# P002 independently derived forced original VXD group-voice termination.
# True original per-group/voice flags required; NOT general MIDI NoteOff.
add_library(awe64_p002_vxd_voice_termination STATIC
    src/creative_compat/CreativeVxdVoiceTermination.cpp)
target_include_directories(awe64_p002_vxd_voice_termination
    PUBLIC src/creative_compat)
target_compile_features(awe64_p002_vxd_voice_termination PUBLIC cxx_std_23)
target_link_libraries(awe64_p002_vxd_voice_termination
    PUBLIC awe64_p002_vxd_mono_priority)
add_executable(awe64_p002_vxd_voice_termination_test
    tests/p002_native_vxd_voice_termination.cpp)
target_link_libraries(awe64_p002_vxd_voice_termination_test
    PRIVATE awe64_p002_vxd_voice_termination)
add_test(NAME p002_original_vxd_explicit_group_voice_termination
         COMMAND awe64_p002_vxd_voice_termination_test)


# P002: execute ONLY original-VXD-proven typed voice hardware port pairs
# on the pinned, ORIGINAL unmodified fork chip; input is independent MIDI/
# original-bank-derived commands, not a decoded original register trace.
add_library(awe64_p002_real_chip_port_bridge STATIC
    src/creative_compat/CreativeChipPortBridge.cpp)
target_include_directories(awe64_p002_real_chip_port_bridge
    PUBLIC src/creative_compat)
target_compile_features(awe64_p002_real_chip_port_bridge PUBLIC cxx_std_23)
target_link_libraries(awe64_p002_real_chip_port_bridge
    PUBLIC awe64_fork_chip awe64_p002_vxd_voice_termination)
add_executable(awe64_p002_real_chip_port_bridge_test
    tests/p002_native_real_chip_port_bridge.cpp)
target_link_libraries(awe64_p002_real_chip_port_bridge_test
    PRIVATE awe64_p002_real_chip_port_bridge
            awe64_p002_pitch_candidate
            awe64_p002_original_controller_volume)
add_test(NAME p002_real_original_dsp_vxd_register_execution
         COMMAND awe64_p002_real_chip_port_bridge_test)


# Actual installed Win95 SBAWE.VXD LCOD 0x1B4A/0x1C19 case5/case6
# sample-address patch; explicit driver mode, no guessed NoteOn flags.
add_library(awe64_p002_vxd_sample_address STATIC
    src/creative_compat/CreativeVxdSampleAddress.cpp)
target_include_directories(awe64_p002_vxd_sample_address PUBLIC src/creative_compat)
target_compile_features(awe64_p002_vxd_sample_address PUBLIC cxx_std_23)
target_link_libraries(awe64_p002_vxd_sample_address
    PUBLIC awe64_p002_sbk_layers awe64_p002_vxd_mono_priority)
add_executable(awe64_p002_vxd_sample_address_test
    tests/p002_native_vxd_sample_address.cpp)
target_link_libraries(awe64_p002_vxd_sample_address_test
    PRIVATE awe64_p002_vxd_sample_address)
add_test(NAME p002_original_vxd_sample_address_case5_case6
    COMMAND awe64_p002_vxd_sample_address_test)
target_link_libraries(awe64_p002_real_chip_port_bridge
    PUBLIC awe64_p002_vxd_sample_address)


# Original Windows95 SBAWE.VXD LCOD +0x6072..+0x6185 MIDI note release.
# Separate from forced group choke: genuine sustain gate, both envelope
# release registers and conditional preserved-byte CSL/PSST update.
# Already-encoded driver per-voice envelope release rates are caller inputs.
add_library(awe64_p002_vxd_note_release STATIC
    src/creative_compat/CreativeVxdNoteRelease.cpp)
target_include_directories(awe64_p002_vxd_note_release PUBLIC src/creative_compat)
target_compile_features(awe64_p002_vxd_note_release PUBLIC cxx_std_23)
target_link_libraries(awe64_p002_vxd_note_release
    PUBLIC awe64_p002_vxd_mono_priority)
add_executable(awe64_p002_vxd_note_release_test
    tests/p002_native_vxd_note_release.cpp)
target_link_libraries(awe64_p002_vxd_note_release_test
    PRIVATE awe64_p002_vxd_note_release)
add_test(NAME p002_original_win95_midi_note_release_registers
         COMMAND awe64_p002_vxd_note_release_test)
target_link_libraries(awe64_p002_real_chip_port_bridge
    PUBLIC awe64_p002_vxd_note_release)


# Original Win95 SBAWE.VXD +0x5BE4..+0x5CA4 first seven per-voice
# NOTE START registers: VTFT, CVCF, ATKHLDV, LFO1VAL, ATKHLD,
# DCYSUS and LFO2VAL. Require already encoded original driver record.
add_library(awe64_p002_vxd_voice_envelope_prelude STATIC
    src/creative_compat/CreativeVxdVoiceEnvelopePrelude.cpp)
target_include_directories(awe64_p002_vxd_voice_envelope_prelude
    PUBLIC src/creative_compat)
target_compile_features(awe64_p002_vxd_voice_envelope_prelude PUBLIC cxx_std_23)
target_link_libraries(awe64_p002_vxd_voice_envelope_prelude
    PUBLIC awe64_p002_vxd_mono_priority)
add_executable(awe64_p002_vxd_voice_envelope_prelude_test
    tests/p002_native_vxd_voice_envelope_prelude.cpp)
target_link_libraries(awe64_p002_vxd_voice_envelope_prelude_test
    PRIVATE awe64_p002_vxd_voice_envelope_prelude awe64_p002_sbk_layers)
add_test(NAME p002_original_creative_vxd_note_start_envelope_prelude
    COMMAND awe64_p002_vxd_voice_envelope_prelude_test)
target_link_libraries(awe64_p002_real_chip_port_bridge
    PUBLIC awe64_p002_vxd_voice_envelope_prelude)
# Original 1MGM instrument generator34 now uses the same proven
# PCOD attack helper as the existing P002 envelope prelude.
target_link_libraries(awe64_p002_bound_voices
    PUBLIC awe64_p002_vxd_voice_envelope_prelude)

# Format-2 original Creative ROM-only 148-byte descriptor note-start stage.
# Existing volume/attack maths are linked; no second bank parser or chip.
add_library(awe64_p002_sf2_voice_descriptor STATIC
    src/creative_compat/CreativeSf2VoiceDescriptor.cpp)
target_include_directories(awe64_p002_sf2_voice_descriptor
    PUBLIC src/creative_compat)
target_compile_features(awe64_p002_sf2_voice_descriptor PUBLIC cxx_std_23)
target_link_libraries(awe64_p002_sf2_voice_descriptor
    PUBLIC awe64_p002_vxd_voice_envelope_prelude
           awe64_p002_original_controller_volume
           awe64_p002_sbk_layers)
add_executable(awe64_p002_sf2_voice_descriptor_test
    tests/p002_native_sf2_voice_descriptor.cpp)
target_link_libraries(awe64_p002_sf2_voice_descriptor_test
    PRIVATE awe64_p002_sf2_voice_descriptor)
# Reuse the descriptor stage in the pre-existing native WinMM→ROM plan
# test, so the entire source-backed SF2 note-start path is executed.
target_link_libraries(awe64_p002_rom_note_plan_test
    PRIVATE awe64_p002_sf2_voice_descriptor)
# Apply original SF2 DATA3/IP words on the pinned P001 chip via the
# pre-existing transactional VXD port bridge, without modifying DSP.
target_link_libraries(awe64_p002_real_chip_port_bridge_test
    PRIVATE awe64_p002_sf2_voice_descriptor)
add_test(NAME p002_original_sf2_148byte_voice_descriptor
    COMMAND awe64_p002_sf2_voice_descriptor_test)

# Actual original Win95 SBAWE.VXD 18 contiguous typed NOTE START OUTs:
# reset VTFT/CVCF, envelope/LFO prelude, then DATA3 IP and IFATN.
# This is a bounded verified register phase, not a full voice allocator.
add_library(awe64_p002_sf2_note_start_ports STATIC
    src/creative_compat/CreativeSf2NoteStartPorts.cpp)
target_include_directories(awe64_p002_sf2_note_start_ports
    PUBLIC src/creative_compat)
target_compile_features(awe64_p002_sf2_note_start_ports PUBLIC cxx_std_23)
target_link_libraries(awe64_p002_sf2_note_start_ports
    PUBLIC awe64_p002_sf2_voice_descriptor
           awe64_p002_vxd_voice_envelope_prelude)
add_executable(awe64_p002_sf2_note_start_ports_test
    tests/p002_native_sf2_note_start_ports.cpp)
target_link_libraries(awe64_p002_sf2_note_start_ports_test
    PRIVATE awe64_p002_sf2_note_start_ports)
add_test(NAME p002_original_sf2_18_typed_note_start_ports
    COMMAND awe64_p002_sf2_note_start_ports_test)
# The existing source-pinned chip bridge executes these EXACT 18 typed OUTs.
target_link_libraries(awe64_p002_real_chip_port_bridge_test
    PRIVATE awe64_p002_sf2_note_start_ports)
# Source-disassembled original LCOD+0x5CCF..+0x5EE6: following 16
# contiguous typed OUTs (PEFE/FMMOD/TREMFRQ/FM2FRQ2/ENVVAL/ENVVOL
# plus source PTRX/CPF resets), not an inferred generic NoteOn.
add_library(awe64_p002_sf2_note_continuation STATIC
    src/creative_compat/CreativeSf2NoteContinuationPorts.cpp)
target_include_directories(awe64_p002_sf2_note_continuation
    PUBLIC src/creative_compat)
target_compile_features(awe64_p002_sf2_note_continuation PUBLIC cxx_std_23)
target_link_libraries(awe64_p002_sf2_note_continuation
    PUBLIC awe64_p002_sf2_note_start_ports)
add_executable(awe64_p002_sf2_note_continuation_test
    tests/p002_native_sf2_note_continuation_ports.cpp)
target_link_libraries(awe64_p002_sf2_note_continuation_test
    PRIVATE awe64_p002_sf2_note_continuation)
add_test(NAME p002_original_sf2_next_16_typed_outs
    COMMAND awe64_p002_sf2_note_continuation_test)
# Both original segments are executed on the unchanged real chip core.
target_link_libraries(awe64_p002_real_chip_port_bridge_test
    PRIVATE awe64_p002_sf2_note_continuation)
# Actual remaining 22 original typed OUTs: decoded source ROM sample
# geometry, CCCA/Q, Z1/Z2, VTFT/CVCF, PTRX, CPF and DCYSUSV trigger.
# Only two VXD device/voice state targets are explicit verified inputs.
add_library(awe64_p002_sf2_final_note_ports STATIC
    src/creative_compat/CreativeSf2NoteFinalPorts.cpp)
target_include_directories(awe64_p002_sf2_final_note_ports
    PUBLIC src/creative_compat)
target_compile_features(awe64_p002_sf2_final_note_ports PUBLIC cxx_std_23)
target_link_libraries(awe64_p002_sf2_final_note_ports
    PUBLIC awe64_p002_sf2_note_continuation
           awe64_p002_vxd_sample_address)
add_executable(awe64_p002_sf2_final_note_ports_test
    tests/p002_native_sf2_note_final_ports.cpp)
target_link_libraries(awe64_p002_sf2_final_note_ports_test
    PRIVATE awe64_p002_sf2_final_note_ports)
add_test(NAME p002_original_sf2_final_22_typed_noteon_outs
    COMMAND awe64_p002_sf2_final_note_ports_test)
target_link_libraries(awe64_p002_real_chip_port_bridge_test
    PRIVATE awe64_p002_sf2_final_note_ports)
# Original Win95 SF2 full one-note batch: all allocator/stop OUTs
# first, then 56 actual note-start OUTs per voice. One-voice=61 writes,
# two-voice=122, up to original six; source operands remain explicit.
add_library(awe64_p002_sf2_full_noteon STATIC
    src/creative_compat/CreativeSf2FullNoteOnPorts.cpp)
target_include_directories(awe64_p002_sf2_full_noteon
    PUBLIC src/creative_compat)
target_compile_features(awe64_p002_sf2_full_noteon PUBLIC cxx_std_23)
target_link_libraries(awe64_p002_sf2_full_noteon
    PUBLIC awe64_p002_sf2_final_note_ports)
add_executable(awe64_p002_sf2_full_noteon_test
    tests/p002_native_sf2_full_noteon.cpp)
target_link_libraries(awe64_p002_sf2_full_noteon_test
    PRIVATE awe64_p002_sf2_full_noteon)
add_test(NAME p002_original_sf2_complete_noteon_order
    COMMAND awe64_p002_sf2_full_noteon_test)
# Actual unchanged 86Box EMU8000 execution; prevalidate the ENTIRE
# source-derived 61/122 OUT batch before a first chip write.
add_library(awe64_p002_sf2_full_noteon_chip STATIC
    src/creative_compat/CreativeSf2FullNoteOnChip.cpp)
target_include_directories(awe64_p002_sf2_full_noteon_chip
    PUBLIC src/creative_compat)
target_compile_features(awe64_p002_sf2_full_noteon_chip PUBLIC cxx_std_23)
target_link_libraries(awe64_p002_sf2_full_noteon_chip
    PUBLIC awe64_p002_sf2_full_noteon awe64_p002_real_chip_port_bridge)
target_link_libraries(awe64_p002_real_chip_port_bridge_test
    PRIVATE awe64_p002_sf2_full_noteon_chip)
# Reconstruct the two remaining source x86 SF2 NoteOn target operands:
# original [channel+0x0e] / [voice+0x0e] / [channel+0x14] arithmetic,
# and LCOD+0x2FA0 private original 16-WORD attack-128 lookup.
add_library(awe64_p002_sf2_original_targets STATIC
    src/creative_compat/CreativeSf2OriginalTargets.cpp)
target_include_directories(awe64_p002_sf2_original_targets
    PUBLIC src/creative_compat)
target_compile_features(awe64_p002_sf2_original_targets PUBLIC cxx_std_23)
target_link_libraries(awe64_p002_sf2_original_targets
    PUBLIC awe64_p002_sf2_full_noteon)
add_executable(awe64_p002_sf2_original_targets_test
    tests/p002_native_sf2_original_targets.cpp)
target_link_libraries(awe64_p002_sf2_original_targets_test
    PRIVATE awe64_p002_sf2_original_targets)
add_test(NAME p002_original_sf2_x86_final_targets
    COMMAND awe64_p002_sf2_original_targets_test)

# Source-verified native VXD memory image: thirty 0x20-byte physical
# voices from 0x42, sixteen 0x24-byte MIDI channel slots from 0x442,
# genuine '1MGM' ROM identity at 0x158e. Resolve exact target operands
# from this SAME immutable source state snapshot for every NoteOn layer.
add_library(awe64_p002_sf2_vxd_state_snapshot STATIC
    src/creative_compat/CreativeSf2VxdStateSnapshot.cpp)
target_include_directories(awe64_p002_sf2_vxd_state_snapshot
    PUBLIC src/creative_compat)
target_compile_features(awe64_p002_sf2_vxd_state_snapshot PUBLIC cxx_std_23)
target_link_libraries(awe64_p002_sf2_vxd_state_snapshot
    PUBLIC awe64_p002_sf2_original_targets awe64_p002_sf2_full_noteon_chip)
add_executable(awe64_p002_sf2_vxd_state_snapshot_test
    tests/p002_native_sf2_vxd_state_snapshot.cpp)
target_link_libraries(awe64_p002_sf2_vxd_state_snapshot_test
    PRIVATE awe64_p002_sf2_vxd_state_snapshot)
add_test(NAME p002_original_sf2_native_vxd_state_snapshot
    COMMAND awe64_p002_sf2_vxd_state_snapshot_test)
# First actual non-silent DIGITAL PCM rendered from ORIGINAL P002 SF2
# NoteOn register engine on immutable pinned EMU8000 using SYNTHETIC
# ROM only. This is not Win95 original-driver/ROM audio parity.
add_executable(awe64_p002_original_sf2_dry_pcm_test
    tests/p002_native_sf2_dry_pcm.cpp)
target_link_libraries(awe64_p002_original_sf2_dry_pcm_test
    PRIVATE awe64_p002_sf2_vxd_state_snapshot)
add_test(NAME p002_source_original_sf2_native_digital_pcm
    COMMAND awe64_p002_original_sf2_dry_pcm_test)
target_link_libraries(awe64_p002_real_chip_port_bridge_test
    PRIVATE awe64_p002_sf2_vxd_state_snapshot)

# Optional *actual* original 1996 x86 PORT TRACE differential. No
# copyrighted original descriptor/trace is ever embedded in public builds.
add_executable(awe64_p002_sf2_note_start_prefix_oracle
    tests/p002_sf2_note_start_prefix_oracle.cpp)
target_link_libraries(awe64_p002_sf2_note_start_prefix_oracle
    PRIVATE awe64_p002_sf2_note_start_ports)
if(DEFINED ENV{AWE64_P002_PRIVATE_SF2_HANDOFF} AND
   EXISTS "$ENV{AWE64_P002_PRIVATE_SF2_HANDOFF}")
    add_test(NAME p002_private_original_sf2_18_ordered_outs
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/p002_private_sf2_note_start_prefix_differential.py"
                --handoff "$ENV{AWE64_P002_PRIVATE_SF2_HANDOFF}"
                --executable "$<TARGET_FILE:awe64_p002_sf2_note_start_prefix_oracle>")
endif()

# The original Creative source-bank handoff is PRIVATE and never committed.
# If configured in the runner/Test Lab environment, compare the native
# encoded descriptors against ALL provided actual original 148-byte outputs.
# Without this private fixture, no private/original differential PASS is claimed.
add_executable(awe64_p002_sf2_descriptor_oracle
    tests/p002_sf2_descriptor_oracle.cpp)
target_link_libraries(awe64_p002_sf2_descriptor_oracle
    PRIVATE awe64_p002_sf2_voice_descriptor)
if(DEFINED ENV{AWE64_P002_PRIVATE_SF2_HANDOFF} AND
   EXISTS "$ENV{AWE64_P002_PRIVATE_SF2_HANDOFF}")
    add_test(NAME p002_private_original_sf2_148byte_differential
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/p002_private_sf2_descriptor_differential.py"
                --handoff "$ENV{AWE64_P002_PRIVATE_SF2_HANDOFF}"
                --executable "$<TARGET_FILE:awe64_p002_sf2_descriptor_oracle>")
    if(DEFINED ENV{AWE64_P002_PRIVATE_VXD} AND
       EXISTS "$ENV{AWE64_P002_PRIVATE_VXD}")
        # Actual SOURCE-LOCKED original LCOD+0x2D88 59 signed words
        # versus genuine original x86 pre-NoteOn 148-byte handoff records.
        # Never copy the VXD or its template to public CI artifacts.
        add_test(NAME p002_private_original_sf2_59word_bank_builder
            COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/p002_private_sf2_bank_builder_differential.py"
                    --handoff "$ENV{AWE64_P002_PRIVATE_SF2_HANDOFF}"
                    --private-vxd "$ENV{AWE64_P002_PRIVATE_VXD}"
                    --executable "$<TARGET_FILE:awe64_p002_sf2_descriptor_oracle>")
    endif()
endif()

# Optional private all-192000-sample-rate native SF2 pitch differential.
# Uses ONLY a user-owned hash-locked original SBAWE.VXD, never public assets.
# Not gated on the separate original bank handoff; VXD tables suffice.
if(DEFINED ENV{AWE64_P002_PRIVATE_VXD} AND
   EXISTS "$ENV{AWE64_P002_PRIVATE_VXD}")
    add_test(NAME p002_private_original_sf2_pitch_192000_rates
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/p002_private_sf2_pitch_matrix_differential.py"
                --private-vxd "$ENV{AWE64_P002_PRIVATE_VXD}"
                --executable "$<TARGET_FILE:awe64_p002_sf2_descriptor_oracle>")
endif()

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
        src/vst3/P002PrivateNoteFixture.h
        src/vst3/P002PrivateNoteFixture.cpp
        src/vst3/P001Factory.cpp
        "${AWE64_VST3_SDK_ROOT}/public.sdk/source/vst/vstsinglecomponenteffect.cpp"
        "${AWE64_VST3_SDK_ROOT}/public.sdk/source/main/dllmain.cpp")
    target_include_directories(AWE64_Gold_P001 PRIVATE
        "${AWE64_VST3_SDK_ROOT}")
    target_link_libraries(AWE64_Gold_P001 PRIVATE sdk awe64_fork_chip
        awe64_p002_sf2_vxd_state_snapshot)
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
