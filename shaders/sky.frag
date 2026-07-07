#version 330 core

out vec4 frag_color;

uniform float u_fov_y;
uniform vec2 u_resolution;
uniform vec3 u_camera_forward;
uniform vec3 u_camera_right;
uniform vec3 u_camera_up;

float hash(vec3 p)
{
    p = fract(p * 0.3183099 + vec3(0.1, 0.2, 0.3));
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

void main()
{
    vec2 uv = gl_FragCoord.xy / u_resolution;
    vec2 centered = uv * 2.0 - 1.0;
    centered.x *= u_resolution.x / u_resolution.y;
    centered *= tan(u_fov_y * 0.5);

    vec3 ray_dir = normalize(u_camera_forward + centered.x * u_camera_right + centered.y * u_camera_up);

    vec3 star_coord = ray_dir * 140.0;
    vec3 cell = floor(star_coord);
    vec3 local = fract(star_coord);

    float star_seed = hash(cell);
    float star_center_seed = hash(cell + vec3(11.0, 7.0, 3.0));
    vec2 star_center = vec2(hash(cell + vec3(2.0, 19.0, 5.0)), star_center_seed);
    float distance_to_star = length(local.xy - star_center);

    float rare_star = step(0.989, star_seed);
    float star = rare_star * smoothstep(0.080, 0.0, distance_to_star);
    float brightness = mix(0.35, 1.0, hash(cell + vec3(4.0, 9.0, 13.0)));

    vec3 background = vec3(0.0015, 0.002, 0.006);
    vec3 color = background + vec3(star * brightness);
    frag_color = vec4(color, 1.0);
}
