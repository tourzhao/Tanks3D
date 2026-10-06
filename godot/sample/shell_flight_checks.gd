extends SceneTree

const Main = preload("res://main.gd")
const Art = preload("res://art.gd")


# Exercise the actual update_world roster/source/shell integration without
# entering Main's scene lifecycle or creating a native gameplay session. Only
# unrelated terrain, vehicle art, HUD, visibility and audio are replaced.
class ShellWorld extends "res://main.gd":
    func update_map() -> void:
        pass

    func update_vehicle(key: String, _data: Dictionary, _enemy: bool,
            live: Dictionary, _dt: float, _reset_gear: bool = false) -> void:
        live[key] = true
        if not vehicles.has(key):
            var node := Node3D.new()
            world.add_child(node)
            vehicles[key] = node

    func drain_audio() -> Array:
        return []


class PassiveFrontend extends CanvasLayer:
    func refresh(_state: Dictionary, _pixel: bool, _dt: float,
            _network: Dictionary = {}, _reset: bool = false) -> void:
        pass


class PassiveVisibility extends RefCounted:
    func update(_camera: Camera3D, _vehicles: Dictionary,
            _state: Dictionary, _dt: float) -> void:
        pass


func require(condition: bool, detail: String) -> bool:
    if not condition:
        push_error("Shell source integration failed: " + detail)
    return condition


func check_shell_source(app: Node, index: int, source: Dictionary) -> bool:
    var item: Dictionary = app.state.shells[index]
    var flight: Dictionary = Main.shell_flight_presentation(item, source)
    var node: Node3D = app.shells[index]
    return require(node.visible == flight.visible and
        node.position.is_equal_approx(Vector3(item.x, flight.height, item.z)) and
        is_equal_approx(node.rotation.y, atan2(-float(item.vx), -float(item.vz))),
        "sparse owner identity, native XZ/velocity and flight presentation for slot %d" % index)


func check_world_sources() -> bool:
    var app := ShellWorld.new()
    app.world = Node3D.new()
    app.add_child(app.world)
    app.viewport = SubViewport.new()
    app.add_child(app.viewport)
    app.base = Node3D.new()
    app.world.add_child(app.base)
    app.base_nation = 0
    app.pixel_material = ShaderMaterial.new()
    app.pixel_material.shader = preload("res://pixel.gdshader")
    app.frontend = PassiveFrontend.new()
    app.add_child(app.frontend)
    app.player_visibility = PassiveVisibility.new()
    var player := {"id": 42, "nation": 0, "level": 0, "active": true}
    var inactive := {"id": 901, "nation": 1, "level": 3, "active": false}
    var enemy := {"id": 77, "nation": 1, "type": 2, "destroyed": false}
    # Deliberately shares the inactive player's sparse ID: owner kind must
    # distinguish their different mounts even after both leave the roster.
    var destroyed := {"id": 901, "nation": 2, "type": 0, "destroyed": true}
    app.state = {"players": [player, inactive], "enemies": [enemy, destroyed], "shells": []}
    var initial: Dictionary = app.state.duplicate(true)
    app.update_world(0.0)
    var passed := require(app.shells.is_empty() and app.state == initial and app.core == null,
        "an empty shell frame creates no shell nodes or native session and preserves its snapshot")
    app.state.shells = [
        {"owner": 0, "owner_index": 901, "x": 1.0, "z": 2.0, "vx": 8.0, "vz": 0.0, "life": 4.0},
        {"owner": 1, "owner_index": 901, "x": 3.0, "z": 4.0, "vx": 0.0, "vz": -8.0, "life": 4.0},
        {"owner": 0, "owner_index": 42, "x": 5.0, "z": 6.0, "vx": -8.0, "vz": 0.0, "life": 3.95},
        {"owner": 1, "owner_index": 77, "x": 7.0, "z": 8.0, "vx": 0.0, "vz": 8.0, "life": 3.95},
        {"owner": 1, "owner_index": 12345, "x": 9.0, "z": 10.0, "vx": 8.0, "vz": 0.0, "life": 4.0}]
    var original: Dictionary = app.state.duplicate(true)
    app.update_world(0.0)
    passed = require(app.vehicles.has("p42") and app.vehicles.has("e77") and
        not app.vehicles.has("p901") and not app.vehicles.has("e901"),
        "inactive/destroyed owners remain sources without becoming visible vehicles") and passed
    for index in range(5):
        passed = check_shell_source(app, index, [inactive, destroyed, player, enemy, {}][index]) and passed
    passed = require(app.state == original and app.core == null,
        "source lookup never mutates snapshot or initializes native state") and passed
    # A later roster may omit owners entirely; lookup must use the current
    # frame's fallback instead of retaining a stale destroyed-owner identity.
    app.state.players = []
    app.state.enemies = []
    app.update_world(0.0)
    for index in range(5):
        passed = check_shell_source(app, index, {}) and passed
    app.state.shells = []
    app.update_world(0.0)
    for node in app.shells:
        passed = require(not node.visible, "empty shell frame hides retained shell nodes") and passed
    app.free()
    return passed


func _initialize() -> void:
    var passed := true
    for nation in range(3):
        for tier in range(4):
            var source := {"id": 0, "nation": nation, "level": tier}
            var muzzle: Vector3 = Art._vehicle_profile(nation, false, 0, tier).muzzle
            for velocity in [Vector2(8, 0), Vector2(-8, 0), Vector2(0, 8), Vector2(0, -8)]:
                var item := {"owner": 0, "owner_index": 0, "vx": velocity.x,
                    "vz": velocity.y, "life": 4.0, "impacting": false}
                var original := item.duplicate(true)
                var flight: Dictionary = Main.shell_flight_presentation(item, source)
                passed = passed and not flight.visible and is_equal_approx(flight.height, muzzle.y) and item == original
                item.life = 4.0 - (-muzzle.z - .625 + .001) / 8.0
                flight = Main.shell_flight_presentation(item, source)
                passed = passed and flight.visible and absf(flight.height - muzzle.y) < .00001
                # Owner turns/movement/destruction never alter a fired trajectory.
                source.merge({"yaw": 1.4, "x": 99, "z": 99, "destroyed": true}, true)
                passed = passed and flight == Main.shell_flight_presentation(item, source)
                item.life = 3.0
                flight = Main.shell_flight_presentation(item, source)
                passed = passed and flight.visible and is_equal_approx(flight.height, .56)
                item.impacting = true
                passed = passed and not Main.shell_flight_presentation(item, source).visible
                passed = passed and original.life == 4.0
            for role in range(4):
                var enemy := {"nation": nation, "type": role, "destroyed": true}
                var item := {"owner": 1, "vx": 8.0, "vz": 0.0, "life": 4.0}
                var flight: Dictionary = Main.shell_flight_presentation(item, enemy)
                var mount: Vector3 = Art._vehicle_profile(nation, true, role, 0).muzzle
                passed = passed and not flight.visible and is_equal_approx(flight.height, mount.y)
    var orphan := {"owner": 1, "vx": 8.0, "vz": 0.0, "life": 4.0}
    passed = passed and Main.shell_flight_presentation(orphan).visible
    passed = check_world_sources() and passed
    if passed:
        print("TANKS_SHELL_FLIGHT_CHECKS_PASSED 12-models/four-directions/owner-lifecycle/native-impact source-integration/empty/sparse/inactive/destroyed/missing")
    else:
        push_error("Shell presentation contract failed")
    quit(0 if passed else 1)
