CXX ?= clang++
RAYLIB_PREFIX ?= $(shell brew --prefix raylib 2>/dev/null)
RAYLIB_HEADER := $(RAYLIB_PREFIX)/include/raylib.h
RAYLIB_STATIC := $(RAYLIB_PREFIX)/lib/libraylib.a
RAYLIB_REQUIRED_VERSION := 6.0
RAYLIB_VERSION := $(shell awk \
	'$$2 == "RAYLIB_VERSION" { \
		gsub(/"/, "", $$3); print $$3 \
	}' "$(RAYLIB_HEADER)" 2>/dev/null)
RAYLIB_LICENSE_SHA256 := $(shell shasum -a 256 \
	LICENSES/Zlib-raylib.txt 2>/dev/null | awk '{print $$1}')
RAYLIB_REQUIRED_LICENSE_SHA256 := \
	882a5a819cf562aa3583aae3af3f2211dda15c63de9fc8cc4b399a2f9e78d799
RAYLIB_MACOS_MINS := $(shell otool -l "$(RAYLIB_STATIC)" 2>/dev/null | \
	awk '/minos/{print $$2}' | LC_ALL=C sort -u)
RAYLIB_MACOS_MIN := $(firstword $(RAYLIB_MACOS_MINS))
RAYLIB_STATIC_MEMBER_COUNT := $(shell ar -t "$(RAYLIB_STATIC)" 2>/dev/null | \
	awk '!/^__\.SYMDEF/{count++} END{print count+0}')
RAYLIB_STATIC_MINOS_COUNT := $(shell otool -l "$(RAYLIB_STATIC)" 2>/dev/null | \
	awk '/minos/{count++} END{print count+0}')
MACOS_MIN ?= $(RAYLIB_MACOS_MIN)
MACOS_TARGET_FLAG := -mmacosx-version-min=$(MACOS_MIN)

TARGET := build/Tanks3D
OBJECT_DIR := build/obj
APP := build/Tanks3D.app
APP_EXECUTABLE := $(APP)/Contents/MacOS/Tanks3D
APP_RESOURCES := $(APP)/Contents/Resources
RELEASE_SCREENSHOT_SMOKE_DIR := build/release-screenshot-smoke
RELEASE_SCREENSHOT_SMOKE_FILE := \
	$(RELEASE_SCREENSHOT_SMOKE_DIR)/tank-showcase.png
RELEASE_PERFORMANCE_SMOKE_DIR := build/release-performance-smoke
RELEASE_PERFORMANCE_SMOKE_FILE := \
	$(RELEASE_PERFORMANCE_SMOKE_DIR)/performance-log-v2.json
RELEASE_PERFORMANCE_CAPABILITY_SCHEMA := \
	tanks3d-release-performance-capabilities-v1
RELEASE_PERFORMANCE_TELEMETRY_SCHEMA := tanks3d-performance-log-v2
RELEASE_PERFORMANCE_CAPABILITY_CONTRACT := \
	tests/expected_release_performance_capabilities.json
RELEASE_PERFORMANCE_CAPABILITY_CONTRACT_SHA256 := $(shell shasum -a 256 \
	$(RELEASE_PERFORMANCE_CAPABILITY_CONTRACT) 2>/dev/null | awk '{print $$1}')
RELEASE_PERFORMANCE_REQUIRED_CONTRACT_SHA256 := \
	5137950da46fa11ee6d5ff60fafe67e83c4c0aacfb5fc83f2b0ce5f74afcfe1c
RELEASE_PERFORMANCE_CAPABILITY_OUTPUT := \
	build/tests/release-performance-capabilities.json
RELEASE_PERFORMANCE_CAPABILITY_STDERR := \
	build/tests/release-performance-capabilities.stderr
COMMAND_SIDE_EFFECT_DISPATCH_SOURCE := \
	src/app/command_side_effect_dispatch.cpp
INPUT_ADAPTER_SOURCE := src/app/input_adapter.cpp
ATOMIC_OUTPUT_FILE_SOURCE := src/app/atomic_output_file.cpp
RELEASE_PERFORMANCE_LOG_SOURCE := src/app/release_performance_log.cpp
RELEASE_PERFORMANCE_CAPABILITY_SOURCE := \
	src/app/release_performance_capabilities.cpp
RELEASE_SCREENSHOT_FILE_SOURCE := src/app/release_screenshot_file.cpp
SHELL_CANCELLATION_PRESENTATION_SOURCE := \
	src/app/shell_cancellation_presentation.cpp
SHELL_MAP_CORE_PRESENTATION_SOURCE := \
	src/app/shell_map_core_presentation.cpp
SHELL_TANK_PRESENTATION_SOURCE := src/app/shell_tank_presentation.cpp
APP_SOURCES := $(COMMAND_SIDE_EFFECT_DISPATCH_SOURCE) \
	$(INPUT_ADAPTER_SOURCE) \
	$(ATOMIC_OUTPUT_FILE_SOURCE) \
	$(RELEASE_PERFORMANCE_CAPABILITY_SOURCE) \
	$(RELEASE_PERFORMANCE_LOG_SOURCE) \
	$(RELEASE_SCREENSHOT_FILE_SOURCE) \
	$(SHELL_CANCELLATION_PRESENTATION_SOURCE) \
	$(SHELL_MAP_CORE_PRESENTATION_SOURCE) \
	$(SHELL_TANK_PRESENTATION_SOURCE)
BONUS_SYSTEM_SOURCE := src/game/bonus_system.cpp
COMBAT_SYSTEM_SOURCE := src/game/combat_system.cpp
ENEMY_SYSTEM_SOURCE := src/game/enemy_system.cpp
PLAYER_SYSTEM_SOURCE := src/game/player_system.cpp
SETTLEMENT_SYSTEM_SOURCE := src/game/settlement_system.cpp
STAGE_GENERATOR_SOURCE := src/game/stage_generator.cpp
STAGE_MAP_SOURCE := src/game/stage_map.cpp
PLATFORM_SOURCES := src/platform/macos_gamepad_backend.mm
PLATFORM_HEADERS := src/platform/gamepad_backend.h \
	src/platform/gamepad_event_accumulator.h
SOURCES := src/main.cpp $(APP_SOURCES) \
	$(BONUS_SYSTEM_SOURCE) $(COMBAT_SYSTEM_SOURCE) $(ENEMY_SYSTEM_SOURCE) \
	$(PLAYER_SYSTEM_SOURCE) $(SETTLEMENT_SYSTEM_SOURCE) \
	$(STAGE_GENERATOR_SOURCE) $(STAGE_MAP_SOURCE)
OBJECTS := $(patsubst src/%.cpp,$(OBJECT_DIR)/%.o,$(SOURCES)) \
	$(patsubst src/%.mm,$(OBJECT_DIR)/%.o,$(PLATFORM_SOURCES))
DEPFILES := $(OBJECTS:.o=.d)
AUDIO_HEADERS := src/audio/audio_cue.h src/audio/audio_output.h
APP_HEADERS := src/app/command_side_effect_dispatch.h \
	src/app/command_side_effect_sink.h src/app/presentation_values.h \
	src/app/input_adapter.h \
	src/app/atomic_output_file.h \
	src/app/release_performance_capabilities.h \
	src/app/release_performance_log.h \
	src/app/release_performance_options.h \
	src/app/release_screenshot_file.h \
	src/app/release_screenshot_options.h \
	src/app/shell_cancellation_presentation.h \
	src/app/shell_map_core_presentation.h \
	src/app/shell_tank_presentation.h
CORE_HEADERS := src/core/coordinates.h src/core/gameplay_rules.h \
	src/core/nation.h
GAME_HEADERS := src/game/bonus_rules.h src/game/bonus_system.h \
	src/game/combat_system.h src/game/enemy_system.h src/game/entities.h \
	src/game/game_event.h src/game/player_system.h \
	src/game/settlement_system.h src/game/stage_generator.h \
	src/game/stage_map.h
PURE_SOURCES := $(BONUS_SYSTEM_SOURCE) $(COMBAT_SYSTEM_SOURCE) \
	$(ENEMY_SYSTEM_SOURCE) $(PLAYER_SYSTEM_SOURCE) \
	$(SETTLEMENT_SYSTEM_SOURCE) \
	$(STAGE_GENERATOR_SOURCE) \
	$(STAGE_MAP_SOURCE)
PURE_HEADERS := $(CORE_HEADERS) $(GAME_HEADERS)
PRODUCTION_HEADERS := src/wwii_tank_model.h src/tank_assets.h src/battle_fx.h \
	src/bonus_assets.h $(AUDIO_HEADERS) $(APP_HEADERS) $(PURE_HEADERS) \
	$(PLATFORM_HEADERS) src/environment_assets.h src/post_process.h
TEST_FILES := tests/test_support.h tests/stage_layout_expectations.h \
	tests/stage_map_expectations.h tests/self_tests.inl
BUNDLE_RESOURCE_MANIFEST := tests/expected_bundle_resources.txt
DIST_RESOURCE_MANIFEST := tests/expected_dist_bundle_resources.txt
DIST_VERIFY_SCRIPT := scripts/verify_macos_dist.sh
DIST_VERIFY_NEGATIVE_TEST := tests/test_macos_dist_verifier.sh
ALPHA_CANDIDATE_BUILD_SCRIPT := scripts/build_alpha_candidate.sh
ALPHA_CANDIDATE_VERIFY_SCRIPT := scripts/verify_alpha_candidate.sh
ALPHA_CANDIDATE_TAGGED_VERIFY_SCRIPT := \
	scripts/verify_tagged_alpha_candidate.sh
TAGGED_CANDIDATE_VERIFIER := scripts/tagged_candidate_verifier.py
TAGGED_CANDIDATE_VERIFIER_TEST := tests/test_tagged_candidate_verifier.py
ALPHA_CANDIDATE_GATE_TEST := tests/test_alpha_candidate_gate.sh
CLEAN_PRESERVATION_TEST := tests/test_clean_preserves_release_outputs.sh
RELEASE_REQUIREMENTS_PROFILE := \
	docs/release-requirements/macos-alpha-v1.json
RELEASE_REQUIREMENTS_PROFILE_V2 := \
	docs/release-requirements/macos-alpha-v2.json
RELEASE_PERFORMANCE_CONTRACT := scripts/release_performance_contract.py
RELEASE_STATUS_VERIFY_SCRIPT := scripts/verify_release_status.py
RELEASE_STATUS_TEST := tests/test_release_status_verifier.py
ALPHA_V2_STATUS_INIT_SCRIPT := scripts/init_alpha_v2_status.py
ALPHA_V2_STATUS_INIT_TEST := tests/test_alpha_v2_status_initializer.py
ALPHA_V2_INTERACTIVE_COMPILER := \
	scripts/compile_alpha_v2_interactive_evidence.py
ALPHA_V2_INTERACTIVE_COMPILER_TEST := \
	tests/test_alpha_v2_interactive_evidence_compiler.py
ALPHA_V2_CLEAN_MAC_QA_PREPARER := \
	scripts/prepare_alpha_v2_clean_mac_qa.py
ALPHA_V2_CLEAN_MAC_QA_PREPARER_TEST := \
	tests/test_alpha_v2_clean_mac_qa_preparer.py
ALPHA_V2_CLEAN_MAC_COLLECTOR := \
	scripts/collect_alpha_v2_clean_mac_qa.sh
ALPHA_V2_CLEAN_MAC_COLLECTOR_TEST := \
	tests/test_alpha_v2_clean_mac_collector.sh
ALPHA_V2_CLEAN_MAC_EVIDENCE_COMPILER := \
	scripts/compile_alpha_v2_clean_mac_evidence.py
ALPHA_V2_CLEAN_MAC_EVIDENCE_COMPILER_TEST := \
	tests/test_alpha_v2_clean_mac_evidence_compiler.py
MEDIA_RECORDING_VALIDATOR := scripts/validate_media_recording.py
MEDIA_RECORDING_VALIDATOR_TEST := tests/test_media_recording_validator.py
MEDIA_RECORDING_FIXTURE := tests/media_recording_fixture.py
RELEASE_PERFORMANCE_QA_RUNNER := scripts/run_release_performance_qa.py
RELEASE_PERFORMANCE_QA_RUNNER_TEST := \
	tests/test_release_performance_runner.py

CORE_TEST_SOURCES := tests/core_coordinates_tests.cpp \
	tests/core_gameplay_rules_tests.cpp
CORE_TEST_TARGETS := $(patsubst tests/%.cpp,build/tests/%,$(CORE_TEST_SOURCES))
BONUS_SYSTEM_TEST_SOURCE := tests/bonus_system_tests.cpp
BONUS_SYSTEM_TEST_TARGET := build/tests/bonus_system_tests
COMBAT_SYSTEM_TEST_SOURCE := tests/combat_system_tests.cpp
COMBAT_SYSTEM_TEST_TARGET := build/tests/combat_system_tests
ENEMY_SYSTEM_TEST_SOURCE := tests/enemy_system_tests.cpp
ENEMY_SYSTEM_TEST_TARGET := build/tests/enemy_system_tests
PLAYER_SYSTEM_TEST_SOURCE := tests/player_system_tests.cpp
PLAYER_SYSTEM_TEST_TARGET := build/tests/player_system_tests
SETTLEMENT_SYSTEM_TEST_SOURCE := tests/settlement_system_tests.cpp
SETTLEMENT_SYSTEM_TEST_TARGET := build/tests/settlement_system_tests
STAGE_GENERATOR_TEST_SOURCE := tests/stage_generator_tests.cpp
STAGE_GENERATOR_TEST_TARGET := build/tests/stage_generator_tests
STAGE_MAP_TEST_SOURCE := tests/stage_map_tests.cpp
STAGE_MAP_TEST_TARGET := build/tests/stage_map_tests
GAME_TEST_SOURCES := tests/game_entities_tests.cpp \
	$(BONUS_SYSTEM_TEST_SOURCE) $(COMBAT_SYSTEM_TEST_SOURCE) \
	$(ENEMY_SYSTEM_TEST_SOURCE) $(PLAYER_SYSTEM_TEST_SOURCE) \
	$(SETTLEMENT_SYSTEM_TEST_SOURCE) \
	$(STAGE_GENERATOR_TEST_SOURCE) $(STAGE_MAP_TEST_SOURCE)
GAME_TEST_TARGETS := $(patsubst tests/%.cpp,build/tests/%,$(GAME_TEST_SOURCES))
RULE_TEST_SOURCES := $(CORE_TEST_SOURCES) $(GAME_TEST_SOURCES)
RULE_TEST_TARGETS := $(CORE_TEST_TARGETS) $(GAME_TEST_TARGETS)
COMMAND_SIDE_EFFECT_DISPATCH_TEST_SOURCE := \
	tests/command_side_effect_dispatch_tests.cpp
COMMAND_SIDE_EFFECT_DISPATCH_TEST_TARGET := \
	build/tests/command_side_effect_dispatch_tests
INPUT_ADAPTER_TEST_SOURCE := tests/input_adapter_tests.cpp
INPUT_ADAPTER_TEST_TARGET := build/tests/input_adapter_tests
ATOMIC_OUTPUT_FILE_TEST_SOURCE := tests/atomic_output_file_tests.cpp
ATOMIC_OUTPUT_FILE_TEST_TARGET := build/tests/atomic_output_file_tests
RELEASE_PERFORMANCE_LOG_TEST_SOURCE := \
	tests/release_performance_log_tests.cpp
RELEASE_PERFORMANCE_LOG_TEST_TARGET := \
	build/tests/release_performance_log_tests
RELEASE_SCREENSHOT_OPTIONS_TEST_SOURCE := \
	tests/release_screenshot_options_tests.cpp
RELEASE_SCREENSHOT_OPTIONS_TEST_TARGET := \
	build/tests/release_screenshot_options_tests
SHELL_TANK_PRESENTATION_TEST_SOURCE := \
	tests/shell_tank_presentation_tests.cpp
SHELL_TANK_PRESENTATION_TEST_TARGET := \
	build/tests/shell_tank_presentation_tests
SHELL_CANCELLATION_PRESENTATION_TEST_SOURCE := \
	tests/shell_cancellation_presentation_tests.cpp
SHELL_CANCELLATION_PRESENTATION_TEST_TARGET := \
	build/tests/shell_cancellation_presentation_tests
SHELL_MAP_CORE_PRESENTATION_TEST_SOURCE := \
	tests/shell_map_core_presentation_tests.cpp
SHELL_MAP_CORE_PRESENTATION_TEST_TARGET := \
	build/tests/shell_map_core_presentation_tests
APP_TEST_TARGETS := $(COMMAND_SIDE_EFFECT_DISPATCH_TEST_TARGET) \
	$(INPUT_ADAPTER_TEST_TARGET) \
	$(ATOMIC_OUTPUT_FILE_TEST_TARGET) \
	$(RELEASE_PERFORMANCE_LOG_TEST_TARGET) \
	$(RELEASE_SCREENSHOT_OPTIONS_TEST_TARGET) \
	$(SHELL_CANCELLATION_PRESENTATION_TEST_TARGET) \
	$(SHELL_MAP_CORE_PRESENTATION_TEST_TARGET) \
	$(SHELL_TANK_PRESENTATION_TEST_TARGET)
COMPILED_RULE_TEST_TARGETS := $(BONUS_SYSTEM_TEST_TARGET) \
	$(COMBAT_SYSTEM_TEST_TARGET) \
	$(ENEMY_SYSTEM_TEST_TARGET) \
	$(PLAYER_SYSTEM_TEST_TARGET) \
	$(SETTLEMENT_SYSTEM_TEST_TARGET) \
	$(STAGE_GENERATOR_TEST_TARGET) \
	$(STAGE_MAP_TEST_TARGET)
HEADER_ONLY_RULE_TEST_TARGETS := $(filter-out \
	$(COMPILED_RULE_TEST_TARGETS),$(RULE_TEST_TARGETS))
RULE_BOUNDARY_CHECK := tests/check_core_boundaries.sh
RENDERER_BRIDGES := tests/nation_renderer_bridge.cpp \
	tests/bonus_renderer_bridge.cpp
RULE_IMPL_OBJECT_DIR := build/tests/obj
RULE_IMPL_OBJECTS := $(patsubst src/%.cpp,$(RULE_IMPL_OBJECT_DIR)/%.o, \
	$(PURE_SOURCES))
RULE_IMPL_DEPFILES := $(RULE_IMPL_OBJECTS:.o=.d)
STAGE_GENERATOR_RULE_OBJECT := \
	$(RULE_IMPL_OBJECT_DIR)/game/stage_generator.o
STAGE_MAP_RULE_OBJECT := $(RULE_IMPL_OBJECT_DIR)/game/stage_map.o
BONUS_SYSTEM_RULE_OBJECT := $(RULE_IMPL_OBJECT_DIR)/game/bonus_system.o
COMBAT_SYSTEM_RULE_OBJECT := $(RULE_IMPL_OBJECT_DIR)/game/combat_system.o
ENEMY_SYSTEM_RULE_OBJECT := $(RULE_IMPL_OBJECT_DIR)/game/enemy_system.o
PLAYER_SYSTEM_RULE_OBJECT := $(RULE_IMPL_OBJECT_DIR)/game/player_system.o
SETTLEMENT_SYSTEM_RULE_OBJECT := \
	$(RULE_IMPL_OBJECT_DIR)/game/settlement_system.o

DEBUG_DIR := build/debug
DEBUG_TARGET := $(DEBUG_DIR)/Tanks3D-debug
DEBUG_OBJECTS := $(patsubst src/%.cpp,$(DEBUG_DIR)/%.o,$(SOURCES)) \
	$(patsubst src/%.mm,$(DEBUG_DIR)/%.o,$(PLATFORM_SOURCES))
DEBUG_DEPFILES := $(DEBUG_OBJECTS:.o=.d)

SANITIZER_DIR := build/sanitize
SANITIZER_TARGET := $(SANITIZER_DIR)/Tanks3D-sanitize
SANITIZER_OBJECTS := $(patsubst src/%.cpp,$(SANITIZER_DIR)/%.o,$(SOURCES)) \
	$(patsubst src/%.mm,$(SANITIZER_DIR)/%.o,$(PLATFORM_SOURCES))
SANITIZER_DEPFILES := $(SANITIZER_OBJECTS:.o=.d)

APP_VERSION := $(shell /usr/libexec/PlistBuddy -c \
	'Print :CFBundleShortVersionString' macos/Info.plist 2>/dev/null)
DIST_CHANNEL ?= alpha.1
DIST_ARCH ?= $(shell lipo -archs "$(RAYLIB_STATIC)" 2>/dev/null)
DIST_MACOS_MIN ?= $(MACOS_MIN)
DIST_DIR := build/dist
DIST_CONFIG_FILE := $(DIST_DIR)/.build-config
DIST_OBJECT_DIR := $(DIST_DIR)/obj
DIST_OBJECTS := $(patsubst src/%.cpp,$(DIST_OBJECT_DIR)/%.o,$(SOURCES)) \
	$(patsubst src/%.mm,$(DIST_OBJECT_DIR)/%.o,$(PLATFORM_SOURCES))
DIST_DEPFILES := $(DIST_OBJECTS:.o=.d)
DIST_TARGET := $(DIST_DIR)/Tanks3D-static
DIST_STAGING_DIR := $(DIST_DIR)/staging
DIST_APP := $(DIST_STAGING_DIR)/Tanks3D.app
DIST_APP_EXECUTABLE := $(DIST_APP)/Contents/MacOS/Tanks3D
DIST_APP_RESOURCES := $(DIST_APP)/Contents/Resources
DIST_APP_STAMP := $(DIST_STAGING_DIR)/.signed
DIST_BASENAME := Tanks3D-$(APP_VERSION)-$(DIST_CHANNEL)-macos-$(DIST_ARCH)-macos$(DIST_MACOS_MIN)
DIST_ARCHIVE := $(DIST_DIR)/$(DIST_BASENAME).zip
DIST_CHECKSUM := $(DIST_ARCHIVE).sha256
ALPHA_CANDIDATE_TAG := v$(APP_VERSION)-$(DIST_CHANNEL)
DIST_SOURCE_COMMIT ?= $(shell git rev-parse --verify HEAD^{commit} 2>/dev/null)
DIST_SOURCE_TAG ?= $(ALPHA_CANDIDATE_TAG)
DIST_IDENTITY_FLAGS := \
	-DTANKS3D_RELEASE_SOURCE_COMMIT=\"$(DIST_SOURCE_COMMIT)\" \
	-DTANKS3D_RELEASE_SOURCE_TAG=\"$(DIST_SOURCE_TAG)\"
ALPHA_CANDIDATE_DIR ?= \
	$(abspath build/release/$(ALPHA_CANDIDATE_TAG))
RELEASE_STATUS_FILE ?= \
	docs/releases/$(ALPHA_CANDIDATE_TAG)-status.json
RELEASE_PERFORMANCE_QA_OUTPUT_DIR ?= \
	$(abspath build/release-evidence/$(ALPHA_CANDIDATE_TAG)/performance)
ALPHA_RELEASE_SCREENSHOT_INPUT_DIR ?= \
	$(abspath build/release-evidence/$(ALPHA_CANDIDATE_TAG)/screenshots)
ALPHA_INTERACTIVE_QA_DIR ?= \
	$(abspath build/release-evidence/$(ALPHA_CANDIDATE_TAG)/interactive)
ALPHA_INTERACTIVE_QA_PLAN ?= \
	$(ALPHA_INTERACTIVE_QA_DIR)/observation-plan.json
ALPHA_INTERACTIVE_QA_OUTPUT_DIR ?= \
	$(ALPHA_INTERACTIVE_QA_DIR)/compiled
CLEAN_MAC_DOWNLOAD_URL ?=
CLEAN_MAC_QA_KIT_DIR ?= \
	$(abspath build/release-evidence/$(ALPHA_CANDIDATE_TAG)/clean-mac-kit)
CLEAN_MAC_INTAKE_DIR ?=
CLEAN_MAC_MEDIA ?=
CLEAN_MAC_REVIEWER ?=
CLEAN_MAC_REVIEWER_SIGNATURE ?=
CLEAN_MAC_REVIEWED_AT_UTC ?=
CLEAN_MAC_REVIEW_NOTES ?=
CLEAN_MAC_RELEASE_NOTE_WORDING_VERIFIED ?=
CLEAN_MAC_EVIDENCE_OUTPUT_DIR ?= \
	$(abspath docs/assets/releases/$(ALPHA_CANDIDATE_TAG)/evidence/clean-mac-compiled)
DIST_COMPILE_FLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror \
	-mmacosx-version-min=$(DIST_MACOS_MIN) $(DIST_IDENTITY_FLAGS)
DIST_LINK_FLAGS := -mmacosx-version-min=$(DIST_MACOS_MIN) \
	-framework Cocoa -framework GameController -framework IOKit \
	-framework OpenGL
DIST_COMPILER_ID := $(shell $(CXX) --version 2>/dev/null | sed -n '1p')
DIST_RAYLIB_SHA256 := $(shell shasum -a 256 "$(RAYLIB_STATIC)" 2>/dev/null | \
	awk '{print $$1}')

COVERAGE_DIR := build/coverage
COVERAGE_TARGET := $(COVERAGE_DIR)/Tanks3D-coverage
COVERAGE_OBJECTS := $(patsubst src/%.cpp,$(COVERAGE_DIR)/%.o,$(SOURCES)) \
	$(patsubst src/%.mm,$(COVERAGE_DIR)/%.o,$(PLATFORM_SOURCES))
COVERAGE_DEPFILES := $(COVERAGE_OBJECTS:.o=.d)
COVERAGE_RAW_PROFILE := $(COVERAGE_DIR)/self-test.profraw
RELEASE_PERFORMANCE_CAPABILITY_COVERAGE_RAW_PROFILE := \
	$(COVERAGE_DIR)/release-performance-capabilities.profraw
RELEASE_PERFORMANCE_CAPABILITY_COVERAGE_OUTPUT := \
	$(COVERAGE_DIR)/release-performance-capabilities.json
RELEASE_PERFORMANCE_CAPABILITY_COVERAGE_STDERR := \
	$(COVERAGE_DIR)/release-performance-capabilities.stderr
COVERAGE_PROFILE := $(COVERAGE_DIR)/self-test.profdata
RULE_COVERAGE_TARGETS := $(patsubst tests/%.cpp,$(COVERAGE_DIR)/%, \
	$(RULE_TEST_SOURCES))
STAGE_GENERATOR_COVERAGE_TARGET := \
	$(COVERAGE_DIR)/stage_generator_tests
STAGE_MAP_COVERAGE_TARGET := $(COVERAGE_DIR)/stage_map_tests
COMBAT_SYSTEM_COVERAGE_TARGET := $(COVERAGE_DIR)/combat_system_tests
ENEMY_SYSTEM_COVERAGE_TARGET := $(COVERAGE_DIR)/enemy_system_tests
PLAYER_SYSTEM_COVERAGE_TARGET := $(COVERAGE_DIR)/player_system_tests
BONUS_SYSTEM_COVERAGE_TARGET := $(COVERAGE_DIR)/bonus_system_tests
SETTLEMENT_SYSTEM_COVERAGE_TARGET := \
	$(COVERAGE_DIR)/settlement_system_tests
COMPILED_RULE_COVERAGE_TARGETS := $(BONUS_SYSTEM_COVERAGE_TARGET) \
	$(COMBAT_SYSTEM_COVERAGE_TARGET) \
	$(ENEMY_SYSTEM_COVERAGE_TARGET) \
	$(PLAYER_SYSTEM_COVERAGE_TARGET) \
	$(SETTLEMENT_SYSTEM_COVERAGE_TARGET) \
	$(STAGE_GENERATOR_COVERAGE_TARGET) \
	$(STAGE_MAP_COVERAGE_TARGET)
HEADER_ONLY_RULE_COVERAGE_TARGETS := $(filter-out \
	$(COMPILED_RULE_COVERAGE_TARGETS),$(RULE_COVERAGE_TARGETS))
STAGE_GENERATOR_COVERAGE_OBJECT := \
	$(COVERAGE_DIR)/game/stage_generator.o
STAGE_MAP_COVERAGE_OBJECT := $(COVERAGE_DIR)/game/stage_map.o
BONUS_SYSTEM_COVERAGE_OBJECT := $(COVERAGE_DIR)/game/bonus_system.o
COMBAT_SYSTEM_COVERAGE_OBJECT := $(COVERAGE_DIR)/game/combat_system.o
ENEMY_SYSTEM_COVERAGE_OBJECT := $(COVERAGE_DIR)/game/enemy_system.o
PLAYER_SYSTEM_COVERAGE_OBJECT := $(COVERAGE_DIR)/game/player_system.o
SETTLEMENT_SYSTEM_COVERAGE_OBJECT := \
	$(COVERAGE_DIR)/game/settlement_system.o
COMPILED_COVERAGE_TEST_OBJECT_DIR := $(COVERAGE_DIR)/tests
BONUS_SYSTEM_COVERAGE_TEST_OBJECT := \
	$(COMPILED_COVERAGE_TEST_OBJECT_DIR)/bonus_system_tests.o
COMBAT_SYSTEM_COVERAGE_TEST_OBJECT := \
	$(COMPILED_COVERAGE_TEST_OBJECT_DIR)/combat_system_tests.o
ENEMY_SYSTEM_COVERAGE_TEST_OBJECT := \
	$(COMPILED_COVERAGE_TEST_OBJECT_DIR)/enemy_system_tests.o
PLAYER_SYSTEM_COVERAGE_TEST_OBJECT := \
	$(COMPILED_COVERAGE_TEST_OBJECT_DIR)/player_system_tests.o
STAGE_GENERATOR_COVERAGE_TEST_OBJECT := \
	$(COMPILED_COVERAGE_TEST_OBJECT_DIR)/stage_generator_tests.o
SETTLEMENT_SYSTEM_COVERAGE_TEST_OBJECT := \
	$(COMPILED_COVERAGE_TEST_OBJECT_DIR)/settlement_system_tests.o
COMPILED_COVERAGE_TEST_DEPFILES := \
	$(BONUS_SYSTEM_COVERAGE_TEST_OBJECT:.o=.d) \
	$(COMBAT_SYSTEM_COVERAGE_TEST_OBJECT:.o=.d) \
	$(ENEMY_SYSTEM_COVERAGE_TEST_OBJECT:.o=.d) \
	$(PLAYER_SYSTEM_COVERAGE_TEST_OBJECT:.o=.d) \
	$(STAGE_GENERATOR_COVERAGE_TEST_OBJECT:.o=.d) \
	$(SETTLEMENT_SYSTEM_COVERAGE_TEST_OBJECT:.o=.d)
COMMAND_SIDE_EFFECT_DISPATCH_COVERAGE_TARGET := \
	$(COVERAGE_DIR)/command_side_effect_dispatch_tests
INPUT_ADAPTER_COVERAGE_TARGET := \
	$(COVERAGE_DIR)/input_adapter_tests
INPUT_ADAPTER_COVERAGE_OBJECT := \
	$(COVERAGE_DIR)/app/input_adapter.o
INPUT_ADAPTER_COVERAGE_TEST_OBJECT := \
	$(COMPILED_COVERAGE_TEST_OBJECT_DIR)/input_adapter_tests.o
ATOMIC_OUTPUT_FILE_COVERAGE_TARGET := \
	$(COVERAGE_DIR)/atomic_output_file_tests
ATOMIC_OUTPUT_FILE_COVERAGE_OBJECT := \
	$(COVERAGE_DIR)/app/atomic_output_file.o
ATOMIC_OUTPUT_FILE_COVERAGE_TEST_OBJECT := \
	$(COMPILED_COVERAGE_TEST_OBJECT_DIR)/atomic_output_file_tests.o
RELEASE_PERFORMANCE_LOG_COVERAGE_TARGET := \
	$(COVERAGE_DIR)/release_performance_log_tests
RELEASE_PERFORMANCE_LOG_COVERAGE_OBJECT := \
	$(COVERAGE_DIR)/app/release_performance_log.o
RELEASE_PERFORMANCE_LOG_COVERAGE_TEST_OBJECT := \
	$(COMPILED_COVERAGE_TEST_OBJECT_DIR)/release_performance_log_tests.o
RELEASE_SCREENSHOT_OPTIONS_COVERAGE_TARGET := \
	$(COVERAGE_DIR)/release_screenshot_options_tests
RELEASE_SCREENSHOT_FILE_COVERAGE_OBJECT := \
	$(COVERAGE_DIR)/app/release_screenshot_file.o
RELEASE_SCREENSHOT_OPTIONS_COVERAGE_TEST_OBJECT := \
	$(COMPILED_COVERAGE_TEST_OBJECT_DIR)/release_screenshot_options_tests.o
COMMAND_SIDE_EFFECT_DISPATCH_COVERAGE_OBJECT := \
	$(COVERAGE_DIR)/app/command_side_effect_dispatch.o
COMMAND_SIDE_EFFECT_DISPATCH_COVERAGE_TEST_OBJECT := \
	$(COMPILED_COVERAGE_TEST_OBJECT_DIR)/command_side_effect_dispatch_tests.o
SHELL_TANK_PRESENTATION_COVERAGE_TARGET := \
	$(COVERAGE_DIR)/shell_tank_presentation_tests
SHELL_TANK_PRESENTATION_COVERAGE_OBJECT := \
	$(COVERAGE_DIR)/app/shell_tank_presentation.o
SHELL_TANK_PRESENTATION_COVERAGE_TEST_OBJECT := \
	$(COMPILED_COVERAGE_TEST_OBJECT_DIR)/shell_tank_presentation_tests.o
SHELL_CANCELLATION_PRESENTATION_COVERAGE_TARGET := \
	$(COVERAGE_DIR)/shell_cancellation_presentation_tests
SHELL_CANCELLATION_PRESENTATION_COVERAGE_OBJECT := \
	$(COVERAGE_DIR)/app/shell_cancellation_presentation.o
SHELL_CANCELLATION_PRESENTATION_COVERAGE_TEST_OBJECT := \
	$(COMPILED_COVERAGE_TEST_OBJECT_DIR)/shell_cancellation_presentation_tests.o
SHELL_MAP_CORE_PRESENTATION_COVERAGE_TARGET := \
	$(COVERAGE_DIR)/shell_map_core_presentation_tests
SHELL_MAP_CORE_PRESENTATION_COVERAGE_OBJECT := \
	$(COVERAGE_DIR)/app/shell_map_core_presentation.o
SHELL_MAP_CORE_PRESENTATION_COVERAGE_TEST_OBJECT := \
	$(COMPILED_COVERAGE_TEST_OBJECT_DIR)/shell_map_core_presentation_tests.o
COMPILED_COVERAGE_TEST_DEPFILES += \
	$(COMMAND_SIDE_EFFECT_DISPATCH_COVERAGE_TEST_OBJECT:.o=.d) \
	$(INPUT_ADAPTER_COVERAGE_TEST_OBJECT:.o=.d) \
	$(ATOMIC_OUTPUT_FILE_COVERAGE_TEST_OBJECT:.o=.d) \
	$(RELEASE_PERFORMANCE_LOG_COVERAGE_TEST_OBJECT:.o=.d) \
	$(RELEASE_SCREENSHOT_OPTIONS_COVERAGE_TEST_OBJECT:.o=.d) \
	$(SHELL_TANK_PRESENTATION_COVERAGE_TEST_OBJECT:.o=.d) \
	$(SHELL_CANCELLATION_PRESENTATION_COVERAGE_TEST_OBJECT:.o=.d) \
	$(SHELL_MAP_CORE_PRESENTATION_COVERAGE_TEST_OBJECT:.o=.d)
APP_COVERAGE_TARGETS := \
	$(COMMAND_SIDE_EFFECT_DISPATCH_COVERAGE_TARGET) \
	$(INPUT_ADAPTER_COVERAGE_TARGET) \
	$(ATOMIC_OUTPUT_FILE_COVERAGE_TARGET) \
	$(RELEASE_PERFORMANCE_LOG_COVERAGE_TARGET) \
	$(RELEASE_SCREENSHOT_OPTIONS_COVERAGE_TARGET) \
	$(SHELL_CANCELLATION_PRESENTATION_COVERAGE_TARGET) \
	$(SHELL_MAP_CORE_PRESENTATION_COVERAGE_TARGET) \
	$(SHELL_TANK_PRESENTATION_COVERAGE_TARGET)
COVERAGE_TEST_TARGETS := $(RULE_COVERAGE_TARGETS) $(APP_COVERAGE_TARGETS)
COVERAGE_TEST_RAW_PROFILES := \
	$(addsuffix .profraw,$(COVERAGE_TEST_TARGETS))
COVERAGE_CXX := $(shell xcrun --find clang++)
COVERAGE_SDKROOT := $(shell xcrun --sdk macosx --show-sdk-path)
LLVM_PROFDATA := $(shell xcrun --find llvm-profdata)
LLVM_COV := $(shell xcrun --find llvm-cov)

SOUND_FILES := \
	resources/sounds/bonus_appeared.ogg \
	resources/sounds/bonus_obtained.ogg \
	resources/sounds/bullet_hit_brick.ogg \
	resources/sounds/bullet_hit_bullet.ogg \
	resources/sounds/bullet_hit_map_boundaries.ogg \
	resources/sounds/bullet_hit_stone.ogg \
	resources/sounds/eagle_destroyed.ogg \
	resources/sounds/enemy_destroyed.ogg \
	resources/sounds/enemy_hit.ogg \
	resources/sounds/game_over.ogg \
	resources/sounds/highscore_beaten.ogg \
	resources/sounds/menu_item_selected.ogg \
	resources/sounds/pause.ogg \
	resources/sounds/player_destroyed.ogg \
	resources/sounds/player_fired.ogg \
	resources/sounds/player_hit.ogg \
	resources/sounds/player_idle.ogg \
	resources/sounds/player_life_up.ogg \
	resources/sounds/player_moving.ogg \
	resources/sounds/player_respawn.ogg \
	resources/sounds/score_point_counted.ogg \
	resources/sounds/stage_start_up.ogg
TEXTURE_FILES := \
	resources/textures/battlefield_grass.png \
	resources/textures/urban_masonry.png
PROBE_MODEL := resources/models/tank_basic.glb
RUNTIME_RESOURCES := $(SOUND_FILES) $(TEXTURE_FILES) $(PROBE_MODEL)
DIST_LICENSE_FILES := LICENSE NOTICE README.md THIRD_PARTY_NOTICES.md \
	ASSET_LICENSES.md LICENSES/Apache-2.0.txt LICENSES/CC0-1.0.txt \
	LICENSES/MIT-upstream.txt LICENSES/Raylib-6.0-dependencies.txt \
	LICENSES/Zlib-raylib.txt

CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic $(MACOS_TARGET_FLAG)
CPPFLAGS := -Isrc -I$(RAYLIB_PREFIX)/include
LDFLAGS := -L$(RAYLIB_PREFIX)/lib $(MACOS_TARGET_FLAG)
LDLIBS := -lraylib -framework Cocoa -framework GameController \
	-framework IOKit -framework OpenGL
SANITIZER_FLAGS := -O1 -g -fno-omit-frame-pointer \
	-fsanitize=address,undefined $(MACOS_TARGET_FLAG)
COVERAGE_FLAGS := -O0 -g -fprofile-instr-generate -fcoverage-mapping \
	-isysroot "$(COVERAGE_SDKROOT)" $(MACOS_TARGET_FLAG)

.PHONY: all clean test-clean debug run run-app test test-core test-game test-rules \
	test-app test-core-boundaries test-pure-boundaries \
	test-app-boundaries test-architecture test-unit test-session \
	test-assets test-bundle test-release-screenshot test-sanitize coverage \
	test-release-performance-capabilities test-release-performance-smoke \
	run-alpha-performance-qa \
	check-dist-prereqs test-dist dist test-alpha-candidate \
	verify-alpha-candidate verify-tagged-alpha-candidate alpha-candidate \
	test-release-status check-alpha-release-evidence \
	init-alpha-v2-status init-alpha-v2-interactive-plan \
	compile-alpha-v2-interactive-evidence \
	prepare-alpha-v2-clean-mac-qa-kit \
	compile-alpha-v2-clean-mac-evidence verify-alpha-release-ready

all: $(TARGET) $(APP_EXECUTABLE)

$(OBJECT_DIR)/%.o: src/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -MF $(@:.o=.d) \
		-c $< -o $@

$(OBJECT_DIR)/%.o: src/%.mm
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -fobjc-arc -MMD -MP \
		-MF $(@:.o=.d) -c $< -o $@

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) $(LDLIBS) -o $@

$(DEBUG_DIR)/%.o: src/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) -std=c++17 -O0 -g -Wall -Wextra -Wpedantic \
		-Werror $(MACOS_TARGET_FLAG) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(DEBUG_DIR)/%.o: src/%.mm
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) -std=c++17 -O0 -g -Wall -Wextra -Wpedantic \
		-Werror $(MACOS_TARGET_FLAG) -fobjc-arc -MMD -MP \
		-MF $(@:.o=.d) -c $< -o $@

$(DEBUG_TARGET): $(DEBUG_OBJECTS)
	$(CXX) $(DEBUG_OBJECTS) $(LDFLAGS) $(LDLIBS) -o $@

debug: $(DEBUG_TARGET)

$(APP_EXECUTABLE): $(TARGET) macos/Info.plist $(RUNTIME_RESOURCES)
	$(RM) -r $(APP)
	mkdir -p $(APP)/Contents/MacOS $(APP_RESOURCES)/sounds \
		$(APP_RESOURCES)/textures $(APP_RESOURCES)/models
	cp $(TARGET) $(APP_EXECUTABLE)
	cp macos/Info.plist $(APP)/Contents/Info.plist
	/usr/libexec/PlistBuddy -c \
		"Set :LSMinimumSystemVersion $(MACOS_MIN)" $(APP)/Contents/Info.plist
	cp $(SOUND_FILES) $(APP_RESOURCES)/sounds/
	cp $(TEXTURE_FILES) $(APP_RESOURCES)/textures/
	cp $(PROBE_MODEL) $(APP_RESOURCES)/models/
	codesign --force --sign - --timestamp=none $(APP)

check-dist-prereqs:
	test -f "$(RAYLIB_HEADER)"
	test -f "$(RAYLIB_STATIC)"
	test "$(RAYLIB_VERSION)" = "$(RAYLIB_REQUIRED_VERSION)"
	test "$(RAYLIB_LICENSE_SHA256)" = \
		"$(RAYLIB_REQUIRED_LICENSE_SHA256)"
	test "$(RELEASE_PERFORMANCE_CAPABILITY_CONTRACT_SHA256)" = \
		"$(RELEASE_PERFORMANCE_REQUIRED_CONTRACT_SHA256)"
	test -n "$(APP_VERSION)"
	printf '%s\n' "$(DIST_SOURCE_COMMIT)" | \
		grep -Eq '^([0-9a-f]{40}|[0-9a-f]{64})$$'
	printf '%s\n' "$(DIST_SOURCE_TAG)" | \
		grep -Eq '^[A-Za-z0-9][A-Za-z0-9._-]*$$'
	test -n "$(DIST_MACOS_MIN)"
	test "$(RAYLIB_STATIC_MEMBER_COUNT)" -gt 0
	test "$(RAYLIB_STATIC_MINOS_COUNT)" -eq \
		"$(RAYLIB_STATIC_MEMBER_COUNT)"
	test "$(words $(RAYLIB_MACOS_MINS))" -eq 1
	test "$(words $(DIST_ARCH))" -eq 1
	test "$(DIST_ARCH)" = "$$(uname -m)"
	test "$(DIST_MACOS_MIN)" = "$(RAYLIB_MACOS_MIN)"

.PHONY: force-dist-config
force-dist-config:

$(DIST_CONFIG_FILE): force-dist-config | check-dist-prereqs
	mkdir -p $(dir $@)
	{ \
		printf '%s\n' 'compiler=$(CXX)' \
			'compiler-id=$(DIST_COMPILER_ID)' \
			'source-commit=$(DIST_SOURCE_COMMIT)' \
			'source-tag=$(DIST_SOURCE_TAG)' \
			'raylib-prefix=$(abspath $(RAYLIB_PREFIX))' \
			'raylib-version=$(RAYLIB_VERSION)' \
			'raylib-sha256=$(DIST_RAYLIB_SHA256)' \
			'performance-capability-schema=$(RELEASE_PERFORMANCE_CAPABILITY_SCHEMA)' \
			'performance-telemetry-schema=$(RELEASE_PERFORMANCE_TELEMETRY_SCHEMA)' \
			'performance-capability-contract-sha256=$(RELEASE_PERFORMANCE_CAPABILITY_CONTRACT_SHA256)' \
			'arch=$(DIST_ARCH)' 'macos-min=$(DIST_MACOS_MIN)' \
			'compile-flags=$(DIST_COMPILE_FLAGS)' \
			'objcxx-flags=-fobjc-arc' \
			'link-flags=$(DIST_LINK_FLAGS)'; \
	} > $@.tmp
	if test ! -f $@ || ! cmp -s $@.tmp $@; then \
		mv $@.tmp $@; \
	else \
		$(RM) $@.tmp; \
	fi

$(DIST_OBJECT_DIR)/%.o: src/%.cpp $(DIST_CONFIG_FILE)
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(DIST_COMPILE_FLAGS) -MMD -MP \
		-MF $(@:.o=.d) -c $< -o $@

$(DIST_OBJECT_DIR)/%.o: src/%.mm $(DIST_CONFIG_FILE)
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(DIST_COMPILE_FLAGS) -fobjc-arc -MMD -MP \
		-MF $(@:.o=.d) -c $< -o $@

$(DIST_TARGET): $(DIST_OBJECTS) $(RAYLIB_STATIC)
	$(CXX) $(DIST_OBJECTS) $(RAYLIB_STATIC) $(DIST_LINK_FLAGS) -o $@

$(DIST_APP_STAMP): $(DIST_TARGET) macos/Info.plist $(RUNTIME_RESOURCES) \
		$(DIST_LICENSE_FILES)
	$(RM) -r $(DIST_APP)
	mkdir -p $(DIST_APP)/Contents/MacOS $(DIST_APP_RESOURCES)/licenses/LICENSES
	ditto --norsrc --noextattr --noqtn --noacl \
		$(DIST_TARGET) $(DIST_APP_EXECUTABLE)
	ditto --norsrc --noextattr --noqtn --noacl \
		macos/Info.plist $(DIST_APP)/Contents/Info.plist
	/usr/libexec/PlistBuddy -c \
		"Set :LSMinimumSystemVersion $(DIST_MACOS_MIN)" \
		$(DIST_APP)/Contents/Info.plist
	ditto --norsrc --noextattr --noqtn --noacl \
		resources/sounds $(DIST_APP_RESOURCES)/sounds
	ditto --norsrc --noextattr --noqtn --noacl \
		resources/textures $(DIST_APP_RESOURCES)/textures
	ditto --norsrc --noextattr --noqtn --noacl \
		resources/models $(DIST_APP_RESOURCES)/models
	ditto --norsrc --noextattr --noqtn --noacl \
		LICENSE $(DIST_APP_RESOURCES)/licenses/LICENSE
	ditto --norsrc --noextattr --noqtn --noacl \
		NOTICE $(DIST_APP_RESOURCES)/licenses/NOTICE
	ditto --norsrc --noextattr --noqtn --noacl \
		README.md $(DIST_APP_RESOURCES)/licenses/README.md
	ditto --norsrc --noextattr --noqtn --noacl \
		THIRD_PARTY_NOTICES.md \
		$(DIST_APP_RESOURCES)/licenses/THIRD_PARTY_NOTICES.md
	ditto --norsrc --noextattr --noqtn --noacl \
		ASSET_LICENSES.md $(DIST_APP_RESOURCES)/licenses/ASSET_LICENSES.md
	ditto --norsrc --noextattr --noqtn --noacl \
		LICENSES/Apache-2.0.txt \
		$(DIST_APP_RESOURCES)/licenses/LICENSES/Apache-2.0.txt
	ditto --norsrc --noextattr --noqtn --noacl \
		LICENSES/CC0-1.0.txt \
		$(DIST_APP_RESOURCES)/licenses/LICENSES/CC0-1.0.txt
	ditto --norsrc --noextattr --noqtn --noacl \
		LICENSES/MIT-upstream.txt \
		$(DIST_APP_RESOURCES)/licenses/LICENSES/MIT-upstream.txt
	ditto --norsrc --noextattr --noqtn --noacl \
		LICENSES/Raylib-6.0-dependencies.txt \
		$(DIST_APP_RESOURCES)/licenses/LICENSES/Raylib-6.0-dependencies.txt
	ditto --norsrc --noextattr --noqtn --noacl \
		LICENSES/Zlib-raylib.txt \
		$(DIST_APP_RESOURCES)/licenses/LICENSES/Zlib-raylib.txt
	codesign --force --sign - --timestamp=none $(DIST_APP)
	touch $@

$(DIST_ARCHIVE): $(DIST_APP_STAMP)
	$(RM) $(DIST_ARCHIVE) $(DIST_CHECKSUM)
	cd $(DIST_STAGING_DIR) && ditto -c -k --keepParent --norsrc \
		--noextattr --noqtn --noacl Tanks3D.app $(abspath $(DIST_ARCHIVE))

$(DIST_CHECKSUM): $(DIST_ARCHIVE)
	cd $(DIST_DIR) && shasum -a 256 $(notdir $(DIST_ARCHIVE)) \
		> $(notdir $(DIST_CHECKSUM))

test-dist: $(DIST_CHECKSUM) $(DIST_RESOURCE_MANIFEST) \
		$(RELEASE_PERFORMANCE_CAPABILITY_CONTRACT) $(DIST_VERIFY_SCRIPT) \
		$(DIST_VERIFY_NEGATIVE_TEST)
	sh $(DIST_VERIFY_SCRIPT) $(abspath .) $(abspath $(DIST_ARCHIVE)) \
		$(abspath $(DIST_CHECKSUM)) $(DIST_ARCH) $(DIST_MACOS_MIN) \
		$(APP_VERSION) $(DIST_BASENAME) $(DIST_SOURCE_COMMIT) \
		$(DIST_SOURCE_TAG)
	sh $(DIST_VERIFY_NEGATIVE_TEST) $(abspath .) \
		$(abspath $(DIST_ARCHIVE)) $(DIST_ARCH) $(DIST_MACOS_MIN) \
		$(APP_VERSION) $(DIST_SOURCE_COMMIT) $(DIST_SOURCE_TAG)

dist: test test-dist

test-release-status: $(RELEASE_REQUIREMENTS_PROFILE) \
		$(RELEASE_REQUIREMENTS_PROFILE_V2) \
		$(RELEASE_PERFORMANCE_CONTRACT) \
		$(TAGGED_CANDIDATE_VERIFIER) \
		$(TAGGED_CANDIDATE_VERIFIER_TEST) \
		$(RELEASE_STATUS_VERIFY_SCRIPT) $(RELEASE_STATUS_TEST) \
		$(ALPHA_V2_STATUS_INIT_SCRIPT) $(ALPHA_V2_STATUS_INIT_TEST) \
		$(ALPHA_V2_INTERACTIVE_COMPILER) \
		$(ALPHA_V2_INTERACTIVE_COMPILER_TEST) \
		$(ALPHA_V2_CLEAN_MAC_QA_PREPARER) \
		$(ALPHA_V2_CLEAN_MAC_QA_PREPARER_TEST) \
		$(ALPHA_V2_CLEAN_MAC_COLLECTOR) \
		$(ALPHA_V2_CLEAN_MAC_COLLECTOR_TEST) \
		$(ALPHA_V2_CLEAN_MAC_EVIDENCE_COMPILER) \
		$(ALPHA_V2_CLEAN_MAC_EVIDENCE_COMPILER_TEST) \
		$(MEDIA_RECORDING_VALIDATOR) \
		$(MEDIA_RECORDING_VALIDATOR_TEST) \
		$(MEDIA_RECORDING_FIXTURE) \
		$(RELEASE_PERFORMANCE_QA_RUNNER) \
		$(RELEASE_PERFORMANCE_QA_RUNNER_TEST)
	PYTHONDONTWRITEBYTECODE=1 python3 -W error \
		$(TAGGED_CANDIDATE_VERIFIER_TEST)
	PYTHONDONTWRITEBYTECODE=1 python3 -W error $(RELEASE_STATUS_TEST)
	PYTHONDONTWRITEBYTECODE=1 python3 -W error \
		$(ALPHA_V2_STATUS_INIT_TEST)
	PYTHONDONTWRITEBYTECODE=1 python3 -W error \
		$(ALPHA_V2_INTERACTIVE_COMPILER_TEST)
	PYTHONDONTWRITEBYTECODE=1 python3 -W error \
		$(ALPHA_V2_CLEAN_MAC_QA_PREPARER_TEST)
	sh $(ALPHA_V2_CLEAN_MAC_COLLECTOR_TEST) "$(abspath .)"
	PYTHONDONTWRITEBYTECODE=1 python3 -W error \
		$(ALPHA_V2_CLEAN_MAC_EVIDENCE_COMPILER_TEST)
	PYTHONDONTWRITEBYTECODE=1 python3 -W error \
		$(MEDIA_RECORDING_VALIDATOR_TEST)
	PYTHONDONTWRITEBYTECODE=1 python3 -W error \
		$(RELEASE_PERFORMANCE_QA_RUNNER_TEST)

test-clean: $(CLEAN_PRESERVATION_TEST)
	sh $(CLEAN_PRESERVATION_TEST) "$(abspath .)"

test-alpha-candidate: test-release-status test-clean \
		$(ALPHA_CANDIDATE_BUILD_SCRIPT) \
		$(ALPHA_CANDIDATE_VERIFY_SCRIPT) \
		$(ALPHA_CANDIDATE_TAGGED_VERIFY_SCRIPT) \
		$(ALPHA_CANDIDATE_GATE_TEST)
	sh $(ALPHA_CANDIDATE_GATE_TEST) "$(abspath .)"

verify-alpha-candidate: $(ALPHA_CANDIDATE_VERIFY_SCRIPT)
	sh $(ALPHA_CANDIDATE_VERIFY_SCRIPT) "$(abspath .)" \
		"$(ALPHA_CANDIDATE_DIR)"

verify-tagged-alpha-candidate: $(ALPHA_CANDIDATE_TAGGED_VERIFY_SCRIPT) \
		$(TAGGED_CANDIDATE_VERIFIER)
	python3 -B $(TAGGED_CANDIDATE_VERIFIER) \
		--project-root "$(abspath .)" \
		--candidate-dir "$(ALPHA_CANDIDATE_DIR)"

# One-shot helper for an honest BLOCKED v2 baseline. It is deliberately not a
# prerequisite of candidate construction or either release-approval gate.
init-alpha-v2-status: test-release-status $(ALPHA_V2_STATUS_INIT_SCRIPT) \
		$(ALPHA_CANDIDATE_TAGGED_VERIFY_SCRIPT) \
		$(RELEASE_REQUIREMENTS_PROFILE_V2)
	python3 -B $(ALPHA_V2_STATUS_INIT_SCRIPT) \
		--project-root "$(abspath .)" \
		--candidate-dir "$(ALPHA_CANDIDATE_DIR)" \
		--screenshot-input-dir "$(ALPHA_RELEASE_SCREENSHOT_INPUT_DIR)"

# Create an explicit all-NOT_RUN observation plan. The compiler never infers a
# human outcome and refuses to replace either this plan or a compiled pack.
init-alpha-v2-interactive-plan: test-release-status \
		$(ALPHA_V2_INTERACTIVE_COMPILER) \
		$(RELEASE_REQUIREMENTS_PROFILE_V2)
	install -d -m 700 "$(ALPHA_INTERACTIVE_QA_DIR)"
	python3 -B $(ALPHA_V2_INTERACTIVE_COMPILER) init-plan \
		--requirements "$(abspath $(RELEASE_REQUIREMENTS_PROFILE_V2))" \
		--output "$(ALPHA_INTERACTIVE_QA_PLAN)"

# Compile only a fully completed plan into a new pack and status.next.json.
# The canonical status and release documents are deliberately left unchanged.
compile-alpha-v2-interactive-evidence: test-release-status \
		$(ALPHA_V2_INTERACTIVE_COMPILER) $(RELEASE_STATUS_FILE) \
		$(ALPHA_INTERACTIVE_QA_PLAN)
	python3 -B $(ALPHA_V2_INTERACTIVE_COMPILER) compile \
		--project-root "$(abspath .)" \
		--status "$(abspath $(RELEASE_STATUS_FILE))" \
		--manifest "$(ALPHA_INTERACTIVE_QA_PLAN)" \
		--output-dir "$(ALPHA_INTERACTIVE_QA_OUTPUT_DIR)"

# Build a minimal, candidate-bound transfer kit for the source-free Mac. The
# preparer and target both refuse replacement; the candidate itself stays out
# of the kit and must be downloaded with Safari during the recorded session.
prepare-alpha-v2-clean-mac-qa-kit: test-release-status \
		$(ALPHA_V2_CLEAN_MAC_QA_PREPARER) \
		$(ALPHA_V2_CLEAN_MAC_COLLECTOR) $(RELEASE_STATUS_FILE)
	@test -n "$(strip $(CLEAN_MAC_DOWNLOAD_URL))" || { \
		echo 'CLEAN_MAC_DOWNLOAD_URL is required' >&2; exit 2; \
	}
	install -d -m 700 "$(dir $(CLEAN_MAC_QA_KIT_DIR))"
	python3 -B $(ALPHA_V2_CLEAN_MAC_QA_PREPARER) \
		--project-root "$(abspath .)" \
		--candidate-dir "$(ALPHA_CANDIDATE_DIR)" \
		--status-file "$(abspath $(RELEASE_STATUS_FILE))" \
		--download-url "$(CLEAN_MAC_DOWNLOAD_URL)" \
		--output-dir "$(CLEAN_MAC_QA_KIT_DIR)"

# Compile returned raw intake only after a different human has reviewed the
# continuous capture. Publication is no-replace and updates status.next.json,
# never the canonical status file.
compile-alpha-v2-clean-mac-evidence: test-release-status \
		$(ALPHA_V2_CLEAN_MAC_EVIDENCE_COMPILER) $(RELEASE_STATUS_FILE)
	@test -n "$(strip $(CLEAN_MAC_INTAKE_DIR))" || { \
		echo 'CLEAN_MAC_INTAKE_DIR is required' >&2; exit 2; \
	}
	@test -n "$(strip $(CLEAN_MAC_MEDIA))" || { \
		echo 'CLEAN_MAC_MEDIA is required' >&2; exit 2; \
	}
	@test -n "$(strip $(CLEAN_MAC_REVIEWER))" || { \
		echo 'CLEAN_MAC_REVIEWER is required' >&2; exit 2; \
	}
	@test -n "$(strip $(CLEAN_MAC_REVIEWER_SIGNATURE))" || { \
		echo 'CLEAN_MAC_REVIEWER_SIGNATURE is required' >&2; exit 2; \
	}
	@test -n "$(strip $(CLEAN_MAC_REVIEWED_AT_UTC))" || { \
		echo 'CLEAN_MAC_REVIEWED_AT_UTC is required' >&2; exit 2; \
	}
	@test -n "$(strip $(CLEAN_MAC_REVIEW_NOTES))" || { \
		echo 'CLEAN_MAC_REVIEW_NOTES is required' >&2; exit 2; \
	}
	@test "$(strip $(CLEAN_MAC_RELEASE_NOTE_WORDING_VERIFIED))" = yes || { \
		echo 'CLEAN_MAC_RELEASE_NOTE_WORDING_VERIFIED=yes is required' >&2; \
		exit 2; \
	}
	install -d -m 700 "$(dir $(CLEAN_MAC_EVIDENCE_OUTPUT_DIR))"
	python3 -B $(ALPHA_V2_CLEAN_MAC_EVIDENCE_COMPILER) \
		--project-root "$(abspath .)" \
		--status "$(abspath $(RELEASE_STATUS_FILE))" \
		--intake-dir "$(CLEAN_MAC_INTAKE_DIR)" \
		--media "$(CLEAN_MAC_MEDIA)" \
		--reviewer "$(CLEAN_MAC_REVIEWER)" \
		--reviewer-signature "$(CLEAN_MAC_REVIEWER_SIGNATURE)" \
		--reviewed-at-utc "$(CLEAN_MAC_REVIEWED_AT_UTC)" \
		--review-notes "$(CLEAN_MAC_REVIEW_NOTES)" \
		--release-note-wording-verified \
			"$(CLEAN_MAC_RELEASE_NOTE_WORDING_VERIFIED)" \
		--output-dir "$(CLEAN_MAC_EVIDENCE_OUTPUT_DIR)"

check-alpha-release-evidence: test-release-status \
		$(RELEASE_STATUS_VERIFY_SCRIPT) $(RELEASE_STATUS_FILE)
	python3 -B $(RELEASE_STATUS_VERIFY_SCRIPT) --allow-blocked \
		--project-root "$(abspath .)" \
		--status "$(abspath $(RELEASE_STATUS_FILE))"

verify-alpha-release-ready: test-release-status \
		$(RELEASE_STATUS_VERIFY_SCRIPT) $(RELEASE_STATUS_FILE)
	python3 -B $(RELEASE_STATUS_VERIFY_SCRIPT) \
		--project-root "$(abspath .)" \
		--status "$(abspath $(RELEASE_STATUS_FILE))"

alpha-candidate: $(ALPHA_CANDIDATE_BUILD_SCRIPT) \
		$(ALPHA_CANDIDATE_VERIFY_SCRIPT) \
		$(ALPHA_CANDIDATE_TAGGED_VERIFY_SCRIPT) \
		$(ALPHA_CANDIDATE_GATE_TEST)
	sh $(ALPHA_CANDIDATE_BUILD_SCRIPT) "$(abspath .)" "$(DIST_CHANNEL)"

run: all
	cd build && ./Tanks3D

run-app: all
	open $(APP)

# Local macOS visual-QA smoke only. It intentionally stays outside `test`, CI,
# and the Alpha candidate gates because it opens a real GPU window.
test-release-screenshot: all
	$(RM) -r $(RELEASE_SCREENSHOT_SMOKE_DIR)
	mkdir -p $(RELEASE_SCREENSHOT_SMOKE_DIR)
	./$(TARGET) --quick-start --tank-showcase \
		--release-screenshot=$(abspath $(RELEASE_SCREENSHOT_SMOKE_FILE)) \
		--release-screenshot-frame=2
	test -s $(RELEASE_SCREENSHOT_SMOKE_FILE)
	LC_ALL=C file $(RELEASE_SCREENSHOT_SMOKE_FILE) | \
		grep -q 'PNG image data, 1280 x 720'
	test "$$(sips -g pixelWidth $(RELEASE_SCREENSHOT_SMOKE_FILE) | \
		awk '/pixelWidth:/{print $$2}')" = 1280
	test "$$(sips -g pixelHeight $(RELEASE_SCREENSHOT_SMOKE_FILE) | \
		awk '/pixelHeight:/{print $$2}')" = 720
	@if ./$(TARGET) --quick-start --tank-showcase \
		--release-screenshot=$(abspath $(RELEASE_SCREENSHOT_SMOKE_FILE)) \
		--release-screenshot-frame=2; then \
		echo 'existing screenshot was overwritten' >&2; exit 1; \
	fi
	@if ./$(TARGET) --quick-start \
		--release-screenshot=$(abspath $(RELEASE_SCREENSHOT_SMOKE_DIR))/missing/shot.png \
		--release-screenshot-frame=2; then \
		echo 'missing screenshot parent was accepted' >&2; exit 1; \
	fi

# Local macOS GPU smoke for the candidate-generated telemetry path. It stays
# outside `test` and the immutable candidate gate because it opens a window.
test-release-performance-smoke: all
	$(RM) -r $(RELEASE_PERFORMANCE_SMOKE_DIR)
	mkdir -p $(RELEASE_PERFORMANCE_SMOKE_DIR)
	./$(TARGET) --quick-start \
		--release-performance-log=$(abspath $(RELEASE_PERFORMANCE_SMOKE_FILE)) \
		--release-candidate-sha256=0000000000000000000000000000000000000000000000000000000000000000 \
		--release-session-nonce=11111111111111111111111111111111 \
		--release-performance-duration-seconds=2
	test -s $(RELEASE_PERFORMANCE_SMOKE_FILE)
	python3 -m json.tool $(RELEASE_PERFORMANCE_SMOKE_FILE) >/dev/null

# Create the output directory once. The runner refuses non-empty destinations,
# re-verifies the immutable candidate, and writes evidence without replacement.
run-alpha-performance-qa: $(RELEASE_PERFORMANCE_QA_RUNNER) \
		$(TAGGED_CANDIDATE_VERIFIER) \
		$(RELEASE_PERFORMANCE_CONTRACT)
	install -d -m 700 $(RELEASE_PERFORMANCE_QA_OUTPUT_DIR)
	python3 -B $(RELEASE_PERFORMANCE_QA_RUNNER) \
		--project-root "$(abspath .)" \
		--candidate-dir "$(ALPHA_CANDIDATE_DIR)" \
		--output-dir "$(RELEASE_PERFORMANCE_QA_OUTPUT_DIR)"

test: all test-bundle test-rules test-app \
		test-release-performance-capabilities
	./$(TARGET) --self-test

test-unit: $(TARGET) test-rules test-app
	./$(TARGET) --self-test=unit

test-session: $(TARGET)
	./$(TARGET) --self-test=session

test-assets: $(TARGET) $(RUNTIME_RESOURCES)
	./$(TARGET) --self-test=assets

test-bundle: $(APP_EXECUTABLE) $(BUNDLE_RESOURCE_MANIFEST)
	cd $(APP_RESOURCES) && find . -type f -print | LC_ALL=C sort | \
		diff -u $(abspath $(BUNDLE_RESOURCE_MANIFEST)) -
	plutil -lint $(APP)/Contents/Info.plist
	test "$$($(shell command -v /usr/libexec/PlistBuddy) -c \
		'Print :LSMinimumSystemVersion' $(APP)/Contents/Info.plist)" = \
		"$(MACOS_MIN)"
	test "$$($(shell command -v /usr/libexec/PlistBuddy) -c \
		'Print :GCSupportsControllerUserInteraction' \
		$(APP)/Contents/Info.plist)" = "true"
	test "$$($(shell command -v /usr/libexec/PlistBuddy) -c \
		'Print :GCSupportedGameControllers:0:ProfileName' \
		$(APP)/Contents/Info.plist)" = "ExtendedGamepad"
	codesign --verify --deep --strict --verbose=4 $(APP)

# This exact, no-window handshake proves that the built executable exposes the
# release-performance contract before any resource or raylib initialization.
test-release-performance-capabilities: $(TARGET) \
		$(RELEASE_PERFORMANCE_CAPABILITY_CONTRACT)
	mkdir -p $(dir $(RELEASE_PERFORMANCE_CAPABILITY_OUTPUT))
	$(RM) $(RELEASE_PERFORMANCE_CAPABILITY_OUTPUT) \
		$(RELEASE_PERFORMANCE_CAPABILITY_STDERR)
	cd $(dir $(RELEASE_PERFORMANCE_CAPABILITY_OUTPUT)) && \
		$(abspath $(TARGET)) --self-test=release-performance-capabilities \
			> $(abspath $(RELEASE_PERFORMANCE_CAPABILITY_OUTPUT)) \
			2> $(abspath $(RELEASE_PERFORMANCE_CAPABILITY_STDERR))
	test ! -s $(RELEASE_PERFORMANCE_CAPABILITY_STDERR)
	cmp -s $(RELEASE_PERFORMANCE_CAPABILITY_CONTRACT) \
		$(RELEASE_PERFORMANCE_CAPABILITY_OUTPUT)
	python3 -m json.tool $(RELEASE_PERFORMANCE_CAPABILITY_OUTPUT) >/dev/null
	@if cd $(dir $(RELEASE_PERFORMANCE_CAPABILITY_OUTPUT)) && \
		$(abspath $(TARGET)) --self-test=release-performance-capabilities \
			--quick-start >/dev/null 2>&1; then \
		echo 'performance capability probe accepted an extra argument' >&2; \
		exit 1; \
	fi
	@if cd $(dir $(RELEASE_PERFORMANCE_CAPABILITY_OUTPUT)) && \
		$(abspath $(TARGET)) --quick-start \
			--self-test=release-performance-capabilities \
			>/dev/null 2>&1; then \
		echo 'performance capability probe was accepted out of position' >&2; \
		exit 1; \
	fi
	@if cd $(dir $(RELEASE_PERFORMANCE_CAPABILITY_OUTPUT)) && \
		$(abspath $(TARGET)) --self-test=release-performance-capabilities \
			--self-test=release-performance-capabilities \
			>/dev/null 2>&1; then \
		echo 'performance capability probe accepted a duplicate argument' >&2; \
		exit 1; \
	fi

$(HEADER_ONLY_RULE_TEST_TARGETS): build/tests/%: tests/%.cpp $(PURE_HEADERS) \
		tests/test_support.h
	mkdir -p $(dir $@)
	$(CXX) -Isrc -Itests -std=c++17 -O0 -g -Wall -Wextra -Wpedantic \
		-Werror $< -o $@

$(RULE_IMPL_OBJECT_DIR)/%.o: src/%.cpp
	mkdir -p $(dir $@)
	$(CXX) -Isrc -std=c++17 -O0 -g -Wall -Wextra -Wpedantic -Werror \
		-MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(BONUS_SYSTEM_TEST_TARGET): $(BONUS_SYSTEM_TEST_SOURCE) \
		$(BONUS_SYSTEM_RULE_OBJECT) $(PURE_HEADERS) tests/test_support.h
	mkdir -p $(dir $@)
	$(CXX) -Isrc -Itests -std=c++17 -O0 -g -Wall -Wextra -Wpedantic \
		-Werror $(BONUS_SYSTEM_TEST_SOURCE) $(BONUS_SYSTEM_RULE_OBJECT) -o $@

$(COMBAT_SYSTEM_TEST_TARGET): $(COMBAT_SYSTEM_TEST_SOURCE) \
		$(COMBAT_SYSTEM_RULE_OBJECT) $(STAGE_MAP_RULE_OBJECT) \
		$(STAGE_GENERATOR_RULE_OBJECT) $(PURE_HEADERS) tests/test_support.h
	mkdir -p $(dir $@)
	$(CXX) -Isrc -Itests -std=c++17 -O0 -g -Wall -Wextra -Wpedantic \
		-Werror $(COMBAT_SYSTEM_TEST_SOURCE) $(COMBAT_SYSTEM_RULE_OBJECT) \
		$(STAGE_MAP_RULE_OBJECT) $(STAGE_GENERATOR_RULE_OBJECT) -o $@

$(ENEMY_SYSTEM_TEST_TARGET): $(ENEMY_SYSTEM_TEST_SOURCE) \
		$(ENEMY_SYSTEM_RULE_OBJECT) $(PURE_HEADERS) tests/test_support.h
	mkdir -p $(dir $@)
	$(CXX) -Isrc -Itests -std=c++17 -O0 -g -Wall -Wextra -Wpedantic \
		-Werror $(ENEMY_SYSTEM_TEST_SOURCE) $(ENEMY_SYSTEM_RULE_OBJECT) -o $@

$(PLAYER_SYSTEM_TEST_TARGET): $(PLAYER_SYSTEM_TEST_SOURCE) \
		$(PLAYER_SYSTEM_RULE_OBJECT) $(PURE_HEADERS) tests/test_support.h
	mkdir -p $(dir $@)
	$(CXX) -Isrc -Itests -std=c++17 -O0 -g -Wall -Wextra -Wpedantic \
		-Werror $(PLAYER_SYSTEM_TEST_SOURCE) $(PLAYER_SYSTEM_RULE_OBJECT) -o $@

$(SETTLEMENT_SYSTEM_TEST_TARGET): $(SETTLEMENT_SYSTEM_TEST_SOURCE) \
		$(SETTLEMENT_SYSTEM_RULE_OBJECT) $(PURE_HEADERS) tests/test_support.h
	mkdir -p $(dir $@)
	$(CXX) -Isrc -Itests -std=c++17 -O0 -g -Wall -Wextra -Wpedantic \
		-Werror $(SETTLEMENT_SYSTEM_TEST_SOURCE) \
		$(SETTLEMENT_SYSTEM_RULE_OBJECT) -o $@

$(STAGE_GENERATOR_TEST_TARGET): $(STAGE_GENERATOR_TEST_SOURCE) \
		$(STAGE_GENERATOR_RULE_OBJECT) $(PURE_HEADERS) \
		tests/test_support.h tests/stage_layout_expectations.h
	mkdir -p $(dir $@)
	$(CXX) -Isrc -Itests -std=c++17 -O0 -g -Wall -Wextra -Wpedantic \
		-Werror $(STAGE_GENERATOR_TEST_SOURCE) \
		$(STAGE_GENERATOR_RULE_OBJECT) -o $@

$(STAGE_MAP_TEST_TARGET): $(STAGE_MAP_TEST_SOURCE) \
		$(STAGE_MAP_RULE_OBJECT) $(STAGE_GENERATOR_RULE_OBJECT) \
		$(PURE_HEADERS) tests/test_support.h \
		tests/stage_layout_expectations.h tests/stage_map_expectations.h
	mkdir -p $(dir $@)
	$(CXX) -Isrc -Itests -std=c++17 -O0 -g -Wall -Wextra -Wpedantic \
		-Werror $(STAGE_MAP_TEST_SOURCE) $(STAGE_MAP_RULE_OBJECT) \
		$(STAGE_GENERATOR_RULE_OBJECT) -o $@

$(COMMAND_SIDE_EFFECT_DISPATCH_TEST_TARGET): \
		$(COMMAND_SIDE_EFFECT_DISPATCH_TEST_SOURCE) \
		$(COMMAND_SIDE_EFFECT_DISPATCH_SOURCE) $(APP_HEADERS) \
		$(AUDIO_HEADERS) $(PURE_HEADERS) tests/test_support.h
	mkdir -p $(dir $@)
	$(CXX) -Isrc -Itests -std=c++17 -O0 -g -Wall -Wextra -Wpedantic \
		-Werror $(COMMAND_SIDE_EFFECT_DISPATCH_TEST_SOURCE) \
		$(COMMAND_SIDE_EFFECT_DISPATCH_SOURCE) -o $@

$(INPUT_ADAPTER_TEST_TARGET): $(INPUT_ADAPTER_TEST_SOURCE) \
		$(INPUT_ADAPTER_SOURCE) src/app/input_adapter.h \
		src/platform/gamepad_event_accumulator.h \
		src/game/player_system.h src/core/coordinates.h tests/test_support.h
	mkdir -p $(dir $@)
	$(CXX) -Isrc -Itests -std=c++17 -O0 -g -Wall -Wextra -Wpedantic \
		-Werror $(INPUT_ADAPTER_TEST_SOURCE) $(INPUT_ADAPTER_SOURCE) -o $@

$(ATOMIC_OUTPUT_FILE_TEST_TARGET): $(ATOMIC_OUTPUT_FILE_TEST_SOURCE) \
		$(ATOMIC_OUTPUT_FILE_SOURCE) src/app/atomic_output_file.h \
		tests/test_support.h
	mkdir -p $(dir $@)
	$(CXX) -Isrc -Itests -std=c++17 -O0 -g -Wall -Wextra -Wpedantic \
		-Werror $(ATOMIC_OUTPUT_FILE_TEST_SOURCE) \
		$(ATOMIC_OUTPUT_FILE_SOURCE) -o $@

$(RELEASE_PERFORMANCE_LOG_TEST_TARGET): \
		$(RELEASE_PERFORMANCE_LOG_TEST_SOURCE) \
		$(RELEASE_PERFORMANCE_LOG_SOURCE) $(ATOMIC_OUTPUT_FILE_SOURCE) \
		src/app/release_performance_log.h \
		src/app/release_performance_options.h \
		src/app/atomic_output_file.h tests/test_support.h
	mkdir -p $(dir $@)
	$(CXX) -Isrc -Itests -std=c++17 -O0 -g -Wall -Wextra -Wpedantic \
		-Werror $(RELEASE_PERFORMANCE_LOG_TEST_SOURCE) \
		$(RELEASE_PERFORMANCE_LOG_SOURCE) $(ATOMIC_OUTPUT_FILE_SOURCE) \
		-o $@

$(RELEASE_SCREENSHOT_OPTIONS_TEST_TARGET): \
		$(RELEASE_SCREENSHOT_OPTIONS_TEST_SOURCE) \
		$(RELEASE_SCREENSHOT_FILE_SOURCE) $(ATOMIC_OUTPUT_FILE_SOURCE) \
		src/app/atomic_output_file.h \
		src/app/release_screenshot_file.h \
		src/app/release_screenshot_options.h tests/test_support.h
	mkdir -p $(dir $@)
	$(CXX) -Isrc -Itests -std=c++17 -O0 -g -Wall -Wextra -Wpedantic \
		-Werror $(RELEASE_SCREENSHOT_OPTIONS_TEST_SOURCE) \
		$(RELEASE_SCREENSHOT_FILE_SOURCE) $(ATOMIC_OUTPUT_FILE_SOURCE) -o $@

$(SHELL_CANCELLATION_PRESENTATION_TEST_TARGET): \
		$(SHELL_CANCELLATION_PRESENTATION_TEST_SOURCE) \
		$(SHELL_CANCELLATION_PRESENTATION_SOURCE) $(APP_HEADERS) \
		$(AUDIO_HEADERS) $(PURE_HEADERS) tests/test_support.h
	mkdir -p $(dir $@)
	$(CXX) -Isrc -Itests -std=c++17 -O0 -g -Wall -Wextra -Wpedantic \
		-Werror $(SHELL_CANCELLATION_PRESENTATION_TEST_SOURCE) \
		$(SHELL_CANCELLATION_PRESENTATION_SOURCE) -o $@

$(SHELL_MAP_CORE_PRESENTATION_TEST_TARGET): \
		$(SHELL_MAP_CORE_PRESENTATION_TEST_SOURCE) \
		$(SHELL_MAP_CORE_PRESENTATION_SOURCE) $(APP_HEADERS) \
		$(AUDIO_HEADERS) $(PURE_HEADERS) tests/test_support.h
	mkdir -p $(dir $@)
	$(CXX) -Isrc -Itests -std=c++17 -O0 -g -Wall -Wextra -Wpedantic \
		-Werror $(SHELL_MAP_CORE_PRESENTATION_TEST_SOURCE) \
		$(SHELL_MAP_CORE_PRESENTATION_SOURCE) -o $@

$(SHELL_TANK_PRESENTATION_TEST_TARGET): \
		$(SHELL_TANK_PRESENTATION_TEST_SOURCE) \
		$(SHELL_TANK_PRESENTATION_SOURCE) $(APP_HEADERS) $(AUDIO_HEADERS) \
		$(PURE_HEADERS) tests/test_support.h
	mkdir -p $(dir $@)
	$(CXX) -Isrc -Itests -std=c++17 -O0 -g -Wall -Wextra -Wpedantic \
		-Werror $(SHELL_TANK_PRESENTATION_TEST_SOURCE) \
		$(SHELL_TANK_PRESENTATION_SOURCE) -o $@

test-pure-boundaries: $(RULE_BOUNDARY_CHECK) $(PURE_HEADERS) \
		$(PURE_SOURCES) \
		$(RENDERER_BRIDGES)
	$(CXX) -Isrc -std=c++17 -Wall -Wextra -Wpedantic -Werror -x c++ \
		-fsyntax-only $(PURE_HEADERS)
	$(CXX) -Isrc -std=c++17 -Wall -Wextra -Wpedantic -Werror \
		-fsyntax-only $(PURE_SOURCES)
	$(CXX) $(CPPFLAGS) -std=c++17 -Wall -Wextra -Wpedantic -Werror \
		-fsyntax-only $(RENDERER_BRIDGES)
	sh $(RULE_BOUNDARY_CHECK)

test-app-boundaries: $(APP_HEADERS) $(AUDIO_HEADERS) $(APP_SOURCES)
	$(CXX) -Isrc -std=c++17 -Wall -Wextra -Wpedantic -Werror -x c++ \
		-fsyntax-only $(APP_HEADERS) $(AUDIO_HEADERS)
	$(CXX) -Isrc -std=c++17 -Wall -Wextra -Wpedantic -Werror \
		-fsyntax-only $(APP_SOURCES)

test-core-boundaries: test-pure-boundaries

test-architecture: test-pure-boundaries test-app-boundaries

test-core: test-architecture $(CORE_TEST_TARGETS)
	@set -e; for test_binary in $(CORE_TEST_TARGETS); do \
		"$$test_binary"; \
	done

test-game: test-architecture $(GAME_TEST_TARGETS)
	@set -e; for test_binary in $(GAME_TEST_TARGETS); do \
		"$$test_binary"; \
	done

test-rules: test-architecture $(RULE_TEST_TARGETS)
	@set -e; for test_binary in $(RULE_TEST_TARGETS); do \
		"$$test_binary"; \
	done

test-app: test-architecture $(APP_TEST_TARGETS)
	@set -e; for test_binary in $(APP_TEST_TARGETS); do \
		"$$test_binary"; \
	done

$(SANITIZER_DIR)/%.o: src/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) -std=c++17 -Wall -Wextra -Wpedantic \
		$(SANITIZER_FLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(SANITIZER_DIR)/%.o: src/%.mm
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) -std=c++17 -Wall -Wextra -Wpedantic \
		$(SANITIZER_FLAGS) -fobjc-arc -MMD -MP -MF $(@:.o=.d) \
		-c $< -o $@

$(SANITIZER_TARGET): $(SANITIZER_OBJECTS)
	$(CXX) $(SANITIZER_OBJECTS) $(SANITIZER_FLAGS) \
		$(LDFLAGS) $(LDLIBS) -o $@

test-sanitize: $(SANITIZER_TARGET) $(RUNTIME_RESOURCES)
	ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
		UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
		./$(SANITIZER_TARGET) --self-test

$(COVERAGE_DIR)/%.o: src/%.cpp
	mkdir -p $(dir $@)
	"$(COVERAGE_CXX)" $(CPPFLAGS) -std=c++17 -Wall -Wextra -Wpedantic \
		$(COVERAGE_FLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(COVERAGE_DIR)/%.o: src/%.mm
	mkdir -p $(dir $@)
	"$(COVERAGE_CXX)" $(CPPFLAGS) -std=c++17 -Wall -Wextra -Wpedantic \
		$(COVERAGE_FLAGS) -fobjc-arc -MMD -MP -MF $(@:.o=.d) \
		-c $< -o $@

$(COVERAGE_TARGET): $(COVERAGE_OBJECTS)
	"$(COVERAGE_CXX)" $(COVERAGE_OBJECTS) $(COVERAGE_FLAGS) \
		$(LDFLAGS) $(LDLIBS) -o $@

$(HEADER_ONLY_RULE_COVERAGE_TARGETS): $(COVERAGE_DIR)/%: tests/%.cpp \
		$(PURE_HEADERS) \
		tests/test_support.h
	mkdir -p $(dir $@)
	"$(COVERAGE_CXX)" -Isrc -Itests -std=c++17 -Wall -Wextra -Wpedantic \
		-Werror $(COVERAGE_FLAGS) $< -o $@

$(COMPILED_COVERAGE_TEST_OBJECT_DIR)/%.o: tests/%.cpp
	mkdir -p $(dir $@)
	"$(COVERAGE_CXX)" -Isrc -Itests -std=c++17 -O0 -g -Wall -Wextra \
		-Wpedantic -Werror -isysroot "$(COVERAGE_SDKROOT)" \
		$(MACOS_TARGET_FLAG) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

# Combat tests exercise inline GameEvent value semantics, so this test object
# is instrumented as well as the canonical production objects. Keep the other
# compiled test driver uninstrumented to avoid a duplicate `main` profile map.
$(COMBAT_SYSTEM_COVERAGE_TEST_OBJECT): $(COMBAT_SYSTEM_TEST_SOURCE)
	mkdir -p $(dir $@)
	"$(COVERAGE_CXX)" -Isrc -Itests -std=c++17 -Wall -Wextra -Wpedantic \
		-Werror $(COVERAGE_FLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

# This driver directly exercises the inline, thread-safe platform event cache.
# Instrument it while linking the canonical input-adapter production object.
$(INPUT_ADAPTER_COVERAGE_TEST_OBJECT): $(INPUT_ADAPTER_TEST_SOURCE) \
		src/app/input_adapter.h src/platform/gamepad_event_accumulator.h \
		src/game/player_system.h src/core/coordinates.h tests/test_support.h
	mkdir -p $(dir $@)
	"$(COVERAGE_CXX)" -Isrc -Itests -std=c++17 -Wall -Wextra -Wpedantic \
		-Werror $(COVERAGE_FLAGS) -MMD -MP -MF $(@:.o=.d) \
		-c $(INPUT_ADAPTER_TEST_SOURCE) -o $@

# This driver instruments the inline option parser; the file writer uses the
# canonical production object to avoid duplicate coverage maps.
$(RELEASE_SCREENSHOT_OPTIONS_COVERAGE_TEST_OBJECT): \
		$(RELEASE_SCREENSHOT_OPTIONS_TEST_SOURCE) \
		src/app/release_screenshot_options.h \
		src/app/release_screenshot_file.h tests/test_support.h
	mkdir -p $(dir $@)
	"$(COVERAGE_CXX)" -Isrc -Itests -std=c++17 -Wall -Wextra -Wpedantic \
		-Werror $(COVERAGE_FLAGS) -MMD -MP -MF $(@:.o=.d) \
		-c $(RELEASE_SCREENSHOT_OPTIONS_TEST_SOURCE) -o $@

# This driver instruments the inline performance option parser; the recorder
# and atomic writer use their canonical production coverage objects.
$(RELEASE_PERFORMANCE_LOG_COVERAGE_TEST_OBJECT): \
		$(RELEASE_PERFORMANCE_LOG_TEST_SOURCE) \
		src/app/release_performance_log.h \
		src/app/release_performance_options.h \
		src/app/atomic_output_file.h tests/test_support.h
	mkdir -p $(dir $@)
	"$(COVERAGE_CXX)" -Isrc -Itests -std=c++17 -Wall -Wextra -Wpedantic \
		-Werror $(COVERAGE_FLAGS) -MMD -MP -MF $(@:.o=.d) \
		-c $(RELEASE_PERFORMANCE_LOG_TEST_SOURCE) -o $@

$(STAGE_GENERATOR_COVERAGE_TARGET): \
		$(STAGE_GENERATOR_COVERAGE_TEST_OBJECT) \
		$(STAGE_GENERATOR_COVERAGE_OBJECT) $(PURE_HEADERS) \
		tests/test_support.h tests/stage_layout_expectations.h
	mkdir -p $(dir $@)
	"$(COVERAGE_CXX)" $(COVERAGE_FLAGS) \
		$(STAGE_GENERATOR_COVERAGE_TEST_OBJECT) \
		$(STAGE_GENERATOR_COVERAGE_OBJECT) -o $@

$(COMBAT_SYSTEM_COVERAGE_TARGET): \
		$(COMBAT_SYSTEM_COVERAGE_TEST_OBJECT) \
		$(COMBAT_SYSTEM_COVERAGE_OBJECT) $(STAGE_MAP_COVERAGE_OBJECT) \
		$(STAGE_GENERATOR_COVERAGE_OBJECT) $(PURE_HEADERS) \
		tests/test_support.h
	mkdir -p $(dir $@)
	"$(COVERAGE_CXX)" $(COVERAGE_FLAGS) \
		$(COMBAT_SYSTEM_COVERAGE_TEST_OBJECT) \
		$(COMBAT_SYSTEM_COVERAGE_OBJECT) $(STAGE_MAP_COVERAGE_OBJECT) \
		$(STAGE_GENERATOR_COVERAGE_OBJECT) -o $@

$(ENEMY_SYSTEM_COVERAGE_TARGET): \
		$(ENEMY_SYSTEM_COVERAGE_TEST_OBJECT) \
		$(ENEMY_SYSTEM_COVERAGE_OBJECT) $(PURE_HEADERS) \
		tests/test_support.h
	mkdir -p $(dir $@)
	"$(COVERAGE_CXX)" $(COVERAGE_FLAGS) \
		$(ENEMY_SYSTEM_COVERAGE_TEST_OBJECT) \
		$(ENEMY_SYSTEM_COVERAGE_OBJECT) -o $@

$(PLAYER_SYSTEM_COVERAGE_TARGET): \
		$(PLAYER_SYSTEM_COVERAGE_TEST_OBJECT) \
		$(PLAYER_SYSTEM_COVERAGE_OBJECT) $(PURE_HEADERS) \
		tests/test_support.h
	mkdir -p $(dir $@)
	"$(COVERAGE_CXX)" $(COVERAGE_FLAGS) \
		$(PLAYER_SYSTEM_COVERAGE_TEST_OBJECT) \
		$(PLAYER_SYSTEM_COVERAGE_OBJECT) -o $@

$(BONUS_SYSTEM_COVERAGE_TARGET): \
		$(BONUS_SYSTEM_COVERAGE_TEST_OBJECT) \
		$(BONUS_SYSTEM_COVERAGE_OBJECT) $(PURE_HEADERS) \
		tests/test_support.h
	mkdir -p $(dir $@)
	"$(COVERAGE_CXX)" $(COVERAGE_FLAGS) \
		$(BONUS_SYSTEM_COVERAGE_TEST_OBJECT) \
		$(BONUS_SYSTEM_COVERAGE_OBJECT) -o $@

$(SETTLEMENT_SYSTEM_COVERAGE_TARGET): \
		$(SETTLEMENT_SYSTEM_COVERAGE_TEST_OBJECT) \
		$(SETTLEMENT_SYSTEM_COVERAGE_OBJECT) $(PURE_HEADERS) \
		tests/test_support.h
	mkdir -p $(dir $@)
	"$(COVERAGE_CXX)" $(COVERAGE_FLAGS) \
		$(SETTLEMENT_SYSTEM_COVERAGE_TEST_OBJECT) \
		$(SETTLEMENT_SYSTEM_COVERAGE_OBJECT) -o $@

$(STAGE_MAP_COVERAGE_TARGET): $(STAGE_MAP_TEST_SOURCE) \
		$(STAGE_MAP_COVERAGE_OBJECT) $(STAGE_GENERATOR_COVERAGE_OBJECT) \
		$(PURE_HEADERS) tests/test_support.h \
		tests/stage_layout_expectations.h tests/stage_map_expectations.h
	mkdir -p $(dir $@)
	"$(COVERAGE_CXX)" -Isrc -Itests -std=c++17 -Wall -Wextra -Wpedantic \
		-Werror $(COVERAGE_FLAGS) $(STAGE_MAP_TEST_SOURCE) \
		$(STAGE_MAP_COVERAGE_OBJECT) $(STAGE_GENERATOR_COVERAGE_OBJECT) \
		-o $@

$(COMMAND_SIDE_EFFECT_DISPATCH_COVERAGE_TARGET): \
		$(COMMAND_SIDE_EFFECT_DISPATCH_COVERAGE_TEST_OBJECT) \
		$(COMMAND_SIDE_EFFECT_DISPATCH_COVERAGE_OBJECT) \
		$(APP_HEADERS) $(AUDIO_HEADERS) $(PURE_HEADERS) \
		tests/test_support.h
	mkdir -p $(dir $@)
	"$(COVERAGE_CXX)" $(COVERAGE_FLAGS) \
		$(COMMAND_SIDE_EFFECT_DISPATCH_COVERAGE_TEST_OBJECT) \
		$(COMMAND_SIDE_EFFECT_DISPATCH_COVERAGE_OBJECT) -o $@

$(INPUT_ADAPTER_COVERAGE_TARGET): \
		$(INPUT_ADAPTER_COVERAGE_TEST_OBJECT) \
		$(INPUT_ADAPTER_COVERAGE_OBJECT) src/app/input_adapter.h \
		src/platform/gamepad_event_accumulator.h \
		src/game/player_system.h src/core/coordinates.h tests/test_support.h
	mkdir -p $(dir $@)
	"$(COVERAGE_CXX)" $(COVERAGE_FLAGS) \
		$(INPUT_ADAPTER_COVERAGE_TEST_OBJECT) \
		$(INPUT_ADAPTER_COVERAGE_OBJECT) -o $@

$(ATOMIC_OUTPUT_FILE_COVERAGE_TARGET): \
		$(ATOMIC_OUTPUT_FILE_COVERAGE_TEST_OBJECT) \
		$(ATOMIC_OUTPUT_FILE_COVERAGE_OBJECT) \
		src/app/atomic_output_file.h tests/test_support.h
	mkdir -p $(dir $@)
	"$(COVERAGE_CXX)" $(COVERAGE_FLAGS) \
		$(ATOMIC_OUTPUT_FILE_COVERAGE_TEST_OBJECT) \
		$(ATOMIC_OUTPUT_FILE_COVERAGE_OBJECT) -o $@

$(RELEASE_PERFORMANCE_LOG_COVERAGE_TARGET): \
		$(RELEASE_PERFORMANCE_LOG_COVERAGE_TEST_OBJECT) \
		$(RELEASE_PERFORMANCE_LOG_COVERAGE_OBJECT) \
		$(ATOMIC_OUTPUT_FILE_COVERAGE_OBJECT) \
		src/app/release_performance_log.h \
		src/app/release_performance_options.h \
		src/app/atomic_output_file.h tests/test_support.h
	mkdir -p $(dir $@)
	"$(COVERAGE_CXX)" $(COVERAGE_FLAGS) \
		$(RELEASE_PERFORMANCE_LOG_COVERAGE_TEST_OBJECT) \
		$(RELEASE_PERFORMANCE_LOG_COVERAGE_OBJECT) \
		$(ATOMIC_OUTPUT_FILE_COVERAGE_OBJECT) -o $@

$(RELEASE_SCREENSHOT_OPTIONS_COVERAGE_TARGET): \
		$(RELEASE_SCREENSHOT_OPTIONS_COVERAGE_TEST_OBJECT) \
		$(RELEASE_SCREENSHOT_FILE_COVERAGE_OBJECT) \
		$(ATOMIC_OUTPUT_FILE_COVERAGE_OBJECT) \
		src/app/atomic_output_file.h \
		src/app/release_screenshot_file.h \
		src/app/release_screenshot_options.h tests/test_support.h
	mkdir -p $(dir $@)
	"$(COVERAGE_CXX)" $(COVERAGE_FLAGS) \
		$(RELEASE_SCREENSHOT_OPTIONS_COVERAGE_TEST_OBJECT) \
		$(RELEASE_SCREENSHOT_FILE_COVERAGE_OBJECT) \
		$(ATOMIC_OUTPUT_FILE_COVERAGE_OBJECT) -o $@

$(SHELL_TANK_PRESENTATION_COVERAGE_TARGET): \
		$(SHELL_TANK_PRESENTATION_COVERAGE_TEST_OBJECT) \
		$(SHELL_TANK_PRESENTATION_COVERAGE_OBJECT) \
		$(APP_HEADERS) $(AUDIO_HEADERS) $(PURE_HEADERS) \
		tests/test_support.h
	mkdir -p $(dir $@)
	"$(COVERAGE_CXX)" $(COVERAGE_FLAGS) \
		$(SHELL_TANK_PRESENTATION_COVERAGE_TEST_OBJECT) \
		$(SHELL_TANK_PRESENTATION_COVERAGE_OBJECT) -o $@

$(SHELL_CANCELLATION_PRESENTATION_COVERAGE_TARGET): \
		$(SHELL_CANCELLATION_PRESENTATION_COVERAGE_TEST_OBJECT) \
		$(SHELL_CANCELLATION_PRESENTATION_COVERAGE_OBJECT) \
		$(APP_HEADERS) $(AUDIO_HEADERS) $(PURE_HEADERS) \
		tests/test_support.h
	mkdir -p $(dir $@)
	"$(COVERAGE_CXX)" $(COVERAGE_FLAGS) \
		$(SHELL_CANCELLATION_PRESENTATION_COVERAGE_TEST_OBJECT) \
		$(SHELL_CANCELLATION_PRESENTATION_COVERAGE_OBJECT) -o $@

$(SHELL_MAP_CORE_PRESENTATION_COVERAGE_TARGET): \
		$(SHELL_MAP_CORE_PRESENTATION_COVERAGE_TEST_OBJECT) \
		$(SHELL_MAP_CORE_PRESENTATION_COVERAGE_OBJECT) \
		$(APP_HEADERS) $(AUDIO_HEADERS) $(PURE_HEADERS) \
		tests/test_support.h
	mkdir -p $(dir $@)
	"$(COVERAGE_CXX)" $(COVERAGE_FLAGS) \
		$(SHELL_MAP_CORE_PRESENTATION_COVERAGE_TEST_OBJECT) \
		$(SHELL_MAP_CORE_PRESENTATION_COVERAGE_OBJECT) -o $@

$(OBJECT_DIR)/main.o $(DEBUG_DIR)/main.o $(SANITIZER_DIR)/main.o \
		$(COVERAGE_DIR)/main.o: \
	$(PRODUCTION_HEADERS) $(TEST_FILES)

# The game contains the canonical coverage maps for every production path;
# standalone profiles add counts. Passing their binaries again duplicates
# inline maps and makes llvm-cov report spurious mismatched-data warnings. The
# exclusive capability handshake needs its own game profile because the normal
# integrated self-test deliberately cannot enter that pre-resource CLI path.
coverage: $(COVERAGE_TARGET) $(COVERAGE_TEST_TARGETS) $(RUNTIME_RESOURCES) \
		$(RELEASE_PERFORMANCE_CAPABILITY_CONTRACT)
	$(RM) $(COVERAGE_RAW_PROFILE) \
		$(RELEASE_PERFORMANCE_CAPABILITY_COVERAGE_RAW_PROFILE) \
		$(RELEASE_PERFORMANCE_CAPABILITY_COVERAGE_OUTPUT) \
		$(RELEASE_PERFORMANCE_CAPABILITY_COVERAGE_STDERR) \
		$(COVERAGE_TEST_RAW_PROFILES) $(COVERAGE_PROFILE)
	LLVM_PROFILE_FILE=$(abspath $(COVERAGE_RAW_PROFILE)) \
		./$(COVERAGE_TARGET) --self-test
	LLVM_PROFILE_FILE=$(abspath \
		$(RELEASE_PERFORMANCE_CAPABILITY_COVERAGE_RAW_PROFILE)) \
		./$(COVERAGE_TARGET) --self-test=release-performance-capabilities \
			> $(RELEASE_PERFORMANCE_CAPABILITY_COVERAGE_OUTPUT) \
			2> $(RELEASE_PERFORMANCE_CAPABILITY_COVERAGE_STDERR)
	test ! -s $(RELEASE_PERFORMANCE_CAPABILITY_COVERAGE_STDERR)
	cmp -s $(RELEASE_PERFORMANCE_CAPABILITY_CONTRACT) \
		$(RELEASE_PERFORMANCE_CAPABILITY_COVERAGE_OUTPUT)
	@set -e; for test_binary in $(COVERAGE_TEST_TARGETS); do \
		profile_name=$${test_binary##*/}; \
		LLVM_PROFILE_FILE="$(abspath $(COVERAGE_DIR))/$$profile_name.profraw" \
			"$$test_binary"; \
	done
	"$(LLVM_PROFDATA)" merge -sparse $(COVERAGE_RAW_PROFILE) \
		$(RELEASE_PERFORMANCE_CAPABILITY_COVERAGE_RAW_PROFILE) \
		$(COVERAGE_TEST_RAW_PROFILES) \
		-o $(COVERAGE_PROFILE)
	"$(LLVM_COV)" report $(COVERAGE_TARGET) \
		-instr-profile=$(COVERAGE_PROFILE) \
		$(SOURCES) $(PLATFORM_SOURCES) $(PRODUCTION_HEADERS)

# Candidate packages and candidate-bound QA under build/release{,-evidence}
# are immutable records, not disposable compiler output. Keep the cleanup list
# explicit so an ordinary rebuild cannot erase them.
clean:
	$(RM) -r $(TARGET) $(APP) $(OBJECT_DIR) $(DEBUG_DIR) \
		$(SANITIZER_DIR) $(COVERAGE_DIR) $(DIST_DIR) build/tests \
		$(RELEASE_SCREENSHOT_SMOKE_DIR) \
		$(RELEASE_PERFORMANCE_SMOKE_DIR) build/verifier-path-escape.*

-include $(DEPFILES) $(DEBUG_DEPFILES) $(SANITIZER_DEPFILES) \
	$(COVERAGE_DEPFILES) $(RULE_IMPL_DEPFILES) \
	$(COMPILED_COVERAGE_TEST_DEPFILES) $(DIST_DEPFILES)
