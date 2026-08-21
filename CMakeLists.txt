cmake_minimum_required(VERSION 3.21)
project(zoomer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SHADER_VERT "${CMAKE_CURRENT_SOURCE_DIR}/shaders/vert.glsl")
set(SHADER_FRAG "${CMAKE_CURRENT_SOURCE_DIR}/shaders/frag.glsl")

set(GEN_SHADER_VERT "${CMAKE_CURRENT_BINARY_DIR}/shader_vertex.hpp")
set(GEN_SHADER_FRAG "${CMAKE_CURRENT_BINARY_DIR}/shader_fragment.hpp")


add_executable(embedder tools/embedder.cpp)

add_custom_command(
    OUTPUT ${GEN_SHADER_VERT}
    COMMAND embedder ${SHADER_VERT} ${GEN_SHADER_VERT}
    DEPENDS embedder ${SHADER_VERT}
    COMMENT "Baking VERT Shader"
)

add_custom_command(
    OUTPUT ${GEN_SHADER_FRAG}
    COMMAND embedder ${SHADER_FRAG} ${GEN_SHADER_FRAG}
    DEPENDS embedder ${SHADER_FRAG}
    COMMENT "Baking FRAG Shader"
)

find_package(SDL3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

add_executable(zoomer
    src/main.cpp
    src/shader.cpp
    src/glad.c
    ${GEN_SHADER_VERT}
    ${GEN_SHADER_FRAG}
)

target_include_directories(zoomer PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    include
)

target_link_libraries(zoomer PRIVATE
    SDL3::SDL3
    OpenGL::GL
    Threads::Threads
)

if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(X11 REQUIRED x11)
    pkg_check_modules(Systemd REQUIRED IMPORTED_TARGET libsystemd)
    find_package(sdbus-c++ REQUIRED)

    target_sources(zoomer PRIVATE src/capture.cpp)
    target_include_directories(zoomer PRIVATE ${X11_INCLUDE_DIRS})
    target_link_libraries(zoomer PRIVATE
        ${X11_LIBRARIES}
        SDBusCpp::sdbus-c++
        PkgConfig::Systemd
    )
elseif(WIN32)
    target_sources(zoomer PRIVATE src/capture_win.cpp)
    # Windows libs for screen capture
endif()

install(TARGETS zoomer DESTINATION bin)
