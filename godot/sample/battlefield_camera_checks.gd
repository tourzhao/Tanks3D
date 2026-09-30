extends SceneTree
const EdgeCamera = preload("res://battlefield_camera.gd")
const CoopCamera = preload("res://coop_camera.gd")
const EPSILON := 0.0002
var valid := true
var cases := 0
var reduced := 0
var unchanged := 0
var viewport: SubViewport
var camera: Camera3D

func check(condition: bool, message: String) -> void:
    if not condition:
        valid = false
        push_error(message)

func rig(yaw: float, elevation: float, focus: Vector3, span: float = 18.5) -> Dictionary:
    var a := deg_to_rad(yaw)
    var e := deg_to_rad(elevation)
    var distance := maxf(21.017376,span*.5*cos(e)/sin(e)+4.0)
    return {"position":focus+Vector3(sin(a)*cos(e),sin(e),cos(a)*cos(e))*distance,
        "target":focus,"span":span,"native_metadata":{"untouched":true}}

func player(center: Vector3, size: Vector3 = Vector3(1.5,1.3,1.8)) -> Dictionary:
    return {"center":center,"bounds":AABB(center-Vector3(size.x*.5,0,size.z*.5),size)}

func configure(value: Dictionary, dimensions: Vector2i) -> void:
    viewport.size = dimensions
    camera.position = value.position
    camera.look_at(value.target,Vector3.UP)
    camera.size = value.span

func project(point: Vector3) -> Vector2:
    return camera.unproject_position(point)/Vector2(viewport.size)

func player_points(value: Dictionary, players: Array) -> Array[Vector3]:
    var direction: Vector3 = value.target-value.position
    direction.y = 0.0
    direction = direction.normalized()
    var result: Array[Vector3] = []
    for item in players:
        var bounds: AABB = item.bounds
        for corner in 8: result.append(bounds.get_endpoint(corner))
        var center: Vector3 = item.center
        center.y = 0.0
        result.append(center+direction*3.0)
        result.append(center-direction*3.0)
    return result

func fits(points: Array[Vector3], safe: Rect2) -> bool:
    for point in points:
        if not safe.grow(EPSILON).has_point(project(point)):
            return false
    return true

func clip(polygon: Array[Vector2], axis: int, edge: float, lower: bool) -> Array[Vector2]:
    var result: Array[Vector2] = []
    if polygon.is_empty(): return result
    var previous := polygon[-1]
    var previous_inside := previous[axis] >= edge if lower else previous[axis] <= edge
    for point in polygon:
        var inside := point[axis] >= edge if lower else point[axis] <= edge
        if inside != previous_inside:
            result.append(previous.lerp(point,(edge-previous[axis])/(point[axis]-previous[axis])))
        if inside: result.append(point)
        previous = point
        previous_inside = inside
    return result

func board_coverage(board: Rect2, safe: Rect2) -> float:
    # Clip the actual projected board polygon, not its bounding rectangle.
    # This independently catches regressions in visible grass at oblique yaw.
    var polygon: Array[Vector2] = []
    for corner in [board.position,Vector2(board.end.x,board.position.y),board.end,Vector2(board.position.x,board.end.y)]:
        polygon.append(project(Vector3(corner.x,0,corner.y)))
    polygon = clip(polygon,0,safe.position.x,true)
    polygon = clip(polygon,0,safe.end.x,false)
    polygon = clip(polygon,1,safe.position.y,true)
    polygon = clip(polygon,1,safe.end.y,false)
    var area := 0.0
    for index in polygon.size():
        area += polygon[index].cross(polygon[(index+1)%polygon.size()])
    return absf(area)*.5

func exercise(incoming: Dictionary, dimensions: Vector2i, safe: Rect2, players: Array,
        label: String, board: Rect2 = Rect2(0,0,26,26)) -> Dictionary:
    var aspect := float(dimensions.x)/dimensions.y
    var original := incoming.duplicate(true)
    var roster := players.duplicate(true)
    configure(incoming,dimensions)
    var points := player_points(incoming,players)
    var was_safe := fits(points,safe)
    var old_coverage := board_coverage(board,safe)
    var forward: Vector3 = (incoming.target-incoming.position).normalized()
    var result: Dictionary = EdgeCamera.constrain(incoming,aspect,safe,players,board)
    configure(result,dimensions)
    check(incoming == original and players == roster and result.native_metadata == incoming.native_metadata,
        label+": constraint mutated source state")
    check(result.span == incoming.span,label+": edge fit changed scale")
    var offset: Vector3 = result.position-result.target
    check(offset.is_equal_approx(incoming.position-incoming.target),label+": edge fit changed orbit/direction")
    var shift: Vector3 = result.target-incoming.target
    check(absf(shift.dot(forward)) < EPSILON,label+": edge fit moved along camera depth")
    for point in points:
        var before: float = (point-incoming.position).dot(forward)
        var after: float = (point-result.position).dot(forward)
        check(absf(before-after) < EPSILON,label+": near-plane depth changed")
        if before >= .1+EPSILON:
            check(not camera.is_position_behind(point),label+": edge movement introduced near clipping")
    var lo := Vector2(INF,INF)
    var hi := Vector2(-INF,-INF)
    for point in points:
        lo = lo.min(project(point))
        hi = hi.max(project(point))
    if (hi-lo).x <= safe.size.x+EPSILON and (hi-lo).y <= safe.size.y+EPSILON:
        check(fits(points,safe),label+": feasible model/road envelope left the safe region")
    else:
        check(result == incoming,label+": an impossible player fit changed the camera")
    var new_coverage := board_coverage(board,safe)
    if was_safe:
        check(new_coverage+0.00001 >= old_coverage,label+": visible off-board area increased")
    if new_coverage > old_coverage+0.00001: reduced += 1
    if result == incoming: unchanged += 1
    var reversed := players.duplicate()
    reversed.reverse()
    check(EdgeCamera.constrain(incoming,aspect,safe,reversed,board) == result,label+": player order changed framing")
    # A second application must not creep toward a different answer each frame.
    var again: Dictionary = EdgeCamera.constrain(result,aspect,safe,players,board)
    check((again.position as Vector3).is_equal_approx(result.position) and
        (again.target as Vector3).is_equal_approx(result.target) and again.span == result.span,
        label+": repeated edge constraints drift")
    cases += 1
    return result

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
    var safe := Rect2(.035,.15,.93,.81)
    var positions := [Vector3(.7,0,.7),Vector3(13,0,.7),Vector3(25.3,0,.7),
        Vector3(.7,0,13),Vector3(13,0,13),Vector3(25.3,0,13),
        Vector3(.7,0,25.3),Vector3(13,0,25.3),Vector3(25.3,0,25.3)]
    var pairs := [[Vector3(1.3,0,2),Vector3(2.7,0,2)],
        [Vector3(23.3,0,24),Vector3(24.7,0,24)],
        [Vector3(13,0,.7),Vector3(13,0,25.3)],
        [Vector3(.7,0,13),Vector3(25.3,0,13)],
        [Vector3(.7,0,.7),Vector3(25.3,0,25.3)]]
    for dimensions in [Vector2i(1280,720),Vector2i(720,720),Vector2i(720,1280)]:
        var aspect: float = float(dimensions.x)/dimensions.y
        for yaw in [-45.0,-20.0,0.0,20.0,45.0]:
            for elevation in [40.0,55.0,70.0]:
                for center in positions:
                    exercise(rig(yaw,elevation,center+Vector3(0,.35,0)),dimensions,safe,[player(center)],
                        "solo/%s/%s/%s/%s" % [dimensions,yaw,elevation,center])
                for pair in pairs:
                    var players: Array = [player(pair[0],Vector3(1.8,1.3,1.8)),player(pair[1],Vector3(1.5,1.45,1.8))]
                    var focus: Vector3 = (pair[0]+pair[1])*.5+Vector3(0,.35,0)
                    var fitted: Dictionary = CoopCamera.fit(rig(yaw,elevation,focus),aspect,safe,players)
                    exercise(fitted,dimensions,safe,players,"coop/%s/%s/%s/%s" % [dimensions,yaw,elevation,pair])
                exercise(rig(yaw,elevation,Vector3(4,.35,23)),dimensions,safe,[player(Vector3(19,0,8))],
                    "lag/%s/%s/%s" % [dimensions,yaw,elevation])
                exercise(rig(yaw,elevation,Vector3(8,.35,8),40.0),dimensions,safe,[player(Vector3(12,0,12))],
                    "small-board/%s/%s/%s" % [dimensions,yaw,elevation],Rect2(10,10,6,6))

    # A fixed-angle loop covers every point around all four map edges. The
    # result must join at the seam and cannot jump farther than its input moves.
    for yaw in [-45.0,0.0,45.0]:
        for elevation in [40.0,50.0,70.0]:
            var previous: Dictionary = {}
            var first: Dictionary = {}
            var last_center := Vector3.ZERO
            for index in 121:
                var side: int = (index % 120)/30
                var along := float(index % 30)/30.0
                var center := Vector3.ZERO
                if side == 0: center = Vector3(1.0+24.0*along,0,1)
                elif side == 1: center = Vector3(25,0,1.0+24.0*along)
                elif side == 2: center = Vector3(25.0-24.0*along,0,25)
                else: center = Vector3(1,0,25.0-24.0*along)
                var value := exercise(rig(yaw,elevation,center+Vector3(0,.35,0)),Vector2i(1280,720),safe,[player(center)],
                    "loop/%s/%s/%s" % [yaw,elevation,index])
                if previous.is_empty(): first = value
                else:
                    check((value.target as Vector3).distance_to(previous.target) <= center.distance_to(last_center)*1.01+EPSILON,
                        "Edge constraint introduced a discontinuous camera jump")
                previous = value
                last_center = center
            check((first.target as Vector3).is_equal_approx(previous.target),"Edge loop did not close continuously")

    var central := rig(0,50,Vector3(13,.35,13))
    check(EdgeCamera.constrain(central,16.0/9.0,safe,[]) == central,"No-player camera changed")
    # This narrower viewport lies wholly inside the large board at its center.
    check(EdgeCamera.constrain(central,1.0,safe,[player(Vector3(13,0,13))]) == central,"Well-covered central camera changed")
    var too_small := rig(0,50,Vector3(13,.35,13),1.0)
    check(EdgeCamera.constrain(too_small,1.0,safe,[player(Vector3(13,0,13))]) == too_small,"Impossible player envelope triggered zoom or cropping")
    var small_board := Rect2(10,10,6,6)
    var centered: Dictionary = EdgeCamera.constrain(rig(0,50,Vector3(11,.35,12),40),16.0/9.0,safe,[player(Vector3(13,0,13))],small_board)
    configure(centered,Vector2i(1280,720))
    check(project(Vector3(13,0,13)).distance_to(safe.get_center()) < EPSILON,"Small map blank margins are not centered")
    check(reduced > 100 and unchanged > 0,"Edge cases neither improve coverage nor preserve fitting views")
    viewport.free()
    if valid:
        print("TANKS_BATTLEFIELD_CAMERA_PASSED "+JSON.stringify({"status":"passed","cases":cases,
            "checks":["only-translation","player-envelope","road-margin","map-coverage","centering",
                "angles","order","continuity","lag","near-depth","no-players"]}))
    quit(0 if valid else 1)
