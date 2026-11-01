#include "obj_dir/Vtop_bsv.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include <cstdlib>
#include <ctime>

#include <random>

int main(int argc, char **argv) {
  std::mt19937 randomEngine(0);
  Verilated::commandArgs(argc, argv);
  auto top = std::make_unique<Vtop_bsv>();
  VerilatedVcdC *tfp = new VerilatedVcdC;
  Verilated::traceEverOn(true);
  top->trace(tfp, 99);
  tfp->open("sim.vcd");
  std::srand(std::time(nullptr));
  top->rst = 1;
  top->clk = 0;
  top->eval();
  tfp->dump(0);
  for (size_t i = 1; i < 50000; ++i) {
    top->clk = !top->clk;
    if (i == 2)
      top->rst = 0;
    top->eval();
    tfp->dump(i);
  }
  tfp->close();
  return 0;
}
