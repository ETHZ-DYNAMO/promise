# General configurations
# Yosys (built as a subproject below) requires CMake 3.28.
cmake_minimum_required(VERSION 3.28)

project(Promise)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set the location of linked binaries to promise/build/bin instead of promise/build
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Build Yosys as a subproject; Promise links against `Yosys::libyosys`.
set(YOSYS_ROOT ${CMAKE_SOURCE_DIR}/ext/yosys)
set(YOSYS_ENABLE_UNIT_TESTS OFF)
# Yosys prepends `${CMAKE_SOURCE_DIR}/cmake` to the module path, assuming it is
# the top-level project; this makes its own modules visible when it is not.
list(PREPEND CMAKE_MODULE_PATH ${YOSYS_ROOT}/cmake)
add_subdirectory(${YOSYS_ROOT} EXCLUDE_FROM_ALL)

include(cmake/FindABC.cmake)

find_package(Eigen3 3.3 REQUIRED NO_MODULE)
find_package(Boost 1.82 REQUIRED COMPONENTS filesystem system graph json program_options)

# See abc/misc/util/abc_global.h: "long long" behaves differently on
# difference archtectures, this flag is used to tell that the machine is
# a linux OS running on 64-bit machine
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ABC_ARCH_NAME LIN64)
    message(STATUS "System is Linux and 64-bit, setting ABC_ARCH_NAME to LIN64")
else()
    message(FATAL_ERROR "This project requires a 64-bit Linux system.")
endif()

# If we put it not in the root CMakeLists.txt, `$ ninja test` will not run.
enable_testing()

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(SYSTEM ${EIGEN3_INCLUDE_DIR})
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
include_directories(SYSTEM ${ABC_ROOT}/src)

# STATIC: Yosys sets the cache option `BUILD_SHARED_LIBS` to ON.
add_library(Promise STATIC)

# `yosys_common` carries the Yosys include directories (including the generated
# `kernel/yosys_config.h` with the `YOSYS_ENABLE_*` macros) and `_YOSYS_`;
# `libyosys` links it privately, so it is not propagated.
target_link_libraries(Promise PUBLIC Yosys::libyosys yosys_common)

target_compile_options(Promise
    PRIVATE

    -Wall -Wextra
    -DPROMISE_BINARIES_DIR="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    -D${ABC_ARCH_NAME}
)

function(add_promise_binary binary_name)
    add_executable(${binary_name} ${ARGN})
    target_link_libraries(${binary_name}
        PRIVATE

        Promise
        ${ABC_LIB}
        Eigen3::Eigen
        Boost::json
        Boost::filesystem
        Boost::program_options
    )
    target_compile_options(${binary_name}
        PRIVATE

        -Wall -Wextra
        -DPROMISE_BINARIES_DIR="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
        -D${ABC_ARCH_NAME}
    )
endfunction()

# Assuming your script is at ${CMAKE_SOURCE_DIR}/scripts/myscript.sh
# libyosys looks up `yosys-abc` and its data directory (`share`, e.g., the
# default techmap library) next to the executable, i.e., in build/bin.
add_custom_target(link_script ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/bin
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        $<TARGET_FILE:yosys-abc>
        ${CMAKE_BINARY_DIR}/bin/yosys-abc
    COMMENT "Symlinking yosys-abc -> build/bin/yosys-abc"
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${YOSYS_CMAKE_BINARY_DIR}/share
        ${CMAKE_BINARY_DIR}/bin/share
    COMMENT "Symlinking the Yosys data directory -> build/bin/share"
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_SOURCE_DIR}/ext/rIC3/bin/rIC3
        ${CMAKE_BINARY_DIR}/bin/rIC3
    COMMENT "Symlinking rIC3 -> build/bin/rIC3"
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_SOURCE_DIR}/ext/verilator/bin/verilator
        ${CMAKE_BINARY_DIR}/bin/verilator
    COMMENT "Symlinking verilator -> build/bin/verilator"
)
add_dependencies(link_script yosys-abc)

add_subdirectory(${CMAKE_SOURCE_DIR}/lib)
add_subdirectory(${CMAKE_SOURCE_DIR}/experiments)
add_subdirectory(${CMAKE_SOURCE_DIR}/tools)
add_subdirectory(${CMAKE_SOURCE_DIR}/test)
