# syntax=docker/dockerfile:1
FROM ubuntu:focal AS base

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=US/Eastern

RUN apt -y update
RUN apt -y install build-essential autoconf automake git bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo libisl-dev curl gcc-multilib qemu-system-x86 nasm dosfstools xterm

RUN git clone https://github.com/httpe/simple-os.git simple-os

FROM base AS toolchain

WORKDIR /simple-os

RUN ./build-toolchain.sh

FROM base AS final

COPY --from=toolchain /simple-os/toolchain /simple-os/toolchain

WORKDIR /simple-os

COPY applications/Makefile applications/Makefile

RUN ./clean.sh 
RUN ./build.sh

RUN dd if=/dev/zero of=testfs.fat bs=1024 count=$(expr 16 \* 1024) && mkfs.fat -F 32 testfs.fat

CMD ["./qemu.sh"]