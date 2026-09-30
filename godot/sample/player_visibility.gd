extends RefCounted
## Original presentation-only hint for players obscured by brick buildings.
## Does not alter model materials/bounds, game snapshots or the random stream.
const GhostShader = preload("res://player_visibility.gdshader")
const MAP_SIZE := 26
const MASK_SIZE := MAP_SIZE*2
const HEIGHT_SCALE := 4.0
# These cells contain the separately modeled headquarters wall, not buildings.
const BASE_WALL_CELLS := [609,610,611,612,635,638,661,664]
const PART_PATHS := ["Body","Hull","TrackLeft","TrackRight","Boat/Body"]
const TEAM_COLORS := [Color("efc665"),Color("70d9df")]

var _world: Node3D
var _root: Node3D
var _players: Dictionary = {}
var _buildings: Array[AABB] = []
var _forests: Array[AABB] = []
var _forest_cells: Array[Rect2] = []
var _view_buildings: Array[AABB] = []
var _view_forests: Array[AABB] = []
var _last_camera := Transform3D.IDENTITY
var _projection_valid := false
var _mask: ImageTexture
var _mask_image: Image
var _created := 0


func _init(world: Node3D) -> void:
    _world = world
    _root = Node3D.new()
    _root.name = "PlayerVisibility"
    world.add_child(_root)
    var image := Image.create(MASK_SIZE,MASK_SIZE,false,Image.FORMAT_RGBA8)
    image.fill(Color(0,0,0,0))
    _mask_image = image
    _mask = ImageTexture.create_from_image(image)


func sync_terrain(tile_nodes: Dictionary, state: Dictionary) -> void:
    _buildings.clear()
    _forests.clear()
    _forest_cells.clear()
    _projection_valid = false
    var image := Image.create(MASK_SIZE,MASK_SIZE,false,Image.FORMAT_RGBA8)
    image.fill(Color(0,0,0,0))
    var rows: Array = state.get("map",[])
    var masks: Array = state.get("brick_masks",[])
    if rows.size() == MAP_SIZE and masks.size() == MAP_SIZE*MAP_SIZE:
        for index in tile_nodes:
            var row: int = int(index)/MAP_SIZE
            var col: int = int(index)%MAP_SIZE
            if row < 0 or row >= MAP_SIZE or str(rows[row]).length() != MAP_SIZE:
                continue
            var symbol := str(rows[row]).substr(col,1)
            if symbol != "#" and symbol != "%":
                continue
            var node: Node3D = tile_nodes[index]
            if not is_instance_valid(node):
                continue
            var body := node.get_node_or_null("Body") as MeshInstance3D
            if body == null or body.mesh == null:
                continue
            var bounds := body.global_transform*body.mesh.get_aabb()
            if symbol == "%":
                _forests.append(bounds.grow(.025))
                _forest_cells.append(Rect2(col,row,1,1))
            elif not int(index) in BASE_WALL_CELLS:
                var mask := int(masks[index])&15
                if mask == 0:
                    continue
                _buildings.append(bounds)
                # Half-cell texels preserve destroyed brick quadrants. Height
                # bounds reject the ground under a roof/remaining wall. This
                # remains a bounded terrain classification, not an object ID.
                var height_code := Color(1,clampf(bounds.position.y/HEIGHT_SCALE,0,1),
                    clampf(bounds.end.y/HEIGHT_SCALE,0,1),1)
                for quadrant in 4:
                    if mask&(1<<quadrant):
                        image.set_pixel(col*2+(quadrant&1),row*2+((quadrant>>1)&1),height_code)
    _mask_image = image
    _mask.update(image)
    # Immediately hide old hints when a forest/map change arrives; there is no
    # fade that could leak a player through newly active cover.
    for record in _players.values():
        record.root.visible = false


static func _view_bounds(bounds: AABB, camera_pose: Transform3D) -> AABB:
    var center := bounds.get_center()-camera_pose.origin
    var half := bounds.size*.5
    var right := camera_pose.basis.x
    var up := camera_pose.basis.y
    var forward := -camera_pose.basis.z
    var position := Vector3(right.dot(center),up.dot(center),forward.dot(center))
    var radius := Vector3(right.abs().dot(half),up.abs().dot(half),forward.abs().dot(half))
    return AABB(position-radius,radius*2)


static func _overlaps_in_front(occluder: AABB, player: AABB) -> bool:
    return occluder.position.x <= player.end.x and occluder.end.x >= player.position.x and \
        occluder.position.y <= player.end.y and occluder.end.y >= player.position.y and \
        occluder.position.z < player.end.z-.02 and occluder.end.z > 0.0


func _project_terrain(camera: Camera3D) -> void:
    if _projection_valid and _last_camera == camera.global_transform:
        return
    _last_camera = camera.global_transform
    _view_buildings.clear()
    _view_forests.clear()
    for bounds in _buildings: _view_buildings.append(_view_bounds(bounds,_last_camera))
    for bounds in _forests: _view_forests.append(_view_bounds(bounds,_last_camera))
    _projection_valid = true


func _reason(bounds: AABB) -> String:
    var footprint := Rect2(Vector2(bounds.position.x,bounds.position.z),Vector2(bounds.size.x,bounds.size.z))
    for cell in _forest_cells:
        if footprint.intersects(cell): return "forest_cover"
    var projected := _view_bounds(bounds,_last_camera)
    # Whole projected envelopes are deliberately conservative: even a partial
    # foreground canopy overlap disables the entire hint, not just one ray.
    for tree in _view_forests:
        if _overlaps_in_front(tree,projected): return "forest_in_front"
    for building in _view_buildings:
        if _overlaps_in_front(building,projected): return "building"
    return "clear"


func _release(id: int) -> void:
    if not _players.has(id): return
    var node: Node3D = _players[id].root
    if is_instance_valid(node): node.free()
    _players.erase(id)


func _create(id: int, source: Node3D) -> Dictionary:
    var root := Node3D.new()
    root.name = "Player%d"%id
    root.visible = false
    _root.add_child(root)
    var material := ShaderMaterial.new()
    material.shader = GhostShader
    material.set_shader_parameter("team_color",TEAM_COLORS[id])
    material.set_shader_parameter("building_mask",_mask)
    var copies: Array[MeshInstance3D] = []
    var originals: Array[MeshInstance3D] = []
    for path in PART_PATHS:
        var part := source.get_node_or_null(path) as MeshInstance3D
        if part == null or part.mesh == null: continue
        var ghost := MeshInstance3D.new()
        ghost.name = path.replace("/","_")
        ghost.mesh = part.mesh
        ghost.material_override = material
        ghost.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
        root.add_child(ghost)
        copies.append(ghost)
        originals.append(part)
    _created += 1
    return {"source":source,"root":root,"copies":copies,"originals":originals,"material":material}


func update(camera: Camera3D, vehicles: Dictionary, state: Dictionary, _dt: float) -> void:
    if not is_instance_valid(_root): return
    var live: Dictionary = {}
    if camera.projection == Camera3D.PROJECTION_ORTHOGONAL:
        _project_terrain(camera)
        for player in state.get("players",[]):
            var id := int(player.get("id",-1))
            var key := "p%d"%id
            if id < 0 or id > 1 or not player.get("active",false) or not vehicles.has(key):
                continue
            var source: Node3D = vehicles[key]
            if not is_instance_valid(source): continue
            live[id] = true
            if _players.has(id) and _players[id].source != source:
                _release(id)
            if not _players.has(id): _players[id] = _create(id,source)
            var record: Dictionary = _players[id]
            var reason := "hidden"
            if float(player.get("creating",0.0)) > 0.0:
                reason = "creating"
            elif source.is_visible_in_tree() and source.has_meta("visual_bounds"):
                reason = _reason(source.global_transform*source.get_meta("visual_bounds"))
            var enabled := reason == "building"
            record.reason = reason
            record.root.visible = enabled
            if not enabled: continue
            for index in record.copies.size():
                var original: MeshInstance3D = record.originals[index]
                var copy: MeshInstance3D = record.copies[index]
                copy.visible = is_instance_valid(original) and original.is_visible_in_tree()
                if copy.visible:
                    copy.global_transform = original.global_transform
                    if copy.mesh != original.mesh: copy.mesh = original.mesh
    for id in _players.keys():
        if not live.has(id): _release(id)


func stats() -> Dictionary:
    var active_ids: Array = []
    var reasons: Dictionary = {}
    for id in _players:
        var record: Dictionary = _players[id]
        if record.root.visible: active_ids.append(id)
        reasons[id] = record.get("reason","hidden")
    return {"players":_players.size(),"active":active_ids.size(),"created":_created,
        "active_ids":active_ids,"reasons":reasons,"buildings":_buildings.size(),
        "forests":_forests.size(),"mask_size":MASK_SIZE}


func debug_stats() -> Dictionary:
    return stats()


func dispose() -> void:
    _players.clear()
    if is_instance_valid(_root): _root.free()
    _root = null
    _world = null
