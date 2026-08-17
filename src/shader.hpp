#pragma once

#include <glad/glad.h>

GLuint load_shader_program(const char* vert_shader, int vert_len,
                           const char* frag_shader, int frag_len, char* logs);
