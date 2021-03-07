#!/bin/sh
filename=$3
ext="${filename##*.}"
rm -rf build
CROSS_COMPILE=riscv64-linux-gnu- make PLATFORM=$1 FW_PAYLOAD_PATH=$2 -j8
mv build/platform/$1/firmware/fw_payload.$ext $3
echo built for $1 with $2 to $3 
