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

Image decodeImage(const std::vector<uint8_t>& data) {
    if (data.size() < 14) throw std::runtime_error("decodeImage: too small");
    if (!(data[0]=='N' && data[1]=='C' && data[2]=='0' && data[3]=='6'))
        throw std::runtime_error("decodeImage: bad magic (expect NC06)");

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

    int isColor = dec.decodeBit(wm.sigCtx[0]);

    Image img;
    img.width = W; img.height = H;
    img.channels = isColor ? 3 : 1;
    img.pixels.assign((size_t)W * H * img.channels, 0);

    int sw = (W + 1) / 2, sh = (H + 1) / 2;

    // ---- decode Y plane ----
    {
        int pw = W, ph = H;
        int pl = computeLevels(pw, ph);
        SpihtTree tree = buildSpihtTree(pw, ph, pl);
        std::vector<int> qc = spihtDecode(dec, wm, tree);

        std::vector<float> plane(pw * ph);
        for (int i = 0; i < pw * ph; i++) plane[i] = (float)qc[i] * step;
        idwt2d(plane, pw, ph, pl);

        if (!isColor) {
            for (int i = 0; i < W * H; i++) {
                int v = (int)std::lround(plane[i] + 128.0f);
                if (v < 0) v = 0; if (v > 255) v = 255;
                img.pixels[i] = (uint8_t)v;
            }
            return img;
        }

        std::vector<float> Yv(W * H);
        for (int i = 0; i < W * H; i++) Yv[i] = plane[i] + 128.0f;

        // Cb
        int plB = computeLevels(sw, sh);
        SpihtTree treeB = buildSpihtTree(sw, sh, plB);
        std::vector<int> qcB = spihtDecode(dec, wm, treeB);
        std::vector<float> Cbv(sw * sh);
        for (int i = 0; i < sw * sh; i++) Cbv[i] = (float)qcB[i] * step;
        idwt2d(Cbv, sw, sh, plB);

        // Cr
        int plR = computeLevels(sw, sh);
        SpihtTree treeR = buildSpihtTree(sw, sh, plR);
        std::vector<int> qcR = spihtDecode(dec, wm, treeR);
        std::vector<float> Crv(sw * sh);
        for (int i = 0; i < sw * sh; i++) Crv[i] = (float)qcR[i] * step;
        idwt2d(Crv, sw, sh, plR);

        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                float Y  = Yv[y * W + x];
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
                int idx = (y * W + x) * 3;
                img.pixels[idx + 0] = clip(R);
                img.pixels[idx + 1] = clip(G);
                img.pixels[idx + 2] = clip(B);
            }
        }
    }

    return img;
}

} // namespace codec