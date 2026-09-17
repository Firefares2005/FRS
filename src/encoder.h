#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace codec {

struct Image {
    int width  = 0;
    int height = 0;
    std::vector<uint8_t> pixels; // grayscale
};

// الصورة → بايتات مضغوطة
std::vector<uint8_t> encodeImage(const Image& img, int quality);

// بايتات مضغوطة → صورة
Image decodeImage(const std::vector<uint8_t>& data);

// PGM (P5) I/O
Image readPGM (const std::string& path);
void  writePGM(const std::string& path, const Image& img);

} // namespace codec