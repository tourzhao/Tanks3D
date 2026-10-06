extends RefCounted
## Shared daylight for battle and model reviews. No screen-space effects or
## extra lights: the key/fill balance should describe the authored armor planes.

static func make_environment() -> WorldEnvironment:
    var node := WorldEnvironment.new()
    var environment := Environment.new()
    environment.background_mode = Environment.BG_COLOR
    environment.background_color = Color("243c43")
    environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
    environment.ambient_light_color = Color("a1bcc2")
    environment.ambient_light_energy = 0.32
    environment.tonemap_mode = Environment.TONE_MAPPER_FILMIC
    environment.tonemap_exposure = 1.0
    environment.glow_enabled = true
    environment.glow_intensity = 0.25
    if RenderingServer.get_current_rendering_method() == "forward_plus":
        environment.ssao_enabled = true
        environment.ssao_radius = 0.75
        environment.ssao_intensity = 1.7
    node.environment = environment
    return node

static func make_sun() -> DirectionalLight3D:
    var sun := DirectionalLight3D.new()
    sun.rotation_degrees = Vector3(-54, -28, 0)
    sun.light_color = Color("fff2dc")
    sun.light_energy = 1.55
    sun.shadow_enabled = true
    sun.directional_shadow_mode = DirectionalLight3D.SHADOW_ORTHOGONAL
    sun.shadow_bias = 0.04
    sun.shadow_normal_bias = 0.45
    return sun
