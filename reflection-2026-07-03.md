# SigmaNova Reflection - 2026-07-03

## What I Built Today

Today was a major rendering day for SigmaNova. The project moved from a ray-intersected red giant and experimental spacetime grid into a more complete scene with mesh-rendered fabric, volumetric raymarching, turbulent gas density, corona glow, additive blending, and documented visual milestones.

## Commits From Today

- `17a4ff5` - Add blotchy red giant surface variation
- `e244fee` - Fix resize and frame-rate bugs, add clang-format
- `482bafe` - Replace raymarched fabric with mesh grid rendering
- `5f87b71` - Remove local resume notes from repository
- `da3be10` - ray sphere derivation
- `0c7e9d2` - Marching my rays instead of tracing them
- `d1c209d` - Add volumetric raymarching gas pass
- `391867f` - Add volumetric gas shading and corona blending
- `f35cf24` - Add volumetric gas noise milestone

## Biggest Technical Wins

### 1. Replaced Shader-Raymarched Fabric With Mesh Geometry

The first spacetime fabric attempt rendered the grid inside the fullscreen fragment shader. That caused noisy red/purple/cyan artifacts because the shader was trying to procedurally draw thin grid lines on a curved surface per pixel.


Reflection:
- My grid looked weird at certain points. I found out that trying to draw the grid procedurally inside the same fullscreen shader as the sphere would cause some issues and make random colors. 
- To fix this I made the grid its own object and rendered it alongside the sphere
- I larped gravity by using a function I found online to fake the gravity well for cool visuals


### 2. Moved From Ray-Sphere Surface Rendering To Volumetric Raymarching

The red giant used to be rendered by finding one ray-sphere hit point and shading the surface. Today, the shader changed to compute sphere entry and exit distances, then march through the inside of the sphere in steps.

The big mental shift:

```text
Ray tracing surface: one hit point
Raymarching volume: many samples along the ray
```

Reflection:
- This was the next step up from the Ray Tracing in One Weekend, and I am proud to have applied the things that I have learned from that into this project. The videos I watched of ray marching made it very complex to me. But the math of implementing this was rather easy. It is essentially just regular ray tracing except you have to return the entry and exit and then iterate through it and accumulate colors until the march reaches the exist distance. 
Basically a subtle change in the hit sphere function and a loop. 

### 3. Added `sample_density` As A Future Simulation Bridge

Density is now sampled through a function:

```glsl
float sample_density(vec3 p, vec3 sphere_center, float sphere_radius)
```

Right now this function uses procedural density, but later it can be replaced with a 3D texture or simulation-backed field. This keeps the renderer expandable for a future Navier-Stokes or Stable Fluids path.

Reflection:
- I am no longer just hitting a sphere and shading it in. I have to think of it as a bunch of density fields with different density values inside the gas volume. I made this a separate function so that I can later expand on this and make it better. It separates the ray marching renderer from the gas density.
I honestly really struggled trying to trouble shoot this problem as I had an issue where the edges were insanely dark. 

### 4. Added Procedural Volumetric Gas Noise

The shader moved away from simple sine-band noise and toward value-noise/FBM-style gas texture. The current noise uses:

- a `hash` function for pseudo-random lattice values
- `value_noise` to interpolate between cube corners
- `gas_noise` to layer noise at multiple frequencies

Reference:

- The Book of Shaders: Noise - https://thebookofshaders.com/11/

Reflection:
- So before this I had a bunch of sine bands. It made the star look like it had a certain pattern to it like a bunch of longitude and lattitude lines. 
And I needed it to look gassy. So I needed it to be random but a controlled random.
- I created pseudo random values using hash and then I used floor and fract to determine what grid cell im in and the specific location of the grid that I am in. 
- Blended random corner value sand layered multiple noise frequencies to mimic the effect of plasma. this is not real fluids but its pretty cool looking. 
## EXPLANATION IN MY OWN WORDS

How value noise and FBM work mathematically without looking at the code.
- Ok so imagine we have a cube. This cube has 8 corners. Assign a random value at each of the 8 corners. Then interpolate (find a value between these two values) using a percentage (which is based off a sample point inside the cube, and based on how far along to sample point is in the cube (if its 30% across the cube in x axis (local.x = .3), or more than halfway up in the y axis like 60% (local.y = .6), etc)), these local values we use for interpolation percentages we use mix() and then smooth it out for smoother transitions.
    - Local tells you where you are inside the cube. You can use local as a mix percentage but that makes the transition between neighboring cells change direction to sharply at cube boundaries. This can create faint grid like creases.
        - we do local = local * local * (3.0 - 2.0 * local)
            - this equation is a hermite fade or smoothstep
            - smooths interpolation weights before mxing which flattnes the slope at cell boundaries so the value noise does not show hard gride like creases
        - Fractal brownian motion (fbm) layers these noise patterns on top of eachother, each layer being an octave
            - Layer 1: lower freq high amp
                - Big soft blobs
            - layer 2: higher freq smaller amp
                - medium details
            - layer 3: even higher freq and even lower amp
                - smaller detais
        - Higher frequencies make smaller more detailed shapes.
            - Higher frequencies means the noise changes more often in the same space so the blobs/features get tighter and smaller
        - Amplitude is how much that noise layer changes the final result
            - High amps means the layer is visually evident by alot
            - lower amps means it adds a subtle texture 

### 5. Added Temperature-Based Gas Color

Density now drives color:

- low density: dark red
- medium density: orange
- high density: yellow/white hot core

This makes the red giant feel more like hot plasma instead of a flat orange fog ball.

Reflection:
- density as a stand-in for temperature. Denser regions are mapped to hotter colors while thinner regions stay darker or orange. This gives a heat map inside the gas.

### 6. Fixed The Corona / Black Ring Issue

The black ring came from the outer shell density being crushed too close to black. The fix was to:

- lift density near the sphere edge with `sqrt(radial)`
- raise the noise floor so low-noise regions stay as dim gas
- clamp density before using it in the color ramp
- match inner rim glow and outer corona at the sphere boundary

Reflection:
- A ray can hit the volume but can stil return black if the density is too low. The edge volume relies on the density fall off function. the sqrt radial lifts the low density values along the edges so that they collect a visual glow. the inner rim and the corona need to match so it looks continuous which led to me discovering it was a density issue. 


### 7. Added Additive Blending For Glow Over The Grid

The star shader is now blended over the grid with additive blending:

```cpp
glEnable(GL_BLEND);
glBlendFunc(GL_ONE, GL_ONE);
```

This makes the star and corona behave more like light being added over the spacetime fabric instead of dark pixels painting over the grid.

Reflection:
- Well I learned what additive blending is which helped me render the star and corona more like light. I was drawing dark corona pixels over the space time grid which made the grid disappear around the star. With the addition of additive blending, the star adds its color to the grid. I used this function to do so. 
glBlendFunc(GL_ONE, GL_ONE) makes the source and destination colors add together.


## Things I Understand Better Now

Fill this in:

- Ray-sphere entry and exit distances:
    - hand-derived the ray sphere interactions that I learned from RTiOW and it helped me understand it more.
    - it also helped me port it to raymarching
- Raymarching:
    - Basically ray tracing except you take the entry and you add onto it and accumuate colors until you reach the end. 
- VAO/VBO/EBO:
    - VBO is the raw floats and stuff that you put into an array
    - VAO tells opengl how the data should be interpreted
    - EBO you can reuse some vertices if they share the same vertices
- `GL_LINES`: 
    - Used this to draw the space time fabric
- Camera basis vectors:
    - Cameras local directions os that there are rays everywhere like forward, right, and up.
    - Fragment shaders use these directions to build a ray direction for each pixel. Forward to where cmaera looks, right offsets ray horizontally, and up offsets it vertically.
- Value noise:
    - the entire hash floor fract to create pseudo random values to make gas look organic 
        - hash creates pseudo random values at lattice/cube corners
        - uses floor to find the current grid cell and fract to find the local position inside that cell. 
        - blends the corner values with mix so the noise changes smoothly instead
- FBM-style layered noise:
    -   FBM-style noise means stacking multiple layers of noise at different scales. The first layer gives big soft shapes, then later layers add smaller details. Each layer usually increases frequency and lowers amplitude. this helps the gas look more like turbulent plasma instead of one smooth fog ball or simple sine-wave stripes.
- Additive blending: add color on top of previous color instead of replacing it. 
- Difference between renderer and simulator:
    - Renderer decides how something looks on screen. Simulator decides how the underlying state evolves over time. 

## Things That Still Feel Fuzzy That I Need To Look Into More

- How additive blending interacts with depth testing and render order.
- How a future 3D density texture would connect to `sample_density`.

## Question to really think about 
Near the gravity well, the grid lines compress. Why does that compression break lines computed per-pixel from shader math, but NOT break lines drawn as GL_LINES geometry?
- I tried to render the space fabric in the same shader as the star. Every pixel would figure out where its ray hit the curved fabic surface and decide whether or not its on a grid line or not. It mostly worked but towarrds the well, i got broken dashes and red and purple speckles. 
## Problem Diagnosis
 - near the well the grid compresses and the lines get so thin that the entire lines were thinner than the pixels trying to sample them so it would hit or miss at random. This was undersampling aliasing, where the grid pattern got finner than my sampling rate
## Solution
 - Instead of every pixel asking if a line exists or not, I generated the grid as a real geometry in C++, vertices that are bent by a gravity-well function, uploaded to the VBO, drawn with GL Lines. The rasterizer walks along the segment and fills the pixels it crosses so the line's existence does not whether a single pixel sample lands exactly on the mathematica line. 
Why didn't smoothing (fwidth-style) fix it?
 - Because smoothing is post-processing on information already captured. When the pixel samples are missing the lines then there is noting to smooth ebcause there is no information or detail to sample or process
What's the one-word name for this class of problem?
 - Aliasing or undersampling. The signal changes ffaster than the sampling rate can capture. 