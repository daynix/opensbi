#!/bin/sh
cd "$(dirname "$0")"
./make-123.sh fpga/ariane ariane-vmlinux.bin fw-fpga.bin
cd - > /dev/null
