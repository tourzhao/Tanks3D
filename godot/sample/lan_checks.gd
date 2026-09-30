extends SceneTree
## Two independent headless Godot processes exercise the real native TCP path.
## The Python harness supplies separate roles; this is not physical two-Mac QA.

var core: Object
var role := ""
var output := ""
var address := ""
var target_ticks := 480
var traces: FileAccess
var observed_ticks := {}
var last_tick := -1
var first_positions: Array = []
var moved := [false, false]
var fired := [0, 0]
var ready_written := false
var started_usec := 0
var final_settings: Dictionary = {}
var poll_interval_ms := 0
var last_evidence_write := 0.0

func _initialize() -> void:
    Engine.max_fps = 240
    for argument in OS.get_cmdline_user_args():
        if argument.begins_with("--role="): role = argument.trim_prefix("--role=")
        elif argument.begins_with("--output="): output = argument.trim_prefix("--output=")
        elif argument.begins_with("--address="): address = argument.trim_prefix("--address=")
        elif argument.begins_with("--ticks="): target_ticks = int(argument.trim_prefix("--ticks="))
        elif argument.begins_with("--poll-interval-ms="): poll_interval_ms = int(argument.trim_prefix("--poll-interval-ms="))
        else:
            fail("Unexpected LAN check argument: " + argument)
            return
    if role not in ["host", "guest"] or output.is_empty() or target_ticks < 360 or poll_interval_ms < 0 or poll_interval_ms > 100:
        fail("LAN checks require host/guest role, an output directory and at least 360 ticks")
        return
    call_deferred("run")

func fail(message: String) -> void:
    push_error("TANKS_LAN_CHECK_FAILED " + role + ": " + message)
    if core != null:
        core.lan_stop()
        core = null
    quit(1)

func write_json(name: String, value: Dictionary) -> bool:
    var path := output.path_join(name)
    var pending := path + ".pending"
    var file := FileAccess.open(pending, FileAccess.WRITE)
    if file == null: return false
    file.store_string(JSON.stringify(value) + "\n")
    file.close()
    return DirAccess.rename_absolute(pending, path) == OK

func now() -> float:
    return Time.get_ticks_usec() / 1000000.0

func run() -> void:
    if not ClassDB.class_exists("TanksSampleCore"):
        fail("Native GDExtension is unavailable")
        return
    core = ClassDB.instantiate("TanksSampleCore")
    if not core.open(ProjectSettings.globalize_path("res://resources")):
        fail(core.error())
        return
    traces = FileAccess.open(output.path_join(role + ".ticks.jsonl"), FileAccess.WRITE)
    if traces == null:
        fail("Cannot open tick evidence")
        return
    var config := {"stage": 1, "players": 2, "ai_p2": false, "lives": 5,
        "nation_p1": 2, "nation_p2": 0, "max_hp": 6, "enemy_speed": -25,
        "enemy_fire": 10, "enemy_spawn": -30, "camera_yaw": 25, "camera_elevation": 60}
    if role == "host":
        if not core.lan_host(731, config, 0, now()):
            fail(core.error())
            return
        var hosted: Dictionary = JSON.parse_string(core.lan_status())
        if int(hosted.get("port", 0)) <= 0 or not write_json("endpoint.json", {
                "address": "127.0.0.1:%d" % int(hosted.port), "pid": OS.get_process_id()}):
            fail("Cannot publish the real ephemeral TCP endpoint")
            return
    elif address.is_empty() or not core.lan_join(address, 1, -35, 45, now()):
        fail("Join failed: " + core.error())
        return
    started_usec = Time.get_ticks_usec()
    while Time.get_ticks_usec() - started_usec < 30000000:
        var accepted: bool = core.lan_poll(now(), 17 if role == "host" else 24)
        # The following successful status call clears the C ABI's last error.
        var poll_error: String = core.error() if not accepted else ""
        var status: Dictionary = JSON.parse_string(core.lan_status())
        if not accepted:
            if role == "guest" and ready_written and status.get("phase", "") == "failed" and not bool(status.get("active", true)):
                var reason: String = poll_error
                if reason.is_empty() or core.step(1.0 / 60.0, 0, 0):
                    fail("Disconnected guest silently resumed offline or omitted its error")
                    return
                var frozen: Dictionary = JSON.parse_string(core.snapshot())
                core.lan_stop()
                var stopped: Dictionary = JSON.parse_string(core.lan_status())
                if stopped.get("phase", "") != "idle" or bool(stopped.get("active", true)):
                    fail("Disconnect cleanup did not reach idle")
                    return
                finish(true, reason, int(frozen.tick))
                return
            fail("Unexpected poll failure: " + poll_error)
            return
        if status.get("phase", "") == "playing":
            if int(status.get("local_player", -1)) != (0 if role == "host" else 1):
                fail("The native role selected the wrong local player")
                return
            final_settings = status.settings
            for key in ["stage", "players", "ai_p2", "lives", "nation_p1", "max_hp", "enemy_speed", "enemy_fire", "enemy_spawn"]:
                if final_settings.get(key) != config[key]:
                    fail("Authoritative room setting mismatch: " + key)
                    return
            if int(final_settings.nation_p2) != 1:
                fail("Guest nation was not negotiated into P2")
                return
            if int(final_settings.camera_yaw) != (25 if role == "host" else -35) or int(final_settings.camera_elevation) != (60 if role == "host" else 45):
                fail("Peer camera settings were not retained locally")
                return
            var state: Dictionary = JSON.parse_string(core.snapshot())
            if int(state.player_count) != 2 or bool(state.ai_p2) or int(state.players[0].nation) != 2 or int(state.players[1].nation) != 1:
                fail("Running world identity/settings differ from negotiated room")
                return
            if first_positions.is_empty():
                for player in state.players:
                    if int(player.max_hp) != 6 or int(player.lives) != 5:
                        fail("Authoritative lives/HP were not applied to the running players")
                        return
            observe(state)
            core.drain_audio()
            # A poll may consume several native ticks on a busy machine. Wait
            # for actual shared observations, not a fixed wall-time assumption
            # that every intermediate snapshot was visible to both scripts.
            if role == "guest" and last_tick >= target_ticks and now() - last_evidence_write >= 0.1:
                ready_written = write_json("guest-ready.json", {"tick": last_tick,
                    "pid": OS.get_process_id(), "observations": observed_ticks})
                last_evidence_write = now()
                if not ready_written:
                    fail("Cannot publish guest progress")
                    return
            if role == "host" and last_tick >= target_ticks and FileAccess.file_exists(output.path_join("guest-ready.json")):
                var ready: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(output.path_join("guest-ready.json")))
                var shared := 0
                var last_shared := 0
                for tick in ready.get("observations", {}):
                    if int(tick) <= 0 or not observed_ticks.has(tick): continue
                    if str(ready.observations[tick]) != str(observed_ticks[tick]):
                        fail("Peer state/RNG digest differs at tick " + str(tick))
                        return
                    shared += 1
                    last_shared = maxi(last_shared, int(tick))
                if shared >= 360 and last_shared >= target_ticks:
                    if not core.lan_stop():
                        fail(core.error())
                        return
                    var stopped: Dictionary = JSON.parse_string(core.lan_status())
                    if stopped.get("phase", "") != "idle" or bool(stopped.get("active", true)):
                        fail("Host stop did not close the room")
                        return
                    finish(true, "host requested disconnect", last_tick)
                    return
        if poll_interval_ms > 0:
            await create_timer(poll_interval_ms / 1000.0).timeout
        else:
            await process_frame
    fail("Timed out waiting for authoritative ticks or disconnect")

func observe(state: Dictionary) -> void:
    var tick := int(state.tick)
    if tick == last_tick: return
    last_tick = tick
    if first_positions.is_empty():
        for player in state.players:
            first_positions.append(Vector2(float(player.x), float(player.z)))
    for index in range(2):
        var player: Dictionary = state.players[index]
        if first_positions[index].distance_to(Vector2(float(player.x), float(player.z))) > 0.25:
            moved[index] = true
    for event in state.events:
        if event.type == "ShellFired" and int(event.source_player) in [0, 1]:
            fired[int(event.source_player)] += 1
    observed_ticks[str(tick)] = str(state.digest)
    traces.store_line(JSON.stringify({"tick": tick, "digest": str(state.digest)}))

func finish(disconnected: bool, reason: String, tick: int) -> void:
    traces.close()
    var report := {"status": "passed", "role": role, "pid": OS.get_process_id(),
        "last_tick": tick, "observations": observed_ticks.size(), "settings": final_settings,
        "local_player": 0 if role == "host" else 1, "input_mask": 17 if role == "host" else 24,
        "players_moved": moved, "player_shots": fired, "disconnected": disconnected,
        "disconnect_reason": reason, "elapsed_seconds": (Time.get_ticks_usec() - started_usec) / 1000000.0}
    if not write_json(role + ".report.json", report):
        fail("Cannot save the process report")
        return
    core = null
    print("TANKS_LAN_PROCESS_PASSED " + JSON.stringify(report))
    quit(0)
