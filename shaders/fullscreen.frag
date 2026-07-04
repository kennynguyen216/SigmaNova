#version 330 core

out vec4 frag_color;

uniform float u_time;
uniform vec2 u_resolution;
uniform vec3 u_camera_pos;
uniform vec3 u_camera_forward;
uniform vec3 u_camera_right;
uniform vec3 u_camera_up;

// ray-sphere intersection: returns the distance along the ray to the nearest hit, or -1.0 on a miss
// need to update to return both the entry and the exit
vec2 hit_sphere_bounds(vec3 ray_origin, vec3 ray_dir, vec3 sphere_center, float sphere_radius)
{
    vec3 origin_center = sphere_center - ray_origin;
    float a = dot(ray_dir, ray_dir);
    float h = dot(ray_dir, origin_center);
    float c = dot(origin_center, origin_center) - (sphere_radius * sphere_radius);
    float discriminant = h * h - a * c;

    if (discriminant < 0.0) {
        return vec2(-1.0, -1.0); // did not hit
    }
    float root = sqrt(discriminant);
    float t_entry =  (h - root) / a; // entry is the least val
    float t_exit = (h + root) / a; // exit is further

    return vec2(t_entry, t_exit); // x is the entry y is exit
   
}
//make some noise
float gas_noise(vec3 p){
    float noise_scale = 11.0;
    float noise_speed = 0.45;
    float bands =
            sin(p.x * noise_scale + u_time * noise_speed) *
            sin(p.y * noise_scale * 1.37 - u_time * noise_speed * 0.8) *
            sin(p.z * noise_scale * 1.91 + u_time * noise_speed * 0.55);
    return bands * 0.5 + 0.5;
}

//helper function to sample density
float sample_density(vec3 p, vec3 sphere_center, float sphere_radius) {
    float distance_center = length(p - sphere_center);
    float radial = clamp(1.0 - (distance_center / sphere_radius), 0.0, 1.0);
    // sqrt lifts density near the surface so grazing rays still pick up glow;
    // linear falloff left the outer shell nearly empty and it rendered as a black ring
    float radial_density = sqrt(radial);
    float noise = gas_noise(p);
    // noise floor of 0.55 keeps low-noise pockets as dim gas instead of black voids
    float turbulent_density = radial_density * mix(0.55, 1.45, noise);
    return clamp(turbulent_density, 0.0, 1.0);
}

//how close does the ray pass to the center
float ray_sphere_near_distance(vec3 ray_origin, vec3 ray_dir, vec3 sphere_center){
    vec3 origin_center = sphere_center - ray_origin; // vector from camera to sphere_center
    float closest_t = dot(origin_center, ray_dir);
    vec3 closest_point = ray_origin + closest_t * ray_dir;
    return length(closest_point - sphere_center);
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
    vec2 bounds = hit_sphere_bounds(ray_origin, ray_dir, sphere_center, sphere_radius);

    if (bounds.y > 0.0) {
        float t = max(bounds.x, 0.0); // start marching at entry point unless behind camera
        float t_end = bounds.y;
        float step_size = 0.03;
        vec3 accumulated_color = vec3(0.0);

        while (t < t_end) {
            vec3 sample_point = ray_origin + t * ray_dir;
            float density  = sample_density(sample_point, sphere_center, sphere_radius);
            // different temps = different desn 
            vec3 cool_gas = vec3(0.45, 0.02, 0.00);
            vec3 warm_gas = vec3(0.95, 0.35, 0.00);
            vec3 hot_gas = vec3(1.00, 0.92, 0.75);
            vec3 gas_color = mix(cool_gas, warm_gas, density);
            float hot_mask = smoothstep(0.55, 1.0, density);
            gas_color = mix(gas_color, hot_gas, hot_mask);

            accumulated_color += gas_color * density * step_size * 2.0;
            t += step_size;
        }
        // inner rim: ramps up toward the silhouette with the same color and strength the
        // outer corona has there, so the glow is continuous across the sphere edge
        float near_distance = ray_sphere_near_distance(ray_origin, ray_dir, sphere_center);
        float inner_rim = smoothstep(sphere_radius * 0.55, sphere_radius, near_distance);
        inner_rim = inner_rim * inner_rim;
        accumulated_color += vec3(1.0, 0.22, 0.04) * inner_rim * 0.45;

        frag_color = vec4(accumulated_color, 1.0);

        return;
    }

    // outer corona: starts at full strength on the silhouette (matching the inner rim)
    // and fades out by 1.9 radii
    float near_distance = ray_sphere_near_distance(ray_origin, ray_dir, sphere_center);
    float corona = 1.0 - smoothstep(sphere_radius, sphere_radius * 1.9, near_distance);
    corona = corona * corona;
    if (corona > 0.001) {
        vec3 corona_color = vec3(1.0, 0.22, 0.04) * corona * 0.45;
        frag_color = vec4(corona_color, 1.0);
        return;
    }

    discard;
}