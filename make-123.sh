#!/bin/sh
filename=$3
ext="${filename##*.}"
rm -rf build
echo $PATH
toolchain=riscv64-unknown-linux-gnu-
which ${toolchain}gcc 2>/dev/null
if [ $? -gt 0 ]; then toolchain=riscv64-linux-gnu- ; fi
echo Using toolchain $toolchain
CROSS_COMPILE=$toolchain make PLATFORM=$1 FW_PAYLOAD_PATH=$2 -j8
mv build/platform/$1/firmware/fw_payload.$ext $3
echo built for $1 with $2 to $3 
