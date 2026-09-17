#include "transform.h"
#include <cmath>

namespace codec {

static const double PI = 3.14159265358979323846;

// مصفوفة DCT مؤقتة لأي حجم حتى 16
template<int SZ>
static void dctMatrix(double C[SZ][SZ]) {
    for (int k = 0; k < SZ; k++) {
        double s = (k == 0) ? std::sqrt(1.0 / SZ) : std::sqrt(2.0 / SZ);
        for (int n = 0; n < SZ; n++)
            C[k][n] = s * std::cos((2.0 * n + 1.0) * k * PI / (2.0 * SZ));
    }
}

template<int SZ>
void dct2dT(const BlockT<SZ>& in, BlockT<SZ>& out) {
    static double C[SZ][SZ];
    static bool init = false;
    if (!init) { dctMatrix<SZ>(C); init = true; }

    double tmp[SZ][SZ];
    for (int i = 0; i < SZ; i++)
        for (int j = 0; j < SZ; j++) {
            double s = 0;
            for (int k = 0; k < SZ; k++) s += C[i][k] * in[k][j];
            tmp[i][j] = s;
        }
    for (int i = 0; i < SZ; i++)
        for (int j = 0; j < SZ; j++) {
            double s = 0;
            for (int k = 0; k < SZ; k++) s += tmp[i][k] * C[j][k];
            out[i][j] = (float)s;
        }
}

template<int SZ>
void idct2dT(const BlockT<SZ>& in, BlockT<SZ>& out) {
    static double C[SZ][SZ];
    static bool init = false;
    if (!init) { dctMatrix<SZ>(C); init = true; }

    double tmp[SZ][SZ];
    for (int i = 0; i < SZ; i++)
        for (int j = 0; j < SZ; j++) {
            double s = 0;
            for (int k = 0; k < SZ; k++) s += C[k][i] * in[k][j];
            tmp[i][j] = s;
        }
    for (int i = 0; i < SZ; i++)
        for (int j = 0; j < SZ; j++) {
            double s = 0;
            for (int k = 0; k < SZ; k++) s += tmp[i][k] * C[k][j];
            out[i][j] = (float)s;
        }
}

// Explicit instantiation
template void dct2dT<8>(const Block&, Block&);
template void dct2dT<16>(const Block16&, Block16&);
template void idct2dT<8>(const Block&, Block&);
template void idct2dT<16>(const Block16&, Block16&);

void dct2d (const Block& in, Block& out)   { dct2dT<8>(in, out); }
void idct2d(const Block& in, Block& out)   { idct2dT<8>(in, out); }

const std::array<std::array<int, N>, N> STD_LUMA_Q = {{
    {16,11,10,16,24,40,51,61},
    {12,12,14,19,26,58,60,55},
    {14,13,16,24,40,57,69,56},
    {14,17,22,29,51,87,80,62},
    {18,22,37,56,68,109,103,77},
    {24,35,55,64,81,104,113,92},
    {49,64,78,87,103,121,120,101},
    {72,92,95,98,112,100,103,99}
}};

// -------- zigzag 8x8 (قيم صريحة كما كانت) --------
const std::array<int, 64> ZIGZAG = {
     0, 1, 8,16, 9, 2, 3,10,
    17,24,32,25,18,11, 4, 5,
    12,19,26,33,40,48,41,34,
    27,20,13, 6, 7,14,21,28,
    35,42,49,56,57,50,43,36,
    29,22,15,23,30,37,44,51,
    58,59,52,45,38,31,39,46,
    53,60,61,54,47,55,62,63
};

// -------- zigzag 16x16 (يُبنى عند التحميل) --------
static std::array<int, 256> buildZigzag16() {
    std::array<int, 256> z{};
    int idx = 0;
    for (int sum = 0; sum <= 2 * 15; sum++) {
        if (sum % 2 == 0) {
            int ystart = (sum < 16) ? sum : 15;
            for (int y = ystart; y >= 0 && sum - y < 16; y--) {
                int x = sum - y;
                if (x >= 0 && x < 16) z[idx++] = y * 16 + x;
            }
        } else {
            int xstart = (sum < 16) ? sum : 15;
            for (int x = xstart; x >= 0 && sum - x < 16; x--) {
                int y = sum - x;
                if (y >= 0 && y < 16) z[idx++] = y * 16 + x;
            }
        }
    }
    return z;
}
const std::array<int, 256> ZIGZAG16 = buildZigzag16();

std::array<std::array<int, N>, N> makeQuantTable(int quality) {
    if (quality < 1)   quality = 1;
    if (quality > 100) quality = 100;
    int scale = (quality < 50) ? (5000 / quality) : (200 - quality * 2);
    std::array<std::array<int, N>, N> q;
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            int v = (STD_LUMA_Q[i][j] * scale + 50) / 100;
            if (v < 1)   v = 1;
            if (v > 255) v = 255;
            q[i][j] = v;
        }
    return q;
}

std::array<std::array<int, 16>, 16> makeQuantTable16(int quality) {
    auto Q8 = makeQuantTable(quality);
    std::array<std::array<int, 16>, 16> q;
    // DCT 16x16 يضخّم المعاملات بـ 2x مقارنة بـ 8x8 (orthonormal)
    // نضاعف قيم Q للجودة المكافئة
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++)
            q[y][x] = 2 * Q8[y * 8 / 16][x * 8 / 16];
    return q;
}

} // namespace codec