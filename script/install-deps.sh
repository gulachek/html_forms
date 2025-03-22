#!/bin/bash

set -e
set -x

. script/util.sh

cmake_build_install() {
	cmake --build build
	cmake --install build --prefix "$VENDOR"
	rm -rf build
}

SRC="$PWD"

# gtest
GTEST_DOWNLOAD="$VENDORSRC/download-gtest.tgz"

download \
	-u "https://github.com/google/googletest/releases/download/v1.16.0/googletest-1.16.0.tar.gz" \
	-c "78c676fc63881529bf97bf9d45948d905a66833fbfa5318ea2cd7478cb98f399" \
	-o "$GTEST_DOWNLOAD"
	
GTEST="$VENDORSRC/googletest"

md "$GTEST"

untar -f "$GTEST_DOWNLOAD" -d "$GTEST"

cd "$GTEST"
cmake -DBUILD_GMOCK=OFF "-DCMAKE_INSTALL_PREFIX=$VENDOR" -S . -B build
cmake_build_install

rm "$GTEST_DOWNLOAD"

# zlib
ZLIB_DOWNLOAD="$VENDORSRC/download-zlib.tgz"
download \
	-u "https://zlib.net/zlib-1.3.1.tar.gz" \
	-c "9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23" \
	-o "$ZLIB_DOWNLOAD"

ZLIB="$VENDORSRC/zlib"
md "$ZLIB"
untar -f "$ZLIB_DOWNLOAD" -d "$ZLIB"

cd "$ZLIB"
cmake -S . -B build \
	-DZLIB_BUILD_EXAMPLES=OFF \
	-DCMAKE_INSTALL_PREFIX="$VENDOR" \
	-DINSTALL_PKGCONFIG_DIR="$VENDOR/lib/pkgconfig"
cmake_build_install

rm "$ZLIB_DOWNLOAD"
cd "$SRC"

# cJSON
CJSON_DOWNLOAD="$VENDORSRC/download-cjson.tgz"
download \
	-u "https://github.com/DaveGamble/cJSON/archive/refs/tags/v1.7.18.tar.gz" \
	-o "$CJSON_DOWNLOAD" \
	-c "3aa806844a03442c00769b83e99970be70fbef03735ff898f4811dd03b9f5ee5"

CJSON="$VENDORSRC/cJSON"
md "$CJSON"

untar -d "$CJSON" -f "$CJSON_DOWNLOAD"
# cJSON CMakeLists.txt configures files that have full install paths. Must define prefix

cd "$CJSON"
cmake -DENABLE_CJSON_TEST=OFF -DBUILD_SHARED_LIBS=OFF "-DCMAKE_INSTALL_PREFIX=$VENDOR" -S . -B build
cmake_build_install

rm "$CJSON_DOWNLOAD"

# msgstream
MSGSTREAM_DOWNLOAD="$VENDORSRC/download-msgstream.tgz"
download \
	-u "https://github.com/gulachek/msgstream/releases/download/v0.3.2/msgstream-0.3.2.tgz" \
	-o "$MSGSTREAM_DOWNLOAD" \
	-c "5126ddd87fc61d0372aa7bb310be035ff18f76d1613e3784a7ac8f6b9a4940bb"

MSGSTREAM="$VENDORSRC/msgstream"
md "$MSGSTREAM"

untar -f "$MSGSTREAM_DOWNLOAD" -d "$MSGSTREAM"
cd "$MSGSTREAM"
cmake -S . -B build
cmake_build_install

rm "$MSGSTREAM_DOWNLOAD"

# unixsocket
UNIX_DOWNLOAD="$VENDORSRC/unixsocket-download.tgz"

download \
	-u "https://github.com/gulachek/unixsocket/releases/download/v0.1.1/unixsocket-0.1.1.tgz" \
	-o "$UNIX_DOWNLOAD" \
	-c "187244123c7dcbb6f96f52f126447321714658b5bd5cc41bc07338659f795c40"

UNIX="$VENDORSRC/unixsocket"
md "$UNIX"

untar -f "$UNIX_DOWNLOAD" -d "$UNIX"
cd "$UNIX"
cmake -S . -B build
cmake_build_install

rm "$UNIX_DOWNLOAD"

# catui
CATUI_DOWNLOAD="$VENDORSRC/catui-download.tgz"

download \
	-u "https://github.com/gulachek/catui/releases/download/v0.1.3/catui-0.1.3.tgz" \
	-o "$CATUI_DOWNLOAD" \
	-c "3957d8249dcbc8fa6f8add23263cef7c7c0a3fb21bb3cf865316dcd8ff8c97bc"

CATUI="$VENDORSRC/catui"
md "$CATUI"

untar -f "$CATUI_DOWNLOAD" -d "$CATUI"
cd "$CATUI"
cmake -DCMAKE_PREFIX_PATH="$VENDOR" -S . -B build
cmake_build_install

rm "$CATUI_DOWNLOAD"
