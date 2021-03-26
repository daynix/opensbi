#!/bin/sh
cd "$(dirname "$0")"
./make-123.sh fpga/ariane ariane-vmlinux.bin fw-fpga.bin
mv build/platform/fpga/ariane/firmware/fw_payload.elf fw-fpga.elf
cd - > /dev/null
