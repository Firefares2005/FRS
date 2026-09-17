# FRS Library — C++ Developer Guide

A C++17 image compression library. Compress and decompress images in one line of code.

> **Version:** 1.1.0  
> **License:** MIT  
> **Platforms:** Windows, Linux, macOS  
> **Dependencies:** None (only C++ standard library)

---

## Table of Contents

1. [Installation](#1-installation)
2. [Quick Start](#2-quick-start)
3. [Image Structure](#3-image-structure)
4. [API Reference](#4-api-reference)
5. [Supported Formats](#5-supported-formats)
6. [Quality Parameter](#6-quality-parameter)
7. [Examples](#7-examples)
8. [Build Integration](#8-build-integration)
9. [FAQ](#9-faq)
10. [License](#10-license)

---

## 1. Installation

### Step 1 — Download

Get the latest library from GitHub Releases:

**https://github.com/Firefares2005/FRS/releases**

Download `FRS-Library-1.1.0.zip`.

### Step 2 — Extract

Extract the archive to any folder, for example `C:\frs\`:

C:\frs
├── include
│ └── frs.h Public header
├── lib
│ └── libfrs.a Static library
└── README.md This file


### Step 3 — Verify

You should now have exactly **two files** that matter:

| File | Purpose |
|------|---------|
| `C:\frs\include\frs.h` | Header (declarations) |
| `C:\frs\lib\libfrs.a` | Library (implementations) |

That is all you need.

---

## 2. Quick Start

### Minimal program

```cpp
#include <frs.h>
#include <cstdio>

int main() {
    // Compress a JPG to FRS
    if (frs::compress("photo.jpg", "photo.frs", 85))
        printf("Compressed!\n");

    // Decompress back to PNG
    if (frs::decompress("photo.frs", "restored.png"))
        printf("Decompressed!\n");

    return 0;
}
```

### Build command

```bash
g++ app.cpp -IC:\frs\include -LC:\frs\lib -lfrs -o app.exe
```

### Build flags explained

| Flag | Meaning |
|------|---------|
| `-IC:\frs\include` | Where to find `frs.h` |
| `-LC:\frs\lib` | Where to find `libfrs.a` |
| `-lfrs` | Link with the FRS library |

---

## 3. Image Structure

All in-memory images use this struct:

```cpp
namespace frs {

struct Image {
    int width    = 0;              // Width in pixels
    int height   = 0;              // Height in pixels
    int channels = 0;              // 1 = grayscale, 3 = RGB
    std::vector<uint8_t> pixels;   // Raw pixel data (row-major)
};

}
```

### Pixel layout

**Grayscale (`channels == 1`):**

pixels.size() == width * height
pixel (x, y) at index: y * width + x


**RGB (`channels == 3`):**

pixels.size() == width * height * 3
pixel (x, y) at index: (y * width + x) * 3 + c
where c = 0 (R), 1 (G), 2 (B)


### Access example

```cpp
frs::Image img = frs::load("photo.jpg");

if (img.channels == 3) {
    int x = 10, y = 20;
    int idx = (y * img.width + x) * 3;
    uint8_t R = img.pixels[idx + 0];
    uint8_t G = img.pixels[idx + 1];
    uint8_t B = img.pixels[idx + 2];
}
```

---

## 4. API Reference

The library exposes **8 functions** in the `frs` namespace.

### 4.1 File-based functions

#### `compress`

```cpp
bool compress(const std::string& in,
              const std::string& out,
              int quality = 85);
```

Compress an image file to `.frs` format.

| Parameter | Type | Description |
|-----------|------|-------------|
| `in` | `std::string` | Path to source image |
| `out` | `std::string` | Path to output `.frs` file |
| `quality` | `int` | 1–100 (default `85`) |
| **Returns** | `bool` | `true` on success |

**Example:**
```cpp
frs::compress("photo.jpg", "photo.frs", 85);
```

---

#### `decompress`

```cpp
bool decompress(const std::string& in,
                const std::string& out);
```

Decompress a `.frs` file back to an image.

| Parameter | Type | Description |
|-----------|------|-------------|
| `in` | `std::string` | Path to `.frs` file |
| `out` | `std::string` | Path to output image |
| **Returns** | `bool` | `true` on success |

**Example:**
```cpp
frs::decompress("photo.frs", "restored.png");
```

---

### 4.2 In-memory functions

#### `load`

```cpp
Image load(const std::string& path);
```

Load an image file into memory.

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `std::string` | Path to source image |
| **Returns** | `Image` | Loaded image, or empty `Image` on failure |

**Example:**
```cpp
frs::Image img = frs::load("photo.jpg");
printf("%dx%d, %d channels\n", img.width, img.height, img.channels);
```

---

#### `save`

```cpp
bool save(const std::string& path, const Image& img);
```

Save an `Image` to a file. Format is chosen by the extension.

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `std::string` | Output path (extension determines format) |
| `img` | `const Image&` | Image to save |
| **Returns** | `bool` | `true` on success |

**Example:**
```cpp
frs::Image img = frs::load("photo.jpg");
frs::save("copy.png", img);
```

---

#### `encode`

```cpp
std::vector<uint8_t> encode(const Image& img, int quality = 85);
```

Encode an `Image` into FRS compressed bytes.

| Parameter | Type | Description |
|-----------|------|-------------|
| `img` | `const Image&` | Image to compress |
| `quality` | `int` | 1–100 (default `85`) |
| **Returns** | `std::vector<uint8_t>` | Compressed data |

**Example — networking:**
```cpp
frs::Image img = frs::load("photo.jpg");
auto compressed = frs::encode(img, 85);
network.send(compressed);
```

---

#### `decode`

```cpp
Image decode(const std::vector<uint8_t>& data);
```

Decode FRS compressed bytes back to an `Image`.

| Parameter | Type | Description |
|-----------|------|-------------|
| `data` | `const std::vector<uint8_t>&` | FRS compressed data |
| **Returns** | `Image` | Decoded image |

**Example — networking:**
```cpp
auto received = network.receive();
frs::Image img = frs::decode(received);
frs::save("received.png", img);
```

---

### 4.3 Utility functions

#### `version`

```cpp
std::string version();
```

Return the library version string.

**Example:**
```cpp
printf("FRS %s\n", frs::version().c_str());
// Output: FRS 1.1.0
```

---

#### `read_header`

```cpp
std::vector<uint8_t> read_header(const std::string& frsFile);
```

Return the metadata header of an FRS file (10 bytes).

| Byte range | Meaning |
|------------|---------|
| `[0–3]` | Width (little-endian `uint32`) |
| `[4–7]` | Height (little-endian `uint32`) |
| `[8]` | Quality (1–100) |
| `[9]` | Channels (1 or 3) |

**Example:**
```cpp
auto h = frs::read_header("photo.frs");
if (h.size() >= 10) {
    uint32_t w  = h[0] | (h[1]<<8) | (h[2]<<16) | (h[3]<<24);
    uint32_t ht = h[4] | (h[5]<<8) | (h[6]<<16) | (h[7]<<24);
    int q       = h[8];
    int c       = h[9];
    printf("Size: %ux%u, quality: %d, channels: %d\n", w, ht, q, c);
}
```

---

### 4.4 API Summary

| # | Function | Purpose |
|---|----------|---------|
| 1 | `compress(in, out, q)` | File → File |
| 2 | `decompress(in, out)` | File → File |
| 3 | `load(path)` | File → Image |
| 4 | `save(path, img)` | Image → File |
| 5 | `encode(img, q)` | Image → bytes |
| 6 | `decode(bytes)` | bytes → Image |
| 7 | `version()` | Version string |
| 8 | `read_header(frs)` | 10-byte metadata |

---

## 5. Supported Formats

| Direction | Formats |
|-----------|---------|
| **Input**  | PNG, JPG, JPEG, BMP, TGA, PGM, PPM |
| **Output** | PNG, JPG, BMP, TGA, PGM, PPM |

- **Input**: format is auto-detected from file contents
- **Output**: format is selected from file extension
- **Internal**: RGB or grayscale, 8-bit per channel

---

## 6. Quality Parameter

Quality ranges from **1** (smallest file) to **100** (best quality).

| Quality | Use case | Typical result |
|---------|----------|----------------|
| 20–40 | Thumbnails, previews | Very small files |
| 50–70 | Web images, social media | Small files |
| 75–85 | General purpose (default) | Balanced |
| 90–95 | Photography, archives | Larger files |
| 96–100 | Near-lossless | Very large files |

**Recommendation:** start with `85` and adjust based on your needs.

---

## 7. Examples

### 7.1 Basic compression

```cpp
#include <frs.h>
#include <cstdio>

int main() {
    if (!frs::compress("input.jpg", "output.frs", 85)) {
        printf("Compression failed\n");
        return 1;
    }
    printf("Compressed successfully\n");
    return 0;
}
```

---

### 7.2 Compress with metadata reporting

```cpp
#include <frs.h>
#include <cstdio>

int main() {
    // Compress
    if (!frs::compress("photo.jpg", "photo.frs", 90)) return 1;

    // Read metadata
    auto h = frs::read_header("photo.frs");
    if (h.size() >= 10) {
        uint32_t w  = h[0] | (h[1]<<8) | (h[2]<<16) | (h[3]<<24);
        uint32_t ht = h[4] | (h[5]<<8) | (h[6]<<16) | (h[7]<<24);
        printf("Compressed: %ux%u, quality=%d, channels=%d\n",
               w, ht, h[8], h[9]);
    }
    return 0;
}
```

---

### 7.3 Batch compress a folder

```cpp
#include <frs.h>
#include <filesystem>
#include <cstdio>

int main() {
    namespace fs = std::filesystem;

    for (auto& entry : fs::directory_iterator("photos/")) {
        auto ext = entry.path().extension().string();
        if (ext != ".jpg" && ext != ".png" && ext != ".bmp") continue;

        std::string in  = entry.path().string();
        std::string out = in + ".frs";

        if (frs::compress(in, out, 85)) {
            auto oldSize = fs::file_size(in);
            auto newSize = fs::file_size(out);
            printf("OK  %-30s  %llu -> %llu bytes\n",
                   entry.path().filename().string().c_str(),
                   oldSize, newSize);
        } else {
            printf("FAIL %s\n", entry.path().filename().string().c_str());
        }
    }
    return 0;
}
```

Build:
```bash
g++ -std=c++17 -O3 batch.cpp -IC:\frs\include -LC:\frs\lib -lfrs -o batch.exe
```

---

### 7.4 In-memory (network / streaming)

```cpp
#include <frs.h>
#include <vector>
#include <cstdio>

int main() {
    // Load file into memory
    frs::Image img = frs::load("input.png");

    // Compress to FRS bytes
    std::vector<uint8_t> data = frs::encode(img, 85);
    printf("Compressed to %zu bytes\n", data.size());

    // ... send data over network ...

    // Decode received bytes
    frs::Image decoded = frs::decode(data);

    // Save to disk
    frs::save("output.png", decoded);

    return 0;
}
```

---

### 7.5 Modify pixels before saving

```cpp
#include <frs.h>

int main() {
    frs::Image img = frs::load("photo.jpg");

    // Convert to grayscale
    if (img.channels == 3) {
        for (int i = 0; i < img.width * img.height; i++) {
            int idx = i * 3;
            int gray = (img.pixels[idx+0]
                      + img.pixels[idx+1]
                      + img.pixels[idx+2]) / 3;
            img.pixels[idx+0] = gray;
            img.pixels[idx+1] = gray;
            img.pixels[idx+2] = gray;
        }
    }

    frs::save("gray.png", img);
    return 0;
}
```

---

## 8. Build Integration

### 8.1 Command line (g++, clang++)

```bash
g++ app.cpp -IC:\frs\include -LC:\frs\lib -lfrs -o app.exe
```

### 8.2 CMake

```cmake
cmake_minimum_required(VERSION 3.10)
project(my_app CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(my_app main.cpp)

target_include_directories(my_app PRIVATE "C:/frs/include")
target_link_libraries(my_app PRIVATE "C:/frs/lib/libfrs.a")
```

### 8.3 Visual Studio (2019+)

1. **Project Properties → C/C++ → General → Additional Include Directories**  
   Add: `C:\frs\include`

2. **Project Properties → Linker → General → Additional Library Directories**  
   Add: `C:\frs\lib`

3. **Project Properties → Linker → Input → Additional Dependencies**  
   Add: `libfrs.a`

### 8.4 Makefile

```makefile
CXX = g++
CXXFLAGS = -std=c++17 -O3 -IC:/frs/include
LDFLAGS = -LC:/frs/lib -lfrs

app.exe: main.cpp
	$(CXX) $(CXXFLAGS) main.cpp $(LDFLAGS) -o app.exe
```

---

## 9. FAQ

**Q: Does it require SDL, OpenCV, or any other library?**  
A: No. Only the C++ standard library.

**Q: What platforms are supported?**  
A: Windows, Linux, macOS. Anywhere GCC, Clang, or MSVC works.

**Q: What image sizes work?**  
A: Any size. Images are auto-padded to multiples of 64 internally, then cropped back.

**Q: Does it support transparency (alpha channel)?**  
A: Not yet. RGBA images are converted to RGB on load.

**Q: Does it support 16-bit or HDR images?**  
A: Not yet. Only 8-bit images.

**Q: Is it lossless?**  
A: No, FRS is a lossy codec. Quality 96–100 approaches near-lossless.

**Q: How does it compare to JPEG XL?**  
A: FRS produces 15–50% smaller files at matched quality on most benchmark images.

**Q: Can I use it in commercial software?**  
A: Yes — MIT license allows commercial use.

**Q: Where do I get the source code?**  
A: https://github.com/Firefares2005/FRS

**Q: Where do I report bugs?**  
A: https://github.com/Firefares2005/FRS/issues

---

## 10. License

MIT License

Copyright (c) 2026 Firefares2005

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
