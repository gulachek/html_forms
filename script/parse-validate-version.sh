#!/bin/bash

if [ ! -f package.json ]; then
	echo "Please run $0 from project root!"
	exit 1
fi

. script/parse-package.sh

PUBLISH_VERSION="${GITHUB_REF#refs/tags/v}"

if [ "$PUBLISH_VERSION" != "$PACKAGE_VERSION" ]; then
	echo "Version in GITHUB_REF '$GITHUB_REF' does not match package.json version $PACKAGE_VERSION"
	exit 1
fi

# We now know this is the same
VERSION="$PACKAGE_VERSION"
