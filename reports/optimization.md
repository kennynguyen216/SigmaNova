## This was with the help of AI. 

## The red supergiant was originally running at around `42 FPS`, or `24.0 ms/frame`. After optimizing the volumetric gas renderer, it improved to roughly `146 FPS`, or `6.84 ms/frame`.

- This is about a `3.5x` speedup while keeping the main visual result intact: the dissolved gas silhouette, turbulent core, warm plasma color, and soft halo over the spacetime fabric still read correctly.
- The comparison was measured with the title-bar frame-time readout, which averages frames over a half-second window. The profiling run used the same scene view before and after optimization, with vsync disabled so the FPS number reflected shader cost instead of monitor refresh rate.

## The Problem
- The red supergiant is rendered as a fullscreen raymarched volume. For every visible pixel, the shader:

1. Generates a camera ray.
2. Finds where that ray enters and exits the gas volume.
3. Marches along that ray in small steps.
4. Samples procedural density at each step.
5. Uses noise functions to create turbulent gas structure.
6. Accumulates emitted light and reduces transmittance through absorption.

The cost multiplies quickly.

## The Fixes
1. Cheaper Hash function.
    - Before, I was using sin(), which is GPU expensive.
    - I switched to a multiplicative hash using fract().
2. Fractal brownian motion layers noises on top of each other.
    - The new optimization tracks the total amplitude and divides the accumulated noise by it.
    - This keeps the output in a similar range even when the octave count changes. This matters because the shader can now use fewer octaves in cheaper paths without completely changing the brightness or density behavior.
    - Essentially, the important parts will get the 4 octaves of noise, while the less important outer details will get maybe 2 or 3.
3. Tiered Noise Quality.
    - Not every bit of noise needs the same amount of attention.
    - The core should get the most amount of attention, the finer details less attention, and the surface edge noise the least amount.
## How are 2 and 3 separate points?
    - Normalized FBM makes cheap and expensive noise outputs stay in the same brightness range.
    - Tiered Noise Quality decides when to use cheap or expensive noise.
4. Cutting useless sample_density().
    - Shader rejects doing expensive noise calculations whenever possible.
    - If a sample point is beyond the maximum possible gas radius, density immediately returns `0.0`.
    - If a point is outside the noise-warped effective surface, the shader also returns `0.0` before computing the expensive convection/detail layers.
    - Essentially, this handles the edge cases of the star because the majority of the points are gonna miss or be 0.
5. Tightened up the ray marching bound.
    - The raymarch bounding sphere was tightened from `1.4` to `1.35`.
    - The density math guarantees gas cannot exist beyond the maximum warped radius, so marching farther only wastes steps through empty space.
6. Adaptive Step Size.
    - We have our standard step size for the inner core, where it needs to have lots of detail.
    - The step size for the outer part of the star is increased so we can skip some calculations, as it's not really needed/impactful as the inner core detail.
7. Transmittance Early Exit.
    - This was not the main new optimization pass, but it is an important existing early-exit. Once absorption drives transmittance near zero, the shader stops marching because later samples would barely affect the final pixel.

## Result

Before optimization:

~24.0 ms/frame
~42 FPS

After optimization:

~6.84 ms/frame
~146 FPS

~3.5x faster

## Lesson
    - I reduced the calculations wherever it did not matter as much in terms of visual quality and stopped pointless calculations.
    - I spent the computational budget on where it actually mattered.
