shader_type canvas_item;
render_mode unshaded;
uniform bool pixel_style = false;
uniform vec2 scene_size = vec2(1280.0, 720.0);

void fragment() {
    vec4 color = texture(TEXTURE, UV);
    if (pixel_style) {
        // Keep quiet color fields on the 2x2 grid, but retain a source texel
        // when the block sample would erase a contrasting barrel/track edge.
        // Choose real texels rather than blending them into a soft outline.
        ivec2 size = textureSize(TEXTURE, 0);
        ivec2 native_texel = clamp(ivec2(floor(UV * vec2(size))), ivec2(0), size - ivec2(1));
        ivec2 block_texel = min((native_texel / 2) * 2 + ivec2(1), size - ivec2(1));
        vec4 native_color = texelFetch(TEXTURE, native_texel, 0);
        vec4 block_color = texelFetch(TEXTURE, block_texel, 0);
        vec3 difference = abs(native_color.rgb - block_color.rgb);
        float contrast = max(max(difference.r, difference.g), difference.b);
        color = contrast > 0.085 ? native_color : block_color;
    }
    if (pixel_style) {
        color.rgb = mix(color.rgb, floor(color.rgb * 31.0 + 0.5) / 31.0, 0.55);
    }
    COLOR = color;
}
