# End-to-end: simulation, invariant mining, proof, and FF replacement.
add_test(NAME TestDependentFFs
  COMMAND
  promise-bsc main ${CMAKE_CURRENT_SOURCE_DIR}/hdl/main.v
)
# With PASS_REGULAR_EXPRESSION, CTest ignores the exit code: also require the
# successful exit, after the replacement.
set_tests_properties(TestDependentFFs PROPERTIES
  PASS_REGULAR_EXPRESSION "removed 2 dependent FF bits.*exits successfully"
  FAIL_REGULAR_EXPRESSION "terminate called"
)
