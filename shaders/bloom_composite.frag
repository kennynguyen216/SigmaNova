#version 330 core

out vec4 frag_color;

uniform sampler2D u_scene_texture;
uniform sampler2D u_bloom_texture;
uniform vec2 u_resolution;
uniform float u_bloom_strength;
uniform float u_speed_blur_strength;
uniform float u_whiteout_strength;

void main()
{
    vec2 uv = gl_FragCoord.xy / u_resolution;
    vec3 scene_color = texture(u_scene_texture, uv).rgb;
    vec3 bloom_color = texture(u_bloom_texture, uv).rgb;

    vec2 center = vec2(0.5);
    vec2 radial_vector = uv - center;
    vec2 blur_step = radial_vector * 0.075 * u_speed_blur_strength;
    vec3 speed_bloom = bloom_color * 0.30;
    speed_bloom += texture(u_bloom_texture, uv - blur_step * 0.45).rgb * 0.22;
    speed_bloom += texture(u_bloom_texture, uv - blur_step * 0.90).rgb * 0.18;
    speed_bloom += texture(u_bloom_texture, uv - blur_step * 1.35).rgb * 0.14;
    speed_bloom += texture(u_bloom_texture, uv - blur_step * 1.80).rgb * 0.10;
    speed_bloom += texture(u_bloom_texture, uv - blur_step * 2.25).rgb * 0.06;
    bloom_color = mix(bloom_color, speed_bloom, u_speed_blur_strength);

    vec3 color = scene_color + bloom_color * u_bloom_strength;

    // Final screen-space flashbang overlay. Keeping this in the composite pass
    // makes the recovery a true whole-screen fade instead of a bright volume
    // lingering underneath bloom/tone mapping.
    vec3 whiteout_color = vec3(1.0, 0.985, 0.94);
    color = mix(color, whiteout_color, clamp(u_whiteout_strength, 0.0, 1.0));

    frag_color = vec4(clamp(color, 0.0, 1.0), 1.0);
}
