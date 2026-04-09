# Project 9 — Advanced Shader Implementation with Camera Control

**Course:** CST-310: Computer Graphics
**Institution:** Grand Canyon University
**Authors:** Jared Walker &amp; James Hohn
**Instructor:** Ricardo Citro
**Date:** April 9th, 2026
**GitHub Repository:** [Your GitHub URL here]

---

## Overview

A 3D scene rendered entirely through a custom GLSL shader pipeline. The scene features a procedurally generated checkerboard floor with three objects — a sphere, a cylinder, and a cube — each shaded with per-fragment Blinn-Phong lighting. A free-look camera lets the user navigate across the checkerboard and inspect all three objects.

---

## Features

- Two GLSL shader programs (solid objects and checkerboard floor)
- Per-fragment Blinn-Phong lighting with configurable shininess per object
- Procedural checkerboard pattern generated in the fragment shader from world position
- Hand-generated sphere (spherical coordinates), cylinder (parametric circle + caps), and cube (per-face normals)
- Free-look camera with forward/strafe movement and yaw/pitch rotation
- Custom buildPerspective and buildView matrix functions (no deprecated gluLookAt/gluPerspective)
- All geometry stored in VAO/VBO on the GPU

---

## Controls

| Key | Action |
|-----|--------|
| W | Move forward |
| S | Move backward |
| A | Strafe left |
| D | Strafe right |
| Left / Right arrow | Rotate camera yaw |
| Up / Down arrow | Rotate camera pitch |
| Ctrl + Left / Right | Precise yaw (2 degrees) |
| Ctrl + Up / Down | Precise pitch (2 degrees) |
| Shift + Up | Slide camera in (positive Z) |
| Shift + Down | Slide camera out (negative Z) |
| < | Roll camera left |
| > | Roll camera right |
| R | Reset camera to default position |
| ESC | Quit |

---

## Compile & Run

```bash
g++ project9.cpp -o project9 -lGL -lGLU -lGLEW -lglut -std=c++17
./project9
```

---

## Software Requirements

- **OS:** Ubuntu 22.04 LTS or later
- **Compiler:** g++ with C++17 support
- **OpenGL:** 3.3 core profile or higher
- **Libraries:** GLEW, FreeGLUT, GLU

Install:

```bash
sudo apt install build-essential freeglut3-dev libglew-dev libglu1-mesa-dev -y
```

---

## File Structure

```
project9/
├── project9.cpp       # Full source — geometry, shaders, camera
├── instructions.txt   # Run instructions and requirements
├── README.md          # This file
└── screenshots/
    ├── scene.png
    └── camera_angle.png
```

---

## Shader Summary

| Shader | Purpose |
|--------|---------|
| Vertex (shared) | MVP transform, normal matrix, world-space fragPos |
| Fragment — solid | Blinn-Phong with objectColor and shininess uniforms |
| Fragment — board | Procedural checker from floor(fragPos.xz) + Blinn-Phong |

---


## References

- de Vries, J. (2021). *Basic lighting*. LearnOpenGL. https://learnopengl.com/Lighting/Basic-Lighting
- de Vries, J. (2021). *Camera*. LearnOpenGL. https://learnopengl.com/Getting-started/Camera
- Khronos Group. (2024). *OpenGL wiki*. https://www.khronos.org/opengl/wiki
- Shreiner, D., et al. (2013). *OpenGL programming guide* (8th ed.). Addison-Wesley.
- Angel, E., &amp; Shreiner, D. (2015). *Interactive computer graphics* (7th ed.). Pearson.
