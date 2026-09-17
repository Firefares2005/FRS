#include "../src/encoder.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>

using namespace codec;

static void makeGray(Image& img) {
    const int W = 256, H = 256;
    img.width = W; img.height = H; img.channels = 1;
    img.pixels.resize(W * H);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            double v = 128 + 80*std::sin(x*0.05) + 60*std::cos(y*0.07);
            if ((x-128)*(x-128) + (y-128)*(y-128) < 2500) v = 30;
            if (x > 20 && x < 60 && y > 20 && y < 200) v = 220;
            v = std::max(0.0, std::min(255.0, v));
            img.pixels[y*W + x] = (uint8_t)v;
        }
}

static void makeColor(Image& img) {
    const int W = 128, H = 128;
    img.width = W; img.height = H; img.channels = 3;
    img.pixels.resize(W * H * 3);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int idx = (y*W + x)*3;
            img.pixels[idx + 0] = (uint8_t)((x * 2) & 0xFF);
            img.pixels[idx + 1] = (uint8_t)((y * 2) & 0xFF);
            img.pixels[idx + 2] = (uint8_t)(((x + y)) & 0xFF);
        }
}

static double psnr(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    double mse = 0;
    for (size_t i = 0; i < a.size(); i++) {
        double d = (double)a[i] - (double)b[i];
        mse += d * d;
    }
    mse /= a.size();
    return mse == 0 ? 1e9 : 10.0 * std::log10(255.0 * 255.0 / mse);
}

int main() {
    bool ok = true;

    // -------- grayscale --------
    {
        Image img; makeGray(img);
        std::printf("--- grayscale 256x256 ---\n");
        for (int q : {30, 50, 80, 95}) {
            auto enc = encodeImage(img, q);
            Image dec = decodeImage(enc);
            double p = psnr(img.pixels, dec.pixels);
            std::printf("q=%2d  %6zu B  ratio=%5.2fx  PSNR=%6.2f dB\n",
                        q, enc.size(),
                        (double)img.pixels.size() / enc.size(), p);
            if (p < 25.0) ok = false;
        }
    }

    // -------- color --------
    {
        Image img; makeColor(img);
        std::printf("--- color 128x128 ---\n");
        for (int q : {50, 80}) {
            auto enc = encodeImage(img, q);
            Image dec = decodeImage(enc);
            double p = psnr(img.pixels, dec.pixels);
            std::printf("q=%2d  %6zu B  ratio=%5.2fx  PSNR=%6.2f dB\n",
                        q, enc.size(),
                        (double)img.pixels.size() / enc.size(), p);
            if (p < 20.0) ok = false;
        }
    }

    if (!ok) { std::fprintf(stderr, "FAIL\n"); return 1; }
    std::printf("NC04 Roundtrip OK\n");
    return 0;
}