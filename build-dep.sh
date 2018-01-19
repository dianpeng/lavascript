#!/bin/bash

## Zydis
pushd dep/zydis-2.0.0-beta1/
mkdir -p build
pushd build
cmake ../
make
popd
popd
