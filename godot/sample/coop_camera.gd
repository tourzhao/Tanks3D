extends RefCounted
## Pure presentation constraint for a shared orthographic camera. Native rig,
## player state, model geometry and simulation clocks remain untouched.
const ROAD_MARGIN := 3.0
const EPSILON := 0.00001

static func fit(native: Dictionary, aspect: float, safe: Rect2, players: Array) -> Dictionary:
    var result := native.duplicate(true)
    if players.size() != 2:
        return result
    if not native.get("position") is Vector3 or not native.get("target") is Vector3:
        return result
    var position: Vector3 = native.position
    var target: Vector3 = native.target
    var span := float(native.get("span", 0.0))
    if not position.is_finite() or not target.is_finite() or not is_finite(span) or span <= 0.0:
        return result
    if not is_finite(aspect) or aspect <= 0.0 or not safe.position.is_finite() or not safe.size.is_finite():
        return result
    if safe.size.x <= 0.0 or safe.size.y <= 0.0 or safe.position.x < 0.0 or safe.position.y < 0.0 or safe.end.x > 1.0 or safe.end.y > 1.0:
        return result
    var offset := target-position
    var distance := offset.length()
    if distance <= EPSILON:
        return result
    var forward := offset/distance
    var right := forward.cross(Vector3.UP)
    if right.length_squared() <= EPSILON:
        return result
    right = right.normalized()
    var up := right.cross(forward).normalized()
    var ground_forward := Vector3(forward.x,0.0,forward.z).normalized()
    # The game supports downward-looking cameras. Keep malformed/custom rigs
    # intact instead of manufacturing a new view direction for them.
    var sine := -forward.y
    if sine <= EPSILON:
        return result

    var minimum := Vector2(INF,INF)
    var maximum := Vector2(-INF,-INF)
    for player in players:
        if not player is Dictionary or not player.get("center") is Vector3 or not player.get("bounds") is AABB:
            return result
        var center: Vector3 = player.center
        var bounds: AABB = player.bounds
        if not center.is_finite() or not bounds.position.is_finite() or not bounds.size.is_finite() or bounds.size.x < 0.0 or bounds.size.y < 0.0 or bounds.size.z < 0.0:
            return result
        var points: Array[Vector3] = []
        for corner in 8:
            points.append(bounds.get_endpoint(corner))
        # Three world units along screen-forward/back on the ground, rather
        # than adding three units of projected screen height or tank height.
        var ground_center := Vector3(center.x,0.0,center.z)
        points.append(ground_center+ground_forward*ROAD_MARGIN)
        points.append(ground_center-ground_forward*ROAD_MARGIN)
        for point in points:
            var relative := point-target
            var projected := Vector2(right.dot(relative),up.dot(relative))
            minimum = minimum.min(projected)
            maximum = maximum.max(projected)

    var left := (safe.position.x-.5)*span*aspect
    var right_edge := (safe.end.x-.5)*span*aspect
    var bottom := (.5-safe.end.y)*span
    var top := (.5-safe.position.y)*span
    if minimum.x >= left-EPSILON and maximum.x <= right_edge+EPSILON and minimum.y >= bottom-EPSILON and maximum.y <= top+EPSILON:
        return result # Preserve a fitting native rig bit-for-bit, including its orbit.

    var fitted_span := maxf(span,maxf((maximum.x-minimum.x)/(safe.size.x*aspect),(maximum.y-minimum.y)/safe.size.y))
    if not is_finite(fitted_span):
        return result
    # Feasible target-shift intervals; choosing the closest point to zero
    # preserves as much of the native midpoint/follow as the safe area permits.
    var shift_min := maximum-Vector2((safe.end.x-.5)*fitted_span*aspect,(.5-safe.position.y)*fitted_span)
    var shift_max := minimum-Vector2((safe.position.x-.5)*fitted_span*aspect,(.5-safe.end.y)*fitted_span)
    var shift := Vector2(clampf(0.0,shift_min.x,maxf(shift_min.x,shift_max.x)),
        clampf(0.0,shift_min.y,maxf(shift_min.y,shift_max.y)))
    var translation := right*shift.x+up*shift.y
    var cosine := Vector2(forward.x,forward.z).length()
    var fitted_distance := maxf(distance,fitted_span*.5*cosine/sine+4.0)
    result.position = position+translation-forward*(fitted_distance-distance)
    result.target = target+translation
    result.span = fitted_span
    return result
