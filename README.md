# Texture-Preserving Abstraction

A C++ implementation of **cumulative range geodesic filtering (CRGF)** from David Mould's paper *Texture-Preserving Abstraction* (NPAR 2012).

The filter simplifies photographs while retaining texture cues and prominent boundaries. Each output pixel averages an irregular neighborhood grown along paths of similar color. The implementation uses C++11 and the standard library; OpenCV provides image storage and file input/output.

![Original image and results for five mask sizes](results/mask-size-comparison/comparison.png)

*Original image and results for mask sizes 900, 640, 450, 360, and 160, with gamma fixed at 1.0. Larger masks produce stronger abstraction in this example.*

[Full-resolution results and measurements](results/mask-size-comparison/README.md) · [Native-resolution detail comparison](results/mask-size-comparison/detail-comparison.png) · [Reference paper](https://doi.org/10.2312/PE/NPAR/NPAR12/075-082)

## Requirements

- A C++11-compatible compiler, such as GCC, Clang, or MSVC.
- CMake 3.20 or later.
- OpenCV 3 or 4 development files, including the `core` and `imgcodecs` modules.

OpenCV must match your compiler and architecture. The Python `opencv-python` package alone does not supply the C++ development files needed to build this project.

The current build has been validated on Windows with GCC 16.2.0 and OpenCV 4.12.0 in Release mode.

## Build and run

Run the following commands from the repository root.

### Linux

On Debian or Ubuntu, install the dependencies:

```sh
sudo apt install build-essential cmake libopencv-dev
```

Build in Release mode and process the included example image:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/texture_abstraction images/paper-original.jpg output.png 160 1.0
```

### Windows with Visual Studio

Install Visual Studio with the **Desktop development with C++** workload, CMake, and an MSVC-compatible OpenCV distribution. In a developer terminal, set `OpenCV_DIR` to the directory containing `OpenCVConfig.cmake`:

```powershell
cmake -S . -B build-msvc -A x64 -DOpenCV_DIR="C:/opencv/build"
cmake --build build-msvc --config Release --parallel
.\build-msvc\Release\texture_abstraction.exe images\paper-original.jpg output.png 160 1.0
```

These commands use a Visual Studio generator. If OpenCV was built as shared libraries, add the directory containing its runtime DLLs to `PATH` before running the executable.

### Windows with MinGW

With the matching MinGW compiler and CMake on `PATH`, point CMake at a MinGW-compatible OpenCV package:

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DOpenCV_DIR="C:/path/to/opencv/config"
cmake --build build --parallel
.\build\texture_abstraction.exe images\paper-original.jpg output.png 160 1.0
```

Use a separate build directory when switching compilers or generators.

### OpenCV discovery and the local preset

If CMake cannot find OpenCV, pass `-DOpenCV_DIR="/path/to/opencv/config"`, replacing the path with the directory containing the configuration file for your installation.

The optional `workstation` preset in [CMakePresets.json](CMakePresets.json) uses the maintainer's shared Windows GCC/OpenCV toolchain. On that configured machine, the build and test commands are:

```sh
cmake --preset workstation
cmake --build --preset workstation
ctest --preset workstation
```

For other machines, use the instructions above or adapt the preset's toolchain path to your installation.

## Usage

```text
texture_abstraction input output [mask_size] [gamma]
```

| Argument | Default | Description |
| --- | --- | --- |
| `input` | Required | Path to an 8-bit grayscale or three-channel color image. |
| `output` | Required | Output image path. PNG is recommended to avoid JPEG compression artifacts. |
| `mask_size` | `160` | Number of pixels averaged for each output pixel, including the center. Must be a positive integer. |
| `gamma` | `1.0` | Weight of local color changes along each path. Must be finite and nonnegative. |

Start with `160 1.0`, then increase the mask size for stronger simplification. Mask size is a **pixel count**, rather than a radius or square window width. A value of `1` returns an unchanged copy; values larger than the image's pixel count are clamped to that count.

The output preserves the input dimensions and channel count. Alpha-channel, 16-bit, and floating-point inputs are rejected; convert them to a supported format before filtering. The output directory must already exist, and an existing output file is overwritten. Quote paths containing spaces.

Print the command-line help with:

```sh
texture_abstraction --help
```

Use the executable path for your build, such as `./build/texture_abstraction` or `.\build\texture_abstraction.exe`.

## Use the filter from C++

```cpp
#include "texture_abstraction.h"
#include <opencv2/imgcodecs.hpp>

int main() {
    cv::Mat input = cv::imread("photo.jpg", cv::IMREAD_COLOR);
    if (input.empty()) {
        return 1;
    }

    cv::Mat output = texturePreservingAbstraction(input, 160, 1.0);
    return cv::imwrite("result.png", output) ? 0 : 1;
}
```

Link against the CMake target `crgf`, or compile `src/texture_abstraction.cpp` with your application and add `include/` to its header search path. OpenCV's `core` and `imgcodecs` modules are required.

The function accepts `CV_8UC1` and `CV_8UC3` images, including non-contiguous cropped views. It returns a new image and leaves the input unchanged. Invalid arguments throw `std::invalid_argument`; oversized images and overflowing path costs throw `std::overflow_error`.

## How it works

For every center pixel `a`, the filter grows a neighborhood using Dijkstra's shortest-path search on a four-connected image grid. The cost of a step from pixel `g` to its neighbor `h` is:

```text
step_cost(g, h; a) = ||I(h) - I(a)||₂ + gamma * ||I(h) - I(g)||₂
```

The first term compares the new pixel with the fixed center color. The second weights the local color change along the path. Costs accumulate across the path, allowing neighborhoods to follow connected regions of similar color.

The search stops after accepting `mask_size` distinct pixels. Their original colors are averaged with equal weights to produce the output pixel. Every search reads the original image, so filtered values never feed into subsequent searches.

Implementation details:

- Four-connected neighborhoods with no image padding or wraparound.
- Euclidean distance in the original 8-bit color values.
- A priority queue with row-major tie breaking for equal queued distances.
- Double-precision path costs and sums, followed by rounding to 8-bit output.

This is an independent implementation of the published method. Grid and tie-breaking choices can produce differences from the author's results.

## Tests

Tests are built by default and run through CTest:

```sh
ctest --test-dir build -C Release --output-on-failure
```

For the Visual Studio example, replace `build` with `build-msvc`. For the configured WorkStation setup, use `ctest --preset workstation`.

The tests compare small grayscale and color images against an independent Bellman–Ford reference across multiple mask sizes and gamma values. They also cover weak edges, thin features, fading, constant images, oversized masks, non-contiguous inputs, invalid arguments, and command-line validation. No external test framework is required.

To build without tests, configure with `-DBUILD_TESTING=OFF`.

## Performance and limitations

The filter runs on a single CPU thread and starts a shortest-path search for every pixel. Use a Release build; large images and large masks can take substantial time.

For `N` image pixels and a neighborhood of `K` pixels, the implementation uses approximately `O(N × K × log(K + 1))` time and `O(N + K)` working memory, in addition to image storage.

Preserving weak edges can also preserve noise, and very fine textures may disappear. Results depend on the image and parameter settings. The saved mask comparison includes timings and output checks in [measurements.json](results/mask-size-comparison/measurements.json); those timings were collected with five filter processes running concurrently.

## Repository layout

```text
include/texture_abstraction.h   Public filter API
src/texture_abstraction.cpp     CRGF implementation
src/main.cpp                   Command-line application
tests/test_crgf.cpp             Reference and behavior checks
images/                        Example input images
results/mask-size-comparison/   Saved outputs, comparisons, and measurements
CMakeLists.txt                 Library, application, and test targets
CMakePresets.json               Optional local WorkStation preset
Mould_2012_*.pdf                Reference paper
```

## Reference

David Mould. **Texture-Preserving Abstraction.** *International Symposium on Non-Photorealistic Animation and Rendering (NPAR)*, 2012, pp. 75–82.

[DOI and publication](https://doi.org/10.2312/PE/NPAR/NPAR12/075-082) · [Local PDF](Mould_2012_Texture-Preserving_Abstraction.pdf)

```bibtex
@inproceedings{mould2012texture,
  author    = {David Mould},
  title     = {Texture-Preserving Abstraction},
  booktitle = {International Symposium on Non-Photorealistic Animation and Rendering},
  year      = {2012},
  pages     = {75--82},
  publisher = {The Eurographics Association},
  doi       = {10.2312/PE/NPAR/NPAR12/075-082}
}
```
