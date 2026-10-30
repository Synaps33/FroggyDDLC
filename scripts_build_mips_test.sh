#!/bin/bash
set -e
TOP="$(cd "$(dirname "$0")" && pwd)"
CROSS=${CROSS:-mipsel-linux-muslsf-}
CC=${CROSS}gcc
CXX=${CROSS}g++
FLAGS="-EL -march=mips32 -msoft-float -O2 -static -I$TOP/include -I$TOP/vendor -I$TOP/vendor/lua-5.1.5/src -DSCREEN_WIDTH=320 -DSCREEN_HEIGHT=240"
OUT=${OUT:-/tmp/ddlc_mips}
rm -rf $OUT
mkdir -p $OUT/lua
for f in $TOP/vendor/lua-5.1.5/src/*.c; do
  b=$(basename $f .c)
  case $b in lua|luac|print) continue;; esac
  $CC $FLAGS -c $f -o $OUT/lua/$b.o
done
$CC $FLAGS -c $TOP/vendor/miniz.c -o $OUT/miniz.o
$CC $FLAGS -c $TOP/vendor/bit.c -o $OUT/bit.o
for f in $TOP/src/*.cpp $TOP/libretro/libretro_core.cpp $TOP/runner/mips_runner.cpp; do
  b=$(basename $f .cpp)
  $CXX $FLAGS -std=gnu++11 -fno-exceptions -fno-rtti -fno-strict-aliasing -c $f -o $OUT/$b.o
done
$CXX $FLAGS -o $OUT/ddlc_mips $OUT/*.o $OUT/lua/*.o -lm
echo "built $OUT/ddlc_mips"
