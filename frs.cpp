#include "frs.h"
#include "encoder.h"
#include "image_io.h"
#include <cstdio>

namespace frs {

static Image fromCodec(const codec::Image& c) {
    Image i;
    i.width = c.width; i.height = c.height;
    i.channels = c.channels; i.pixels = c.pixels;
    return i;
}

static codec::Image toCodec(const Image& i) {
    codec::Image c;
    c.width = i.width; c.height = i.height;
    c.channels = i.channels; c.pixels = i.pixels;
    return c;
}

std::vector<uint8_t> encode(const Image& img, int quality) {
    return codec::encodeImage(toCodec(img), quality);
}

Image decode(const std::vector<uint8_t>& data) {
    return fromCodec(codec::decodeImage(data));
}

Image load(const std::string& path) {
    return fromCodec(codec::readImageAuto(path));
}

bool save(const std::string& path, const Image& img) {
    codec::writeImageAuto(path, toCodec(img));
    return true;
}

bool compress(const std::string& in, const std::string& out, int quality) {
    Image img = load(in);
    if (img.pixels.empty()) return false;
    auto data = encode(img, quality);
    FILE* f = fopen(out.c_str(), "wb");
    if (!f) return false;
    fwrite(data.data(), 1, data.size(), f);
    fclose(f);
    return true;
}

bool decompress(const std::string& in, const std::string& out) {
    FILE* f = fopen(in.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    size_t sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> data(sz);
    fread(data.data(), 1, sz, f);
    fclose(f);
    return save(out, decode(data));
}

std::string version() {
    return "1.1.0";
}

std::vector<uint8_t> read_header(const std::string& frsFile) {
    FILE* f = fopen(frsFile.c_str(), "rb");
    if (!f) return {};
    uint8_t header[14];
    size_t n = fread(header, 1, 14, f);
    fclose(f);
    if (n < 14) return {};
    if (!(header[0] == 'N' && header[1] == 'C')) return {};
    return std::vector<uint8_t>(header + 4, header + 14);
}

} // namespace frs
