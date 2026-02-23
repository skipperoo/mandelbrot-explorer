#!/bin/bash
for i in {0..4}; do
  make clean
  make BENCHFLAGS="-DZOOM=0.5" -j
  make bench >>benchmark_05.log
  make clean
  make BENCHFLAGS="-DZOOM=1" -j
  make bench >>benchmark_1.log
  make clean
  make BENCHFLAGS="-DZOOM=5" -j
  make bench >>benchmark_5.log
  make clean
  make BENCHFLAGS="-DZOOM=0.5 -DTHREAD_PINNING" -j
  make bench >>benchmark_05_TP.log
  make clean
  make BENCHFLAGS="-DZOOM=1 -DTHREAD_PINNING" -j
  make bench >>benchmark_1_TP.log
  make clean
  make BENCHFLAGS="-DZOOM=5 -DTHREAD_PINNING" -j
  make bench >>benchmark_5_TP.log
  make clean
done
