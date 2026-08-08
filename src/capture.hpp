#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_image/SDL_image.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

enum class Session { X11, Wayland, Unknown };

Session detect_session();
SDL_Surface* capture_x11();
SDL_Surface* capture_wayland();
SDL_Surface* capture_screenshot();
