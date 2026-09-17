#include "encoder.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <iostream>

using namespace codec;

static void usage(const char* a0) {
    std::fprintf(stderr,
        "new-codec CLI\n"
        "  %s encode <input.pgm> <output.nc> [quality=80]\n"
        "  %s decode <input.nc>  <output.pgm>\n",
        a0, a0);
}

int main(int argc, char** argv) {
    if (argc < 4) { usage(argv[0]); return 1; }
    std::string cmd = argv[1];

    try {
        if (cmd == "encode") {
            int q = (argc >= 5) ? std::atoi(argv[4]) : 80;
            Image img = readPGM(argv[2]);
            auto data = encodeImage(img, q);
            std::ofstream f(argv[3], std::ios::binary);
            f.write((const char*)data.data(), (std::streamsize)data.size());
            double ratio = (double)img.pixels.size() / (double)data.size();
            std::fprintf(stderr,
                "encoded %dx%d -> %zu bytes (q=%d, ratio=%.2fx)\n",
                img.width, img.height, data.size(), q, ratio);
        } else if (cmd == "decode") {
            std::ifstream f(argv[2], std::ios::binary);
            std::vector<uint8_t> data(
                (std::istreambuf_iterator<char>(f)),
                 std::istreambuf_iterator<char>());
            Image img = decodeImage(data);
            writePGM(argv[3], img);
            std::fprintf(stderr, "decoded %dx%d\n", img.width, img.height);
        } else {
            usage(argv[0]);
            return 1;
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 2;
    }
    return 0;
}