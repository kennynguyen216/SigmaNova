#version 330 core

out vec4 frag_color;

uniform sampler2D u_scene_texture;
uniform vec2 u_resolution;
uniform float u_threshold;
uniform float u_soft_knee;

void main()
{
    vec2 uv = gl_FragCoord.xy / u_resolution;
    vec3 color = texture(u_scene_texture, uv).rgb;

    float brightness = max(color.r, max(color.g, color.b));
    float bright_mask = smoothstep(u_threshold, u_threshold + u_soft_knee, brightness);

    frag_color = vec4(color * bright_mask, 1.0);
}
