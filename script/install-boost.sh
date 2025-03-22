#!/bin/bash

set -e
set -x

. script/util.sh

SRC="$PWD"

BOOST_DOWNLOAD="$VENDORSRC/download-boost.tgz"

download \
	-u "https://archives.boost.io/release/1.87.0/source/boost_1_87_0.tar.gz" \
	-c "f55c340aa49763b1925ccf02b2e83f35fdcf634c9d5164a2acb87540173c741d" \
	-o "$BOOST_DOWNLOAD"
	
BOOST="$VENDORSRC/boost"
md "$BOOST"

untar -f "$BOOST_DOWNLOAD" -d "$BOOST"

cd "$BOOST"
./bootstrap.sh --prefix="$VENDOR" --with-libraries=headers
./b2 install

cd "$SRC"
rm -rf "$BOOST"
rm "$BOOST_DOWNLOAD"

# Install pkg-config files
cd "$SRC/boost"
cmake -DCMAKE_INSTALL_PREFIX="$VENDOR" -S . -B build
cmake --build build
cmake --install build
rm -rf build
cd "$SRC"
