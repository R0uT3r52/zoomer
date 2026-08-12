#include "capture.hpp"
#include <SDL3/SDL_surface.h>
#include <SDL3_image/SDL_image.h>
#include <cstdint>
#include <cstdlib>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <memory>

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

SDL_Surface* capture_x11(int* out_x, int* out_y) {
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        SDL_Log("ERROR: X11 Could not open display");
        return nullptr;
    }

    float mouse_x = 0, mouse_y = 0;
    SDL_GetGlobalMouseState(&mouse_x, &mouse_y);

    SDL_Point pt = {static_cast<int>(mouse_x), static_cast<int>(mouse_y)};
    SDL_DisplayID dID = SDL_GetDisplayForPoint(&pt);

    SDL_Rect bounds = {0, 0, 0, 0};
    if (dID != 0) {
        SDL_GetDisplayBounds(dID, &bounds);
    } else {
        int count = 0;
        SDL_DisplayID* displays = SDL_GetDisplays(&count);
        if (displays && count > 0) {
            SDL_GetDisplayBounds(displays[0], &bounds);
        }
        SDL_free(displays);
    }

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);

    if (bounds.w <= 0 || bounds.h <= 0) {
        bounds.w = DisplayWidth(dpy, screen);
        bounds.h = DisplayHeight(dpy, screen);
        bounds.x = 0;
        bounds.y = 0;
    }

    if (out_x) *out_x = bounds.x;
    if (out_y) *out_y = bounds.y;

    auto old_handler =
        XSetErrorHandler([](Display*, XErrorEvent*) -> int { return 0; });

    // Get all pixels
    XImage* xi = XGetImage(dpy, root, bounds.x, bounds.y, bounds.w, bounds.h, AllPlanes, ZPixmap);

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
    SDL_Log("X11 capture successful (%dx%d) at offset (%d,%d)", surf->w, surf->h, bounds.x, bounds.y);
    return surf;
}

std::unique_ptr<sdbus::IConnection> connection;

std::string uri_to_path(const std::string uri) {
    if (uri.rfind("file://", 0) == 0) {
        return uri.substr(7);
    }
    return uri;
}

int sdbus_screenshot(char *path_to_file) {
    connection = sdbus::createBusConnection();

    sdbus::ServiceName svc_name{"org.freedesktop.portal.Desktop"};
    sdbus::ObjectPath object_path{"/org/freedesktop/portal/desktop"};

    auto prox = sdbus::createProxy(*connection, svc_name, object_path);

    std::string parent_window = "";

    std::map<std::string, sdbus::Variant> options;
    options["interactive"] = sdbus::Variant(false);

    sdbus::ObjectPath request_path;

    try {
        prox->callMethod("Screenshot").onInterface("org.freedesktop.portal.Screenshot").withArguments(parent_window, options).storeResultsTo(request_path);
    } catch (const sdbus::Error &e) {
        SDL_Log("ERROR: Calling portal error: %s", e.getMessage().c_str());
        return 1;
    }

    auto request_monitor = sdbus::createProxy(*connection, svc_name, request_path);

    std::promise<std::string> promise;
    auto path_future = promise.get_future();


    request_monitor->uponSignal("Response").onInterface("org.freedesktop.portal.Request").call([&promise](uint32_t response, std::map<std::string, sdbus::Variant> results){
        if (response == 0) {
            if(results.find("uri") != results.end()) {
                std::string file_path = results["uri"].get<std::string>();
                promise.set_value(file_path);
            } else {
                SDL_Log("WARN: No file path (uri) were observed");
            }
        } else {
            promise.set_value("");
            SDL_Log("ERROR: Looks like user cancelled the request");
        }

        connection->leaveEventLoop();
    });

    connection->enterEventLoop();

    std::string uri = path_future.get();
    if (uri.length() == 0) {
        SDL_Log("WARN: file uri path is an empty string");
    }

    uri = uri_to_path(uri);

    strncpy(path_to_file, uri.c_str(), 512);

    return 0;
}

SDL_Surface* capture_wayland_commands(int* out_x, int* out_y) {
    if (out_x) *out_x = 0;
    if (out_y) *out_y = 0;

    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path temp_file = std::filesystem::temp_directory_path() / ("zoomer_screenshot_" + std::to_string(now) + ".png");

    // Commands to attempt for Wayland capture
    const std::string commands[] = {
        "grim \"" + temp_file.string() + "\" >/dev/null 2>&1",
        "hyprshot -m output -o \"" + temp_file.parent_path().string() + "\" -f \"" + temp_file.filename().string() + "\" >/dev/null 2>&1",
        "spectacle -b -n -m -o \"" + temp_file.string() + "\" >/dev/null 2>&1",
        "flameshot screen -p \"" + temp_file.string() + "\" >/dev/null 2>&1",
    };

    for (const auto& cmd : commands) {
        int res = std::system(cmd.c_str());
        if (res == 0 && std::filesystem::exists(temp_file)) {
            SDL_Surface* surf = IMG_Load(temp_file.string().c_str());
            std::filesystem::remove(temp_file);
            if (surf) {
                SDL_Log("Wayland capture successful via command");
                return surf;
            }
        }
    }

    SDL_Log("Wayland: native capture commands failed");
    return nullptr;
}

SDL_Surface* capture_wayland(int* out_x, int* out_y) {
    if (out_x) *out_x = 0;
    if (out_y) *out_y = 0;

    char file_path[512];

    if(sdbus_screenshot(file_path) != 0) {
        SDL_Log("SDBUS Screenshot was unable to make screenshot");
        return nullptr;
    }

    SDL_Surface *surf = IMG_Load(file_path);
    if(!surf) {
        SDL_Log("ERROR: SDL Could not open image made by SDBUS screenshot method");
        return nullptr;
    }

    std::filesystem::remove(file_path);
    return surf;
}

SDL_Surface* capture_screenshot(int* out_x, int* out_y) {
    Session session = detect_session();
    SDL_Surface* surf = nullptr;

    if (session == Session::Wayland) {
        SDL_Log("Attempting Wayland screen capture via Dbus...");
        surf = capture_wayland(out_x, out_y);
        if (!surf) {
            SDL_Log("Wayland capture via Dbus failed, attempting CLI tools fallback...");
            surf = capture_wayland_commands(out_x, out_y);
        }
        if(!surf) {
            SDL_Log("Wayland capture failed, attempting X11 fallback");
            surf = capture_x11(out_x, out_y);
        }
    } else if (session == Session::X11) {
        SDL_Log("Attempting X11 screen capture...");
        surf = capture_x11(out_x, out_y);
        if (!surf) {
            SDL_Log("X11 capture failed, attempting Wayland Dbus fallback...");
            surf = capture_wayland(out_x, out_y);
        }
        if (!surf) {
            SDL_Log("X11 capture failed, attempting Wayland commands fallback...");
            surf = capture_wayland_commands(out_x, out_y);
        }
    } else {
        SDL_Log("Session unknown, trying Wayland capture then X11 capture...");
        surf = capture_wayland(out_x, out_y);
        if(!surf) surf = capture_wayland_commands(out_x, out_y);
        if (!surf) surf = capture_x11(out_x, out_y);
    }

    return surf;
}
