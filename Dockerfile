FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Our framework needs Boost 1.82
RUN apt-get -y update && apt-get -y upgrade

# Dependencies 
RUN apt-get install -y \
  build-essential git libgtest-dev \
  libeigen3-dev ninja-build cmake libboost-dev \
 	lld bison flex libreadline-dev gawk tcl-dev libffi-dev \
	graphviz xdot pkg-config python3 zlib1g-dev \
	help2man perl make curl python3-venv python3-pip autoconf

RUN curl https://sh.rustup.rs -sSf | sh -s -- -y
ENV PATH="/root/.cargo/bin:${PATH}"
RUN rustc --version && cargo --version

# rIC3 requires nightly version of the rust compiler:
RUN rustup install nightly && \
	rustup override set nightly

# We need boost >= 1.82:
RUN cd /tmp && \
	curl https://archives.boost.io/release/1.82.0/source/boost_1_82_0.tar.gz -o boost_1_82_0.tar.gz && \
	tar -xf boost_1_82_0.tar.gz && \
	cd boost_1_82_0 && \
	./bootstrap.sh --prefix=/usr/local && \
	./b2 install -j$(nproc) && \
	rm -rf /tmp/boost_1_82_0

COPY . /promise

RUN cd /promise && bash build.sh
