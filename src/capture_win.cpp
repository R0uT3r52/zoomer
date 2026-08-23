#include "capture.hpp"

// windows headers
#ifdef _WIN32
#include <windows.h>
#endif

SDL_Surface* capture_gdi(int* out_x, int* out_y) {
    if (out_x) *out_x = 0;
    if (out_y) *out_y = 0;

    SDL_Surface* surf = nullptr;

    HDC hdc = GetDC(NULL);
    HDC hDest = CreateCompatibleDC(hdc);

    int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);

    // create a bitmap
    HBITMAP hbDesktop = CreateCompatibleBitmap(hdc, width, height);

    SelectObject(hDest, hbDesktop);

    // copy from the desktop device context to the bitmap device context
    // call this once per 'frame'
    BitBlt(hDest, 0, 0, width, height, hdc, 0, 0, SRCCOPY);

    BITMAP bm;
    GetObject(hbDesktop, sizeof(bm), &bm);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bm.bmWidth;
    bmi.bmiHeader.biHeight = -bm.bmHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    surf = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_ARGB8888);

    if (!surf) {
        SDL_Log("ERROR: Unable to create surface in windows capture: %s",
                SDL_GetError());
        ReleaseDC(NULL, hdc);
        DeleteDC(hDest);
        DeleteObject(hbDesktop);
        return nullptr;
    }

    SDL_LockSurface(surf);

    // get pixel data from the HBITMAP into our buffer
    if (!GetDIBits(hdc, hbDesktop, 0, bm.bmHeight, surf->pixels, &bmi,
                   DIB_RGB_COLORS)) {
        SDL_Log("ERROR: Unable to capture screenshot in capture windows");

        SDL_UnlockSurface(surf);
        SDL_DestroySurface(surf);

        ReleaseDC(NULL, hdc);
        DeleteDC(hDest);
        DeleteObject(hbDesktop);
        return nullptr;
    }

    SDL_UnlockSurface(surf);

    // release the desktop context
    // and delete the context
    ReleaseDC(NULL, hdc);
    DeleteDC(hDest);
    DeleteObject(hbDesktop);

    return surf;
}

SDL_Surface* capture_screenshot(int* out_x, int* out_y) {
    // windows implementation

    SDL_Surface* result = capture_gdi(out_x, out_y);
    if (!result) {
        SDL_Log("ERROR: Could not capture screenshot with GDI");
        return nullptr;
    }

    return result;
}
