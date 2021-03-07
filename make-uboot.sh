#!/bin/sh
cd "$(dirname "$0")"
./make-123.sh generic u-boot.bin fw-uboot.elf
cd - > /dev/null
