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

// ★ NC08.1: Bilinear chroma upsampling
static inline float bilinearSample(const std::vector<float>& src,
                                   int sw, int sh,
                                   float fx, float fy) {
    int x0 = (int)fx, y0 = (int)fy;
    int x1 = x0 + 1, y1 = y0 + 1;
    if (x1 >= sw) x1 = sw - 1;
    if (y1 >= sh) y1 = sh - 1;
    float tx = fx - x0, ty = fy - y0;
    float v00 = src[y0 * sw + x0];
    float v10 = src[y0 * sw + x1];
    float v01 = src[y1 * sw + x0];
    float v11 = src[y1 * sw + x1];
    return (1-tx)*(1-ty)*v00 + tx*(1-ty)*v10
         + (1-tx)*ty*v01    + tx*ty*v11;
}

// ★ NC08.1: Adaptive bilateral-like filter (يزيل ضوضاء التكميم من المساحات الناعمة)
static void postFilter(std::vector<uint8_t>& pix, int W, int H, int C) {
    if (W < 3 || H < 3) return;
    std::vector<uint8_t> out = pix;

    // نعمل على قناة Y فقط (القناة 0 في YCbCr المرئية):
    // في الواقع نخفف جميع القنوات لكن بشدة مختلفة.
    // نبسّط: نعبر كل بكسل، نرى التباين المحلي، إن كان منخفضاً نطبّق mean.

    int chans = C;
    for (int y = 1; y < H - 1; y++) {
        for (int x = 1; x < W - 1; x++) {
            for (int c = 0; c < chans; c++) {
                int i = (y * W + x) * chans + c;

                // التباين المحلي (3x3)
                int vmin = 255, vmax = 0, sum = 0;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        int v = pix[((y+dy) * W + (x+dx)) * chans + c];
                        if (v < vmin) vmin = v;
                        if (v > vmax) vmax = v;
                        sum += v;
                    }
                int range = vmax - vmin;
                int mean = sum / 9;

                // إن كانت المساحة ناعمة (range < 12) → ناعم قليلاً
                // إن كانت حافة (range > 30) → احتفظ
                if (range < 12) {
                    out[i] = (uint8_t)mean;
                } else if (range < 30) {
                    // دمج خفيف
                    out[i] = (uint8_t)((pix[i] * 2 + mean) / 3);
                }
                // else: احتفظ بالقيمة الأصلية
            }
        }
    }
    pix.swap(out);
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

    std::vector<float> Yv = decodePlane(dec, wm, W, H, step);

    if (!isColor) {
        for (int i = 0; i < W * H; i++) {
            int v = (int)std::lround(Yv[i] + 128.0f);
            if (v < 0) v = 0; if (v > 255) v = 255;
            img.pixels[i] = (uint8_t)v;
        }
        // ★ filter
        postFilter(img.pixels, W, H, 1);
        return img;
    }

    for (int i = 0; i < W * H; i++) Yv[i] += 128.0f;

    if (use444) {
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
        int sw = (W + 1) / 2, sh = (H + 1) / 2;
        std::vector<float> Cbv = decodePlane(dec, wm, sw, sh, step);
        std::vector<float> Crv = decodePlane(dec, wm, sw, sh, step);

        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                int i = y * W + x;
                float Y  = Yv[i];

                // ★ NC08.1: Bilinear upsampling (بدل nearest neighbor)
                float fx = (x - 0.5f) * 0.5f;
                float fy = (y - 0.5f) * 0.5f;
                if (fx < 0) fx = 0;
                if (fy < 0) fy = 0;
                if (fx > sw - 1.001f) fx = sw - 1.001f;
                if (fy > sh - 1.001f) fy = sh - 1.001f;

                float Cb = bilinearSample(Cbv, sw, sh, fx, fy);
                float Cr = bilinearSample(Crv, sw, sh, fx, fy);

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

    // ★ filter على الصورة الملوّنة
    postFilter(img.pixels, W, H, 3);

    return img;
}

} // namespace codec