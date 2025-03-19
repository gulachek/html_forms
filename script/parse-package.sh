#!/bin/bash

set -e
set -x

PACKAGE_NAME="$(jq -r .name package.json)"
PACKAGE_VERSION="$(jq -r .version package.json)"
