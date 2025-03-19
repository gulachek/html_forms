#!/bin/sh

set -e
set -x

# Validates and sets VERSION
. script/parse-validate-version.sh

TGZ="$PACKAGE_NAME-$VERSION.tgz"
PKG="${1:?Usage: $0 <$TGZ>}"

if [ "$(basename $PKG)" != "$TGZ" ]; then
	echo "Unexpected tarball '$PKG' given to $0"
	exit 1
fi

gh release upload "v$VERSION" "$PKG#Source Package"
