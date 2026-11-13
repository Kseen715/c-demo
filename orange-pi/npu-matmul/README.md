# NPU matrix multiply benchmark

Multiplies square matrices, C = A x B, on the NPU of the Orange Pi 3B
(Rockchip RK3566) with Rockchip's matmul API (`rknn_matmul_*` in
`librknnrt`). It compares the NPU with the board's CPU (4 Cortex-A55
cores) on one core and on all four, and checks that the results agree.

## What it measures

Two NPU types:

- 16-bit float x 16-bit float, 32-bit float result
- 8-bit int x 8-bit int, 32-bit int result

The RK3566 rejects a 16-bit float result. The 4-bit types in the header
are for other chips, and the mixed and 8-bit-result types need
quantization scales, so they are left out.

For each size and type it prints a time in milliseconds and a rate in
GOPS (2 x M x K x N operations per multiply):

| Column | What is timed |
| --- | --- |
| npu run | `rknn_matmul_run` alone, with A and B already in NPU memory |
| npu+copy | copy A and B in, hand them over, run, copy C out |
| cpu 1 | the same multiply on one core |
| cpu all | the same multiply on all cores (OpenMP) |
| check | float: largest error relative to the largest value; int: wrong elements |

The CPU code is a plain loop the compiler vectorizes for the Cortex-A55,
in 32-bit float for the float type. It is not a tuned library such as
OpenBLAS, which would be faster. The float inputs are rounded to 16 bits
first, so the CPU multiplies exactly what the NPU gets.

Each timing repeats until 300 ms have passed. The first line shows the
load average: other work on the board slows the CPU columns.

## Requirements

On the board:

- A kernel with Rockchip's NPU driver (`RKNPU`), such as Armbian's vendor
  kernel 6.1 (tested with driver v0.9.8).
- `gcc` with OpenMP (`libgomp`), `make` and `curl`, and access to GitHub
  for the first build.

Tested as root on Armbian 26.8 (Ubuntu 24.04).

## Build and run

Copy this folder to the board, then:

```sh
make run
```

The first build downloads `librknnrt.so`, `rknn_api.h` and
`rknn_matmul_api.h` into `rknn/` (ignored by git), pinned to
rknn-toolkit2 v2.5.0. The program finds the library there, so nothing is
installed system-wide. `make clean` removes the build, `make distclean`
the downloads too.

## Usage

```sh
build/npu-matmul [SIZE...]
```

Sizes default to 64 128 256 512 1024 2048. Each must be a multiple of
32 (the NPU's alignment for K and N), at most 10240. The default set
takes a few minutes, mostly the single-core runs at 2048.

## Results

Measured on an Orange Pi 3B, load average 0.24, in GOPS:

| Size | fp16 npu run | fp16 npu+copy | fp16 cpu all | int8 npu run | int8 npu+copy | int8 cpu all |
| --- | --- | --- | --- | --- | --- | --- |
| 64 | 6.4 | 4.0 | 4.0 | 6.4 | 3.9 | 7.5 |
| 128 | 17.2 | 10.3 | 3.8 | 20.1 | 11.7 | 5.6 |
| 256 | 48.7 | 22.3 | 3.8 | 51.5 | 26.1 | 7.3 |
| 512 | 101.8 | 33.5 | 3.2 | 114.6 | 52.9 | 8.2 |
| 1024 | 147.5 | 42.4 | 1.8 | 241.4 | 67.0 | 7.9 |
| 2048 | 105.8 | 48.3 | 2.7 | 350.5 | 100.1 | 7.4 |

One core managed about 1 GOPS in float and 2 GOPS in int8. The int8
results were exact at every size; the float ones stayed within 2e-6 of
the CPU's.

- From 128 up, the NPU beats all four cores even with the copies: 18 to
  24 times faster in float, 8 to 14 times in int8 at 1024 and 2048.
- At 64 the CPU ties in float and wins in int8: the cost of handing
  data to the NPU outweighs the work.
- The copies make large sizes 2 to 3.5 times slower than the run alone.
  Most of that is converting the matrices from row order into the NPU's
  own layout, done on the CPU at each hand-over. A matrix that stays the
  same between calls can be converted once
  (`rknn_B_normal_layout_to_native_layout`) and passed in native layout.
- The int8 peak, 350 GOPS, is about 44% of the chip's rated 0.8 TOPS.
