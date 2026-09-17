#include "encoder.h"
#include "wavelet.h"
#include "models.h"

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

// -------------------- فك ترميز plane (NC05) --------------------
static std::vector<int> decodePlane(RangeDecoder& dec, WaveletModels& m,
                                    int w, int h, int L) {
    int N = w * h;
    std::vector<int> qc(N, 0);

    CoefInfo info = prepareCoefInfo(w, h, L);

    // Pass 1: IS_NZ
    std::vector<uint8_t> nz(N, 0);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int i = y * w + x;
            int nNZ = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = x + dx, ny = y + dy;
                    if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                    if (nz[ny * w + nx]) nNZ++;
                }
            if (nNZ > 4) nNZ = 4;
            nz[i] = (uint8_t)dec.decodeBit(m.nzCtx[nNZ][info.subband[i]]);
        }
    }

    int maxBit = (int)decodeUInt(dec, m.maxBitLen, m.maxBitVal);

    // Pass 2: bit planes
    std::vector<uint8_t> sig(N, 0);
    for (int bit = maxBit; bit >= 0; bit--) {
        int mask = 1 << bit;
        for (int idx = 0; idx < N; idx++) {
            int i = info.order[idx];
            if (!nz[i]) continue;

            int y = i / w, x = i % w;
            int sb = info.subband[i];

            if (!sig[i]) {
                int nsig = 0;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = x + dx, ny = y + dy;
                        if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                        if (sig[ny * w + nx]) nsig++;
                    }
                if (nsig > 3) nsig = 3;

                int parentSig = 0;
                int pi = info.parent[i];
                if (pi >= 0 && sig[pi]) parentSig = 1;

                int isSig = dec.decodeBit(m.sigCtx[nsig][sb][parentSig]);
                if (isSig) {
                    int s = dec.decodeBit(m.signCtx[sb]);
                    qc[i] = mask;
                    if (s) qc[i] = -qc[i];
                    sig[i] = 1;
                }
            } else {
                int b = dec.decodeBit(m.refCtx[sb]);
                if (b) {
                    if (qc[i] > 0) qc[i] |= mask;
                    else           qc[i] = -((-qc[i]) | mask);
                }
            }
        }
    }

    return qc;
}

// -------------------- الفك الكامل --------------------
Image decodeImage(const std::vector<uint8_t>& data) {
    if (data.size() < 14) throw std::runtime_error("decodeImage: too small");
    if (!(data[0]=='N' && data[1]=='C' && data[2]=='0' && data[3]=='5'))
        throw std::runtime_error("decodeImage: bad magic (expect NC05)");

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

    const int levels = computeLevels(W, H);

    // نفس معادلة التكميم (يجب أن تطابق encoder)
    double qn = (100 - std::min(100, std::max(0, quality))) / 100.0;
    float step = 1.0f + (float)(qn * 80.0f);

    RangeDecoder dec(data.data() + 14, data.size() - 14);
    WaveletModels wm;

    int isColor = dec.decodeBit(wm.nzCtx[0][0]);

    Image img;
    img.width = W; img.height = H;
    img.channels = isColor ? 3 : 1;
    img.pixels.assign((size_t)W * H * img.channels, 0);

    int sw = (W + 1) / 2, sh = (H + 1) / 2;

    {
        std::vector<int> qc = decodePlane(dec, wm, W, H, levels);
        std::vector<float> plane(W * H);
        for (int i = 0; i < W * H; i++) plane[i] = (float)qc[i] * step;
        idwt2d(plane, W, H, levels);

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

        std::vector<int> qcB = decodePlane(dec, wm, sw, sh, levels);
        std::vector<float> Cbv(sw * sh);
        for (int i = 0; i < sw * sh; i++) Cbv[i] = (float)qcB[i] * step;
        idwt2d(Cbv, sw, sh, levels);

        std::vector<int> qcR = decodePlane(dec, wm, sw, sh, levels);
        std::vector<float> Crv(sw * sh);
        for (int i = 0; i < sw * sh; i++) Crv[i] = (float)qcR[i] * step;
        idwt2d(Crv, sw, sh, levels);

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