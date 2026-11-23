#!/bin/bash
set -e
P=/opt/mw
S=/home/claude/deps-src
TC=/home/claude/build-scripts/mingw.cmake
CM="cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=$TC -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$P -DCMAKE_PREFIX_PATH=$P -DCMAKE_POLICY_VERSION_MINIMUM=3.5"
export PKG_CONFIG_LIBDIR=$P/lib/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=
step(){ echo "=== $1 $(date +%T)"; }
# SDL2 prebuilt
step SDL2; cp -r $S/SDL2-2.30.9/x86_64-w64-mingw32/* $P/
sed -i "s#^prefix=.*#prefix=$P#" $P/lib/pkgconfig/sdl2.pc
find $P/lib/cmake/SDL2 -name '*.cmake' -exec sed -i "s#/tmp/[^\"]*x86_64-w64-mingw32#$P#g" {} \; || true
# zlib
step zlib; $CM -S $S/zlib -B $S/b-zlib -DZLIB_BUILD_EXAMPLES=OFF >/dev/null && ninja -C $S/b-zlib install >/dev/null
# minizip (static, from zlib contrib)
step minizip; mkdir -p $S/b-mz $P/include/minizip; cd $S/b-mz
for f in ioapi unzip zip iowin32; do x86_64-w64-mingw32-gcc-posix -O2 -I$P/include -c $S/zlib/contrib/minizip/$f.c -o $f.o; done
x86_64-w64-mingw32-ar rcs $P/lib/libminizip.a *.o
cp $S/zlib/contrib/minizip/{ioapi,unzip,zip,iowin32,crypt}.h $P/include/minizip/
cat > $P/lib/pkgconfig/minizip.pc <<PC
prefix=$P
libdir=\${prefix}/lib
includedir=\${prefix}/include
Name: minizip
Description: minizip
Version: 1.3.1
Libs: -L\${libdir} -lminizip -lz
Cflags: -I\${includedir}
PC
# libpng
step libpng; $CM -S $S/libpng -B $S/b-png -DPNG_TESTS=OFF -DPNG_TOOLS=OFF -DPNG_STATIC=OFF >/dev/null && ninja -C $S/b-png install >/dev/null
# libjpeg-turbo
step jpeg; $CM -S $S/libjpeg-turbo -B $S/b-jpeg -DENABLE_STATIC=OFF -DWITH_TURBOJPEG=OFF -DWITH_JPEG8=ON >/dev/null && ninja -C $S/b-jpeg install >/dev/null
# dav1d
step dav1d; cd $S/dav1d; meson setup $S/b-dav1d --cross-file /home/claude/build-scripts/meson-cross.txt --prefix=$P --buildtype=release -Denable_tools=false -Denable_tests=false -Ddefault_library=shared >/dev/null && ninja -C $S/b-dav1d install >/dev/null
# libavif
step avif; $CM -S $S/libavif -B $S/b-avif -DAVIF_CODEC_DAV1D=SYSTEM -DAVIF_LIBYUV=OFF -DBUILD_SHARED_LIBS=ON >/dev/null && ninja -C $S/b-avif install >/dev/null
# glew
step glew; $CM -S $S/glew-2.2.0/build/cmake -B $S/b-glew -DBUILD_UTILS=OFF -DglewBUILD_SHARED_LIBS=ON >/dev/null && ninja -C $S/b-glew install >/dev/null
# openal
step openal; $CM -S $S/openal-soft -B $S/b-al -DALSOFT_EXAMPLES=OFF -DALSOFT_UTILS=OFF -DALSOFT_TESTS=OFF -DALSOFT_INSTALL_EXAMPLES=OFF -DALSOFT_INSTALL_UTILS=OFF >/dev/null && ninja -C $S/b-al install >/dev/null
# flac
step flac; $CM -S $S/flac -B $S/b-flac -DBUILD_SHARED_LIBS=ON -DWITH_OGG=OFF -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF -DBUILD_DOCS=OFF -DINSTALL_MANPAGES=OFF -DWITH_STACK_PROTECTOR=OFF >/dev/null && ninja -C $S/b-flac install >/dev/null
# libmad (static)
step mad; mkdir -p $S/b-mad; cd $S/b-mad
cp $S/libmad-0.15.1b/msvc++/config.h config.h; sed -i 's/^#define inline __inline//' config.h
for f in bit decoder fixed frame huffman layer12 layer3 stream synth timer version; do x86_64-w64-mingw32-gcc-posix -O2 -DFPM_64BIT -DHAVE_CONFIG_H -I. -I$S/libmad-0.15.1b -c $S/libmad-0.15.1b/$f.c -o $f.o; done
x86_64-w64-mingw32-ar rcs $P/lib/libmad.a *.o
sed -e 's/^# define FPM_INTEL/# define FPM_64BIT/' $S/libmad-0.15.1b/mad.h > $P/include/mad.h
step DONE
