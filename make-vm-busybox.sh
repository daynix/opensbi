#!/bin/sh
cd "$(dirname "$0")"
./make-123.sh generic ariane-vmlinux.bin fw-busybox.elf
cd - > /dev/null
