#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_video.h>

#include "capture.hpp"
#include "shader.hpp"

struct app {
    SDL_Window* window;
    int img_w, img_h;
    unsigned int program, VAO, VBO;
};

float vertices[] = {
    // Coordinates          // Colors (RGB)
    -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.5f, -0.5f, 0.0f,
    1.0f,  0.0f,  1.0f, 0.0f, 0.5f, 0.0f, 1.0f, 0.0f,  1.0f};

const char* VERTSHADER =
    "#version 330 core\n"
    "layout (location=0) in vec3 vertexPos;\n"
    "layout (location=1) in vec3 vertexCol;\n"
    "out vec3 col;\n"
    "void main() {\n"
    "gl_Position = vec4(vertexPos.xyz, 1.0);\n"
    "col = vertexCol;\n}";
const char* FRAGSHADER =
    "#version 330 core\n"
    "in vec3 col;\n"
    "out vec4 fragCol;\n"
    "void main() {\n"
    "fragCol = vec4(col, 1.0f);\n}";

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("ERROR: Could not init SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);

    app* state = new app;

    SDL_Surface* surf = capture_screenshot();
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

    SDL_GLContext gl_ctx = SDL_GL_CreateContext(state->window);

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        SDL_Log("ERROR: Failed to init glad");
        return SDL_APP_FAILURE;
    }

    // GLenum err = glewInit();
    // if (GLEW_OK != err) {
    //     SDL_Log("ERROR: glewInit exited with error: %s",
    //     glewGetErrorString(err)); return SDL_APP_FAILURE;
    // }

    SDL_Surface* converted_surf =
        SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA8888);

    // unsigned int VAO;
    glGenVertexArrays(1, &state->VAO);
    glBindVertexArray(state->VAO);

    // unsigned int VBO;
    glGenBuffers(1, &state->VBO);
    glBindBuffer(GL_ARRAY_BUFFER, state->VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          (GLvoid*)(0));
    glEnableVertexAttribArray(0);

    // color
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          (GLvoid*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    char shader_logs[512];

    state->program = load_shader_program(VERTSHADER, FRAGSHADER, shader_logs);

    if (!state->program) {
        SDL_Log(
            "ERROR: Compiling shaders or linking shader program ended with "
            "error: %s",
            shader_logs);
        return SDL_APP_FAILURE;
    }

    // GLuint *textr;

    // glGenTextures(1, textr);
    // glBindTexture(GL_TEXTURE_2D, *textr);

    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
    // GL_LINEAR_MIPMAP_LINEAR); glTexParameteri(GL_TEXTURE_2D,
    // GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, img_w, img_h, 0, GL_RGBA8,
    // GL_UNSIGNED_BYTE, converted_surf->pixels);

    if (!SDL_GL_MakeCurrent(state->window, gl_ctx)) {
        SDL_Log("ERROR: GL_MakeCurrent exited with error: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_Log("Screenshot loaded successfully into SDL3 window (%dx%d)",
            state->img_w, state->img_h);

    *appstate = state;
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
    app* state = static_cast<app*>(appstate);

    glClearColor(0.0f, 0.0f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(state->program);
    glBindVertexArray(state->VAO);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    SDL_GL_SwapWindow(state->window);

    // SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    // SDL_RenderClear(renderer);

    // if (texture) {
    //     SDL_RenderTexture(renderer, texture, nullptr, nullptr);
    // }

    // SDL_RenderPresent(renderer);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {}
