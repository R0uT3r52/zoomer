# Zoomer

Zoomer is a screen magnification utility for Linux, built with C++, SDL3, and OpenGL 3.3. It allows users to capture a screenshot of their active workspace and zoom into specific areas.

The primary goal of the project is to make the transition between the desktop and the magnified view as seamless as possible. While the project strives for this "invisible" integration, the level of seamlessness depends on the specific compositor and display server configuration.

![App preview](./assets/preview.gif)

## Features

- **X11 & Wayland Support**: Detects the session type and uses appropriate capture methods.
- **Active Monitor Focus**: On X11, the application captures the monitor where the cursor is currently located.
- **OpenGL**: Uses OpenGL 3.3 for smooth zooming and panning.
- **Modern SDL3**: Implemented using the new SDL3 callback-based architecture.
- **AppImage Distribution**: Portable build support via Docker.

## Prerequisites

To build and run Zoomer, you need the following dependencies:

- **C++20 Compiler** (GCC 11+ or Clang 13+)
- **CMake** (3.21+) and **PkgConfig**
- **SDL3** && **SDL3_image** (with PNG/JPEG support)
- **sdbus-c++** (for Wayland portal screenshot capture) && **libsystemd**
- **OpenGL / Mesa** && **libX11** dev headers
- **xxd** (for baking GLSL shaders into the executable)

### Installing dependencies

- **Arch Linux**:
  ```bash
  sudo pacman -S gcc cmake pkgconf sdl3 sdl3_image sdbus-cpp libx11 mesa vim
  ```
- **Fedora**:
  ```bash
  sudo dnf install gcc-c++ cmake pkgconf-pkg-config SDL3-devel SDL3_image-devel sdbus-c++-devel libX11-devel mesa-libGL-devel xxd
  ```
- **Debian / Ubuntu**:
  ```bash
  sudo apt install build-essential cmake pkg-config libsdl3-dev libsdl3-image-dev libsdbus-c++-dev libsystemd-dev libx11-dev libgl1-mesa-dev xxd
  ```
> [!NOTE]
> If `SDL3` or `SDL3_image` are missing from your distribution's repositories, build them from source or use the Docker environment below.
> 
> If you encounter compilation errors (such as syntax mismatches), it is highly recommended to build `SDL3`, `SDL3_image`, and `sdbus-cpp` from source to ensure version compatibility. Used versions are provided in `Dockerfile`
> 
> Alternatively, you can use the provided Docker container to automatically build the application as an AppImage.


On Wayland, the application attempts to use the **XDG Desktop Portal** (via `sdbus-c++`). If that fails, it falls back to one of the following tools: `grim`, `hyprshot`, `spectacle`, or `flameshot`.


## Installation

### Download Release
You can download the pre-compiled portable **AppImage** from the [GitHub Releases](https://github.com/R0uT3r52/zoomer/releases) page.

### Standard Build
1. Clone the repository:
   ```bash
   git clone https://github.com/R0uT3r52/zoomer.git
   cd zoomer
   ```
2. Configure and build:
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ```
3. Run: `./build/zoomer`

### Building AppImage (via Docker)
1. Build the Docker image: `docker build -t zoomer-builder .`
2. Extract the AppImage: `docker run --rm -v $(pwd):/out zoomer-builder`

## Setup Recommendation

Since Zoomer is not a background daemon, it is highly recommended to bind it to a system-wide hotkey (e.g., `Super + Z`). 

- **X11**: Use your desktop environment's keyboard settings or `xbindkeys`.
- **Wayland**: Use your compositor's configuration.

Point the hotkey to the absolute path of the `zoomer` binary or the downloaded `AppImage`.

## Usage

When launched, Zoomer captures a snapshot of the current screen and opens in a fullscreen window.

### Controls

| Input | Action |
| :--- | :--- |
| **Mouse Wheel** | Zoom in / Zoom out |
| **Left Click + Drag** | Pan the view |
| **R** | Reset zoom and position |
| **Q** / **Esc** | Exit application |

## Known Issues

- **Multi-monitor Support**: While the application attempts to detect the active monitor, behavior on complex multi-monitor setups may be inconsistent on X11 or Wayland. Improvements are currently in development.

## Acknowledgements

- This project is heavily inspired by [boomer](https://github.com/tsoding/boomer) by **tsoding**.
- Shader logic and general application flow are based on the original nim implementation.

## License

This project is licensed under the MIT License. See the LICENSE file for details.
