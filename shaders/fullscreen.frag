#version 330 core

out vec4 frag_color;

uniform float u_time;
uniform vec2 u_resolution;
uniform vec3 u_camera_pos;
uniform vec3 u_camera_forward;
uniform vec3 u_camera_right;
uniform vec3 u_camera_up;

// ray-sphere intersection: returns the distance along the ray to the nearest hit, or -1.0 on a miss
float hit_sphere(vec3 ray_origin, vec3 ray_dir, vec3 sphere_center, float sphere_radius)
{
    vec3 origin_center = sphere_center - ray_origin;
    float a = dot(ray_dir, ray_dir);
    float h = dot(ray_dir, origin_center);
    float c = dot(origin_center, origin_center) - (sphere_radius * sphere_radius);
    float discriminant = h * h - a * c;

    if (discriminant < 0.0) {
        return -1.0;
    }

    return (h - sqrt(discriminant)) / a;
}

void main()
{
    vec2 uv = gl_FragCoord.xy / u_resolution;
    vec2 centered = uv * 2.0 - 1.0;
    centered.x *= u_resolution.x / u_resolution.y;

    vec3 ray_origin = u_camera_pos;
    vec3 ray_dir = normalize(u_camera_forward + centered.x * u_camera_right + centered.y * u_camera_up);
    vec3 sphere_center = vec3(0.0, 0.0, 0.0);
    float sphere_radius = 1.0;

    float pulse = sin(u_time * 2.4) * 0.5 + 0.5;
    pulse = pulse * pulse;

    vec3 hot_color = mix(vec3(0.95, 0.18, 0.035), vec3(1.0, 0.55, 0.10), pulse);
    float emission_strength = 0.12 + 0.34 * pulse;
    float rim_strength = 0.22 + 0.45 * pulse;
    float core_strength = 0.18 + 0.55 * pulse;
    vec3 background_color = vec3(0.01, 0.015, 0.03);

    float t = hit_sphere(ray_origin, ray_dir, sphere_center, sphere_radius);

    if (t > 0.0) {
        vec3 hit_point = ray_origin + t * ray_dir;
        vec3 normal = normalize(hit_point - sphere_center);

        float noise_scale = 13.0;
        float noise_speed = 0.65;

        float bands =
            sin(normal.x * noise_scale + u_time * noise_speed) *
            sin(normal.y * noise_scale * 1.37 - u_time * noise_speed * 0.8) *
            sin(normal.z * noise_scale * 1.91 + u_time * noise_speed * 0.55);

        bands = bands * 0.5 + 0.5;

        float fine_bands =
            sin((normal.x + normal.y) * noise_scale * 1.8 + u_time * noise_speed * 0.6) *
            sin((normal.z - normal.y) * noise_scale * 2.2 - u_time * noise_speed * 0.4);

        fine_bands = fine_bands * 0.5 + 0.5;

        float surface_variation = mix(bands, fine_bands, 0.35);

        vec3 cool_color = vec3(0.16, 0.012, 0.004);
        vec3 warm_color = vec3(0.78, 0.10, 0.018);
        vec3 hot_patch_color = vec3(1.0, 0.38, 0.06);
        vec3 turbulent_color = mix(cool_color, warm_color, surface_variation);
        float hot_mask = smoothstep(0.52, 0.88, surface_variation);
        turbulent_color = mix(turbulent_color, hot_patch_color, hot_mask);

        float rim = 1.0 - max(dot(normal, -ray_dir), 0.0);
        rim = rim * rim;
        float facing = max(dot(normal, -ray_dir), 0.0);
        float core_glow = facing * facing;
        vec3 core_glow_color = hot_color * core_glow * core_strength;
        float light_amount = normal.y * 0.5 + 0.5;
        vec3 surface = turbulent_color * light_amount;
        vec3 emission = hot_color * emission_strength;
        vec3 rim_glow = hot_color * rim * rim_strength;

        vec3 final_color = surface + emission + rim_glow + core_glow_color;
        frag_color = vec4(final_color, 1.0);

        return;
    }

    frag_color = vec4(background_color, 1.0);
}
