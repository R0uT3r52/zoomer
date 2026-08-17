#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_surface.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sdbus-c++/sdbus-c++.h>
#include <SDL3/SDL_log.h>
#include <sdbus-c++/Error.h>
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/Types.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

enum class Session { X11, Wayland, Unknown };

Session detect_session();
SDL_Surface* capture_x11(int* out_x = nullptr, int* out_y = nullptr);
SDL_Surface* capture_wayland(int* out_x = nullptr, int* out_y = nullptr);
SDL_Surface* capture_screenshot(int* out_x = nullptr, int* out_y = nullptr);
std::string uri_to_path(const std::string uri);
