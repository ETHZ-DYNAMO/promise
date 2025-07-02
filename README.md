# Promise: Property Mining for Sequential Synthesis

Promise is an optimization tool for removing the redundancy in sequential
circuits. Promise relies on the technique described
[here](https://dynamo.ethz.ch/wp-content/uploads/sites/22/2025/06/Xu_IWLS25_Promise_Property_Mining_for_Sequential_Synthesis.pdf)
to generate invariants from simulation traces to aid existing sequential
synthesis algorithms and remove redundant FFs.

## Table of Contents

- [Getting Started](#getting-started)
- [Using Promise](#using-promise)
- [License Notice](#license-notice)

## Getting Started

This section describes how to build Promise.

### Recommended: Using the Provided Dockerfile

For convenience, we provide a Dockerfile, you can build the Docker image as follows:

```
$ docker build -t promise-image . # builds the image
$ docker run -it -u promise-image /bin/bash # launches the image
```

### Installing Dependencies

```
$ sudo apt-get install -y \
  build-essential git libgtest-dev \
  libeigen3-dev ninja-build cmake libboost-dev \
  lld bison flex libreadline-dev gawk tcl-dev libffi-dev \
  graphviz xdot pkg-config python3 zlib1g-dev \
  help2man perl make curl python3-venv python3-pip autoconf
```

The default version of boost is too old (Promise needs at least 1.82); you can get it from the [official boost website](https://www.boost.org/releases/1.82.0/).

This project can optionally use `rIC3`. Which requires a [rust
compiler](https://www.rust-lang.org/tools/install). `rIC3` uses some unstable rust features. Therefore, we need to enable the nightly version:

```
$ rustup install nightly
$ rustup override set nightly
```

### Building the Project

Clone Promise from GitHub and all its submodules:
```
$ git clone --recurse-submodules git@github.com:ETHZ-DYNAMO/promise.git
```

Build Promise:
```
$ cd promise && bash build.sh
```

### Testing

To make sure that the project has been correctly built, run the test suite:
```
$ cd build
$ ninja test
```

## Using Promise

```
$ build/bin/promise <top_name> <src1.v> <src2.v> <src3.v> ...
```

### Running the IWLS/ICCAD '25 Experiments

Please refer to the description in `experiments/iccad25-promise`.

## License Notice

Promise is licensed under the [GPL-3.0 license](https://www.gnu.org/licenses/gpl-3.0.en.html).

Promise depends on the following components:
- [Yosys](https://github.com/YosysHQ/yosys) is licensed under the ISC license.
- [ABC](https://github.com/berkeley-abc/abc) is copyrighted by the University of California, Berkeley.
- [Verilator](https://github.com/verilator/verilator) is licensed under the LGPL-3.0 license.
- [rIC3](https://github.com/gipsyh/rIC3) is licensed under the GPL-3.0 license.
