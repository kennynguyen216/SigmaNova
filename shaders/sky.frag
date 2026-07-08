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

    vec3 background = vec3(0.0015, 0.002, 0.006);
    vec3 color = background;

    vec3 fine_coord = ray_dir * 260.0;
    vec3 fine_cell = floor(fine_coord);
    vec3 fine_local = fract(fine_coord);
    float fine_seed = hash(fine_cell);
    vec2 fine_center = vec2(hash(fine_cell + vec3(2.0, 19.0, 5.0)), hash(fine_cell + vec3(11.0, 7.0, 3.0)));
    float fine_dist = length(fine_local.xy - fine_center);
    float fine_star = step(0.992, fine_seed) * smoothstep(0.050, 0.0, fine_dist);
    float fine_brightness = mix(0.20, 0.85, hash(fine_cell + vec3(4.0, 9.0, 13.0)));
    color += vec3(fine_star * fine_brightness);

    vec3 bright_coord = ray_dir * 90.0;
    vec3 bright_cell = floor(bright_coord);
    vec3 bright_local = fract(bright_coord);
    float bright_seed = hash(bright_cell);
    vec2 bright_center = vec2(hash(bright_cell + vec3(17.0, 3.0, 23.0)), hash(bright_cell + vec3(5.0, 31.0, 9.0)));
    float bright_dist = length(bright_local.xy - bright_center);
    float bright_star = step(0.986, bright_seed) * smoothstep(0.095, 0.0, bright_dist);
    float bright_glint = step(0.997, bright_seed) * smoothstep(0.16, 0.0, bright_dist);
    float temp = hash(bright_cell + vec3(29.0, 1.0, 15.0));
    vec3 star_tint = mix(vec3(0.78, 0.86, 1.0), vec3(1.0, 0.82, 0.48), temp);
    color += star_tint * (bright_star * 0.95 + bright_glint * 1.35);

    frag_color = vec4(color, 1.0);
}
