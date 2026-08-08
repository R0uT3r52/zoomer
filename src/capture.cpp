#include "capture.hpp"

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
