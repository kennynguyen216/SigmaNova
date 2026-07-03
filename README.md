# SigmaNova

SigmaNova is a C++/OpenGL project for building a real-time supernova visualization engine.

The goal is to start from a clean graphics programming foundation, then build toward an interactive volumetric renderer that can show a glowing, expanding supernova-like cloud.

## Current Milestone

![Red giant pulse demo](assets/captures/red-giant-pulse.gif)

[Full MP4 demo](assets/captures/red-giant-pulse.mp4)

The current build renders a red-giant-inspired emissive sphere from a fullscreen fragment shader. The sphere is ray-intersected per pixel, shaded with a surface normal, and enhanced with emissive color, rim glow, center glow, and a time-based pulse.

This milestone builds on the orbit camera path: C++ owns camera movement, sends camera uniforms to GLSL, and the fragment shader generates a ray per pixel to test against a 3D object. The visual target is now shifting from a test sphere toward the pre-supernova red supergiant phase.

## Rendering Notes

The spacetime-fabric grid is currently being redesigned. An early prototype rendered the grid inside the fullscreen fragment shader by ray marching against a curved surface and procedurally drawing lines per pixel. That proved the idea, but it caused noisy red/purple/cyan artifacts and broken-looking lines where the grid compressed near the gravity well.

![Shader-raymarched spacetime fabric artifacts](assets/captures/spacetime-fabric-artifact.png)

The next direction is to render the fabric as real OpenGL line geometry instead of as a raymarched shader illusion. The plan is to generate grid vertices in C++, bend those vertices with a simple parabola/gravity-well equation, upload them to a VBO/VAO, and draw them with `GL_LINES`. The red giant can stay ray-rendered in the fullscreen shader while the fabric becomes a separate mesh-rendered object.

The current fix follows that plan: the spacetime fabric is now a separate mesh grid drawn with OpenGL line geometry, while the red giant remains ray-rendered in the fullscreen shader.

![Mesh-rendered spacetime fabric fix](assets/captures/mesh-grid-fabric-fix.png)

## Milestone Progression

### 1. Fullscreen Shader Gradient

![Fullscreen shader gradient milestone](assets/captures/fullscreen-gradient-milestone.png)

This milestone proved that the renderer could use a fullscreen quad as a shader canvas. C++ sends `u_time` and `u_resolution` uniforms, and the fragment shader uses `gl_FragCoord` to compute a color for each pixel.

### 2. Orbit Camera Ray-Sphere

![Orbit camera ray-sphere milestone](assets/captures/orbit-camera-sphere-milestone.png)

This milestone adds a ray-sphere intersection and a C++ orbit camera. The camera sends position, forward, right, and up vectors into GLSL, letting the fragment shader generate camera-controlled rays for each pixel.

### 3. Red Giant Pulse

![Red giant pulse demo](assets/captures/red-giant-pulse.gif)

[Watch the full MP4 demo](assets/captures/red-giant-pulse.mp4)

This milestone starts the visual language for the pre-supernova star. The shader keeps the ray-sphere foundation, then layers in red-orange surface color, time-varying emission, rim glow, and center glow so the object reads more like a hot red giant than a matte test sphere.

### 4. Mesh Spacetime Fabric

![Mesh-rendered spacetime fabric fix](assets/captures/mesh-grid-fabric-fix.png)

This milestone replaces the shader-raymarched grid prototype with real OpenGL line geometry. C++ generates a grid of vertices, bends the grid with a simple gravity-well height function, uploads it to a VBO/VAO, and draws it with `GL_LINES`. The result is a cleaner black-and-white spacetime fabric without the noisy shader-line artifacts.

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
