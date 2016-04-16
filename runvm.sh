#!/bin/bash

exec build/xhyve -m 1G -c 1 -s 0:0,hostbridge -s 31,lpc -l com1,/dev/ttys002 -f multiboot,/Users/thomas/scratch/xhyve-smartos/platform/i86pc/kernel/amd64/unix,/Users/thomas/scratch/xhyve-smartos/platform/i86pc/amd64/boot_archive,"-v  -B console=ttya"
