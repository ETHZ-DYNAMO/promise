
/data/verilator/bin/verilator \
  --trace \
  --trace-underscore \
  --Wno-UNOPTFLAT \
  --Wno-WIDTHTRUNC \
  --Wno-CASEINCOMPLETE \
  --top-module top_bsv \
  --cc \
  --DBSV_POSITIVE_RESET \
  ./bsv/output/rv32_bsv.v \
  ./bsv/output/top_bsv.v \
  ./hdl/BRAM1BELoad.v \
  ./hdl/BRAM2BELoad.v \
  ./hdl/FIFO2.v \
  ./hdl/FIFOL1.v \
  ./hdl/RevertReg.v \
  ./hdl/SizedFIFO.v \
  --exe tb.cpp

make -j8 -C obj_dir -f Vtop_bsv.mk Vtop_bsv

./obj_dir/Vtop_bsv 
