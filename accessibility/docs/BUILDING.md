# Building from source

The source folder contains the modified Endless Sky 0.11.4 code (C++ sources, tests,
CMake files) and a patch. Game data, images and sounds are unchanged from 0.11.4 and are
not included; take them from https://github.com/endless-sky/endless-sky (tag v0.11.4) or
your installation.

## Windows (official way)

1. git clone https://github.com/endless-sky/endless-sky.git and git checkout v0.11.4
2. git apply endless-sky-accessibility-1.0.16.patch (or copy the source and tests folders
   over the checkout).
3. Follow the project's Windows instructions (docs/readme-cmake.md): install MinGW-w64
   or Visual Studio, CMake and Ninja; vcpkg supplies the libraries.
   cmake --preset mingw   then   cmake --build --preset mingw-debug  (or release)
4. Copy UniversalSpeech.dll and nvdaControllerClient.dll (64-bit) next to
   Endless Sky.exe.

## How this release was built (Linux cross-compile)

- Ubuntu with g++-mingw-w64-x86-64-posix 13.2, CMake 3.31, Ninja.
- Libraries cross-compiled from source: SDL2 2.30.9 (official MinGW binaries), zlib 1.3.1,
  minizip (zlib contrib), libpng 1.6.44, libjpeg-turbo 3.0.4, dav1d 1.5.0,
  libavif 1.1.1, GLEW 2.2.0, OpenAL Soft 1.24.1, FLAC 1.4.3, libmad 0.15.1b.
  See build-scripts/build-deps.sh.
- The Ubuntu MinGW headers lack two Windows 11 DWM constants used by WinWindow.cpp; a
  small toolchain header shim (build-scripts/dwmapi.h) provides them. No game source
  change is needed for this.
- Configure: cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=build-scripts/mingw.cmake
  -DES_USE_VCPKG=OFF -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_CXX_FLAGS="-isystem <shim folder>"

## Linux build and tests

cmake -G Ninja -DES_USE_VCPKG=OFF -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
ninja; ./tests/endless-sky-tests "[accessibility]"
Set ES_ACCESS_STDOUT=1 to print every announcement to standard output.
