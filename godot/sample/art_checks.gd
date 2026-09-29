extends SceneTree
## Headless asset contracts, not rendered art acceptance. Budget ceilings are
## agreed with the artist; exact triangle counts and silhouettes are not golden.

var Art: GDScript
const EPSILON := 0.001
const PLAYER_NAMES := [
    ["M4A3(75) SHERMAN", "M26 PERSHING", "M60A3", "M1A1 ABRAMS"],
    ["T-34/76", "IS-2", "T-62", "T-90A"],
    ["PANTHER AUSF. A", "TIGER II", "LEOPARD 1", "LEOPARD 2A4"],
]
const ENEMY_NAMES := [
    ["M26 PERSHING", "M4A3(75) SHERMAN", "M60A3", "M1A1 ABRAMS"],
    ["IS-2", "T-34/76", "T-62", "T-90A"],
    ["TIGER II", "PANTHER AUSF. A", "LEOPARD 1", "LEOPARD 2A4"],
]
# New model-space gun tips are presentation only. Physical shell origins are
# characterized in the native session tests and are deliberately unchanged.
const PLAYER_MUZZLES := [
    [Vector2(.5974,-.87),Vector2(.4872,-1.36),Vector2(.5714,-1.44),Vector2(.455,-1.38)],
    [Vector2(.4952,-.82),Vector2(.5218,-1.49),Vector2(.4456,-1.40),Vector2(.4504,-1.52)],
    [Vector2(.5622,-1.19),Vector2(.6064,-1.43),Vector2(.4774,-1.48),Vector2(.5194,-1.43)],
]
var failures: Array[String] = []
var completed_groups: Array[String] = []
var checked_meshes: Dictionary = {}
var checked_camouflage: Dictionary = {}
var national_shape_results: Array = []
var maximums := {"tank": 0, "brick": 0, "pickup": 0, "forest": 0, "effect": 0}
var counts := {"vehicles": 0, "national_color_cases": 0, "brick_cases": 0, "pickups": 0, "base_walls": 0, "bases": 0, "forest_cases": 0, "motion_cases": 0}


func expect(value: bool, detail: String) -> bool:
    if not value:
        failures.append(detail)
        if failures.size() <= 20: push_error("Art contract: " + detail)
    return value


func _initialize() -> void:
    Art = load("res://art.gd") as GDScript
    if Art == null or not Art.can_instantiate():
        push_error("Art contract: art.gd did not compile")
        quit(1)
        return
    # Includes first construction and cache hits. Visual variety must not
    # consume the shared random stream when objects appear or change state.
    seed(20260917)
    var expected_random := randi()
    seed(20260917)
    check_vehicles()
    check_world()
    check_effects()
    expect(randi() == expected_random, "building/updating art consumed the global random stream")
    expect(completed_groups == ["vehicles","world","effects"],"an asset/runtime failure aborted a check group")
    expect(counts == {"vehicles":24,"national_color_cases":192,"brick_cases":512,"pickups":9,"base_walls":10,"bases":3,"forest_cases":676,"motion_cases":24},
        "incomplete checks: an asset dependency/runtime failure skipped cases")
    expect(checked_meshes.size() > 0 and maximums.effect > 0,"no actual meshes/effects checked")
    if not failures.is_empty():
        print("TANKS_ART_CHECKS_FAILED " + JSON.stringify(failures.slice(0, 20)))
        quit(1)
        return
    print("TANKS_ART_CHECKS_PASSED " + JSON.stringify({
        "status": "passed", "counts": counts, "maximum_triangles": maximums,
        "meshes_checked": checked_meshes.size(),
        "national_shapes": national_shape_results,
        "checks": ["geometry", "winding", "budgets", "cache", "material-isolation",
            "identity", "national-color", "national-shape", "armor-planes", "cast-surfaces", "enemy-status-patches", "muzzle", "footprints", "tier-proportions", "motion", "effects", "rng"],
    }))
    quit()


func mesh_nodes(node: Node) -> Array[MeshInstance3D]:
    var result: Array[MeshInstance3D] = []
    if node is MeshInstance3D: result.append(node)
    for child in node.get_children(): result.append_array(mesh_nodes(child))
    return result


func body(node: Node3D) -> MeshInstance3D:
    return node.get_node("Body") as MeshInstance3D


func mesh_triangles(mesh: ArrayMesh) -> int:
    var total := 0
    for surface in mesh.get_surface_count():
        var arrays := mesh.surface_get_arrays(surface)
        var indices: PackedInt32Array = arrays[Mesh.ARRAY_INDEX]
        var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
        total += (indices.size() if not indices.is_empty() else vertices.size()) / 3
    return total


func check_mesh(mesh: ArrayMesh, label: String, all_triangle_normals: bool = false) -> void:
    if not expect(mesh != null, label + ": missing mesh"): return
    if checked_meshes.has(mesh.get_instance_id()): return
    # Hold the Resource alive so IDs cannot be reused during this check.
    checked_meshes[mesh.get_instance_id()] = mesh
    expect(mesh.get_surface_count() > 0, label + ": invisible empty mesh")
    var bounds := mesh.get_aabb()
    expect(bounds.position.is_finite() and bounds.size.is_finite(), label + ": nonfinite bounds")
    for surface in mesh.get_surface_count():
        var part := "%s/surface%d" % [label, surface]
        expect(mesh.surface_get_primitive_type(surface) == Mesh.PRIMITIVE_TRIANGLES, part + ": unsupported primitive")
        expect(mesh.surface_get_material(surface) != null, part + ": missing material")
        var arrays := mesh.surface_get_arrays(surface)
        var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
        var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
        var indices: PackedInt32Array = arrays[Mesh.ARRAY_INDEX]
        if not expect(vertices.size() > 0 and vertices.size() == normals.size(), part + ": incomplete vertices/normals"):
            continue
        for index in vertices.size():
            expect(vertices[index].is_finite(), part + ": nonfinite vertex")
            expect(normals[index].is_finite() and absf(normals[index].length() - 1.0) < .002,
                part + ": missing or non-unit normal")
        if indices.is_empty():
            indices.resize(vertices.size())
            for index in indices.size(): indices[index] = index
        if not expect(indices.size() % 3 == 0, part + ": truncated triangle"): continue
        for start in range(0, indices.size(), 3):
            var valid := true
            for offset in 3:
                valid = expect(indices[start + offset] >= 0 and indices[start + offset] < vertices.size(),
                    part + ": index outside vertex buffer") and valid
            if not valid: continue
            var a := indices[start]
            var b := indices[start + 1]
            var c := indices[start + 2]
            var face := (vertices[b] - vertices[a]).cross(vertices[c] - vertices[a])
            if not expect(face.length_squared() > 1e-16, part + ": zero-area triangle %d" % (start / 3)):
                continue
            # Godot fronts wind clockwise. Preserve the existing angle limit;
            # tank cast surfaces may have a distinct normal at each corner, so
            # checking only the first corner can miss an inverted smooth edge.
            var face_normal := face.normalized()
            for corner in range(3 if all_triangle_normals else 1):
                var vertex := indices[start + corner]
                expect(face_normal.dot(normals[vertex]) < -.75,
                    part + ": reversed face/normal at triangle %d corner %d" % [start / 3, corner])


func check_tree(node: Node, label: String, all_triangle_normals: bool = false) -> void:
    for instance in mesh_nodes(node):
        expect(instance.transform.is_finite(), label + ": nonfinite transform")
        check_mesh(instance.mesh as ArrayMesh, label + "/" + instance.name, all_triangle_normals)


func immutable_mesh_signature(mesh: ArrayMesh) -> Array:
    var result: Array = [mesh.get_aabb()]
    for surface in mesh.get_surface_count():
        var arrays := mesh.surface_get_arrays(surface)
        result.append([hash(arrays[Mesh.ARRAY_VERTEX]), hash(arrays[Mesh.ARRAY_NORMAL]),
            hash(arrays[Mesh.ARRAY_COLOR]), hash(arrays[Mesh.ARRAY_INDEX]), mesh.surface_get_material(surface)])
    return result


func colors(instance: MeshInstance3D) -> Array[Color]:
    var result: Array[Color] = []
    for surface in instance.mesh.get_surface_count():
        var material := instance.get_active_material(surface) as StandardMaterial3D
        result.append(material.albedo_color if material != null else Color.TRANSPARENT)
    return result


func check_muzzle(tank: Node3D, label: String) -> void:
    var muzzle: Vector3 = tank.get_meta("neutral_muzzle", Vector3.INF)
    var mesh := body(tank).mesh as ArrayMesh
    expect(muzzle.is_finite() and absf(muzzle.x) < EPSILON and muzzle.y > .2 and muzzle.z < -.2,
        label + ": primary muzzle does not follow local -Z convention")
    expect(mesh.get_aabb().grow(EPSILON).has_point(muzzle), label + ": muzzle outside body envelope")
    var lip_points := 0
    var lip_radius := 0.0
    var lip_radii: Array[float] = []
    for surface in mesh.get_surface_count():
        var vertices: PackedVector3Array = mesh.surface_get_arrays(surface)[Mesh.ARRAY_VERTEX]
        for point in vertices:
            var delta := point - muzzle
            if absf(delta.z) < EPSILON and delta.length() > .015 and delta.length() < .18:
                lip_points += 1
                var radius := Vector2(delta.x,delta.y).length()
                lip_radius = maxf(lip_radius,radius)
                var known := false
                for prior in lip_radii:
                    if absf(prior-radius) < .00001: known = true
                if not known: lip_radii.append(radius)
    expect(lip_points >= 6, label + ": no rendered cannon lip supports the muzzle attachment")
    # The stockier September 29 direction increases the entire barrel by 30%,
    # including the existing brake. Keep an absolute limit independent of the
    # implementation coefficient, plus the circular-lip and fixed-mount checks.
    expect(lip_radius >= .050 and lip_radius <= .095,
        label + ": cannon outside the approved stocky barrel envelope")
    expect(lip_radii.size() == 2,label + ": muzzle inner/outer lips are no longer circular")


func check_recognition_landmarks(tank: Node3D, label: String) -> void:
    var name: String = tank.get_meta("vehicle_name", "")
    var lenses := {"M60A3":Color("314c49"), "LEOPARD 1":Color("d1cfad"), "T-90A":Color("554135")}
    if not lenses.has(name): return
    var points := PackedVector3Array()
    var mesh := body(tank).mesh as ArrayMesh
    for surface in mesh.get_surface_count():
        var arrays := mesh.surface_get_arrays(surface)
        var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
        var tints: PackedColorArray = arrays[Mesh.ARRAY_COLOR]
        for i in vertices.size():
            var tint := tints[i]
            var target: Color = lenses[name]
            # Mesh vertex colors are quantized; inspect actual optical faces.
            if maxf(absf(tint.r-target.r),maxf(absf(tint.g-target.g),absf(tint.b-target.b))) <= 1.0/255.0+.000001:
                points.append(vertices[i])
    if not expect(points.size() >= 12,label+": missing model-specific optical faces"): return
    var bounds := points_bounds(points)
    if name == "M60A3":
        expect(bounds.position.x > .2,label+": A3 rangefinder port must be on the right")
    elif name == "LEOPARD 1":
        expect(absf(bounds.get_center().x) < .01 and bounds.size.x > .1,
            label+": early Leopard searchlight must be centered above the mantlet")
    else:
        expect(bounds.position.x < -.2 and bounds.end.x > .2 and bounds.end.z < -.15,
            label+": T-90A needs paired front optical housings beside the gun")


func check_vehicles() -> void:
    for nation in 3:
        for enemy in [false, true]:
            for tier in 4:
                var label := "tank/%d/%s/%d" % [nation, enemy, tier]
                var tank: Node3D = Art.make_tank(nation, enemy, tier, 0, tier)
                var peer: Node3D = Art.make_tank(nation, enemy, tier, 0, tier)
                var mesh := body(tank).mesh as ArrayMesh
                expect(tank.basis.is_equal_approx(Basis.IDENTITY),
                    label + ": whole-vehicle scaling distorts round fittings and suspension")
                check_tree(tank, label, true)
                # Running gear swaps complete shared meshes. Validate every
                # authored frame, not only the neutral one in the tree.
                for bank_name in ["gear_frames_left","gear_frames_right"]:
                    var bank: Array = tank.get_meta(bank_name,[])
                    expect(bank.size() > 1 and bank.size() <= 32,label+": absent/unbounded running gear bank")
                    for frame in bank.size():
                        check_mesh(bank[frame],"%s/%s/%d"%[label,bank_name,frame],true)
                expect(body(peer).mesh == mesh, label + ": identical geometry was not cached")
                var triangles := 0
                var surfaces := 0
                var bounds := mesh.get_aabb()
                for part_name in ["Body","Hull","TrackLeft","TrackRight"]:
                    var part := tank.get_node(part_name) as MeshInstance3D
                    var peer_part := peer.get_node(part_name) as MeshInstance3D
                    expect(part.mesh == peer_part.mesh,label + ": rigid part cache not shared")
                    triangles += mesh_triangles(part.mesh)
                    surfaces += part.mesh.get_surface_count()
                    bounds = bounds.merge(part.mesh.get_aabb())
                maximums.tank = maxi(maximums.tank, triangles)
                expect(triangles <= 4000 and surfaces <= 13,
                    label + ": vehicle budget exceeded (%d triangles, %d surfaces)" % [triangles,surfaces])
                expect(bounds.position.y >= -.02 and bounds.end.y < 1.5 and bounds.size.x < 1.6 and bounds.size.z < 2.55,
                    label + ": ground/footprint envelope incompatible with normal game scale")
                expect(tank.get_meta("vehicle_name", "") == (ENEMY_NAMES if enemy else PLAYER_NAMES)[nation][tier],
                    label + ": wrong nation/tier/role identity")
                expect(tank.get_meta("visual_bounds", AABB()).is_equal_approx(bounds), label + ": stale neutral crop bounds")
                var attachment: Vector2 = PLAYER_MUZZLES[nation][[1,0,2,3][tier] if enemy else tier]
                var muzzle: Vector3 = tank.get_meta("neutral_muzzle")
                expect(muzzle.is_equal_approx(Vector3(0,attachment.x,attachment.y)),
                    label + ": rendered gun tip disagrees with its presentation attachment")
                check_muzzle(tank, label)
                check_recognition_landmarks(tank, label)
                check_national_colors(tank, peer, nation, enemy, label)
                var peer_colors := colors(body(peer))
                Art.set_vehicle_state(tank, {"moving": true, "boat": true, "frozen": 1.0, "armor": 4}, .11)
                expect(tank.get_node("Boat").visible and not peer.get_node("Boat").visible, label + ": boat state leaked")
                expect(colors(body(peer)) == peer_colors and body(peer).transform == Transform3D.IDENTITY,
                    label + ": state update changed an untouched tank")
                var reported: AABB = tank.get_meta("visual_bounds")
                expect(reported.grow(EPSILON).encloses(body(tank).transform * mesh.get_aabb()), label + ": moving body clipped by metadata")
                var boat := tank.get_node("Boat/Body") as MeshInstance3D
                expect(reported.grow(EPSILON).encloses(tank.get_node("Boat").transform*boat.mesh.get_aabb()), label + ": boat omitted from crop bounds")
                if enemy:
                    var original_colors := colors(body(tank))
                    Art.set_vehicle_state(peer, {"armor": 1}, .0)
                    expect(colors(body(tank)) == original_colors, label + ": enemy armor tint shared across instances")
                    expect(original_colors != colors(body(peer)), label + ": armor damage has no distinct appearance")
                    expect(tank.get_node("Frozen").visible and not peer.get_node("Frozen").visible, label + ": frozen state leaked")
                else:
                    check_player_identity(tank, nation, tier)
                expect(tank.get_meta("neutral_muzzle") == peer.get_meta("neutral_muzzle"), label + ": cosmetic motion changed neutral attachment")
                var frozen_pose := body(tank).transform
                Art.set_vehicle_state(tank, {"moving": false, "boat": false, "frozen": 0.0}, .0)
                expect(body(tank).transform == frozen_pose and not tank.get_node("Boat").visible,
                    label + ": zero-dt call advanced animation or left stale flotation")
                check_vehicle_motion(tank,peer,label)
                tank.free()
                peer.free()
                counts.vehicles += 1

    check_tier_proportions()
    check_national_shapes()
    check_armor_planes()
    check_cast_surfaces()
    completed_groups.append("vehicles")


func check_armor_planes() -> void:
    # This checks actual emitted armor geometry. Different labels or shape
    # coefficients alone cannot establish a planar welded plate.
    var cheek_runs: Dictionary = {}
    for nation in 3:
        for tier in 4:
            var d: Dictionary = Art._vehicle_profile(nation,false,0,tier)
            for hull in [false,true]:
                var sections: Array = Art._chassis_hull_sections(d) if hull else Art._cartoon_crown(d)
                var plan: Array = Art._armor_plan(d.shape,hull)
                var rings: Array = Art.Geometry.armor_rings(sections,plan)
                var welded: bool = plan[4] == .5 and plan[5] == .5
                var geometry = Art.Geometry.new()
                geometry.cast_loft(sections,Color.WHITE,Vector3.ZERO,0.0 if welded else .58,false,false,1.0,plan)
                var mesh: ArrayMesh = geometry.surfaces.armor.commit()
                var arrays := mesh.surface_get_arrays(0)
                var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
                var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
                var label: String = d.shape+("/hull" if hull else "/turret")
                for ring in rings:
                    for point in ring:
                        expect(vertices.has(point),label+": inspected section is absent from submitted armor")
                if not hull:
                    cheek_runs[d.shape] = (rings[1][3].z-rings[1][1].z)/(sections[1][3]-sections[1][2])
                if not welded: continue
                for offset in range(0,vertices.size(),3):
                    var normal := (vertices[offset+2]-vertices[offset]).cross(vertices[offset+1]-vertices[offset]).normalized()
                    for corner in 3:
                        expect(normal.dot(normals[offset+corner]) > .9999,label+": welded plate has a smoothed or false normal")
                for level in rings.size()-1:
                    for edge in 12:
                        var a: Vector3 = rings[level][edge]
                        var b: Vector3 = rings[level][(edge+1)%12]
                        var c: Vector3 = rings[level+1][edge]
                        var e: Vector3 = rings[level+1][(edge+1)%12]
                        var normal := (b-a).cross(c-a).normalized()
                        expect(absf((e-a).dot(normal)) < .000002,label+": broad armor plate is a twisted quad")
    expect(cheek_runs.abrams > cheek_runs.leopard2*4.0,
        "Abrams diagonal cheeks collapsed into the Leopard 2 A4 short front bevel")
    expect(cheek_runs.t90 > cheek_runs.tiger2*3.0,
        "T90 welded cheeks collapsed into the Tiger II flat-front corner treatment")
    # The T62 cap is deliberately small. Its visible roof fittings must sit on
    # it rather than enlarging the low dome to hide an attachment overhang.
    var t62: Dictionary = Art._vehicle_profile(1,false,0,2)
    var cap: Array = Art.Geometry.armor_rings(Art._cartoon_crown(t62),Art._armor_plan("t62")).back()
    var center_z: float = (cap[0].z+cap[6].z)*.5
    var supports: Array[Vector2] = []
    for angle in 24:
        var unit := Vector2(cos(TAU*angle/24.0),sin(TAU*angle/24.0))
        supports.append(Vector2(-t62.w*.22,center_z+.025)+unit*.0738*1.10)
        supports.append(Vector2(0,cap[6].z-.073)+unit*.044)
    for x in [-1.0,1.0]:
        for z in [-1.0,1.0]: supports.append(Vector2(t62.w*.22+x*.0615,center_z+z*.0925))
    for point in supports:
        for i in 12:
            var a := Vector2(cap[i].x,cap[i].z)
            var b := Vector2(cap[(i+1)%12].x,cap[(i+1)%12].z)
            expect((b-a).cross(point-a) >= -.00001,"T62 roof fixture extends beyond its supporting casting")


func check_cast_surfaces() -> void:
    # Test submitted geometry: continuous side normals must not introduce an
    # artificial seam at duplicate triangle vertices, while caps stay planar.
    for identity in [Vector2i(0,0),Vector2i(0,1),Vector2i(0,2),Vector2i(1,1),Vector2i(1,2),Vector2i(2,2)]:
        var d: Dictionary = Art._vehicle_profile(identity.x,false,0,identity.y)
        var sections: Array = Art._cartoon_crown(d)
        var plan: Array = Art._armor_plan(d.shape)
        var geometry = Art.Geometry.new()
        geometry.cast_loft(sections,Color.WHITE,Vector3.ZERO,.58,false,false,1.0,plan,true)
        var mesh: ArrayMesh = geometry.surfaces.armor.commit()
        var arrays := mesh.surface_get_arrays(0)
        var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
        var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
        var colors: PackedColorArray = arrays[Mesh.ARRAY_COLOR]
        var baseline = Art.Geometry.new()
        baseline.cast_loft(sections,Color.WHITE,Vector3.ZERO,.58,false,false,1.0,plan)
        var old_mesh: ArrayMesh = baseline.surfaces.armor.commit()
        expect(mesh.get_aabb().is_equal_approx(old_mesh.get_aabb()),d.shape+": casting refinement changed vehicle envelope")
        for ring in Art.Geometry.armor_rings(sections,plan):
            for point in ring:
                expect(vertices.has(point),d.shape+": cast shoulder or roof landmark moved")
        # Lower neck and upper shoulder meet at an intentional hard seam;
        # there must be no triangle seams inside either curved surface.
        var shared_groups: Array[Dictionary] = [{},{}]
        var side_vertices: int = (sections.size()-1)*20*6
        var neck_vertices: int = Art.Geometry.cast_shoulder_ring(sections)*20*6
        for offset in range(0,side_vertices,3):
            var shared: Dictionary = shared_groups[0 if offset < neck_vertices else 1]
            var face := (vertices[offset+2]-vertices[offset]).cross(vertices[offset+1]-vertices[offset]).normalized()
            for corner in 3:
                var index := offset+corner
                var position := vertices[index]
                expect(face.dot(normals[index]) > .75,d.shape+": continuous casting normal points away from surface")
                if shared.has(position):
                    var prior: int = shared[position]
                    expect(normals[prior].is_equal_approx(normals[index]),d.shape+": casting has a split side normal")
                    expect(colors[prior].is_equal_approx(colors[index]),d.shape+": casting has a split side tint")
                else:
                    shared[position] = index
        for index in range(side_vertices,vertices.size()):
            expect(absf(normals[index].y) > .9999,d.shape+": roof or underside lost its hard normal")


func armor_vertices(part: MeshInstance3D) -> PackedVector3Array:
    var groups: Array = part.get_meta("surface_groups")
    var surface := groups.find("armor")
    if surface < 0: return PackedVector3Array()
    return part.mesh.surface_get_arrays(surface)[Mesh.ARRAY_VERTEX]


func points_bounds(points: PackedVector3Array) -> AABB:
    if points.is_empty(): return AABB()
    var bounds := AABB(points[0],Vector3.ZERO)
    for point in points: bounds = bounds.expand(point)
    return bounds


func check_tier_proportions() -> void:
    var footprints: Array = []
    var turrets: Array = []
    var hull_heights: Array = []
    var roof_heights: Array = []
    for nation in 3:
        var nation_tracks: Array[Vector2] = []
        var nation_turrets: Array[Vector2] = []
        var nation_hulls: Array[float] = []
        var nation_roofs: Array[float] = []
        for tier in 4:
            var tank: Node3D = Art.make_tank(nation,false,tier,0,tier)
            var label := "proportions/%d/%d" % [nation,tier]
            var tracks: AABB = tank.get_node("TrackLeft").mesh.get_aabb().merge(
                tank.get_node("TrackRight").mesh.get_aabb())
            var footprint := Vector2(tracks.size.x,tracks.size.z)
            var hull := points_bounds(armor_vertices(tank.get_node("Hull")))
            var authored_aspect: float = footprint.y/(footprint.x/Art.VEHICLE_WIDTH_SCALE)
            expect(authored_aspect > 1.80 and authored_aspect < 2.36,
                label + ": chassis outside the source-informed stylized aspect envelope")
            expect(footprint.y < 2.01 and footprint.x < 1.1*Art.VEHICLE_WIDTH_SCALE,
                label + ": longer gun was allowed to enlarge the running gear envelope")
            expect(hull.size.x > footprint.x*.50 and hull.size.z > footprint.y*.60,
                label + ": growing running gear has no substantial matching hull")
            var reported: AABB = tank.get_meta("turret_bounds",AABB())
            var cast_vertices := PackedVector3Array()
            for vertex in armor_vertices(body(tank)):
                if reported.grow(EPSILON).has_point(vertex): cast_vertices.append(vertex)
            var actual := points_bounds(cast_vertices)
            # Verify that the claimed casting envelope is present in the
            # submitted armor vertices. Long guns and roof accessories cannot
            # supply the width/length used to pass the progression contract.
            expect(cast_vertices.size() >= 24 and actual.position.distance_to(reported.position) <= EPSILON
                and actual.end.distance_to(reported.end) <= EPSILON,label + ": turret bounds do not match rendered casting: %s versus %s" % [actual,reported])
            var turret := Vector2(actual.size.x,actual.size.z)
            expect(actual.size.y < footprint.y*.16 and turret.x < footprint.x*.88,
                label + ": turret reverted to an oversized tall crew pod")
            expect(turret.x > .4 and turret.y > .4,label + ": missing substantial turret body")
            if nation == 1 and tier == 0:
                # Check submitted casting vertices, not a placement parameter:
                # the T-34 fighting compartment must leave a long rear deck.
                expect(actual.get_center().z < tracks.get_center().z-tracks.size.z*.08,
                    label + ": T-34 turret drifted behind its forward fighting compartment")
                expect(hull.end.z-actual.end.z > actual.position.z-hull.position.z,
                    label + ": T-34 engine deck is shorter than the foredeck")
            else:
                expect(actual.get_center().z >= tracks.position.z+tracks.size.z*.35
                    and actual.get_center().z <= tracks.position.z+tracks.size.z*.58,
                    label + ": turret crowds the rear engine deck")
            if tier > 0:
                var previous_track: Vector2 = nation_tracks.back()
                var previous_turret: Vector2 = nation_turrets.back()
                # Real types can become shorter and wider (Sherman -> M26).
                # Preserve upgrade plan area instead of forcing both axes up.
                expect(footprint.x*footprint.y > previous_track.x*previous_track.y*1.10,
                    label + ": upgrading no longer enlarges the actual chassis plan area")
                expect(turret.x > previous_turret.x*1.05 and turret.y > previous_turret.y*1.05,
                    label + ": upgrading no longer enlarges the actual turret body")
            nation_tracks.append(footprint)
            nation_turrets.append(turret)
            nation_hulls.append(hull.end.y)
            nation_roofs.append(actual.end.y)
            tank.free()
        footprints.append(nation_tracks)
        turrets.append(nation_turrets)
        hull_heights.append(nation_hulls)
        roof_heights.append(nation_roofs)
    expect(hull_heights[0][0] > hull_heights[0][1]+.055,
        "Sherman no longer has its visibly higher hull than Pershing")
    expect(hull_heights[0][2] > hull_heights[0][1]+.035 and roof_heights[0][2] > roof_heights[0][1]+.065,
        "M60 no longer has a taller hull and turret than Pershing")
    expect(hull_heights[0][3] < hull_heights[0][2]-.055 and roof_heights[0][3] < roof_heights[0][2]-.1,
        "Abrams lost its low hull and turret relative to M60")
    for pair in [[Vector2i(0,0),Vector2i(0,1)], [Vector2i(1,1),Vector2i(1,3)], [Vector2i(2,2),Vector2i(2,3)]]:
        var slim: Vector2 = footprints[pair[0].x][pair[0].y]
        var broad: Vector2 = footprints[pair[1].x][pair[1].y]
        expect(slim.y/slim.x > broad.y/broad.x*1.08,
            "model-specific narrow/long and broad/short chassis collapsed to one aspect")
    for tier in 4:
        var areas: Array[float] = []
        for nation in 3:
            areas.append(footprints[nation][tier].x*footprints[nation][tier].y)
        expect(areas.max() < areas.min()*1.01,
            "same-tier historical shaping changed running-gear plan area")
        for axis in 2:
            var track_sizes := [footprints[0][tier][axis],footprints[1][tier][axis],footprints[2][tier][axis]]
            var turret_sizes := [turrets[0][tier][axis],turrets[1][tier][axis],turrets[2][tier][axis]]
            expect(track_sizes.max() <= track_sizes.min()*1.12+EPSILON,
                "same-tier national chassis axes differ by more than twelve percent")
            expect(turret_sizes.max() <= turret_sizes.min()*1.16+EPSILON,
                "same-tier national turret sizes differ by more than sixteen percent (real turret/bustle proportions)")


func triangle_values(arrays: Array, channel: int) -> Array:
    # SurfaceTool.index deduplicates color as well as position. Compare the
    # actual triangle stream so a paint boundary can split shared vertices.
    var result: Array = []
    var indices: PackedInt32Array = arrays[Mesh.ARRAY_INDEX]
    if indices.is_empty():
        for value in arrays[channel]: result.append(value)
    else:
        for index in indices: result.append(arrays[channel][index])
    return result


func armor_shape_triangles(part: MeshInstance3D, cut_height: float) -> Array[PackedVector3Array]:
    var result: Array[PackedVector3Array] = []
    var groups: Array = part.get_meta("surface_groups")
    var surface := groups.find("armor")
    if surface < 0: return result
    var arrays := part.mesh.surface_get_arrays(surface)
    var points := triangle_values(arrays,Mesh.ARRAY_VERTEX)
    for offset in range(0,points.size(),3):
        var triangle := PackedVector3Array()
        for corner in 3:
            triangle.append(part.transform*points[offset+corner])
        var clipped := PackedVector3Array()
        for corner in 3:
            var first := triangle[corner]
            var second := triangle[(corner+1)%3]
            if first.y >= cut_height:
                clipped.append(first)
            if (first.y >= cut_height) != (second.y >= cut_height):
                clipped.append(first.lerp(second,(cut_height-first.y)/(second.y-first.y)))
        for corner in range(1,clipped.size()-1):
            result.append(PackedVector3Array([clipped[0],clipped[corner],clipped[corner+1]]))
    return result


func armor_shape_mask(triangles: Array[PackedVector3Array], heading: float, elevation: float) -> PackedByteArray:
    # Use actual submitted triangle coverage, with no color, normals or part
    # names used as national identity. 48 pixels/unit is near gameplay scale;
    # all subjects retain their real coordinates and the same projection.
    var mask := PackedByteArray()
    mask.resize(128*128)
    var c := cos(deg_to_rad(heading))
    var s := sin(deg_to_rad(heading))
    var cy := cos(deg_to_rad(elevation))
    var sy := sin(deg_to_rad(elevation))
    for triangle in triangles:
        var projected: Array[Vector2] = []
        for point in triangle:
            projected.append(Vector2((point.x*c-point.z*s)*48.0+64.0,
                (point.y*cy+(point.x*s+point.z*c)*sy)*48.0+32.0))
        var a := projected[0]
        var b := projected[1]
        var d := projected[2]
        if absf((b-a).cross(d-a)) < .00001: continue
        var minimum := a.min(b).min(d).floor().clamp(Vector2.ZERO,Vector2(127,127))
        var maximum := a.max(b).max(d).ceil().clamp(Vector2.ZERO,Vector2(127,127))
        for y in range(int(minimum.y),int(maximum.y)+1):
            for x in range(int(minimum.x),int(maximum.x)+1):
                var point := Vector2(x+.5,y+.5)
                var ab := (b-a).cross(point-a)
                var bd := (d-b).cross(point-b)
                var da := (a-d).cross(point-d)
                if (ab >= 0.0 and bd >= 0.0 and da >= 0.0) or (ab <= 0.0 and bd <= 0.0 and da <= 0.0):
                    mask[y*128+x] = 1
    return mask


func armor_shape_difference(first: PackedByteArray, second: PackedByteArray) -> float:
    var intersection := 0
    var combined := 0
    for pixel in first.size():
        if first[pixel] != 0 and second[pixel] != 0: intersection += 1
        if first[pixel] != 0 or second[pixel] != 0: combined += 1
    return 1.0-float(intersection)/float(combined) if combined > 0 else 0.0


func check_national_shapes() -> void:
    # These are anti-collapse floors, not an aesthetic score or golden image.
    # Turret thresholds match the raylib guard; the common running gear is
    # deliberately excluded. Hulls must also differ as broad armor volumes,
    # so a roof trinket cannot disguise an identical lower vehicle.
    for tier in 4:
        var parts: Array = []
        for nation in 3:
            var tank: Node3D = Art.make_tank(nation,false,tier,0,tier)
            parts.append({"turret":armor_shape_triangles(body(tank),tank.get_meta("turret_bounds").position.y+.025),
                "hull":armor_shape_triangles(tank.get_node("Hull"),-INF)})
            tank.free()
        for part_name in ["turret","hull"]:
            var samples := [[],[],[]]
            for heading in [-45.0,0.0,45.0]:
                for elevation in [40.0,50.0,70.0]:
                    var masks: Array[PackedByteArray] = []
                    for nation in 3:
                        masks.append(armor_shape_mask(parts[nation][part_name],heading,elevation))
                    var pair := 0
                    for first in 3:
                        for second in range(first+1,3):
                            var difference := armor_shape_difference(masks[first],masks[second])
                            samples[pair].append(difference)
                            expect(difference > (.04 if part_name == "turret" else .02),
                                "shape/L%d/%s/%d-%d at %.0f/%.0f: national armor collapsed (%.4f)" % [tier,part_name,first,second,heading,elevation,difference])
                            pair += 1
            for pair in samples.size():
                var total := 0.0
                for value in samples[pair]: total += float(value)
                var average := total/9.0
                expect(average > (.075 if part_name == "turret" else .04),
                    "shape/L%d/%s/pair%d: national difference reduced to a small fixture (%.4f)" % [tier,part_name,pair,average])
                national_shape_results.append({"tier":tier,"part":part_name,"nation_pair":[[0,1],[0,2],[1,2]][pair],
                    "minimum_difference":samples[pair].min(),"mean_difference":average})
    expect(national_shape_results.size() == 24,"national shape checks did not cover every tier/part/nation pair")


func check_player_identity(p1: Node3D, nation: int, tier: int) -> void:
    var p2: Node3D = Art.make_tank(nation, false, tier, 1, tier)
    var identity_changed := false
    for part_name in ["Body","Hull","TrackLeft","TrackRight"]:
        var first := p1.get_node(part_name) as MeshInstance3D
        var second := p2.get_node(part_name) as MeshInstance3D
        var groups: Array = first.get_meta("surface_groups")
        if not expect(first.mesh.get_surface_count() == second.mesh.get_surface_count()
                and groups == second.get_meta("surface_groups"), "player identity changed material groups"):
            continue
        for surface in first.mesh.get_surface_count():
            var one: Array = first.mesh.surface_get_arrays(surface)
            var two: Array = second.mesh.surface_get_arrays(surface)
            expect(triangle_values(one, Mesh.ARRAY_VERTEX) == triangle_values(two, Mesh.ARRAY_VERTEX),
                "player identity changed vehicle triangles")
            expect(triangle_values(one, Mesh.ARRAY_NORMAL) == triangle_values(two, Mesh.ARRAY_NORMAL),
                "player identity changed vehicle normals")
            var one_colors := triangle_values(one, Mesh.ARRAY_COLOR)
            var two_colors := triangle_values(two, Mesh.ARRAY_COLOR)
            if groups[surface] in ["rubber", "metal", "optic"]:
                expect(one_colors == two_colors, "team color contaminated rubber/metal/optics")
            else:
                identity_changed = identity_changed or one_colors != two_colors
    expect(identity_changed, "P1 and P2 markings are identical")
    p2.free()


func vehicle_material_state(tank: Node3D, protected_only: bool = false) -> Array:
    var result: Array = []
    for part_name in ["Body", "Hull", "TrackLeft", "TrackRight"]:
        var part := tank.get_node(part_name) as MeshInstance3D
        var groups: Array = part.get_meta("surface_groups")
        for surface in part.mesh.get_surface_count():
            if protected_only and not groups[surface] in ["rubber", "metal", "optic"]: continue
            var material := part.get_active_material(surface) as StandardMaterial3D
            result.append([material, material.albedo_color, material.roughness, material.metallic,
                material.metallic_specular, material.diffuse_mode, material.shading_mode,
                material.vertex_color_use_as_albedo, material.vertex_color_is_srgb,
                material.emission_enabled, material.emission, material.emission_energy_multiplier,
                material.albedo_texture, material.uv1_triplanar, material.uv1_world_triplanar,
                material.uv1_scale, material.uv1_offset])
    return result


func armor_surface_average(part: MeshInstance3D, label: String) -> Dictionary:
    var groups: Array = part.get_meta("surface_groups")
    var surface := groups.find("armor")
    if not expect(surface >= 0, label + ": no broad armor surface"): return {}
    var arrays := part.mesh.surface_get_arrays(surface)
    var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
    var tints: PackedColorArray = arrays[Mesh.ARRAY_COLOR]
    var indices: PackedInt32Array = arrays[Mesh.ARRAY_INDEX]
    if not expect(tints.size() == vertices.size(), label + ": missing painted armor vertices"): return {}
    if indices.is_empty():
        indices.resize(vertices.size())
        for index in indices.size(): indices[index] = index
    var total := Vector3.ZERO
    var area := 0.0
    for start in range(0, indices.size(), 3):
        var a := indices[start]
        var b := indices[start + 1]
        var c := indices[start + 2]
        var weight := (vertices[b] - vertices[a]).cross(vertices[c] - vertices[a]).length() * .5
        var average := Vector3.ZERO
        for index in [a, b, c]:
            var linear := tints[index].srgb_to_linear()
            average += Vector3(linear.r, linear.g, linear.b) / 3.0
        total += average * weight
        area += weight
    expect(area > .1, label + ": national color reduced to a tiny decorative patch")
    return {"surface": surface, "linear_rgb": total / maxf(area, .000001)}


func expect_national_hue(part: MeshInstance3D, average: Dictionary, nation: int, label: String) -> void:
    if average.is_empty(): return
    var material := part.get_active_material(int(average.surface)) as StandardMaterial3D
    expect(material.vertex_color_use_as_albedo and material.vertex_color_is_srgb,
        label + ": armor material ignores its authored national paint")
    var rgb: Vector3 = average.linear_rgb
    # Evaluate the actual authored triangle area and active instance material,
    # including HP/carrier tint. These broad hue contracts do not copy the
    # implementation's color table or assume that matching a constant renders it.
    var effective := Color(rgb.x, rgb.y, rgb.z) * material.albedo_color.srgb_to_linear()
    var color := effective.linear_to_srgb()
    var maximum := maxf(color.r, maxf(color.g, color.b))
    var minimum := minf(color.r, minf(color.g, color.b))
    expect(maximum > .2, label + ": broad armor is too dark to carry a national color")
    if nation == 0:
        expect(color.g > color.r and color.r > color.b * 1.10,
            label + ": American armor lost its army olive hue: " + str(color))
    elif nation == 1:
        # The requested winter finish is muted gray-white. Keep highlight
        # headroom as well as a light neutral base; the previous near-white
        # target erased relief under actual game illumination.
        var base_color := Color(rgb.x,rgb.y,rgb.z).linear_to_srgb()
        var base_min := minf(base_color.r,minf(base_color.g,base_color.b))
        var base_max := maxf(base_color.r,maxf(base_color.g,base_color.b))
        expect(base_min > .55 and base_max < .82,
            label + ": Soviet base paint lacks muted whitewash/highlight headroom: " + str(base_color))
        # Existing enemy armor-status modulation is darker than the base paint.
        expect(minimum > .48 and maximum < .84 and maximum - minimum < maximum * .20 and color.r >= color.b,
            label + ": Soviet status tint lost its neutral winter identity: " + str(color))
    else:
        expect(maximum < .60 and minimum > .14 and maximum - minimum < maximum * .25,
            label + ": German armor lost its dark neutral gray base: " + str(color))


func check_camouflage(tank: Node3D, nation: int, label: String) -> void:
    for part_name in ["Body", "Hull", "TrackLeft", "TrackRight"]:
        var part := tank.get_node(part_name) as MeshInstance3D
        var groups: Array = part.get_meta("surface_groups")
        for surface in groups.size():
            var material := part.get_active_material(surface) as StandardMaterial3D
            if nation == 2 or groups[surface] != "armor":
                expect(material.albedo_texture == null,label + ": camouflage leaked into German gray or a protected material")
                continue
            var texture := material.albedo_texture
            if not expect(texture != null,label + ": national armor has no camouflage"): continue
            expect(material.uv1_triplanar and not material.uv1_world_triplanar,
                label + ": camouflage is not attached to the moving vehicle")
            if checked_camouflage.has(texture): continue
            checked_camouflage[texture] = true
            var image := texture.get_image()
            if not expect(image != null,label + ": missing camouflage pixels"): continue
            var base_pixels := 0
            var base_peak := 0.0
            var dark_pixels := 0
            var warm_pixels := 0
            var green_pixels := 0
            var adjacent_matches := 0
            for y in image.get_height():
                for x in image.get_width():
                    var pixel := image.get_pixel(x,y)
                    if minf(pixel.r,minf(pixel.g,pixel.b)) > .85:
                        base_pixels += 1
                        base_peak = maxf(base_peak,maxf(pixel.r,maxf(pixel.g,pixel.b)))
                    elif maxf(pixel.r,maxf(pixel.g,pixel.b)) < .65: dark_pixels += 1
                    if pixel.r > pixel.g * 1.05 and pixel.g > pixel.b * 1.05: warm_pixels += 1
                    if pixel.g > pixel.r * 1.08 and pixel.g > pixel.b * 1.12: green_pixels += 1
                    if x + 1 < image.get_width() and pixel.is_equal_approx(image.get_pixel(x + 1,y)):
                        adjacent_matches += 1
            var pixels := image.get_width()*image.get_height()
            # Textures modulate the authored paint; the light area keeps that
            # paint intact and broad patches supply brown/black or dark green.
            expect(base_pixels > pixels*.45 and base_pixels < pixels*.85 and dark_pixels > pixels*.08,
                label + ": camouflage lost its broad base/patch contrast")
            if nation == 0:
                expect(warm_pixels > pixels*.05,label + ": American woodland camouflage has no brown region")
            else:
                expect(green_pixels > pixels*.08,label + ": Soviet whitewash has no exposed green region")
                expect(base_peak < .95 and material.diffuse_mode == BaseMaterial3D.DIFFUSE_LAMBERT
                    and material.metallic_specular <= .10,
                    label + ": winter roofs reverted to a white/highlight-washed material")
            expect(adjacent_matches > (image.get_width()-1)*image.get_height()*.85,
                label + ": camouflage collapsed into fine per-pixel noise")


func status_patch_colors(tank: Node3D) -> Array:
    var result: Array = []
    for part_name in ["Body", "Hull"]:
        var part := tank.get_node(part_name) as MeshInstance3D
        var groups: Array = part.get_meta("surface_groups")
        var surface := groups.find("paint")
        if surface >= 0:
            result.append((part.get_active_material(surface) as StandardMaterial3D).albedo_color)
    return result


func check_national_colors(tank: Node3D, peer: Node3D, nation: int, enemy: bool, label: String) -> void:
    check_camouflage(tank,nation,label)
    var armor_averages: Array = []
    for part_name in ["Body", "Hull"]:
        armor_averages.append(armor_surface_average(tank.get_node(part_name), label + "/" + part_name))
    var original_meshes: Array = []
    var original_signatures: Array = []
    for part_name in ["Body", "Hull", "TrackLeft", "TrackRight"]:
        var part := tank.get_node(part_name) as MeshInstance3D
        original_meshes.append(part.mesh)
        original_signatures.append(immutable_mesh_signature(part.mesh))
    var original_nodes := mesh_nodes(tank)
    var protected_materials := vehicle_material_state(tank, true)
    var peer_materials := vehicle_material_state(peer)
    var allocated_materials: Array = []
    var health_swatches: Array = []
    var original_motion: Dictionary = tank.get_meta("motion").duplicate(true)
    var motion: Dictionary = tank.get_meta("motion")
    # Sample a real flash peak, then its trough below. At dt=0 the public
    # update must leave pose/gear unchanged while applying current state.
    motion.time = (TAU + PI * .5 - float(tank.get_meta("phase", 0.0)) * .75) / 8.1
    tank.set_meta("motion", motion)
    for hp in range(1, 5):
        var unlit_patch: Array = []
        for carrier in [false, true]:
            Art.set_vehicle_state(tank, {"armor": hp, "carries_bonus": carrier}, 0.0)
            var case_label := "%s/hp%d/carrier%s" % [label, hp, carrier]
            for index in 2:
                expect_national_hue(tank.get_node("Body" if index == 0 else "Hull"),
                    armor_averages[index], nation, case_label)
            expect(vehicle_material_state(tank, true) == protected_materials,
                case_label + ": HP/carrier tint changed metal, rubber or optics")
            expect(vehicle_material_state(peer) == peer_materials,
                case_label + ": instance tint contaminated a cached peer")
            var materials: Array = []
            for state in vehicle_material_state(tank): materials.append(state[0])
            if allocated_materials.is_empty(): allocated_materials = materials
            expect(materials == allocated_materials, case_label + ": state update reallocated materials")
            var patch := status_patch_colors(tank)
            if not carrier:
                unlit_patch = patch
                if not health_swatches.has(patch): health_swatches.append(patch)
            elif enemy:
                expect(not patch.is_empty() and patch != unlit_patch,
                    case_label + ": bonus carrier has no distinct status patch")
            else:
                expect(vehicle_material_state(tank) == peer_materials,
                    case_label + ": enemy HP/carrier state recolored a player")
            counts.national_color_cases += 1
    if enemy:
        expect(health_swatches.size() == 4, label + ": enemy HP levels lost their distinct status patches")
        var peak_patch := status_patch_colors(tank)
        Art.set_vehicle_state(tank, {"armor": 4, "bonus_carrier": true}, 0.0)
        expect(status_patch_colors(tank) == peak_patch, label + ": bonus carrier alias lost its flash")
        motion.time = (TAU + PI * 1.5 - float(tank.get_meta("phase", 0.0)) * .75) / 8.1
        tank.set_meta("motion", motion)
        Art.set_vehicle_state(tank, {"armor": 4, "carries_bonus": true}, 0.0)
        expect(status_patch_colors(tank) != peak_patch, label + ": carrier patch does not pulse")
    for index in 4:
        var part := tank.get_node(["Body", "Hull", "TrackLeft", "TrackRight"][index]) as MeshInstance3D
        expect(part.mesh == original_meshes[index]
            and immutable_mesh_signature(part.mesh) == original_signatures[index],
            label + ": coloring replaced or mutated cached geometry/surfaces/vertex colors")
    expect(mesh_nodes(tank) == original_nodes, label + ": coloring allocated extra mesh nodes")
    tank.set_meta("motion", original_motion)


func check_vehicle_motion(tank: Node3D, peer: Node3D, label: String) -> void:
    var root_pose := tank.transform
    var neutral_muzzle: Vector3 = tank.get_meta("neutral_muzzle")
    var hull := tank.get_node("Hull") as MeshInstance3D
    var hull_points: Array[Vector3] = []
    for surface in hull.mesh.get_surface_count():
        for point in hull.mesh.surface_get_arrays(surface)[Mesh.ARRAY_VERTEX]:
            if not hull_points.has(point): hull_points.append(point)
    var initial: Array = []
    var peer_colors: Array = []
    for part_name in ["Body","Hull","TrackLeft","TrackRight"]:
        initial.append(peer.get_node(part_name).transform)
        peer_colors.append(colors(peer.get_node(part_name)))
    var maximum_gap := 0.0
    # Repeated starts and quarter turns exercise the later low point of the
    # suspension, where a thicker lower hull can scrape through the ground.
    for frame in 1080:
        var moving := frame%360 >= 60 and frame%360 < 300
        var yaw := PI*.5*((frame/90)%4)
        Art.set_vehicle_state(tank,{"moving":moving,"yaw":yaw,"armor":4},1.0/60.0)
        var lowest_hull := INF
        for point in hull_points:
            lowest_hull = minf(lowest_hull,(hull.transform*point).y)
        expect(lowest_hull >= .0,label+": lower hull enters ground during suspension motion")
        expect(tank.transform == root_pose,label+": visual suspension moved simulation root")
        expect(tank.get_meta("neutral_muzzle") == neutral_muzzle,label+": motion rewrote native gun attachment")
        var bounds: AABB = tank.get_meta("visual_bounds")
        var index := 0
        for part_name in ["Body","Hull","TrackLeft","TrackRight"]:
            var part := tank.get_node(part_name) as MeshInstance3D
            var peer_part := peer.get_node(part_name) as MeshInstance3D
            var pose := part.transform
            expect(pose.is_finite() and pose.basis.is_equal_approx(pose.basis.orthonormalized()),label+": non-rigid or nonfinite animation")
            expect(bounds.grow(EPSILON).encloses(pose*part.mesh.get_aabb()),label+": crop omitted animated rigid part")
            expect(peer_part.transform == initial[index] and colors(peer_part) == peer_colors[index],label+": motion/tint leaked to cached peer")
            if part_name.begins_with("Track"):
                expect((pose*part.mesh.get_aabb()).position.y >= -.002,label+": track enters ground during suspension motion")
            var paused := part.transform
            Art.set_vehicle_state(tank,{"moving":moving,"yaw":yaw,"armor":4},0.0)
            expect(part.transform == paused,label+": pause advances suspension")
            index += 1
        maximum_gap = maxf(maximum_gap,absf(body(tank).position.y-tank.get_node("Hull").position.y))
    expect(maximum_gap > .002,label+": turret and hull have no independent follow motion")
    if bool(tank.get_meta("enemy")):
        var pose := body(tank).transform
        Art.set_vehicle_state(tank,{"moving":true,"yaw":PI,"frozen":1.0},.1)
        expect(body(tank).transform == pose,label+": clock-frozen enemy still breathes")
    counts.motion_cases += 1


func clip_axis(polygon: PackedVector2Array, axis: int, edge: float, lower: bool) -> PackedVector2Array:
    var result := PackedVector2Array()
    if polygon.is_empty(): return result
    var previous := polygon[-1]
    var was_inside := previous[axis] >= edge if lower else previous[axis] <= edge
    for point in polygon:
        var inside := point[axis] >= edge if lower else point[axis] <= edge
        if inside != was_inside:
            result.append(previous.lerp(point, (edge - previous[axis]) / (point[axis] - previous[axis])))
        if inside: result.append(point)
        previous = point
        was_inside = inside
    return result


func missing_quadrant_overlap(a: Vector3, b: Vector3, c: Vector3, quadrant: int) -> bool:
    var x := -.5 + .5 * (quadrant & 1)
    var z := -.5 + .5 * ((quadrant >> 1) & 1)
    var polygon := PackedVector2Array([Vector2(a.x, a.z), Vector2(b.x, b.z), Vector2(c.x, c.z)])
    polygon = clip_axis(polygon, 0, x, true)
    polygon = clip_axis(polygon, 0, x + .5, false)
    polygon = clip_axis(polygon, 1, z, true)
    polygon = clip_axis(polygon, 1, z + .5, false)
    var area := 0.0
    for index in polygon.size(): area += polygon[index].cross(polygon[(index + 1) % polygon.size()])
    return absf(area) > 0.000001


func check_brick_footprint(mesh: ArrayMesh, mask: int, label: String) -> void:
    for surface in mesh.get_surface_count():
        var arrays := mesh.surface_get_arrays(surface)
        var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
        var indices: PackedInt32Array = arrays[Mesh.ARRAY_INDEX]
        for point in vertices:
            var supported := false
            for quadrant in 4:
                var x := -.5 + .5 * (quadrant & 1)
                var z := -.5 + .5 * ((quadrant >> 1) & 1)
                supported = supported or ((mask & (1 << quadrant)) != 0 and point.x >= x - EPSILON and
                    point.x <= x + .5 + EPSILON and point.z >= z - EPSILON and point.z <= z + .5 + EPSILON)
            expect(supported, label + ": vertex outside surviving brick cells")
        # Vertex bounds alone miss a roof stretched across a destroyed quadrant.
        # Clip each projected face against the missing cells as well.
        for start in range(0, indices.size(), 3):
            for quadrant in 4:
                if (mask & (1 << quadrant)) == 0:
                    expect(not missing_quadrant_overlap(vertices[indices[start]], vertices[indices[start + 1]],
                        vertices[indices[start + 2]], quadrant), label + ": face bridges a destroyed quadrant")


func check_environment_edges() -> void:
    var rows: Array = []
    var masks: Array = []
    for row in 26: rows.append(".".repeat(26))
    masks.resize(676)
    masks.fill(15)
    rows[12] = ".".repeat(12)+"#@%"+".".repeat(11)
    var original_rows := rows.duplicate()
    var original_masks := masks.duplicate()
    var edges: Node3D = Art.make_environment_edges(rows,masks)
    expect(rows == original_rows and masks == original_masks,"environment shoulders mutate native terrain")
    check_tree(edges,"environment-edges")
    expect(mesh_nodes(edges).size() == 1,"environment shoulders are not a single mesh")
    var instance := body(edges)
    expect(instance.cast_shadow == GeometryInstance3D.SHADOW_CASTING_SETTING_OFF,"flat shoulders cast an obstacle shadow")
    expect(instance.mesh.get_surface_count() == 1 and mesh_triangles(instance.mesh) <= 80,"shoulder fixture surface/geometry budget exceeded")
    var arrays := instance.mesh.surface_get_arrays(0)
    for point in arrays[Mesh.ARRAY_VERTEX]:
        expect(point.y >= -.025 and point.y <= -.017,"shoulder leaves its ground plane")
        expect(point.x > 0.0 and point.x < 26.0 and point.z > 0.0 and point.z < 26.0,"shoulder leaves map")
        expect(str(rows[floori(point.z)])[floori(point.x)] == ".","shoulder changes an occupied terrain cell")
    for normal in arrays[Mesh.ARRAY_NORMAL]:
        expect(normal.dot(Vector3.UP) > .999,"shoulder has a vertical collider-like face")
    edges.free()
    rows[12] = ".".repeat(26)
    var clear: Node3D = Art.make_environment_edges(rows,masks)
    expect(mesh_nodes(clear).is_empty(),"clear map gains decorative roads")
    clear.free()


func check_world() -> void:
    check_environment_edges()
    # Neighboring half-lots meet at the same height along every roof crease.
    for kind in 4:
        for variant in 2:
            for sample in 21:
                var at := -.5+sample*.05
                for parity in 2:
                    expect(is_equal_approx(Art._roof_height(.5,at,kind,0,parity,variant),Art._roof_height(-.5,at,kind,1,parity,variant)),"roof x seam is discontinuous")
                    expect(is_equal_approx(Art._roof_height(at,.5,kind,parity,0,variant),Art._roof_height(at,-.5,kind,parity,1,variant)),"roof z seam is discontinuous")
    # Two complete rows across eight lots exercise the current roof families
    # and both cell parities without importing the private variant selector.
    for row in 2:
        for col in 16:
            for mask in 16:
                var tile: Node3D = Art.make_tile("#", mask, row, col)
                var label := "brick/%d/%d/%d" % [row, col, mask]
                if mask == 0:
                    expect(mesh_nodes(tile).is_empty(), label + ": destroyed cell still renders an obstacle")
                else:
                    var mesh := body(tile).mesh as ArrayMesh
                    check_tree(tile, label)
                    var triangles := mesh_triangles(mesh)
                    maximums.brick = maxi(maximums.brick, triangles)
                    expect(mesh.get_surface_count() == 1 and triangles <= 600, label + ": tile budget exceeded")
                    check_brick_footprint(mesh, mask, label)
                tile.free()
                counts.brick_cases += 1
    for tile_type in ["@", "%", "~", "-"]:
        for row in 2:
            for col in 2:
                var tile: Node3D = Art.make_tile(tile_type, 15, row, col)
                var again: Node3D = Art.make_tile(tile_type, 15, row, col)
                check_tree(tile, "terrain/" + tile_type)
                expect(body(tile).mesh == body(again).mesh, "terrain cache rebuilt identical geometry")
                tile.free()
                again.free()
    var forest_meshes: Dictionary = {}
    for row in 26:
        for col in 26:
            var tree: Node3D = Art.make_tile("%", 15, row, col)
            var label := "forest/%d/%d" % [row,col]
            check_tree(tree,label)
            var mesh := body(tree).mesh as ArrayMesh
            var bounds := mesh.get_aabb()
            expect(bounds.position.x >= -.499 and bounds.end.x <= .499 and bounds.position.z >= -.499 and bounds.end.z <= .499,
                label + ": foliage spills outside its cover footprint")
            expect(bounds.position.y >= -.02 and bounds.end.y <= 1.4,label + ": foliage height envelope exceeded")
            var triangles := mesh_triangles(mesh)
            maximums.forest = maxi(maximums.forest,triangles)
            expect(triangles <= 208 and mesh.get_surface_count() <= 2,label + ": foliage budget exceeded")
            forest_meshes[mesh.get_instance_id()] = mesh
            tree.free()
            counts.forest_cases += 1
    expect(forest_meshes.size() <= 16,"forest cache grows with map coordinates")
    for health in 5:
        for steel in [false, true]:
            var wall: Node3D = Art.make_base_wall(health, steel)
            check_tree(wall, "base-wall/%d/%s" % [health, steel])
            var bounds := body(wall).mesh.get_aabb()
            expect(bounds.position.x >= -.501 and bounds.end.x <= .501 and bounds.position.z >= -.501 and bounds.end.z <= .501,
                "base wall intrudes outside its collision cell")
            expect(bounds.end.y <= .101 if health == 0 else bounds.end.y > .15, "base wall misrepresents whether it blocks movement")
            wall.free()
            counts.base_walls += 1
    for nation in 3:
        var base: Node3D = Art.make_base(nation)
        check_tree(base, "headquarters/%d" % nation)
        var bounds := body(base).mesh.get_aabb()
        expect(bounds.position.x >= -1 and bounds.end.x <= 1 and bounds.position.z >= -1 and bounds.end.z <= 1,
            "headquarters intrudes into neighboring wall cells")
        base.free()
        counts.bases += 1
    for kind in 9:
        var pickup: Node3D = Art.make_pickup(kind)
        var again: Node3D = Art.make_pickup(kind)
        check_tree(pickup, "pickup/%d" % kind)
        var mesh := body(pickup).mesh as ArrayMesh
        var triangles := mesh_triangles(mesh)
        maximums.pickup = maxi(maximums.pickup, triangles)
        expect(triangles <= 1200 and mesh.get_surface_count() <= 3, "pickup budget exceeded")
        expect(mesh == body(again).mesh, "pickup geometry rebuilt for an identical instance")
        pickup.free()
        again.free()
        counts.pickups += 1
    var shell: Node3D = Art.make_shell()
    var shell_peer: Node3D = Art.make_shell()
    check_tree(shell, "shell")
    var shell_mesh := body(shell).mesh as ArrayMesh
    expect(shell_mesh.get_surface_count() == 1 and mesh_triangles(shell_mesh) <= 96,
        "shell exceeds the single-surface low-cost geometry budget")
    expect(body(shell_peer).mesh == shell_mesh, "identical shells rebuild immutable geometry")
    var shell_bounds := shell_mesh.get_aabb()
    expect(shell_bounds.position.z < 0.0 and shell_bounds.size.z > maxf(shell_bounds.size.x, shell_bounds.size.y),
        "shell no longer follows the local -Z firing axis")
    shell.free()
    shell_peer.free()
    completed_groups.append("world")


func check_directional_flash() -> void:
    var flash: Node3D = Art.make_explosion(false)
    var peer: Node3D = Art.make_explosion(false)
    var generic_mesh := body(flash).mesh as ArrayMesh
    var generic_signature := immutable_mesh_signature(generic_mesh)
    var material: Material = body(flash).material_override
    Art.set_muzzle_flash(flash, true)
    Art.set_muzzle_flash(peer, true)
    check_tree(flash, "directional-muzzle")
    var mesh := body(flash).mesh as ArrayMesh
    var signature := immutable_mesh_signature(mesh)
    expect(bool(flash.get_meta("muzzle_flash", false)) and mesh != generic_mesh,
        "muzzle mode fails to select distinct directional geometry")
    expect(mesh == body(peer).mesh and material == body(flash).material_override and
        material != body(peer).material_override, "muzzle mesh selection replaces or shares mutable fade state")
    expect(mesh.get_surface_count() == 1 and mesh_triangles(mesh) <= 96,
        "directional muzzle exceeds the single-surface geometry budget")
    expect(not flash.has_node("Smoke") and mesh_nodes(flash).size() == 1,
        "directional muzzle adds smoke or hidden per-shot mesh nodes")
    var bounds := mesh.get_aabb()
    expect(bounds.position.z < -.1 and bounds.end.z <= EPSILON and
        bounds.size.z > maxf(bounds.size.x, bounds.size.y), "muzzle burst points behind or sideways from the barrel")
    var root_basis := Basis.from_euler(Vector3(.12, -.8, .18))
    flash.basis = root_basis
    var root_position := Vector3(4.0, .8, 7.0)
    flash.position = root_position
    var initial_alpha := 0.0
    var late_alpha := 0.0
    var peer_color: Color = body(peer).material_override.albedo_color
    for phase in [-1.0, 0.0, .10, .35, .60, .90, 1.0, 2.0]:
        Art.set_effect_state(flash, phase, .23)
        var flame := body(flash)
        var fade := flame.material_override as StandardMaterial3D
        expect(flash.transform.is_finite() and flame.transform.is_finite(), "muzzle animation produces a nonfinite pose")
        expect(flash.position == root_position and flash.basis.orthonormalized().is_equal_approx(root_basis) and
            flash.scale.is_equal_approx(Vector3.ONE * .23), "muzzle animation changes its attachment position, firing axis, or size")
        expect(flame.position.is_zero_approx() and flame.rotation.is_zero_approx(),
            "directional flash adds an upright explosion drift or yaw below the firing basis")
        expect(flame.cast_shadow == GeometryInstance3D.SHADOW_CASTING_SETTING_OFF,
            "muzzle light adds a solid projectile-like shadow")
        expect(fade != null and fade.transparency == BaseMaterial3D.TRANSPARENCY_ALPHA and
            is_finite(fade.albedo_color.a) and fade.albedo_color.a >= 0.0 and fade.albedo_color.a <= 1.0 and
            is_finite(fade.emission_energy_multiplier) and fade.emission_energy_multiplier >= 0.0 and
            flame.transparency == 0.0, "muzzle fade is nonfinite or unsupported by Mobile")
        if phase == 0.0:
            initial_alpha = fade.albedo_color.a
            expect(flame.visible, "new muzzle flash is already expired")
        if phase == .90: late_alpha = fade.albedo_color.a
        if phase >= 1.0: expect(not flame.visible, "expired muzzle flash remains visible")
        expect(immutable_mesh_signature(mesh) == signature and immutable_mesh_signature(generic_mesh) == generic_signature,
            "muzzle animation mutates shared directional or generic geometry")
        expect(body(peer).material_override.albedo_color == peer_color, "muzzle age changes another flash's fade material")
    expect(initial_alpha > late_alpha, "muzzle flash does not fade before expiry")
    Art.set_muzzle_flash(flash, false)
    expect(not bool(flash.get_meta("muzzle_flash", false)) and body(flash).mesh == generic_mesh and
        body(flash).material_override == material, "disabling muzzle mode fails to restore generic mesh with the same material")
    flash.free()
    peer.free()


func check_effects() -> void:
    check_directional_flash()
    var effect: Node3D = Art.make_explosion()
    var peer: Node3D = Art.make_explosion()
    check_tree(effect, "explosion")
    var flame := body(effect)
    var smoke := effect.get_node("Smoke/Body") as MeshInstance3D
    var peer_smoke := peer.get_node("Smoke/Body") as MeshInstance3D
    var small: Node3D = Art.make_explosion(false)
    check_tree(small,"small-flash")
    expect(not small.has_node("Smoke") and mesh_nodes(small).size() == 1,"small flash allocates unused smoke")
    expect(body(small).mesh == flame.mesh,"small flash duplicates shared flame geometry")
    expect(body(small).material_override != flame.material_override,"small flash shares mutable opacity")
    for phase in [.0,.25,.5,.85]:
        Art.set_effect_state(small,phase,.23)
        var material := body(small).material_override as StandardMaterial3D
        expect(material != null and material.albedo_color.a > 0.0 and material.albedo_color.a <= 1.0,
            "smoke-free flash has invalid fade")
    small.free()
    maximums.effect = mesh_triangles(flame.mesh)+mesh_triangles(smoke.mesh)
    expect(maximums.effect <= 512 and flame.mesh.get_surface_count() == 1 and smoke.mesh.get_surface_count() == 1,
        "explosion geometry/surface budget exceeded")
    expect(flame.mesh == body(peer).mesh and smoke.mesh == peer_smoke.mesh, "effect geometry is not cached")
    expect(flame.material_override != body(peer).material_override and smoke.material_override != peer_smoke.material_override,
        "effect fades share mutable materials")
    for phase in [.0, .3, .7, .95]:
        Art.set_effect_state(effect, phase, 1.0)
        for instance in [flame, smoke]:
            var material := instance.material_override as StandardMaterial3D
            expect(material != null and material.transparency == BaseMaterial3D.TRANSPARENCY_ALPHA,
                "effect fade lacks Mobile-compatible material alpha")
            expect(material.albedo_color.a > 0 and material.albedo_color.a <= 1 and instance.transparency == 0,
                "effect opacity invalid or uses unsupported Mobile instance transparency")
        expect(body(peer).material_override.albedo_color.a == 1 and peer_smoke.material_override.albedo_color.a == 1,
            "updating one explosion faded a different explosion")
    expect(smoke.material_override.albedo_color.a < 1, "old explosion smoke never fades")
    effect.free()
    peer.free()
    var water: Node3D = Art.make_tile("~", 15, 0, 0)
    var material := body(water).get_active_material(0) as ShaderMaterial
    Art.set_visual_time(4.25)
    expect(material != null and is_equal_approx(material.get_shader_parameter("visual_time"), 4.25),
        "water cannot freeze its visual clock for paired captures")
    Art.set_visual_time(4.25)
    expect(is_equal_approx(material.get_shader_parameter("visual_time"), 4.25), "water time advances while frozen")
    water.free()
    completed_groups.append("effects")
