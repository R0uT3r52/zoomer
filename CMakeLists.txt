cmake_minimum_required(VERSION 3.10)
project(zoomer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED sdl3)
pkg_check_modules(SDL3_IMAGE REQUIRED sdl3-image)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(GL REQUIRED gl)

add_executable(zoomer src/main.cpp src/glad.c)

target_include_directories(zoomer PRIVATE
    ${SDL3_INCLUDE_DIRS}
    ${SDL3_IMAGE_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}
    ${GL_INCLUDE_DIRS}
    src/include
)

target_link_libraries(zoomer PRIVATE
    ${SDL3_LIBRARIES}
    ${SDL3_IMAGE_LIBRARIES}
    ${X11_LIBRARIES}
    ${GL_LIBRARIES}
)
