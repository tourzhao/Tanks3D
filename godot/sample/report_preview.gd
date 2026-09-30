extends SubViewportContainer

# Report-only display of the existing roster, isolated from the battle camera,
# actors and simulation. Render once when its contents or canvas size change.
const Art = preload("res://art.gd")
var preview: SubViewport
var lineup: Node3D
var roster_key := ""


func _ready() -> void:
    mouse_filter = Control.MOUSE_FILTER_IGNORE
    custom_minimum_size.y = 160
    stretch = true
    preview = SubViewport.new()
    preview.own_world_3d = true
    preview.size = Vector2i(maxi(1, roundi(size.x)), maxi(1, roundi(size.y)))
    preview.render_target_update_mode = SubViewport.UPDATE_DISABLED
    preview.msaa_3d = Viewport.MSAA_2X
    add_child(preview)
    var world := Node3D.new()
    preview.add_child(world)
    var environment := WorldEnvironment.new()
    environment.environment = Environment.new()
    environment.environment.background_mode = Environment.BG_COLOR
    environment.environment.background_color = Color("131b1d")
    environment.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
    environment.environment.ambient_light_color = Color("bcc9ce")
    environment.environment.ambient_light_energy = 0.55
    world.add_child(environment)
    var sunlight := DirectionalLight3D.new()
    sunlight.rotation_degrees = Vector3(-48, -35, 0)
    sunlight.light_color = Color("fff0cf")
    sunlight.light_energy = 1.3
    sunlight.shadow_enabled = true
    world.add_child(sunlight)
    var camera := Camera3D.new()
    camera.projection = Camera3D.PROJECTION_ORTHOGONAL
    camera.size = 2.1
    camera.position = Vector3(4.8, 3.85, 5.8)
    world.add_child(camera)
    camera.look_at(Vector3(0, 0.6, 0))
    camera.current = true
    var ground := MeshInstance3D.new()
    var plane := PlaneMesh.new()
    plane.size = Vector2(8, 4)
    ground.mesh = plane
    ground.position.y = -0.025
    var paint := StandardMaterial3D.new()
    paint.albedo_color = Color("272e2b")
    paint.roughness = 1.0
    ground.material_override = paint
    world.add_child(ground)
    lineup = Node3D.new()
    world.add_child(lineup)
    visibility_changed.connect(refresh_preview)
    resized.connect(refresh_preview)


func set_players(players: Array) -> void:
    var identities: Array = []
    for player in players:
        identities.append([int(player.get("id", 0)), int(player.get("nation", 0)), int(player.get("level", 0))])
    var next_key := JSON.stringify(identities)
    if next_key == roster_key or lineup == null:
        return
    roster_key = next_key
    for model in lineup.get_children():
        lineup.remove_child(model)
        model.queue_free()
    for index in range(identities.size()):
        var identity: Array = identities[index]
        var model := Art.make_tank(identity[1], false, 0, identity[0], identity[2])
        var offset := (float(index) - float(identities.size() - 1) * 0.5) * 2.65
        model.position = Vector3(offset * 0.77, 0, -offset * 0.64)
        model.rotation.y = PI
        lineup.add_child(model)
    refresh_preview()


func refresh_preview() -> void:
    if preview != null:
        preview.render_target_update_mode = SubViewport.UPDATE_ONCE if is_visible_in_tree() else SubViewport.UPDATE_DISABLED
