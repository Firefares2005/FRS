#pragma once
#include "entropy.h"
#include <cstdint>
#include <vector>
#include <cstddef>
#include <algorithm>
#include <cmath>

namespace codec {

// ---------- نماذج موسّعة (EBCOT-like) ----------
struct SpihtModels {
    BitModel maxBitLen[24];
    BitModel maxBitVal[24];

    // Significance: [nSig 0..4][subband 0..3][parentSig 0..1]
    BitModel sigCtx[5][4][2];

    // Sign: [subband][h-sign][v-sign]  (h/v: 0=negative, 1=positive, 2=unknown)
    BitModel signCtx[4][3][3];

    // Set significance
    BitModel setSigACtx[4];
    BitModel setSigBCtx[4];

    // Refinement: [subband][bitPosition 0..3]
    BitModel refCtx[4][4];
};

// ---------- الشجرة + عوامل التكميم ----------
struct SpihtTree {
    int W, H, L;
    std::vector<int>     parent;
    std::vector<uint8_t> subband;
    std::vector<uint8_t> lev;
    std::vector<float>   factor;
    std::vector<std::vector<int>> children;
    std::vector<int>     llPixels;
};

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

    for (int y = 0; y < Hk[L]; y++)
        for (int x = 0; x < Wk[L]; x++) {
            int i = y * W + x;
            tree.subband[i] = 0;
            tree.lev[i] = 0;
            tree.llPixels.push_back(i);
        }

    for (int k = 1; k <= L; k++) {
        // HL
        for (int y = 0; y < Hk[k]; y++)
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
        // LH
        for (int y = Hk[k]; y < Hk[k-1]; y++)
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
        // HH
        for (int y = Hk[k]; y < Hk[k-1]; y++)
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

        // ★ NC08.3: توزيع ذكي — LL أكثر حماية، HH أقل
    tree.factor.assign(N, 1.0f);
    for (int i = 0; i < N; i++) {
        int sb = tree.subband[i];
        int lv = tree.lev[i];
        float f;
        if (lv == 0) {
            f = 0.72f;                       // LL: تكميم ألطف (كان 0.80)
        } else {
            f = 1.0f + 0.22f * (float)(L - lv);   // منحدر أقل
            if (sb == 3) f += 0.42f;              // HH: تكميم أقوى (كان 0.30)
        }
        tree.factor[i] = f;
    }
    return tree;
}

// ---------- exp-golomb ----------
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

// ---------- Sign context helper (EBCOT-like) ----------
// 0 = negative, 1 = positive, 2 = unknown/not-significant
inline int signClass(int s) {
    if (s < 0) return 0;
    if (s > 0) return 1;
    return 2;
}

// ---------- Encoder ----------
inline void spihtEncode(RangeEncoder& enc, SpihtModels& m,
                        std::vector<int>& coefs, const SpihtTree& tree) {
    int N = (int)coefs.size();
    int W = tree.W;

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

        // ---- LIP ----
        int lipSnapshot = (int)LIP.size();
        for (int k = 0; k < lipSnapshot; k++) {
            int i = LIP[k];
            if (inLSP[i]) continue;
            int a = coefs[i] < 0 ? -coefs[i] : coefs[i];
            int sb = tree.subband[i];
            if (a >= mask) {
                // Context: عدد الجيران المُهمَّة (4-connectivity)
                int y = i / W, x = i % W;
                int nSig = 0;
                if (x > 0     && inLSP[i-1]) nSig++;
                if (x < W - 1 && inLSP[i+1]) nSig++;
                if (y > 0     && inLSP[i-W]) nSig++;
                if (y < tree.H - 1 && inLSP[i+W]) nSig++;
                int pidx = tree.parent[i];
                int pSig = (pidx >= 0 && inLSP[pidx]) ? 1 : 0;
                if (nSig > 4) nSig = 4;

                enc.encodeBit(m.sigCtx[nSig][sb][pSig], 1);

                // Sign context: من إشارات الجيران المُهمَّة
                int hSign = 2, vSign = 2;
                if (x > 0     && inLSP[i-1]) hSign = signClass(coefs[i-1]);
                if (x < W - 1 && inLSP[i+1] && hSign == 2) hSign = signClass(coefs[i+1]);
                if (y > 0     && inLSP[i-W]) vSign = signClass(coefs[i-W]);
                if (y < tree.H - 1 && inLSP[i+W] && vSign == 2)
                    vSign = signClass(coefs[i+W]);
                enc.encodeBit(m.signCtx[sb][hSign][vSign], coefs[i] < 0 ? 1 : 0);

                LSP.push_back(i);
                inLSP[i] = 1;
            } else {
                int y = i / W, x = i % W;
                int nSig = 0;
                if (x > 0     && inLSP[i-1]) nSig++;
                if (x < W - 1 && inLSP[i+1]) nSig++;
                if (y > 0     && inLSP[i-W]) nSig++;
                if (y < tree.H - 1 && inLSP[i+W]) nSig++;
                int pidx = tree.parent[i];
                int pSig = (pidx >= 0 && inLSP[pidx]) ? 1 : 0;
                if (nSig > 4) nSig = 4;
                enc.encodeBit(m.sigCtx[nSig][sb][pSig], 0);
            }
        }
        {
            std::vector<int> newLIP;
            newLIP.reserve(LIP.size());
            for (int i : LIP) if (!inLSP[i]) newLIP.push_back(i);
            LIP.swap(newLIP);
        }

        // ---- LIS ----
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
                            int cy = c / W, cx = c % W;
                            int cns = 0;
                            if (cx > 0 && inLSP[c-1]) cns++;
                            if (cx < W-1 && inLSP[c+1]) cns++;
                            if (cy > 0 && inLSP[c-W]) cns++;
                            if (cy < tree.H-1 && inLSP[c+W]) cns++;
                            if (cns > 4) cns = 4;
                            int cp = tree.parent[c];
                            int cpSig = (cp >= 0 && inLSP[cp]) ? 1 : 0;
                            enc.encodeBit(m.sigCtx[cns][csb][cpSig], 1);

                            int chS = 2, cvS = 2;
                            if (cx > 0 && inLSP[c-1]) chS = signClass(coefs[c-1]);
                            if (cx < W-1 && inLSP[c+1] && chS == 2)
                                chS = signClass(coefs[c+1]);
                            if (cy > 0 && inLSP[c-W]) cvS = signClass(coefs[c-W]);
                            if (cy < tree.H-1 && inLSP[c+W] && cvS == 2)
                                cvS = signClass(coefs[c+W]);
                            enc.encodeBit(m.signCtx[csb][chS][cvS],
                                          coefs[c] < 0 ? 1 : 0);

                            LSP.push_back(c);
                            inLSP[c] = 1;
                        } else {
                            int cy = c / W, cx = c % W;
                            int cns = 0;
                            if (cx > 0 && inLSP[c-1]) cns++;
                            if (cx < W-1 && inLSP[c+1]) cns++;
                            if (cy > 0 && inLSP[c-W]) cns++;
                            if (cy < tree.H-1 && inLSP[c+W]) cns++;
                            if (cns > 4) cns = 4;
                            int cp = tree.parent[c];
                            int cpSig = (cp >= 0 && inLSP[cp]) ? 1 : 0;
                            enc.encodeBit(m.sigCtx[cns][csb][cpSig], 0);
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

        // ---- Refinement ----
        for (int k = 0; k < lspRefStart; k++) {
            int i = LSP[k];
            int a = coefs[i] < 0 ? -coefs[i] : coefs[i];
            int b = (a >> bit) & 1;
            int rctx = bit & 3;
            enc.encodeBit(m.refCtx[tree.subband[i]][rctx], b);
        }
        lspRefStart = (int)LSP.size();
    }
}

// ---------- Decoder ----------
inline std::vector<int> spihtDecode(RangeDecoder& dec, SpihtModels& m,
                                    const SpihtTree& tree) {
    int N = (int)tree.parent.size();
    int W = tree.W;
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

        // ---- LIP ----
        int lipSnapshot = (int)LIP.size();
        for (int k = 0; k < lipSnapshot; k++) {
            int i = LIP[k];
            if (inLSP[i]) continue;
            int sb = tree.subband[i];
            int y = i / W, x = i % W;
            int nSig = 0;
            if (x > 0     && inLSP[i-1]) nSig++;
            if (x < W - 1 && inLSP[i+1]) nSig++;
            if (y > 0     && inLSP[i-W]) nSig++;
            if (y < tree.H - 1 && inLSP[i+W]) nSig++;
            if (nSig > 4) nSig = 4;
            int pidx = tree.parent[i];
            int pSig = (pidx >= 0 && inLSP[pidx]) ? 1 : 0;

            int sig = dec.decodeBit(m.sigCtx[nSig][sb][pSig]);
            if (sig) {
                int hSign = 2, vSign = 2;
                if (x > 0     && inLSP[i-1]) hSign = signClass(coefs[i-1]);
                if (x < W - 1 && inLSP[i+1] && hSign == 2) hSign = signClass(coefs[i+1]);
                if (y > 0     && inLSP[i-W]) vSign = signClass(coefs[i-W]);
                if (y < tree.H - 1 && inLSP[i+W] && vSign == 2)
                    vSign = signClass(coefs[i+W]);

                int sign = dec.decodeBit(m.signCtx[sb][hSign][vSign]);
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

        // ---- LIS ----
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
                        int cy = c / W, cx = c % W;
                        int cns = 0;
                        if (cx > 0 && inLSP[c-1]) cns++;
                        if (cx < W-1 && inLSP[c+1]) cns++;
                        if (cy > 0 && inLSP[c-W]) cns++;
                        if (cy < tree.H-1 && inLSP[c+W]) cns++;
                        if (cns > 4) cns = 4;
                        int cp = tree.parent[c];
                        int cpSig = (cp >= 0 && inLSP[cp]) ? 1 : 0;

                        int csig = dec.decodeBit(m.sigCtx[cns][csb][cpSig]);
                        if (csig) {
                            int chS = 2, cvS = 2;
                            if (cx > 0 && inLSP[c-1]) chS = signClass(coefs[c-1]);
                            if (cx < W-1 && inLSP[c+1] && chS == 2)
                                chS = signClass(coefs[c+1]);
                            if (cy > 0 && inLSP[c-W]) cvS = signClass(coefs[c-W]);
                            if (cy < tree.H-1 && inLSP[c+W] && cvS == 2)
                                cvS = signClass(coefs[c+W]);

                            int sign = dec.decodeBit(m.signCtx[csb][chS][cvS]);
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

        // ---- Refinement ----
        for (int k = 0; k < lspRefStart; k++) {
            int i = LSP[k];
            int rctx = bit & 3;
            int b = dec.decodeBit(m.refCtx[tree.subband[i]][rctx]);
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