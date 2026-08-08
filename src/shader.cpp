#include "shader.hpp"

GLuint load_shader_program(const char* vert_shader, const char* frag_shader,
                           const char* logs) {
    unsigned int vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    int success;
    char log[512];

    glShaderSource(vertex_shader, 1, &vert_shader, nullptr);
    glCompileShader(vertex_shader);

    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertex_shader, 512, nullptr, log);
        logs = log;
        return 0;
    }

    unsigned int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(fragment_shader, 1, &frag_shader, nullptr);
    glCompileShader(fragment_shader);

    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragment_shader, 512, nullptr, log);
        logs = log;
        return 0;
    }

    unsigned int program = glCreateProgram();

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, nullptr, log);
        logs = log;
        return 0;
    }

    return program;
}
