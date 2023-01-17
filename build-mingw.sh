#!/bin/env bash

set -e # Terminate the script when an error occurs

config="${config:-"Debug"}"
generator="${generator:-"Unix Makefiles"}"

srcpath="${srcpath:-"$(realpath .)"}"
dstpath="${dstpath:-build}"

if [[ -v 1 ]]; then config="$1"; fi

if [[ -v installpath ]]
then install=("-DCMAKE_INSTALL_PREFIX=$installpath")
else install=()
fi

mkdir -p "$dstpath"
cd "$dstpath"

echo 'Configuration:   "'"$config"\"
echo 'Generator:       "'"$generator"\"
echo 'Source dir:      "'"$srcpath"\"
echo 'Destination dir: "'"$dstpath"\"

x86_64-w64-mingw32-cmake -DCMAKE_BUILD_TYPE="$config" "${install[@]}" "$srcpath" -G "$generator"
cmake --build . --config "$config"

cp -t "$dstpath" \
	/usr/x86_64-w64-mingw32/bin/libgcc_s_seh-1.dll \
	/usr/x86_64-w64-mingw32/bin/libstdc++-6.dll \
	/usr/x86_64-w64-mingw32/bin/libwinpthread-1.dll
