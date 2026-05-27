# Black Hole Simulator

A real-time black hole visualization written in C++ using SFML and GPU fragment shaders. The simulator renders a single black hole with relativistic-style light bending, an Interstellar-inspired accretion disk halo, and a warped spacetime grid beneath the black hole.

This project started as a basic event horizon simulator and evolved into a shader-based visual experiment exploring gravitational lensing, accretion disk glow, photon-ring effects, and spacetime curvature.

## Features

- Real-time GPU fragment shader rendering
- Single black hole visualization
- Relativistic-style light bending
- Interstellar-inspired accretion disk halo
- Warped spacetime grid beneath the black hole
- Smooth galactic background light field
- Adjustable mass and camera angle
- White lensing glow around the photon region
- C++17 + SFML 3

## Demo Controls

| Key | Action |
|---|---|
| `Q` | Increase black hole mass |
| `E` | Decrease black hole mass |
| `W` | Raise camera |
| `S` | Lower camera |
| `A` | Move camera farther away |
| `D` | Move camera closer |
| `Esc` | Quit |

## Requirements

You will need:

- C++17-compatible compiler
- CMake
- SFML 3
- vcpkg recommended for dependency installation

## Installing SFML with vcpkg

```powershell
git clone https://github.com/microsoft/vcpkg
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg install sfml:x64-windows
