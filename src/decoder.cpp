#include "encoder.h"
#include "transform.h"
#include "models.h"

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace codec {

Image decodeImage(const std::vector<uint8_t>& data) {
    if (data.size() < 13) throw std::runtime_error("decodeImage: too small");
    if (!(data[0]=='N' && data[1]=='C' && data[2]=='0' && data[3]=='3'))
        throw std::runtime_error("decodeImage: bad magic (expect NC03)");

    auto rd32 = [&](size_t off) -> uint32_t {
        return  (uint32_t)data[off]
             | ((uint32_t)data[off+1] << 8)
             | ((uint32_t)data[off+2] << 16)
             | ((uint32_t)data[off+3] << 24);
    };

    const int W = (int)rd32(4), H = (int)rd32(8);
    const int quality = (int)data[12];
    if (W <= 0 || H <= 0) throw std::runtime_error("decodeImage: bad size");

    auto Q8  = makeQuantTable(quality);
    auto Q16 = makeQuantTable16(quality);

    RangeDecoder dec(data.data() + 13, data.size() - 13);
    CoefModels   m8;
    CoefModels16 m16;

    Image img;
    img.width = W; img.height = H;
    img.pixels.assign((size_t)W * H, 0);

    const int mbx = (W + 15) / 16;
    const int mby = (H + 15) / 16;

    std::vector<int> dcRowPrevM(mbx + 1, 0), dcRowCurM(mbx + 1, 0);
    int dcPrevLeftM = 0;

    auto writeBlock = [&](const Block& blk, int byi, int bxi) {
        for (int y = 0; y < 8; y++) {
            int sy = byi * 8 + y; if (sy >= H) continue;
            for (int x = 0; x < 8; x++) {
                int sx = bxi * 8 + x; if (sx >= W) continue;
                int v = (int)std::lround(blk[y][x] + 128.0f);
                if (v < 0) v = 0; if (v > 255) v = 255;
                img.pixels[(size_t)sy * W + sx] = (uint8_t)v;
            }
        }
    };

    for (int my = 0; my < mby; my++) {
        dcPrevLeftM = 0;
        for (int mx = 0; mx < mbx; mx++) {
            int baseY = my * 16, baseX = mx * 16;

            int dcLeftM = dcPrevLeftM;
            int dcUpM   = dcRowPrevM[mx + 1];
            int dcPredM;
            if (mx == 0 && my == 0) dcPredM = 0;
            else if (mx == 0)       dcPredM = dcUpM;
            else if (my == 0)       dcPredM = dcLeftM;
            else                    dcPredM = (dcLeftM + dcUpM) / 2;

            int ctxM = classifyContext(dcLeftM, dcUpM);

            int use16bit = dec.decodeBit(m16.blockType);
            bool use16 = (use16bit == 0);

            if (use16) {
                int isSkip = dec.decodeBit(m16.skipFlag);
                Block16 coefs16{};
                int dc;
                if (isSkip) {
                    dc = dcPredM;
                    for (int y = 0; y < 16; y++)
                        for (int x = 0; x < 16; x++)
                            coefs16[y][x] = 0.0f;
                    coefs16[0][0] = (float)dc * (float)Q16[0][0];
                } else {
                    int dcRes = unzigzagSigned(
                        decodeUInt(dec, m16.dcLen[ctxM], m16.dcVal[ctxM]));
                    dc = dcPredM + dcRes;

                    int lastNZ = 0;
                    for (int i = 7; i >= 0; i--)
                        lastNZ |= dec.decodeBit(m16.eobBits[i]) << i;

                    int zz[256]; std::memset(zz, 0, sizeof(zz));
                    zz[0] = dc;
                    if (lastNZ > 0) {
                        int pos = 1;
                        while (pos <= lastNZ) {
                            int run = decodeZeroRun(dec, m16.zeroRun);
                            pos += run;
                            if (pos > lastNZ) break;
                            int c = acClass(pos);
                            zz[pos] = unzigzagSigned(
                                decodeUInt(dec, m16.acLen[c][ctxM], m16.acVal[c][ctxM]));
                            pos++;
                        }
                    }
                    for (int y = 0; y < 16; y++)
                        for (int x = 0; x < 16; x++)
                            coefs16[y][x] = (float)zz[ZIGZAG16[y*16+x]] * (float)Q16[y][x];
                }

                Block16 pix16;
                idct2dT<16>(coefs16, pix16);

                for (int y = 0; y < 16; y++) {
                    int sy = baseY + y; if (sy >= H) continue;
                    for (int x = 0; x < 16; x++) {
                        int sx = baseX + x; if (sx >= W) continue;
                        int v = (int)std::lround(pix16[y][x] + 128.0f);
                        if (v < 0) v = 0; if (v > 255) v = 255;
                        img.pixels[(size_t)sy * W + sx] = (uint8_t)v;
                    }
                }

                dcRowCurM[mx+1] = dc;
                dcPrevLeftM     = dc;

            } else {
                // ---------- 4 × 8×8 ----------
                int lastDC = dcPredM;
                for (int sy = 0; sy < 2; sy++) {
                    for (int sx = 0; sx < 2; sx++) {
                        int dcPred = dcPredM;
                        int ctx = ctxM;

                        int isSkip = dec.decodeBit(m8.skipFlag);
                        int zz[64]; std::memset(zz, 0, sizeof(zz));

                        if (isSkip) {
                            zz[0] = dcPred;
                        } else {
                            int dcRes = unzigzagSigned(
                                decodeUInt(dec, m8.dcLen[ctx], m8.dcVal[ctx]));
                            zz[0] = dcPred + dcRes;

                            int lastNZ = 0;
                            for (int i = 5; i >= 0; i--)
                                lastNZ |= dec.decodeBit(m8.eobBits[i]) << i;

                            if (lastNZ > 0) {
                                int pos = 1;
                                while (pos <= lastNZ) {
                                    int run = decodeZeroRun(dec, m8.zeroRun);
                                    pos += run;
                                    if (pos > lastNZ) break;
                                    int c = acClass(pos);
                                    zz[pos] = unzigzagSigned(
                                        decodeUInt(dec, m8.acLen[c][ctx], m8.acVal[c][ctx]));
                                    pos++;
                                }
                            }
                            lastDC = zz[0];
                        }

                        Block coefs{};
                        for (int y = 0; y < 8; y++)
                            for (int x = 0; x < 8; x++)
                                coefs[y][x] = (float)zz[ZIGZAG[y*8+x]] * (float)Q8[y][x];

                        Block blk;
                        idct2d(coefs, blk);
                        writeBlock(blk, my*2 + sy, mx*2 + sx);
                    }
                }
                dcRowCurM[mx+1] = lastDC;
                dcPrevLeftM     = lastDC;
            }
        }
        std::swap(dcRowPrevM, dcRowCurM);
    }

    return img;
}

} // namespace codec