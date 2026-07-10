#version 330 core

out vec4 frag_color;

uniform sampler2D u_image;
uniform vec2 u_texel_size;
uniform int u_horizontal;

void main()
{
    vec2 uv = gl_FragCoord.xy * u_texel_size;
    vec2 direction = (u_horizontal == 1) ? vec2(u_texel_size.x, 0.0) : vec2(0.0, u_texel_size.y);

    vec3 color = texture(u_image, uv).rgb * 0.227027;
    color += texture(u_image, uv + direction * 1.384615).rgb * 0.316216;
    color += texture(u_image, uv - direction * 1.384615).rgb * 0.316216;
    color += texture(u_image, uv + direction * 3.230769).rgb * 0.070270;
    color += texture(u_image, uv - direction * 3.230769).rgb * 0.070270;

    frag_color = vec4(color, 1.0);
}
