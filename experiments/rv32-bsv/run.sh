# Runs Promise on the RV32 Bluespec core (from Koika's examples/rv/etc/bsv).
#
# The top is `main` (main.v): it defines BSV_POSITIVE_RESET, so `rst` is active
# high, which is what promise-bsc assumes when it computes the post-reset state.
set -e

cd "$(dirname "$0")"
PROMISE_ROOT=$(realpath ../..)

mkdir -p ./output

"$PROMISE_ROOT/build/bin/promise-bsc" main \
  ./main.v ./hdl/FIFO2.v ./hdl/FIFOL1.v ./hdl/RevertReg.v ./hdl/rv32_bsv.v \
  2> >(tee ./output/promise.log >&2)
