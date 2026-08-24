# Zoomer

Zoomer is a screen magnification utility for **Linux** and **Windows**, built with C++, SDL3, and OpenGL 3.3. It allows users to capture a screenshot of their active workspace and zoom into specific areas.

The primary goal of the project is to make the transition between the desktop and the magnified view as seamless as possible. While the project strives for this "invisible" integration, the level of seamlessness depends on the specific compositor and display server configuration.

![App preview](./assets/preview.gif)

## Features

- **Cross-Platform**: Runs on Linux and Windows.
- **X11 & Wayland Support**: Detects the session type and uses appropriate capture methods.
- **Windows Support**: Screen capture via GDI, no extra dependencies required.
- **Active Monitor Focus**: On X11, the application captures the monitor where the cursor is currently located.
- **OpenGL**: Uses OpenGL 3.3 for smooth zooming and panning.
- **Modern SDL3**: Implemented using the new SDL3 callback-based architecture.
- **Portable Distribution**: AppImage (Linux, built via Docker) and standalone `.exe` (Windows, built via vcpkg).

## Prerequisites

To build and run Zoomer, you need the following dependencies:

- **C++20 Compiler** (GCC 11+ / Clang 13+ on Linux; MSVC 2022+ on Windows)
- **CMake** (3.21+)

**Linux only:**
- **PkgConfig** (used by CMake to locate X11 and libsystemd)
- **SDL3**
- **sdbus-c++** (for Wayland portal screenshot capture) and **libsystemd**
- **OpenGL / Mesa** and **libX11** dev headers

**Windows only:**
- **vcpkg** (with the `VCPKG_ROOT` environment variable set) — SDL3 and Ninja are fetched automatically
- **Visual Studio 2022 or newer** with the C++ desktop workload (or MSVC Build Tools)

GLSL shaders are baked into the executable by `embedder` tool that is built automatically as part of the build, so no extra utilities are needed.

### Installing dependencies (Linux)

- **Arch Linux**:
  ```bash
  sudo pacman -S gcc cmake pkgconf sdl3 sdbus-cpp libx11 mesa
  ```
- **Fedora**:
  ```bash
  sudo dnf install gcc-c++ cmake pkgconf-pkg-config SDL3-devel sdbus-c++-devel libX11-devel mesa-libGL-devel
  ```
- **Debian / Ubuntu**:
  ```bash
  sudo apt install build-essential cmake pkg-config libsdl3-dev libsdbus-c++-dev libsystemd-dev libx11-dev libgl1-mesa-dev
  ```
> [!NOTE]
> If `SDL3` is missing from your distribution's repositories, build it from source or use the Docker environment below.
> 
> If you encounter compilation errors (such as syntax mismatches), it is highly recommended to build `SDL3` and `sdbus-cpp` from source to ensure version compatibility. Used versions are provided in `Dockerfile`
> 
> Alternatively, you can use the provided Docker container to automatically build the application as an AppImage.


On Wayland, the application attempts to use the **XDG Desktop Portal** (via `sdbus-c++`). If that fails, it falls back to one of the following tools: `grim`, `hyprshot`, `spectacle`, or `flameshot`.


## Installation

### Download Release
You can download pre-compiled binaries from the [GitHub Releases](https://github.com/R0uT3r52/zoomer/releases) page:
- **Linux**: portable **AppImage**
- **Windows**: standalone **zoomer-x64.exe**

### Standard Build (Linux)
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

### Building on Windows (vcpkg)
1. Make sure `VCPKG_ROOT` points to your vcpkg installation.
2. Launch **Developer PowerShell** with the **x64/amd64** architecture (both host and target), otherwise the build will fail. In Visual Studio 2022 this can be done by adding `-Arch amd64 -HostArch amd64` to the DevShell arguments.
   Alternatively, use the **x64 Native Tools Command Prompt for VS 2022**.
3. Configure and build:
   ```powershell
   cmake --preset windows
   cmake --build --preset windows-release
   ```
4. Run: `.\build-win\zoomer.exe`

Dependencies (SDL3) are downloaded automatically by vcpkg according to `vcpkg.json`.

### Building Linux AppImage (via Docker)
1. Build the Docker image: `docker build -t zoomer-builder .`
2. Extract the AppImage: `docker run --rm -v $(pwd):/out zoomer-builder`

## Setup Recommendation

Since Zoomer is not a background daemon, it is highly recommended to bind it to a system-wide hotkey (e.g., `Super + Z`).

- **Linux (X11)**: Use your desktop environment's keyboard settings or `xbindkeys`.
- **Linux (Wayland)**: Use your compositor's configuration.
- **Windows**: Use shortcuts placed in the Start Menu / Desktop folder, or third-party tools like [AutoHotkey](https://www.autohotkey.com/).

Point the hotkey to the absolute path of the `zoomer` binary or the downloaded AppImage.

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
