#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_surface.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

struct capture_attempt{
  SDL_Surface *surf;
  std::string reason;
  explicit operator bool() {return surf != nullptr;}
};
capture_attempt capture_screenshot(int* out_x = nullptr, int* out_y = nullptr);
std::string uri_to_path(const std::string uri);
