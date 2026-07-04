# SigmaNova

SigmaNova is a C++/OpenGL project for building a real-time supernova visualization engine.

The goal is to start from a clean graphics programming foundation, then build toward an interactive volumetric renderer that can show a glowing, expanding supernova-like cloud.

## Current Milestone

![Volumetric gas demo](assets/captures/volumetric-gas-demo.gif)

The current build renders a red-giant-inspired volumetric gas sphere above a mesh-rendered spacetime fabric. The fullscreen fragment shader computes camera rays, finds sphere entry/exit bounds, raymarches through the volume, samples turbulent density, maps density to hot gas color, and blends a soft corona over the grid.

This milestone builds on the orbit camera path: C++ owns camera movement, sends camera uniforms to GLSL, and the fragment shader generates a ray per pixel. The visual target is now shifting from a surface test sphere toward a gaseous pre-supernova red supergiant.

## Rendering Notes

The spacetime-fabric grid started as a fullscreen fragment-shader experiment. The early prototype ray marched against a curved surface and procedurally drew grid lines per pixel. That proved the idea, but it caused aliasing: where the grid compressed near the gravity well, thin procedural lines could fall between pixel samples, creating noisy red/purple/cyan artifacts and broken-looking line segments.

![Shader-raymarched spacetime fabric artifacts](assets/captures/spacetime-fabric-artifact.png)

The fix is to render the fabric as real OpenGL line geometry instead of as a raymarched shader illusion. C++ now generates grid vertices, bends those vertices with a simple gravity-well equation, uploads them to a VBO/VAO, and draws them with `GL_LINES`. The red giant remains ray-rendered in the fullscreen shader while the fabric is a separate mesh-rendered object.

![Mesh-rendered spacetime fabric fix](assets/captures/mesh-grid-fabric-fix.png)

## Milestone Progression

### 1. Fullscreen Shader Gradient

![Fullscreen shader gradient milestone](assets/captures/fullscreen-gradient-milestone.png)

This milestone proved that the renderer could use a fullscreen quad as a shader canvas. C++ sends `u_time` and `u_resolution` uniforms, and the fragment shader uses `gl_FragCoord` to compute a color for each pixel.

### 2. Orbit Camera Ray-Sphere

![Orbit camera ray-sphere milestone](assets/captures/orbit-camera-sphere-milestone.png)

This milestone adds a ray-sphere intersection and a C++ orbit camera. The camera sends position, forward, right, and up vectors into GLSL, letting the fragment shader generate camera-controlled rays for each pixel.

#### Ray-Sphere Math Notes

These notes show the derivation behind the ray-sphere intersection used in the fragment shader. The shader starts from the ray equation `P(t) = origin + t * direction`, substitutes that into the sphere equation, and solves the resulting quadratic to find whether the ray hits the sphere.

![Ray-sphere intersection handwritten notes page 1](assets/captures/ray-sphere-notes-1.jpg)

![Ray-sphere intersection handwritten notes page 2](assets/captures/ray-sphere-notes-2.jpg)

### 3. Red Giant Pulse

![Red giant pulse demo](assets/captures/red-giant-pulse.gif)

[Watch the full MP4 demo](assets/captures/red-giant-pulse.mp4)

This milestone starts the visual language for the pre-supernova star. The shader keeps the ray-sphere foundation, then layers in red-orange surface color, time-varying emission, rim glow, and center glow so the object reads more like a hot red giant than a matte test sphere.

### 4. Mesh Spacetime Fabric

![Mesh-rendered spacetime fabric fix](assets/captures/mesh-grid-fabric-fix.png)

This milestone replaces the shader-raymarched grid prototype with real OpenGL line geometry. C++ generates a grid of vertices, bends the grid with a simple gravity-well height function, uploads it to a VBO/VAO, and draws it with `GL_LINES`. The result is a cleaner black-and-white spacetime fabric without the noisy shader-line artifacts.

### 5. Volumetric Gas And Procedural Noise

![Volumetric gas demo](assets/captures/volumetric-gas-demo.gif)

This milestone moves the red giant from a surface-shaded sphere toward a volumetric gas object. The fragment shader computes sphere entry/exit bounds, marches through the volume, samples density along the ray, and accumulates emissive gas color. Density is now separated into a `sample_density` function so the current procedural shader field can later be replaced by a C++/texture-backed simulation field.

The gas texture is moving from simple sine-band modulation toward value-noise/FBM-style procedural noise. The goal is to replace visible sine spokes with more organic plasma-like clumps.

Reference used for this direction:

- [The Book of Shaders: Noise](https://thebookofshaders.com/11/) - explains random functions, value noise, smooth interpolation, and how noise can create organic shader patterns.

## Planned Stack

- C++
- CMake
- OpenGL
- GLFW
- GLAD
- GLM
- vcpkg

## First Milestone

Set up the project from scratch and prove the toolchain works:

- Create the CMake project structure.
- Build a minimal C++ executable.
- Add an OpenGL window with GLFW.
- Load OpenGL functions with GLAD.
- Print the OpenGL version.
- Clear the window to a visible color.
- Close cleanly with Escape.

## Build Status

### 2026-06-28

- Started the SigmaNova repository and README.
- Set up the first OpenGL "Hello World" milestone.

### 2026-06-29

- Added Escape key handling so the window can close cleanly.
- Drew the first triangle.
- Built the first OpenGL triangle rendering pipeline.
- Added basic shader file loading error output.

### 2026-06-30

- Cleaned up early project code and spacing.
- Added indexed rectangle rendering with an EBO.
- Added a uniform-driven shader color so the shape pulses green over time.

### 2026-07-02

- Added a reusable Shader class.
- Switched from triangle rendering to a fullscreen quad.
- Added time and resolution uniforms for a fullscreen shader gradient.
- Added a ray-sphere intersection shader.
- Added a C++ orbit camera with keyboard controls.
- Sent camera position and basis vectors into the fragment shader.
- Tuned the ray-rendered sphere into a red-giant-inspired emissive pulse.

### 2026-07-03

- Replaced the shader-raymarched spacetime fabric prototype with mesh-rendered `GL_LINES` geometry.
- Added a separate grid shader path for the fabric while keeping the star in the fullscreen ray shader.
- Hand-derived and documented the ray-sphere math used for entry/exit bounds.
- Moved the star from one-hit ray-sphere surface shading to volumetric raymarching.
- Added `sample_density` as the bridge between the renderer and future simulation-backed gas fields.
- Replaced sine-band surface patterns with value-noise/FBM-style procedural gas.
- Added density-driven temperature color: dark red gas, orange mid-density, and hot yellow-white core.
- Fixed the corona/black-ring issue by lifting edge density and matching inner/outer glow behavior.
- Added additive blending so the star and corona add light over the spacetime fabric instead of painting dark pixels over it.
- Added milestone captures for the mesh fabric, ray-sphere notes, and volumetric gas progress.
