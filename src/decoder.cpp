#include "encoder.h"
#include "wavelet.h"
#include "spiht.h"

#include <cmath>
#include <cstring>
#include <stdexcept>
#include <algorithm>

namespace codec {

static int computeLevels(int W, int H) {
    int s = std::min(W, H);
    int lv = 0;
    while (s > 8 && lv < 6) { s /= 2; lv++; }
    return lv < 1 ? 1 : lv;
}

// ★ فك plane واحد وإرجاعه بحجمه الكامل
static std::vector<float> decodePlane(RangeDecoder& dec, SpihtModels& m,
                                      int pw, int ph, float step) {
    int pl = computeLevels(pw, ph);
    SpihtTree tree = buildSpihtTree(pw, ph, pl);
    std::vector<int> qc = spihtDecode(dec, m, tree);

    std::vector<float> plane(pw * ph);
    for (int i = 0; i < pw * ph; i++)
        plane[i] = (float)qc[i] * step * tree.factor[i];

    idwt2d(plane, pw, ph, pl);
    return plane;
}

Image decodeImage(const std::vector<uint8_t>& data) {
    if (data.size() < 14) throw std::runtime_error("decodeImage: too small");
    if (!(data[0]=='N' && data[1]=='C' && data[2]=='0' && data[3]=='8'))
        throw std::runtime_error("decodeImage: bad magic (expect NC08)");

    auto rd32 = [&](size_t off) -> uint32_t {
        return  (uint32_t)data[off]
             | ((uint32_t)data[off+1] << 8)
             | ((uint32_t)data[off+2] << 16)
             | ((uint32_t)data[off+3] << 24);
    };

    const int W = (int)rd32(4);
    const int H = (int)rd32(8);
    const int quality = (int)data[12];
    const int C = (int)data[13];
    if (W <= 0 || H <= 0) throw std::runtime_error("decodeImage: bad size");

    double qn = (100 - std::min(100, std::max(0, quality))) / 100.0;
    float step = 1.0f + (float)(qn * 80.0f);

    RangeDecoder dec(data.data() + 14, data.size() - 14);
    SpihtModels  wm;

    int isColor = dec.decodeBit(wm.sigCtx[0][0][0]);
    int use444  = 0;
    if (isColor)
        use444 = dec.decodeBit(wm.sigCtx[1][0][0]);

    Image img;
    img.width = W; img.height = H;
    img.channels = isColor ? 3 : 1;
    img.pixels.assign((size_t)W * H * img.channels, 0);

    // ---- Y plane ----
    std::vector<float> Yv = decodePlane(dec, wm, W, H, step);

    if (!isColor) {
        for (int i = 0; i < W * H; i++) {
            int v = (int)std::lround(Yv[i] + 128.0f);
            if (v < 0) v = 0; if (v > 255) v = 255;
            img.pixels[i] = (uint8_t)v;
        }
        return img;
    }

    for (int i = 0; i < W * H; i++) Yv[i] += 128.0f;

    if (use444) {
        // 4:4:4
        std::vector<float> Cbv = decodePlane(dec, wm, W, H, step);
        std::vector<float> Crv = decodePlane(dec, wm, W, H, step);

        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                int i = y * W + x;
                float Y  = Yv[i];
                float Cb = Cbv[i];
                float Cr = Crv[i];
                float R = Y + 1.402f * Cr;
                float G = Y - 0.344136f * Cb - 0.714136f * Cr;
                float B = Y + 1.772f * Cb;
                auto clip = [](float v) {
                    int i = (int)std::lround(v);
                    return (uint8_t)(i < 0 ? 0 : (i > 255 ? 255 : i));
                };
                int idx = i * 3;
                img.pixels[idx + 0] = clip(R);
                img.pixels[idx + 1] = clip(G);
                img.pixels[idx + 2] = clip(B);
            }
        }
    } else {
        // 4:2:0
        int sw = (W + 1) / 2, sh = (H + 1) / 2;
        std::vector<float> Cbv = decodePlane(dec, wm, sw, sh, step);
        std::vector<float> Crv = decodePlane(dec, wm, sw, sh, step);

        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                int i = y * W + x;
                float Y  = Yv[i];
                int cx = x / 2, cy = y / 2;
                if (cx >= sw) cx = sw - 1;
                if (cy >= sh) cy = sh - 1;
                float Cb = Cbv[cy * sw + cx];
                float Cr = Crv[cy * sw + cx];
                float R = Y + 1.402f * Cr;
                float G = Y - 0.344136f * Cb - 0.714136f * Cr;
                float B = Y + 1.772f * Cb;
                auto clip = [](float v) {
                    int i = (int)std::lround(v);
                    return (uint8_t)(i < 0 ? 0 : (i > 255 ? 255 : i));
                };
                int idx = i * 3;
                img.pixels[idx + 0] = clip(R);
                img.pixels[idx + 1] = clip(G);
                img.pixels[idx + 2] = clip(B);
            }
        }
    }

    return img;
}

} // namespace codec