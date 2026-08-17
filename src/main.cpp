#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_oldnames.h>
#include <SDL3/SDL_surface.h>

#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_video.h>

#include "capture.hpp"
#include "shader.hpp"
#include "utils.hpp"
#include "shader_vertex.hpp"
#include "shader_fragment.hpp"

struct app {
    SDL_Window* window = nullptr;
    SDL_GLContext gl_ctx = nullptr;
    int img_w = 0, img_h = 0;
    unsigned int program = 0, VAO = 0, VBO = 0, texture = 0;
    float dt = 0.0f, ref_rate = 0.0f;
    bool is_resetting = false;
    camera cam;
    cursor curs;

    // OpenGL uniform IDs
    int cameraPosGLLocation = -1;
    int cameraScaleGLLocation = -1;
    int windowSizeGLLocation = -1;
    int screenshotSizeGLLocation = -1;
    int cursorPosGLLocation = -1;
    int textureSizeGLLocation = -1;
    int currentZoomGLLocation = -1;
};

float vertices[] = {
    // Coordinates          // Colors (RGB)       // Texture
    -1.0f,  1.0f, 0.0f,     0.0f, 0.0f, 0.0f,     0.0f, 0.0f,
    -1.0f, -1.0f, 0.0f,     0.0f, 0.0f, 0.0f,     0.0f, 1.0f,
     1.0f,  1.0f, 0.0f,     0.0f, 0.0f, 0.0f,     1.0f, 0.0f,

    -1.0f, -1.0f, 0.0f,     0.0f, 0.0f, 0.0f,     0.0f, 1.0f,
     1.0f,  1.0f, 0.0f,     0.0f, 0.0f, 0.0f,     1.0f, 0.0f,
     1.0f, -1.0f, 0.0f,     0.0f, 0.0f, 0.0f,     1.0f, 1.0f,
};

int init_opengl_state(app* state) {
    // unsigned int VAO;
    glGenVertexArrays(1, &state->VAO);
    glBindVertexArray(state->VAO);

    // unsigned int VBO;
    glGenBuffers(1, &state->VBO);
    glBindBuffer(GL_ARRAY_BUFFER, state->VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (GLvoid*)(0));
    glEnableVertexAttribArray(0);

    // color
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (GLvoid*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // texture
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (GLvoid*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    char shader_logs[512];

    state->program = load_shader_program((const char*)vert_glsl, vert_glsl_len, (const char*)frag_glsl, frag_glsl_len, shader_logs);

    if (!state->program) {
        SDL_Log(
            "ERROR: Compiling shaders or linking shader program ended with "
            "error: %s",
            shader_logs);
        return 0;
    }

    state->cameraPosGLLocation = glGetUniformLocation(state->program, "cameraPos");
    state->cameraScaleGLLocation = glGetUniformLocation(state->program, "cameraScale");
    state->windowSizeGLLocation = glGetUniformLocation(state->program, "windowSize");
    state->screenshotSizeGLLocation = glGetUniformLocation(state->program, "screenshotSize");
    state->cursorPosGLLocation = glGetUniformLocation(state->program, "cursorPos");
    state->textureSizeGLLocation = glGetUniformLocation(state->program, "textureSize");
    state->currentZoomGLLocation = glGetUniformLocation(state->program, "currentZoom");

    glGenTextures(1, &state->texture);
    glBindTexture(GL_TEXTURE_2D, state->texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
    GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,
    GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    return 1;
}


SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {

    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("ERROR: Could not init SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);

    app* state = new app;
    *appstate = state;

    int screen_x = 0, screen_y = 0;
    SDL_Surface* surf = capture_screenshot(&screen_x, &screen_y);
    if (!surf) {
        SDL_Log("ERROR: Could not capture screenshot on X11 or Wayland");
        return SDL_APP_FAILURE;
    }

    state->img_w = surf->w;
    state->img_h = surf->h;

    state->window = SDL_CreateWindow(
        "zoomer", state->img_w, state->img_h,
        SDL_WINDOW_FULLSCREEN | SDL_WINDOW_BORDERLESS | SDL_WINDOW_OPENGL);
    if (!state->window) {
        SDL_Log("ERROR: Could not create a window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_SetWindowPosition(state->window, screen_x, screen_y);

    state->gl_ctx = SDL_GL_CreateContext(state->window);
    if (!state->gl_ctx) {
        SDL_Log("ERROR: Could not create GL context: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_GL_MakeCurrent(state->window, state->gl_ctx)) {
        SDL_Log("ERROR: GL_MakeCurrent exited with error: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GL_SetSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        SDL_Log("ERROR: Failed to init glad");
        return SDL_APP_FAILURE;
    }

    SDL_DisplayID dID = SDL_GetDisplayForWindow(state->window);
    const SDL_DisplayMode *mode = SDL_GetDesktopDisplayMode(dID);

    float refresh_rate = (mode && mode->refresh_rate > 0.0f) ? mode->refresh_rate : 60.0f;
    state->dt = 1.0 / refresh_rate;
    state->ref_rate = refresh_rate;

    // I dont know why, but SDL's ABGR8 = GL_RGBA8
    SDL_Surface* converted_surf =
        SDL_ConvertSurface(surf, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(surf);

    if (!converted_surf) {
        SDL_Log("ERROR: Could not convert surface: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!init_opengl_state(state)) {
        SDL_DestroySurface(converted_surf);
        return SDL_APP_FAILURE;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, state->img_w, state->img_h, 0, GL_RGBA,
    GL_UNSIGNED_BYTE, converted_surf->pixels);

    SDL_DestroySurface(converted_surf);

    SDL_Log("Screenshot loaded successfully into SDL3 window (%dx%d)",
            state->img_w, state->img_h);


    *appstate = state;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {

    app *state = static_cast<app*>(appstate);

    if (event->type == SDL_EVENT_KEY_DOWN &&
        (event->key.key == SDLK_ESCAPE || event->key.key == SDLK_Q)) {
        return SDL_APP_SUCCESS;
    }
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }
    if(event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_R) {
        // Smoothly reset state
        state->is_resetting = true;
        state->cam.dScale = 0.0f;
        state->cam.Vel = Vec2();
    }
    if (event->type == SDL_EVENT_MOUSE_WHEEL) {
        // SDL_Log("LOG: Scroll x: %f, Scroll y: %f", event->wheel.x, event->wheel.y);
        state->is_resetting = false;

        // wheel.y is already handling UP/DOWN with negative/positive value
        state->cam.dScale += event->wheel.y;
        state->cam.scalePivot = state->curs.Cur;
    }
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_LEFT) {
        state->is_resetting = false;
        state->curs.drag = true;
    }
    if (event->type == SDL_EVENT_MOUSE_BUTTON_UP && event->button.button == SDL_BUTTON_LEFT) {
        state->is_resetting = false;
        state->curs.drag = false;
    }
    if (event->type == SDL_EVENT_MOUSE_MOTION) {
        // SDL_Log("LOG: posX: %f, posY: %f", event->motion.x, event->motion.y);
        state->curs.Cur = Vec2(event->motion.x, event->motion.y);

        if (state->curs.drag) {
            Vec2 delta = world(state->cam, state->curs.Prev) - world(state->cam, state->curs.Cur);
            state->cam.Pos += delta;
            state->cam.Vel = delta * state->ref_rate;
        }

        state->curs.Prev = state->curs.Cur;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    app* state = static_cast<app*>(appstate);

    // trying to fix zoom on X11 while built with vcpkg
    static Uint64 last_time = 0;
    Uint64 now = SDL_GetTicksNS();
    float dt = (last_time > 0) ? static_cast<float>(now - last_time) / 1.0e9f : state->dt;
    last_time = now;
    if (dt > 0.05f) dt = 0.05f;

    state->cam.update(dt, state->curs, Vec2(state->img_w, state->img_h), state->is_resetting);

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(state->program);

    glUniform2f(state->cameraPosGLLocation, state->cam.Pos.x, state->cam.Pos.y);
    glUniform1f(state->cameraScaleGLLocation, state->cam.Scale);
    // window size
    // TODO: Add separate window size and screenshot size (now its the same)
    glUniform2f(state->windowSizeGLLocation, state->img_w, state->img_h);
    glUniform2f(state->screenshotSizeGLLocation, state->img_w, state->img_h);
    glUniform2f(state->cursorPosGLLocation, state->curs.Cur.x, state->curs.Cur.y);
    glUniform2f(state->textureSizeGLLocation, state->img_w, state->img_h);
    glUniform1f(state->currentZoomGLLocation, state->cam.Scale);

    glBindTexture(GL_TEXTURE_2D, state->texture);
    glBindVertexArray(state->VAO);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    SDL_GL_SwapWindow(state->window);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    app *state = static_cast<app*>(appstate);
    if (!state) {
        SDL_Quit();
        return;
    }

    if (state->texture) glDeleteTextures(1, &state->texture);
    if (state->program) glDeleteProgram(state->program);
    if (state->VBO) glDeleteBuffers(1, &state->VBO);
    if (state->VAO) glDeleteVertexArrays(1, &state->VAO);

    if (state->gl_ctx) SDL_GL_DestroyContext(state->gl_ctx);
    if (state->window) SDL_DestroyWindow(state->window);

    delete state;
    SDL_Quit();
}
