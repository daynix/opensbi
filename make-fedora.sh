#!/bin/sh
cd "$(dirname "$0")"
./make-123.sh generic vmlinux-fedora.bin fw-fedora.elf
cd - > /dev/null
