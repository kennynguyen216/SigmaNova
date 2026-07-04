#version 330 core

out vec4 frag_color;

uniform float u_time;
uniform float u_fov_y;
uniform vec2 u_resolution;
uniform vec3 u_camera_pos;
uniform vec3 u_camera_forward;
uniform vec3 u_camera_right;
uniform vec3 u_camera_up;

// Returns the ray's entry and exit distances through a sphere, or (-1, -1) on a miss.
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
// Takes a 3D coordinate and turns it into a pseudo-random number.
// Integer-free multiplicative hash: faster than the sin() version and
// free of the precision artifacts sin-hashes show on some GPUs.
float hash(vec3 p)
{
    p = fract(p * 0.3183099 + vec3(0.1, 0.2, 0.3));
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}
// Smooths random lattice values into continuous value noise.
float value_noise(vec3 p)
{
    // Which grid cube am I in?
    vec3 cell = floor(p);
    // Where am I inside that cube?
    vec3 local = fract(p);
    // Makes transitions less harsh.
    local = local * local * (3.0 - 2.0 * local);
    // A cube has 8 corners.
    float c000 = hash(cell + vec3(0.0, 0.0, 0.0)); // corner at x0, y0, z0
    float c100 = hash(cell + vec3(1.0, 0.0, 0.0)); // etc
    float c010 = hash(cell + vec3(0.0, 1.0, 0.0));
    float c110 = hash(cell + vec3(1.0, 1.0, 0.0));
    float c001 = hash(cell + vec3(0.0, 0.0, 1.0));
    float c101 = hash(cell + vec3(1.0, 0.0, 1.0));
    float c011 = hash(cell + vec3(0.0, 1.0, 1.0));
    float c111 = hash(cell + vec3(1.0, 1.0, 1.0));
    // Blend along x, then y, then z.
    float x00 = mix(c000, c100, local.x);
    float x10 = mix(c010, c110, local.x);
    float x01 = mix(c001, c101, local.x);
    float x11 = mix(c011, c111, local.x);
    float y0 = mix(x00, x10, local.y);
    float y1 = mix(x01, x11, local.y);
    // One smooth random value for point p.
    return mix(y0, y1, local.z);
}
// Scrolls the sampling point over time so the gas churns.
// Roughly half the original scroll speed: fast motion reads as smoke,
// near-frozen motion reads as a still image.
vec3 animate_point(vec3 p)
{
    return p + vec3(sin(u_time * 0.18) * 0.22, u_time * 0.16, -u_time * 0.11);
}

// Layers value noise octaves, normalized so any octave count covers ~[0, 1].
float fbm(vec3 p, int octaves)
{
    float noise = 0.0;
    float amplitude = 0.5;
    float frequency = 2.2;
    float total_amplitude = 0.0;

    for (int i = 0; i < octaves; ++i) {
        noise += value_noise(p * frequency) * amplitude;
        total_amplitude += amplitude;
        frequency *= 2.0;
        amplitude *= 0.5;
    }

    return noise / total_amplitude;
}

// Full-quality gas: domain warp + 4 octaves. Only the main convection cells
// need this; the warp is what gives the gas its curling, fluid look.
float gas_noise(vec3 p)
{
    vec3 animated_p = animate_point(p);
    vec3 warp = vec3(
        value_noise(animated_p * 2.1 + vec3(5.2, 1.3, 0.7)),
        value_noise(animated_p * 2.1 + vec3(8.4, 2.8, 4.1)),
        value_noise(animated_p * 2.1 + vec3(1.9, 7.3, 6.6))) - vec3(0.5);
    animated_p += warp * 0.55;

    return clamp(fbm(animated_p, 4) * 1.11, 0.0, 1.0);
}

// Cheap gas: no domain warp, caller picks the octave count. Used for the
// secondary layers (fine detail, surface warp) where the warp is invisible.
float gas_noise_cheap(vec3 p, int octaves)
{
    return clamp(fbm(animate_point(p), octaves) * 1.11, 0.0, 1.0);
}

// Samples the current procedural density field. Later this can be swapped for texture-backed simulation data.
float sample_density(vec3 p, vec3 sphere_center, float sphere_radius) {
    float distance_center = length(p - sphere_center);
    // The warped surface can never exceed 1.35 * R (0.9 + 0.45 * max noise), so
    // beyond that no gas can exist -- skip all noise work.
    if (distance_center >= sphere_radius * 1.35) {
        return 0.0;
    }
    // Low-frequency noise offsets the effective surface radius per direction, so the
    // silhouette is lumpy instead of a perfect circle and gas can push past the
    // nominal radius. This is what dissolves the "drawn ring" edge into wispy gas.
    float surface_noise = gas_noise_cheap(p * 1.3 + vec3(17.0, 3.0, 11.0), 2);
    float effective_radius = sphere_radius * (0.9 + 0.45 * surface_noise);
    if (distance_center >= effective_radius) {
        return 0.0; // outside the warped surface; skip the expensive layers
    }
    float radial = 1.0 - (distance_center / effective_radius);
    // sqrt lifts density near the surface so grazing rays still pick up glow;
    // linear falloff left the outer shell nearly empty and it rendered as a black ring
    float radial_density = sqrt(radial);
    // Two scales of convection: a few giant cells across the volume
    // with fine granulation layered on top.
    float cells = gas_noise(p * 0.55);
    float detail = gas_noise_cheap(p * 2.6 + vec3(9.2, 4.7, 1.3), 3);
    float noise = cells * 0.62 + detail * 0.38;
    // Contrast curve carves the noise into distinct bright pockets and dark lanes
    // instead of one smooth gradient, so the core reads as turbulent plasma.
    noise = pow(noise, 1.5);
    // The wider noise range deepens the dark lanes between hot clumps.
    float turbulent_density = radial_density * mix(0.12, 1.95, noise);
    return clamp(turbulent_density, 0.0, 1.0);
}

// How close does the ray pass to the center?
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
    centered *= tan(u_fov_y * 0.5);

    vec3 ray_origin = u_camera_pos;
    vec3 ray_dir = normalize(u_camera_forward + centered.x * u_camera_right + centered.y * u_camera_up);
    vec3 sphere_center = vec3(0.0, 0.0, 0.0);
    float sphere_radius = 1.0;   // nominal radius the density field fades out around
    float march_radius = 1.35;   // matches the density field's maximum warped radius
    vec2 bounds = hit_sphere_bounds(ray_origin, ray_dir, sphere_center, march_radius);

    // Wide soft halo, keyed loosely to the nominal radius so there is no hard ring
    // at exactly R. This is the only "atmosphere" term left now that the rim/collar
    // are gone; the volume itself supplies the disk edge.
    float near_distance = ray_sphere_near_distance(ray_origin, ray_dir, sphere_center);
    float halo = 1.0 - smoothstep(sphere_radius * 0.85, sphere_radius * 2.1, near_distance);
    halo = pow(halo, 2.0);
    vec3 halo_color = vec3(0.9, 0.16, 0.03) * halo * 0.5;

    vec3 accumulated_color = vec3(0.0);
    float transmittance = 1.0;

    if (bounds.y > 0.0) {
        float t = max(bounds.x, 0.0); // start marching at entry point unless behind camera
        float t_end = bounds.y;
        // Higher absorption lets front clumps shadow the interior, so the core
        // shows turbulent depth instead of one evenly-lit patch.
        float absorption_strength = 0.7;
        float emission_strength = 6.0;

        while (t < t_end) {
            vec3 sample_point = ray_origin + t * ray_dir;
            // Fine steps only matter in the dense core; the thin outer shell
            // can stride coarser without visible banding.
            float core_distance = length(sample_point - sphere_center);
            float step_size = mix(0.03, 0.075, smoothstep(0.7, 1.35, core_distance));
            float density  = sample_density(sample_point, sphere_center, sphere_radius);
            // Higher density maps to hotter gas colors without washing the core
            // into a flat white blob.
            vec3 cool_gas = vec3(0.45, 0.02, 0.00);
            vec3 warm_gas = vec3(0.95, 0.35, 0.00);
            vec3 hot_gas = vec3(1.00, 0.82, 0.32);
            vec3 gas_color = mix(cool_gas, warm_gas, density);
            // High threshold keeps the hot core small and lets turbulence survive.
            float hot_mask = smoothstep(0.83, 1.0, density);
            gas_color = mix(gas_color, hot_gas, hot_mask);
            vec3 emission = gas_color * density * emission_strength;
            float absorption =  density * absorption_strength;
            accumulated_color += transmittance * emission * step_size;
            transmittance *= exp(-absorption * step_size);
            if (transmittance < 0.01) {
                break;
            }
            t += step_size;
        }
    }

    // The halo fills in behind the gas: where the star is opaque it is occluded
    // (transmittance ~ 0), and at the wispy edge it shows through and blends, so the
    // silhouette dissolves into glow instead of ending at a drawn ring.
    accumulated_color += halo_color * transmittance;

    float luminance = max(accumulated_color.r, max(accumulated_color.g, accumulated_color.b));
    if (luminance < 0.002) {
        discard; // nothing here — let the grid and background show through
    }

    accumulated_color = accumulated_color / (accumulated_color + vec3(1.0));
    frag_color = vec4(accumulated_color, 1.0);
}
