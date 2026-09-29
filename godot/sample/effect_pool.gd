extends RefCounted

# Lazy presentation-only leases. Each variant can satisfy all 48 live effects;
# retaining both kinds bounds the pool at 96 roots without type-change churn.
# A slot owns its mutable fade materials, while Art shares immutable meshes.
const Art = preload("res://art.gd")
const LIVE_LIMIT := 48
const RETAINED_LIMIT := LIVE_LIMIT * 2

var _parent: Node3D
var _small_free: Array[Node3D] = []
var _large_free: Array[Node3D] = []
var _slots: Array[Node3D] = []
var _active: Dictionary = {}
var _small_count := 0
var _large_count := 0
var _created := 0
var _reused := 0


func _init(parent: Node3D) -> void:
    _parent = parent


func acquire(position: Vector3, size: float, muzzle_flash: bool = false) -> Node3D:
    if _active.size() >= LIVE_LIMIT or not is_instance_valid(_parent):
        return null
    var large := size > 0.5
    var available: Array[Node3D] = _large_free if large else _small_free
    var node: Node3D
    if available.is_empty():
        node = Art.make_explosion(large)
        node.visible = false
        _parent.add_child(node)
        _slots.append(node)
        if large:
            _large_count += 1
        else:
            _small_count += 1
        _created += 1
    else:
        node = available.pop_back()
        _reused += 1
    _reset_visual(node, size, muzzle_flash and not large)
    node.position = position
    _active[node] = large
    node.visible = true
    return node


func release(node: Node3D) -> void:
    if not _active.has(node):
        return
    var large: bool = _active[node]
    _active.erase(node)
    node.visible = false
    # Keep the final fade state while hidden. Acquire resets it before showing
    # the node, avoiding redundant transform/material writes on every expiry.
    if large:
        _large_free.append(node)
    else:
        _small_free.append(node)


func clear() -> void:
    # keys() snapshots leases so release can update the dictionary safely.
    for node in _active.keys():
        release(node)
    # Session clear is infrequent and resets even already-idle visual state.
    for node in _small_free:
        _reset_visual(node, 0.23)
    for node in _large_free:
        _reset_visual(node, 1.0)


func dispose() -> void:
    # World ownership also frees every slot on scene teardown. The validity
    # check makes explicit disposal safe after that parent has already exited.
    for node in _slots:
        if is_instance_valid(node):
            node.free()
    _slots.clear()
    _active.clear()
    _small_free.clear()
    _large_free.clear()
    _small_count = 0
    _large_count = 0
    _parent = null


func stats() -> Dictionary:
    return {"active": _active.size(), "allocated": _slots.size(),
        "small": _small_count, "large": _large_count,
        "created": _created, "reused": _reused,
        "live_limit": LIVE_LIMIT, "retained_limit": RETAINED_LIMIT}


func _reset_visual(node: Node3D, size: float, muzzle_flash: bool = false) -> void:
    node.transform = Transform3D.IDENTITY
    # Muzzle and impact bursts share small leases and their fade material.
    # Restore the mesh kind before resetting its phase-dependent state.
    Art.set_muzzle_flash(node, muzzle_flash)
    var flame: MeshInstance3D = node.get_node("Body")
    flame.transform = Transform3D.IDENTITY
    flame.transparency = 0.0
    flame.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
    var smoke: Node3D = node.get_node_or_null("Smoke")
    if smoke != null:
        smoke.transform = Transform3D.IDENTITY
        var body: MeshInstance3D = smoke.get_node("Body")
        body.transform = Transform3D.IDENTITY
        body.visible = true
        body.transparency = 0.0
        body.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
    # Resets phase-dependent visibility, transforms, opacity and emission using
    # the same art function that advances live effects. No draw/prewarm occurs.
    Art.set_effect_state(node, 0.0, size)
