#include "encoder.h"
#include "transform.h"
#include "models.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace codec {

Image decodeImage(const std::vector<uint8_t>& data) {
    if (data.size() < 13)
        throw std::runtime_error("decodeImage: too small");
    if (!(data[0] == 'N' && data[1] == 'C' &&
          data[2] == '0' && data[3] == '2'))
        throw std::runtime_error("decodeImage: bad magic (expect NC02)");

    auto rd32 = [&](size_t off) -> uint32_t {
        return  (uint32_t)data[off]
             | ((uint32_t)data[off + 1] <<  8)
             | ((uint32_t)data[off + 2] << 16)
             | ((uint32_t)data[off + 3] << 24);
    };

    const int W = (int)rd32(4);
    const int H = (int)rd32(8);
    const int quality = (int)data[12];
    if (W <= 0 || H <= 0)
        throw std::runtime_error("decodeImage: bad size");

    auto Q = makeQuantTable(quality);

    RangeDecoder dec(data.data() + 13, data.size() - 13);
    CoefModels   models;

    Image img;
    img.width  = W;
    img.height = H;
    img.pixels.assign((size_t)W * H, 0);

    const int bx = (W + N - 1) / N;
    const int by = (H + N - 1) / N;

    std::vector<int> dcRowPrev(bx + 1, 0);
    std::vector<int> dcRowCur (bx + 1, 0);
    int dcPrevLeft = 0;

    Block coefs, block;

    for (int byi = 0; byi < by; byi++) {
        dcPrevLeft = 0;
        for (int bxi = 0; bxi < bx; bxi++) {
            // 1) التنبؤ بنفس ترتيب الـ encoder
            int dcUp   = dcRowPrev[bxi + 1];
            int dcLeft = dcPrevLeft;
            int dcPred;
            if (bxi == 0 && byi == 0) dcPred = 0;
            else if (bxi == 0)        dcPred = dcUp;
            else if (byi == 0)        dcPred = dcLeft;
            else                      dcPred = (dcLeft + dcUp) / 2;

            // 2) راية التخطي
            int isSkip = dec.decodeBit(models.skipFlag);

            int zz[64];
            std::memset(zz, 0, sizeof(zz));

            if (isSkip) {
                zz[0] = dcPred;
                dcRowCur[bxi + 1] = dcPred;
                dcPrevLeft        = dcPred;
            } else {
                int ctx = classifyContext(dcLeft, dcUp);

                int dcResidual = unzigzagSigned(
                    decodeUInt(dec, models.dcLen[ctx], models.dcVal[ctx]));
                zz[0] = dcPred + dcResidual;

                // اقرأ آخر معامل (6 بتات)
                int lastNZ = 0;
                for (int i = 5; i >= 0; i--)
                    lastNZ |= dec.decodeBit(models.eobBits[i]) << i;

                // AC مع zero-run
                if (lastNZ > 0) {
                    int pos = 1;
                    while (pos <= lastNZ) {
                        int run = decodeZeroRun(dec, models);
                        pos += run;
                        if (pos > lastNZ) break;
                        int c = acClass(pos);
                        zz[pos] = unzigzagSigned(
                            decodeUInt(dec, models.acLen[c][ctx], models.acVal[c][ctx]));
                        pos++;
                    }
                }

                dcRowCur[bxi + 1] = zz[0];
                dcPrevLeft        = zz[0];
            }

            // 3) دي-تكميم + zigzag عكسي
            for (int y = 0; y < N; y++)
                for (int x = 0; x < N; x++)
                    coefs[y][x] = (float)zz[ZIGZAG[y * N + x]] * (float)Q[y][x];

            // 4) IDCT + كتابة البكسلات
            idct2d(coefs, block);

            for (int y = 0; y < N; y++) {
                int sy = byi * N + y;
                if (sy >= H) continue;
                for (int x = 0; x < N; x++) {
                    int sx = bxi * N + x;
                    if (sx >= W) continue;
                    int v = (int)std::lround(block[y][x] + 128.0f);
                    if (v < 0)   v = 0;
                    if (v > 255) v = 255;
                    img.pixels[(size_t)sy * W + sx] = (uint8_t)v;
                }
            }
        }
        std::swap(dcRowPrev, dcRowCur);
    }

    return img;
}

} // namespace codec