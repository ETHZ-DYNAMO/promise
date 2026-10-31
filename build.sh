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

build_ric3() {
  mkdir -p "$EXT_DIR/rIC3"
  # Install rIC3 from crates. --locked: use the dependency versions from rIC3's
  # lockfile; newer versions of its dependencies don't compile with 1.4.1.
  # rIC3 uses unstable Rust features; later nightlies changed them, so its
  # dependencies (logicrs) no longer compile with them.
  cargo +nightly-2025-06-26 install rIC3 --locked --force --version 1.4.1 --root $EXT_DIR/rIC3
  exit_on_failure "rIC3 build failed"
}

build_verilator() {
  cd $EXT_DIR/verilator
  autoconf && ./configure && make -j8
  exit_on_failure "verilator build failed"
}

# Yosys is built by CMake as a subproject (see CMakeLists.txt).
git -C "$EXT_DIR/yosys" submodule update --init --recursive
# rIC3 first: its build depends on the Rust toolchain and fails most often.
build_ric3
build_abc
build_verilator

mkdir -p "$PROMISE_CWD/build"
cd "$PROMISE_CWD/build"

cmake .. -G Ninja \
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_BUILD_TYPE=Debug \
  -DABC_ROOT="$PROMISE_CWD/ext/abc" \

ninja
