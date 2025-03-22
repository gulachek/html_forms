#!/bin/bash

set -e
set -x

. script/util.sh

SRC="$PWD"
LA_DOWNLOAD="$VENDORSRC/download-libarchive.tgz"

download \
	-u "https://github.com/libarchive/libarchive/releases/download/v3.7.8/libarchive-3.7.8.tar.gz" \
	-c "a123d87b1bd8adb19e8c187da17ae2d957c7f9596e741b929e6b9ceefea5ad0f" \
	-o "$LA_DOWNLOAD"

LA="$VENDORSRC/libarchive"
md "$LA"

untar -f "$LA_DOWNLOAD" -d "$LA"

cd "$LA"

# Timings listed from x64 macOS 13.7.4 w/ 2.9 GHz proc
# ~1.5 min
cmake -S . -B build \
	-DCMAKE_INSTALL_PREFIX="$VENDOR" \
	-DBUILD_SHARED_LIBS=OFF \
	-DBUILD_TESTING=OFF \
	-DENABLE_BZip2=OFF \
	-DENABLE_CAT=OFF \
	-DENABLE_CNG=OFF \
	-DENABLE_COVERAGE=OFF \
	-DENABLE_CPIO=OFF \
	-DENABLE_EXPAT=OFF \
	-DENABLE_ICONV=OFF \
	-DENABLE_INSTALL=ON \
	-DENABLE_LIBB2=OFF \
	-DENABLE_LIBGCC=OFF \
	-DENABLE_LIBXML2=OFF \
	-DENABLE_LZ4=OFF \
	-DENABLE_LZMA=OFF \
	-DENABLE_LZO=OFF \
	-DENABLE_MBEDTLS=OFF \
	-DENABLE_NETTLE=OFF \
	-DENABLE_OPENSSL=OFF \
	-DENABLE_PCRE2POSIX=OFF \
	-DENABLE_PCREPOSIX=OFF \
	-DENABLE_TAR=OFF \
	-DENABLE_TEST=OFF \
	-DENABLE_UNZIP=OFF \
	-DENABLE_XATTR=OFF \
	-DENABLE_ZLIB=ON \
	-DENABLE_ZSTD=OFF

# ~25 sec
cmake --build build

# ~Instantaneous
cmake --install build

# pkg-config file has -lz as Libs.private. Should be Requires.private

# Does not install CMake package config file by default

rm -rf build
rm "$LA_DOWNLOAD"
cd "$SRC"
