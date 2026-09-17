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

    int prevDC = 0;
    Block block, coefs;

    for (int byi = 0; byi < by; byi++) {
        for (int bxi = 0; bxi < bx; bxi++) {
            // اقرأ الكتلة مع حشو من الحافة
            for (int y = 0; y < N; y++) {
                int sy = byi * N + y;
                if (sy >= H) sy = H - 1;
                for (int x = 0; x < N; x++) {
                    int sx = bxi * N + x;
                    if (sx >= W) sx = W - 1;
                    block[y][x] = (float)img.pixels[(size_t)sy * W + sx] - 128.0f;
                }
            }

            dct2d(block, coefs);

            // تكميم + ترتيب zigzag
            int zz[64];
            for (int y = 0; y < N; y++)
                for (int x = 0; x < N; x++) {
                    float q = coefs[y][x] / (float)Q[y][x];
                    zz[ZIGZAG[y * N + x]] = (int)std::lround(q);
                }

            // آخر معامل غير صفري
            int lastNZ = 0;
            for (int i = 63; i >= 1; i--)
                if (zz[i] != 0) { lastNZ = i; break; }

            // شفّر lastNZ بـ 6 بتات
            for (int i = 5; i >= 0; i--)
                enc.encodeBit(models.eobBits[i], (lastNZ >> i) & 1);

            // شفّر DC (فرق عن الكتلة السابقة)
            int dcdiff = zz[0] - prevDC;
            prevDC = zz[0];
            encodeUInt(enc, models.dcLen, models.dcVal,
                       zigzagSigned(dcdiff));

            // شفّر AC
            for (int i = 1; i <= lastNZ; i++) {
                int c = acClass(i);
                encodeUInt(enc, models.acLen[c], models.acVal[c],
                           zigzagSigned(zz[i]));
            }
        }
    }

    enc.flush();

    // -------- الترويسة + الحمولة --------
    std::vector<uint8_t> out;
    out.reserve(enc.data().size() + 13);
    out.push_back('N'); out.push_back('C');
    out.push_back('0'); out.push_back('1');
    auto push32 = [&](uint32_t v) {
        out.push_back((uint8_t)(v & 0xFF));
        out.push_back((uint8_t)((v >> 8) & 0xFF));
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
    img.width = W;
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