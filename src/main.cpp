#include "encoder.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

using namespace codec;

static void usage(const char* a0) {
    std::fprintf(stderr,
        "new-codec v4 (NC04 wavelet)\n"
        "  %s encode <in.pgm|in.ppm> <out.nc> [quality=80]\n"
        "  %s decode <in.nc> <out.pgm|out.ppm>\n",
        a0, a0);
}

int main(int argc, char** argv) {
    if (argc < 4) { usage(argv[0]); return 1; }
    std::string cmd = argv[1];

    try {
        if (cmd == "encode") {
            int q = (argc >= 5) ? std::atoi(argv[4]) : 80;
            Image img = readImage(argv[2]);
            auto data = encodeImage(img, q);
            std::ofstream f(argv[3], std::ios::binary);
            f.write((const char*)data.data(), (std::streamsize)data.size());

            size_t raw = img.pixels.size();
            std::fprintf(stderr,
                "encoded %dx%d ch=%d -> %zu bytes (q=%d, ratio=%.2fx)\n",
                img.width, img.height, img.channels,
                data.size(), q, (double)raw / data.size());
        } else if (cmd == "decode") {
            std::ifstream f(argv[2], std::ios::binary);
            std::vector<uint8_t> data(
                (std::istreambuf_iterator<char>(f)),
                 std::istreambuf_iterator<char>());
            Image img = decodeImage(data);
            writeImage(argv[3], img, img.channels == 3);
            std::fprintf(stderr, "decoded %dx%d ch=%d\n",
                         img.width, img.height, img.channels);
        } else {
            usage(argv[0]); return 1;
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 2;
    }
    return 0;
}