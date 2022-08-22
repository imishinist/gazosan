#!/bin/bash -x

set -e

arch=$(uname -m)
[ $arch = arm64 ] && arch=aarch64

version=$1
dest=gazosan-$version-$arch-linux

dir=$(pwd)

tmpdir=$(mktemp -d)
cd $tmpdir

cmake -DCMAKE_BUILD_TYPE=Release $dir
cmake --build . -j$(nproc)
cmake --install . --prefix $dest --strip
tar czf $dir/$dest.tar.gz $dest
