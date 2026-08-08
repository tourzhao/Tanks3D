CXX ?= clang++
RAYLIB_PREFIX ?= $(shell brew --prefix raylib 2>/dev/null)
RAYLIB_STATIC := $(RAYLIB_PREFIX)/lib/libraylib.a
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
COMMAND_SIDE_EFFECT_DISPATCH_SOURCE := \
	src/app/command_side_effect_dispatch.cpp
SHELL_CANCELLATION_PRESENTATION_SOURCE := \
	src/app/shell_cancellation_presentation.cpp
SHELL_MAP_CORE_PRESENTATION_SOURCE := \
	src/app/shell_map_core_presentation.cpp
SHELL_TANK_PRESENTATION_SOURCE := src/app/shell_tank_presentation.cpp
APP_SOURCES := $(COMMAND_SIDE_EFFECT_DISPATCH_SOURCE) \
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
SOURCES := src/main.cpp $(APP_SOURCES) \
	$(BONUS_SYSTEM_SOURCE) $(COMBAT_SYSTEM_SOURCE) $(ENEMY_SYSTEM_SOURCE) \
	$(PLAYER_SYSTEM_SOURCE) $(SETTLEMENT_SYSTEM_SOURCE) \
	$(STAGE_GENERATOR_SOURCE) $(STAGE_MAP_SOURCE)
OBJECTS := $(patsubst src/%.cpp,$(OBJECT_DIR)/%.o,$(SOURCES))
DEPFILES := $(OBJECTS:.o=.d)
AUDIO_HEADERS := src/audio/audio_cue.h src/audio/audio_output.h
APP_HEADERS := src/app/command_side_effect_dispatch.h \
	src/app/command_side_effect_sink.h src/app/presentation_values.h \
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
	src/environment_assets.h src/post_process.h
TEST_FILES := tests/test_support.h tests/stage_layout_expectations.h \
	tests/stage_map_expectations.h tests/self_tests.inl
BUNDLE_RESOURCE_MANIFEST := tests/expected_bundle_resources.txt
DIST_RESOURCE_MANIFEST := tests/expected_dist_bundle_resources.txt
DIST_VERIFY_SCRIPT := scripts/verify_macos_dist.sh
DIST_VERIFY_NEGATIVE_TEST := tests/test_macos_dist_verifier.sh
ALPHA_CANDIDATE_BUILD_SCRIPT := scripts/build_alpha_candidate.sh
ALPHA_CANDIDATE_VERIFY_SCRIPT := scripts/verify_alpha_candidate.sh
ALPHA_CANDIDATE_GATE_TEST := tests/test_alpha_candidate_gate.sh

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
DEBUG_OBJECTS := $(patsubst src/%.cpp,$(DEBUG_DIR)/%.o,$(SOURCES))
DEBUG_DEPFILES := $(DEBUG_OBJECTS:.o=.d)

SANITIZER_DIR := build/sanitize
SANITIZER_TARGET := $(SANITIZER_DIR)/Tanks3D-sanitize
SANITIZER_OBJECTS := $(patsubst src/%.cpp,$(SANITIZER_DIR)/%.o,$(SOURCES))
SANITIZER_DEPFILES := $(SANITIZER_OBJECTS:.o=.d)

APP_VERSION := $(shell /usr/libexec/PlistBuddy -c \
	'Print :CFBundleShortVersionString' macos/Info.plist 2>/dev/null)
DIST_CHANNEL ?= alpha.1
DIST_ARCH ?= $(shell lipo -archs "$(RAYLIB_STATIC)" 2>/dev/null)
DIST_MACOS_MIN ?= $(MACOS_MIN)
DIST_DIR := build/dist
DIST_CONFIG_FILE := $(DIST_DIR)/.build-config
DIST_OBJECT_DIR := $(DIST_DIR)/obj
DIST_OBJECTS := $(patsubst src/%.cpp,$(DIST_OBJECT_DIR)/%.o,$(SOURCES))
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
ALPHA_CANDIDATE_DIR ?= \
	$(abspath build/release/$(ALPHA_CANDIDATE_TAG))
DIST_COMPILE_FLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror \
	-mmacosx-version-min=$(DIST_MACOS_MIN)
DIST_LINK_FLAGS := -mmacosx-version-min=$(DIST_MACOS_MIN) \
	-framework Cocoa -framework IOKit -framework OpenGL
DIST_COMPILER_ID := $(shell $(CXX) --version 2>/dev/null | sed -n '1p')
DIST_RAYLIB_SHA256 := $(shell shasum -a 256 "$(RAYLIB_STATIC)" 2>/dev/null | \
	awk '{print $$1}')

COVERAGE_DIR := build/coverage
COVERAGE_TARGET := $(COVERAGE_DIR)/Tanks3D-coverage
COVERAGE_OBJECTS := $(patsubst src/%.cpp,$(COVERAGE_DIR)/%.o,$(SOURCES))
COVERAGE_DEPFILES := $(COVERAGE_OBJECTS:.o=.d)
COVERAGE_RAW_PROFILE := $(COVERAGE_DIR)/self-test.profraw
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
	$(SHELL_TANK_PRESENTATION_COVERAGE_TEST_OBJECT:.o=.d) \
	$(SHELL_CANCELLATION_PRESENTATION_COVERAGE_TEST_OBJECT:.o=.d) \
	$(SHELL_MAP_CORE_PRESENTATION_COVERAGE_TEST_OBJECT:.o=.d)
APP_COVERAGE_TARGETS := \
	$(COMMAND_SIDE_EFFECT_DISPATCH_COVERAGE_TARGET) \
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
	ASSET_LICENSES.md LICENSES/CC0-1.0.txt LICENSES/MIT-upstream.txt \
	LICENSES/Zlib-raylib.txt

CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic $(MACOS_TARGET_FLAG)
CPPFLAGS := -Isrc -I$(RAYLIB_PREFIX)/include
LDFLAGS := -L$(RAYLIB_PREFIX)/lib $(MACOS_TARGET_FLAG)
LDLIBS := -lraylib -framework Cocoa -framework IOKit -framework OpenGL
SANITIZER_FLAGS := -O1 -g -fno-omit-frame-pointer \
	-fsanitize=address,undefined $(MACOS_TARGET_FLAG)
COVERAGE_FLAGS := -O0 -g -fprofile-instr-generate -fcoverage-mapping \
	-isysroot "$(COVERAGE_SDKROOT)" $(MACOS_TARGET_FLAG)

.PHONY: all clean debug run run-app test test-core test-game test-rules \
	test-app test-core-boundaries test-pure-boundaries \
	test-app-boundaries test-architecture test-unit test-session \
	test-assets test-bundle test-sanitize coverage \
	check-dist-prereqs test-dist dist test-alpha-candidate \
	verify-alpha-candidate alpha-candidate

all: $(TARGET) $(APP_EXECUTABLE)

$(OBJECT_DIR)/%.o: src/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -MF $(@:.o=.d) \
		-c $< -o $@

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) $(LDLIBS) -o $@

$(DEBUG_DIR)/%.o: src/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) -std=c++17 -O0 -g -Wall -Wextra -Wpedantic \
		-Werror $(MACOS_TARGET_FLAG) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

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
	test -f "$(RAYLIB_STATIC)"
	test -n "$(APP_VERSION)"
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
			'raylib-prefix=$(abspath $(RAYLIB_PREFIX))' \
			'raylib-sha256=$(DIST_RAYLIB_SHA256)' \
			'arch=$(DIST_ARCH)' 'macos-min=$(DIST_MACOS_MIN)' \
			'compile-flags=$(DIST_COMPILE_FLAGS)' \
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
		LICENSES/CC0-1.0.txt \
		$(DIST_APP_RESOURCES)/licenses/LICENSES/CC0-1.0.txt
	ditto --norsrc --noextattr --noqtn --noacl \
		LICENSES/MIT-upstream.txt \
		$(DIST_APP_RESOURCES)/licenses/LICENSES/MIT-upstream.txt
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

test-dist: $(DIST_CHECKSUM) $(DIST_RESOURCE_MANIFEST) $(DIST_VERIFY_SCRIPT) \
		$(DIST_VERIFY_NEGATIVE_TEST)
	sh $(DIST_VERIFY_SCRIPT) $(abspath .) $(abspath $(DIST_ARCHIVE)) \
		$(abspath $(DIST_CHECKSUM)) $(DIST_ARCH) $(DIST_MACOS_MIN) \
		$(APP_VERSION) $(DIST_BASENAME)
	sh $(DIST_VERIFY_NEGATIVE_TEST) $(abspath .) \
		$(abspath $(DIST_ARCHIVE)) $(DIST_ARCH) $(DIST_MACOS_MIN) \
		$(APP_VERSION)

dist: test test-dist

test-alpha-candidate: $(ALPHA_CANDIDATE_BUILD_SCRIPT) \
		$(ALPHA_CANDIDATE_VERIFY_SCRIPT) $(ALPHA_CANDIDATE_GATE_TEST)
	sh $(ALPHA_CANDIDATE_GATE_TEST) $(abspath .)

verify-alpha-candidate: $(ALPHA_CANDIDATE_VERIFY_SCRIPT)
	sh $(ALPHA_CANDIDATE_VERIFY_SCRIPT) $(abspath .) \
		$(ALPHA_CANDIDATE_DIR)

alpha-candidate: $(ALPHA_CANDIDATE_BUILD_SCRIPT) \
		$(ALPHA_CANDIDATE_VERIFY_SCRIPT) $(ALPHA_CANDIDATE_GATE_TEST)
	sh $(ALPHA_CANDIDATE_BUILD_SCRIPT) $(abspath .) $(DIST_CHANNEL)

run: all
	cd build && ./Tanks3D

run-app: all
	open $(APP)

test: all test-bundle test-rules test-app
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
	codesign --verify --deep --strict --verbose=4 $(APP)

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
# inline maps and makes llvm-cov report spurious mismatched-data warnings.
coverage: $(COVERAGE_TARGET) $(COVERAGE_TEST_TARGETS) $(RUNTIME_RESOURCES)
	$(RM) $(COVERAGE_RAW_PROFILE) $(COVERAGE_TEST_RAW_PROFILES) \
		$(COVERAGE_PROFILE)
	LLVM_PROFILE_FILE=$(abspath $(COVERAGE_RAW_PROFILE)) \
		./$(COVERAGE_TARGET) --self-test
	@set -e; for test_binary in $(COVERAGE_TEST_TARGETS); do \
		profile_name=$${test_binary##*/}; \
		LLVM_PROFILE_FILE="$(abspath $(COVERAGE_DIR))/$$profile_name.profraw" \
			"$$test_binary"; \
	done
	"$(LLVM_PROFDATA)" merge -sparse $(COVERAGE_RAW_PROFILE) \
		$(COVERAGE_TEST_RAW_PROFILES) \
		-o $(COVERAGE_PROFILE)
	"$(LLVM_COV)" report $(COVERAGE_TARGET) \
		-instr-profile=$(COVERAGE_PROFILE) \
		$(SOURCES) $(PRODUCTION_HEADERS)

clean:
	$(RM) -r build

-include $(DEPFILES) $(DEBUG_DEPFILES) $(SANITIZER_DEPFILES) \
	$(COVERAGE_DEPFILES) $(RULE_IMPL_DEPFILES) \
	$(COMPILED_COVERAGE_TEST_DEPFILES) $(DIST_DEPFILES)
