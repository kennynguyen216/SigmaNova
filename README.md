# SigmaNova

SigmaNova is a C++/OpenGL project for building a real-time supernova visualization engine.

The goal is to start from a clean graphics programming foundation, then build toward an interactive volumetric renderer that can show a glowing, expanding supernova-like cloud.

## Current Milestone

![Orbit camera ray-sphere milestone](assets/captures/orbit-camera-sphere-milestone.png)

The current build renders a ray-intersected sphere from a fullscreen fragment shader and uses a C++ orbit camera to control the ray origin and camera basis vectors. `A`/`D` orbit around the sphere, `W`/`S` zoom the camera distance, and `Esc` closes the window.

This milestone proves the core renderer path for SigmaNova: C++ owns camera movement, sends camera uniforms to GLSL, and the fragment shader generates a ray per pixel to test against a 3D object. This is the bridge from simple shader gradients toward volumetric raymarching.

## Milestone Progression

### 1. Fullscreen Shader Gradient

![Fullscreen shader gradient milestone](assets/captures/fullscreen-gradient-milestone.png)

This milestone proved that the renderer could use a fullscreen quad as a shader canvas. C++ sends `u_time` and `u_resolution` uniforms, and the fragment shader uses `gl_FragCoord` to compute a color for each pixel.

### 2. Orbit Camera Ray-Sphere

![Orbit camera ray-sphere milestone](assets/captures/orbit-camera-sphere-milestone.png)

This milestone adds a ray-sphere intersection and a C++ orbit camera. The camera sends position, forward, right, and up vectors into GLSL, letting the fragment shader generate camera-controlled rays for each pixel.

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
