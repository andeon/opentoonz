#!/usr/bin/env bash
cd thirdparty/libmypaint

echo ">>> Cloning libmypaint"
git clone -n https://github.com/mypaint/libmypaint brushlib

cd brushlib

echo ">>> Switching branch libmypaint 1.4.0"
git checkout 477cb94 --quiet

echo "*" >| .gitignore

echo ">>> Optimizing libmypaint compilation"
export CFLAGS='-Ofast -ftree-vectorize -fopt-info-vec-optimized -march=native -mtune=native -funsafe-math-optimizations -funsafe-loop-optimizations'

echo ">>> Generating libmypaint environment"
./autogen.sh

echo ">>> Configuring libmypaint build"
sudo ./configure

echo ">>> Building libmypaint"
sudo make

echo ">>> Installing libmypaint"
sudo make install

echo ">>> Updating the cache for the linker"
sudo ldconfig
