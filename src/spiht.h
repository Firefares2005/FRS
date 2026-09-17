#pragma once
#include "entropy.h"
#include <cstdint>
#include <vector>
#include <cstddef>
#include <algorithm>
#include <cmath>

namespace codec {

struct SpihtModels {
    BitModel maxBitLen[24];
    BitModel maxBitVal[24];
    BitModel sigCtx[4];
    BitModel signCtx[4];
    BitModel setSigACtx[4];
    BitModel setSigBCtx[4];
    BitModel refCtx[4];
};

struct SpihtTree {
    int W, H, L;
    std::vector<int>     parent;
    std::vector<uint8_t> subband;
    std::vector<uint8_t> lev;       // ★ NEW: 0=LL, 1..L = detail level
    std::vector<float>   factor;    // ★ NEW: quantization factor per coef
    std::vector<std::vector<int>> children;
    std::vector<int>     llPixels;
};

// ---------------- بناء الشجرة + العوامل ----------------
inline SpihtTree buildSpihtTree(int W, int H, int L) {
    SpihtTree tree;
    tree.W = W; tree.H = H; tree.L = L;
    int N = W * H;
    tree.parent.assign(N, -1);
    tree.subband.assign(N, 0);
    tree.lev.assign(N, 0);
    tree.children.resize(N);

    std::vector<int> Wk(L+1), Hk(L+1);
    Wk[0] = W; Hk[0] = H;
    for (int k = 1; k <= L; k++) {
        Wk[k] = (Wk[k-1] + 1) / 2;
        Hk[k] = (Hk[k-1] + 1) / 2;
    }

    // LL
    for (int y = 0; y < Hk[L]; y++)
        for (int x = 0; x < Wk[L]; x++) {
            int i = y * W + x;
            tree.subband[i] = 0;
            tree.lev[i] = 0;
            tree.llPixels.push_back(i);
        }

    for (int k = 1; k <= L; k++) {
        // HL_k
        for (int y = 0; y < Hk[k]; y++) {
            for (int x = Wk[k]; x < Wk[k-1]; x++) {
                int i = y * W + x;
                tree.subband[i] = 1;
                tree.lev[i] = (uint8_t)k;
                int px, py;
                if (k == L) { px = x - Wk[L]; py = y; }
                else        { px = Wk[k+1] + (x - Wk[k]) / 2; py = y / 2; }
                int pidx = py * W + px;
                if (pidx >= 0 && pidx < N) {
                    tree.parent[i] = pidx;
                    tree.children[pidx].push_back(i);
                }
            }
        }
        // LH_k
        for (int y = Hk[k]; y < Hk[k-1]; y++) {
            for (int x = 0; x < Wk[k]; x++) {
                int i = y * W + x;
                tree.subband[i] = 2;
                tree.lev[i] = (uint8_t)k;
                int px, py;
                if (k == L) { px = x; py = y - Hk[L]; }
                else        { px = x / 2; py = Hk[k+1] + (y - Hk[k]) / 2; }
                int pidx = py * W + px;
                if (pidx >= 0 && pidx < N) {
                    tree.parent[i] = pidx;
                    tree.children[pidx].push_back(i);
                }
            }
        }
        // HH_k
        for (int y = Hk[k]; y < Hk[k-1]; y++) {
            for (int x = Wk[k]; x < Wk[k-1]; x++) {
                int i = y * W + x;
                tree.subband[i] = 3;
                tree.lev[i] = (uint8_t)k;
                int px, py;
                if (k == L) { px = x - Wk[L]; py = y - Hk[L]; }
                else        { px = Wk[k+1] + (x - Wk[k]) / 2;
                              py = Hk[k+1] + (y - Hk[k]) / 2; }
                int pidx = py * W + px;
                if (pidx >= 0 && pidx < N) {
                    tree.parent[i] = pidx;
                    tree.children[pidx].push_back(i);
                }
            }
        }
    }

    // ★ حساب عوامل التكميم التكيفية ★
    // factor > 1  → تكميم أقوى (جودة أقل، حجم أصغر)
    // factor = 1  → تكميم عادي
    // factor < 1  → تكميم ألطف
    tree.factor.assign(N, 1.0f);
    for (int i = 0; i < N; i++) {
        int sb = tree.subband[i];
        int lv = tree.lev[i];
        float f;
        if (lv == 0) {
            f = 1.0f;  // LL: أهم → تكميم عادي
        } else {
            // التفاصيل الدقيقة (lv=1) تُكمَّم أكثر
            // التفاصيل الخشنة (lv=L) تُكمَّم أقل
            f = 1.0f + 0.25f * (float)(L - lv);
            if (sb == 3) f += 0.4f;   // HH أضعف بصرياً → تكميم أكثر
        }
        tree.factor[i] = f;
    }

    return tree;
}

// ---------------- exp-golomb ----------------
inline void spihtEncodeUInt(RangeEncoder& enc, BitModel* len, BitModel* val, uint32_t v) {
    uint32_t x = v + 1;
    int k = 0;
    while (x > 1) { x >>= 1; k++; }
    for (int i = 0; i < k; i++) enc.encodeBit(len[i], 0);
    enc.encodeBit(len[k], 1);
    uint32_t rem = v + 1 - ((uint32_t)1 << k);
    for (int i = k - 1; i >= 0; i--)
        enc.encodeBit(val[i], (int)((rem >> i) & 1));
}

inline uint32_t spihtDecodeUInt(RangeDecoder& dec, BitModel* len, BitModel* val) {
    int k = 0;
    while (k < 23 && dec.decodeBit(len[k]) == 0) k++;
    uint32_t rem = 0;
    for (int i = k - 1; i >= 0; i--)
        rem = (rem << 1) | (uint32_t)dec.decodeBit(val[i]);
    return ((uint32_t)1 << k) + rem - 1;
}

inline void dfsMaxDesc(int i, const SpihtTree& tree,
                       const std::vector<int>& coefBit,
                       std::vector<int>& maxDescBit) {
    int mx = -1;
    for (int c : tree.children[i]) {
        dfsMaxDesc(c, tree, coefBit, maxDescBit);
        if (coefBit[c]    > mx) mx = coefBit[c];
        if (maxDescBit[c] > mx) mx = maxDescBit[c];
    }
    maxDescBit[i] = mx;
}

// ---------------- Encoder ----------------
inline void spihtEncode(RangeEncoder& enc, SpihtModels& m,
                        std::vector<int>& coefs, const SpihtTree& tree) {
    int N = (int)coefs.size();

    int maxAbs = 0;
    for (int i = 0; i < N; i++) {
        int a = coefs[i] < 0 ? -coefs[i] : coefs[i];
        if (a > maxAbs) maxAbs = a;
    }
    int maxBit = 0;
    while ((1 << maxBit) <= maxAbs) maxBit++;
    if (maxBit > 0) maxBit--;
    spihtEncodeUInt(enc, m.maxBitLen, m.maxBitVal, (uint32_t)maxBit);

    std::vector<int> coefBit(N, -1);
    for (int i = 0; i < N; i++) {
        int a = coefs[i] < 0 ? -coefs[i] : coefs[i];
        if (a > 0) {
            int b = 0;
            while ((1 << b) <= a) b++;
            coefBit[i] = b - 1;
        }
    }
    std::vector<int> maxDescBit(N, -1);
    for (int r : tree.llPixels) dfsMaxDesc(r, tree, coefBit, maxDescBit);

    std::vector<int> LIP = tree.llPixels;
    std::vector<int> LSP;
    struct LisEntry { int idx; int type; };
    std::vector<LisEntry> LIS;
    for (int i : tree.llPixels) LIS.push_back({i, 0});

    std::vector<uint8_t> inLSP(N, 0);
    int lspRefStart = 0;

    for (int bit = maxBit; bit >= 0; bit--) {
        int mask = 1 << bit;

        int lipSnapshot = (int)LIP.size();
        for (int k = 0; k < lipSnapshot; k++) {
            int i = LIP[k];
            if (inLSP[i]) continue;
            int a = coefs[i] < 0 ? -coefs[i] : coefs[i];
            int sb = tree.subband[i];
            if (a >= mask) {
                enc.encodeBit(m.sigCtx[sb], 1);
                enc.encodeBit(m.signCtx[sb], coefs[i] < 0 ? 1 : 0);
                LSP.push_back(i);
                inLSP[i] = 1;
            } else {
                enc.encodeBit(m.sigCtx[sb], 0);
            }
        }
        {
            std::vector<int> newLIP;
            newLIP.reserve(LIP.size());
            for (int i : LIP) if (!inLSP[i]) newLIP.push_back(i);
            LIP.swap(newLIP);
        }

        int lisIdx = 0;
        while (lisIdx < (int)LIS.size()) {
            LisEntry e = LIS[lisIdx];
            int i = e.idx;
            int sb = tree.subband[i];
            if (e.type == 0) {
                bool sig = (maxDescBit[i] >= bit);
                enc.encodeBit(m.setSigACtx[sb], sig ? 1 : 0);
                if (sig) {
                    for (int c : tree.children[i]) {
                        int ca = coefs[c] < 0 ? -coefs[c] : coefs[c];
                        int csb = tree.subband[c];
                        if (ca >= mask) {
                            enc.encodeBit(m.sigCtx[csb], 1);
                            enc.encodeBit(m.signCtx[csb], coefs[c] < 0 ? 1 : 0);
                            LSP.push_back(c);
                            inLSP[c] = 1;
                        } else {
                            enc.encodeBit(m.sigCtx[csb], 0);
                            LIP.push_back(c);
                        }
                    }
                    bool hasGrand = false;
                    for (int c : tree.children[i])
                        if (!tree.children[c].empty()) { hasGrand = true; break; }
                    if (hasGrand) LIS.push_back({i, 1});
                    LIS.erase(LIS.begin() + lisIdx);
                    continue;
                }
                lisIdx++;
            } else {
                bool sig = false;
                {
                    int mx = -1;
                    for (int c : tree.children[i])
                        for (int g : tree.children[c]) {
                            int ga = coefs[g] < 0 ? -coefs[g] : coefs[g];
                            if (ga > mx) mx = ga;
                        }
                    sig = (mx >= mask);
                }
                enc.encodeBit(m.setSigBCtx[sb], sig ? 1 : 0);
                if (sig) {
                    for (int c : tree.children[i]) LIS.push_back({c, 0});
                    LIS.erase(LIS.begin() + lisIdx);
                    continue;
                }
                lisIdx++;
            }
        }

        for (int k = 0; k < lspRefStart; k++) {
            int i = LSP[k];
            int a = coefs[i] < 0 ? -coefs[i] : coefs[i];
            int b = (a >> bit) & 1;
            enc.encodeBit(m.refCtx[tree.subband[i]], b);
        }
        lspRefStart = (int)LSP.size();
    }
}

// ---------------- Decoder ----------------
inline std::vector<int> spihtDecode(RangeDecoder& dec, SpihtModels& m,
                                    const SpihtTree& tree) {
    int N = (int)tree.parent.size();
    std::vector<int> coefs(N, 0);

    int maxBit = (int)spihtDecodeUInt(dec, m.maxBitLen, m.maxBitVal);

    std::vector<int> LIP = tree.llPixels;
    std::vector<int> LSP;
    struct LisEntry { int idx; int type; };
    std::vector<LisEntry> LIS;
    for (int i : tree.llPixels) LIS.push_back({i, 0});

    std::vector<uint8_t> inLSP(N, 0);
    int lspRefStart = 0;

    for (int bit = maxBit; bit >= 0; bit--) {
        int mask = 1 << bit;

        int lipSnapshot = (int)LIP.size();
        for (int k = 0; k < lipSnapshot; k++) {
            int i = LIP[k];
            if (inLSP[i]) continue;
            int sb = tree.subband[i];
            int sig = dec.decodeBit(m.sigCtx[sb]);
            if (sig) {
                int sign = dec.decodeBit(m.signCtx[sb]);
                coefs[i] = mask;
                if (sign) coefs[i] = -coefs[i];
                LSP.push_back(i);
                inLSP[i] = 1;
            }
        }
        {
            std::vector<int> newLIP;
            newLIP.reserve(LIP.size());
            for (int i : LIP) if (!inLSP[i]) newLIP.push_back(i);
            LIP.swap(newLIP);
        }

        int lisIdx = 0;
        while (lisIdx < (int)LIS.size()) {
            LisEntry e = LIS[lisIdx];
            int i = e.idx;
            int sb = tree.subband[i];
            if (e.type == 0) {
                int sig = dec.decodeBit(m.setSigACtx[sb]);
                if (sig) {
                    for (int c : tree.children[i]) {
                        int csb = tree.subband[c];
                        int csig = dec.decodeBit(m.sigCtx[csb]);
                        if (csig) {
                            int sign = dec.decodeBit(m.signCtx[csb]);
                            coefs[c] = mask;
                            if (sign) coefs[c] = -coefs[c];
                            LSP.push_back(c);
                            inLSP[c] = 1;
                        } else {
                            LIP.push_back(c);
                        }
                    }
                    bool hasGrand = false;
                    for (int c : tree.children[i])
                        if (!tree.children[c].empty()) { hasGrand = true; break; }
                    if (hasGrand) LIS.push_back({i, 1});
                    LIS.erase(LIS.begin() + lisIdx);
                    continue;
                }
                lisIdx++;
            } else {
                int sig = dec.decodeBit(m.setSigBCtx[sb]);
                if (sig) {
                    for (int c : tree.children[i]) LIS.push_back({c, 0});
                    LIS.erase(LIS.begin() + lisIdx);
                    continue;
                }
                lisIdx++;
            }
        }

        for (int k = 0; k < lspRefStart; k++) {
            int i = LSP[k];
            int b = dec.decodeBit(m.refCtx[tree.subband[i]]);
            if (b) {
                int a = coefs[i] < 0 ? -coefs[i] : coefs[i];
                a |= mask;
                coefs[i] = (coefs[i] < 0) ? -a : a;
            }
        }
        lspRefStart = (int)LSP.size();
    }

    return coefs;
}

} // namespace codec