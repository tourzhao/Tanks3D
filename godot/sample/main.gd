extends Node

const Art = preload("res://art.gd")
const SceneLighting = preload("res://scene_lighting.gd")
const Frontend = preload("res://frontend.gd")
const AudioBank = preload("res://audio_bank.gd")
const EffectPool = preload("res://effect_pool.gd")
const CoopCamera = preload("res://coop_camera.gd")
const BattlefieldCamera = preload("res://battlefield_camera.gd")
const PlayerVisibility = preload("res://player_visibility.gd")
const STEP := 1.0 / 60.0
const MUZZLE_YAWS := [0.0, 0.0, PI, PI * 0.5, -PI * 0.5]
const BASE_CELLS := [Vector2i(11, 23), Vector2i(12, 23), Vector2i(13, 23), Vector2i(14, 23),
                    Vector2i(11, 24), Vector2i(14, 24), Vector2i(11, 25), Vector2i(14, 25)]
var core: RefCounted
var state: Dictionary = {}
var world: Node3D
var terrain: Node3D
var environment_edges: Node3D
var camera: Camera3D
var viewport: SubViewport
var display: TextureRect
var pixel_material: ShaderMaterial
var frontend: CanvasLayer
var tile_nodes: Dictionary = {}
var tile_keys: Dictionary = {}
var rendered_rows: Array = []
var rendered_masks: Array = []
var rendered_base_walls: Array = []
var rendered_steel := false
var terrain_scans := 0
var vehicles: Dictionary = {}
var presented_vehicle_stage := -1
var presented_vehicle_tick := -1
var presented_vehicle_intro := false
var shells: Array[Node3D] = []
var bonuses: Array[Node3D] = []
var effects: Array[Dictionary] = []
var effect_pool: RefCounted
var player_visibility: RefCounted
var audio_bank: Node
var base: Node3D
var frame_number := 0
var elapsed := 0.0
var pixel_style := false
var ai_p2 := true
var player_count := 2
var stage := 1
var seed_value := 20260916
var demo := false
var demo_command := -1
var close_up := false
var capture_dir := ""
var frame_limit := 0
var capture_at := 300
var render_size := Vector2i(1280, 720)
var fixed_render_size := Vector2i.ZERO
var timings: Array[float] = []
var update_times: Array[float] = []
var capturing := false
var paused_for_capture := false
var benchmark := false
var benchmark_seconds := 0.0
var benchmark_stress := false
var benchmark_wide := false
var max_player_separation := 0.0
var max_camera_span := 0.0
var wide_gameplay_frames := 0
var wide_wall_seconds := 0.0
var previous_wide := false
var previous_wide_drawable := false
var previous_wide_draw_count := 0
var wide_draw_wall_seconds := 0.0
var wide_metrics: RefCounted
var benchmark_metrics: RefCounted
var trace_requested := false
var frame_trace: RefCounted
var report_requested := false
var max_live_enemies := 0
var max_shells := 0
var max_effects := 0
var max_draw_calls := 0
var max_objects := 0
var max_memory := 0.0
var capture_crop := Rect2i()
var benchmark_resets := 0
var active_frames := 0
var queued_commands := 0
var queued_players := [0, 0]
var modifier_fire_held := [0, 0]
var configuration: Dictionary = Frontend.DEFAULTS.duplicate()
var quick_start := false
var audio_volume := 0.75
var network_active := false
var network_started := false
var network_state: Dictionary = {}
var assigned_pads := [-1, -1]
var vehicle_keys: Dictionary = {}
var base_nation := -1
var wall_timings: Array[float] = []
var previous_process_us := 0
var first_process_us := 0
var previous_active := false
var ui_self_test := false
var capture_menu := false
var cancel_requested := false
var latched_pad_buttons: Dictionary = {}
var cli_settings: Dictionary = {}
var presentation_dirty := true
var presentation_updates := 0


func _enter_tree() -> void:
    # Focus notifications can arrive while detached, before a Viewport exists.
    # Synchronize from the window on entry so missing an early focus-in cannot
    # leave the deployment menu disabled (including after a scene reattachment).
    get_viewport().gui_disable_input = not get_window().has_focus()


func _ready() -> void:
    var ready_begin := Time.get_ticks_usec()
    Input.set_ignore_joypad_on_unfocused_application(true)
    # Preserve individual input edges/axis reports; sample the latest buffered
    # events immediately before this frame's simulation (see _process).
    Input.use_accumulated_input = false
    # Godot names face buttons by position: its A is Nintendo's B. Accept both
    # bottom and right faces so Switch A works as well; neither is a cancel key.
    for button in [JOY_BUTTON_A, JOY_BUTTON_B]:
        var accept_button := InputEventJoypadButton.new()
        accept_button.device = -1
        accept_button.button_index = button
        if not InputMap.action_has_event(&"ui_accept", accept_button):
            InputMap.action_add_event(&"ui_accept", accept_button)
    for arg in OS.get_cmdline_user_args():
        if arg == "--quick-start":
            quick_start = true
        elif arg == "--ui-self-test":
            ui_self_test = true
        elif arg == "--capture-menu":
            capture_menu = true
        elif arg == "--solo":
            quick_start = true
            player_count = 1
            ai_p2 = false
        elif arg == "--human-2p":
            quick_start = true
            player_count = 2
            ai_p2 = false
        elif arg == "--demo":
            demo = true
        elif arg.begins_with("--demo-command="):
            demo = true
            demo_command = clampi(int(arg.get_slice("=", 1)), 0, 31)
        elif arg == "--pixel":
            pixel_style = true
        elif arg == "--close-up":
            close_up = true
        elif arg == "--benchmark":
            benchmark = true
        elif arg == "--trace-slow-frames":
            trace_requested = true
        elif arg.begins_with("--benchmark-seconds="):
            benchmark_seconds = clampf(float(arg.get_slice("=", 1)), 0.0, 7200.0)
            benchmark = true
            demo = true
        elif arg == "--benchmark-stress":
            benchmark_stress = true
            benchmark = true
            demo = true
        elif arg == "--benchmark-wide":
            benchmark_wide = true
            benchmark = true
            demo = true
        elif arg.begins_with("--stage="):
            stage = clampi(arg.get_slice("=", 1).to_int(), 1, 35)
            cli_settings.stage = stage
        elif arg.begins_with("--seed="):
            seed_value = arg.get_slice("=", 1).to_int()
        elif arg.begins_with("--capture="):
            capture_dir = arg.trim_prefix("--capture=")
        elif arg.begins_with("--frames="):
            frame_limit = arg.get_slice("=", 1).to_int()
        elif arg.begins_with("--capture-at="):
            capture_at = arg.get_slice("=", 1).to_int()
        elif arg.begins_with("--camera-yaw="):
            cli_settings.camera_yaw = int(arg.get_slice("=", 1))
        elif arg.begins_with("--camera-elevation="):
            cli_settings.camera_elevation = int(arg.get_slice("=", 1))
        elif arg.begins_with("--nation-p1="):
            cli_settings.nation_p1 = int(arg.get_slice("=", 1))
        elif arg.begins_with("--nation-p2="):
            cli_settings.nation_p2 = int(arg.get_slice("=", 1))
        elif arg.begins_with("--render-size="):
            var dimensions: PackedStringArray = arg.get_slice("=", 1).split("x")
            if dimensions.size() == 2:
                render_size = Vector2i(dimensions[0].to_int(), dimensions[1].to_int())
                fixed_render_size = render_size
    if render_size.x < 640 or render_size.y < 360 or render_size.x > 3840 or render_size.y > 2160 or render_size.x * 9 != render_size.y * 16:
        push_error("Sample camera comparison requires a 16:9 render size from 640x360 through 3840x2160.")
        get_tree().quit(2)
        return
    # Diagnostic captures and timed runs keep their established dimensions even
    # when the host window changes; an explicit --render-size takes precedence.
    if fixed_render_size == Vector2i.ZERO and (benchmark or capture_menu or
            not capture_dir.is_empty() or frame_limit > 0):
        fixed_render_size = render_size
    if not ClassDB.class_exists("TanksSampleCore"):
        push_error("Native gameplay extension missing. Run make godot-sample.")
        get_tree().quit(2)
        return
    core = ClassDB.instantiate("TanksSampleCore")
    if not core.open(ProjectSettings.globalize_path("res://resources")):
        push_error(core.error())
        get_tree().quit(2)
        return
    if not demo and frame_limit == 0 and not ui_self_test:
        if not quick_start:
            player_count = int(configuration.players)
            ai_p2 = bool(configuration.ai_p2)
        load_profile()
    else:
        configuration.lives = 3
    configuration.stage = stage
    configuration.players = player_count
    configuration.ai_p2 = ai_p2
    configuration.merge(cli_settings, true)
    if benchmark:
        benchmark_metrics = preload("res://frame_metrics.gd").new()
    if trace_requested:
        if not benchmark:
            push_error("--trace-slow-frames requires --benchmark")
            get_tree().quit(2)
            return
        frame_trace = preload("res://frame_trace.gd").new()
    if benchmark_stress:
        configuration.merge({"players": 2, "ai_p2": true, "lives": 99, "max_hp": 6,
            "enemy_speed": 30, "enemy_fire": 30, "enemy_spawn": 30}, true)
        player_count = 2
        ai_p2 = true
        demo_command = 16
    if benchmark_wide:
        # Exercise the normal two-human camera with native movement commands.
        # No position, map, camera, or simulation state is injected.
        configuration.merge({"players": 2, "ai_p2": false, "lives": 99, "max_hp": 6}, true)
        player_count = 2
        ai_p2 = false
        wide_metrics = preload("res://frame_metrics.gd").new()
    stage = int(configuration.stage)
    if OS.get_cmdline_user_args().has("--pixel"): pixel_style = true
    # Construct every shared chassis phase before battle begins, including
    # enemy models that may first appear several stages into a session.
    Art.warm_running_gear()
    create_scene()
    create_audio()
    frontend.configure(configuration, pixel_style, audio_volume)
    if not quick_start and not demo and frame_limit == 0:
        frontend.show_menu()
    restart()
    if capture_menu:
        frontend.show_menu()
        capture_frontend.call_deferred("menu")
    if ui_self_test:
        run_ui_checks.call_deferred()
    print("TANKS_SAMPLE_READY renderer=%s driver=%s render_size=%s" % [
        RenderingServer.get_current_rendering_method(),
        RenderingServer.get_current_rendering_driver_name(), render_size])
    if frame_trace: frame_trace.initialization_ms = (Time.get_ticks_usec() - ready_begin) / 1000.0


func create_scene() -> void:
    render_size = desired_render_size(get_window().size)
    viewport = SubViewport.new()
    viewport.size = render_size
    viewport.own_world_3d = true
    viewport.render_target_update_mode = SubViewport.UPDATE_ONCE
    viewport.msaa_3d = Viewport.MSAA_2X
    add_child(viewport)
    world = Node3D.new()
    viewport.add_child(world)
    effect_pool = EffectPool.new(world)
    terrain = Node3D.new()
    world.add_child(terrain)

    world.add_child(SceneLighting.make_environment())
    world.add_child(SceneLighting.make_sun())

    # Runtime-created materials are invisible to the editor's 3D texture
    # detector. Generate a mip chain explicitly to prevent distant grass crawl.
    var grass_source: Texture2D = load("res://resources/textures/battlefield_grass.png")
    var grass_image := grass_source.get_image()
    if grass_image.is_compressed():
        grass_image.decompress()
    grass_image.generate_mipmaps()
    var grass_texture := ImageTexture.create_from_image(grass_image)

    var backdrop := MeshInstance3D.new()
    var backdrop_plane := PlaneMesh.new()
    backdrop_plane.size = Vector2(100, 100)
    backdrop.mesh = backdrop_plane
    var backdrop_material := StandardMaterial3D.new()
    backdrop_material.albedo_color = Color("465a50")
    backdrop_material.roughness = 1.0
    backdrop_material.albedo_texture = grass_texture
    backdrop_material.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC
    backdrop_material.uv1_scale = Vector3(40, 40, 1)
    backdrop.material_override = backdrop_material
    backdrop.position = Vector3(13, -0.09, 13)
    world.add_child(backdrop)

    var floor_mesh := MeshInstance3D.new()
    var plane := PlaneMesh.new()
    plane.size = Vector2(26, 26)
    floor_mesh.mesh = plane
    var ground_material := StandardMaterial3D.new()
    ground_material.albedo_color = Color("aab39e")
    ground_material.roughness = 1.0
    ground_material.albedo_texture = backdrop_material.albedo_texture
    ground_material.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC
    ground_material.uv1_scale = Vector3(13, 13, 1)
    floor_mesh.material_override = ground_material
    floor_mesh.position = Vector3(13, -0.035, 13)
    world.add_child(floor_mesh)
    base = Art.make_base()
    base.position = Vector3(13, 0, 25)
    world.add_child(base)
    camera = Camera3D.new()
    camera.projection = Camera3D.PROJECTION_ORTHOGONAL
    camera.keep_aspect = Camera3D.KEEP_HEIGHT
    camera.size = 18.5
    camera.near = 0.1
    # Godot uses camera.far for orthographic directional shadows. Keep enough
    # depth for the widest co-op view without spreading the shadow map to 150.
    camera.far = 70.0
    world.add_child(camera)
    player_visibility = PlayerVisibility.new(world)

    # Water first appears in stage 4. Register its mesh/material with the real
    # viewport during setup so Mobile can prepare the surface pipeline before
    # that transition. A material alone does not cover instance preparation.
    # Keep this one hidden instance outside terrain and all gameplay queries.
    var water_warmup := Art.make_tile("~", 0, 0, 0)
    water_warmup.name = "WaterShaderWarmup"
    water_warmup.visible = false
    world.add_child(water_warmup)

    display = TextureRect.new()
    display.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    display.texture = viewport.get_texture()
    display.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
    display.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
    display.mouse_filter = Control.MOUSE_FILTER_IGNORE
    pixel_material = ShaderMaterial.new()
    pixel_material.shader = preload("res://pixel.gdshader")
    pixel_material.set_shader_parameter("scene_size", Vector2(render_size))
    display.material = pixel_material
    add_child(display)
    frontend = Frontend.new()
    add_child(frontend)
    frontend.start_requested.connect(start_from_menu)
    frontend.resume_requested.connect(func(): queued_commands |= 64)
    frontend.restart_requested.connect(request_restart)
    frontend.menu_requested.connect(return_to_menu)
    frontend.confirm_requested.connect(func(): queued_commands |= 32)
    frontend.pixel_changed.connect(set_pixel_style)
    get_viewport().size_changed.connect(update_render_size)
    frontend.volume_changed.connect(set_audio_volume)
    frontend.network_requested.connect(start_network)
    frontend.network_cancel_requested.connect(return_to_menu)
    frontend.quit_requested.connect(func():
        save_profile()
        get_tree().quit())


func desired_render_size(window_pixels: Vector2i) -> Vector2i:
    if fixed_render_size != Vector2i.ZERO:
        return fixed_render_size
    # Fit the letterboxed 16:9 scene in actual window pixels. Window.size already
    # accounts for high DPI; the stretched 1280x720 canvas is not a pixel budget.
    # Preserve the existing Pixel grid and bound normal rendering to 1080p.
    var units := clampi(floori(minf(window_pixels.x / 16.0, window_pixels.y / 9.0)),
        40, 80 if pixel_style else 120)
    return Vector2i(units * 16, units * 9)


func update_render_size() -> void:
    presentation_dirty = true
    var desired := desired_render_size(get_window().size)
    if desired == render_size:
        return
    render_size = desired
    viewport.size = render_size
    # Menus freeze simulation and reuse the last scene frame. A resized render
    # target needs one fresh draw even while process_game_frame returns early.
    viewport.render_target_update_mode = SubViewport.UPDATE_ONCE
    pixel_material.set_shader_parameter("scene_size", Vector2(render_size))


func set_pixel_style(value: bool) -> void:
    pixel_style = value
    pixel_material.set_shader_parameter("pixel_style", value)
    update_render_size()


func create_audio() -> void:
    audio_bank = AudioBank.new()
    audio_bank.enabled = not demo and not ui_self_test
    audio_bank.volume = audio_volume
    add_child(audio_bank)
    frontend.selection_changed.connect(func():
        if frontend.menu_open: sound("menu_item_selected"))


func sound(name: String) -> void:
    audio_bank.play(AudioBank.CUES.find(name))


func drain_audio() -> Array:
    var commands: Variant = JSON.parse_string(core.drain_audio())
    if not commands is Array:
        push_error("Native audio queue returned invalid commands")
        return []
    # Starting a hidden preview world must not play its stage intro in the menu.
    if not frontend.menu_open:
        audio_bank.consume(commands)
    return commands


func restart(reset_stage: int = -1) -> void:
    var value := configuration.duplicate()
    value.stage = stage if reset_stage < 0 else reset_stage
    value.players = player_count
    value.ai_p2 = ai_p2
    if not core.reset_config(seed_value, value):
        frontend.show_menu(core.error())
        return
    configuration = value
    clear_effects()
    queued_commands = 0
    queued_players = [0, 0]
    previous_active = false
    state = JSON.parse_string(core.presentation_snapshot())
    update_world(0.0, true)


func start_from_menu(value: Dictionary) -> void:
    if network_active:
        core.lan_stop()
        network_active = false
        network_state = {}
    if not core.start_config(value):
        frontend.set_status(core.error())
        return
    configuration = value.duplicate()
    stage = int(value.stage)
    player_count = int(value.players)
    ai_p2 = bool(value.ai_p2)
    save_profile()
    # A controller's START can confirm setup as well as pause combat. Record
    # its current held edge after the native reset, before accepting battle
    # input, so opening the game cannot immediately pause it with the same press.
    clear_battle_input()
    input_bits(0)
    if player_count == 2 and not ai_p2:
        input_bits(1)
    clear_battle_input()
    frontend.show_battle()
    clear_effects()
    queued_commands = 0
    queued_players = [0, 0]
    state = JSON.parse_string(core.presentation_snapshot())
    update_world(0.0, true)


func request_restart() -> void:
    if network_active:
        queued_commands |= 128
    elif core.restart():
        clear_effects()
        queued_commands = 0
        queued_players = [0, 0]
        state = JSON.parse_string(core.presentation_snapshot())
        configuration.stage = int(state.stage)
        update_world(0.0, true)


func return_to_menu() -> void:
    if network_active:
        core.lan_stop()
    network_active = false
    network_started = false
    network_state = {}
    frontend.set_network_pending(false)
    clear_battle_input()
    audio_bank.stop_all()
    core.drain_audio()
    if core.has_method("reset_pad"):
        for slot in range(2): core.reset_pad(slot, true)
    frontend.configure(configuration, pixel_style, audio_volume)
    frontend.show_menu()


func set_audio_volume(value: float) -> void:
    audio_volume = clampf(value, 0, 1)
    if audio_bank != null:
        audio_bank.set_volume(audio_volume)


func load_profile() -> void:
    var config := ConfigFile.new()
    if config.load("user://deployment.cfg") != OK:
        return
    for key in configuration.keys():
        var value: Variant = config.get_value("deployment", key, configuration[key])
        if typeof(value) == typeof(configuration[key]):
            configuration[key] = value
    # External/corrupt preferences cannot bypass native configuration validation.
    var bounds := {"stage": [1, 35, 1], "lives": [1, 99, 1], "players": [1, 2, 1],
        "nation_p1": [0, 2, 1], "nation_p2": [0, 2, 1], "max_hp": [1, 6, 1],
        "enemy_speed": [-30, 30, 5], "enemy_fire": [-30, 30, 5], "enemy_spawn": [-30, 30, 5],
        "camera_yaw": [-45, 45, 5], "camera_elevation": [40, 70, 5]}
    for key in bounds:
        var limit: Array = bounds[key]
        configuration[key] = clampi(roundi(float(configuration[key]) / int(limit[2])) * int(limit[2]), int(limit[0]), int(limit[1]))
    if int(configuration.players) == 1:
        configuration.ai_p2 = false
    pixel_style = bool(config.get_value("presentation", "pixel", false))
    audio_volume = clampf(float(config.get_value("presentation", "volume", 0.75)), 0, 1)
    # Explicit CLI values are reapplied by _ready; the menu uses the saved mode.
    if not quick_start:
        player_count = int(configuration.players)
        ai_p2 = bool(configuration.ai_p2)
        stage = int(configuration.stage)


func save_profile() -> void:
    if ui_self_test or demo:
        return
    var config := ConfigFile.new()
    for key in configuration:
        config.set_value("deployment", key, configuration[key])
    config.set_value("presentation", "pixel", pixel_style)
    config.set_value("presentation", "volume", audio_volume)
    var result := config.save("user://deployment.cfg")
    if result != OK:
        frontend.set_status("Settings could not be saved. This session can still be played.")


func start_network(host: bool, address: String, port: int, value: Dictionary) -> void:
    var now := Time.get_ticks_usec() / 1000000.0
    core.lan_stop()
    network_active = false
    network_started = false
    network_state = {}
    var success: bool
    if host:
        success = core.lan_host(seed_value, value, port, now)
    else:
        success = core.lan_join("%s:%d" % [address, port], int(value.nation_p1), int(value.camera_yaw), int(value.camera_elevation), now)
    if not success:
        frontend.set_network_pending(false)
        frontend.set_status(core.error())
        return
    network_active = true
    network_started = false
    configuration = value.duplicate()
    frontend.configure(configuration, pixel_style, audio_volume)
    frontend.set_network_pending(true, host)
    frontend.set_status("Waiting for the other player…" if host else "Connecting…")


func update_network(local_bits: int) -> bool:
    if not core.lan_poll(Time.get_ticks_usec() / 1000000.0, local_bits):
        var problem: String = core.error()
        return_to_menu()
        frontend.set_status(problem)
        return false
    network_state = JSON.parse_string(core.lan_status())
    var status: String = str(network_state.get("phase", "Connecting…")).to_upper()
    if network_state.get("host", false):
        status += "\n" + ", ".join(network_state.get("addresses", [])) + ":" + str(network_state.get("port", 41987))
    if not str(network_state.get("error", "")).is_empty(): status += "\n" + str(network_state.error)
    network_state.status = ("P%d · %s" % [int(network_state.get("local_player", 0)) + 1, str(network_state.get("role", "")).to_upper()]) if network_state.get("phase", "") == "playing" else status.replace("\n", " ")
    frontend.set_status(status + "\nEsc cancels the connection.")
    if network_state.get("phase", "") == "playing" and not network_started:
        network_started = true
        var connected_snapshot: Dictionary = JSON.parse_string(core.presentation_snapshot())
        configuration = connected_snapshot.get("settings", configuration).duplicate()
        for key in configuration:
            configuration[key] = bool(configuration[key]) if key == "ai_p2" else int(configuration[key])
        stage = int(connected_snapshot.get("stage", stage))
        player_count = 2
        ai_p2 = false
        frontend.set_network_pending(false)
        frontend.show_battle()
    if network_state.get("phase", "") in ["failed", "disconnected", "closed"]:
        var detail: String = str(network_state.get("error", network_state.get("status", "Connection ended.")))
        return_to_menu()
        frontend.set_status(detail)
        return false
    return network_started


func _unhandled_key_input(event: InputEvent) -> void:
    if capturing or not event.is_pressed() or event.is_echo():
        return
    if event.keycode == KEY_F11:
        if DisplayServer.get_name() != "headless":
            var window := get_window()
            window.mode = Window.MODE_WINDOWED if window.mode in [Window.MODE_FULLSCREEN, Window.MODE_EXCLUSIVE_FULLSCREEN] else Window.MODE_FULLSCREEN
        get_viewport().set_input_as_handled()
        return
    if frontend.menu_open:
        if event.keycode == KEY_ESCAPE and network_active:
            return_to_menu()
        return
    if event.keycode == KEY_R:
        request_restart()
    elif event.keycode == KEY_ESCAPE:
        cancel_requested = true
    elif event.keycode == KEY_ENTER:
        queued_commands |= 32 if frontend.report_panel.visible else 64
    else:
        var key_sets := [[KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_SPACE], [KEY_W, KEY_S, KEY_A, KEY_D, KEY_F]]
        for slot in range(2):
            var index: int = key_sets[slot].find(event.keycode)
            if index >= 0:
                queued_players[slot] |= 1 << index


func track_modifier_fire(event: InputEventKey) -> void:
    # Godot's global Ctrl/Alt state does not distinguish their physical sides.
    # Each side belongs to one player; keep Ctrl and Alt independently held.
    var key := event.physical_keycode if event.physical_keycode != KEY_NONE else event.keycode
    var bit := 1 if key == KEY_CTRL else (2 if key == KEY_ALT else 0)
    if bit == 0 or event.echo:
        return
    var slot := 0 if event.location == KEY_LOCATION_RIGHT else (1 if event.location == KEY_LOCATION_LEFT else -1)
    if slot < 0:
        # An ambiguous release can safely end a hold, but must never fire both
        # players. macOS/Godot 4.7 supplies locations on normal modifier events.
        if not event.pressed:
            for index in range(2): modifier_fire_held[index] &= ~bit
        return
    if event.pressed:
        modifier_fire_held[slot] |= bit
        queued_players[slot] |= 16
    else:
        modifier_fire_held[slot] &= ~bit


func clear_battle_input() -> void:
    queued_commands = 0
    queued_players = [0, 0]
    modifier_fire_held = [0, 0]
    cancel_requested = false
    latched_pad_buttons.clear()


func filter_modal_confirmation(bits: int, report_start_confirm: bool) -> int:
    # A/Enter belong to the focused GUI button. Start has no default ui_accept
    # binding, so preserve its native rising-edge confirmation on reports.
    if frontend.pause_panel.visible or frontend.report_panel.visible:
        bits &= ~32
    if report_start_confirm:
        bits |= 32
    return bits


func input_bits(slot: int) -> int:
    var report_start_confirm := false
    var result: int = queued_players[slot]
    queued_players[slot] = 0
    if modifier_fire_held[slot] != 0:
        result |= 16
    var keys := [KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_SPACE] if slot == 0 else [KEY_W, KEY_S, KEY_A, KEY_D, KEY_F]
    for index in range(keys.size()):
        if Input.is_key_pressed(keys[index]):
            result |= 1 << index
    # Enter is a press-edge command from the GUI or _unhandled_key_input.
    # Polling its held state would turn a menu's START press into an immediate
    # PAUSE as soon as that same press opens the battlefield.
    var pads := Input.get_connected_joypads()
    for index in range(2):
        if assigned_pads[index] >= 0 and not pads.has(assigned_pads[index]):
            if core.has_method("reset_pad"): core.reset_pad(index, false)
            assigned_pads[index] = -1
    for pad in pads:
        if not assigned_pads.has(pad):
            var vacancy: int = assigned_pads.find(-1)
            if vacancy >= 0: assigned_pads[vacancy] = pad
    var mapped_pads: Array = [assigned_pads[slot]]
    if slot == 0 and (ai_p2 or player_count == 1 or network_active):
        mapped_pads = assigned_pads
    for pad in mapped_pads:
        if pad < 0: continue
        var axis := Vector2(Input.get_joy_axis(pad, JOY_AXIS_LEFT_X), Input.get_joy_axis(pad, JOY_AXIS_LEFT_Y))
        var buttons: int = latched_pad_buttons.get(pad, 0)
        latched_pad_buttons.erase(pad)
        for entry in [[JOY_BUTTON_DPAD_UP, 1], [JOY_BUTTON_DPAD_DOWN, 2], [JOY_BUTTON_DPAD_LEFT, 4], [JOY_BUTTON_DPAD_RIGHT, 8]]:
            if Input.is_joy_button_pressed(pad, entry[0]): buttons |= entry[1]
        if Input.is_joy_button_pressed(pad, JOY_BUTTON_A) or Input.is_joy_button_pressed(pad, JOY_BUTTON_X) or Input.is_joy_button_pressed(pad, JOY_BUTTON_RIGHT_SHOULDER) or Input.get_joy_axis(pad, JOY_AXIS_TRIGGER_RIGHT) > 0.5:
            buttons |= 16
        if Input.is_joy_button_pressed(pad, JOY_BUTTON_A): buttons |= 32
        if Input.is_joy_button_pressed(pad, JOY_BUTTON_START): buttons |= 64
        if Input.is_joy_button_pressed(pad, JOY_BUTTON_BACK): buttons |= 128
        if core.has_method("map_pad"):
            var mapped: int = core.map_pad(assigned_pads.find(pad), axis.x, axis.y, buttons, int(configuration.camera_yaw))
            if frontend.report_panel.visible and (buttons & 64) != 0 and (mapped & 32) != 0:
                report_start_confirm = true
            cancel_requested = cancel_requested or (mapped & 128) != 0
            result |= mapped & 127
        else:
            var dpad := buttons & 15
            if dpad:
                result |= dpad
            elif axis.length() > 0.2:
                var rotated := axis.rotated(deg_to_rad(-float(configuration.camera_yaw)))
                result |= (8 if rotated.x > 0 else 4) if absf(rotated.x) > absf(rotated.y) else (2 if rotated.y > 0 else 1)
            result |= buttons & 112
    return filter_modal_confirmation(result, report_start_confirm)


func _process(delta: float) -> void:
    var begin := Time.get_ticks_usec()
    if frame_trace: frame_trace.enter(begin, trace_context(), trace_pipeline_counters())
    # Deliver any events queued after the engine's preceding flush before menu
    # guards and native input sampling. This does not add a simulation step.
    Input.flush_buffered_events()
    if frame_trace: frame_trace.mark("input_dispatch", Time.get_ticks_usec())
    process_game_frame(delta, begin)
    if frame_trace: frame_trace.finish(Time.get_ticks_usec(), trace_context())
    if report_requested:
        report_requested = false
        write_report()
        get_tree().quit()


func trace_context() -> Dictionary:
    var phase := "combat"
    if core == null or state.is_empty(): phase = "uninitialized"
    elif ui_self_test: phase = "ui_self_test"
    elif paused_for_capture: phase = "capture"
    elif network_active and not network_started: phase = "network_wait"
    elif frontend.menu_open: phase = "menu"
    elif state.get("high_score", false): phase = "high_score"
    elif state.get("settling", false): phase = "settling"
    elif state.get("game_over", false): phase = "game_over"
    elif state.get("paused", false): phase = "paused"
    elif state.get("intro", false): phase = "intro"
    elif state.get("stage_transition", false): phase = "stage_transition"
    return {"phase": phase, "tick": int(state.get("tick", -1)), "resets": benchmark_resets,
        "window_focused": get_window().has_focus(),
        "window_drawable": DisplayServer.window_can_draw(get_window().get_window_id()),
        "frames_drawn": Engine.get_frames_drawn(),
        "stage": int(state.get("stage", stage)), "terrain_scans": terrain_scans,
        "cached_meshes": Art._meshes.size(), "cached_materials": Art._materials.size(),
        "effects": effects.size(), "vehicles": vehicles.size(),
        "effect_pool": effect_pool.stats() if effect_pool else {},
        "nodes": int(Performance.get_monitor(Performance.OBJECT_NODE_COUNT)),
        "resources": int(Performance.get_monitor(Performance.OBJECT_RESOURCE_COUNT))}


func trace_pipeline_counters() -> Dictionary:
    return {"canvas": Performance.get_monitor(Performance.PIPELINE_COMPILATIONS_CANVAS),
        "mesh": Performance.get_monitor(Performance.PIPELINE_COMPILATIONS_MESH),
        "surface": Performance.get_monitor(Performance.PIPELINE_COMPILATIONS_SURFACE),
        "draw": Performance.get_monitor(Performance.PIPELINE_COMPILATIONS_DRAW),
        "specialization": Performance.get_monitor(Performance.PIPELINE_COMPILATIONS_SPECIALIZATION)}


func process_game_frame(delta: float, begin: int) -> void:
    var interval := (begin - previous_process_us) / 1000.0 if previous_process_us > 0 else 0.0
    previous_process_us = begin
    if first_process_us == 0: first_process_us = begin
    if core == null or state.is_empty() or paused_for_capture or ui_self_test:
        previous_active = false
        return
    if frontend.menu_open and not network_active:
        previous_active = false
        return
    if benchmark and (state.get("game_over", false) or state.get("settling", false) or state.get("menu_requested", false)):
        restart()
        benchmark_resets += 1
        previous_wide = false
        previous_wide_drawable = false
    if frame_trace: frame_trace.mark("restart_and_guards", Time.get_ticks_usec())
    var accepts_input := get_window().has_focus()
    var p1 := (input_bits(0) | queued_commands) if accepts_input else 0
    queued_commands = 0
    var p2 := input_bits(1) if accepts_input and player_count == 2 and not ai_p2 and not network_active else 0
    if cancel_requested:
        cancel_requested = false
        return_to_menu()
        return
    if demo:
        var segment: int = (frame_number / 180) % 6
        p1 = demo_command if demo_command >= 0 else [1 | 16, 8 | 16, 1 | 16, 4 | 16, 2 | 16, 16][segment]
        p2 = 0
        if benchmark_wide:
            p1 = 1 | 16
            p2 = 16
    var dt := STEP if demo else minf(delta, 0.05)
    var was_paused: bool = state.get("paused", false)
    if frame_trace: frame_trace.mark("input", Time.get_ticks_usec())
    if network_active:
        if not update_network(p1):
            previous_active = false
            return
    elif not core.step(dt, p1, p2):
        frontend.show_menu(core.error())
        return
    if frame_trace: frame_trace.mark("native", Time.get_ticks_usec())
    state = JSON.parse_string(core.presentation_snapshot())
    if frame_trace: frame_trace.mark("snapshot", Time.get_ticks_usec())
    if state.get("menu_requested", false) and not benchmark and not demo:
        return_to_menu()
        return
    var visual_dt := 0.0 if state.get("paused", false) else dt
    elapsed += visual_dt
    # Keep native input edges, LAN polling/ticks and ordered audio live. Only
    # reuse presentation when both snapshots are paused and nothing changed
    # locally; resume/restart/resize/pixel changes must render immediately.
    if not was_paused or not state.get("paused", false) or presentation_dirty:
        update_world(visual_dt)
    else:
        frontend.refresh_control_hints(state, network_state if network_active else {})
        drain_audio()
    if frame_trace: frame_trace.mark("presentation", Time.get_ticks_usec())
    var active: bool = not state.get("intro", false) and not state.get("paused", false) and not state.get("game_over", false) and not state.get("settling", false)
    if active: active_frames += 1
    if benchmark and active:
        benchmark_metrics.record(delta * 1000.0, (Time.get_ticks_usec() - begin) / 1000.0,
            interval if previous_active else 0.0, (begin - first_process_us) / 1000000.0)
        var live_enemies := 0
        for enemy in state.get("enemies", []):
            if not enemy.destroyed and enemy.creating <= 0.0: live_enemies += 1
        max_live_enemies = maxi(max_live_enemies, live_enemies)
        max_shells = maxi(max_shells, state.get("shells", []).size())
        max_effects = maxi(max_effects, effects.size())
        if benchmark_wide:
            var wide_now := false
            var drawable := get_window().has_focus() and DisplayServer.window_can_draw(get_window().get_window_id())
            var drawn := Engine.get_frames_drawn()
            var players: Array = state.get("players", [])
            if players.size() == 2 and players[0].active and players[1].active and players[0].creating <= 0.0 and players[1].creating <= 0.0:
                var separation := Vector2(float(players[0].x) - float(players[1].x),
                    float(players[0].z) - float(players[1].z)).length()
                max_player_separation = maxf(max_player_separation, separation)
                max_camera_span = maxf(max_camera_span, camera.size)
                if separation >= 10.0 and camera.size > 18.51:
                    wide_gameplay_frames += 1
                    wide_now = true
                    if previous_wide and previous_active: wide_wall_seconds += interval / 1000.0
                    if previous_wide and previous_active and previous_wide_drawable and drawable and drawn > previous_wide_draw_count:
                        wide_draw_wall_seconds += interval / 1000.0
                        wide_metrics.record(delta * 1000.0, (Time.get_ticks_usec() - begin) / 1000.0,
                            interval, (begin - first_process_us) / 1000000.0)
            previous_wide = wide_now
            previous_wide_drawable = drawable
            previous_wide_draw_count = drawn
    elif not benchmark and frame_limit > 0:
        update_times.append((Time.get_ticks_usec() - begin) / 1000.0)
        timings.append(delta * 1000.0)
        if active and previous_active and interval > 0:
            wall_timings.append(interval)
        if timings.size() > 108000:
            timings.pop_front()
            update_times.pop_front()
        if wall_timings.size() > 108000: wall_timings.pop_front()
    previous_active = active
    if not active: previous_wide = false
    max_draw_calls = maxi(max_draw_calls, int(Performance.get_monitor(Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME)))
    max_objects = maxi(max_objects, int(Performance.get_monitor(Performance.RENDER_TOTAL_OBJECTS_IN_FRAME)))
    max_memory = maxf(max_memory, Performance.get_monitor(Performance.MEMORY_STATIC))
    frame_number += 1
    if not capture_dir.is_empty() and frame_number == capture_at and not capturing:
        capture_pair()
    var duration_complete := benchmark_seconds > 0.0 and (Time.get_ticks_usec() - first_process_us) / 1000000.0 >= benchmark_seconds
    if ((frame_limit > 0 and frame_number >= frame_limit) or duration_complete) and not capturing:
        report_requested = true


func update_world(dt: float, reset_gear: bool = false) -> void:
    presentation_dirty = false
    presentation_updates += 1
    # Request a new scene frame when snapshots are presented. The deployment
    # menu reuses this texture instead of rendering an unchanged battlefield.
    viewport.render_target_update_mode = SubViewport.UPDATE_ONCE
    Art.set_visual_time(elapsed)
    var previous_terrain_scans := terrain_scans
    update_map()
    if terrain_scans != previous_terrain_scans:
        player_visibility.sync_terrain(tile_nodes, state)
    # Same-model nodes can survive restart, stage advance and LAN deployment.
    # Reset only wheel/tread history at a native session boundary, not the
    # existing suspension pose or any native state.
    var native_stage := int(state.get("stage", stage))
    var native_tick := int(state.get("tick", 0))
    var native_intro := bool(state.get("intro", false))
    reset_gear = (reset_gear or native_stage != presented_vehicle_stage
        or native_tick < presented_vehicle_tick or (native_intro and not presented_vehicle_intro))
    presented_vehicle_stage = native_stage
    presented_vehicle_tick = native_tick
    presented_vehicle_intro = native_intro
    var live: Dictionary = {}
    var shell_data: Array = state.get("shells", [])
    var has_shells := not shell_data.is_empty()
    var shell_sources: Dictionary = {}
    # Collect identities before visibility filters: inactive/destroyed owners
    # can still have shells in flight. Position/yaw/body animation stay unused.
    for player in state.get("players", []):
        var key := "p%d" % int(player.id)
        if has_shells:
            shell_sources[key] = player
        if player.active:
            update_vehicle(key, player, false, live, dt, reset_gear)
    for enemy in state.get("enemies", []):
        var key := "e%d" % int(enemy.id)
        if has_shells:
            shell_sources[key] = enemy
        if not enemy.get("destroyed", false):
            update_vehicle(key, enemy, true, live, dt, reset_gear)
    for key in vehicles.keys():
        if not live.has(key):
            vehicles[key].queue_free()
            vehicles.erase(key)
            vehicle_keys.erase(key)
    while shells.size() < shell_data.size():
        var node := Art.make_shell()
        world.add_child(node)
        shells.append(node)
    for index in range(shells.size()):
        var visible_shell := index < shell_data.size()
        shells[index].visible = visible_shell
        if visible_shell:
            var item: Dictionary = shell_data[index]
            var source_key := ("p%d" if int(item.get("owner", 0)) == 0 else "e%d") % int(item.get("owner_index", -1))
            var flight := shell_flight_presentation(item, shell_sources.get(source_key, {}))
            shells[index].visible = flight.visible
            shells[index].position = Vector3(item.x, flight.height, item.z)
            shells[index].rotation.y = atan2(-item.get("vx", 0.0), -item.get("vz", -1.0))
    var bonus_data: Array = state.get("pickups", [])
    while bonuses.size() > bonus_data.size():
        bonuses.pop_back().queue_free()
    for index in range(bonus_data.size()):
        var item: Dictionary = bonus_data[index]
        if index >= bonuses.size():
            var node := Art.make_pickup(int(item.type))
            node.set_meta("bonus_type", int(item.type))
            world.add_child(node)
            bonuses.append(node)
        elif bonuses[index].get_meta("bonus_type", -1) != int(item.type):
            bonuses[index].queue_free()
            bonuses[index] = Art.make_pickup(int(item.type))
            bonuses[index].set_meta("bonus_type", int(item.type))
            world.add_child(bonuses[index])
        bonuses[index].position = Vector3(item.x, 0.65 + sin(elapsed * 2.5) * 0.09, item.z)
        bonuses[index].rotation.y = elapsed * 0.65
        var blink_interval := 0.35 if float(item.age) < float(item.life) * 0.75 else 0.175
        bonuses[index].visible = int(float(item.age) / blink_interval) % 2 == 0
    if base_nation != int(state.get("base_nation", 0)):
        base.queue_free()
        base_nation = int(state.get("base_nation", 0))
        base = Art.make_base(base_nation)
        base.position = Vector3(13, 0, 25)
        world.add_child(base)
    base.visible = state.get("base_alive", true)
    for event in state.get("events", []):
        if event.type == "TankDestroyed":
            spawn_effect(Vector3(event.x, 0.45, event.z), 1.0)
        elif event.type == "BaseDamaged":
            spawn_effect(Vector3(event.x, 0.3, event.z), 1.2 if int(event.get("base_part", 0)) == 2 else 0.3)
        elif event.type == "BrickHit":
            spawn_effect(Vector3(event.x, 0.32, event.z), 0.25)
        elif event.type == "ShellFired":
            var source_key := "p%d" % int(event.source_player) if int(event.get("source_player", -1)) >= 0 else "e%d" % int(event.get("source_enemy", -1))
            var muzzle := Vector3(event.x, 0.7, event.z)
            # The native direction is a fallback for a source no longer drawn.
            # Its cardinal values are None/North/South/West/East; art faces -Z.
            var direction := clampi(int(event.get("direction", 0)), 0, 4)
            var muzzle_basis := Basis(Vector3.UP, MUZZLE_YAWS[direction])
            if vehicles.has(source_key):
                var source: Node3D = vehicles[source_key]
                var body: Node3D = source.get_node("Body")
                muzzle = body.to_global(source.get_meta("neutral_muzzle", Vector3(0, 0.7, -0.625)))
                muzzle_basis = body.global_basis
            spawn_effect(muzzle, 0.23, muzzle_basis, true)
        elif event.type == "ShellCancelled":
            spawn_effect(Vector3(event.x, 0.56, event.z), 0.20)
    for index in range(effects.size() - 1, -1, -1):
        var effect: Dictionary = effects[index]
        effect.age += dt
        var t: float = effect.age / effect.life
        if t >= 1:
            effect_pool.release(effect.node)
            effects.remove_at(index)
        else:
            Art.set_effect_state(effect.node, t, effect.size)
            # A gun jet stays at its firing position; impact bursts keep their
            # original upward drift. Neither follows a moving source vehicle.
            if not effect.node.get_meta("muzzle_flash", false):
                effect.node.position.y += dt * 0.4
    var rig: Dictionary = state.get("camera", {})
    if not rig.is_empty():
        var position_data: Array = rig.position
        var target_data: Array = rig.target
        var presentation := {
            "position": Vector3(position_data[0], position_data[1], position_data[2]),
            "target": Vector3(target_data[0], target_data[1], target_data[2]),
            "span": float(rig.span)}
        var players: Array = []
        for player in state.get("players", []):
            var key := "p%d" % int(player.id)
            if player.get("active", false) and vehicles.has(key):
                var vehicle: Node3D = vehicles[key]
                var bounds: AABB = vehicle.get_meta("visual_bounds")
                players.append({"center": vehicle.global_position,
                    "bounds": vehicle.global_transform * bounds})
        # Fit the actual animated silhouettes and road context below the HUD.
        # This is presentation only: the native rig and simulation stay intact.
        presentation = CoopCamera.fit(presentation, float(render_size.x) / render_size.y,
            coop_safe_rect(), players)
        # Preserve that scale and direction while spending less of the view on
        # the apron outside the map. Player/road clearance takes precedence.
        presentation = BattlefieldCamera.constrain(presentation, float(render_size.x) / render_size.y,
            coop_safe_rect(), players)
        camera.position = presentation.position
        var target: Vector3 = presentation.target
        camera.size = presentation.span
        if close_up and not state.players.is_empty():
            var player: Dictionary = state.players[0]
            target = Vector3(player.x, 0.65, player.z)
            camera.position = target + Vector3(3.0, 4.5, 5.0)
            camera.size = 5.0
        camera.look_at(target, Vector3.UP)
    player_visibility.update(camera, vehicles, state, dt)
    pixel_material.set_shader_parameter("pixel_style", pixel_style)
    frontend.refresh(state, pixel_style, dt, network_state if network_active else {}, reset_gear)
    drain_audio()


func coop_safe_rect() -> Rect2:
    # TextureRect letterboxes the scene. HUD controls use canvas coordinates,
    # so subtract only their overlap with the displayed 3D image, not a fixed
    # count of physical pixels (which fails on resized / Retina windows).
    var available := display.get_global_rect()
    if available.size.x <= 0.0 or available.size.y <= 0.0:
        available = get_viewport().get_visible_rect()
    var scale := minf(available.size.x / render_size.x, available.size.y / render_size.y)
    var scene_size := Vector2(render_size) * scale
    if scene_size.y <= 0.0:
        return Rect2(0.035, 0.15, 0.93, 0.81)
    var scene := Rect2(available.position + (available.size - scene_size) * 0.5, scene_size)
    var top := 0.025
    if frontend.hud_root.is_visible_in_tree():
        var hud: Rect2 = frontend.hud_root.get_global_rect()
        if scene.intersects(hud):
            top = clampf((hud.end.y - scene.position.y) / scene.size.y + 0.025, top, 0.75)
    return Rect2(0.035, top, 0.93, 0.96 - top)


func update_map() -> void:
    var rows: Array = state.get("map", [])
    var masks: Array = state.get("brick_masks", [])
    var steel_visible: bool = state.get("base_steel_visible", state.get("base_steel", false))
    if rows.size() != 26 or masks.size() != 676:
        return
    var base_walls: Array = state.get("base_walls", [])
    if rows == rendered_rows and masks == rendered_masks and base_walls == rendered_base_walls and steel_visible == rendered_steel:
        return
    if rows != rendered_rows:
        if is_instance_valid(environment_edges):
            world.remove_child(environment_edges)
            environment_edges.queue_free()
        environment_edges = Art.make_environment_edges(rows, masks)
        world.add_child(environment_edges)
    # Snapshots are newly parsed, read-only values. Retain only the terrain
    # fields; vehicle movement must not rebuild per-cell presentation keys.
    rendered_rows = rows
    rendered_masks = masks
    rendered_base_walls = base_walls
    rendered_steel = steel_visible
    terrain_scans += 1
    for row in range(26):
        for col in range(26):
            var index := row * 26 + col
            var tile: String = rows[row][col]
            var mask := int(masks[index])
            var key := tile + str(mask)
            var wall_index := BASE_CELLS.find(Vector2i(col, row))
            var health := int(state.base_walls[wall_index]) if wall_index >= 0 else 0
            if wall_index >= 0:
                key = "base_%s_%s" % [health, steel_visible]
            if tile_keys.get(index, "") == key:
                continue
            tile_keys[index] = key
            if tile_nodes.has(index):
                tile_nodes[index].queue_free()
                tile_nodes.erase(index)
            if wall_index >= 0 or (tile != "." and tile != " "):
                var node: Node3D = Art.make_base_wall(health, steel_visible) if wall_index >= 0 else Art.make_tile(tile, mask, row, col)
                if node != null:
                    node.position = Vector3(col + 0.5, 0, row + 0.5)
                    terrain.add_child(node)
                    tile_nodes[index] = node


func update_vehicle(key: String, data: Dictionary, enemy: bool, live: Dictionary, dt: float, reset_gear: bool = false) -> void:
    live[key] = true
    var model_key := "%d/%d/%d" % [int(data.nation), int(data.get("type", -1)), int(data.get("level", 0))]
    if vehicles.has(key) and vehicle_keys.get(key, "") != model_key:
        vehicles[key].queue_free()
        vehicles.erase(key)
    if not vehicles.has(key):
        var node := Art.make_tank(int(data.nation), enemy, int(data.get("type", 0)), int(data.id), int(data.get("level", 0)))
        vehicle_keys[key] = model_key
        world.add_child(node)
        vehicles[key] = node
    var vehicle: Node3D = vehicles[key]
    vehicle.position = Vector3(data.x, 0.0, data.z)
    vehicle.rotation.y = -float(data.yaw)
    if reset_gear:
        Art.reset_running_gear(vehicle)
    Art.set_vehicle_state(vehicle, data, dt)
    vehicle.visible = float(data.get("creating", 0)) <= 0 or int(elapsed * 10) % 2 == 0
    if not enemy:
        if not vehicle.has_node("Shield"):
            var shield := MeshInstance3D.new()
            shield.name = "Shield"
            var ring := TorusMesh.new()
            ring.inner_radius = 0.70
            ring.outer_radius = 0.75
            ring.rings = 24
            ring.ring_segments = 6
            shield.mesh = ring
            var material := StandardMaterial3D.new()
            material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
            material.albedo_color = Color("83dae0")
            shield.material_override = material
            shield.position.y = 0.06
            shield.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
            vehicle.add_child(shield)
        vehicle.get_node("Shield").visible = float(data.get("shield", 0)) > 0


# Mirrors the renderer-only C++ helper. The native origin, XZ flight and impacts
# stay untouched; a long visual gun hides the portion before its muzzle plane.
static func shell_flight_presentation(item: Dictionary, source: Dictionary = {}) -> Dictionary:
    var muzzle := Vector3(0, .56, -.625)
    if not source.is_empty():
        var enemy := int(item.get("owner", 0)) == 1
        muzzle = Art.vehicle_muzzle(int(source.get("nation", 0)), enemy,
            int(source.get("type", 0)), int(source.get("level", 0)))
    var speed := Vector2(float(item.get("vx", 0)), float(item.get("vz", 0))).length()
    var traveled := maxf(0.0, 4.0 - float(item.get("life", 0.0))) * speed
    var emerged := traveled + .625 + muzzle.z
    var fraction := clampf(emerged / 1.5, 0.0, 1.0)
    var blend := fraction * fraction * (3.0 - 2.0 * fraction)
    return {"visible": not item.get("impacting", false) and emerged >= 0.0,
        "height": lerpf(muzzle.y, .56, blend)}


func spawn_effect(position: Vector3, size: float, basis: Basis = Basis.IDENTITY, muzzle_flash: bool = false) -> void:
    if effects.size() >= 48:
        return
    var node: Node3D = effect_pool.acquire(position, size, muzzle_flash)
    if node == null:
        return
    # Copy the source's rigid orientation without erasing the effect size.
    # The flash remains in world space after spawning, as impacts do.
    node.quaternion = basis.orthonormalized().get_rotation_quaternion()
    effects.append({"node": node, "age": 0.0, "life": 0.7 if size > 0.5 else 0.10, "size": size})


func clear_effects() -> void:
    effect_pool.clear()
    effects.clear()


func capture_pair() -> void:
    capturing = true
    paused_for_capture = true
    # Diagnostics retain this frame's complete world/RNG fingerprint.
    var captured_frame := frame_number
    var captured_snapshot: Dictionary = JSON.parse_string(core.snapshot())
    clear_battle_input()
    DirAccess.make_dir_recursive_absolute(capture_dir)
    var initial := pixel_style
    for value in [false, true]:
        pixel_style = value
        pixel_material.set_shader_parameter("pixel_style", value)
        frontend.refresh(state, value, 0.0, network_state if network_active else {})
        await RenderingServer.frame_post_draw
        await RenderingServer.frame_post_draw
        var suffix := "pixel" if value else "clean"
        var image := get_viewport().get_texture().get_image()
        image.save_png(capture_dir.path_join("%s.png" % suffix))
        # Crop the actual final image, downstream of pixel processing; no rescale.
        capture_crop = tank_screen_bounds(image.get_size())
        if capture_crop.has_area():
            image.get_region(capture_crop).save_png(capture_dir.path_join("tank-native-%s.png" % suffix))
    var metadata := {"window": get_viewport().get_visible_rect().size,
        "render_size": render_size, "frame": captured_frame, "snapshot": captured_snapshot,
        "camera_position": camera.position, "camera_size": camera.size,
        "camera_rotation": camera.rotation_degrees, "close_up": close_up,
        "tank_crop_pixels": [capture_crop.size.x, capture_crop.size.y],
        "tank_crop_origin": [capture_crop.position.x, capture_crop.position.y],
        "renderer": RenderingServer.get_current_rendering_method(),
        "driver": RenderingServer.get_current_rendering_driver_name()}
    var file := FileAccess.open(capture_dir.path_join("capture.json"), FileAccess.WRITE)
    if file:
        file.store_string(JSON.stringify(metadata, "  "))
    pixel_style = initial
    pixel_material.set_shader_parameter("pixel_style", initial)
    paused_for_capture = false
    capturing = false


func tank_screen_bounds(image_size: Vector2i) -> Rect2i:
    if not vehicles.has("p0"):
        return Rect2i()
    var body: Node3D = vehicles.p0
    var bounds: AABB = body.get_meta("visual_bounds", AABB(Vector3(-0.8, 0, -1.0), Vector3(1.6, 1.5, 2.0)))
    var minimum := Vector2(INF, INF)
    var maximum := Vector2(-INF, -INF)
    for corner in range(8):
        var p := bounds.get_endpoint(corner)
        var screen := camera.unproject_position(body.global_transform * p)
        minimum = minimum.min(screen)
        maximum = maximum.max(screen)
    var scale_factor := minf(float(image_size.x) / render_size.x, float(image_size.y) / render_size.y)
    var offset := (Vector2(image_size) - Vector2(render_size) * scale_factor) * 0.5
    var pixel_min := Vector2i((minimum * scale_factor + offset).floor())
    var pixel_max := Vector2i((maximum * scale_factor + offset).ceil())
    var result := Rect2i(pixel_min, pixel_max - pixel_min)
    return result.intersection(Rect2i(Vector2i.ZERO, image_size))


func write_report() -> void:
    # Only diagnostics need the complete world/RNG digest; rendering omits it.
    var final_snapshot: Dictionary = JSON.parse_string(core.snapshot())
    var steady: Array[float] = timings.slice(mini(120, timings.size()))
    var update_steady: Array[float] = update_times.slice(mini(120, update_times.size()))
    steady.sort()
    update_steady.sort()
    var wall_steady: Array[float] = wall_timings.slice(mini(120, wall_timings.size()))
    wall_steady.sort()
    var report := {"frames": frame_number, "renderer": RenderingServer.get_current_rendering_method(),
        "driver": RenderingServer.get_current_rendering_driver_name(), "render_size": render_size,
        "pixel_style": pixel_style,
        "p50_frame_ms": percentile(steady, 0.5), "p95_frame_ms": percentile(steady, 0.95),
        "p99_frame_ms": percentile(steady, 0.99), "p95_update_ms": percentile(update_steady, 0.95),
        "p50_wall_frame_ms": percentile(wall_steady, 0.5), "p95_wall_frame_ms": percentile(wall_steady, 0.95),
        "p99_wall_frame_ms": percentile(wall_steady, 0.99), "wall_timing_samples": wall_steady.size(),
        "wall_sample_clock": "Time.get_ticks_usec", "wall_observed_seconds": (Time.get_ticks_usec() - first_process_us) / 1000000.0,
        "max_draw_calls": max_draw_calls, "max_objects": max_objects, "max_static_bytes": max_memory,
        "final_digest": final_snapshot.get("digest", ""), "stage": stage, "seed": seed_value,
        "demo_fixed_step": demo, "active_gameplay_frames": active_frames, "benchmark_resets": benchmark_resets,
        "timing_samples": steady.size(), "terrain_scans": terrain_scans,
        "note": "Process delta includes vsync; not GPU timing. Excludes first 120 samples. Benchmark collects active combat only, resets on end."}
    if benchmark:
        report.merge(benchmark_metrics.report(report.wall_observed_seconds), true)
        report.merge({"benchmark_seconds": benchmark_seconds, "benchmark_stress": benchmark_stress,
            "benchmark_wide": benchmark_wide, "wide_gameplay_frames": wide_gameplay_frames,
            "wide_wall_seconds": wide_wall_seconds,
            "max_player_separation": max_player_separation, "max_camera_span": max_camera_span,
            "max_live_enemies": max_live_enemies, "max_shells": max_shells, "max_effects": max_effects,
            "benchmark_settings": configuration.duplicate(true)}, true)
        if benchmark_wide:
            report["wide_draw_wall_seconds"] = wide_draw_wall_seconds
            report["wide_draw_timing"] = wide_metrics.report(report.wall_observed_seconds)
    if frame_trace:
        report["slow_frame_trace"] = frame_trace.report()
        # Last available observations; monitor readback can lag the final draw.
        report["pipeline_counters_observed_at_report"] = trace_pipeline_counters()
    if (benchmark or frame_trace) and effect_pool:
        report["effect_pool"] = effect_pool.stats()
    print("TANKS_SAMPLE_REPORT " + JSON.stringify(report))
    if not capture_dir.is_empty():
        DirAccess.make_dir_recursive_absolute(capture_dir)
        var file := FileAccess.open(capture_dir.path_join("performance.json"), FileAccess.WRITE)
        if file:
            file.store_string(JSON.stringify(report, "  "))


func percentile(values: Array[float], fraction: float) -> float:
    return values[mini(values.size() - 1, int(values.size() * fraction))] if not values.is_empty() else 0.0


func capture_frontend(name: String) -> void:
    if capture_dir.is_empty():
        capture_dir = ProjectSettings.globalize_path("res://../ui-capture")
    DirAccess.make_dir_recursive_absolute(capture_dir)
    await RenderingServer.frame_post_draw
    await RenderingServer.frame_post_draw
    get_viewport().get_texture().get_image().save_png(capture_dir.path_join(name + ".png"))
    if capture_menu: get_tree().quit()


func run_ui_checks() -> void:
    var valid: bool = await preload("res://ui_checks.gd").run(self)
    get_tree().quit(0 if valid else 4)


func _notification(what: int) -> void:
    if what == NOTIFICATION_APPLICATION_FOCUS_OUT:
        # Gameplay sampling is focus-gated separately. Also stop GUI dispatch:
        # buttons invoke deployment/restart directly, outside the native step.
        if is_inside_tree(): get_viewport().gui_disable_input = true
        clear_battle_input()
        if core != null:
            for slot in range(2): core.reset_pad(slot, true)
    elif what == NOTIFICATION_APPLICATION_FOCUS_IN:
        if is_inside_tree(): get_viewport().gui_disable_input = false
    if what == NOTIFICATION_WM_CLOSE_REQUEST and not demo and not ui_self_test:
        save_profile()


func _input(event: InputEvent) -> void:
    if frontend != null and not capturing and not paused_for_capture and get_window().has_focus():
        # Presentation only: observe the same assigned controllers accepted by
        # gameplay, without consuming an input or changing player ownership.
        frontend.note_input(event, frontend.menu_open or assigned_pads.has(event.device))
    if event is InputEventKey:
        # Tab is the published presentation shortcut, including while paused.
        # Consume it before a focused pause/report button can navigate with it.
        if event.keycode == KEY_TAB and event.pressed and not event.echo and \
                not frontend.menu_open and not capturing and not paused_for_capture and \
                not get_viewport().gui_disable_input:
            set_pixel_style(not pixel_style)
            get_viewport().set_input_as_handled()
            return
        # Always observe releases, including one consumed by a focused Control
        # or delivered while captures/menus suspend gameplay input.
        if not event.pressed or (not frontend.menu_open and not capturing and not paused_for_capture and get_window().has_focus()):
            track_modifier_fire(event)
    if event is InputEventJoypadButton and event.pressed and not frontend.menu_open and not capturing and get_window().has_focus():
        var bits := 0
        match event.button_index:
            JOY_BUTTON_DPAD_UP: bits = 1
            JOY_BUTTON_DPAD_DOWN: bits = 2
            JOY_BUTTON_DPAD_LEFT: bits = 4
            JOY_BUTTON_DPAD_RIGHT: bits = 8
            JOY_BUTTON_A: bits = 16 | 32
            JOY_BUTTON_X, JOY_BUTTON_RIGHT_SHOULDER: bits = 16
            JOY_BUTTON_START: bits = 64
            JOY_BUTTON_BACK: bits = 128
        latched_pad_buttons[event.device] = int(latched_pad_buttons.get(event.device, 0)) | bits
