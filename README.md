# SigmaNova

SigmaNova is a C++/OpenGL project for building a real-time supernova visualization engine.

The goal is to start from a clean graphics programming foundation, then build toward an interactive volumetric renderer that can show a glowing, expanding supernova-like cloud.

## Current Milestone

![Fullscreen shader gradient milestone](assets/captures/fullscreen-gradient-milestone.png)

The current build renders a fullscreen shader-driven gradient using fragment coordinates, resolution, and time uniforms. This is the first step toward a shader-first volumetric renderer.

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
