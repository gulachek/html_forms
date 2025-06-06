#!/bin/bash

set -e
set -x

if [ ! -f package.json ]; then
	echo "Please run from project root!"
	exit 1
fi

. script/util.sh

VERSION="$(jq -r .version package.json)"
TGZ="-$VERSION.tgz"

DIST="$(realpath ${1:?Usage: $0 <package$TGZ>})"
if [[ "$(basename $DIST)" != *"$TGZ" ]]; then
	echo "Unexpected tarball '$DIST' given to $0"
	exit 1
fi

if [ ! -f "$DIST" ]; then
	echo "$DIST doesn't exist"
	exit 1
fi

NAME="$(basename $DIST)"
NAME="${NAME%$TGZ}"

PACKAGE="$VENDORSRC/$NAME"
md "$PACKAGE"

untar -d "$PACKAGE" -f "$DIST"

cmake -DCMAKE_PREFIX_PATH="$VENDOR" -S "$PACKAGE" -B "$PACKAGE/build"
cmake --build "$PACKAGE/build"
cmake --install "$PACKAGE/build" --prefix "$VENDOR"

TEST="$PWD/test/release/$NAME"

echo "Testing pkgconfig"
rm -rf "$TEST/build"
node "$TEST/make.mjs" --srcdir "$TEST" --outdir "$TEST/build" test

echo "Testing CMake"
rm -rf "$TEST/build"
cmake -DCMAKE_PREFIX_PATH="$VENDOR" -S "$TEST" -B "$TEST/build"
cmake --build "$TEST/build"
"$TEST/build/test"
