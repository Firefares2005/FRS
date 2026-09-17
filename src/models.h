#pragma once
#include "entropy.h"
#include <cstdint>

namespace codec {

// نماذج ثنائية لتشفير المعاملات
struct CoefModels {
    BitModel eobBits[6];          // طول آخر معامل غير صفري (6 بتات)
    BitModel dcLen[24];
    BitModel dcVal[24];
    BitModel acLen[3][24];        // 3 أصناف ترددية للـ AC
    BitModel acVal[3][24];
};

inline int acClass(int pos) {
    if (pos < 8)  return 0;   // منخفض
    if (pos < 32) return 1;   // متوسط
    return 2;                 // عالٍ
}

inline uint32_t zigzagSigned(int v) {
    return (v >= 0) ? (uint32_t)(2 * v) : (uint32_t)(-2 * v - 1);
}
inline int unzigzagSigned(uint32_t u) {
    return (u & 1) ? -(int)((u + 1) >> 1) : (int)(u >> 1);
}

// ترميز عدد صحيح غير سالب بـ exp-golomb تكيّفي
inline void encodeUInt(RangeEncoder& enc, BitModel* len, BitModel* val, uint32_t v) {
    uint32_t x = v + 1;
    int k = 0;
    while (x > 1) { x >>= 1; k++; }
    for (int i = 0; i < k; i++) enc.encodeBit(len[i], 0);
    enc.encodeBit(len[k], 1);
    uint32_t rem = v + 1 - (1u << k);
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

} // namespace codec