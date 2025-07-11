# General configurations
cmake_minimum_required(VERSION 3.13)

project(Promise)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set the location of linked binaries to promise/build/bin instead of promise/build
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(cmake/FindYosys.cmake)
include(cmake/FindABC.cmake)

find_package(Eigen3 3.3 REQUIRED NO_MODULE)
find_package(Boost 1.82 REQUIRED COMPONENTS filesystem system graph json)

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
include_directories(SYSTEM ${YOSYS_ROOT})
include_directories(SYSTEM ${EIGEN3_INCLUDE_DIR})
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
include_directories(SYSTEM ${ABC_ROOT}/src)

add_library(Promise)

target_compile_options(Promise
    PRIVATE 

    -D_YOSYS_
    -Wall -Wextra
    -DYOSYS_ENABLE_GLOB
    -DYOSYS_ENABLE_ZLIB
    -DYOSYS_ENABLE_ABC
    -DYOSYS_ENABLE_COVER
    -DPROMISE_BINARIES_DIR="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    -D${ABC_ARCH_NAME}
)

function(add_promise_binary binary_name)
    add_executable(${binary_name} ${ARGN})
    target_link_libraries(${binary_name}
        PRIVATE

        Promise
        ${YOSYS_LIB}
        ${ABC_LIB}
        Eigen3::Eigen
        Boost::json
        Boost::filesystem
    )
    target_compile_options(${binary_name}
        PRIVATE 

        -D_YOSYS_
        -Wall -Wextra
        -DYOSYS_ENABLE_GLOB
        -DYOSYS_ENABLE_ZLIB
        -DYOSYS_ENABLE_ABC
        -DYOSYS_ENABLE_COVER
        -DPROMISE_BINARIES_DIR="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
        -D${ABC_ARCH_NAME}
    )
endfunction()

# Assuming your script is at ${CMAKE_SOURCE_DIR}/scripts/myscript.sh
add_custom_target(link_script ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/bin
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${YOSYS_ROOT}/install/bin/yosys-abc
        ${CMAKE_BINARY_DIR}/bin/yosys-abc
    COMMENT "Symlinking yosys-abc -> build/bin/yosys-abc"
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_SOURCE_DIR}/ext/rIC3/bin/rIC3
        ${CMAKE_BINARY_DIR}/bin/rIC3
    COMMENT "Symlinking rIC3 -> build/bin/rIC3"
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_SOURCE_DIR}/ext/verilator/bin/verilator
        ${CMAKE_BINARY_DIR}/bin/verilator
    COMMENT "Symlinking verilator -> build/bin/verilator"
)

add_subdirectory(${CMAKE_SOURCE_DIR}/lib)
add_subdirectory(${CMAKE_SOURCE_DIR}/experiments)
add_subdirectory(${CMAKE_SOURCE_DIR}/tools)
add_subdirectory(${CMAKE_SOURCE_DIR}/test)