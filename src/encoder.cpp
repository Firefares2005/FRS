#include "encoder.h"
#include "wavelet.h"
#include "models.h"

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

// -------------------- ترميز plane واحد (NC05) --------------------
static void encodePlane(RangeEncoder& enc, WaveletModels& m,
                        std::vector<int>& qc, int w, int h, int L) {
    int N = w * h;

    CoefInfo info = prepareCoefInfo(w, h, L);

    // Pass 1: IS_NZ map
    std::vector<uint8_t> nz(N, 0);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int i = y * w + x;
            int isNZ = (qc[i] != 0) ? 1 : 0;
            int nNZ = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = x + dx, ny = y + dy;
                    if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                    if (nz[ny * w + nx]) nNZ++;
                }
            if (nNZ > 4) nNZ = 4;
            enc.encodeBit(m.nzCtx[nNZ][info.subband[i]], isNZ);
            nz[i] = isNZ;
        }
    }

    // maxBit
    int maxAbs = 0;
    for (int i = 0; i < N; i++) {
        int a = qc[i] < 0 ? -qc[i] : qc[i];
        if (a > maxAbs) maxAbs = a;
    }
    int maxBit = 0;
    while ((1 << maxBit) <= maxAbs) maxBit++;
    if (maxBit > 0) maxBit--;
    encodeUInt(enc, m.maxBitLen, m.maxBitVal, (uint32_t)maxBit);

    // Pass 2: bit planes (level-order scan)
    std::vector<uint8_t> sig(N, 0);
    for (int bit = maxBit; bit >= 0; bit--) {
        int mask = 1 << bit;
        for (int idx = 0; idx < N; idx++) {
            int i = info.order[idx];
            if (!nz[i]) continue;

            int y = i / w, x = i % w;
            int c = qc[i];
            int mag = c < 0 ? -c : c;
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

                int isSig = (mag >= mask) ? 1 : 0;
                enc.encodeBit(m.sigCtx[nsig][sb][parentSig], isSig);
                if (isSig) {
                    int s = (c < 0) ? 1 : 0;
                    enc.encodeBit(m.signCtx[sb], s);
                    sig[i] = 1;
                }
            } else {
                int b = (mag >> bit) & 1;
                enc.encodeBit(m.refCtx[sb], b);
            }
        }
    }
}

// -------------------- التشفير الكامل --------------------
std::vector<uint8_t> encodeImage(const Image& img, int quality) {
    const int W = img.width, H = img.height, C = img.channels;
    if (W <= 0 || H <= 0) throw std::runtime_error("encodeImage: bad size");
    if (C != 1 && C != 3) throw std::runtime_error("encodeImage: bad channels");

    const int levels = computeLevels(W, H);

    // معادلة التكميم
    double qn = (100 - std::min(100, std::max(0, quality))) / 100.0;
    float step = 1.0f + (float)(qn * 80.0f);

    // -------- تجهيز الـ planes --------
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
        int sw = (W + 1) / 2, sh = (H + 1) / 2;
        std::vector<float> Cbs(sw * sh), Crs(sw * sh);
        for (int y = 0; y < sh; y++)
            for (int x = 0; x < sw; x++) {
                float sumCb = 0, sumCr = 0;
                int cnt = 0;
                for (int dy = 0; dy < 2; dy++)
                    for (int dx = 0; dx < 2; dx++) {
                        int yy = y*2 + dy, xx = x*2 + dx;
                        if (yy >= H || xx >= W) continue;
                        sumCb += Cb[yy*W + xx];
                        sumCr += Cr[yy*W + xx];
                        cnt++;
                    }
                Cbs[y*sw + x] = sumCb / cnt;
                Crs[y*sw + x] = sumCr / cnt;
            }
        planes.push_back(std::move(Y));    planeSizes.push_back({W, H});
        planes.push_back(std::move(Cbs));  planeSizes.push_back({sw, sh});
        planes.push_back(std::move(Crs));  planeSizes.push_back({sw, sh});
    }

    // -------- التشفير --------
    RangeEncoder enc;
    WaveletModels wm;

    enc.encodeBit(wm.nzCtx[0][0], (C == 3) ? 1 : 0);

    for (size_t p = 0; p < planes.size(); p++) {
        int pw = planeSizes[p].first;
        int ph = planeSizes[p].second;

        dwt2d(planes[p], pw, ph, levels);

        std::vector<int> qc(pw * ph);
        for (int i = 0; i < pw * ph; i++)
            qc[i] = (int)std::lround(planes[p][i] / step);

        encodePlane(enc, wm, qc, pw, ph, levels);
    }

    enc.flush();

    // -------- الترويسة NC05 --------
    std::vector<uint8_t> out;
    out.reserve(enc.data().size() + 14);
    out.push_back('N'); out.push_back('C');
    out.push_back('0'); out.push_back('5');
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

// -------------------- PGM/PPM I/O --------------------
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