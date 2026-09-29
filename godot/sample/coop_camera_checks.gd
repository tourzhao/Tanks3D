extends SceneTree
## Independent projection checks use Godot's actual orthographic Camera3D
## unprojection; no graphics device or fabricated gameplay state is required.
const CoopCamera = preload("res://coop_camera.gd")
const EPSILON := 0.0002
var valid := true
var cases := 0
var viewport: SubViewport
var camera: Camera3D

func check(condition: bool, message: String) -> void:
    if not condition:
        valid = false
        push_error(message)

func rig(yaw: float = 0.0, elevation: float = 50.0, focus: Vector3 = Vector3(13,.35,13), span: float = 18.5) -> Dictionary:
    var azimuth := deg_to_rad(yaw)
    var angle := deg_to_rad(elevation)
    var offset := Vector3(sin(azimuth)*cos(angle),sin(angle),cos(azimuth)*cos(angle))
    return {"position":focus+offset*21.017376,"target":focus,"span":span,"native_metadata":{"unchanged":true}}

func player(center: Vector3, size: Vector3 = Vector3(1.5,1.3,1.8)) -> Dictionary:
    return {"center":center,"bounds":AABB(center-Vector3(size.x*.5,0,size.z*.5),size)}

func configure(value: Dictionary, dimensions: Vector2i) -> void:
    viewport.size = dimensions
    camera.position = value.position
    camera.look_at(value.target,Vector3.UP)
    camera.size = value.span

func projected(point: Vector3) -> Vector2:
    return camera.unproject_position(point)/Vector2(viewport.size)

func required_points(value: Dictionary, players: Array) -> Array[Vector3]:
    var forward: Vector3 = (value.target-value.position).normalized()
    var ground_direction := Vector3(forward.x,0,forward.z).normalized()
    var result: Array[Vector3] = []
    for item in players:
        var bounds: AABB = item.bounds
        for corner in 8:
            result.append(bounds.get_endpoint(corner))
        var center: Vector3 = item.center
        center.y = 0.0
        result.append(center+ground_direction*3.0)
        result.append(center-ground_direction*3.0)
    return result

func fits(points: Array[Vector3], safe: Rect2, tolerance: float = EPSILON) -> bool:
    for point in points:
        var pixel := projected(point)
        if not pixel.is_finite() or pixel.x < safe.position.x-tolerance or pixel.x > safe.end.x+tolerance or pixel.y < safe.position.y-tolerance or pixel.y > safe.end.y+tolerance:
            return false
    return true

func fit_case(native: Dictionary, dimensions: Vector2i, safe: Rect2, players: Array, label: String) -> void:
    var original := native.duplicate(true)
    var originals := players.duplicate(true)
    var aspect := float(dimensions.x)/dimensions.y
    var result: Dictionary = CoopCamera.fit(native,aspect,safe,players)
    configure(result,dimensions)
    var points := required_points(native,players)
    check(fits(points,safe),label+": projected model/road envelope crosses the safe region")
    for point in points:
        check(not camera.is_position_behind(point),label+": near plane clipped required geometry")
    var native_forward: Vector3 = (native.target-native.position).normalized()
    var fitted_forward: Vector3 = (result.target-result.position).normalized()
    check(native_forward.is_equal_approx(fitted_forward),label+": fit changed yaw/elevation")
    check(float(result.span) >= float(native.span)-EPSILON,label+": fit zoomed in")
    check(native == original and players == originals and result.native_metadata == native.native_metadata,
        label+": fit mutated input state or discarded native metadata")
    var reversed: Array = players.duplicate()
    reversed.reverse()
    check(CoopCamera.fit(native,aspect,safe,reversed) == result,label+": P1/P2 order changed camera")

    # Reducing an expanded size cannot fit the projected envelope even with
    # an arbitrary translation: its width or height exceeds the safe rectangle.
    if float(result.span) > float(native.span)+.002:
        var smaller: Dictionary = result.duplicate(true)
        smaller.span = float(result.span)-.001
        configure(smaller,dimensions)
        var lo := Vector2(INF,INF)
        var hi := Vector2(-INF,-INF)
        for point in points:
            lo = lo.min(projected(point))
            hi = hi.max(projected(point))
        check((hi-lo).x > safe.size.x or (hi-lo).y > safe.size.y,label+": fit enlarged more than necessary")
        var sine := -native_forward.y
        var cosine := Vector2(native_forward.x,native_forward.z).length()
        check(result.position.distance_to(result.target)+EPSILON >= float(result.span)*.5*cosine/sine+4.0,
            label+": enlarged view did not retreat its near plane")

    # Returning either nonzero shift axis toward the native target must violate
    # a boundary; otherwise the chosen translation was unnecessarily large.
    var right := native_forward.cross(Vector3.UP).normalized()
    var up := right.cross(native_forward).normalized()
    var translation: Vector3 = result.target-native.target
    for axis in [right,up]:
        var amount: float = translation.dot(axis)
        if absf(amount) > .01:
            var closer: Dictionary = result.duplicate(true)
            closer.position -= axis*amount*.1
            closer.target -= axis*amount*.1
            configure(closer,dimensions)
            check(not fits(points,safe,0.000001),label+": target moved farther than required")
    cases += 1

func _initialize() -> void:
    run_checks.call_deferred()

func run_checks() -> void:
    viewport = SubViewport.new()
    viewport.own_world_3d = true
    root.add_child(viewport)
    camera = Camera3D.new()
    camera.projection = Camera3D.PROJECTION_ORTHOGONAL
    camera.keep_aspect = Camera3D.KEEP_HEIGHT
    camera.near = .1
    camera.far = 150.0
    viewport.add_child(camera)
    camera.make_current()
    var safe := Rect2(.04,.15,.92,.80)
    var nearby: Array = [player(Vector3(11,0,23)),player(Vector3(15,0,23))]
    var native := rig(0,50,Vector3(13,.35,23))
    check(CoopCamera.fit(native,16.0/9.0,safe,nearby) == native,"Nearby players changed a fitting native rig")
    for roster in [[],[nearby[0]],[nearby[0],nearby[1],nearby[0]]]:
        check(CoopCamera.fit(native,16.0/9.0,safe,roster) == native,"Non-two-player view changed")
    var wide := rig(0,50,Vector3(13,.35,13),40.0)
    var separated: Array = [player(Vector3(13,0,.7)),player(Vector3(13,0,25.3))]
    check(CoopCamera.fit(wide,16.0/9.0,safe,separated) == wide,"Already fitting co-op rig changed")
    var layouts := [[Vector3(13,0,.7),Vector3(13,0,25.3)],
        [Vector3(.7,0,13),Vector3(25.3,0,13)],
        [Vector3(.7,0,.7),Vector3(25.3,0,25.3)],
        [Vector3(.7,0,25.3),Vector3(25.3,0,.7)]]
    for dimensions in [Vector2i(1280,720),Vector2i(720,720),Vector2i(720,1280)]:
        for yaw in [-45.0,0.0,45.0]:
            for elevation in [40.0,50.0,70.0]:
                for layout in layouts:
                    # Include a broad flotation footprint and a tall heavy pod.
                    var players: Array = [player(layout[0],Vector3(1.8,1.3,1.8)),player(layout[1],Vector3(1.5,1.45,1.8))]
                    fit_case(rig(yaw,elevation),dimensions,safe,players,"extremes/%s/%s/%s/%s" % [dimensions,yaw,elevation,layout])
                # Native follow/zoom can lag a moving or newly respawned pair.
                var lagged: Array = [player(Vector3(19,0,4)),player(Vector3(23,0,8))]
                fit_case(rig(yaw,elevation,Vector3(6,.35,22)),dimensions,safe,lagged,"lag/%s/%s/%s" % [dimensions,yaw,elevation])
    for invalid in [Rect2(),Rect2(-.1,.1,1,.8),Rect2(.1,.1,1,1)]:
        check(CoopCamera.fit(native,16.0/9.0,invalid,separated) == native,"Invalid safe area changed native view")
    check(CoopCamera.fit(native,0.0,safe,separated) == native,"Invalid aspect changed native view")
    check(CoopCamera.fit(native,16.0/9.0,safe,[{},{}]) == native,"Missing player geometry changed native view")
    viewport.free()
    if valid:
        print("TANKS_COOP_CAMERA_PASSED projected-bounds/road-margin/minimum-fit/orbit/solo-nearby/order/lag/aspect cases=%d" % cases)
    quit(0 if valid else 1)
