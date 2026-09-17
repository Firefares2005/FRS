#include "transform.h"
#include <cmath>

namespace codec {

static const double PI = 3.14159265358979323846;
static double C[N][N];
static bool   g_init = false;

static void initTables() {
    if (g_init) return;
    for (int k = 0; k < N; k++) {
        double s = (k == 0) ? std::sqrt(1.0 / N) : std::sqrt(2.0 / N);
        for (int n = 0; n < N; n++)
            C[k][n] = s * std::cos((2.0 * n + 1.0) * k * PI / (2.0 * N));
    }
    g_init = true;
}

void dct2d(const Block& in, Block& out) {
    initTables();
    double tmp[N][N];
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            double s = 0;
            for (int k = 0; k < N; k++) s += C[i][k] * in[k][j];
            tmp[i][j] = s;
        }
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            double s = 0;
            for (int k = 0; k < N; k++) s += tmp[i][k] * C[j][k];
            out[i][j] = (float)s;
        }
}

void idct2d(const Block& in, Block& out) {
    initTables();
    double tmp[N][N];
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            double s = 0;
            for (int k = 0; k < N; k++) s += C[k][i] * in[k][j];
            tmp[i][j] = s;
        }
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            double s = 0;
            for (int k = 0; k < N; k++) s += tmp[i][k] * C[k][j];
            out[i][j] = (float)s;
        }
}

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

} // namespace codec