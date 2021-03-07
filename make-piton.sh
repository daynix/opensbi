#!/bin/sh
cd "$(dirname "$0")"
./make-123.sh fpga/openpiton ariane-vmlinux.bin fw-piton.bin
cd - > /dev/null
