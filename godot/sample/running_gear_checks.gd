extends SceneTree
## Headless displacement/geometry/lifecycle contracts. Rendered motion remains
## a separate GPU acceptance check; these checks do not estimate frame rate.
const Art = preload("res://art.gd")
const Visibility = preload("res://player_visibility.gd")
const PARTS := ["Body","Hull","TrackLeft","TrackRight"]
const CONTRACTS := ["displacement","blocked","cardinals","reverse","turn","lateral",
    "pause","freeze","creating","teleport","respawn","isolation","shared-mesh",
    "bounds","budgets","ghost-shadows","boat","reset","catchup","rng","historical-chassis"]
var failures: Array[String] = []
var models := 0
var cases := 0
var geometry_frames := 0
var maximum_triangles := 0
var warmed_frames: Dictionary = {}
var checked_periods: Dictionary = {}
var world: Node3D
var camera: Camera3D


func _initialize() -> void:
    call_deferred("run")


func expect(value: bool, detail: String) -> void:
    if not value:
        failures.append(detail)
        if failures.size() <= 20: push_error("Running gear contract: "+detail)


func travel(tank: Node3D) -> Vector2:
    var gear: Dictionary = tank.get_meta("gear_state")
    return Vector2(gear.left_travel,gear.right_travel)


func meshes(tank: Node3D) -> Array:
    return [tank.get_node("TrackLeft").mesh,tank.get_node("TrackRight").mesh]


func bank_signature(tank: Node3D) -> Array:
    var result: Array = []
    for key in ["gear_frames_left","gear_frames_right"]:
        for mesh in tank.get_meta(key):
            result.append([mesh.get_instance_id(),mesh.get_aabb()])
            for surface in mesh.get_surface_count():
                var arrays: Array = mesh.surface_get_arrays(surface)
                result.append([hash(arrays[Mesh.ARRAY_VERTEX]),hash(arrays[Mesh.ARRAY_NORMAL]),
                    hash(arrays[Mesh.ARRAY_COLOR]),mesh.surface_get_material(surface)])
    return result


func cached_frames() -> Dictionary:
    var result := {}
    for key in Art._gear_cache:
        var ids: Array = []
        for mesh in Art._gear_cache[key]: ids.append(mesh.get_instance_id())
        result[key] = ids
    return result


func forward(yaw: float) -> Vector3:
    return Basis(Vector3.UP,-yaw)*Vector3.FORWARD


func step(tank: Node3D, position: Vector3, yaw: float, moving: bool = true,
        dt: float = 1.0/60.0, extra: Dictionary = {}) -> void:
    tank.position = position
    tank.rotation.y = -yaw
    var root_pose := tank.transform
    var data := {"x":position.x,"z":position.z,"yaw":yaw,"moving":moving,"armor":4}
    data.merge(extra,true)
    var original := data.duplicate(true)
    Art.set_vehicle_state(tank,data,dt)
    expect(tank.transform == root_pose,"presentation changed simulation root transform")
    expect(data == original,"presentation changed supplied snapshot")


func triangles(mesh: ArrayMesh) -> int:
    var total := 0
    for surface in mesh.get_surface_count():
        var arrays := mesh.surface_get_arrays(surface)
        var indices: PackedInt32Array = arrays[Mesh.ARRAY_INDEX]
        var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
        total += (indices.size() if not indices.is_empty() else vertices.size())/3
    return total


func check_current_geometry(tank: Node3D, label: String) -> void:
    var gear: Dictionary = tank.get_meta("gear_state")
    var banks: Array = [tank.get_meta("gear_frames_left"),tank.get_meta("gear_frames_right")]
    var frames: Array = [gear.left_frame,gear.right_frame]
    var bounds: AABB = tank.get_meta("visual_bounds")
    expect(bounds.position.is_finite() and bounds.size.is_finite(),label+": nonfinite visual bounds")
    for index in 2:
        var part: MeshInstance3D = tank.get_node("TrackLeft" if index == 0 else "TrackRight")
        expect(int(frames[index]) >= 0 and int(frames[index]) < banks[index].size(),label+": frame outside shared bank")
        expect(part.mesh == banks[index][int(frames[index])],label+": visible running gear is not the selected shared frame")
        expect(part.cast_shadow == GeometryInstance3D.SHADOW_CASTING_SETTING_ON,label+": current geometry no longer casts matching shadows")
    for name in PARTS:
        var part: MeshInstance3D = tank.get_node(name)
        var actual := part.transform*part.mesh.get_aabb()
        expect(bounds.grow(.001).encloses(actual),label+": crop/camera bounds omit animated "+name)
        if name.begins_with("Track"):
            expect(actual.position.y >= -.002,label+": running gear penetrates ground")
    if tank.get_node("Boat").visible:
        var boat: MeshInstance3D = tank.get_node("Boat/Body")
        expect(bounds.grow(.001).encloses(boat.mesh.get_aabb()),label+": boat missing from visual bounds")


func point_key(point: Vector3) -> String:
    # Hard edges legitimately duplicate vertices. Compare rendered triangles,
    # allowing subpixel floating-point noise when a period wraps around TAU.
    return "%d,%d,%d"%[roundi(point.x*10000),roundi(point.y*10000),roundi(point.z*10000)]


func triangle_signature(mesh: ArrayMesh, label: String) -> Array:
    var result: Array = []
    for surface in mesh.get_surface_count():
        var arrays: Array = mesh.surface_get_arrays(surface)
        var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
        var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
        var colors: PackedColorArray = arrays[Mesh.ARRAY_COLOR]
        var indices: PackedInt32Array = arrays[Mesh.ARRAY_INDEX]
        var signatures: Array[String] = []
        var occupied := {}
        for offset in range(0,indices.size(),3):
            var points: Array[String] = []
            var attributes: Array[String] = []
            var a := vertices[indices[offset]]
            var b := vertices[indices[offset+1]]
            var c := vertices[indices[offset+2]]
            expect(a.is_finite() and b.is_finite() and c.is_finite(),label+": nonfinite period geometry")
            expect((b-a).cross(c-a).length_squared() > 1e-14,label+": collapsed running gear triangle")
            for corner in 3:
                var index := indices[offset+corner]
                var key := point_key(vertices[index])
                points.append(key)
                var color := colors[index]
                attributes.append(key+"/"+point_key(normals[index])+"/%d,%d,%d,%d"%
                    [roundi(color.r*4096),roundi(color.g*4096),roundi(color.b*4096),roundi(color.a*4096)])
            points.sort()
            var footprint := ";".join(points)
            expect(not occupied.has(footprint),label+": duplicate coplanar running gear triangle")
            occupied[footprint] = true
            # Canonical cyclic order preserves winding while allowing the
            # next shoe/spoke to replace the previous one after a full period.
            var rotations := [";".join(attributes),
                ";".join([attributes[1],attributes[2],attributes[0]]),
                ";".join([attributes[2],attributes[0],attributes[1]])]
            rotations.sort()
            signatures.append(rotations[0])
        signatures.sort()
        result.append(signatures)
    return result


func check_period(bank: Array, profile: Dictionary, side: float, label: String) -> void:
    var id: int = bank[0].get_instance_id()
    if checked_periods.has(id): return
    checked_periods[id] = true
    var geometry = Art.Geometry.new()
    Art._running_gear(geometry,profile,side,1.0)
    var endpoint: ArrayMesh = geometry.finish({"metal":Art._material("metal"),"rubber":Art._material("rubber")})
    expect(triangle_signature(bank[0],label) == triangle_signature(endpoint,label),
        label+": full visual period does not close with matching surfaces, normals and color")
    # Positive vehicle travel is local -Z; moving contact geometry must travel
    # along +Z relative to the hull. Check every actual mesh step, including
    # the final step to the generated periodic endpoint, not just the odometer.
    for frame in bank.size():
        var current: ArrayMesh = bank[frame]
        var next_mesh: ArrayMesh = endpoint if frame+1 == bank.size() else bank[frame+1]
        triangle_signature(current,label)
        var contacts := 0
        for surface in current.get_surface_count():
            var first_arrays: Array = current.surface_get_arrays(surface)
            var next_arrays: Array = next_mesh.surface_get_arrays(surface)
            var first: PackedVector3Array = first_arrays[Mesh.ARRAY_VERTEX]
            var next: PackedVector3Array = next_arrays[Mesh.ARRAY_VERTEX]
            var first_indices: PackedInt32Array = first_arrays[Mesh.ARRAY_INDEX]
            var next_indices: PackedInt32Array = next_arrays[Mesh.ARRAY_INDEX]
            # SurfaceTool deduplicates each frame independently; rendered
            # triangle-corner order supplies correspondence, vertex IDs do not.
            expect(first_indices.size() == next_indices.size(),label+": neighboring frame topology changed")
            if first_indices.size() != next_indices.size(): continue
            for index in first_indices.size():
                var start := first[first_indices[index]]
                var finish := next[next_indices[index]]
                var displacement := finish-start
                expect(displacement.length() < current.get_aabb().size.length()/bank.size(),
                    label+": neighboring mesh frame jumps across the running gear")
                if start.y < .05 and finish.y < .05 and displacement.length() > .00001:
                    expect(displacement.z > 0.0,label+": contact geometry rolls forward relative to moving hull")
                    contacts += 1
        expect(contacts > 0,label+": no moving rendered contact geometry")


func check_banks(tank: Node3D, peer: Node3D, profile: Dictionary, label: String) -> void:
    var left: Array = tank.get_meta("gear_frames_left")
    var right: Array = tank.get_meta("gear_frames_right")
    var peer_left: Array = peer.get_meta("gear_frames_left")
    var peer_right: Array = peer.get_meta("gear_frames_right")
    expect(left.size() > 1 and left.size() <= 32 and right.size() == left.size(),label+": unbounded or absent animation bank")
    var rigid := triangles(tank.get_node("Body").mesh)+triangles(tank.get_node("Hull").mesh)
    var differs := false
    for frame in left.size():
        geometry_frames += 2
        expect(left[frame] == peer_left[frame] and right[frame] == peer_right[frame],label+": identical models do not share frame meshes")
        var total := rigid+triangles(left[frame])+triangles(right[frame])
        maximum_triangles = maxi(maximum_triangles,total)
        expect(total <= 4000,label+": animated frame exceeds original vehicle geometry budget")
        for bank in [left,right]:
            var mesh: ArrayMesh = bank[frame]
            expect(mesh.get_surface_count() <= 2,label+": running gear surface budget increased")
            expect(mesh.get_aabb().position.is_finite() and mesh.get_aabb().size.is_finite(),label+": frame has nonfinite envelope")
            if frame > 0:
                for surface in mesh.get_surface_count():
                    var current: Array = mesh.surface_get_arrays(surface)
                    var first: Array = bank[0].surface_get_arrays(surface)
                    differs = differs or current[Mesh.ARRAY_VERTEX] != first[Mesh.ARRAY_VERTEX]
    expect(differs,label+": phase bank never changes actual rendered geometry")
    check_period(left,profile,-1.0,label+"/left")
    check_period(right,profile,1.0,label+"/right")
    cases += 1


func check_ghost(tank: Node3D, label: String) -> void:
    var hint := Visibility.new(world)
    # A conservative wall envelope isolates geometry-copy behavior. Actual
    # terrain masks, forest protection and depth rendering have separate tests.
    hint._buildings.assign([AABB(Vector3(8,0,10),Vector3(6,4,1))])
    camera.position = Vector3(12,8,17)
    camera.look_at(Vector3(12,.4,9.5))
    var state := {"players":[{"id":0,"active":true}]}
    step(tank,Vector3(10.5,0,9.4),0.0,true,0.0,{"boat":true})
    var stride: float = tank.get_meta("gear_cycle")/8.0
    for frame in 20:
        step(tank,tank.position+forward(0.0)*stride,0.0,true,1.0/60.0,{"boat":true})
        hint.update(camera,{"p0":tank},state,0.0)
        expect(hint.stats().active == 1,label+": test wall did not acquire player ghost")
        var record: Dictionary = hint._players[0]
        for index in record.copies.size():
            var ghost: MeshInstance3D = record.copies[index]
            var source: MeshInstance3D = record.originals[index]
            expect(ghost.mesh == source.mesh and ghost.global_transform == source.global_transform,
                label+": ghost lags selected geometry or suspension pose")
            expect(ghost.cast_shadow == GeometryInstance3D.SHADOW_CASTING_SETTING_OFF,label+": ghost adds duplicate shadows")
        check_current_geometry(tank,label)
    hint.dispose()
    cases += 1


func check_model(nation: int, enemy: bool, tier: int) -> void:
    var label := "%d/%s/%d"%[nation,enemy,tier]
    var tank: Node3D = Art.make_tank(nation,enemy,tier,0,tier)
    var peer: Node3D = Art.make_tank(nation,enemy,tier,0,tier)
    world.add_child(tank)
    world.add_child(peer)
    expect(cached_frames() == warmed_frames,label+": unseen chassis built animation frames after startup warmup")
    var muzzle: Vector3 = tank.get_meta("neutral_muzzle")
    var peer_state: Dictionary = peer.get_meta("gear_state").duplicate(true)
    var peer_meshes := meshes(peer)
    var peer_pose := peer.transform
    var initial_mesh_count: int = Art._meshes.size()
    var initial_gear_cache: int = Art._gear_cache.size()
    var initial_bank_signature := bank_signature(tank)
    check_banks(tank,peer,Art._vehicle_profile(nation,enemy,tier,tier),label)
    var position := Vector3(13,0,13)
    step(tank,position,0.0,true)
    expect(travel(tank) == Vector2.ZERO,label+": first snapshot invented travel")
    cases += 1
    var distance: float = tank.get_meta("gear_cycle")*.25
    for yaw in [0.0,PI*.5,PI,-PI*.5]:
        step(tank,position,yaw,true,0.0)
        var before := travel(tank)
        var old_meshes := meshes(tank)
        position += forward(yaw)*distance
        step(tank,position,yaw)
        expect((travel(tank)-before).is_equal_approx(Vector2.ONE*distance),label+": cardinal signed displacement wrong")
        expect(meshes(tank) != old_meshes,label+": cardinal motion did not change visible frame")
        check_current_geometry(tank,label)
        cases += 1
    var yaw := -PI*.5
    var stopped := travel(tank)
    var stopped_meshes := meshes(tank)
    for _frame in 30: step(tank,position,yaw,true)
    expect(travel(tank) == stopped and meshes(tank) == stopped_meshes,label+": held movement against wall crawls")
    cases += 1
    position += (Basis(Vector3.UP,-yaw)*Vector3.RIGHT)*.035
    step(tank,position,yaw,true)
    expect(travel(tank).is_equal_approx(stopped),label+": lateral lane correction becomes forward tread travel")
    cases += 1
    position -= forward(yaw)*distance
    step(tank,position,yaw,true)
    expect((travel(tank)-stopped).is_equal_approx(-Vector2.ONE*distance),label+": reverse slide does not reverse running gear")
    position += forward(yaw)*distance
    step(tank,position,yaw,true)
    expect(travel(tank).is_equal_approx(stopped) and meshes(tank) == stopped_meshes,label+": reversing path does not restore the frame")
    cases += 1
    var before_turn := travel(tank)
    yaw += .2
    step(tank,position,yaw,false)
    var turn := travel(tank)-before_turn
    expect(turn.x > 0.0 and turn.y < 0.0 and absf(turn.x+turn.y) < .0001,label+": right turn has wrong differential sense")
    step(tank,position,PI-.01,false,0.0)
    var before_wrap := travel(tank)
    yaw = -PI+.01
    step(tank,position,yaw,false)
    var wrapped := travel(tank)-before_wrap
    expect(wrapped.x > 0 and wrapped.y < 0 and wrapped.length() < .05,label+": yaw wrap introduces a full spin")
    cases += 1
    stopped = travel(tank)
    stopped_meshes = meshes(tank)
    for _frame in 60: step(tank,position,yaw,false)
    expect(travel(tank) == stopped and meshes(tank) == stopped_meshes,label+": stopped vehicle's tread keeps advancing")
    cases += 1
    for kind in ["pause","freeze","creating"]:
        var extra := {"frozen":1.0} if kind == "freeze" else {"creating":.5} if kind == "creating" else {}
        var dt := 0.0 if kind == "pause" else 1.0/60.0
        # Capture/paused snapshots may reposition a reused node across a stage.
        # Frozen snapshots must also rebase without changing the visible frame.
        position += forward(yaw)*(.35 if kind == "creating" else 10.0)
        yaw += .12
        step(tank,position,yaw,true,dt,extra)
        if kind == "creating":
            stopped = Vector2.ZERO
            stopped_meshes = [tank.get_meta("gear_frames_left")[0],tank.get_meta("gear_frames_right")[0]]
        expect(travel(tank) == stopped and meshes(tank) == stopped_meshes,label+": "+kind+" violated frozen/new-life phase")
        step(tank,position,yaw,false)
        var resumed := travel(tank)
        expect(resumed == stopped,label+": "+kind+" resume accumulated suppressed displacement")
        position += forward(yaw)*distance
        step(tank,position,yaw,true)
        expect((travel(tank)-resumed).is_equal_approx(Vector2.ONE*distance),label+": "+kind+" resume lost ordinary travel")
        stopped = travel(tank)
        stopped_meshes = meshes(tank)
        cases += 1
    position += Vector3(10,0,-10)
    step(tank,position,yaw)
    expect(travel(tank) == Vector2.ZERO and meshes(tank) == [tank.get_meta("gear_frames_left")[0],tank.get_meta("gear_frames_right")[0]],
        label+": teleport did not establish a neutral new-location anchor")
    position += forward(yaw)*distance
    step(tank,position,yaw)
    expect(travel(tank).is_equal_approx(Vector2.ONE*distance),label+": teleport left a stale displacement anchor")
    cases += 1
    var before_catchup := travel(tank)
    position += forward(yaw)
    step(tank,position,yaw,true,.001)
    expect((travel(tank)-before_catchup).is_equal_approx(Vector2.ONE),label+": legitimate catch-up distance discarded because of render dt")
    cases += 1
    var before_boat := travel(tank)
    step(tank,position,yaw,true,0.0,{"boat":true})
    expect(travel(tank) == before_boat and tank.get_node("Boat").visible,label+": boat toggle resets or advances phase")
    check_current_geometry(tank,label)
    position += forward(yaw)*distance
    step(tank,position,yaw,true,1.0/60.0,{"boat":true})
    expect((travel(tank)-before_boat).is_equal_approx(Vector2.ONE*distance),label+": boat travel breaks gear progression")
    check_current_geometry(tank,label)
    cases += 1
    for frame in 80:
        position += forward(yaw)*float(tank.get_meta("gear_cycle"))/16.0
        step(tank,position,yaw)
        check_current_geometry(tank,label)
    expect(Art._meshes.size() == initial_mesh_count and Art._gear_cache.size() == initial_gear_cache,
        label+": animation allocated new uncached mesh resources")
    expect(bank_signature(tank) == initial_bank_signature,label+": phase update mutated shared frame geometry or materials")
    expect(peer.get_meta("gear_state") == peer_state and meshes(peer) == peer_meshes and peer.transform == peer_pose,
        label+": one vehicle's movement changed its shared-mesh peer")
    expect(tank.get_meta("neutral_muzzle") == muzzle,label+": animation changed native muzzle attachment")
    cases += 1
    Art.reset_running_gear(tank)
    expect(travel(tank) == Vector2.ZERO and not tank.get_meta("gear_state").initialized,label+": explicit session reset retained odometer")
    step(tank,position+Vector3(2,0,2),0.0,true)
    expect(travel(tank) == Vector2.ZERO,label+": first snapshot after explicit reset counted old position")
    cases += 1
    var replacement: Node3D = Art.make_tank(nation,enemy,tier,0,tier)
    world.add_child(replacement)
    step(replacement,Vector3(25,0,25),yaw)
    expect(travel(replacement) == Vector2.ZERO,label+": respawn inherited another instance's travel")
    replacement.free()
    cases += 1
    if not enemy: check_ghost(tank,label)
    tank.free()
    peer.free()
    models += 1


func check_historical_chassis() -> void:
    # Count rendered hub-cap centers (six adjacent triangles), rather than
    # trusting wheel-count metadata. Includes raised sprocket and idler hubs.
    var wheel_counts := [[6,6,6,7],[5,6,5,6],[8,9,7,7]]
    var roller_counts := [[3,5,3,2],[0,3,0,3],[0,0,4,4]]
    var banks := {}
    for nation in 3:
        for tier in 4:
            var label := "historical/%d/%d"%[nation,tier]
            var profile: Dictionary = Art._vehicle_profile(nation,false,0,tier)
            var gear: Dictionary = Art._chassis_running_gear(profile)
            expect(gear.positions.size() == wheel_counts[nation][tier],label+": incorrect roadwheel stations")
            expect(gear.rollers.size() == roller_counts[nation][tier],label+": incorrect upper return rollers")
            expect(gear.front_drive == (nation == 0 and tier == 0 or nation == 2 and tier < 2),label+": drive sprocket on wrong end")
            var mesh: ArrayMesh = Art._gear_frames(profile,1.0)[0]
            expect(not banks.has(mesh.get_instance_id()),label+": distinct historical chassis shares mesh bank")
            banks[mesh.get_instance_id()] = true
            var cap_color: Color = Art.NATIONAL_PAINT[nation].lerp(Color("a5ada0"),.23).lightened(.08)
            var visits := {}
            var locations := {}
            for surface in mesh.get_surface_count():
                var arrays: Array = mesh.surface_get_arrays(surface)
                var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
                var colors: PackedColorArray = arrays[Mesh.ARRAY_COLOR]
                var indices: PackedInt32Array = arrays[Mesh.ARRAY_INDEX]
                for index in indices:
                    # ArrayMesh stores vertex colors in 8-bit channels.
                    if absf(colors[index].r-cap_color.r) <= 1.0/255.0 and absf(colors[index].g-cap_color.g) <= 1.0/255.0 and absf(colors[index].b-cap_color.b) <= 1.0/255.0:
                        var key := point_key(vertices[index])
                        visits[key] = int(visits.get(key,0))+1
                        locations[key] = vertices[index]
            var hubs: Array[Vector3] = []
            for key in visits:
                if visits[key] == 6: hubs.append(locations[key])
            expect(hubs.size() == wheel_counts[nation][tier]+2,label+": actual mesh hub count does not match roadwheels plus two end wheels")
            var road_z: Array[float] = []
            for hub in hubs:
                if absf(hub.y-(float(gear.radius)+.041*float(profile.chassis))) < .0001:
                    road_z.append(hub.z)
            road_z.sort()
            expect(road_z.size() == wheel_counts[nation][tier],label+": roadwheels not seated at consistent ground height")
            if nation == 1 and tier == 2 and road_z.size() == 5:
                expect(road_z[3]-road_z[2] > (road_z[1]-road_z[0])*1.10 and
                    road_z[4]-road_z[3] > (road_z[1]-road_z[0])*1.10,label+": T62 lost its two larger rear gaps")
            if nation == 0 and tier == 0 and road_z.size() == 6:
                expect(road_z[2]-road_z[1] > road_z[1]-road_z[0] and
                    road_z[4]-road_z[3] > road_z[3]-road_z[2],label+": Sherman no longer reads as three paired bogies")


func run() -> void:
    seed(20260918)
    var next_random := randi()
    seed(20260918)
    Art.warm_running_gear()
    warmed_frames = cached_frames()
    expect(warmed_frames.size() == 24,"startup did not prepare twelve model chassis banks on both sides")
    Art.warm_running_gear()
    expect(cached_frames() == warmed_frames,"repeated warmup replaced/expanded cached animation frames")
    check_historical_chassis()
    world = Node3D.new()
    root.add_child(world)
    camera = Camera3D.new()
    camera.projection = Camera3D.PROJECTION_ORTHOGONAL
    world.add_child(camera)
    for nation in 3:
        for enemy in [false,true]:
            for tier in 4: check_model(nation,enemy,tier)
    expect(randi() == next_random,"animation consumed global gameplay random stream")
    cases += 1
    expect(models == 24 and cases == 493,"incomplete model/displacement/lifecycle checks")
    world.free()
    if not failures.is_empty():
        print("TANKS_RUNNING_GEAR_FAILED "+JSON.stringify(failures.slice(0,20)))
        quit(1)
        return
    print("TANKS_RUNNING_GEAR_PASSED "+JSON.stringify({"status":"passed","models":models,"cases":cases,
        "geometry_frames":geometry_frames,"maximum_triangles":maximum_triangles,"checks":CONTRACTS}))
    quit()
