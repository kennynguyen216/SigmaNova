#version 330 core

out vec4 frag_color;

uniform float u_time;
uniform float u_fov_y;
uniform vec2 u_resolution;
uniform vec3 u_camera_pos;
uniform vec3 u_camera_forward;
uniform vec3 u_camera_right;
uniform vec3 u_camera_up;
uniform float u_noise_speed;
uniform float u_noise_scale;
uniform float u_emission_strength;
uniform float u_absorption_strength;
uniform float u_edge_raggedness;
uniform float u_halo_strength;
uniform float u_event_active;
uniform float u_event_time;

// ---------------------------------------------------------------------------
// Supernova event timeline.
// main.cpp only supplies a clock (u_event_active, u_event_time in seconds);
// all choreography lives here. Each phase exposes a 0..1 mask and downstream
// code mixes on the masks -- nothing else reads u_event_time directly, so
// retiming the sequence means editing only these constants.
const float SWELL_DURATION = 2.0;
const float COLLAPSE_START = SWELL_DURATION;
const float COLLAPSE_DURATION = 0.8;
const float COLLAPSE_END = COLLAPSE_START + COLLAPSE_DURATION;
const float FLASH_START = COLLAPSE_END;
const float FLASH_DURATION = 0.45;   // the burst expands to full size this fast
const float FLASH_END = FLASH_START + FLASH_DURATION;
// World-space glare front choreography. The light physically travels: slow
// creep, hard acceleration, slams through the camera (whiteout), holds a
// blind beat, then recedes back onto the remnant to reveal it.
// NOTE: GLARE_HIT must stay in sync with event_glare_hit_delay in main.cpp
// (the camera shake fires when the front hits).
const float GLARE_CREEP_END = FLASH_START + 0.55; // slow-burn growth ends
const float GLARE_HIT = FLASH_START + 0.95;       // front reaches the camera
const float RECEDE_START = GLARE_HIT + 0.35;      // blind beat, then pullback
const float RECEDE_END = RECEDE_START + 1.35;     // glare fully returned home
const float MIST_END = RECEDE_END; // remnant is settled right as it is revealed

struct EventState {
    float swell;        // 0 at rest -> 1 fully swollen, unstable pre-collapse star
    float collapse;     // 0 -> 1 as the star implodes and the core superheats
    float burst;        // 0 -> 1 high-velocity radius expansion of the detonation
    float flash;        // pulse: snaps to 1 at detonation, decays as the mist takes over
    float mist;         // 0 -> 1 as the glare fades into colourful gaseous ejecta
    float glare_grow;   // 0 -> 1 light front expanding outward toward the camera
    float glare_recede; // 0 -> 1 front pulling back from the camera onto the remnant
    float afterglow;    // remnant heat left by the blast: peaks at the hit, cools to 0
};

EventState evaluate_event()
{
    EventState s;
    s.swell = u_event_active * smoothstep(0.0, SWELL_DURATION, u_event_time);
    float infall = u_event_active * smoothstep(COLLAPSE_START, COLLAPSE_END, u_event_time);
    // Squaring makes the infall start gently and accelerate, the way
    // gravitational collapse should feel -- slow give, then a plunge.
    s.collapse = infall * infall;
    // The flash snaps on almost instantly, rides through the burst expansion,
    // then hands off: mist rises on exactly the curve the flash decays on.
    float detonation = u_event_active * smoothstep(FLASH_START, FLASH_START + 0.08, u_event_time);
    float settle = u_event_active * smoothstep(FLASH_END, MIST_END, u_event_time);
    s.burst = u_event_active * smoothstep(FLASH_START, FLASH_END, u_event_time);
    // The flash is only the detonation punch on the gas volume: a short bright
    // stab that fades fast. The sustained screen whiteout is owned by the
    // world-space glare front below, so this must not linger or the two merge
    // into one long white smear.
    float flash_fade = u_event_active * smoothstep(FLASH_START + 0.05, FLASH_START + 0.4, u_event_time);
    s.flash = detonation * (1.0 - flash_fade);
    s.mist = settle;
    // Glare front: cubed so it creeps outward slowly, then accelerates into the
    // camera -- the "it's coming... it's HERE" beat. Recede is a plain ease back.
    s.glare_grow = pow(u_event_active * smoothstep(FLASH_START, GLARE_HIT, u_event_time), 3.0);
    s.glare_recede = u_event_active * smoothstep(RECEDE_START, RECEDE_END, u_event_time);
    // Afterglow rides up with the hit, survives the glare pullback, then cools
    // after the remnant is revealed. This keeps the remnant incandescent when
    // the white front collapses back onto the singularity.
    float heat_on = u_event_active * smoothstep(FLASH_START + 0.3, GLARE_HIT, u_event_time);
    float remnant_cooling = smoothstep(RECEDE_END, RECEDE_END + 2.2, u_event_time);
    s.afterglow = heat_on * (1.0 - remnant_cooling);
    return s;
}

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
    return p + vec3(
        sin(u_time * 0.18 * u_noise_speed) * 0.22,
        u_time * 0.16 * u_noise_speed,
        -u_time * 0.11 * u_noise_speed);
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
    vec3 animated_p = animate_point(p * u_noise_scale);
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
    return clamp(fbm(animate_point(p * u_noise_scale), octaves) * 1.11, 0.0, 1.0);
}

// Samples the current procedural density field. Later this can be swapped for texture-backed simulation data.
// Raggedness is a parameter (not the raw uniform) so event phases can agitate
// the edge without this function knowing about the timeline.
float sample_density(vec3 p, vec3 sphere_center, float sphere_radius, float raggedness) {
    float distance_center = length(p - sphere_center);
    float max_effective_radius = sphere_radius * (0.9 + raggedness);

    // The warped surface can never exceed max_effective_radius, so
    // beyond that no gas can exist -- skip all noise work.
    if (distance_center >= max_effective_radius) {
        return 0.0;
    }
    // Low-frequency noise offsets the effective surface radius per direction, so the
    // silhouette is lumpy instead of a perfect circle and gas can push past the
    // nominal radius. This is what dissolves the "drawn ring" edge into wispy gas.
    float surface_noise = gas_noise_cheap(p * 1.3 + vec3(17.0, 3.0, 11.0), 2);
    float effective_radius = sphere_radius * (0.9 + raggedness * surface_noise);
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

float stellar_surface_heat(vec3 p, vec3 sphere_center, float sphere_radius)
{
    vec3 offset = p - sphere_center;
    float distance_center = length(offset);
    vec3 dir = normalize(offset + vec3(0.0001));

    float normalized_radius = distance_center / max(sphere_radius, 0.001);
    float photosphere_band = smoothstep(0.42, 0.92, normalized_radius)
                            * (1.0 - smoothstep(1.02, 1.18, normalized_radius));

    // Large red-supergiant convection cells plus smaller photosphere granules.
    // Blend direction-space with object-space noise so the pattern wraps around
    // the star without forming spokes that all point back to the center.
    vec3 object_p = p - sphere_center;
    float cells = gas_noise_cheap(object_p * 1.55 + dir * 0.7 + vec3(12.0, 4.0, 21.0), 3);
    float granules = gas_noise_cheap(object_p * 5.0 + dir * 1.8 + vec3(3.0, 19.0, 8.0), 2);
    float active_regions = pow(cells * 0.72 + granules * 0.28, 2.6);

    return active_regions * photosphere_band;
}

// Returns the ejecta density and, via out_shell_depth, the sample's normalized
// position across the (direction-warped) shell. Both are derived from the same
// lobe noise, so depth is a free by-product -- the old ejecta_shell_depth()
// recomputed that noise for every sample.
float sample_ejecta_density(vec3 p, vec3 sphere_center, float shell_radius, float shell_thickness, float shell_strength, out float out_shell_depth)
{
    out_shell_depth = 0.0;
    if (shell_strength <= 0.0) {
        return 0.0;
    }

    vec3 offset = p - sphere_center;
    float distance_center = length(offset);
    vec3 dir = normalize(offset + vec3(0.0001));

    // Supernova remnants are not clean spheres. Warp the shell radius by
    // direction so the boundary grows in lobes and wisps instead of one circle.
    float lobe_noise = gas_noise_cheap(dir * 2.4 + vec3(41.0, 13.0, 7.0), 3);
    float fine_lobes = gas_noise_cheap(dir * 6.2 + vec3(4.0, 29.0, 17.0), 2);
    float directional_push = mix(lobe_noise, fine_lobes, 0.35);
    directional_push = directional_push * directional_push;
    float warped_radius = shell_radius * mix(0.74, 1.28, directional_push);
    float warped_thickness = shell_thickness * mix(0.75, 1.45, fine_lobes);

    out_shell_depth = clamp((distance_center - (warped_radius - warped_thickness)) / (warped_thickness * 2.0), 0.0, 1.0);

    float shell_distance = (distance_center - warped_radius) / warped_thickness;

    if (abs(shell_distance) > 3.0) {
        return 0.0;
    }

    float shell = exp(-shell_distance * shell_distance);

    // Domain-warp the sampling frame so filaments curl and flow like gauzy smoke
    // instead of hard radial ticks. This warp is the single biggest lever between
    // "spray of spikes" and the soft, wispy NASA-remnant look.
    vec3 warp = vec3(
        gas_noise_cheap(p * 1.05 + vec3(3.0, 8.0, 1.0), 2),
        gas_noise_cheap(p * 1.05 + vec3(9.0, 2.0, 6.0), 2),
        gas_noise_cheap(p * 1.05 + vec3(4.0, 7.0, 3.0), 2)) - 0.5;
    vec3 wdir = normalize(dir + warp * 0.8);

    // Radial filaments in the warped frame: wdir is (mostly) constant along a
    // radius so this stays a stretched-outward strand, but the warp bends it into
    // a flowing wisp. Softer ridge power keeps strands gauzy, not spiky; a slow
    // radial term breaks them into segments along their length.
    float ang = gas_noise_cheap(wdir * 4.6 + vec3(31.0, 7.0, 19.0), 3);
    float finger_ridge = 1.0 - abs(2.0 * ang - 1.0);
    float radial_break = gas_noise_cheap((p + warp * 0.4) * 1.6 + vec3(3.0, 41.0, 12.0), 2);
    float filaments = pow(finger_ridge, 2.4) * (0.5 + 0.5 * radial_break);
    // A dim luminous haze fills the shell so the body glows, bright wispy strands
    // ride on top, and a separate low-frequency field punches the dark mottled
    // voids of a real remnant -- rather than uniform black between every strand.
    // This haze-plus-holes is what makes it read as glowing gas, not matte spikes.
    float strands = smoothstep(0.10, 0.52, filaments);
    float holes = smoothstep(0.34, 0.64, gas_noise_cheap(p * 0.9 + vec3(50.0, 20.0, 5.0), 2));
    float body = max(strands, 0.14) * (1.0 - holes * 0.92);
    return clamp(shell * body * 3.0 * shell_strength, 0.0, 1.0);
}

// cooling (0..1) ages the remnant: an early hot nebula recombines from warm
// H-alpha through a magenta/purple mid phase into a cool, faint blue late phase.
vec3 ejecta_color(vec3 p, vec3 sphere_center, float shell_depth, float filament_density, float cooling)
{
    vec3 dir = normalize(p - sphere_center + vec3(0.0001));
    // Colour hierarchy, not a rainbow: one warm-dominant body + one cool accent.
    // Warm H-alpha fills the interior (salmon-orange heart deepening to red);
    // cool O-III is confined to the ionised outer shell, the single cyan accent
    // that wraps the silhouette.
    vec3 warm = mix(vec3(1.00, 0.52, 0.24), vec3(0.85, 0.20, 0.10), smoothstep(0.10, 0.70, shell_depth));
    // Age the body: warm -> purple -> deep blue as the gas cools and recombines.
    vec3 aged = mix(vec3(0.52, 0.10, 0.66), vec3(0.10, 0.16, 0.72), cooling);
    vec3 body = mix(warm, aged, cooling * 0.85);

    // The rim ionisation, broken up by direction-space noise so it stays ragged
    // instead of a painted ring. Its band thins with age (the shock front cools
    // to a narrower ionised rind).
    float rim_breakup = smoothstep(0.32, 0.86, gas_noise_cheap(dir * 3.0 + vec3(6.0, 23.0, 14.0), 3));
    float rim_lo = mix(0.58, 0.80, cooling);
    float rim_band = smoothstep(rim_lo, 0.98, shell_depth) * rim_breakup;
    vec3 base = mix(body, vec3(0.10, 0.78, 1.00), rim_band);

    // Magenta secondary peaks through the mid phase -- the beauty stage where the
    // channels separate most strongly -- and fades again as the body goes blue.
    float mid_phase = cooling * (1.0 - cooling) * 4.0;
    float magenta_patch = smoothstep(0.60, 0.90, gas_noise_cheap(p * 0.72 + vec3(19.0, 3.0, 27.0), 2))
                        * smoothstep(0.18, 0.55, shell_depth) * (1.0 - rim_band);
    base = mix(base, vec3(0.95, 0.12, 0.42), magenta_patch * (0.28 + 0.4 * mid_phase));

    // A few hottest strand cores keep glowing orange even as the body cools, so
    // an aged remnant is dark blue gas threaded with surviving warm filaments.
    float hot_core = smoothstep(0.82, 1.0, filament_density);
    base = mix(base, vec3(1.00, 0.46, 0.12), hot_core * (0.35 + 0.35 * (1.0 - cooling)));

    // Strand cores carry the light; gaps fall toward black so they read as empty
    // space, not dim fog.
    float core = smoothstep(0.45, 0.95, filament_density);
    return base * (0.05 + core * 1.6);
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
    // Phase masks -> physical knobs. Everything below consumes these knobs;
    // no other code needs to know which phase produced them.
    EventState event = evaluate_event();
    vec3 sphere_center = vec3(0.0, 0.0, 0.0);
    // Pre-flash instability: the star itself jitters before it blows. This is
    // separate from the camera shake, which happens later when the blast hits us.
    float instability = u_event_active * smoothstep(0.35, FLASH_START, u_event_time)
                      * (1.0 - smoothstep(FLASH_START - 0.08, FLASH_START + 0.08, u_event_time));
    float tremor_strength = 0.018 * instability * (1.0 + event.collapse * 2.2);
    sphere_center += u_camera_right * sin(u_time * 37.0 + sin(u_time * 11.0)) * tremor_strength;
    sphere_center += u_camera_up * sin(u_time * 49.0 + 1.7) * tremor_strength;
    // Swell: the star inflates, runs hotter, and its edge destabilizes.
    float radius_scale = mix(1.0, 1.45, event.swell);
    float brightness = mix(1.0, 1.7, event.swell);
    float raggedness = u_edge_raggedness * mix(1.0, 1.8, event.swell);
    // Collapse: infall crushes the swollen star into a tiny, brilliant,
    // near-spherical proto-singularity -- small, bright, and white-hot, the
    // loaded spring the accretion disk will spew from. Each knob mixes from
    // its swollen value so the phases chain.
    radius_scale = mix(radius_scale, 0.22, event.collapse);
    brightness = mix(brightness, 1.15, event.collapse);
    raggedness = mix(raggedness, u_edge_raggedness * 0.35, event.collapse);
    // Detonation: the flash pulse stabs the core white-hot, but the star
    // volume does NOT re-inflate afterwards -- the explosion is carried by
    // the glare front and then the ejecta shell. What survives here is the
    // crushed singularity core, which dims as the remnant takes over. (It
    // used to re-expand into a purple-recoloured mist ball that fought the
    // ejecta shell for the same space.)
    brightness = mix(brightness, 3.4, event.flash);
    float star_fade = 1.0 - 0.85 * event.mist; // singularity ember dims, never fully dies
    float core_glow = event.collapse * (1.0 - 0.8 * event.mist); // white core dims once spent
    float flash_white = event.flash;
    float mist = event.mist;
    float afterglow = event.afterglow;
    float absorption_scale = mix(1.0, 0.55, mist); // mist is translucent
    // Quick fade-in: the shell must become visible while it is still small,
    // otherwise it materialises already mid-sized and reads as a glitch-pop.
    // The slow part of the entrance is the radius growth, not the opacity.
    float remnant_reveal = smoothstep(RECEDE_END, RECEDE_END + 0.45, u_event_time);
    remnant_reveal *= remnant_reveal;
    float ejecta_age = max(u_event_time - RECEDE_END, 0.0);
    float ejecta_strength = remnant_reveal;
    // Cooling clock for the remnant's evolution: 0 = fresh hot nebula,
    // 1 = old faint blue shell. Drives the palette and luminosity decay so the
    // remnant ages instead of just becoming a bigger peach cloud. Tuned to reach
    // the magenta/purple beauty phase early (~age 3) so viewers see the best
    // version without waiting for the full lifecycle.
    float cooling = smoothstep(1.5, 5.5, ejecta_age);
    // The raw Sedov curve (t^0.42) has a near-vertical slope at t = 0, which
    // made the shell pop to full size the frame it appeared. The launch ease
    // throttles early growth so the shell visibly swells out of the
    // singularity, then hands over to the coasting expansion.
    // Tuning: the 1.6s launch window and the 0.45 time-scale inside pow() are
    // what set how leisurely the shell grows; the perceived speed doubles as
    // it nears the camera, so keep the curve gentler than feels right on paper.
    float launch = smoothstep(0.0, 1.6, ejecta_age);
    // Coefficient sets the framed size of the settled remnant: kept below ~1.8
    // so the whole shell sits in frame with black margin around it, rather than
    // expanding past the camera into a screen-filling blob. Expansion also eases
    // off once cooling sets in, so the remnant lingers near its beauty size for
    // viewing instead of thinning into a huge faint cloud.
    float expansion = pow(ejecta_age * 0.45 + 0.02, 0.45);
    expansion = mix(expansion, expansion * 0.7 + 0.42, cooling);
    float shell_radius = 0.3 + 0.95 * launch * expansion;
    // Thin shell: material concentrated near the front so the remnant reads as
    // an expanding shell (limb-brightened, hollow) rather than a filled ball.
    float shell_thickness = mix(0.10, 0.26, smoothstep(0.0, 4.0, ejecta_age));

    float sphere_radius = radius_scale;   // nominal radius the density field fades out around
    float star_march_radius = sphere_radius * (0.9 + raggedness);   // matches the density field's maximum warped radius
    float ejecta_march_radius = shell_radius + shell_thickness * 3.0;
    float march_radius = mix(star_march_radius, max(star_march_radius, ejecta_march_radius), ejecta_strength);
    vec2 bounds = hit_sphere_bounds(ray_origin, ray_dir, sphere_center, march_radius);
    // Between the star volume's outer surface and the ejecta shell's inner
    // face there is provably no medium -- both density functions early-out
    // there. The march loop jumps that hollow interior analytically instead
    // of stepping through it. (Pre-detonation this radius sits inside the
    // star, so the skip condition simply never fires.)
    float shell_inner_radius = shell_radius - shell_thickness * 3.0;

    // Wide soft halo, keyed loosely to the nominal radius so there is no hard ring
    // at exactly R. This is the only "atmosphere" term left now that the rim/collar
    // are gone; the volume itself supplies the disk edge.
    float near_distance = ray_sphere_near_distance(ray_origin, ray_dir, sphere_center);
    float halo = 1.0 - smoothstep(sphere_radius * 0.85, sphere_radius * 2.1, near_distance);
    halo = pow(halo, 2.0);
    // The halo whitens and intensifies with the collapsing core, then blows
    // out fully during the flash, so the aura tracks the object's heat.
    float halo_heat = clamp(core_glow + flash_white, 0.0, 1.0);
    vec3 halo_tint = mix(vec3(0.9, 0.16, 0.03), vec3(1.0, 0.88, 0.72), halo_heat);
    vec3 halo_color = halo_tint * halo * u_halo_strength * brightness * (1.0 + core_glow * 1.5 + flash_white * 1.2);

    vec3 accumulated_color = vec3(0.0);
    float transmittance = 1.0;

    if (bounds.y > 0.0) {
        float t = max(bounds.x, 0.0); // start marching at entry point unless behind camera
        float t_end = bounds.y;
        while (t < t_end) {
            vec3 sample_point = ray_origin + t * ray_dir;
            // Fine steps only matter in the dense core; the thin outer shell
            // can stride coarser without visible banding.
            float core_distance = length(sample_point - sphere_center);
            // Hollow-interior skip: stepping s along any ray changes the
            // distance to center by at most s, so jumping (inner - d) from
            // inside the void can never overshoot into the shell. The whole
            // empty interior costs one iteration instead of dozens.
            if (core_distance > star_march_radius && core_distance < shell_inner_radius) {
                t += max(shell_inner_radius - core_distance, 0.05);
                continue;
            }
            // Step bounds scale with the star so a swollen star keeps the same
            // sample density (and cost profile) as the idle one.
            float step_size = mix(0.03, 0.075, smoothstep(0.7 * radius_scale, 1.35 * radius_scale, core_distance));
            step_size *= mix(1.0, 1.8, mist);
            // Outside the star, resolution only needs to resolve the shell's
            // Gaussian profile: ~2 samples per sigma is plenty for gas this
            // soft. As the shell disperses and thickens, the stride grows with
            // it instead of oversampling a blur.
            if (core_distance > star_march_radius) {
                step_size = max(step_size, shell_thickness * 0.55);
            }
            float density  = sample_density(sample_point, sphere_center, sphere_radius, raggedness);
            float shell_depth;
            float ejecta_density = sample_ejecta_density(sample_point, sphere_center, shell_radius, shell_thickness, ejecta_strength, shell_depth);
            // Higher density maps to hotter gas colors without washing the core
            // into a flat white blob. The star volume keeps its stellar ramp for
            // its whole life -- remnant colours belong to ejecta_color(), not here.
            // Surface heat is a photosphere effect: its factor is zero once the
            // star whitens/collapses/disperses (core_glow/flash/mist), and it is
            // zero outside the photosphere band anyway, so skip its two noise
            // octaves in those cases instead of computing and discarding them.
            float heat_factor = 1.0 - clamp(core_glow + flash_white + mist, 0.0, 1.0);
            float surface_heat = 0.0;
            if (heat_factor > 0.001 && core_distance < sphere_radius * 1.2) {
                surface_heat = stellar_surface_heat(sample_point, sphere_center, sphere_radius) * heat_factor;
            }

            vec3 cool_gas = vec3(0.22, 0.006, 0.00);
            vec3 warm_gas = vec3(0.66, 0.045, 0.00);
            vec3 hot_gas = vec3(0.95, 0.20, 0.025);
            vec3 flare_patch = vec3(1.0, 0.42, 0.08);
            vec3 gas_color = mix(cool_gas, warm_gas, density);
            // High threshold keeps the hot core small and lets turbulence survive.
            float hot_mask = smoothstep(0.83, 1.0, density);
            gas_color = mix(gas_color, hot_gas, hot_mask);
            gas_color = mix(gas_color, flare_patch, smoothstep(0.48, 0.90, surface_heat));
            // Collapse core: proximity to the center whitens the gas itself and
            // adds a strong bloom, so the crushed star reads as a brilliant
            // white-hot point of light rather than a dim red ember. The kernel
            // extends past the warped surface (1.2R) so even the rim whitens
            // and no dark red shell survives around the singularity.
            float core_proximity = 1.0 - smoothstep(0.0, sphere_radius * 1.2, core_distance);
            vec3 singularity_white = vec3(1.0, 0.96, 0.88);
            // The flash whitens the whole volume, not just the core.
            gas_color = mix(gas_color, singularity_white, clamp(core_glow * core_proximity + flash_white, 0.0, 1.0));
            // Remnant colour is applied unconditionally: gating it on ejecta_density
            // was measured slower, because that test diverges within a warp (strand
            // vs gap at the same step) and the GPU then runs both sides plus branch
            // overhead -- more costly than the two noise octaves it would save.
            vec3 shell_color = ejecta_color(sample_point, sphere_center, shell_depth, ejecta_density, cooling);
            // afterglow briefly whitens the freshly-revealed remnant, then it cools.
            vec3 afterglow_shell = mix(shell_color, vec3(1.0, 0.92, 0.82), afterglow * 0.55);
            vec3 emission = gas_color * density * u_emission_strength * brightness * star_fade;
            emission += flare_patch * surface_heat * density * u_emission_strength * 0.95 * star_fade;
            emission += singularity_white * (core_glow * 5.0) * pow(core_proximity, 2.0) * density * u_emission_strength;
            // Age-based luminosity decay: the diffuse haze dims strongly while the
            // bright filaments persist, so a late remnant is dark gas threaded with
            // a few surviving glowing veins rather than a uniformly fading cloud.
            float vein = smoothstep(0.42, 0.90, ejecta_density);
            float age_fade = mix(1.0 - cooling * 0.62, 1.0 - cooling * 0.15, vein);
            emission += afterglow_shell * ejecta_density * u_emission_strength * 0.62 * age_fade * ejecta_strength;
            // Cyan rim glow inside the volume: the ionised outer shell emits,
            // weighted by the soft (haze-inclusive) density toward the rim. The band
            // thins with age but the rim stays cyan, so the silhouette keeps a crisp
            // ionised edge as the body cools.
            float rim_glow = smoothstep(mix(0.60, 0.82, cooling), 1.0, shell_depth);
            emission += vec3(0.14, 0.72, 1.00) * ejecta_density * rim_glow * u_emission_strength * mix(0.45, 0.55, cooling) * ejecta_strength;
            // Higher ejecta absorption makes near strands occlude far ones, so the
            // remnant reads as layered 3D structure with a darker hollow centre
            // instead of an additive cotton-candy wash. It thins with age so the
            // old remnant becomes translucent -- you see through it to the far side.
            float absorption = (density * u_absorption_strength * absorption_scale) + (ejecta_density * u_absorption_strength * mix(0.55, 0.30, cooling));
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

    // Remnant cyan corona: a soft analytic ionised halo that blooms outward past
    // the shell, giving the whole silhouette a radiant glowing edge (the signature
    // of the reference). Brightest at/inside the rim and fading into black just
    // outside; added with transmittance so opaque gas occludes it and it glows
    // through the wispy edge and the dark holes. Gated to the remnant phase.
    float corona = pow(1.0 - smoothstep(shell_radius * 0.82, shell_radius * 1.55, near_distance), 1.5);
    accumulated_color += vec3(0.10, 0.62, 1.00) * corona * ejecta_strength * transmittance * 0.6;

    // World-space glare front. The detonation's light is modelled as a luminous
    // sphere centred on the remnant whose radius physically travels: it grows
    // out from the blast, engulfs the camera (whiteout), then recedes back onto
    // the remnant to reveal it. Because it lives in world space, its screen size
    // is set by geometry -- a disk that swells from the remnant, fills the frame,
    // then collapses back to a point, no hand-tuned screen radius.
    vec3 cam_to_center = sphere_center - u_camera_pos;
    float cam_distance = length(cam_to_center);
    float front_depth = max(dot(cam_to_center, u_camera_forward), 0.001);
    // The remnant's position on the view plane, in the same units as `centered`.
    vec2 remnant_view = vec2(dot(cam_to_center, u_camera_right),
                             dot(cam_to_center, u_camera_up)) / front_depth;
    float glare_screen_dist = length(centered - remnant_view);

    // Front radius: grows past the camera (overshoot engulfs it), then recedes
    // all the way back to the remnant's own shell.
    float overshoot_radius = cam_distance * 1.15;
    float grow_radius = mix(0.3, overshoot_radius, event.glare_grow);
    // Ease-out so the front plunges back through the camera quickly, then spends
    // most of the recede visibly shrinking to a small knot on the remnant. The
    // angular size stays screen-filling while the radius is near cam_distance, so
    // the visible pull-back only happens once the radius drops well below it.
    float recede_eased = 1.0 - pow(1.0 - event.glare_recede, 2.0);
    float front_radius = mix(grow_radius, 0.35, recede_eased);

    // Energy holds blinding through the hit and the whole shrink, fading only at
    // the very end -- so the frame reads as the white disk collapsing onto the
    // remnant, not just dimming in place.
    float glare_energy = clamp(event.glare_grow * 1.25, 0.0, 1.0)
                       * (1.0 - smoothstep(0.85, 1.0, event.glare_recede));

    if (glare_energy > 0.0) {
        vec3 glare_rgb;
        if (front_radius >= cam_distance) {
            glare_rgb = vec3(1.0); // camera is inside the front: full whiteout
        } else {
            // Angular size of the sphere on the view plane; as the front recedes
            // this shrinks, collapsing the white disk back onto the remnant.
            float base_radius = front_radius / sqrt(cam_distance * cam_distance - front_radius * front_radius);
            // Sample the edge per channel at slightly different radii so the
            // receding rim splits into a spectral fringe (chromatic dispersion)
            // without any post pass -- red largest, blue smallest.
            float inner = base_radius * 0.72;
            glare_rgb.r = 1.0 - smoothstep(inner * 1.000, base_radius * 1.000, glare_screen_dist);
            glare_rgb.g = 1.0 - smoothstep(inner * 0.985, base_radius * 0.985, glare_screen_dist);
            glare_rgb.b = 1.0 - smoothstep(inner * 0.970, base_radius * 0.970, glare_screen_dist);
        }
        vec3 glare_tint = mix(vec3(1.0, 0.97, 0.90), vec3(1.0, 0.88, 0.66), event.glare_recede);
        float glare_coverage = clamp(max(glare_rgb.r, max(glare_rgb.g, glare_rgb.b)) * glare_energy, 0.0, 1.0);
        float whiteout_strength = glare_coverage * (1.0 - smoothstep(0.35, 0.92, event.glare_recede));
        accumulated_color = mix(accumulated_color, vec3(90.0) * glare_tint, whiteout_strength);
        accumulated_color += glare_tint * glare_rgb * glare_energy * 2.8;

        // Anisotropic bloom: stretch the same detonation light horizontally
        // instead of drawing a separate disk over the round flash. The wide
        // core bridge keeps the flare glued to the white center.
        vec2 flare_p = centered - remnant_view;
        float flare_life = 1.0 - smoothstep(0.72, 1.0, event.glare_recede);
        float line_width = mix(0.055, 0.026, event.glare_recede);
        float line_length = mix(0.85, 2.35, event.glare_grow);
        float horizontal_core = exp(-(flare_p.y * flare_p.y) / (line_width * line_width));
        horizontal_core *= exp(-abs(flare_p.x) / line_length);

        vec2 bridge_p = vec2(flare_p.x * 0.45, flare_p.y * 1.8);
        float center_bridge = exp(-dot(bridge_p, bridge_p) * 3.2);
        // Hollow the midline so it reads like a luminous ring/flare around the
        // blast instead of a solid bar pasted across the center.
        float hollow_core = 1.0 - exp(-dot(flare_p, flare_p) * 18.0);
        float ring_band = smoothstep(0.05, 0.18, abs(flare_p.y)) * (1.0 - smoothstep(0.28, 0.62, abs(flare_p.y)));
        float hollow_flare = horizontal_core * mix(0.28, 1.0, hollow_core);
        hollow_flare += horizontal_core * ring_band * 0.42;
        float blended_flare = max(hollow_flare * 0.82, center_bridge * 0.55);
        blended_flare *= flare_life * glare_energy;

        // Slight chromatic edge: blue on the outer glare, warm near the core.
        float flare_edge = smoothstep(0.12, 0.9, abs(flare_p.x));
        vec3 lens_tint = mix(vec3(1.0, 0.66, 0.24), vec3(0.62, 0.74, 1.0), flare_edge);
        accumulated_color += lens_tint * blended_flare * 5.2;
    }

    float luminance = max(accumulated_color.r, max(accumulated_color.g, accumulated_color.b));
    if (luminance < 0.002) {
        discard; // nothing here — let the grid and background show through
    }

    float remnant_exposure = mix(1.0, 0.48, ejecta_strength * (1.0 - flash_white));
    accumulated_color *= remnant_exposure;
    accumulated_color = accumulated_color / (accumulated_color + vec3(1.0));
    frag_color = vec4(accumulated_color, 1.0);
}
