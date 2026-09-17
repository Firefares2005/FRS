#include "encoder.h"
#include "transform.h"
#include "models.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <cctype>

namespace codec {

std::vector<uint8_t> encodeImage(const Image& img, int quality) {
    if (img.width <= 0 || img.height <= 0)
        throw std::runtime_error("encodeImage: invalid image size");

    auto Q = makeQuantTable(quality);

    RangeEncoder enc;
    CoefModels   models;

    const int W = img.width;
    const int H = img.height;
    const int bx = (W + N - 1) / N;
    const int by = (H + N - 1) / N;

    // حالة التنبؤ: DC الصف السابق، الصف الحالي، والجار الأيسر
    std::vector<int> dcRowPrev(bx + 1, 0);
    std::vector<int> dcRowCur (bx + 1, 0);
    int dcPrevLeft = 0;

    Block block, coefs;

    for (int byi = 0; byi < by; byi++) {
        dcPrevLeft = 0;
        for (int bxi = 0; bxi < bx; bxi++) {
            // 1) اقرأ الكتلة مع حشو من الحافة
            for (int y = 0; y < N; y++) {
                int sy = byi * N + y;
                if (sy >= H) sy = H - 1;
                for (int x = 0; x < N; x++) {
                    int sx = bxi * N + x;
                    if (sx >= W) sx = W - 1;
                    block[y][x] = (float)img.pixels[(size_t)sy * W + sx] - 128.0f;
                }
            }

            // 2) DCT + تكميم + zigzag
            dct2d(block, coefs);
            int zz[64];
            for (int y = 0; y < N; y++)
                for (int x = 0; x < N; x++)
                    zz[ZIGZAG[y * N + x]] =
                        (int)std::lround(coefs[y][x] / (float)Q[y][x]);

            // 3) التنبؤ بـ DC
            int dcUp   = dcRowPrev[bxi + 1];
            int dcLeft = dcPrevLeft;
            int dcPred;
            if (bxi == 0 && byi == 0) dcPred = 0;
            else if (bxi == 0)        dcPred = dcUp;
            else if (byi == 0)        dcPred = dcLeft;
            else                      dcPred = (dcLeft + dcUp) / 2;

            int dcResidual = zz[0] - dcPred;

            // 4) آخر معامل غير صفري في AC
            int lastNZ = 0;
            for (int i = 63; i >= 1; i--)
                if (zz[i] != 0) { lastNZ = i; break; }

            // 5) راية التخطي
            bool isSkip = (dcResidual == 0 && lastNZ == 0);
            enc.encodeBit(models.skipFlag, isSkip ? 1 : 0);

            if (isSkip) {
                dcRowCur[bxi + 1] = dcPred;
                dcPrevLeft        = dcPred;
                continue;
            }

            // 6) السياق
            int ctx = classifyContext(dcLeft, dcUp);

            // 7) DC المتبقي
            encodeUInt(enc, models.dcLen[ctx], models.dcVal[ctx],
                       zigzagSigned(dcResidual));

            // 8) آخر معامل (6 بتات)
            for (int i = 5; i >= 0; i--)
                enc.encodeBit(models.eobBits[i], (lastNZ >> i) & 1);

            // 9) AC مع zero-run
            int pos = 1;
            while (pos <= lastNZ) {
                int run = 0;
                while (pos + run <= lastNZ && zz[pos + run] == 0) run++;
                encodeZeroRun(enc, models, run);
                pos += run;
                if (pos > lastNZ) break;
                int c = acClass(pos);
                encodeUInt(enc, models.acLen[c][ctx], models.acVal[c][ctx],
                           zigzagSigned(zz[pos]));
                pos++;
            }

            dcRowCur[bxi + 1] = zz[0];
            dcPrevLeft        = zz[0];
        }
        std::swap(dcRowPrev, dcRowCur);
    }

    enc.flush();

    // ---- الترويسة NC02 + الحمولة ----
    std::vector<uint8_t> out;
    out.reserve(enc.data().size() + 13);
    out.push_back('N'); out.push_back('C');
    out.push_back('0'); out.push_back('2');
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

// ---------------- PGM I/O ----------------

Image readPGM(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("readPGM: cannot open " + path);

    std::string magic;
    f >> magic;
    if (magic != "P5")
        throw std::runtime_error("readPGM: only binary PGM (P5) supported");

    auto skipWS = [&]() {
        while (true) {
            int c = f.peek();
            if (c == '#') { std::string line; std::getline(f, line); }
            else if (std::isspace(c)) f.get();
            else break;
        }
    };

    int W = 0, H = 0, maxv = 0;
    skipWS(); f >> W;
    skipWS(); f >> H;
    skipWS(); f >> maxv;
    f.get(); // فاصل واحد

    Image img;
    img.width  = W;
    img.height = H;
    img.pixels.resize((size_t)W * H);
    f.read((char*)img.pixels.data(), (std::streamsize)img.pixels.size());
    if (!f) throw std::runtime_error("readPGM: truncated file");
    return img;
}

void writePGM(const std::string& path, const Image& img) {
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("writePGM: cannot write " + path);
    f << "P5\n" << img.width << " " << img.height << "\n255\n";
    f.write((const char*)img.pixels.data(), (std::streamsize)img.pixels.size());
}

} // namespace codec