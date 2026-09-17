#pragma once
#include <array>
#include <cstdint>

namespace codec {

constexpr int N = 8;

template<int SZ>
using BlockT = std::array<std::array<float, SZ>, SZ>;

using Block   = BlockT<N>;
using Block16 = BlockT<16>;

// DCT عشوائي الأبعاد
template<int SZ>
void dct2dT(const BlockT<SZ>& in, BlockT<SZ>& out);

template<int SZ>
void idct2dT(const BlockT<SZ>& in, BlockT<SZ>& out);

void dct2d (const Block& in,   Block& out);
void idct2d(const Block& in,   Block& out);

extern const std::array<std::array<int, N>, N> STD_LUMA_Q;

// zigzag
extern const std::array<int, 64>  ZIGZAG;    // 8x8
extern const std::array<int, 256> ZIGZAG16;  // 16x16

std::array<std::array<int, N>, N> makeQuantTable(int quality);
std::array<std::array<int, 16>, 16> makeQuantTable16(int quality);

} // namespace codec