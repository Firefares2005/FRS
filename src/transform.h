#pragma once
#include <array>
#include <cstdint>

namespace codec {

constexpr int N = 8;
using Block = std::array<std::array<float, N>, N>;

// DCT-II orthonormal 2D
void dct2d (const Block& in, Block& out);
void idct2d(const Block& in, Block& out);

extern const std::array<std::array<int, N>, N> STD_LUMA_Q;
extern const std::array<int, 64> ZIGZAG;

std::array<std::array<int, N>, N> makeQuantTable(int quality);

} // namespace codec