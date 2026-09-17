#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace frs {

struct Image {
    int width    = 0;
    int height   = 0;
    int channels = 0;
    std::vector<uint8_t> pixels;
};

std::vector<uint8_t> encode(const Image& img, int quality = 85);
Image decode(const std::vector<uint8_t>& data);
Image load(const std::string& path);
bool save(const std::string& path, const Image& img);
bool compress(const std::string& in, const std::string& out, int quality = 85);
bool decompress(const std::string& in, const std::string& out);

} // namespace frs
