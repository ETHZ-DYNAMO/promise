exit_on_failure() {
  if [ $? -ne 0 ]; then
    echo "Error: $1"
    exit 1
  fi
}

set -e

PROMISE_CWD=$(realpath .)

EXT_DIR="${PROMISE_CWD}/ext"

build_abc() {
    cd "$EXT_DIR/abc"
    make clean
    CC=g++ make -j8 ABC_USE_PIC=1 libabc.so
    exit_on_failure "ABC build failed"
}

build_yosys() {
    cd "$EXT_DIR/yosys"
    git submodule update --recursive
    make clean
    make install -j18 ENABLE_LIBYOSYS=1 PREFIX="$EXT_DIR/yosys/install"
    exit_on_failure "Yosys build failed"
}

build_ric3() {
  mkdir -p "$EXT_DIR/rIC3"
  # Install rIC3 from crates:
  cargo install rIC3 --force --version 1.4.1 --root $EXT_DIR/rIC3
  exit_on_failure "rIC3 build failed"
}

build_verilator() {
  cd $EXT_DIR/verilator
  autoconf && ./configure && make -j8
  exit_on_failure "verilator build failed"
}

build_yosys
build_abc
build_ric3
build_verilator

mkdir -p "$PROMISE_CWD/build"
cd "$PROMISE_CWD/build"

cmake .. -G Ninja \
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_BUILD_TYPE=Debug \
  -DYOSYS_ROOT="$PROMISE_CWD/ext/yosys" \
  -DABC_ROOT="$PROMISE_CWD/ext/abc" \

ninja
