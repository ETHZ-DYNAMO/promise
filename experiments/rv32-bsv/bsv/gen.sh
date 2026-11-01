#!/bin/bash

BUILD="_build"

mkdir -p $BUILD

VOUT="./output"

mkdir -p "$VOUT"

FLAGS="-D BSV_POSITIVE_RESET -aggressive-conditions +RTS -K16M -RTS -bdir $BUILD -verilog -u -vdir $VOUT -g"

/data/bsc/inst/bin/bsc $FLAGS top_bsv top_bsv.bsv
/data/bsc/inst/bin/bsc $FLAGS rv32_bsv rv32_bsv.bsv

