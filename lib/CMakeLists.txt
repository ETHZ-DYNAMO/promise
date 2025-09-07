target_sources(Promise
PRIVATE
  LinearAlgebra.cpp
  ModelCheckingResult.cpp
  Invariants.cpp
  AbcCommands.cpp
  RTLIL/RTLILUtils.cpp
  RTLIL/EquipInvariants.cpp
  RTLIL/EncodingOptimization.cpp
)

add_subdirectory(Simulation)
