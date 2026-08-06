#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

enum class Session { X11, Wayland, Unknown };

Session detect_session() {
    const char* s = getenv("XDG_SESSION_TYPE");
    if (s) {
        if (!strcmp(s, "wayland")) return Session::Wayland;
        if (!strcmp(s, "x11")) return Session::X11;
    }
    if (getenv("WAYLAND_DISPLAY")) return Session::Wayland;
    if (getenv("DISPLAY")) return Session::X11;

    return Session::Unknown;
}

SDL_Surface* capture_x11() {
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        SDL_Log("ERROR: X11 Could not open display");
        return nullptr;
    }

    auto old_handler =
        XSetErrorHandler([](Display*, XErrorEvent*) -> int { return 0; });

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);
    int width = DisplayWidth(dpy, screen);
    int height = DisplayHeight(dpy, screen);

    // Get all pixels
    XImage* xi = XGetImage(dpy, root, 0, 0, width, height, AllPlanes, ZPixmap);

    XSetErrorHandler(old_handler);

    if (!xi) {
        XCloseDisplay(dpy);
        SDL_Log("ERROR: X11 XGetImage failed");
        return nullptr;
    }

    if (xi->bits_per_pixel != 32 && xi->bits_per_pixel != 24) {
        SDL_Log("ERROR: X11 unsupported color depth: %d bpp",
                xi->bits_per_pixel);
        XDestroyImage(xi);
        XCloseDisplay(dpy);
        return nullptr;
    }

    SDL_Surface* surf =
        SDL_CreateSurface(xi->width, xi->height, SDL_PIXELFORMAT_ARGB8888);
    if (!surf) {
        XDestroyImage(xi);
        XCloseDisplay(dpy);
        return nullptr;
    }

    auto shift = [](uint32_t m) {
        int s = 0;
        while (m && !(m & 1)) {
            m >>= 1;
            ++s;
        }
        return s;
    };
    const int rs = shift(xi->red_mask);
    const int gs = shift(xi->green_mask);
    const int bs = shift(xi->blue_mask);

    SDL_LockSurface(surf);
    for (int y = 0; y < xi->height; ++y) {
        const uint8_t* src_row =
            reinterpret_cast<const uint8_t*>(xi->data) + y * xi->bytes_per_line;
        uint32_t* dst_row = reinterpret_cast<uint32_t*>(
            static_cast<uint8_t*>(surf->pixels) + y * surf->pitch);
        for (int x = 0; x < xi->width; ++x) {
            uint32_t c;
            if (xi->bits_per_pixel == 32) {
                c = reinterpret_cast<const uint32_t*>(src_row)[x];
            } else {
                c = src_row[3 * x] | (src_row[3 * x + 1] << 8) |
                    (src_row[3 * x + 2] << 16);
            }
            uint32_t r = (c >> rs) & 0xFF;
            uint32_t g = (c >> gs) & 0xFF;
            uint32_t b = (c >> bs) & 0xFF;
            dst_row[x] = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
    }
    SDL_UnlockSurface(surf);

    XDestroyImage(xi);
    XCloseDisplay(dpy);
    SDL_Log("X11 capture successful (%dx%d)", surf->w, surf->h);
    return surf;
}

SDL_Surface* capture_wayland() {
    const std::string temp_file = "/tmp/zoomer_screenshot.png";
    std::remove(temp_file.c_str());

    // Commands to attempt for Wayland capture
    const std::string commands[] = {
        "grim " + temp_file + " >/dev/null 2>&1",
        "hyprshot -m output -o /tmp -f zoomer_screenshot.png >/dev/null 2>&1",
        "spectacle -b -n -o " + temp_file + " >/dev/null 2>&1",
        "gnome-screenshot -f " + temp_file + " >/dev/null 2>&1"};

    for (const auto& cmd : commands) {
        int res = std::system(cmd.c_str());
        if (res == 0 && std::filesystem::exists(temp_file)) {
            SDL_Surface* surf = IMG_Load(temp_file.c_str());
            std::remove(temp_file.c_str());
            if (surf) {
                SDL_Log("Wayland capture successful via command");
                return surf;
            }
        }
    }

    SDL_Log("Wayland: native capture commands failed");
    return nullptr;
}

SDL_Surface* capture_screenshot() {
    Session session = detect_session();
    SDL_Surface* surf = nullptr;

    if (session == Session::Wayland) {
        SDL_Log("Attempting Wayland screen capture...");
        surf = capture_wayland();
        if (!surf) {
            SDL_Log(
                "Wayland capture failed, attempting X11 (Xwayland) "
                "fallback...");
            surf = capture_x11();
        }
    } else if (session == Session::X11) {
        SDL_Log("Attempting X11 screen capture...");
        surf = capture_x11();
        if (!surf) {
            SDL_Log("X11 capture failed, attempting Wayland fallback...");
            surf = capture_wayland();
        }
    } else {
        SDL_Log("Session unknown, trying Wayland capture then X11 capture...");
        surf = capture_wayland();
        if (!surf) surf = capture_x11();
    }

    return surf;
}

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
SDL_Texture* texture = nullptr;
int img_w = 0;
int img_h = 0;

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("ERROR: Could not init SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_Surface* surf = capture_screenshot();
    if (!surf) {
        SDL_Log("ERROR: Could not capture screenshot on X11 or Wayland");
        return SDL_APP_FAILURE;
    }

    img_w = surf->w;
    img_h = surf->h;

    if (!SDL_CreateWindowAndRenderer(
            "zoomer", img_w, img_h, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("ERROR: Could not create window and renderer: %s",
                SDL_GetError());
        SDL_DestroySurface(surf);
        return SDL_APP_FAILURE;
    }

    texture = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_DestroySurface(surf);

    if (!texture) {
        SDL_Log("ERROR: Could not create texture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_Log("Screenshot loaded successfully into SDL3 window (%dx%d)", img_w,
            img_h);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    if (event->type == SDL_EVENT_KEY_DOWN &&
        (event->key.key == SDLK_ESCAPE || event->key.key == SDLK_Q)) {
        return SDL_APP_SUCCESS;
    }
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    if (texture) {
        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
    }

    SDL_RenderPresent(renderer);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
}
