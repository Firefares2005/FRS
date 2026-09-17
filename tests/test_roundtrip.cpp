#include "../src/encoder.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>

using namespace codec;

int main() {
    const int W = 256, H = 256;
    Image img;
    img.width  = W;
    img.height = H;
    img.pixels.resize((size_t)W * H);

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            double v = 128.0 + 80.0 * std::sin(x * 0.05) + 60.0 * std::cos(y * 0.07);
            if ((x - 128) * (x - 128) + (y - 128) * (y - 128) < 2500) v = 30;
            if (x > 20 && x < 60 && y > 20 && y < 200) v = 220;
            v = std::max(0.0, std::min(255.0, v));
            img.pixels[(size_t)y * W + x] = (uint8_t)v;
        }
    }

    bool ok = true;
    const size_t rawBytes = img.pixels.size();

    for (int q : {30, 50, 80, 95}) {
        auto enc = encodeImage(img, q);
        Image dec = decodeImage(enc);

        if (dec.width != W || dec.height != H) {
            std::fprintf(stderr, "FAIL: size mismatch (q=%d)\n", q);
            ok = false;
            continue;
        }
        double mse = 0.0;
        for (size_t i = 0; i < img.pixels.size(); i++) {
            double d = (double)img.pixels[i] - (double)dec.pixels[i];
            mse += d * d;
        }
        mse /= (double)img.pixels.size();
        double psnr = (mse == 0.0) ? 1e9 : 10.0 * std::log10(255.0 * 255.0 / mse);

        std::printf(
            "q=%2d  compressed=%6zu B  ratio=%5.2fx  PSNR=%6.2f dB\n",
            q, enc.size(),
            (double)rawBytes / (double)enc.size(),
            psnr);

        if (psnr < 25.0) {
            std::fprintf(stderr, "FAIL: PSNR too low (q=%d)\n", q);
            ok = false;
        }
    }

    if (!ok) return 1;
    std::printf("NC02 Roundtrip OK\n");
    return 0;
}