#!/usr/bin/env bash
cd thirdparty/libmypaint

echo ">>> Cloning libmypaint"
git clone https://github.com/mypaint/libmypaint.git -b v1.4.0 brushlib

cd brushlib

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
