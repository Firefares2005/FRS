#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#include "image_io.h"
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <cstring>

namespace codec {

Image readImageAuto(const std::string& path) {
    int w = 0, h = 0, channels = 0;
    uint8_t* data = stbi_load(path.c_str(), &w, &h, &channels, 0);

    if (!data) {
        // fallback إلى PGM/PPM
        try {
            return readImage(path);
        } catch (...) {
            throw std::runtime_error("cannot read image: " + path);
        }
    }

    Image img;
    img.width  = w;
    img.height = h;

    if (channels == 1 || channels == 2) {
        // Grayscale
        img.channels = 1;
        img.pixels.resize((size_t)w * h);
        for (int i = 0; i < w * h; i++)
            img.pixels[i] = data[i * channels];
    } else {
        // RGB أو RGBA → RGB
        img.channels = 3;
        img.pixels.resize((size_t)w * h * 3);
        for (int i = 0; i < w * h; i++) {
            img.pixels[i*3 + 0] = data[i * channels + 0];
            img.pixels[i*3 + 1] = data[i * channels + 1];
            img.pixels[i*3 + 2] = data[i * channels + 2];
        }
    }

    stbi_image_free(data);
    return img;
}

void writeImageAuto(const std::string& path, const Image& img) {
    // استخرج الامتداد
    std::string ext;
    size_t dot = path.rfind('.');
    if (dot != std::string::npos) {
        ext = path.substr(dot + 1);
        for (auto& c : ext) c = (char)std::tolower(c);
    }

    const int W = img.width, H = img.height;
    const int C = img.channels;
    const uint8_t* pix = img.pixels.data();

    if (ext == "png") {
        stbi_write_png(path.c_str(), W, H, C, pix, W * C);
    } else if (ext == "jpg" || ext == "jpeg") {
        if (C != 3 && C != 1)
            throw std::runtime_error("JPEG: only gray or RGB supported");
        stbi_write_jpg(path.c_str(), W, H, C, pix, 95);
    } else if (ext == "bmp") {
        stbi_write_bmp(path.c_str(), W, H, C, pix);
    } else if (ext == "tga") {
        stbi_write_tga(path.c_str(), W, H, C, pix);
    } else {
        // fallback إلى PGM/PPM
        writeImage(path, img, C == 3);
    }
}

} // namespace codec