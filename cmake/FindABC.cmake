
# Set YOSYS_ROOT to the path where abc is built (e.g. ../abc)
if(NOT DEFINED ABC_ROOT)
  message(FATAL_ERROR "Please set ABC_ROOT to point to your abc source directory")
endif()

# Check if abc.a exists
find_library(ABC_LIB 
  NAMES abc libabc 
  PATHS "${ABC_ROOT}"
  NO_DEFAULT_PATH
)

if(NOT ABC_LIB)
  message(FATAL_ERROR "Could not find libabc.so in \"${ABC_ROOT}\". Make sure you ran 'make' in the abc source dir.")
endif()

message(STATUS "Found abc dynamic lib: ${ABC_LIB}")