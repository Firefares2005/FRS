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
          data[2] == '0' && data[3] == '1'))
        throw std::runtime_error("decodeImage: bad magic");

    auto rd32 = [&](size_t off) -> uint32_t {
        return (uint32_t)data[off]
             | ((uint32_t)data[off + 1] << 8)
             | ((uint32_t)data[off + 2] << 16)
             | ((uint32_t)data[off + 3] << 24);
    };

    const int W = (int)rd32(4);
    const int H = (int)rd32(8);
    const int quality = (int)data[12];
    if (W <= 0 || H <= 0) throw std::runtime_error("decodeImage: bad size");

    auto Q = makeQuantTable(quality);

    RangeDecoder dec(data.data() + 13, data.size() - 13);
    CoefModels   models;

    Image img;
    img.width = W;
    img.height = H;
    img.pixels.assign((size_t)W * H, 0);

    const int bx = (W + N - 1) / N;
    const int by = (H + N - 1) / N;

    int prevDC = 0;
    Block coefs, block;

    for (int byi = 0; byi < by; byi++) {
        for (int bxi = 0; bxi < bx; bxi++) {
            // lastNZ
            int lastNZ = 0;
            for (int i = 5; i >= 0; i--)
                lastNZ |= dec.decodeBit(models.eobBits[i]) << i;

            // DC
            int dcdiff = unzigzagSigned(
                decodeUInt(dec, models.dcLen, models.dcVal));
            int dc = prevDC + dcdiff;
            prevDC = dc;

            int zz[64];
            std::memset(zz, 0, sizeof(zz));
            zz[0] = dc;

            // AC
            for (int i = 1; i <= lastNZ; i++) {
                int c = acClass(i);
                zz[i] = unzigzagSigned(
                    decodeUInt(dec, models.acLen[c], models.acVal[c]));
            }

            // دي-تكميم + ترتيب معاكس للـ zigzag
            for (int y = 0; y < N; y++)
                for (int x = 0; x < N; x++)
                    coefs[y][x] = (float)zz[ZIGZAG[y * N + x]] * (float)Q[y][x];

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
    }
    return img;
}

} // namespace codec