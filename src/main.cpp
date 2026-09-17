#include "encoder.h"
#include "image_io.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

using namespace codec;

static void usage(const char* a0) {
    std::fprintf(stderr,
        "FRS - Fast Range-compressed Subband codec\n"
        "\n"
        "Usage:\n"
        "  %s encode <input> <output.frs> [quality=85]\n"
        "  %s decode <input.frs> <output>\n"
        "\n"
        "Input formats:  PNG, JPG, BMP, TGA, PGM, PPM\n"
        "Output formats: PNG, JPG, BMP, TGA, PGM, PPM\n"
        "\n"
        "Examples:\n"
        "  %s encode photo.jpg photo.frs 85\n"
        "  %s decode photo.frs photo_restored.png\n",
        a0, a0, a0, a0);
}

int main(int argc, char** argv) {
    if (argc < 4) { usage(argv[0]); return 1; }
    std::string cmd = argv[1];

    try {
        if (cmd == "encode") {
            int q = (argc >= 5) ? std::atoi(argv[4]) : 85;
            Image img = readImageAuto(argv[2]);
            auto data = encodeImage(img, q);

            std::ofstream f(argv[3], std::ios::binary);
            if (!f) throw std::runtime_error("cannot write " + std::string(argv[3]));
            f.write((const char*)data.data(), (std::streamsize)data.size());

            size_t raw = img.pixels.size();
            std::fprintf(stderr,
                "encoded %dx%d ch=%d -> %zu bytes (q=%d, ratio=%.2fx)\n",
                img.width, img.height, img.channels,
                data.size(), q, (double)raw / data.size());
        } else if (cmd == "decode") {
            std::ifstream f(argv[2], std::ios::binary);
            if (!f) throw std::runtime_error("cannot open " + std::string(argv[2]));
            std::vector<uint8_t> data(
                (std::istreambuf_iterator<char>(f)),
                 std::istreambuf_iterator<char>());
            Image img = decodeImage(data);
            writeImageAuto(argv[3], img);
            std::fprintf(stderr, "decoded %dx%d ch=%d -> %s\n",
                         img.width, img.height, img.channels, argv[3]);
        } else {
            usage(argv[0]); return 1;
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 2;
    }
    return 0;
}