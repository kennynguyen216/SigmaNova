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
            float distance_center = length(sample_point - sphere_center);
            float density = 1.0 - (distance_center / sphere_radius);
            density = max(density, 0.0);
            accumulated_color += vec3(1.0, .25, .05) * density * step_size *2.0;
            t += step_size;
        }

        //vec3 final_color = surface + emission + rim_glow + core_glow_color;
        frag_color = vec4(accumulated_color, 1.0);

        return;
    }

    discard;
}