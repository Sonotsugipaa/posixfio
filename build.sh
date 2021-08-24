#!/bin/bash

set -e # Terminate the script when an error occurs

config="${1:-"Debug"}"
generator="${generator:-"Unix Makefiles"}"

srcpath="${srcpath:-"$(realpath .)"}"
dstpath="${dstpath:-build-"$config"}"

if [[ -v installpath ]]
then install=("-DCMAKE_INSTALL_PREFIX=$installpath")
else install=()
fi

mkdir -p "$dstpath"
cd "$dstpath"

cmake -DCMAKE_BUILD_TYPE="$config" "${install[@]}" "$srcpath" -G "$generator"
cmake --build . --config "$config"
