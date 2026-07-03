#version 330 core



out vec4 frag_color;

uniform float u_time;
uniform vec2 u_resolution;
uniform vec3 u_camera_pos;
uniform vec3 u_camera_forward;
uniform vec3 u_camera_right;
uniform vec3 u_camera_up;


// helper function to help with ray sphere initersection 

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

    vec3 background_color = vec3(0.01, 0.015, 0.03);
    float t = hit_sphere(ray_origin, ray_dir, sphere_center, sphere_radius);

    if (t > 0.0) {
        vec3 hit_point = ray_origin + t * ray_dir;
        vec3 normal = normalize(hit_point - sphere_center);
        float light_amount = normal.y * 0.5 + 0.5;
        vec3 sphere_color = vec3(1.0, 0.45, 0.1) * light_amount;

        frag_color = vec4(sphere_color, 1.0);
        return;
    }

    frag_color = vec4(background_color, 1.0);
}
