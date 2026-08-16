FROM ubuntu:22.04 AS app-builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build git curl zip unzip tar pkg-config ca-certificates \
    autoconf autoconf-archive automake libtool libltdl-dev \
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev \
    libxft-dev libibus-1.0-dev \
    xxd python3 python3-venv \
    libxkbcommon-dev libwayland-dev libgl1-mesa-dev libegl1-mesa-dev \
    libdbus-1-dev libsystemd-dev \
    && rm -rf /var/lib/apt/lists/*

# Install vcpkg
ENV VCPKG_ROOT=/opt/vcpkg
RUN git clone https://github.com/microsoft/vcpkg.git /opt/vcpkg && \
    /opt/vcpkg/bootstrap-vcpkg.sh -disableMetrics

WORKDIR /app

# Copy first for layer caching
COPY vcpkg.json vcpkg-configuration.json CMakePresets.json /app/

# Pre-install vcpkg dependencies into /app/build/vcpkg_installed
RUN /opt/vcpkg/vcpkg install \
    --triplet x64-linux \
    --x-manifest-root=/app \
    --x-install-root=/app/build/vcpkg_installed

# Copy application source
COPY . /app

# Build and install the app
RUN cmake --preset default -DCMAKE_INSTALL_PREFIX=/usr && \
    cmake --build build -j$(nproc) && \
    DESTDIR=/app/AppDir cmake --install build

FROM ubuntu:22.04 AS appimage-builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    wget file desktop-file-utils libglib2.0-0 ca-certificates curl \
    libx11-6 libxext6 libxrandr2 libxcursor1 libxfixes3 \
    libxi6 libxss1 libxtst6 libxkbcommon0 libdrm2 libgbm1 \
    libgl1-mesa-glx libegl1-mesa libglvnd0 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy AppDir
COPY --from=app-builder /app/AppDir /app/AppDir

# Download linuxdeploy and appimagetool
RUN wget --no-check-certificate https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage && \
    chmod +x linuxdeploy-x86_64.AppImage && \
    wget --no-check-certificate https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage && \
    chmod +x appimagetool-x86_64.AppImage && \
    wget --no-check-certificate https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-x86_64 && \
    chmod +x runtime-x86_64

COPY assets/zoomer.png /app/zoomer.png

# Create desktop file
RUN mkdir -p /app/AppDir/usr/share/applications && \
    printf "[Desktop Entry]\nType=Application\nName=Zoomer\nExec=zoomer\nIcon=zoomer\nCategories=Utility;\nTerminal=false\n" > /app/AppDir/usr/share/applications/zoomer.desktop

# Copy icon
RUN mkdir -p /app/AppDir/usr/share/icons/hicolor/96x96/apps/ && \
    cp /app/zoomer.png /app/AppDir/usr/share/icons/hicolor/96x96/apps/zoomer.png && \
    cp /app/zoomer.png /app/AppDir/zoomer.png

ENV ARCH=x86_64
ENV APPIMAGE_EXTRACT_AND_RUN=1

# Use linuxdeploy to bundle dependencies
RUN ./linuxdeploy-x86_64.AppImage --appdir /app/AppDir --executable /app/AppDir/usr/bin/zoomer

# Generate AppImage
RUN ./appimagetool-x86_64.AppImage --runtime-file runtime-x86_64 /app/AppDir Zoomer-x86_64.AppImage

# Output AppImage build
CMD ["sh", "-c", "cp Zoomer*.AppImage /out/"]
