#!/usr/bin/env bash

ARCH=$(arch)
CWD=$(pwd)

if [ $ARCH == "aarch64" ]
then
    echo "Compiling x86 binaries on an aarch64 machine"
    export GCC5_BIN="x86_64-linux-gnu-"
fi

cd ..
. ./edksetup.sh
build -p DxeSmmHook/DxeSmmHook.dsc -b DEBUG -a X64 -t GCC5
cd $CWD
