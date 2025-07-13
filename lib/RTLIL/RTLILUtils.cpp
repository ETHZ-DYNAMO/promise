#include "kernel/rtlil.h"
#include "kernel/yosys_common.h"
#include "promise/Invariants.h"
#include <kernel/ff.h>
#include <kernel/ffinit.h>
#include <kernel/log.h>
#include <kernel/sigtools.h>
#include <stdexcept>
#include <vector>

#include "promise/RTLIL/RTLILUtils.h"

USING_YOSYS_NAMESPACE

bool checkFFInitialized(Module *module, Cell *ff) {

  SigMap sigmap(module);
  FfInitVals initVals(&sigmap, module);

  FfData ffData(&initVals, ff);

  for (int bit = 0; bit < ffData.width; ++bit) {
    if (ffData.val_init[bit] != State::S0 &&
        ffData.val_init[bit] != State::S1) {
      std::cerr << "Warning: " << log_id(ffData.name)
                << " has not been initialized!\n";
      if (ff->has_attribute(Yosys::RTLIL::escape_id("src"))) {
        std::cerr << "HDL src: " << (ff->get_src_attribute()) << "\n";
      }
      return false /* i.e., FFs are not initialized */;
    }
  }

  return true /* i.e., FFs are all initialized */;
}