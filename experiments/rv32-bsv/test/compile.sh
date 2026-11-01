PROGRAM="./tmp.c"
#
# PROGRAM="$1"

[ -f $PROGRAM ] || { echo "$1 is not a file!" ; exit 1; }

echo "Compiling program $PROGRAM"

LLVM_BINS=/opt/llvm-project-20/build/bin

RV_TOOLCHAIN_ROOT="/opt/riscv-toolchains/rv32i-ilp32"

BIT_WIDTH=32

set -e

CSMITH_INC="$HOME/Build/csmith/runtime"

CSMITH_BUILD_INC="$HOME/Build/csmith/build/runtime"

LIBC="$RV_TOOLCHAIN_ROOT/riscv32-unknown-elf/usr/include"

CC="$RV_TOOLCHAIN_ROOT/bin/riscv32-unknown-elf-gcc -O3 -I$CSMITH_INC -I$CSMITH_BUILD_INC -march=rv32i -mabi=ilp32"

## clang -I$CSMITH_INC -I$LIBC --target=riscv32 -march=rv32i -mabi=ilp32 -S $PROGRAM -o test.s
## clang -I$CSMITH_INC -I$LIBC --target=riscv32 -march=rv32i -mabi=ilp32 -c $PROGRAM -o test.o
$CC -S $PROGRAM -o test.s
$CC $PROGRAM -o test.o

"$RV_TOOLCHAIN_ROOT/bin/riscv32-unknown-elf-elf2hex" \
  --bit-width $BIT_WIDTH \
  --input test.o \
  --output mem.vmh

echo "Compiled mem.vmh"
