#!/bin/bash

function build_cmake {
  pushd "$1"
  mkdir -p build
  pushd build
  cmake ../
  make
  popd
  popd
}


for D in `find dep/ -maxdepth 1 -type d`
do
  if [ -f $D/build.sh ]; then
    echo "Folder $D has build.sh, use it to build"
    $D/build.sh
  fi

  if [ -f $D/Makefile ]; then
    echo "Folder $D has Makefile, use make to build"
    pushd $D
    make
    popd
  fi

  if [ -f $D/CMakeLists.txt ]; then
    echo "Folder $D has CMakeLists.txt, use cmake to build"
    build_cmake $D
  fi
done
