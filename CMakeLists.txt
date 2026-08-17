cmake_minimum_required(VERSION 3.21)
project(zoomer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SHADER_VERT "${CMAKE_CURRENT_SOURCE_DIR}/shaders/vert.glsl")
set(SHADER_FRAG "${CMAKE_CURRENT_SOURCE_DIR}/shaders/frag.glsl")

set(GEN_SHADER_VERT "${CMAKE_CURRENT_BINARY_DIR}/shader_vertex.hpp")
set(GEN_SHADER_FRAG "${CMAKE_CURRENT_BINARY_DIR}/shader_fragment.hpp")

set(OpenGL_GL_PREFERENCE GLVND)

add_custom_command(
    OUTPUT ${GEN_SHADER_VERT}
    COMMAND xxd -i vert.glsl > ${GEN_SHADER_VERT}
    DEPENDS ${SHADER_VERT}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
    COMMENT "Baking VERT Shader"
)

add_custom_command(
    OUTPUT ${GEN_SHADER_FRAG}
    COMMAND xxd -i frag.glsl > ${GEN_SHADER_FRAG}
    DEPENDS ${SHADER_FRAG}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
    COMMENT "Baking FRAG Shader"
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED sdl3)
pkg_check_modules(SDL3_IMAGE REQUIRED sdl3-image)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(GL REQUIRED gl)
pkg_check_modules(Systemd REQUIRED IMPORTED_TARGET libsystemd)

find_package(sdbus-c++ REQUIRED)
find_package(Threads REQUIRED)

add_executable(zoomer src/main.cpp src/shader.cpp src/capture.cpp src/glad.c ${GEN_SHADER_VERT} ${GEN_SHADER_FRAG})

target_include_directories(zoomer PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${SDL3_INCLUDE_DIRS}
    ${SDL3_IMAGE_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}
    ${GL_INCLUDE_DIRS}
    include
)

target_link_libraries(zoomer PRIVATE
    ${SDL3_LIBRARIES}
    ${SDL3_IMAGE_LIBRARIES}
    ${X11_LIBRARIES}
    ${GL_LIBRARIES}
    SDBusCpp::sdbus-c++
    PkgConfig::Systemd
    Threads::Threads
)

install(TARGETS zoomer DESTINATION bin)
