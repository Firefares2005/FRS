#pragma once
#include "entropy.h"
#include <cstdint>

namespace codec {

constexpr int NUM_CTX = 4;

// ---------- نماذج 8x8 ----------
struct CoefModels {
    BitModel skipFlag;
    BitModel dcLen[NUM_CTX][24];
    BitModel dcVal[NUM_CTX][24];
    BitModel acLen[3][NUM_CTX][24];
    BitModel acVal[3][NUM_CTX][24];
    BitModel zeroRun[24];
    BitModel eobBits[6];
};

// ---------- نماذج 16x16 ----------
struct CoefModels16 {
    BitModel blockType;      // 0=16x16, 1=8x8
    BitModel skipFlag;
    BitModel dcLen[NUM_CTX][24];
    BitModel dcVal[NUM_CTX][24];
    BitModel acLen[3][NUM_CTX][24];
    BitModel acVal[3][NUM_CTX][24];
    BitModel zeroRun[24];
    BitModel eobBits[8];     // 8 بتات (0..255)
};

// ---------- أدوات مشتركة ----------
inline int acClass(int pos) {
    if (pos < 8)  return 0;
    if (pos < 32) return 1;
    return 2;
}

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

inline int classifyContext(int dcLeft, int dcUp) {
    int d = dcLeft - dcUp;
    int md = (d < 0) ? -d : d;
    if (md < 4)  return 3;
    if (md < 16) return 2;
    if (md < 48) return 1;
    return 0;
}

inline void encodeZeroRun(RangeEncoder& enc, BitModel* run, int r) {
    encodeUInt(enc, run, run, (uint32_t)r);
}
inline int decodeZeroRun(RangeDecoder& dec, BitModel* run) {
    return (int)decodeUInt(dec, run, run);
}

} // namespace codec