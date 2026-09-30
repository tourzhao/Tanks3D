extends RefCounted
## Presentation-only edge constraint after co-op framing. Translation is in
## the camera image plane, so projection size, view direction and world-point
## depths remain unchanged. Player silhouettes and road context take priority.
const ROAD_MARGIN := 3.0
const EPSILON := 0.00001

static func constrain(rig: Dictionary, aspect: float, safe: Rect2, players: Array,
        board: Rect2 = Rect2(0,0,26,26)) -> Dictionary:
    var result := rig.duplicate(true)
    if players.is_empty(): return result
    if not rig.get("position") is Vector3 or not rig.get("target") is Vector3: return result
    var position: Vector3 = rig.position
    var target: Vector3 = rig.target
    var span := float(rig.get("span",0.0))
    if not position.is_finite() or not target.is_finite() or not is_finite(span) or span <= 0.0 or not is_finite(aspect) or aspect <= 0.0:
        return result
    if not safe.position.is_finite() or not safe.size.is_finite() or safe.size.x <= 0.0 or safe.size.y <= 0.0 or safe.position.x < 0.0 or safe.position.y < 0.0 or safe.end.x > 1.0 or safe.end.y > 1.0:
        return result
    if not board.position.is_finite() or not board.size.is_finite() or board.size.x <= 0.0 or board.size.y <= 0.0:
        return result
    var forward := (target-position).normalized()
    var right := forward.cross(Vector3.UP)
    if not forward.is_finite() or right.length_squared() <= EPSILON or forward.y >= -EPSILON:
        return result
    right = right.normalized()
    var up := right.cross(forward).normalized()
    var ground_forward := Vector3(forward.x,0,forward.z).normalized()
    var visible_min := Vector2((safe.position.x-.5)*span*aspect,(.5-safe.end.y)*span)
    var visible_max := Vector2((safe.end.x-.5)*span*aspect,(.5-safe.position.y)*span)

    var player_min := Vector2(INF,INF)
    var player_max := Vector2(-INF,-INF)
    for player in players:
        if not player is Dictionary or not player.get("center") is Vector3 or not player.get("bounds") is AABB:
            return result
        var center: Vector3 = player.center
        var bounds: AABB = player.bounds
        if not center.is_finite() or not bounds.position.is_finite() or not bounds.size.is_finite() or bounds.size.x < 0.0 or bounds.size.y < 0.0 or bounds.size.z < 0.0:
            return result
        var points: Array[Vector3] = []
        for corner in 8: points.append(bounds.get_endpoint(corner))
        var ground_center := Vector3(center.x,0,center.z)
        points.append(ground_center+ground_forward*ROAD_MARGIN)
        points.append(ground_center-ground_forward*ROAD_MARGIN)
        for point in points:
            var relative := point-target
            var projected := Vector2(right.dot(relative),up.dot(relative))
            player_min = player_min.min(projected)
            player_max = player_max.max(projected)
    var allowed_min := player_max-visible_max
    var allowed_max := player_min-visible_min
    if allowed_min.x > allowed_max.x+EPSILON or allowed_min.y > allowed_max.y+EPSILON:
        return result # This span cannot fit the players; never conceal it by zooming.

    var map_min := Vector2(INF,INF)
    var map_max := Vector2(-INF,-INF)
    for corner in [board.position,Vector2(board.end.x,board.position.y),board.end,Vector2(board.position.x,board.end.y)]:
        var relative := Vector3(corner.x,0,corner.y)-target
        var projected := Vector2(right.dot(relative),up.dot(relative))
        map_min = map_min.min(projected)
        map_max = map_max.max(projected)
    var desired := Vector2.ZERO
    for axis in 2:
        if map_max[axis]-map_min[axis] >= visible_max[axis]-visible_min[axis]:
            # Keep the safe viewport within the map's projected axis extent.
            desired[axis] = clampf(0.0,map_min[axis]-visible_min[axis],map_max[axis]-visible_max[axis])
        else:
            # The whole map cannot fill this axis: balance unavoidable margins.
            desired[axis] = (map_min[axis]+map_max[axis]-visible_min[axis]-visible_max[axis])*.5
        desired[axis] = clampf(desired[axis],allowed_min[axis],maxf(allowed_min[axis],allowed_max[axis]))
    if desired.length_squared() <= EPSILON*EPSILON:
        return result
    var translation := right*desired.x+up*desired.y
    result.position = position+translation
    result.target = target+translation
    return result
