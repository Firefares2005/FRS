#include "encoder.h"
#include "wavelet.h"
#include "spiht.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <cctype>
#include <algorithm>

namespace codec {

static int computeLevels(int W, int H) {
    int s = std::min(W, H);
    int lv = 0;
    while (s > 8 && lv < 6) { s /= 2; lv++; }
    return lv < 1 ? 1 : lv;
}

// ★ حساب "تفصيل الصورة" — متوسط التدرج الأفقي/الرأسي
static float detailScore(const Image& img) {
    if (img.channels != 3) return 0.0f;
    const int W = img.width, H = img.height;
    double sum = 0;
    long cnt = 0;
    for (int y = 1; y < H; y++) {
        for (int x = 1; x < W; x++) {
            int idx  = (y * W + x) * 3;
            int idxL = (y * W + x - 1) * 3;
            int idxU = ((y - 1) * W + x) * 3;
            // استخدام قناة Y تقديرياً (R+G+B)/3
            float Y  = (img.pixels[idx]   + img.pixels[idx+1]   + img.pixels[idx+2])   / 3.0f;
            float YL = (img.pixels[idxL]  + img.pixels[idxL+1]  + img.pixels[idxL+2])  / 3.0f;
            float YU = (img.pixels[idxU]  + img.pixels[idxU+1]  + img.pixels[idxU+2])  / 3.0f;
            sum += std::abs(Y - YL) + std::abs(Y - YU);
            cnt += 2;
        }
    }
    return cnt > 0 ? (float)(sum / cnt) : 0.0f;
}

std::vector<uint8_t> encodeImage(const Image& img, int quality) {
    const int W = img.width, H = img.height, C = img.channels;
    if (W <= 0 || H <= 0) throw std::runtime_error("encodeImage: bad size");
    if (C != 1 && C != 3) throw std::runtime_error("encodeImage: bad channels");

    double qn = (100 - std::min(100, std::max(0, quality))) / 100.0;
    float step = 1.0f + (float)(qn * 70.0f);

    // ★ قرار 4:4:4 مقابل 4:2:0
    bool use444 = false;
    if (C == 3) {
        float d = detailScore(img);
        use444 = (d > 12.0f);   // عتبة مضبوطة تجريبياً
    }

    std::vector<std::vector<float>> planes;
    std::vector<std::pair<int,int>> planeSizes;

    if (C == 1) {
        std::vector<float> Y(W * H);
        for (int i = 0; i < W * H; i++)
            Y[i] = (float)img.pixels[i] - 128.0f;
        planes.push_back(std::move(Y));
        planeSizes.push_back({W, H});
    } else {
        std::vector<float> Y(W * H), Cb(W * H), Cr(W * H);
        for (int i = 0; i < W * H; i++) {
            float R = img.pixels[i*3 + 0];
            float G = img.pixels[i*3 + 1];
            float B = img.pixels[i*3 + 2];
            Y[i]  =  0.299f*R + 0.587f*G + 0.114f*B - 128.0f;
            Cb[i] = -0.1687f*R - 0.3313f*G + 0.5f*B;
            Cr[i] =  0.5f*R - 0.4187f*G - 0.0813f*B;
        }

        if (use444) {
            // 4:4:4 — chroma بنفس الدقة
            planes.push_back(std::move(Y));    planeSizes.push_back({W, H});
            planes.push_back(std::move(Cb));   planeSizes.push_back({W, H});
            planes.push_back(std::move(Cr));   planeSizes.push_back({W, H});
        } else {
            // 4:2:0 — chroma بنصف الدقة
            int sw = (W + 1) / 2, sh = (H + 1) / 2;
            std::vector<float> Cbs(sw * sh), Crs(sw * sh);
            for (int y = 0; y < sh; y++)
                for (int x = 0; x < sw; x++) {
                    float sCb = 0, sCr = 0;
                    int cnt = 0;
                    for (int dy = 0; dy < 2; dy++)
                        for (int dx = 0; dx < 2; dx++) {
                            int yy = y*2 + dy, xx = x*2 + dx;
                            if (yy >= H || xx >= W) continue;
                            sCb += Cb[yy*W + xx];
                            sCr += Cr[yy*W + xx];
                            cnt++;
                        }
                    Cbs[y*sw + x] = sCb / cnt;
                    Crs[y*sw + x] = sCr / cnt;
                }
            planes.push_back(std::move(Y));    planeSizes.push_back({W, H});
            planes.push_back(std::move(Cbs));  planeSizes.push_back({sw, sh});
            planes.push_back(std::move(Crs));  planeSizes.push_back({sw, sh});
        }
    }

    RangeEncoder enc;
    SpihtModels  wm;

    // راية اللون + راية 4:4:4
    enc.encodeBit(wm.sigCtx[0][0][0], (C == 3) ? 1 : 0);
    if (C == 3)
        enc.encodeBit(wm.sigCtx[1][0][0], use444 ? 1 : 0);

    for (size_t p = 0; p < planes.size(); p++) {
        int pw = planeSizes[p].first;
        int ph = planeSizes[p].second;
        int pl = computeLevels(pw, ph);

        dwt2d(planes[p], pw, ph, pl);

        SpihtTree tree = buildSpihtTree(pw, ph, pl);

        std::vector<int> qc(pw * ph);
        for (int i = 0; i < pw * ph; i++)
            qc[i] = (int)std::lround(planes[p][i] / (step * tree.factor[i]));

        spihtEncode(enc, wm, qc, tree);
    }

    enc.flush();

    std::vector<uint8_t> out;
    out.reserve(enc.data().size() + 14);
    out.push_back('N'); out.push_back('C');
    out.push_back('0'); out.push_back('8');
    auto push32 = [&](uint32_t v) {
        out.push_back((uint8_t)( v        & 0xFF));
        out.push_back((uint8_t)((v >>  8) & 0xFF));
        out.push_back((uint8_t)((v >> 16) & 0xFF));
        out.push_back((uint8_t)((v >> 24) & 0xFF));
    };
    push32((uint32_t)W);
    push32((uint32_t)H);
    out.push_back((uint8_t)quality);
    out.push_back((uint8_t)C);

    const auto& body = enc.data();
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

static void skipWS(std::ifstream& f) {
    while (true) {
        int c = f.peek();
        if (c == '#') { std::string line; std::getline(f, line); }
        else if (std::isspace(c)) f.get();
        else break;
    }
}

Image readImage(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("readImage: cannot open " + path);

    std::string magic;
    f >> magic;
    if (magic != "P5" && magic != "P6")
        throw std::runtime_error("readImage: only P5/P6 supported");

    int W = 0, H = 0, maxv = 0;
    skipWS(f); f >> W;
    skipWS(f); f >> H;
    skipWS(f); f >> maxv;
    f.get();

    Image img;
    img.width  = W;
    img.height = H;
    img.channels = (magic == "P6") ? 3 : 1;
    img.pixels.resize((size_t)W * H * img.channels);
    f.read((char*)img.pixels.data(), (std::streamsize)img.pixels.size());
    if (!f) throw std::runtime_error("readImage: truncated");
    return img;
}

void writeImage(const std::string& path, const Image& img, bool color) {
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("writeImage: cannot write " + path);
    if (color && img.channels == 3)
        f << "P6\n" << img.width << " " << img.height << "\n255\n";
    else
        f << "P5\n" << img.width << " " << img.height << "\n255\n";
    f.write((const char*)img.pixels.data(), (std::streamsize)img.pixels.size());
}

} // namespace codec