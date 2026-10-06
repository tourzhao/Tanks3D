extends SceneTree
## Actual-render diagnostic. All cells use the same camera, light, viewport
## dimensions and model scale. Does not change the game's default camera.
## Godot --path ... --script art_review.gd -- --review=roster --output=/.../sheet.png

const Art = preload("res://art.gd")
const SceneLighting = preload("res://scene_lighting.gd")
const CELL := Vector2i(320,272)
const HEADER := 52
const LABEL := 39
var mode := "roster"
var destination := ""
var reviews: Array = []
var camera_position := Vector3(-3.4,3.9,-5.0)
var camera_target := Vector3(0,.47,0)
var elevation := 29.6
var camera_yaw := -145.8
var animation_frames := 0
var neutral_materials := false
var review_view := "oblique"

func _initialize() -> void:
    for arg in OS.get_cmdline_user_args():
        if arg.begins_with("--review="):
            mode = arg.get_slice("=",1)
        elif arg.begins_with("--output="):
            destination = arg.trim_prefix("--output=")
        elif arg.begins_with("--animate-frames="):
            animation_frames = clampi(int(arg.get_slice("=",1)),0,600)
        elif arg == "--neutral-materials":
            neutral_materials = true
        elif arg.begins_with("--view="):
            review_view = arg.get_slice("=",1)
    if review_view == "front":
        camera_position = camera_target+Vector3(0,0,-8)
        elevation = 0.0
        camera_yaw = 180.0
    elif review_view == "side":
        camera_position = camera_target+Vector3(-8,0,0)
        elevation = 0.0
        camera_yaw = -90.0
    elif review_view != "oblique":
        push_error("Unknown review view: "+review_view)
        quit(2)
        return
    call_deferred("make_sheet")

func label(text: String, position: Vector2, width: float, size: int = 14) -> Label:
    var node := Label.new()
    node.position = position
    node.size = Vector2(width,36)
    node.text = text
    node.add_theme_font_size_override("font_size",size)
    node.add_theme_color_override("font_color",Color("e5dfc5"))
    root.add_child(node)
    return node

func apply_neutral_materials(node: Node, material: StandardMaterial3D) -> void:
    # Review-only override: remove paint, vertex tints, textures, emission and
    # mechanical material cues while keeping the actual mesh and its normals.
    # Do not mutate shared production materials or alter the model silhouette.
    if node is MeshInstance3D:
        node.material_override = material
        node.material_overlay = null
    for child in node.get_children():
        apply_neutral_materials(child,material)

func make_sheet() -> void:
    var subjects: Array = []
    if mode in ["effects","effects-game"]:
        camera_target.y += .38
        camera_position.y += .38
    var game_scale := mode in ["roster-game","effects-game","forest-game"]
    if game_scale:
        elevation = 50.0
        camera_yaw = 0.0
        camera_position = camera_target+Vector3(0,sin(deg_to_rad(elevation)),cos(deg_to_rad(elevation)))*8.0
    if mode in ["roster","roster-game","enemies"]:
        for nation in 3:
            for tier in 4:
                var vehicle := Art.make_tank(nation,mode == "enemies",tier,0,tier)
                if game_scale:
                    vehicle.rotation.y = -PI*.5
                Art.set_vehicle_state(vehicle,{"moving":false,"armor":4 if tier == 3 else 1},0.0)
                # Keep one comparison scale, with room for the widest current
                # hull and long guns. This only affects the diagnostic sheet.
                subjects.append({"node":vehicle,"label":"%s %s%d  %s" % [["USA","USSR","GER"][nation],"R" if mode == "enemies" else "L",tier,vehicle.get_meta("vehicle_name")],"span":18.5*float(CELL.y)/720.0 if game_scale else 2.60})
    elif mode == "pickups":
        for type in 9:
            subjects.append({"node":Art.make_pickup(type),"label":["GRENADE","HELMET","CLOCK","SHOVEL","TANK","STAR","GUN","BOAT","BANDAGE"][type],"span":1.42})
    elif mode in ["effects","effects-game"]:
        for age in [.0,.08,.18,.30,.44,.60,.78,.94]:
            subjects.append({"node":Art.make_explosion(),"label":"EXPLOSION","age":age,"effect_size":1.0,"span":18.5*float(CELL.y)/720.0 if game_scale else 2.70})
        for age in [.0,.25,.50,.85]:
            subjects.append({"node":Art.make_explosion(false),"label":"SMALL FLASH","age":age,"effect_size":.23,"span":18.5*float(CELL.y)/720.0 if game_scale else 2.70})
    elif mode in ["forest","forest-game"]:
        for patch in 4:
            var grove := Node3D.new()
            for row in 4:
                for col in 4:
                    var tree := Art.make_tile("%",15,row+patch*5,col+patch*3)
                    tree.position = Vector3(col-1.5,0,row-1.5)
                    grove.add_child(tree)
            var player := Art.make_tank(patch%2,false,0,patch%2,0)
            player.position = Vector3(.13,0,.11)
            player.rotation.y = -PI*.5
            grove.add_child(player)
            subjects.append({"node":grove,"label":"FOREST PATCH %d · P%d UNDER COVER" % [patch,patch%2+1],"span":18.5*float(CELL.y)/720.0 if game_scale else 4.20})
    elif mode == "terrain":
        for kind in 4:
            var lot := Node3D.new()
            for row in 2:
                for col in 2:
                    var node := Art.make_tile("#",15,row,col+kind*2)
                    node.position = Vector3(col-.5,0,row-.5)
                    lot.add_child(node)
            subjects.append({"node":lot,"label":"CONNECTED BUILDING LOT %d" % kind,"span":2.40})
        for mask in [3,7]:
            subjects.append({"node":Art.make_tile("#",mask,2,4),"label":"BRICK MASK %d" % mask,"span":1.80})
        for symbol in ["@","%","~","-"]:
            subjects.append({"node":Art.make_tile(symbol,15,4,6),"label":"TERRAIN %s" % symbol,"span":1.80})
        subjects.append({"node":Art.make_base_wall(2,false),"label":"DAMAGED BASE WALL","span":1.80})
        subjects.append({"node":Art.make_base(0),"label":"USA HEADQUARTERS","span":2.40})
    else:
        push_error("Unknown review mode: "+mode)
        quit(2)
        return
    var rows := ceili(subjects.size()/4.0)
    root.content_scale_mode = Window.CONTENT_SCALE_MODE_DISABLED
    root.content_scale_size = Vector2i.ZERO
    root.size = Vector2i(CELL.x*4,HEADER+rows*(CELL.y+LABEL)+34)
    var background := ColorRect.new()
    background.color = Color("172a2d")
    background.size = Vector2(root.size)
    root.add_child(background)
    label("TANKS 3D · GODOT ART STUDY / "+mode.to_upper(),Vector2(15,6),1250,20)
    label(("NEUTRAL CLAY · one matte gray material" if neutral_materials else "Original procedural geometry")+" · Pixel OFF · "+("GAME PIXEL SCALE" if game_scale else "native capture")+" · identical camera/light per cell",Vector2(15,29),1250,13)
    var clay := StandardMaterial3D.new()
    clay.albedo_color = Color("a2a2a2")
    clay.roughness = 1.0
    clay.metallic = 0.0
    clay.metallic_specular = 0.0
    clay.vertex_color_use_as_albedo = false
    for index in subjects.size():
        var subject: Dictionary = subjects[index]
        var x := (index%4)*CELL.x
        var y := HEADER+floori(index/4.0)*(CELL.y+LABEL)
        var container := SubViewportContainer.new()
        container.position = Vector2(x,y)
        container.size = Vector2(CELL)
        container.stretch = true
        root.add_child(container)
        var view := SubViewport.new()
        view.size = CELL
        view.own_world_3d = true
        view.render_target_update_mode = SubViewport.UPDATE_ALWAYS
        view.msaa_3d = Viewport.MSAA_2X
        container.add_child(view)
        var world := Node3D.new()
        view.add_child(world)
        world.add_child(SceneLighting.make_environment())
        world.add_child(SceneLighting.make_sun())
        var ground := MeshInstance3D.new()
        var plane := PlaneMesh.new()
        plane.size = Vector2(20,20)
        ground.mesh = plane
        var material := StandardMaterial3D.new()
        material.albedo_color = Color("797a60")
        material.roughness = 1.0
        ground.material_override = material
        ground.position.y = -.025
        world.add_child(ground)
        var model: Node3D = subject.node
        world.add_child(model)
        if neutral_materials:
            apply_neutral_materials(model,clay)
        if subject.has("age"):
            pose_effect(model,float(subject.age),float(subject.effect_size))
        var camera := Camera3D.new()
        camera.projection = Camera3D.PROJECTION_ORTHOGONAL
        # Orthographic directional shadows use camera.far, not the sun's
        # shadow distance. Include the ground at the top of game-scale views.
        camera.far = 14.0
        camera.keep_aspect = Camera3D.KEEP_HEIGHT
        camera.size = subject.span
        world.add_child(camera)
        camera.position = camera_position
        camera.look_at(camera_target)
        var caption := label(subject.label,Vector2(x+8,y+CELL.y+1),CELL.x-16,12)
        reviews.append({"camera":camera,"model":model,"caption":caption,"span":subject.span,"label":subject.label,
            "age":subject.get("age",-1.0),"effect_size":subject.get("effect_size",0.0),"initial_age":subject.get("age",-1.0)})
    label("Window %d×%d · viewport 320×272 · orthographic · elevation %.1f°, yaw %.1f° · tank width ×%.2f" % [root.size.x,root.size.y,elevation,camera_yaw,Art.VEHICLE_WIDTH_SCALE],Vector2(15,root.size.y-27),1250,13)
    for i in 4:
        await process_frame
    for frame in animation_frames:
        for review in reviews:
            if float(review.initial_age) < 0.0:
                continue
            var life := .70 if float(review.effect_size) > .5 else .10
            var age := fposmod(float(review.initial_age)+float(frame)/60.0/life,1.0)
            review.age = age
            pose_effect(review.model,age,float(review.effect_size))
            review.caption.text = "%s · %d ms / %d ms" % [review.label,roundi(age*life*1000),roundi(life*1000)]
        await process_frame
        await RenderingServer.frame_post_draw
    await RenderingServer.frame_post_draw
    var metadata: Array = []
    for review in reviews:
        var model: Node3D = review.model
        var pixels := Vector2i.ZERO
        if model.has_meta("visual_bounds"):
            var bounds: AABB = model.get_meta("visual_bounds")
            var lower := Vector2(INF,INF)
            var upper := Vector2(-INF,-INF)
            for corner in 8:
                var point := Vector3(bounds.end.x if corner&1 else bounds.position.x,bounds.end.y if corner&2 else bounds.position.y,bounds.end.z if corner&4 else bounds.position.z)
                var screen: Vector2 = review.camera.unproject_position(model.to_global(point))
                lower = lower.min(screen)
                upper = upper.max(screen)
            pixels = Vector2i((upper-lower).ceil())
        if float(review.age) >= 0.0:
            var life := .70 if float(review.effect_size) > .5 else .10
            review.caption.text = "%s · %d ms / %d ms" % [review.label,roundi(float(review.age)*life*1000),roundi(life*1000)]
        if pixels != Vector2i.ZERO:
            review.caption.text += "\n%d×%d px bounds · ortho %.2f" % [pixels.x,pixels.y,review.span]
        metadata.append({"label":review.label,"span":review.span,"model_scale":[model.scale.x,model.scale.y,model.scale.z],"bounds_pixels":[pixels.x,pixels.y],"age_fraction":review.age,"effect_size":review.effect_size})
    await RenderingServer.frame_post_draw
    await RenderingServer.frame_post_draw
    if not destination.is_empty():
        DirAccess.make_dir_recursive_absolute(destination.get_base_dir())
        var result := root.get_texture().get_image().save_png(destination)
        if result != OK:
            push_error("Cannot save review image: "+destination)
            quit(3)
            return
        var receipt := FileAccess.open(destination.get_basename()+".json",FileAccess.WRITE)
        receipt.store_string(JSON.stringify({"mode":mode,"material_mode":"neutral-clay" if neutral_materials else "production","neutral_materials":neutral_materials,"neutral_albedo":"a2a2a2" if neutral_materials else "","window":[root.size.x,root.size.y],"viewport":[CELL.x,CELL.y],"pixel_style":false,"scale":1.0,"game_pixel_scale":game_scale,"animation_frames":animation_frames,"animation_fps":60,"elevation_degrees":elevation,"yaw_degrees":camera_yaw,"vehicle_yaw_degrees":-90 if game_scale else 0,"camera_position":[camera_position.x,camera_position.y,camera_position.z],"camera_target":[camera_target.x,camera_target.y,camera_target.z],"subjects":metadata},"  "))
        print("ART REVIEW SAVED ",destination)
    quit()


func pose_effect(effect: Node3D, age: float, size: float) -> void:
    Art.set_effect_state(effect,age,size)
    var large := size > .5
    var life := .70 if large else .10
    # Match the presentation adapter's event height and upward drift. These
    # review fixtures do not emit events or alter the running game.
    effect.position.y = (.45 if large else .77)+age*life*.4
    var flame: MeshInstance3D = effect.get_node("Body")
    var smoke: Node3D = effect.get_node_or_null("Smoke")
    var bounds := flame.transform*flame.mesh.get_aabb() if flame.visible else AABB()
    if smoke != null and smoke.visible:
        var smoke_body: MeshInstance3D = smoke.get_node("Body")
        var smoke_bounds := smoke.transform*(smoke_body.transform*smoke_body.mesh.get_aabb())
        bounds = bounds.merge(smoke_bounds) if flame.visible else smoke_bounds
    effect.set_meta("visual_bounds",bounds)
