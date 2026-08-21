#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_surface.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

SDL_Surface* capture_screenshot(int* out_x = nullptr, int* out_y = nullptr);
std::string uri_to_path(const std::string uri);
