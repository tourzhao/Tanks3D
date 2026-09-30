extends RefCounted

# Integration checks use the real extension, UI signals, and GUI event dispatch.
# They deliberately do not synthesize a settlement snapshot or mutate game rules.
const STEP := 1.0 / 60.0
const MAX_REPORT_TICKS := 20000
const Frontend = preload("res://frontend.gd")
const AudioBank = preload("res://audio_bank.gd")
const ArcadeUI = preload("res://arcade_ui_checks.gd")
const ParityUI = preload("res://parity_checks.gd")


static func check(condition: bool, detail: String) -> bool:
    if not condition:
        push_error("Godot UI integration check failed: " + detail)
    return condition


static func await_window_ready(app: Node) -> bool:
    if DisplayServer.get_name() == "headless":
        return true
    # Native macOS activation can change focus after _ready. Do not inject held
    # keys while that startup transition can legitimately release input state.
    # A focus loss during the checks still fails their ordinary assertions.
    var deadline := Time.get_ticks_msec() + 5000
    var focused_since := -1
    while Time.get_ticks_msec() < deadline:
        var now := Time.get_ticks_msec()
        if app.get_window().has_focus():
            if focused_since < 0: focused_since = now
            if now - focused_since >= 200: return true
        else:
            focused_since = -1
        await app.get_tree().process_frame
    return check(false, "graphical UI test requires a focused window after startup")


static func read_state(app: Node) -> Dictionary:
    var value: Variant = JSON.parse_string(app.core.snapshot())
    if value is Dictionary:
        return value
    push_error("Godot UI integration check failed: invalid native snapshot")
    return {}


static func focus_lifecycle_checks(app: Node) -> bool:
    # A detached instance receives the same notification path as the macOS
    # startup race. It has no native core, window, or viewport yet.
    var detached: Node = load("res://main.gd").new()
    detached.queued_commands = 32
    detached.modifier_fire_held = [1, 2]
    detached._notification(Node.NOTIFICATION_APPLICATION_FOCUS_IN)
    detached._notification(Node.NOTIFICATION_APPLICATION_FOCUS_OUT)
    var cleared: bool = detached.queued_commands == 0 and detached.modifier_fire_held == [0, 0]
    detached.free()
    if not check(cleared, "pre-tree focus notifications safely clear input without a viewport"):
        return false

    # Reattach the actual initialized scene, without requesting _ready again.
    # This exercises production _enter_tree and notifications after _exit_tree,
    # while preserving the real native session and existing UI objects.
    var tree := app.get_tree()
    var parent := app.get_parent()
    var index := app.get_index()
    var was_current: bool = tree.current_scene == app
    var core_id: int = app.core.get_instance_id()
    var before := read_state(app)
    app._notification(Node.NOTIFICATION_APPLICATION_FOCUS_OUT)
    parent.remove_child(app)
    app._notification(Node.NOTIFICATION_APPLICATION_FOCUS_OUT)
    app._notification(Node.NOTIFICATION_APPLICATION_FOCUS_IN)
    parent.add_child(app)
    parent.move_child(app, index)
    if was_current: tree.current_scene = app
    var synchronized: bool = app.get_viewport().gui_disable_input == not app.get_window().has_focus()
    var unchanged: bool = app.core.get_instance_id() == core_id and read_state(app).digest == before.digest
    if not check(synchronized and unchanged,
            "scene reentry synchronizes real window focus without rebuilding or advancing the session"):
        return false
    await tree.process_frame
    return true


static func advance(app: Node, p1: int = 0, p2: int = 0) -> bool:
    if not check(app.core.step(STEP, p1, p2), "native step: " + app.core.error()):
        return false
    var heard: Dictionary = app.get_meta("ui_audio_requests", {})
    for command in app.drain_audio():
        if command.get("op") == "play":
            var cue := int(command.cue)
            heard[cue] = int(heard.get(cue, 0)) + 1
    app.set_meta("ui_audio_requests", heard)
    return true


static func refresh(app: Node) -> Dictionary:
    var value := read_state(app)
    app.state = value
    app.frontend.refresh(value, app.pixel_style, 0.0)
    return value


static func advance_commands(app: Node) -> bool:
    var bits: int = app.input_bits(0) | int(app.queued_commands)
    app.queued_commands = 0
    if not advance(app, bits):
        return false
    refresh(app)
    return true


static func accept_event(app: Node, pressed: bool) -> void:
    var event := InputEventAction.new()
    event.action = &"ui_accept"
    event.pressed = pressed
    event.strength = 1.0 if pressed else 0.0
    Input.parse_input_event(event)
    # process_frame can resume the coroutine before accumulated input dispatch.
    # Deliver this synthetic event through the real GUI before stepping rules.
    Input.flush_buffered_events()
    await app.get_tree().process_frame


static func key_event(key: Key, location: KeyLocation, pressed: bool) -> InputEventKey:
    var event := InputEventKey.new()
    event.keycode = key
    event.physical_keycode = key
    event.location = location
    event.pressed = pressed
    return event


static func keyboard_event(app: Node, key: Key, pressed: bool) -> void:
    Input.parse_input_event(key_event(key, KEY_LOCATION_UNSPECIFIED, pressed))
    # parse_input_event queues accumulated keyboard events; process_frame is
    # emitted before that queue is necessarily drained in a headless run.
    Input.flush_buffered_events()
    await app.get_tree().process_frame


static func joy_button_event(app: Node, button: JoyButton, pressed: bool) -> void:
    var event := InputEventJoypadButton.new()
    event.device = 15
    event.button_index = button
    event.pressed = pressed
    Input.parse_input_event(event)
    Input.flush_buffered_events()
    await app.get_tree().process_frame


static func joy_motion_event(app: Node, axis: JoyAxis, value: float) -> void:
    var event := InputEventJoypadMotion.new()
    event.device = 15
    event.axis = axis
    event.axis_value = value
    Input.parse_input_event(event)
    Input.flush_buffered_events()
    await app.get_tree().process_frame


static func controller_menu_checks(app: Node) -> bool:
    var configuration: Dictionary = app.configuration.duplicate(true)
    var initial := read_state(app)
    var elapsed: float = app.elapsed
    var heard: Dictionary = app.get_meta("ui_audio_requests", {}).duplicate(true)
    var frontend: CanvasLayer = app.frontend
    frontend.show_menu()
    frontend.controls.deploy.grab_focus()
    await joy_motion_event(app, JOY_AXIS_LEFT_Y, 1.0)
    if not check(frontend.controls.row_stage.has_focus(), "stick first moves to the stage row"):
        return false
    # Simulate the OS connection notifications, not a physical Bluetooth test.
    # Retain a held direction across a disconnect and reuse the same device ID.
    Input.joy_connection_changed.emit(15, false)
    frontend.controls.deploy.grab_focus()
    frontend.device_help.text = "GAMEPADS -1" # the pre-discovery display is stale
    Input.joy_connection_changed.emit(15, true)
    if not check(frontend.device_help.text.begins_with("GAMEPADS %d " % Input.get_connected_joypads().size()) and
            frontend.controls.deploy.has_focus() and read_state(app) == initial,
            "late controller discovery refreshes the count without moving focus or advancing gameplay"):
        return false
    await joy_motion_event(app, JOY_AXIS_LEFT_Y, 1.0)
    if not check(frontend.controls.row_stage.has_focus(), "reconnected controller's first direction is not swallowed"):
        return false
    await joy_motion_event(app, JOY_AXIS_LEFT_Y, 0.0)
    Input.joy_connection_changed.emit(15, false)
    for mode in range(3):
        for button in [JOY_BUTTON_A, JOY_BUTTON_B, JOY_BUTTON_START]:
            frontend.show_menu()
            frontend.controls.mode.select(mode)
            frontend.refresh_menu()
            frontend.controls.deploy.grab_focus()
            await joy_button_event(app, button, true)
            if not check(not frontend.menu_open,
                    "controller deploy on press: mode=%d button=%d" % [mode, button]):
                return false
            var state := read_state(app)
            if not check(int(state.player_count) == (1 if mode == 0 else 2) and
                    bool(state.ai_p2) == (mode == 2), "controller deployment preserves crew selection"):
                return false
            await joy_button_event(app, button, false)
            if not check((int(app.input_bits(0)) & 64) == 0 and not read_state(app).paused,
                    "menu confirmation does not immediately pause the battle"):
                return false
    for page in ["setup", "advanced"]:
        frontend.show_menu()
        frontend.set_menu_page(page)
        # Start deploys even when a submenu/option row currently has focus.
        if page == "setup": frontend.controls.row_advanced.grab_focus()
        await joy_button_event(app, JOY_BUTTON_START, true)
        if not check(not frontend.menu_open, "Start deploys directly from setup and Advanced"):
            return false
        await joy_button_event(app, JOY_BUTTON_START, false)
    # A late OS event must reach the very next native update. Deliberately
    # buffer a complete tap without the test helpers' explicit input flush.
    var solo := Frontend.DEFAULTS.duplicate()
    app.start_from_menu(solo)
    for tick in range(300):
        if not advance(app): return false
    refresh(app)
    var before := read_state(app)
    var accumulated: bool = Input.use_accumulated_input
    Input.use_accumulated_input = true
    for key in [KEY_LEFT, KEY_SPACE]:
        Input.parse_input_event(key_event(key, KEY_LOCATION_UNSPECIFIED, true))
        Input.parse_input_event(key_event(key, KEY_LOCATION_UNSPECIFIED, false))
    app.ui_self_test = false
    app._process(STEP)
    app.ui_self_test = true
    Input.use_accumulated_input = accumulated
    var after := read_state(app)
    if not check(int(after.tick) == int(before.tick) + 1 and
            float(after.players[0].x) < float(before.players[0].x) and
            float(after.players[0].yaw) != float(before.players[0].yaw) and
            float(after.players[0].fire_cooldown) > 0,
            "late buffered move/turn/fire taps reach the next native update"):
        return false
    if not advance_commands(app): return false
    if not check(not read_state(app).players[0].moving and app.queued_players == [0, 0],
            "released taps do not remain latched on a following frame"):
        return false
    # The new right-face alias must work on GUI-owned pause confirmation too.
    if not advance(app, 64): return false
    # The initiating pause pulse must release before a separate resume pulse.
    if not advance(app): return false
    refresh(app)
    await joy_button_event(app, JOY_BUTTON_B, true)
    if not check((int(app.queued_commands) & 64) != 0,
            "right-face pause confirmation activates on press, not release"):
        return false
    if not advance_commands(app): return false
    await joy_button_event(app, JOY_BUTTON_B, false)
    if not check(not read_state(app).paused, "right face resumes the focused pause button"):
        return false
    app.configuration = configuration
    app.stage = int(configuration.stage)
    app.player_count = int(configuration.players)
    app.ai_p2 = bool(configuration.ai_p2)
    app.elapsed = elapsed
    var restored: bool = app.core.reset_config(app.seed_value, configuration)
    app.clear_effects()
    app.clear_battle_input()
    frontend.configure(configuration, app.pixel_style, app.audio_volume)
    frontend.show_menu()
    app.state = read_state(app)
    app.update_world(0.0, true)
    app.set_meta("ui_audio_requests", heard)
    if not check(restored and read_state(app) == initial,
            "controller checks restore the initial native session"):
        return false
    return true


static func background_gui_checks(app: Node, button: Button, phase: String, activate: bool) -> bool:
    # Notify the production focus handler, then exercise real GUI dispatch with
    # synthetic pad events. This is not an OS focus or physical controller test.
    # parse_input_event bypasses Godot's hardware joy ingress guard, so these
    # assertions also cover events already delivered before that guard applies.
    button.grab_focus()
    var activations := [0]
    var count_activation := func(): activations[0] += 1
    button.pressed.connect(count_activation)
    var before := read_state(app)
    var menu_before: bool = app.frontend.menu_open
    app._notification(Node.NOTIFICATION_APPLICATION_FOCUS_OUT)
    for direction in [JOY_BUTTON_DPAD_DOWN, JOY_BUTTON_DPAD_UP]:
        await joy_button_event(app, direction, true)
        await joy_button_event(app, direction, false)
    for confirm in [JOY_BUTTON_A, JOY_BUTTON_B, JOY_BUTTON_START]:
        await joy_button_event(app, confirm, true)
        await joy_button_event(app, confirm, false)
    var unchanged: bool = activations[0] == 0 and app.frontend.menu_open == menu_before and \
        read_state(app).digest == before.digest and int(app.queued_commands) == 0 and \
        app.get_viewport().gui_get_focus_owner() == button
    if phase == "pause" and unchanged:
        # Disabling GUI input must not disable the main native update callback.
        # A paused native step advances its tick without moving the world/RNG.
        var old_ui_self_test: bool = app.ui_self_test
        app.ui_self_test = false
        app.process_game_frame(STEP, Time.get_ticks_usec())
        app.ui_self_test = old_ui_self_test
        var ticked := read_state(app)
        unchanged = int(ticked.tick) == int(before.tick) + 1 and ticked.digest == before.digest
    app._notification(Node.NOTIFICATION_APPLICATION_FOCUS_IN)
    if not check(unchanged, phase + " background controller events cannot activate or navigate GUI"):
        button.pressed.disconnect(count_activation)
        return false
    if not check(app.get_viewport().gui_get_focus_owner() == button,
            phase + " restores the focused GUI button after focus returns"):
        button.pressed.disconnect(count_activation)
        return false
    if activate:
        # More than one control is available in menu/pause. Keyboard navigation
        # must resume before checking the same real button's controller action.
        var navigation_key: Key = KEY_DOWN if phase == "pause" else KEY_TAB
        await keyboard_event(app, navigation_key, true)
        await keyboard_event(app, navigation_key, false)
        var keyboard_restored: bool = app.get_viewport().gui_get_focus_owner() != null and \
            app.get_viewport().gui_get_focus_owner() != button
        button.grab_focus()
        await joy_button_event(app, JOY_BUTTON_A, true)
        await joy_button_event(app, JOY_BUTTON_A, false)
        if not check(keyboard_restored and activations[0] == 1,
                phase + " keyboard navigation and one controller activation resume after focus returns " +
                "(keyboard=%s activations=%d)" % [keyboard_restored, activations[0]]):
            button.pressed.disconnect(count_activation)
            return false
    button.pressed.disconnect(count_activation)
    return true


static func keyboard_checks(app: Node) -> bool:
    # Exercise the production location mapper directly: headless windows cannot
    # receive physical keyboard focus. Space/F below also use GUI event dispatch.
    for key in [KEY_CTRL, KEY_ALT]:
        for slot in range(2):
            var location := KEY_LOCATION_RIGHT if slot == 0 else KEY_LOCATION_LEFT
            app.clear_battle_input()
            app.track_modifier_fire(key_event(key, location, true))
            if not check((int(app.input_bits(slot)) & 16) != 0 and
                    (int(app.input_bits(1 - slot)) & 16) == 0,
                    "Ctrl/Alt location fires only its assigned player"):
                return false
            for frame in range(3):
                if not check((int(app.input_bits(slot)) & 16) != 0,
                        "modifier fire remains held across input frames"):
                    return false
            app.track_modifier_fire(key_event(key, location, false))
            if not check((int(app.input_bits(slot)) & 16) == 0, "modifier release ends held fire"):
                return false
            app.track_modifier_fire(key_event(key, location, true))
            app.track_modifier_fire(key_event(key, location, false))
            if not check((int(app.input_bits(slot)) & 16) != 0 and
                    (int(app.input_bits(slot)) & 16) == 0, "quick modifier tap survives exactly one frame"):
                return false

    app.track_modifier_fire(key_event(KEY_CTRL, KEY_LOCATION_RIGHT, true))
    app.track_modifier_fire(key_event(KEY_ALT, KEY_LOCATION_RIGHT, true))
    app.input_bits(0)
    app.track_modifier_fire(key_event(KEY_CTRL, KEY_LOCATION_RIGHT, false))
    if not check((int(app.input_bits(0)) & 16) != 0, "releasing Ctrl preserves independently held Alt"):
        return false
    app.track_modifier_fire(key_event(KEY_ALT, KEY_LOCATION_LEFT, true))
    if not check((int(app.input_bits(0)) & 16) != 0 and (int(app.input_bits(1)) & 16) != 0,
            "left and right modifiers support simultaneous two-player fire"):
        return false
    app._notification(Node.NOTIFICATION_APPLICATION_FOCUS_OUT)
    if not check((int(app.input_bits(0)) & 16) == 0 and (int(app.input_bits(1)) & 16) == 0,
            "focus loss clears both held modifiers and pending quick presses"):
        return false
    var echo := key_event(KEY_ALT, KEY_LOCATION_RIGHT, true)
    echo.echo = true
    app.track_modifier_fire(echo)
    app.track_modifier_fire(key_event(KEY_CTRL, KEY_LOCATION_UNSPECIFIED, true))
    if not check((int(app.input_bits(0)) & 16) == 0 and (int(app.input_bits(1)) & 16) == 0,
            "echo and unspecified-side keys cannot recreate fire after focus loss"):
        return false
    app._notification(Node.NOTIFICATION_APPLICATION_FOCUS_IN)
    app.track_modifier_fire(key_event(KEY_CTRL, KEY_LOCATION_RIGHT, true))
    app.input_bits(0)
    app.capturing = true
    app._input(key_event(KEY_CTRL, KEY_LOCATION_RIGHT, false))
    app._input(key_event(KEY_ALT, KEY_LOCATION_LEFT, true))
    app.capturing = false
    if not check((int(app.input_bits(0)) & 16) == 0 and (int(app.input_bits(1)) & 16) == 0,
            "capture accepts releases and rejects fresh modifier presses"):
        return false

    for slot in range(2):
        var key := KEY_SPACE if slot == 0 else KEY_F
        await keyboard_event(app, key, true)
        var own_bits: int = app.input_bits(slot)
        var other_bits: int = app.input_bits(1 - slot)
        if not check((own_bits & 16) != 0 and (other_bits & 16) == 0,
                "Space/F still fire their respective players (P%d own=%d other=%d held=%s)" %
                [slot + 1, own_bits, other_bits, Input.is_key_pressed(key)]):
            await keyboard_event(app, key, false)
            return false
        if not check((int(app.input_bits(slot)) & 16) != 0, "Space/F retain held-fire behavior"):
            await keyboard_event(app, key, false)
            return false
        await keyboard_event(app, key, false)
        if not check((int(app.input_bits(slot)) & 16) == 0, "Space/F release ends fire"):
            return false
    app.clear_battle_input()
    return true


static func report_start_checks(app: Node) -> bool:
    app.core.reset_pad(0, false)
    var start: int = app.core.map_pad(0, 0.0, 0.0, 64, int(app.configuration.camera_yaw))
    if not check((int(app.filter_modal_confirmation(start, (start & 32) != 0)) & 32) != 0,
            "native Start confirmation survives report GUI filtering"):
        return false
    var held: int = app.core.map_pad(0, 0.0, 0.0, 64, int(app.configuration.camera_yaw))
    if not check((int(app.filter_modal_confirmation(held, (held & 32) != 0)) & 32) == 0,
            "held Start does not confirm twice"):
        return false
    app.core.map_pad(0, 0.0, 0.0, 0, int(app.configuration.camera_yaw))
    var again: int = app.core.map_pad(0, 0.0, 0.0, 64, int(app.configuration.camera_yaw))
    app.core.reset_pad(0, false)
    return check((int(app.filter_modal_confirmation(again, (again & 32) != 0)) & 32) != 0,
        "released and re-pressed Start confirms again")


static func tile_mesh(app: Node, index: int) -> Mesh:
    var tile: Node3D = app.tile_nodes.get(index)
    if tile == null:
        return null
    var body := tile.get_node_or_null("Body") as MeshInstance3D
    return body.mesh if body != null else null


static func render_fixture(app: Node, original: Dictionary, fixture: Dictionary,
        index: int, original_mesh: Mesh, label: String) -> bool:
    var before_scans: int = app.terrain_scans
    app.state = fixture
    app.update_map()
    var changed := tile_mesh(app, index)
    var valid := check(changed != null and changed != original_mesh and
        int(app.terrain_scans) == before_scans + 1, label + " rebuilds the actual tile mesh once")
    # Restore even after a failed check. These are render-adapter fixtures, not
    # simulation mutations or visual evidence of events that happened in play.
    app.state = original.duplicate(true)
    app.update_map()
    valid = check(tile_mesh(app, index) == original_mesh,
        label + " restoration reuses the original cached mesh") and valid
    return valid


static func renderer_cache_checks(app: Node) -> bool:
    var original := read_state(app)
    var native_digest: String = original.digest
    app.state = original.duplicate(true)
    app.update_map()
    var brick_index := -1
    for index in range(676):
        var row := index / 26
        var col := index % 26
        if (original.map[row][col] == "#" and int(original.brick_masks[index]) == 15 and
                not app.BASE_CELLS.has(Vector2i(col, row))):
            brick_index = index
            break
    if not check(brick_index >= 0, "native stage has an intact non-base brick for the renderer fixture"):
        return false
    var brick := tile_mesh(app, brick_index)
    if not check(brick != null, "intact native brick has a real Body mesh"):
        return false
    var scans: int = app.terrain_scans
    for iteration in range(3):
        app.state = original.duplicate(true)
        app.update_map()
    if not check(int(app.terrain_scans) == scans and tile_mesh(app, brick_index) == brick,
            "identical snapshot values skip the terrain scan and retain the mesh"):
        return false

    var partial := original.duplicate(true)
    partial.brick_masks[brick_index] = 7
    if not render_fixture(app, original, partial, brick_index, brick, "brick mask 15 to 7"):
        return false
    var steel := original.duplicate(true)
    var brick_row: int = brick_index / 26
    var brick_col: int = brick_index % 26
    var line: String = steel.map[brick_row]
    steel.map[brick_row] = line.left(brick_col) + "@" + line.substr(brick_col + 1)
    if not render_fixture(app, original, steel, brick_index, brick, "brick to steel row change"):
        return false

    var warmup := app.world.get_node_or_null("WaterShaderWarmup") as Node3D
    if not check(warmup != null and not warmup.is_visible_in_tree() and
            not app.tile_nodes.values().has(warmup), "startup water is hidden and outside map cells"):
        return false
    var water_body := warmup.get_node("Body") as MeshInstance3D
    var water := original.duplicate(true)
    water.map[brick_row] = line.left(brick_col) + "~" + line.substr(brick_col + 1)
    app.state = water
    app.update_map()
    var water_mesh := tile_mesh(app, brick_index)
    if not check(water_mesh == water_body.mesh and
            water_mesh.surface_get_material(0) is ShaderMaterial,
            "first visible water reuses the startup mesh and material"):
        return false
    water = water.duplicate(true)
    water.brick_masks[brick_index] = 0
    app.state = water
    app.update_map()
    if not check(tile_mesh(app, brick_index) == water_mesh,
            "water geometry does not depend on irrelevant brick damage"):
        return false
    app.state = original.duplicate(true)
    app.update_map()

    var wall_cell: Vector2i = app.BASE_CELLS[0]
    var wall_index := wall_cell.y * 26 + wall_cell.x
    var wall := tile_mesh(app, wall_index)
    if not check(wall != null and int(original.base_walls[0]) > 1,
            "native base wall has a healthy real Body mesh"):
        return false
    var damaged := original.duplicate(true)
    damaged.base_walls[0] = int(original.base_walls[0]) - 1
    if not render_fixture(app, original, damaged, wall_index, wall, "base wall health change"):
        return false
    var protected := original.duplicate(true)
    protected.base_steel_visible = not bool(original.base_steel_visible)
    if not render_fixture(app, original, protected, wall_index, wall, "visible base steel toggle"):
        return false

    scans = app.terrain_scans
    app.state = original.duplicate(true)
    app.update_map()
    return check(int(app.terrain_scans) == scans and read_state(app).digest == native_digest,
        "restored renderer fixture is cached and never changes native simulation state")


static func capture(app: Node, name: String) -> bool:
    if str(app.capture_dir).is_empty() or DisplayServer.get_name() == "headless":
        return true
    app.update_world(0.0)
    await app.capture_frontend(name)
    # Godot 4.7 keeps the last requested mode in the SubViewport node. The
    # renderer owns UPDATE_ONCE's transition, so inspect its actual RID state.
    var mode := RenderingServer.viewport_get_update_mode(app.viewport.get_viewport_rid())
    return check(mode == RenderingServer.VIEWPORT_UPDATE_DISABLED,
        "idle self-test viewport stops rendering after its requested frame: " + name)


static func fresh_deployment(app: Node, record_tape: bool) -> void:
    # A fresh seeded app session, followed exclusively by normal native steps.
    # No score, report, player or random state is injected by these tests.
    app.return_to_menu()
    var settings: Dictionary = Frontend.DEFAULTS.duplicate()
    settings.stage = 20 if record_tape else 1
    settings.players = 2 if record_tape else 1
    settings.ai_p2 = record_tape
    settings.lives = 3 if record_tape else 1
    settings.max_hp = 3 if record_tape else 1
    settings.nation_p1 = 2 if record_tape else 0
    settings.nation_p2 = 0 if record_tape else 1
    app.configuration = settings
    app.stage = settings.stage
    app.player_count = settings.players
    app.ai_p2 = settings.ai_p2
    app.seed_value = 20260916
    app.frontend.configure(settings, app.pixel_style, app.audio_volume)
    app.frontend.show_battle()
    app.restart()


static func confirm_once(app: Node) -> bool:
    await joy_button_event(app, JOY_BUTTON_A, true)
    if not check((int(app.queued_commands) & 32) != 0, "focused terminal button dispatches confirmation before native stepping"):
        await joy_button_event(app, JOY_BUTTON_A, false)
        return false
    if not advance_commands(app): return false
    if read_state(app).get("menu_requested", false):
        # The ordinary app changes GUI focus on the press, before its release.
        # Keep that ordering here to detect an accidental held-accept redeploy.
        app.ui_self_test = false
        app._process(0.0)
        app.ui_self_test = true
    # Waiting while held must not finish another report phase or dismiss a record.
    for tick in range(3):
        await app.get_tree().process_frame
        if not advance_commands(app): return false
    await joy_button_event(app, JOY_BUTTON_A, false)
    if not advance_commands(app): return false
    if app.frontend.menu_open:
        return check(read_state(app).get("menu_requested", false),
            "a terminal confirm held across menu focus does not redeploy")
    return true


static func handle_native_menu(app: Node) -> bool:
    if not check(read_state(app).get("menu_requested", false), "native terminal requests deployment"):
        return false
    # Use the ordinary menu-request handler, with no elapsed simulation time.
    app.ui_self_test = false
    app._process(0.0)
    app.ui_self_test = true
    if not check(app.frontend.menu_open and not app.frontend.report_panel.visible,
            "native terminal reaches the deployment menu"):
        return false
    await app.get_tree().process_frame
    return check(app.frontend.menu_open, "confirm release does not redeploy from the newly focused menu")


static func terminal_checks(app: Node, record_tape: bool, timeout_record: bool = false) -> bool:
    fresh_deployment(app, record_tape)
    var state := read_state(app)
    if not check(int(state.report.high_score) == 2000, "fresh app uses original session record 2000"):
        return false
    var completed_stages := 0
    var terminal_tick := -1
    for tick in range(MAX_REPORT_TICKS):
        if not advance(app, 16 if record_tape else 0): return false
        state = read_state(app)
        if state.settling:
            state = refresh(app)
            if state.report.game_over:
                terminal_tick = tick
                break
            var stage_before := int(state.stage)
            if state.settlement_counting:
                if not await confirm_once(app): return false
                if not check(read_state(app).settling, "stage tally confirmation only completes counting"):
                    return false
            if not await confirm_once(app): return false
            state = read_state(app)
            if not check(int(state.stage) == stage_before % 35 + 1 and state.intro,
                    "real intermediate victory continues to its next stage"):
                return false
            completed_stages += 1
        if tick % 600 == 0:
            await app.get_tree().process_frame
    if not check(terminal_tick >= 0 and state.report.game_over,
            "normal controls reach real game over within 20,000 ticks"):
        return false
    var maximum_score := 0
    for player in state.report.players:
        maximum_score = maxi(maximum_score, int(player.score))
    if not check((maximum_score > 2000) == record_tape,
            "earned final score selects the intended record/non-record branch"):
        return false
    if not ParityUI.new().report_contracts(app.frontend,state): return false
    if not await capture(app, "record-report" if record_tape else "game-over-report"): return false
    if state.settlement_counting:
        # Let the real counting cadence emit ScoreCounted before GUI fast-forward.
        for tick in range(24):
            if not advance(app): return false
        refresh(app)
        if read_state(app).settlement_counting:
            if not await confirm_once(app): return false
            state = read_state(app)
            if not check(state.settling and not state.settlement_counting and not state.high_score,
                    "game-over first confirmation finishes its tally only"):
                return false
    if not await confirm_once(app): return false
    state = read_state(app)
    if record_tape:
        if not check(state.high_score and not state.menu_requested and
                int(state.report.high_score) == maximum_score and
                app.frontend.report_title.text == "NEW RECORD" and
                app.frontend.report_continue.text == "RETURN TO DEPLOYMENT",
                "earned record is committed and displayed, with no held-confirm dismissal"):
            return false
        if not await capture(app, "high-score"): return false
        if timeout_record:
            for tick in range(600):
                if not advance(app): return false
                state = read_state(app)
                if state.menu_requested: break
            if not check(state.menu_requested and not state.high_score,
                    "native record display expires automatically"):
                return false
        elif not await confirm_once(app):
            return false
    elif not check(not state.high_score and int(state.report.high_score) == 2000,
            "non-record defeat does not show or replace the existing record"):
        return false
    if not await handle_native_menu(app): return false
    # Menu deployment and stage restart preserve the earned session record;
    # player score and combat state reset through the production entry points.
    app.frontend.controls.deploy.pressed.emit()
    state = refresh(app)
    if not check(state.intro and not state.game_over and not state.high_score and
            int(state.report.high_score) == maxi(2000, maximum_score),
            "next menu deployment preserves the session record and clears terminal state"):
        return false
    for player in state.players:
        if not check(int(player.score) == 0, "new deployment resets the player's run score"):
            return false
    app.frontend.restart_requested.emit()
    state = refresh(app)
    if not check(state.intro and int(state.report.high_score) == maxi(2000, maximum_score),
            "stage restart preserves the session record"):
        return false
    app.return_to_menu()
    print("TANKS_UI_TERMINAL ticks=%d cleared=%d score=%d record=%s timeout=%s" %
        [terminal_tick, completed_stages, maximum_score, record_tape, timeout_record])
    return true


static func edge_camera_preserves_scale(app: Node, label: String) -> bool:
    var rig: Dictionary = app.state.camera
    var p: Array = rig.position
    var t: Array = rig.target
    var position := Vector3(p[0],p[1],p[2])
    var target := Vector3(t[0],t[1],t[2])
    var basis := Basis.looking_at(target-position,Vector3.UP)
    if not check(float(app.camera.size) == float(rig.span) and app.camera.basis.is_equal_approx(basis),
            label+" preserves the native span and direction"):
        return false
    # This fresh start is at the southern map edge: a deliberate presentation
    # translation must now show more of the battlefield, without zooming in.
    var old_edge: float = app.camera.unproject_position(Vector3(13,0,26)+app.camera.position-position).y
    var new_edge: float = app.camera.unproject_position(Vector3(13,0,26)).y
    return check(new_edge > old_edge+40.0,label+" replaces foreground apron with visible battlefield")


static func coop_model_points_safe(app: Node, label: String) -> bool:
    var safe: Rect2 = app.coop_safe_rect()
    if not check(safe.has_area() and Rect2(0,0,1,1).encloses(safe),label+": invalid normalized safe region"):
        return false
    var forward: Vector3 = -app.camera.global_basis.z
    forward.y = 0.0
    forward = forward.normalized()
    var viewport_size := Vector2(app.viewport.size)
    var active_players: Array = app.state.players.filter(func(player: Dictionary): return player.active)
    if not check(not active_players.is_empty(),label+": no active players to check"): return false
    for player in active_players:
        var key := "p%d" % int(player.id)
        if not check(app.vehicles.has(key),label+": missing active model "+key): return false
        var vehicle: Node3D = app.vehicles[key]
        if not check(vehicle.has_meta("visual_bounds"),label+": missing full-model bounds "+key): return false
        var bounds: AABB = vehicle.get_meta("visual_bounds")
        var points: Array[Vector3] = []
        for corner in 8: points.append(vehicle.to_global(bounds.get_endpoint(corner)))
        var ground := Vector3(vehicle.global_position.x,0,vehicle.global_position.z)
        points.append(ground+forward*3.0)
        points.append(ground-forward*3.0)
        for index in points.size():
            var projected: Vector2 = app.camera.unproject_position(points[index])/viewport_size
            if not check(not app.camera.is_position_behind(points[index]) and safe.grow(.00002).has_point(projected),
                    "%s: %s point %d projects to %s outside %s" % [label,key,index,projected,safe]):
                return false
    # Independently map normalized 3D coordinates through the real TextureRect
    # letterbox into the same canvas coordinates as the actual HUD control.
    var available: Rect2 = app.display.get_global_rect()
    var scale := minf(available.size.x/viewport_size.x,available.size.y/viewport_size.y)
    var scene_size := viewport_size*scale
    var scene := Rect2(available.position+(available.size-scene_size)*.5,scene_size)
    var hud: Rect2 = app.frontend.hud_root.get_global_rect()
    var safe_canvas := Rect2(scene.position+safe.position*scene.size,safe.size*scene.size)
    return check(app.frontend.hud_root.is_visible_in_tree() and hud.has_area() and scene.has_area() and
        safe_canvas.position.y >= hud.end.y-.01,label+": safe rectangle overlaps the real laid-out HUD")


static func coop_camera_scenarios(app: Node) -> bool:
    var settings: Dictionary = Frontend.DEFAULTS.duplicate()
    settings.merge({"stage":1,"players":2,"ai_p2":false,"max_hp":6,"lives":99,
        "camera_yaw":0,"camera_elevation":50},true)
    app.configuration = settings.duplicate()
    app.stage = 1
    app.player_count = 2
    app.ai_p2 = false
    app.seed_value = 20260916
    app.close_up = false
    app.frontend.configure(settings,false,app.audio_volume)
    app.frontend.show_battle()
    if not check(app.core.reset_config(20260916,settings),"co-op camera seeded native deployment"): return false
    for tick in 525:
        if not check(app.core.step(STEP,17,0),"co-op camera reproduction native step "+str(tick)): return false
    app.state = read_state(app)
    if not check(app.state.players.size() == 2 and app.state.players[0].active and app.state.players[1].active and
            absf(float(app.state.players[0].z)-float(app.state.players[1].z)) > 10.0,
            "co-op camera uses actual vertically separated native players"):
        return false
    var separated: Dictionary = app.state.duplicate(true)
    var layout_receipts: Array = []
    # Headless Window.size accepts physical dimensions without resizing its
    # actual canvas. Do not report those no-op requests as letterbox coverage.
    # Graphical integration exercises all three real window layouts; the pure
    # camera gate independently covers aspect/safe-area combinations headlessly.
    var dimensions_to_check := [Vector2i(1280,720)] if DisplayServer.get_name() == "headless" else \
        [Vector2i(1280,720),Vector2i(1280,960),Vector2i(1920,720)]
    for dimensions in dimensions_to_check:
        app.get_window().size = dimensions
        app.pixel_style = false
        app.frontend.controls.pixel.set_pressed_no_signal(false)
        app.frontend.refresh(app.state,false,0.0)
        for frame in 3: await app.get_tree().process_frame
        app.update_world(0.0)
        var label := "co-op viewport "+str(dimensions)
        if not check(app.get_window().size == dimensions,label+": window resize did not apply"): return false
        if not coop_model_points_safe(app,label): return false
        var pose: Transform3D = app.camera.transform
        var span: float = app.camera.size
        var safe: Rect2 = app.coop_safe_rect()
        app.frontend.controls.pixel.button_pressed = true
        app.update_world(0.0)
        if not check(bool(app.pixel_style) and app.camera.transform == pose and float(app.camera.size) == span and
                app.coop_safe_rect() == safe,label+": Pixel Style changed camera framing"):
            return false
        if not coop_model_points_safe(app,label+" pixel"): return false
        if not check(read_state(app) == separated,label+": presentation/layout changed the complete native snapshot"):
            return false
        layout_receipts.append({"window":[dimensions.x,dimensions.y],"display":str(app.display.get_global_rect()),
            "hud":str(app.frontend.hud_root.get_global_rect()),"safe":str(safe),"camera_span":span})
    app.get_window().size = Vector2i(1280,720)
    for frame in 3: await app.get_tree().process_frame
    # A fresh native start must clear the previous distant zoom. At the map
    # edge, solo and close co-op now pan inward while retaining native scale.
    for players in [1,2]:
        settings.players = players
        app.configuration = settings.duplicate()
        app.player_count = players
        if not check(app.core.reset_config(20260916,settings),"fresh camera reset "+str(players)): return false
        app.state = read_state(app)
        var before: Dictionary = app.state.duplicate(true)
        app.frontend.refresh(app.state,app.pixel_style,0.0)
        for frame in 2: await app.get_tree().process_frame
        app.update_world(0.0)
        if not edge_camera_preserves_scale(app,"solo" if players == 1 else "nearby co-op"): return false
        if not coop_model_points_safe(app,"fresh edge view "+str(players)): return false
        if not check(read_state(app) == before,"fresh scene presentation preserves native snapshot"): return false
    app.set_meta("coop_camera_receipts",layout_receipts)
    return visibility_native_scenarios(app)


static func visibility_native_scenarios(app: Node) -> bool:
    # Normal inputs bring P1 behind a building and then into genuine forest
    # cover in a new game. No renderer fixture edits the native map or players.
    # The stage-1 clear turn must preserve its actual z (9.99995518), not
    # round it to 10. The full-world digest includes that position and its
    # camera follow; terrain, enemies, events and the forest trace are unchanged.
    var cases := [
        {"stage":1,"elevation":40,"commands":[[434,17],[24,8],[30,0]],"position":Vector2(11,10),"hint":true,"digest":"fa2b03477944e84d"},
        {"stage":10,"elevation":50,"commands":[[300,17]],"position":Vector2(9,21.1666374),"hint":false,"digest":"7666e172236f7619"}]
    var receipts: Array = []
    for fixture in cases:
        var settings: Dictionary = Frontend.DEFAULTS.duplicate()
        settings.merge({"stage":fixture.stage,"players":2,"ai_p2":false,"max_hp":3,"lives":10,
            "camera_yaw":0,"camera_elevation":fixture.elevation},true)
        if not check(app.core.reset_config(20260916,settings),"visibility native deployment"): return false
        app.clear_effects()
        var turn_origin: Dictionary = {}
        for command in fixture.commands:
            if int(command[1]) == 8:
                turn_origin = read_state(app).players[0].duplicate()
            for tick in int(command[0]):
                if not check(app.core.step(STEP,int(command[1]),0),"visibility native input tape"): return false
        app.state = read_state(app)
        var before: Dictionary = app.state.duplicate(true)
        if not turn_origin.is_empty() and not check(before.players[0].z == turn_origin.z,
                "clear perpendicular turn preserves the unsnapped lane position"): return false
        if not check(before.digest == fixture.digest and before.players[0].active and float(before.players[0].creating) <= 0,
                "visibility scenario reaches the expected real native position"): return false
        var native_position := Vector2(float(before.players[0].x),float(before.players[0].z))
        if not check(native_position.distance_to(fixture.position) < .0001,
                "visibility scenario preserves its building or forest player position"): return false
        app.frontend.refresh(app.state,app.pixel_style,0.0)
        app.update_world(0.0)
        var hints: RefCounted = app.player_visibility
        if not check(hints.stats().players == 2 and hints.stats().buildings > 0 and hints._players.has(0),
                "actual map and live player meshes reach visibility adapter"): return false
        if not check(hints._players[0].root.visible == bool(fixture.hint),
                "building enables player hint; forest cover disables it"): return false
        var terrain_scans: int = app.terrain_scans
        for tick in 4: app.update_world(0.0)
        if not check(read_state(app) == before and app.terrain_scans == terrain_scans,
                "hint updates neither mutate native state nor rebuild unchanged terrain"): return false
        receipts.append({"stage":fixture.stage,"digest":before.digest,"hint":fixture.hint,"stats":hints.stats()})
    print("TANKS_UI_PLAYER_VISIBILITY_PASSED "+JSON.stringify(receipts))
    return true


static func check_coop_camera(app: Node) -> bool:
    # This runs before the ordinary UI scenarios. Always restore their fresh
    # deployment, actual window layout and request counters, even on failure.
    var configuration: Dictionary = app.configuration.duplicate(true)
    var seed: int = app.seed_value
    var stage: int = app.stage
    var players: int = app.player_count
    var ai_p2: bool = app.ai_p2
    var pixel: bool = app.pixel_style
    var close_up: bool = app.close_up
    var window_size: Vector2i = app.get_window().size
    var menu_open: bool = app.frontend.menu_open
    var heard: Dictionary = app.get_meta("ui_audio_requests",{}).duplicate(true)
    var initial := read_state(app)
    var passed: bool = await coop_camera_scenarios(app)
    app.configuration = configuration
    app.seed_value = seed
    app.stage = stage
    app.player_count = players
    app.ai_p2 = ai_p2
    app.pixel_style = pixel
    app.close_up = close_up
    app.get_window().size = window_size
    var restored: bool = app.core.reset_config(seed,configuration)
    app.clear_effects()
    app.clear_battle_input()
    app.frontend.configure(configuration,pixel,app.audio_volume)
    if menu_open: app.frontend.show_menu()
    else: app.frontend.show_battle()
    for frame in 3: await app.get_tree().process_frame
    app.state = read_state(app)
    app.update_world(0.0)
    app.set_meta("ui_audio_requests",heard)
    passed = check(restored and read_state(app) == initial,"co-op check restores the original fresh native session") and passed
    if passed:
        print("TANKS_UI_COOP_CAMERA_CHECKS_PASSED "+JSON.stringify(app.get_meta("coop_camera_receipts",[])))
    return passed


static func gear_phase(vehicle: Node3D) -> Array:
    var gear: Dictionary = vehicle.get_meta("gear_state")
    return [gear.left_travel,gear.right_travel,gear.left_frame,gear.right_frame]


static func present_native_gear(app: Node, dt: float) -> bool:
    var before: String = app.core.snapshot()
    app.state = JSON.parse_string(before)
    app.elapsed += dt
    app.update_world(dt)
    return check(app.core.snapshot() == before,"running-gear presentation preserves the complete native snapshot")


static func advance_gear(app: Node, bits: int) -> bool:
    if not check(app.core.step(STEP,bits,0),"running-gear native step: "+app.core.error()): return false
    var state := read_state(app)
    return present_native_gear(app,0.0 if state.paused else STEP)


static func running_gear_scenarios(app: Node) -> bool:
    var settings: Dictionary = Frontend.DEFAULTS.duplicate()
    settings.merge({"stage":1,"players":2,"ai_p2":false,"lives":10,"max_hp":3,
        "camera_yaw":0,"camera_elevation":50},true)
    app.configuration = settings.duplicate()
    app.stage = 1
    app.player_count = 2
    app.ai_p2 = false
    app.seed_value = 20260916
    app.frontend.configure(settings,false,app.audio_volume)
    app.frontend.show_battle()
    if not check(app.core.reset_config(20260916,settings),"running-gear fresh native deployment"): return false
    if not present_native_gear(app,0.0): return false
    var player: Node3D = app.vehicles.p0
    var node_id := player.get_instance_id()
    var initial_mesh: Mesh = player.get_node("TrackLeft").mesh
    var mesh_advanced := false
    for tick in 434:
        if not advance_gear(app,17): return false
        mesh_advanced = mesh_advanced or player.get_node("TrackLeft").mesh != initial_mesh
        if tick % 120 == 0: await app.get_tree().process_frame
    var travelled: Dictionary = player.get_meta("gear_state")
    if not check(mesh_advanced and float(travelled.left_travel) > 10.0 and
            float(travelled.right_travel) > 10.0 and app.state.players[0].moving,
            "real native driving advances both running-gear distance and visible mesh phase"):
        return false
    var phase := gear_phase(player)
    var breath: Dictionary = player.get_meta("motion").duplicate(true)
    for repeat in 3:
        if not present_native_gear(app,0.0): return false
    if not check(gear_phase(player) == phase and player.get_meta("motion") == breath,
            "repeated zero-dt presentation holds running gear and established breathing"):
        return false

    # Native tape: turn east beside the row-11 building, then drive south into
    # it. The first south step turns/moves; all following held steps are blocked.
    for tick in 24:
        if not advance_gear(app,8): return false
    if not advance_gear(app,2): return false
    var blocked: Dictionary = app.state.players[0].duplicate(true)
    phase = gear_phase(player)
    for tick in 59:
        if not advance_gear(app,2): return false
        var current: Dictionary = app.state.players[0]
        if not check(current.x == blocked.x and current.z == blocked.z and not current.moving and
                gear_phase(player) == phase,"holding direction against a real brick wall cannot spin running gear"):
            return false
    if not check(app.state.base_alive and not app.state.game_over and not app.state.settling,
            "running-gear wall fixture remains an ordinary live battle"):
        return false

    # The native same-stage restart deliberately retains its public tick.
    var old_tick: int = app.state.tick
    app.request_restart()
    if not check(int(app.state.tick) == old_tick and int(app.state.stage) == 1 and
            app.state.intro and app.vehicles.p0.get_instance_id() == node_id and
            gear_phase(app.vehicles.p0) == [0.0,0.0,0,0],
            "same-stage same-tick restart resets gear on the reused vehicle node"):
        return false
    if not present_native_gear(app,0.0): return false

    for tick in 300:
        if not advance_gear(app,1): return false
        if tick % 120 == 0: await app.get_tree().process_frame
    if not check(float(gear_phase(player)[0]) > 1.0,"restarted native player drives again before new deployment"):
        return false
    app.start_from_menu(settings)
    if not check(int(app.state.tick) == 0 and app.vehicles.p0.get_instance_id() == node_id and
            gear_phase(app.vehicles.p0) == [0.0,0.0,0,0],"new same-model deployment resets running-gear history"):
        return false
    for tick in 300:
        if not advance_gear(app,1): return false
        if tick % 120 == 0: await app.get_tree().process_frame
    if not check(float(gear_phase(player)[0]) > 1.0,"new native session drives before stage-boundary presentation"):
        return false
    # Start an actual native stage, then use ordinary update_world without its
    # explicit reset argument. This checks the stage/tick/intro boundary path.
    settings.stage = 2
    if not check(app.core.start_config(settings),"running-gear native stage-two deployment"): return false
    if not present_native_gear(app,STEP): return false
    if not check(int(app.state.stage) == 2 and app.vehicles.p0.get_instance_id() == node_id and
            gear_phase(app.vehicles.p0) == [0.0,0.0,0,0],"native stage boundary resets reused gear without a caller reset flag"):
        return false
    print("TANKS_UI_RUNNING_GEAR_PASSED native-drive/zero-dt/blocked-wall/same-tick-restart/new-game/stage/native-unchanged")
    return true


static func check_running_gear(app: Node) -> bool:
    # Run before ordinary UI interactions; always restore their exact fresh
    # native session and presentation settings, including after a failed check.
    var configuration: Dictionary = app.configuration.duplicate(true)
    var seed: int = app.seed_value
    var stage: int = app.stage
    var players: int = app.player_count
    var ai_p2: bool = app.ai_p2
    var elapsed: float = app.elapsed
    var menu_open: bool = app.frontend.menu_open
    var initial := read_state(app)
    var heard: Dictionary = app.get_meta("ui_audio_requests",{}).duplicate(true)
    var passed: bool = await running_gear_scenarios(app)
    app.configuration = configuration
    app.seed_value = seed
    app.stage = stage
    app.player_count = players
    app.ai_p2 = ai_p2
    app.elapsed = elapsed
    var restored: bool = app.core.reset_config(seed,configuration)
    app.clear_effects()
    app.clear_battle_input()
    app.frontend.configure(configuration,app.pixel_style,app.audio_volume)
    if menu_open: app.frontend.show_menu()
    else: app.frontend.show_battle()
    app.state = read_state(app)
    app.update_world(0.0,true)
    app.set_meta("ui_audio_requests",heard)
    return check(restored and read_state(app) == initial,
        "running-gear check restores the original fresh native session") and passed


static func present_native_fire(app: Node, p1: int, p2: int) -> bool:
    if not check(app.core.step(STEP,p1,p2),"fire-effects native step: "+app.core.error()): return false
    var before: String = app.core.snapshot()
    app.state = JSON.parse_string(before)
    var dt := 0.0 if app.state.paused else STEP
    app.elapsed += dt
    app.update_world(dt)
    return check(app.core.snapshot() == before,"fire-effects presentation preserves the complete native snapshot")


static func native_fire_scenarios(app: Node) -> bool:
    var nations: Dictionary = {}
    var sources: Dictionary = {}
    var seen_slots: Dictionary = {}
    var reused_kind_changes := 0
    var enemy_shots := 0
    var impact_count := 0
    var checked_anchor := false
    # All national player models receive actual native fire. P2 is human input,
    # so the test exercises both source IDs without installing another policy.
    for national_pair in [[0,1],[2,0]]:
        var settings: Dictionary = Frontend.DEFAULTS.duplicate()
        settings.merge({"stage":1,"players":2,"ai_p2":false,"lives":10,"max_hp":3,
            "nation_p1":national_pair[0],"nation_p2":national_pair[1],
            "camera_yaw":0,"camera_elevation":50},true)
        app.configuration = settings.duplicate()
        app.stage = 1
        app.player_count = 2
        app.ai_p2 = false
        app.seed_value = 20260916
        app.frontend.configure(settings,false,app.audio_volume)
        app.frontend.show_battle()
        if not check(app.core.reset_config(20260916,settings),"fire-effects fresh native deployment"): return false
        app.clear_effects()
        app.state = read_state(app)
        app.update_world(0.0,true)
        for tick in 420:
            if not present_native_fire(app,17,16): return false
            if not check(app.effects.size() < 48,"native fire fixture stays below the unchanged live-effect cap"):
                return false
            var visual_events: Array = []
            for event in app.state.events:
                if event.type in ["TankDestroyed","BaseDamaged","BrickHit","ShellFired","ShellCancelled"]:
                    visual_events.append(event)
            var fresh: Array = []
            for effect in app.effects:
                if is_equal_approx(float(effect.age),STEP): fresh.append(effect)
            if not check(fresh.size() == visual_events.size(),"native events retain presentation order and one effect per event"):
                return false
            for index in visual_events.size():
                var event: Dictionary = visual_events[index]
                var effect: Dictionary = fresh[index]
                var node: Node3D = effect.node
                var muzzle: bool = event.type == "ShellFired"
                if not check(bool(node.get_meta("muzzle_flash",false)) == muzzle,
                        "pooled native event restores the proper muzzle or generic effect kind"):
                    return false
                var identity := node.get_instance_id()
                if seen_slots.has(identity) and bool(seen_slots[identity]) != muzzle: reused_kind_changes += 1
                seen_slots[identity] = muzzle
                if muzzle:
                    var player_id: int = event.source_player
                    var enemy_id: int = event.source_enemy
                    if not check((player_id >= 0) != (enemy_id >= 0) and int(event.direction) in [1,2,3,4],
                            "native firing source and cardinal direction are unambiguous"):
                        return false
                    var key := "p%d"%player_id if player_id >= 0 else "e%d"%enemy_id
                    if not check(app.vehicles.has(key),"actual firing fixture retains its source vehicle"): return false
                    var vehicle: Node3D = app.vehicles[key]
                    var body: Node3D = vehicle.get_node("Body")
                    var forward: Vector3 = -body.global_basis.z
                    forward.y = 0.0
                    var cardinal: Vector3 = [Vector3.ZERO,Vector3.FORWARD,Vector3.BACK,Vector3.LEFT,Vector3.RIGHT][int(event.direction)]
                    if not check(forward.normalized().dot(cardinal) > .999,
                            "visible gun and native shell event agree on cardinal firing direction"):
                        return false
                    var expected: Vector3 = body.to_global(vehicle.get_meta("neutral_muzzle"))
                    if not check(node.global_position.is_equal_approx(expected) and
                            node.global_basis.orthonormalized().is_equal_approx(body.global_basis.orthonormalized()) and
                            is_equal_approx(float(effect.size),.23) and is_equal_approx(float(effect.life),.10),
                            "native muzzle uses the visible animated gun tip and basis without changing lifetime"):
                        return false
                    if player_id >= 0:
                        sources[player_id] = true
                        nations[int(app.state.players[player_id].nation)] = true
                    else:
                        enemy_shots += 1
                else:
                    var size := .25
                    var height := .32
                    if event.type == "TankDestroyed":
                        size = 1.0
                        height = .45
                    elif event.type == "BaseDamaged":
                        size = 1.2 if int(event.base_part) == 2 else .3
                        height = .3
                    elif event.type == "ShellCancelled":
                        size = .20
                        height = .56
                    var expected := Vector3(event.x,height+STEP*.4,event.z)
                    if not check(node.global_position.is_equal_approx(expected) and
                            node.quaternion.is_equal_approx(Quaternion.IDENTITY) and
                            is_equal_approx(float(effect.size),size) and
                            is_equal_approx(float(effect.life),.7 if size > .5 else .10),
                            "generic native impacts keep their prior translation, upward drift, scale and lifetime"):
                        return false
                    impact_count += 1
            if not checked_anchor:
                for effect in fresh:
                    var node: Node3D = effect.node
                    if not bool(node.get_meta("muzzle_flash",false)): continue
                    var anchored: Transform3D = node.global_transform
                    # A real turn on the next native tick must not reattach the
                    # already-emitted flash to the moving tank's new pose.
                    if not present_native_fire(app,8,8): return false
                    if not check(app.effects.any(func(item): return item.node == node) and
                            node.global_transform.is_equal_approx(anchored),
                            "emitted muzzle remains fixed in world space after a real vehicle turn"):
                        return false
                    if not present_native_fire(app,64,0): return false
                    var age: float = effect.age
                    var count: int = app.effects.size()
                    if not check(app.state.paused,"native fire fixture entered a real pause"): return false
                    for held in 3:
                        if not present_native_fire(app,0,0): return false
                        if not check(app.effects.size() == count and effect.age == age and
                                node.global_transform.is_equal_approx(anchored),
                                "zero-dt native pause freezes the flash without drift or duplicate events"):
                            return false
                    if not present_native_fire(app,64,0): return false
                    checked_anchor = true
                    break
            if tick % 120 == 0: await app.get_tree().process_frame
    if not check(sources.size() == 2 and nations.size() == 3 and enemy_shots > 0 and impact_count > 0 and
            reused_kind_changes > 0 and checked_anchor,
            "real firing covers both players, all nations, enemies, impacts and cross-kind pool reuse"):
        return false
    print("TANKS_UI_NATIVE_FIRE_PASSED players/nations/enemy/muzzle-transform/world-anchor/pause/reuse/impact/native-unchanged")
    return true


static func check_native_fire(app: Node) -> bool:
    var configuration: Dictionary = app.configuration.duplicate(true)
    var seed: int = app.seed_value
    var stage: int = app.stage
    var players: int = app.player_count
    var ai_p2: bool = app.ai_p2
    var elapsed: float = app.elapsed
    var menu_open: bool = app.frontend.menu_open
    var initial := read_state(app)
    var heard: Dictionary = app.get_meta("ui_audio_requests",{}).duplicate(true)
    var passed: bool = await native_fire_scenarios(app)
    app.configuration = configuration
    app.seed_value = seed
    app.stage = stage
    app.player_count = players
    app.ai_p2 = ai_p2
    app.elapsed = elapsed
    var restored: bool = app.core.reset_config(seed,configuration)
    app.clear_effects()
    app.clear_battle_input()
    app.frontend.configure(configuration,app.pixel_style,app.audio_volume)
    if menu_open: app.frontend.show_menu()
    else: app.frontend.show_battle()
    app.state = read_state(app)
    app.update_world(0.0,true)
    app.set_meta("ui_audio_requests",heard)
    return check(restored and read_state(app) == initial,
        "fire-effects check restores the original fresh native session") and passed


static func paused_frame_cache_checks(app: Node) -> bool:
    var updates: int = app.presentation_updates
    var before := read_state(app)
    for tick in 12:
        app.ui_self_test = false
        app.process_game_frame(STEP, Time.get_ticks_usec())
        app.ui_self_test = true
    if not check(app.presentation_updates == updates and app.state.paused and
            read_state(app).digest == before.digest and int(read_state(app).tick) == int(before.tick) + 12,
            "paused frames reuse the scene while native input/ticks keep running"):
        return false
    app.get_viewport().size_changed.emit()
    app.ui_self_test = false
    app.process_game_frame(STEP, Time.get_ticks_usec())
    app.ui_self_test = true
    if not check(app.presentation_updates == updates + 1, "paused resize refreshes camera and HUD once"):
        return false
    var original: bool = app.pixel_style
    app.frontend.resume_button.grab_focus()
    for pixel in [not original, original]:
        await keyboard_event(app, KEY_TAB, true)
        await keyboard_event(app, KEY_TAB, false)
        app.ui_self_test = false
        app.process_game_frame(STEP, Time.get_ticks_usec())
        app.ui_self_test = true
        updates += 1
        if not check(app.presentation_updates == updates + 1 and app.pixel_style == pixel and
                app.get_viewport().gui_get_focus_owner() == app.frontend.resume_button and
                app.frontend.pause_style.text.ends_with("ON" if pixel else "OFF"),
                "Tab reaches pixel style through real GUI dispatch without changing pause-button focus"):
            return false
    return true


static func arcade_model_state(app: Node) -> Dictionary:
    var result: Dictionary = {}
    for key in app.vehicles:
        var vehicle: Node3D = app.vehicles[key]
        var parts: Array = [vehicle.get_instance_id(),vehicle.transform]
        for name in ["Body","Hull","TrackLeft","TrackRight"]:
            var part: MeshInstance3D = vehicle.get_node(name)
            parts.append([part.transform,part.mesh])
        result[key] = parts
    return result


static func check_arcade_hud(app: Node) -> bool:
    # Layout uses independent real SubViewports: headless Window resizing is
    # not a reliable way to exercise a different canvas aspect ratio.
    var initial: String = app.core.snapshot()
    var root_pose: Transform3D = app.camera.transform
    var span: float = app.camera.size
    var model_state := arcade_model_state(app)
    var inputs: Array = [app.queued_commands,app.queued_players.duplicate(),app.modifier_fire_held.duplicate()]
    var fixtures: Array = []
    for crew in [[1,false,"solo"],[2,false,"two-player"],[2,true,"ai-p2"]]:
        var settings: Dictionary = app.configuration.duplicate(true)
        settings.players = crew[0]
        settings.ai_p2 = crew[1]
        if not check(app.core.reset_config(app.seed_value,settings),"arcade HUD native crew fixture"):
            app.core.reset_config(app.seed_value,app.configuration)
            app.core.drain_audio()
            return false
        fixtures.append({"name":crew[2],"settings":settings,"snapshot":read_state(app)})
    var native_before: String = app.core.snapshot()
    var layout = ArcadeUI.new()
    var result: Dictionary = await layout.run(app,fixtures)
    var preserved: bool = app.core.snapshot() == native_before and app.camera.transform == root_pose and \
        float(app.camera.size) == span and arcade_model_state(app) == model_state and \
        [app.queued_commands,app.queued_players,app.modifier_fire_held] == inputs
    var restored: bool = app.core.reset_config(app.seed_value,app.configuration)
    # _ready already drained the fresh preview's StageStart cue. Discard only
    # the replacement startup cue introduced by restoring this test fixture.
    app.core.drain_audio()
    if not check(preserved and restored and app.core.snapshot() == initial,
            "HUD/menu layout preserves native state, model camera and existing input state"):
        return false
    if not check(bool(result.passed) and int(result.samples) == 36 and result.layouts.size() == 9,
            "arcade HUD completes all native crew/status/layout checks: "+str(result.failures)):
        return false
    print("TANKS_UI_ARCADE_HUD_PASSED "+JSON.stringify({"samples":result.samples,"layouts":result.layouts,
        "coverage":"real SubViewport canvases; native crew snapshots plus presentation-only label stress"}))
    return true


static func run(app: Node) -> bool:
    if not check(bool(app.ui_self_test), "self-test must own stepping during GUI awaits"):
        return false
    if not await await_window_ready(app):
        return false
    if not await focus_lifecycle_checks(app):
        return false
    if not await controller_menu_checks(app):
        return false
    if not check(Input.is_ignoring_joypad_on_unfocused_application(),
            "native hardware controller input is disabled while the application is unfocused"):
        return false
    if not check(app.audio_bank.streams.size() == 22 and not app.audio_bank.enabled,
            "UI checks load all audio resources without accessing audio hardware"):
        return false
    for index in range(AudioBank.CUES.size()):
        var stream: AudioStream = app.audio_bank.streams[index]
        if not check(stream != null and stream.get_length() > 0.0 and
                is_finite(stream.get_length()), "playable audio resource: " + AudioBank.CUES[index]):
            return false
    if not ParityUI.new().radar_contracts():
        return false
    if not await check_arcade_hud(app):
        return false
    if not await check_coop_camera(app):
        return false
    if not await check_running_gear(app):
        return false
    if not await check_native_fire(app):
        return false
    var frontend: CanvasLayer = app.frontend
    frontend.show_menu()
    if not await capture(app, "menu"): return false
    frontend.controls.mode.select(1)
    frontend.controls.nation_p1.select(2)
    frontend.controls.nation_p2.select(0)
    frontend.controls.stage.value = 10
    frontend.controls.lives.value = 7
    frontend.controls.max_hp.value = 5
    frontend.controls.enemy_speed.value = 15
    frontend.controls.enemy_fire.value = -20
    frontend.controls.enemy_spawn.value = 25
    frontend.controls.camera_yaw.value = -30
    frontend.controls.camera_elevation.value = 65
    if not await background_gui_checks(app, frontend.controls.deploy, "menu", true): return false
    var state := read_state(app)
    if not check(not frontend.menu_open and int(state.stage) == 10 and int(state.player_count) == 2,
            "deployment selects the requested stage and two-player mode"):
        return false
    if not check(int(state.players[0].nation) == 2 and int(state.players[1].nation) == 0 and
            int(state.players[0].lives) == 7 and int(state.players[0].max_hp) == 5,
            "deployment preserves both nations, lives, and maximum HP"):
        return false
    if not check(int(state.settings.enemy_speed) == 15 and int(state.settings.enemy_fire) == -20 and
            int(state.settings.enemy_spawn) == 25 and int(state.camera.yaw) == -30 and
            int(state.camera.elevation) == 65, "advanced settings reach native gameplay and camera"):
        return false
    if not renderer_cache_checks(app):
        return false
    if not await keyboard_checks(app):
        return false
    for tick in range(300):
        if not advance(app): return false
    var key := InputEventKey.new()
    key.keycode = KEY_ENTER
    key.pressed = true
    app._unhandled_key_input(key)
    if not advance_commands(app): return false
    state = read_state(app)
    if not check(state.paused and frontend.pause_panel.visible, "quick pause opens the real pause panel"):
        return false
    if not await capture(app, "pause"): return false
    if not advance(app): return false
    var paused_digest: String = read_state(app).digest
    for tick in range(12):
        if not advance(app): return false
    if not check(read_state(app).digest == paused_digest, "paused native world remains frozen"):
        return false
    if not await paused_frame_cache_checks(app): return false
    if not await background_gui_checks(app, frontend.resume_button, "pause", true): return false
    if not advance_commands(app): return false
    if not check(not app.state.paused and not frontend.pause_panel.visible, "resume returns to gameplay"):
        return false
    if not check(int(app.get_meta("ui_audio_requests", {}).get(1, 0)) == 1,
            "native audio requests Pause once on entry and never on resume"):
        return false
    frontend.controls.pixel.button_pressed = true
    if not check(bool(app.pixel_style), "pixel toggle reaches presentation"):
        return false

    # Restart follows the existing stage and settings; menu deployment below
    # still uses the production start_config path through the actual GUI.
    frontend.restart_requested.emit()
    state = refresh(app)
    if not check(int(state.stage) == 10 and state.intro and not state.paused and
            int(state.players[0].lives) == 7 and int(state.settings.max_hp) == 5,
            "restart preserves current setup and re-enters its stage intro"):
        return false
    app.track_modifier_fire(key_event(KEY_CTRL, KEY_LOCATION_RIGHT, true))
    app.track_modifier_fire(key_event(KEY_ALT, KEY_LOCATION_LEFT, true))
    var before_escape: String = app.core.snapshot()
    await keyboard_event(app, KEY_ESCAPE, true)
    app.ui_self_test = false
    app.process_game_frame(0.0,Time.get_ticks_usec())
    app.ui_self_test = true
    await keyboard_event(app, KEY_ESCAPE, false)
    if not check(app.core.snapshot() == before_escape,"Escape returns to setup without stepping native gameplay"): return false
    if not check(frontend.menu_open and not frontend.pause_panel.visible and
            not frontend.report_panel.visible, "return to deployment clears battle modals"):
        return false
    if not check((int(app.input_bits(0)) & 16) == 0 and (int(app.input_bits(1)) & 16) == 0,
            "return to deployment clears held and queued modifier fire"):
        return false
    # Isolate the real pickup/report tape from RNG consumed by earlier UI
    # scenarios. Reset only the hidden preview; Enter still performs ordinary
    # menu deployment. With precise turns, seed 4 gives P1 an actual Clock at
    # tape tick 1189; its message expires at 1322, before the clear at 3513.
    if not check(app.core.reset_config(4,app.configuration),
            "seeded menu preview for native pickup and report coverage"): return false
    app.core.drain_audio() # Discard the reset preview's unheard StageStart cue.
    frontend.controls.mode.select(2)
    frontend.controls.stage.value = 1
    frontend.controls.lives.value = 3
    frontend.controls.max_hp.value = 3
    frontend.controls.enemy_speed.value = 0
    frontend.controls.enemy_fire.value = 0
    frontend.controls.enemy_spawn.value = 0
    frontend.controls.camera_yaw.value = 0
    frontend.controls.camera_elevation.value = 50
    frontend.controls.deploy.grab_focus()
    await keyboard_event(app,KEY_ENTER,true)
    for held_frame in 4:
        await app.get_tree().process_frame
        if not advance_commands(app): return false
        if not check(not read_state(app).paused,"held setup Enter cannot also pause its new native battle"): return false
    await keyboard_event(app,KEY_ENTER,false)
    if not advance_commands(app): return false
    state = read_state(app)
    if not check(not frontend.menu_open and state.ai_p2 and int(state.stage) == 1 and
            int(state.player_count) == 2 and int(state.players[0].lives) == 3,
            "menu redeployment starts the requested AI co-op session"):
        return false

    # Play real rules until the first actual tally, including native AI decisions.
    # Holding P1 fire gives both players a useful role without a second AI policy.
    var report_tick := -1
    var traced_stage_transition := false
    var saw_native_bonus := false
    var saw_bonus_expire := false
    for tick in range(MAX_REPORT_TICKS):
        if not advance(app, 16): return false
        state = read_state(app)
        if float(state.get("bonus_message_remaining",0)) > 0:
            if not saw_native_bonus:
                frontend.refresh(state,app.pixel_style,0.0)
                if not check(frontend.bonus_message.is_visible_in_tree() and
                        frontend.bonus_message.text == str(state.bonus_message),
                        "actual pickup presents its exact native message and award"): return false
                saw_native_bonus = true
        elif saw_native_bonus and not saw_bonus_expire:
            frontend.refresh(state,app.pixel_style,0.0)
            if not check(not frontend.bonus_message.is_visible_in_tree() or frontend.bonus_message.text.is_empty(),
                    "native pickup message expiration clears its overlay"): return false
            saw_bonus_expire = true
        if state.get("stage_transition", false) and not traced_stage_transition:
            # Check the production classifier against a real native clear phase.
            # No await or native step occurs while the self-test guard is down.
            app.state = state
            app.ui_self_test = false
            var phase: String = app.trace_context().phase
            app.ui_self_test = true
            if not check(phase == "stage_transition", "trace distinguishes live stage-clear transition from combat"):
                return false
            traced_stage_transition = true
        if state.get("settling", false):
            report_tick = tick
            break
        if tick % 600 == 0:
            await app.get_tree().process_frame
    if not check(saw_native_bonus and saw_bonus_expire,"real battle exercises pickup message and expiry"): return false
    if not check(report_tick >= 0, "native battle reaches a report within 20,000 ticks"):
        return false
    if not check(state.settlement_counting and int(state.ai_decisions) > 0,
            "real AI battle produces a counting report"):
        return false
    state = refresh(app)
    var report_stage: int = state.stage
    var was_game_over: bool = state.report.game_over
    if not was_game_over and not check(traced_stage_transition, "real stage clear exercised the trace classifier"):
        return false
    if not check(frontend.report_panel.visible and frontend.report_continue.has_focus(),
            "real report receives GUI focus"):
        return false
    if not ParityUI.new().report_contracts(frontend,state): return false
    if not await capture(app, "report"): return false
    if not await background_gui_checks(app, frontend.report_continue, "report", false): return false
    if not report_start_checks(app):
        return false

    # Native held-button mapping and GUI acceptance must not each send a confirm.
    # This directly covers input_bits' modal filter without requiring a real pad.
    app.queued_players[0] = 32
    if not check((int(app.input_bits(0)) & 32) == 0, "report owns native confirmation input"):
        return false
    var confirmations := [0]
    var count_confirm := func(): confirmations[0] += 1
    frontend.confirm_requested.connect(count_confirm)
    await joy_button_event(app, JOY_BUTTON_A, true)
    if not check(confirmations[0] == 1 and (int(app.queued_commands) & 32) != 0,
            "GUI accept dispatches one confirmation on press"):
        await joy_button_event(app, JOY_BUTTON_A, false)
        return false
    if not advance_commands(app): return false
    state = read_state(app)
    if not check(state.settling and not state.settlement_counting and int(state.stage) == report_stage,
            "first report confirmation finishes counting without leaving the report"):
        await joy_button_event(app, JOY_BUTTON_A, false)
        return false
    if not ParityUI.new().report_contracts(frontend,refresh(app)): return false
    for tick in range(4):
        await app.get_tree().process_frame
        if not advance_commands(app): return false
    await joy_button_event(app, JOY_BUTTON_A, false)
    if not advance_commands(app): return false
    state = read_state(app)
    if not check(confirmations[0] == 1 and state.settling and int(state.stage) == report_stage,
            "accept hold and release do not confirm a second time"):
        return false

    await keyboard_event(app, KEY_ENTER, true)
    if not check(confirmations[0] == 2, "focused report button accepts Enter once on press"):
        await keyboard_event(app, KEY_ENTER, false)
        return false
    if not advance_commands(app): return false
    for tick in range(4):
        await app.get_tree().process_frame
        if not advance_commands(app): return false
    await keyboard_event(app, KEY_ENTER, false)
    if not advance_commands(app): return false
    if not check(confirmations[0] == 2, "Enter hold and release do not repeat report confirmation"):
        return false
    state = read_state(app)
    if was_game_over:
        if state.get("high_score", false):
            await accept_event(app, true)
            if not advance_commands(app): return false
            await accept_event(app, false)
            if not advance_commands(app): return false
            state = read_state(app)
        if not check(state.menu_requested, "game-over confirmation requests the native menu"):
            return false
        # Exercise the ordinary frame's native menu-request handler as well.
        app.ui_self_test = false
        app._process(0.0)
        app.ui_self_test = true
        if not check(frontend.menu_open, "native menu request returns to deployment"):
            return false
    else:
        if not check(int(state.stage) == report_stage % 35 + 1 and state.intro and
                not frontend.report_panel.visible, "stage-clear confirmation starts the next real stage"):
            return false
        frontend.menu_requested.emit()
        if not check(frontend.menu_open, "next stage can return to deployment"):
            return false
    frontend.confirm_requested.disconnect(count_confirm)
    print("TANKS_UI_NATIVE_REPORT ticks=%d game_over=%s" % [report_tick, was_game_over])
    if not await terminal_checks(app, false): return false
    if not await terminal_checks(app, true): return false
    if not await terminal_checks(app, true, true): return false
    var heard: Dictionary = app.get_meta("ui_audio_requests", {})
    for cue in [2, 3, 8, 15, 21]:
        if not check(int(heard.get(cue, 0)) > 0,
                "real native play/count/terminal path requests " + AudioBank.CUES[cue]):
            return false
    print("TANKS_UI_AUDIO resources=22 requests=" + JSON.stringify(heard))
    print("TANKS_UI_CHECKS_PASSED settings/nations/two-player/controller-menu/frame-input/camera/quick-pause/pixel/menu/" +
        "restart/native-report/gui-accept-press-hold-release/keyboard-fire-locations/focus-clear/background-gui/focus-lifecycle/enter-start/render-cache/" +
        "game-over/record/record-timeout/session-record/audio-resources/native-audio/coop-camera/player-visibility/running-gear-lifecycle/native-fire-effects/arcade-hud/raylib-ui-parity")
    return true
