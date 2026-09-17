#include "../src/encoder.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>

using namespace codec;

int main() {
    // صورة اختبارية: تدرّج + أشكال
    const int W = 128, H = 128;
    Image img;
    img.width  = W;
    img.height = H;
    img.pixels.resize((size_t)W * H);

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            double v = 128.0 + 100.0 * std::sin(x * 0.10) * std::cos(y * 0.13);
            // دائرة داكنة في المنتصف
            if ((x - 64) * (x - 64) + (y - 64) * (y - 64) < 400) v = 30;
            // مستطيل فاتح
            if (x > 20 && x < 40 && y > 20 && y < 100) v = 220;
            v = std::max(0.0, std::min(255.0, v));
            img.pixels[(size_t)y * W + x] = (uint8_t)v;
        }
    }

    bool ok = true;
    for (int q : {30, 60, 90}) {
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
            (double)img.pixels.size() / (double)enc.size(),
            psnr);

        if (psnr < 20.0) {
            std::fprintf(stderr, "FAIL: PSNR too low (q=%d)\n", q);
            ok = false;
        }
    }

    if (!ok) return 1;
    std::printf("Roundtrip OK\n");
    return 0;
}