FROM ubuntu:22.04 AS app-builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake libsystemd-dev pkg-config pkg-config git wget ca-certificates \
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev \
    libxi-dev libxkbcommon-dev libdrm-dev libgbm-dev \
    libgl1-mesa-dev libgles2-mesa-dev libegl1-mesa-dev libglvnd-dev \
    libdbus-1-dev libudev-dev \
    libwayland-dev libdecor-0-dev liburing-dev zlib1g-dev xxd \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# Build SDL3
RUN git clone --depth 1 --branch release-3.4.14 https://github.com/libsdl-org/SDL.git && \
    cd SDL && \
    cmake -S . -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DSDL_AUDIO=OFF \
        -DSDL_JOYSTICK=OFF \
        -DSDL_HAPTIC=OFF \
        -DSDL_HIDAPI=OFF \
        -DSDL_POWER=OFF \
        -DSDL_SENSOR=OFF \
        -DSDL_CAMERA=OFF \
        -DSDL_X11_XSCRNSAVER=OFF \
        -DSDL_X11_XTEST=OFF \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DSDL_SHARED=ON \
        -DSDL_STATIC=OFF && \
    cmake --build build -j$(nproc) && \
    cmake --install build && \
    cd .. && rm -rf SDL

# Build SDBUS-CPP
RUN git clone --depth 1 --branch v2.3.1 https://github.com/Kistler-Group/sdbus-cpp.git && \
    cd sdbus-cpp && mkdir build && \
    cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DBUILD_SHARED_LIBS=OFF \
    -DSDBUSCPP_BUILD_LIBSYSTEMD=OFF \
    -DSDBUSCPP_BUILD_DOCS=OFF && \
    cmake --build build -j$(nproc) && \
    cmake --install build && \
    cd .. && rm -rf sdbus-cpp

RUN ldconfig

COPY . /app
WORKDIR /app

# Build the app and install to AppDir
RUN mkdir -p /app/build && rm -rf /app/build/* && \
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr && \
    cmake --build build -j$(nproc) && \
    DESTDIR=/app/AppDir cmake --install build

FROM ubuntu:22.04 AS appimage-builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    wget file desktop-file-utils libglib2.0-0 ca-certificates curl \
    libx11-6 libxext6 libxrandr2 libxcursor1 libxfixes3 \
    libxi6 libxkbcommon0 libdrm2 libgbm1 \
    libgl1-mesa-glx libegl1-mesa libglvnd0 \
    libwayland-client0 libwayland-egl1 libdecor-0-0 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy AppDir
COPY --from=app-builder /app/AppDir /app/AppDir

# Copy built SDL3 libraries to system path
COPY --from=app-builder /usr/local/lib/ /usr/local/lib/
RUN ldconfig

# Download linuxdeploy and appimagetool manually
RUN wget --no-check-certificate https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage && \
    chmod +x linuxdeploy-x86_64.AppImage && \
    wget --no-check-certificate https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-x86_64.AppImage && \
    chmod +x appimagetool-x86_64.AppImage && \
    wget --no-check-certificate https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-x86_64 && \
    chmod +x runtime-x86_64

COPY assets/zoomer.png /app/zoomer.png

# Create desktop file manually in the right place
RUN mkdir -p /app/AppDir/usr/share/applications && \
    printf "[Desktop Entry]\nType=Application\nName=Zoomer\nExec=zoomer\nIcon=zoomer\nCategories=Utility;\nTerminal=false\n" > /app/AppDir/usr/share/applications/zoomer.desktop

# Copy icon to the right place
RUN mkdir -p /app/AppDir/usr/share/icons/hicolor/96x96/apps/ && \
    cp /app/zoomer.png /app/AppDir/usr/share/icons/hicolor/96x96/apps/zoomer.png && \
    cp /app/zoomer.png /app/AppDir/zoomer.png

ENV ARCH=x86_64
ENV APPIMAGE_EXTRACT_AND_RUN=1

# 1. Use linuxdeploy ONLY to deploy dependencies (no output plugin)
RUN ./linuxdeploy-x86_64.AppImage --appdir /app/AppDir --executable /app/AppDir/usr/bin/zoomer

# 2. Manually run appimagetool with pre-downloaded runtime
RUN ./appimagetool-x86_64.AppImage --runtime-file runtime-x86_64 /app/AppDir Zoomer-x86_64.AppImage

# output AppImage build
CMD ["sh", "-c", "cp Zoomer*.AppImage /out/"]
