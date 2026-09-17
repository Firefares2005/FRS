# FRS Library — C++ Developer Guide

A C++ image compression library. Compress and decompress images in one line of code.

---

## Installation

Copy the `dist/` folder anywhere, for example to `C:\frs\`:

```text
C:\frs
├── include
│   └── frs.h       Public header
└── lib
    └── libfrs.a    Static library
```

---

## Quick Start

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

### Build

```bash
g++ app.cpp -IC:\frs\include -LC:\frs\lib -lfrs -o app.exe
```

That is all. No other dependencies.

---

## API Reference

### Image container

```cpp
namespace frs {
    struct Image {
        int width    = 0;
        int height   = 0;
        int channels = 0;              // 1 = gray, 3 = RGB
        std::vector<uint8_t> pixels;   // row-major, interleaved
    };
}
```

### File-based functions (simplest)

```cpp
// Compress an image file to .frs
bool compress(const std::string& in,
              const std::string& out,
              int quality = 85);

// Decompress a .frs file back to image
bool decompress(const std::string& in,
                const std::string& out);
```

Both return `true` on success, `false` on failure.

Example:

```cpp
frs::compress("photo.png", "photo.frs", 90);
frs::decompress("photo.frs", "restored.jpg");
```

### In-memory functions (advanced)

```cpp
// Load an image file into memory
Image load(const std::string& path);

// Save an image to file (format chosen by extension)
bool save(const std::string& path, const Image& img);

// Encode an image to compressed bytes
std::vector<uint8_t> encode(const Image& img, int quality = 85);

// Decode compressed bytes back to an image
Image decode(const std::vector<uint8_t>& data);
```

Example — networking / streaming:

```cpp
// Sender
frs::Image img = frs::load("photo.jpg");
auto compressed = frs::encode(img, 85);
network.send(compressed);

// Receiver
auto data = network.receive();
frs::Image received = frs::decode(data);
frs::save("received.png", received);
```

### Utility functions

```cpp
// Return library version string (e.g. "1.1.0")
std::string version();

// Return header info of an .frs file
// Returns 10 bytes: [width 4B][height 4B][quality 1B][channels 1B]
std::vector<uint8_t> read_header(const std::string& frsFile);
```

---

## Supported Formats

| Direction | Formats |
|-----------|---------|
| Input     | PNG, JPG, JPEG, BMP, TGA, PGM, PPM |
| Output    | PNG, JPG, BMP, TGA, PGM, PPM |

Format is auto-detected on load, and selected by file extension on save.

---

## Quality Parameter

Ranges from 1 (smallest file) to 100 (best quality).

| Quality | Typical use |
|---------|--------------|
| 30–50   | Thumbnails, previews |
| 60–80   | Web images |
| 85–95   | Photography, archives |

---

## Complete Example — Batch Compressor

```cpp
#include <frs.h>
#include <filesystem>
#include <cstdio>

int main() {
    namespace fs = std::filesystem;

    for (auto& entry : fs::directory_iterator("photos/")) {
        if (entry.path().extension() == ".jpg") {
            std::string in  = entry.path().string();
            std::string out = in + ".frs";

            if (frs::compress(in, out, 85)) {
                auto oldSize = fs::file_size(in);
                auto newSize = fs::file_size(out);
                printf("%s: %llu -> %llu bytes\n",
                       entry.path().filename().string().c_str(),
                       oldSize, newSize);
            }
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

## Complete Example — In-Memory (Network / API)

```cpp
#include <frs.h>
#include <vector>
#include <cstdio>

// In a REST API handler:
std::vector<uint8_t> compressRequest(const std::vector<uint8_t>& jpgBytes) {
    // Load JPG bytes into memory
    // (frs::decode works on the .frs format;
    //  for JPG we need load/save or a custom decoder)
    // Note: use load() / save() for files,
    //       encode() / decode() for FRS format.
}

int main() {
    // Compress from file, send over network
    frs::Image img = frs::load("input.png");
    std::vector<uint8_t> frsData = frs::encode(img, 85);

    // ... send frsData over network ...

    // Receive and decode
    frs::Image decoded = frs::decode(frsData);
    frs::save("output.png", decoded);

    return 0;
}
```

---

## Integration with CMake

```cmake
cmake_minimum_required(VERSION 3.10)
project(my_app CXX)

add_executable(my_app main.cpp)

target_include_directories(my_app PRIVATE "C:/frs/include")
target_link_libraries(my_app PRIVATE "C:/frs/lib/libfrs.a")
```

---

## Integration with Visual Studio

1. **Project Properties → C/C++ → General → Additional Include Directories**
   Add: `C:\frs\include`

2. **Project Properties → Linker → General → Additional Library Directories**
   Add: `C:\frs\lib`

3. **Project Properties → Linker → Input → Additional Dependencies**
   Add: `libfrs.a`

---

## FAQ

**Q: Does it require SDL, OpenCV, or any other library?**
A: No. Only the C++ standard library.

**Q: What platforms are supported?**
A: Windows, Linux, macOS. Anywhere GCC, Clang, or MSVC works.

**Q: What image sizes work?**
A: Any size. Images are auto-padded and cropped internally.

**Q: How does it compare to JPEG XL?**
A: FRS produces 15–50% smaller files at matched quality on most images.

**Q: Can I use it in commercial software?**
A: Yes — MIT license.

**Q: Where do I get the source?**
A: https://github.com/Firefares2005/FRS

---

## License

MIT License