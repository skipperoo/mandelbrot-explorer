#!/bin/bash
make clean
make BENCHFLAGS="-DZOOM=0.5" -j
make bench >>benchmark_05_gpu.log
make clean
make BENCHFLAGS="-DZOOM=1" -j
make bench >>benchmark_1_gpu.log
make clean
make BENCHFLAGS="-DZOOM=5" -j
make bench >>benchmark_5_gpu.log
make clean
