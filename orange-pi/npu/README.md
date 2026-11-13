# NPU image classification

Classifies an image with MobileNet v1 on the NPU of the Orange Pi 3B
(Rockchip RK3566), through Rockchip's C runtime `librknnrt`. It prints
the five most likely labels, then times repeated runs.

## Requirements

On the board:

- A kernel with Rockchip's NPU driver (`RKNPU`), such as Armbian's vendor
  kernel 6.1 (tested with driver v0.9.8).
- `gcc`, `make` and `curl`, and access to GitHub for the first build.

Tested as root on Armbian 26.8 (Ubuntu 24.04).

## Build and run

Copy this folder to the board, then:

```sh
make run
```

The first build downloads into `rknn/` (ignored by git), pinned to
rknn-toolkit2 v2.5.0:

- `librknnrt.so` and `rknn_api.h`: Rockchip's runtime and its header
- `mobilenet_v1.rknn`: the model, built for RK3566/RK3568
- `dog_224x224.jpg` and `labels.txt`: a test image and the class names
- `stb_image.h`: the image decoder, from nothings/stb

The program finds `librknnrt.so` in `rknn/` next to its build folder,
so nothing is installed system-wide. `make clean` removes the build,
`make distclean` the downloads too.

## Usage

```sh
build/npu-classify MODEL IMAGE LABELS [RUNS]
```

| Argument | Meaning |
| --- | --- |
| `MODEL` | an `.rknn` model built for this chip |
| `IMAGE` | JPEG or PNG, exactly the model's input size (224x224); it is not resized |
| `LABELS` | one `index:name` per line, as in Rockchip's `labels.txt` |
| `RUNS` | timed runs after the first, default 100; 0 skips timing |

## Example output

```text
input input: 224x224x3
  0.928  Shih-Tzu
  0.063  Pekinese, Pekingese, Peke
  0.004  Lhasa, Lhasa apso
  0.004  Persian cat
  0.000  Siamese cat, Siamese
100 runs: 5.08 ms each, 196.7 per second
```
