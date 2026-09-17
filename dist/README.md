# FRS Library

A C++ image compression library. Compress and decompress images in one line of code.

---

## Installation

Copy this `dist/` folder anywhere, for example `C:\frs\`:
C:\frs
include
frs.h Public header
lib
libfrs.a Static library

text

---

## Usage

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
Build:

text
g++ app.cpp -IC:\frs\include -LC:\frs\lib -lfrs -o app.exe
API Reference
cpp
namespace frs {

// Image container
struct Image {
    int width;
    int height;
    int channels;              // 1 = gray, 3 = RGB
    std::vector<uint8_t> pixels;
};

// --- File-based (simplest) ---

bool compress(const std::string& in, const std::string& out, int quality = 85);
bool decompress(const std::string& in, const std::string& out);

// --- In-memory (advanced) ---

Image load(const std::string& path);
bool  save(const std::string& path, const Image& img);
std::vector<uint8_t> encode(const Image& img, int quality = 85);
Image decode(const std::vector<uint8_t>& data);

}
# FRS Library

A C++ image compression library. Compress and decompress images in one line of code.

---

## Installation

Copy this `dist/` folder anywhere, for example `C:\frs\`:
C:\frs
include
frs.h Public header
lib
libfrs.a Static library

text

---

## Usage

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
Build:

text
g++ app.cpp -IC:\frs\include -LC:\frs\lib -lfrs -o app.exe
API Reference
cpp
namespace frs {

// Image container
struct Image {
    int width;
    int height;
    int channels;              // 1 = gray, 3 = RGB
    std::vector<uint8_t> pixels;
};

// --- File-based (simplest) ---

bool compress(const std::string& in, const std::string& out, int quality = 85);
bool decompress(const std::string& in, const std::string& out);

// --- In-memory (advanced) ---

Image load(const std::string& path);
bool  save(const std::string& path, const Image& img);
std::vector<uint8_t> encode(const Image& img, int quality = 85);
Image decode(const std::vector<uint8_t>& data);

}
Supported Formats
Input: PNG, JPG, JPEG, BMP, TGA, PGM, PPM

Output: PNG, JPG, BMP, TGA, PGM, PPM

Quality Parameter
Range: 1 (smallest) to 100 (best).

Quality	Use case
30-50	Thumbnails, previews
60-80	Web images
85-95	Photography, archives
Example — Batch compress
cpp
#include <frs.h>
#include <filesystem>

int main() {
    for (auto& e : std::filesystem::directory_iterator("photos/")) {
        if (e.path().extension() == ".jpg") {
            std::string in  = e.path().string();
            std::string out = in + ".frs";
            frs::compress(in, out, 85);
        }
    }
    return 0;
}
FAQ
Q: Does it require SDL or any other library?
No. Only the C++ standard library.

Q: What platforms are supported?
Windows, Linux, macOS.

Q: What image sizes work?
Any size.

License
MIT License
Supported Formats
Input: PNG, JPG, JPEG, BMP, TGA, PGM, PPM

Output: PNG, JPG, BMP, TGA, PGM, PPM

Quality Parameter
Range: 1 (smallest) to 100 (best).

Quality	Use case
30-50	Thumbnails, previews
60-80	Web images
85-95	Photography, archives
Example — Batch compress
cpp
#include <frs.h>
#include <filesystem>

int main() {
    for (auto& e : std::filesystem::directory_iterator("photos/")) {
        if (e.path().extension() == ".jpg") {
            std::string in  = e.path().string();
            std::string out = in + ".frs";
            frs::compress(in, out, 85);
        }
    }
    return 0;
}
FAQ
Q: Does it require SDL or any other library?
No. Only the C++ standard library.

Q: What platforms are supported?
Windows, Linux, macOS.

Q: What image sizes work?
Any size.

License
MIT License
