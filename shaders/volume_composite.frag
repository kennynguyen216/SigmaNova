#version 330 core

out vec4 frag_color;

uniform sampler2D u_volume_texture;
uniform vec2 u_output_resolution;

void main()
{
    vec2 uv = gl_FragCoord.xy / u_output_resolution;
    frag_color = vec4(texture(u_volume_texture, uv).rgb, 1.0);
}
