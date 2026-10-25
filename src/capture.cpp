#include "capture.hpp"

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_surface.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sdbus-c++/Error.h>
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/IProxy.h>
#include <sdbus-c++/Types.h>
#include <sdbus-c++/sdbus-c++.h>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <vector>

#include "stb/stb_image.h"

enum class Session { X11, Wayland, Unknown };


Session detect_session();
int sdbus_screenshot(std::string& path_to_file, std::string& reason);
capture_attempt capture_x11(int* out_x = nullptr, int* out_y = nullptr);
capture_attempt capture_wayland(int* out_x = nullptr, int* out_y = nullptr);
capture_attempt capture_wayland_commands(int* out_x, int* out_y);

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

capture_attempt capture_x11(int* out_x, int* out_y) {
    Display* dpy = XOpenDisplay(nullptr);
    std::string reason = "";

    if (!dpy) {
        reason = "Could not open display";
        return {nullptr, reason};
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
    XImage* xi = XGetImage(dpy, root, bounds.x, bounds.y, bounds.w, bounds.h,
                           AllPlanes, ZPixmap);

    XSetErrorHandler(old_handler);

    if (!xi) {
        XCloseDisplay(dpy);
        reason = "X11 XGetImage failed";
        return {nullptr, reason};
    }

    if (xi->bits_per_pixel != 32 && xi->bits_per_pixel != 24) {
        XDestroyImage(xi);
        XCloseDisplay(dpy);
        reason = "X11 unsupported color depth: " + std::to_string(xi->bits_per_pixel) + "bpp";
        return {nullptr, reason};
    }

    SDL_Surface* surf =
        SDL_CreateSurface(xi->width, xi->height, SDL_PIXELFORMAT_ARGB8888);
    if (!surf) {
        XDestroyImage(xi);
        XCloseDisplay(dpy);
        reason = "SDL surface was not created";
        return {nullptr, reason};
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
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "X11 capture successful (%dx%d) at offset (%d,%d)", surf->w, surf->h, bounds.x, bounds.y);
    return {surf, ""};
}

std::unique_ptr<sdbus::IConnection> connection;

std::string uri_to_path(const std::string uri) {
    std::string correct_path;

    // To prevent reallocations
    correct_path.reserve(uri.length());

    for (int i = 0; i < uri.length(); i++) {
        if (uri[i] == '%') {
            char val = std::stoi(uri.substr(i+1, 2), nullptr, 16);
            correct_path += val;
            i += 2;
        } else {
            correct_path += uri[i];
        }
    }

    if (correct_path.rfind("file://", 0) == 0) {
        correct_path = correct_path.substr(7);
    }
    return correct_path;
}

int sdbus_screenshot(std::string& path_to_file, std::string& reason) {
    connection = sdbus::createBusConnection();

    sdbus::ServiceName svc_name{"org.freedesktop.portal.Desktop"};
    sdbus::ObjectPath object_path{"/org/freedesktop/portal/desktop"};

    auto prox = sdbus::createProxy(*connection, svc_name, object_path);

    std::string parent_window = "";

    std::map<std::string, sdbus::Variant> options;
    options["interactive"] = sdbus::Variant(false);

    sdbus::ObjectPath request_path;

    try {
        prox->callMethod("Screenshot")
            .onInterface("org.freedesktop.portal.Screenshot")
            .withArguments(parent_window, options)
            .storeResultsTo(request_path);
    } catch (const sdbus::Error& e) {
        reason = "Calling portal error: " + e.getMessage();
        return 1;
    }

    auto request_monitor =
        sdbus::createProxy(*connection, svc_name, request_path);

    std::promise<std::string> promise;
    auto path_future = promise.get_future();

    request_monitor->uponSignal("Response")
        .onInterface("org.freedesktop.portal.Request")
        .call([&promise](uint32_t response,
                         std::map<std::string, sdbus::Variant> results) {
            if (response == 0) {
                if (results.find("uri") != results.end()) {
                    std::string file_path = results["uri"].get<std::string>();
                    promise.set_value(file_path);
                } else {
                    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "No file path (uri) were observed");
                    promise.set_value("empty_path");
                }
            } else {
                promise.set_value("user_cancelled");
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Looks like user cancelled the request");
            }

            connection->leaveEventLoop();
        });

    connection->enterEventLoop();

    std::string uri = path_future.get();
    if (uri == "empty_path") {
        reason = "file uri path is empty";
        return 1;
    }
    if (uri == "user_cancelled") {
        reason = "user cancelled the screenshot";
        return 1;
    }

    uri = uri_to_path(uri);

    path_to_file = uri;

    return 0;
}

capture_attempt capture_wayland_commands(int* out_x, int* out_y) {
    if (out_x) *out_x = 0;
    if (out_y) *out_y = 0;

    std::string reason = "";

    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path temp_file =
        std::filesystem::temp_directory_path() /
        ("zoomer_screenshot_" + std::to_string(now) + ".png");

    // Commands to attempt for Wayland capture
    const std::string commands[] = {
        "grim \"" + temp_file.string() + "\" >/dev/null 2>&1",
        "hyprshot -m output -o \"" + temp_file.parent_path().string() +
            "\" -f \"" + temp_file.filename().string() + "\" >/dev/null 2>&1",
        "spectacle -b -n -m -o \"" + temp_file.string() + "\" >/dev/null 2>&1",
        "flameshot screen -p \"" + temp_file.string() + "\" >/dev/null 2>&1",
    };

    int width, height, channels;

    for (const auto& cmd : commands) {
        int res = std::system(cmd.c_str());
        if (res == 0 && std::filesystem::exists(temp_file)) {
            unsigned char* data = stbi_load(temp_file.string().c_str(), &width,
                                            &height, &channels, 4);
            SDL_Surface* surf = nullptr;
            if (data) {
                surf = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32);
                if (surf) {
                    memcpy(surf->pixels, data, width * height * 4);
                } else {
                    reason = "SDL could not create surface in wayland_commands";
                }
                stbi_image_free(data);
            } else {
                reason = "Unable to load image data from " + temp_file.string();
            }

            std::filesystem::remove(temp_file);

            if (surf) {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Wayland capture successful via command");
                return {surf, ""};
            }
        }
    }

    if (reason.empty()) {
        reason  = "unable to call screenshot utilities";
    }
    return {nullptr, reason};
}

capture_attempt capture_wayland(int* out_x, int* out_y) {
    if (out_x) *out_x = 0;
    if (out_y) *out_y = 0;

    std::string file_path;
    std::string reason = "";


    if (sdbus_screenshot(file_path, reason) != 0) {
        return {nullptr, reason};
    }

    int width, height, channels;
    unsigned char* data = stbi_load(file_path.c_str(), &width, &height, &channels, 4);

    SDL_Surface* surf = nullptr;
    if (data) {
        surf = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32);
        if (surf) {
            memcpy(surf->pixels, data, width * height * 4);
        } else {
            reason = "SDL could not create surface in capture_wayland";
        }
        stbi_image_free(data);
    } else {
        reason = "Unable to load image data from " + file_path;
    }

    std::filesystem::remove(file_path);
    return {surf, reason};
}

capture_attempt capture_screenshot(int* out_x, int* out_y) {
    Session session = detect_session();
    std::string fld;

    if (session == Session::Wayland) {
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Attempting Wayland screen capture");

        capture_attempt r = capture_wayland(out_x, out_y);

        if (r) {
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[Wayland] screen capture via Dbus successful");
            return r;
        }
        fld += ("wayland: " + r.reason + " | ");

        r = capture_wayland_commands(out_x, out_y);
        if (r) {
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[Wayland] screen capture via commands successful");
            return r;
        }
        fld += ("wayland_commands: " + r.reason + " | ");

        r = capture_x11(out_x, out_y);
        if (r) {
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[Wayland] screen capture via X11 fallback successful");
            return r;
        }
        fld += ("x11: " + r.reason);

        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[Wayland] Screen capture failed: %s", fld.c_str());

        return {nullptr, fld};

    } else if (session == Session::X11) {
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Attempting X11 screen capture");

        capture_attempt r = capture_x11(out_x, out_y);

        if (r) {
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[X11] screen capture successful");
            return r;
        }
        fld += ("X11: " + r.reason + " | ");

        r = capture_wayland(out_x, out_y);
        if (r) {
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[X11] screen capture via dbus successful");
            return r;
        }
        fld += ("wayland: " + r.reason + " | ");

        r = capture_wayland_commands(out_x, out_y);
        if (r) {
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[X11] screen capture via wayland_commands successful");
            return r;
        }
        fld += ("wayland_commands: " + r.reason);

        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[X11] Screen capture failed: %s", fld.c_str());

        return {nullptr, fld};
    } else {
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Session unknown, trying Wayland capture then X11 capture");

        capture_attempt r = capture_wayland(out_x, out_y);
        if (r) {
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[Unknown] Wayland dbus capture successful");
            return r;
        }
        fld += ("wayland: " + r.reason + " | ");

        r = capture_wayland_commands(out_x, out_y);
        if (r) {
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[Unknown] Wayland commands capture successful");
            return r;
        }
        fld += ("wayland_commands: " + r.reason + " | ");

        r = capture_x11(out_x, out_y);
        if (r) {
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[Unknown] X11 native capture successful");
            return r;
        }
        fld += ("X11: " + r.reason);

        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[Unknown] Screen capture failed: %s", fld.c_str());
        return {nullptr, fld};
    }

}
