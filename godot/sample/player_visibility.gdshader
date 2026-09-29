// Original project shader, PolyForm Noncommercial 1.0.0.
// View-space reconstruction follows Godot's documented reverse-Z path:
// https://docs.godotengine.org/en/stable/tutorials/shaders/advanced_postprocessing.html
shader_type spatial;
render_mode unshaded, fog_disabled, blend_mix, depth_draw_never, depth_test_disabled, cull_back;

uniform sampler2D depth_texture : hint_depth_texture, repeat_disable, filter_nearest;
uniform sampler2D building_mask : repeat_disable, filter_nearest;
uniform vec4 team_color : source_color = vec4(0.937,0.776,0.396,1.0);

void fragment() {
    float depth = textureLod(depth_texture,SCREEN_UV,0.0).r;
    #if CURRENT_RENDERER == RENDERER_COMPATIBILITY
    vec3 ndc = vec3(SCREEN_UV,depth)*2.0-1.0;
    #else
    vec3 ndc = vec3(SCREEN_UV*2.0-1.0,depth);
    #endif
    vec4 scene_view = INV_PROJECTION_MATRIX*vec4(ndc,1.0);
    scene_view.xyz /= scene_view.w;
    // Existing visible fragments, including the exposed roof/gun, stay intact.
    if (-scene_view.z+0.045 >= -VERTEX.z) discard;
    vec3 scene_world = (INV_VIEW_MATRIX*vec4(scene_view.xyz,1.0)).xyz;
    if (scene_world.x < 0.0 || scene_world.z < 0.0 || scene_world.x >= 26.0 || scene_world.z >= 26.0) discard;
    vec4 brick = textureLod(building_mask,scene_world.xz/26.0,0.0);
    if (brick.r < 0.5 || scene_world.y < brick.g*4.0-0.025 || scene_world.y > brick.b*4.0+0.025) discard;
    float edge = smoothstep(0.55,0.94,1.0-abs(normalize(NORMAL).z));
    // Cool shaded support separates the cue from warm brick roofs. A narrow,
    // lighter identity rim avoids filling the hidden shape with roof-like gold.
    vec3 edge_color = mix(team_color.rgb,vec3(1.0),0.18);
    ALBEDO = mix(vec3(0.014,0.028,0.038),edge_color,edge);
    ALPHA = mix(0.17,0.34,edge);
}
