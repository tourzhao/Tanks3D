extends SceneTree
## Structural/cover checks. Actual depth shader acceptance requires Metal images.
const Art = preload("res://art.gd")
const Visibility = preload("res://player_visibility.gd")
var failures: Array[String] = []
var cases := 0
var world: Node3D
var camera: Camera3D
var hint: RefCounted
var tiles: Dictionary = {}
var vehicles: Dictionary = {}
var state: Dictionary


func expect(value: bool, detail: String) -> void:
    cases += 1
    if not value:
        failures.append(detail)
        push_error("Player visibility contract: "+detail)


func _initialize() -> void:
    call_deferred("run")


func tile(symbol: String, col: int, row: int, mask: int = 15) -> void:
    var index := row*26+col
    if tiles.has(index):
        tiles[index].free()
        tiles.erase(index)
    var line: String = state.map[row]
    state.map[row] = line.left(col)+symbol+line.substr(col+1)
    state.brick_masks[index] = mask
    if symbol != ".":
        var node := Art.make_tile(symbol,mask,row,col)
        node.position = Vector3(col+.5,0,row+.5)
        world.add_child(node)
        tiles[index] = node


func refresh() -> void:
    hint.sync_terrain(tiles,state)
    hint.update(camera,vehicles,state,0.0)


func run() -> void:
    seed(712913)
    var next_random := randi()
    seed(712913)
    world = Node3D.new()
    root.add_child(world)
    camera = Camera3D.new()
    camera.projection = Camera3D.PROJECTION_ORTHOGONAL
    world.add_child(camera)
    camera.position = Vector3(12,8,17)
    camera.look_at(Vector3(12,.4,9.5))
    state = {"map":[],"brick_masks":[],"players":[{"id":0,"active":true},{"id":1,"active":true}]}
    for row in 26: state.map.append(".".repeat(26))
    state.brick_masks.resize(26*26)
    state.brick_masks.fill(0)
    for id in 2:
        var vehicle := Art.make_tank(id,false,0,id,0)
        vehicle.position = Vector3(10.5+id*3,0,9.4)
        world.add_child(vehicle)
        vehicles["p%d"%id] = vehicle
    var enemy := Art.make_tank(2,true,0,4,0)
    enemy.position = Vector3(10.5,0,9.4)
    world.add_child(enemy)
    vehicles.e4 = enemy
    hint = Visibility.new(world)
    tile("#",10,10)
    tile("#",13,10)
    refresh()
    expect(hint.stats().players == 2 and hint.stats().active == 2,"only two players acquire a brick hint")
    expect(not hint._players.has(4),"enemy has no overlay")
    var native_copy := state.duplicate(true)
    var bounds: AABB = vehicles.p0.get_meta("visual_bounds")
    var geometry: Mesh = vehicles.p0.get_node("Body").mesh
    var source_material: Material = vehicles.p0.get_node("Body").get_active_material(0)
    var record: Dictionary = hint._players[0]
    expect(record.copies.size() == 5,"four rigid parts plus boat have bounded copies")
    for index in record.copies.size():
        var copy: MeshInstance3D = record.copies[index]
        expect(copy.mesh == record.originals[index].mesh and copy.cast_shadow == GeometryInstance3D.SHADOW_CASTING_SETTING_OFF,
            "ghost shares geometry and casts no shadow")
    expect(hint._players[0].material != hint._players[1].material and
        hint._players[0].material.get_shader_parameter("team_color") != hint._players[1].material.get_shader_parameter("team_color"),
        "each player owns distinct team tint material")
    Art.set_vehicle_state(vehicles.p0,{"moving":true,"boat":true,"yaw":.5},.08)
    vehicles.p0.rotation.y = -.5
    hint.update(camera,vehicles,state,.08)
    record = hint._players[0]
    var pose: Transform3D = record.copies[0].global_transform
    expect(record.copies[0].global_transform == record.originals[0].global_transform and record.copies[4].visible,
        "ghost follows animated turret and visible boat")
    var current_bounds: AABB = vehicles.p0.get_meta("visual_bounds")
    hint.update(camera,vehicles,state,0.0)
    expect(record.copies[0].global_transform == pose and vehicles.p0.get_meta("visual_bounds") == current_bounds,
        "zero dt stays stable and never expands visual_bounds")
    expect(state == native_copy and vehicles.p0.get_node("Body").mesh == geometry and
        vehicles.p0.get_node("Body").get_active_material(0) == source_material,"native state and cached source materials untouched")

    tile("#",10,10,5)
    refresh()
    var mask_image: Image = hint._mask_image
    expect(mask_image.get_width() == 52 and not mask_image.has_mipmaps(),"half-cell mask is nearest/no-mip and bounded")
    expect(mask_image.get_pixel(20,20).r > .9 and mask_image.get_pixel(21,20).r == 0 and
        mask_image.get_pixel(20,21).r > .9 and mask_image.get_pixel(21,21).r == 0,"destroyed brick quadrants are absent from the mask")
    var brick_body: MeshInstance3D = tiles[10*26+10].get_node("Body")
    var brick_bounds := brick_body.global_transform*brick_body.mesh.get_aabb()
    var encoded_height := mask_image.get_pixel(20,20)
    expect(absf(encoded_height.g*4-brick_bounds.position.y) <= 4.0/255.0 and
        absf(encoded_height.b*4-brick_bounds.end.y) <= 4.0/255.0,"occluder height codes match actual terrain bounds")
    tile("#",10,10,0)
    refresh()
    expect(not hint._players[0].root.visible and hint.stats().buildings == 1,"destroyed building immediately removes its hint")
    tile("@",10,10)
    tile("#",11,23)
    refresh()
    expect(hint.stats().buildings == 1 and hint._mask_image.get_pixel(20,20).r == 0 and
        hint._mask_image.get_pixel(22,46).r == 0,"steel and base wall never classified as buildings")
    tile("#",10,10)
    tile(".",11,23)
    tile("%",10,9)
    refresh()
    expect(not hint._players[0].root.visible,"player footprint in forest disables the hint immediately")
    tile(".",10,9)
    tile("%",10,10)
    tile("#",10,11)
    refresh()
    expect(not hint._players[0].root.visible,"foreground forest envelope prevents seeing a player through cover")
    tile("#",10,10)
    tile("%",10,5)
    refresh()
    expect(hint._players[0].root.visible,"forest behind the player does not block a foreground building hint")
    tile(".",10,5)
    tile(".",10,10)
    tile(".",10,11)
    refresh()
    expect(not hint._players[0].root.visible,"clear line of sight hides the overlay")
    tile("#",10,10)
    refresh()
    vehicles.p0.visible = false
    hint.update(camera,vehicles,state,0.0)
    expect(not hint._players[0].root.visible,"spawn blinking/hidden source also hides hint")
    vehicles.p0.visible = true
    state.players[0].creating = .4
    hint.update(camera,vehicles,state,0.0)
    expect(not hint._players[0].root.visible,"creation interval suppresses hint even on visible blink frames")
    state.players[0].creating = 0.0
    var old_root: Node3D = hint._players[0].root
    var replacement := Art.make_tank(0,false,0,0,1)
    replacement.position = vehicles.p0.position
    world.add_child(replacement)
    vehicles.p0.free()
    vehicles.p0 = replacement
    hint.update(camera,vehicles,state,0.0)
    expect(not is_instance_valid(old_root) and hint._players[0].source == replacement,"respawn/model replacement destroys obsolete copies")
    state.players[0].active = false
    hint.update(camera,vehicles,state,0.0)
    expect(hint.stats().players == 1 and not hint._players.has(0),"death releases player copies")
    state.players[0].active = true
    hint.update(camera,vehicles,state,0.0)
    var allocations: int = hint.stats().created
    for frame in 120: hint.update(camera,vehicles,state,0.0)
    expect(hint.stats().created == allocations and hint.stats().players == 2,"steady frames do not allocate more copies")
    var hinted_bounds: AABB = replacement.get_meta("visual_bounds")
    expect(hinted_bounds.size.y > 0 and bounds.size.y > 0 and not replacement.has_node("PlayerVisibility"),"overlay remains outside the art/camera tree")
    camera.projection = Camera3D.PROJECTION_PERSPECTIVE
    hint.update(camera,vehicles,state,0.0)
    expect(hint.stats().players == 0,"unsupported projection fails closed")
    var hint_root: Node3D = hint._root
    hint.dispose()
    expect(not is_instance_valid(hint_root) and hint.stats().players == 0,"explicit disposal frees copies")
    hint.dispose()
    expect(randi() == next_random,"construction and updates never consume gameplay RNG")
    world.free()
    if cases != 30:
        failures.append("incomplete check execution: expected 30, got %d"%cases)
    if not failures.is_empty():
        print("TANKS_PLAYER_VISIBILITY_FAILED "+JSON.stringify(failures))
        quit(1)
        return
    print("TANKS_PLAYER_VISIBILITY_PASSED "+JSON.stringify({"status":"passed","cases":cases,
        "checks":["brick-mask","occluder-height","forest-cover","player-only","rigid-follow","material-isolation",
            "source-unchanged","lifetime","bounded-cache","pause","rng"]}))
    quit()
