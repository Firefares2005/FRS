#pragma once
#include "entropy.h"
#include <cstdint>

namespace codec {

// ---------- نماذج NC05 ----------
// Contexts:
//   nzCtx[5][4]     : IS_NZ — [neighbor count 0..4][subband 0..3]
//   sigCtx[4][4][2] : significance — [nsig 0..3][subband 0..3][parent_sig]
//   signCtx[4]      : sign — [subband]
//   refCtx[4]       : refinement — [subband]
struct WaveletModels {
    BitModel nzCtx[5][4];
    BitModel maxBitLen[24];
    BitModel maxBitVal[24];
    BitModel sigCtx[4][4][2];
    BitModel signCtx[4];
    BitModel refCtx[4];
};

inline uint32_t zigzagSigned(int v) {
    return (v >= 0) ? (uint32_t)(2 * v) : (uint32_t)(-2 * v - 1);
}
inline int unzigzagSigned(uint32_t u) {
    return (u & 1) ? -(int)((u + 1) >> 1) : (int)(u >> 1);
}

inline void encodeUInt(RangeEncoder& enc, BitModel* len, BitModel* val, uint32_t v) {
    uint32_t x = v + 1;
    int k = 0;
    while (x > 1) { x >>= 1; k++; }
    for (int i = 0; i < k; i++) enc.encodeBit(len[i], 0);
    enc.encodeBit(len[k], 1);
    uint32_t rem = v + 1 - ((uint32_t)1 << k);
    for (int i = k - 1; i >= 0; i--)
        enc.encodeBit(val[i], (int)((rem >> i) & 1));
}

inline uint32_t decodeUInt(RangeDecoder& dec, BitModel* len, BitModel* val) {
    int k = 0;
    while (k < 23 && dec.decodeBit(len[k]) == 0) k++;
    uint32_t rem = 0;
    for (int i = k - 1; i >= 0; i--)
        rem = (rem << 1) | (uint32_t)dec.decodeBit(val[i]);
    return ((uint32_t)1 << k) + rem - 1;
}

// ---------- بنية معلومات المعاملات ----------
struct CoefInfo {
    std::vector<uint8_t> subband;   // 0=LL, 1=HL, 2=LH, 3=HH
    std::vector<uint8_t> lev;       // 0=LL, 1..L=detail
    std::vector<int>     parent;    // -1 if none
    std::vector<int>     order;     // scan order (coarse → fine)
};

CoefInfo prepareCoefInfo(int W, int H, int L);

} // namespace codec