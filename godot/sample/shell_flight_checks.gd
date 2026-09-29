extends SceneTree

const Main = preload("res://main.gd")
const Art = preload("res://art.gd")

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
    if passed:
        print("TANKS_SHELL_FLIGHT_CHECKS_PASSED 12-models/four-directions/owner-lifecycle/native-impact")
    else:
        push_error("Shell presentation contract failed")
    quit(0 if passed else 1)
