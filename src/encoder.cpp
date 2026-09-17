#include "encoder.h"
#include "transform.h"
#include "models.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <cctype>

namespace codec {

// تقدير تكلفة الكتلة 8x8 (مجموع |coef| بعد التكميم)
static long cost8(const Block& pix, const std::array<std::array<int,8>,8>& Q) {
    Block coefs;
    dct2d(pix, coefs);
    long sum = 0;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            sum += std::abs((long)std::lround(coefs[y][x] / (float)Q[y][x]));
    return sum;
}

static long cost16(const Block16& pix, const std::array<std::array<int,16>,16>& Q) {
    Block16 coefs;
    dct2dT<16>(pix, coefs);
    long sum = 0;
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++)
            sum += std::abs((long)std::lround(coefs[y][x] / (float)Q[y][x]));
    return sum;
}

std::vector<uint8_t> encodeImage(const Image& img, int quality) {
    if (img.width <= 0 || img.height <= 0)
        throw std::runtime_error("encodeImage: invalid size");

    auto Q8  = makeQuantTable(quality);
    auto Q16 = makeQuantTable16(quality);

    RangeEncoder enc;
    CoefModels   m8;
    CoefModels16 m16;

    const int W = img.width, H = img.height;
    const int mbx = (W + 15) / 16;  // عدد macroblocks أفقياً
    const int mby = (H + 15) / 16;

    // تتبّع DC على مستوى macroblock (16×16)
    std::vector<int> dcRowPrevM(mbx + 1, 0), dcRowCurM(mbx + 1, 0);
    int dcPrevLeftM = 0;

    // نستخدم أول بكسل DC من الـ macroblock كقيمة تمثيلية
    auto readPix = [&](int y, int x) -> float {
        if (y >= H) y = H - 1;
        if (x >= W) x = W - 1;
        return (float)img.pixels[(size_t)y * W + x] - 128.0f;
    };

    for (int my = 0; my < mby; my++) {
        dcPrevLeftM = 0;
        for (int mx = 0; mx < mbx; mx++) {
            int baseY = my * 16;
            int baseX = mx * 16;

            // ---- قراءة الـ macroblock ----
            Block16 pix16;
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 16; x++)
                    pix16[y][x] = readPix(baseY + y, baseX + x);

            // ---- حساب تكلفة الوضعين ----
            long c16 = cost16(pix16, Q16);
            long c8  = 0;
            Block pix8[2][2];
            for (int sy = 0; sy < 2; sy++)
                for (int sx = 0; sx < 2; sx++) {
                    for (int y = 0; y < 8; y++)
                        for (int x = 0; x < 8; x++)
                            pix8[sy][sx][y][x] = pix16[sy*8 + y][sx*8 + x];
                    c8 += cost8(pix8[sy][sx], Q8);
                }

            // القرار: 16×16 مفيد إذا تكلفته أقل بـ 30% (لتعويض فقدان التفاصيل)
            bool use16 = (c16 * 10 < c8 * 7);   // c16 < 0.7 * c8

            enc.encodeBit(m16.blockType, use16 ? 0 : 1);

            int dcLeftM = dcPrevLeftM;
            int dcUpM   = dcRowPrevM[mx + 1];
            int dcPredM;
            if (mx == 0 && my == 0) dcPredM = 0;
            else if (mx == 0)       dcPredM = dcUpM;
            else if (my == 0)       dcPredM = dcLeftM;
            else                    dcPredM = (dcLeftM + dcUpM) / 2;

            int ctxM = classifyContext(dcLeftM, dcUpM);

            if (use16) {
                // ---------- مسار 16×16 ----------
                Block16 coefs16;
                dct2dT<16>(pix16, coefs16);

                int zz[256];
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 16; x++)
                        zz[ZIGZAG16[y*16 + x]] =
                            (int)std::lround(coefs16[y][x] / (float)Q16[y][x]);

                int lastNZ = 0;
                for (int i = 255; i >= 1; i--)
                    if (zz[i] != 0) { lastNZ = i; break; }

                int dcRes = zz[0] - dcPredM;
                bool skip = (dcRes == 0 && lastNZ == 0);
                enc.encodeBit(m16.skipFlag, skip ? 1 : 0);

                if (skip) {
                    dcRowCurM[mx+1] = dcPredM;
                    dcPrevLeftM     = dcPredM;
                    continue;
                }

                encodeUInt(enc, m16.dcLen[ctxM], m16.dcVal[ctxM],
                           zigzagSigned(dcRes));

                for (int i = 7; i >= 0; i--)
                    enc.encodeBit(m16.eobBits[i], (lastNZ >> i) & 1);

                int pos = 1;
                while (pos <= lastNZ) {
                    int run = 0;
                    while (pos + run <= lastNZ && zz[pos + run] == 0) run++;
                    encodeZeroRun(enc, m16.zeroRun, run);
                    pos += run;
                    if (pos > lastNZ) break;
                    int c = acClass(pos);
                    encodeUInt(enc, m16.acLen[c][ctxM], m16.acVal[c][ctxM],
                               zigzagSigned(zz[pos]));
                    pos++;
                }

                dcRowCurM[mx+1] = zz[0];
                dcPrevLeftM     = zz[0];

            } else {
                // ---------- مسار 4 × 8×8 (NC02) ----------
                for (int sy = 0; sy < 2; sy++) {
                    for (int sx = 0; sx < 2; sx++) {
                        // DC prediction داخل الـ macroblock
                        int dcLeft, dcUp;
                        int subX = mx*2 + sx;
                        int subY = my*2 + sy;

                        if (sx == 0 && sy == 0) {
                            dcLeft = dcLeftM;
                            dcUp   = dcUpM;
                        } else if (sx == 0) {
                            dcLeft = dcPrevLeftM;
                            dcUp   = (subY > 0) ? 0 : 0;  // تقريبي
                        } else {
                            dcLeft = 0;  // سيُحدّث بعد المعالجة
                            dcUp   = 0;
                        }

                        // تبسيط: استخدم dcPredM للجميع
                        (void)dcLeft; (void)dcUp;
                        int dcPred = dcPredM;

                        int ctx = ctxM;

                        Block coefs;
                        dct2d(pix8[sy][sx], coefs);

                        int zz[64];
                        for (int y = 0; y < 8; y++)
                            for (int x = 0; x < 8; x++)
                                zz[ZIGZAG[y*8 + x]] =
                                    (int)std::lround(coefs[y][x] / (float)Q8[y][x]);

                        int lastNZ = 0;
                        for (int i = 63; i >= 1; i--)
                            if (zz[i] != 0) { lastNZ = i; break; }

                        int dcRes = zz[0] - dcPred;
                        bool skip = (dcRes == 0 && lastNZ == 0);
                        enc.encodeBit(m8.skipFlag, skip ? 1 : 0);

                        if (skip) continue;

                        encodeUInt(enc, m8.dcLen[ctx], m8.dcVal[ctx],
                                   zigzagSigned(dcRes));

                        for (int i = 5; i >= 0; i--)
                            enc.encodeBit(m8.eobBits[i], (lastNZ >> i) & 1);

                        int pos = 1;
                        while (pos <= lastNZ) {
                            int run = 0;
                            while (pos + run <= lastNZ && zz[pos + run] == 0) run++;
                            encodeZeroRun(enc, m8.zeroRun, run);
                            pos += run;
                            if (pos > lastNZ) break;
                            int c = acClass(pos);
                            encodeUInt(enc, m8.acLen[c][ctx], m8.acVal[c][ctx],
                                       zigzagSigned(zz[pos]));
                            pos++;
                        }

                        // تحديث القيمة التمثيلية
                        if (sy == 0 && sx == 0) {
                            dcRowCurM[mx+1] = zz[0];
                            dcPrevLeftM     = zz[0];
                        }
                    }
                }
                // إذا لم يُحدَّث (كل الـ 4 تخطّوها)، احتفظ بـ dcPredM
                if (dcRowCurM[mx+1] == 0 && dcPrevLeftM == 0 &&
                    dcPredM != 0) {
                    dcRowCurM[mx+1] = dcPredM;
                    dcPrevLeftM     = dcPredM;
                }
            }
        }
        std::swap(dcRowPrevM, dcRowCurM);
    }

    enc.flush();

    // ---- الترويسة NC03 ----
    std::vector<uint8_t> out;
    out.reserve(enc.data().size() + 13);
    out.push_back('N'); out.push_back('C');
    out.push_back('0'); out.push_back('3');
    auto push32 = [&](uint32_t v) {
        out.push_back((uint8_t)( v        & 0xFF));
        out.push_back((uint8_t)((v >>  8) & 0xFF));
        out.push_back((uint8_t)((v >> 16) & 0xFF));
        out.push_back((uint8_t)((v >> 24) & 0xFF));
    };
    push32((uint32_t)W);
    push32((uint32_t)H);
    out.push_back((uint8_t)quality);
    const auto& body = enc.data();
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

// ---------------- PGM I/O (كما هو) ----------------
Image readPGM(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("readPGM: cannot open " + path);
    std::string magic;
    f >> magic;
    if (magic != "P5") throw std::runtime_error("readPGM: only P5 supported");
    auto skipWS = [&]() {
        while (true) {
            int c = f.peek();
            if (c == '#') { std::string line; std::getline(f, line); }
            else if (std::isspace(c)) f.get();
            else break;
        }
    };
    int W, H, maxv;
    skipWS(); f >> W; skipWS(); f >> H; skipWS(); f >> maxv; f.get();
    Image img; img.width = W; img.height = H;
    img.pixels.resize((size_t)W * H);
    f.read((char*)img.pixels.data(), (std::streamsize)img.pixels.size());
    if (!f) throw std::runtime_error("readPGM: truncated");
    return img;
}

void writePGM(const std::string& path, const Image& img) {
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("writePGM: cannot write " + path);
    f << "P5\n" << img.width << " " << img.height << "\n255\n";
    f.write((const char*)img.pixels.data(), (std::streamsize)img.pixels.size());
}

} // namespace codec