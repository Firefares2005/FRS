#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace codec {

struct Image {
    int width  = 0;
    int height = 0;
    int channels = 1;   // 1 = grayscale, 3 = RGB
    std::vector<uint8_t> pixels;
};

std::vector<uint8_t> encodeImage(const Image& img, int quality);
Image                decodeImage(const std::vector<uint8_t>& data);

// I/O
Image readImage (const std::string& path);   // يكتشف P5/P6 تلقائياً
void  writeImage(const std::string& path, const Image& img,
                 bool color = false);

} // namespace codec