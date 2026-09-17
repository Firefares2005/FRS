#include "models.h"
#include <algorithm>
#include <vector>

namespace codec {

CoefInfo prepareCoefInfo(int W, int H, int L) {
    int N = W * H;
    CoefInfo info;
    info.subband.assign(N, 0);
    info.lev.assign(N, 0);
    info.parent.assign(N, -1);
    info.order.resize(N);

    std::vector<int> Wk(L + 1), Hk(L + 1);
    Wk[0] = W; Hk[0] = H;
    for (int k = 1; k <= L; k++) {
        Wk[k] = (Wk[k-1] + 1) / 2;
        Hk[k] = (Hk[k-1] + 1) / 2;
    }

    std::vector<int> orderKey(N);

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int i = y * W + x;
            int lev = 0;
            for (int k = 1; k <= L; k++) {
                if (x < Wk[k-1] && y < Hk[k-1] &&
                    (x >= Wk[k] || y >= Hk[k])) {
                    lev = k;
                    break;
                }
            }
            info.lev[i] = (uint8_t)lev;

            if (lev == 0) {
                info.subband[i] = 0;
                if (lev < L) {
                    int px = x / 2, py = y / 2;
                    if (px < Wk[lev+1] && py < Hk[lev+1])
                        info.parent[i] = py * W + px;
                }
            } else {
                bool right  = (x >= Wk[lev]);
                bool bottom = (y >= Hk[lev]);
                if (right && !bottom)       info.subband[i] = 1; // HL
                else if (!right && bottom)  info.subband[i] = 2; // LH
                else                         info.subband[i] = 3; // HH

                if (lev < L) {
                    int kp = lev + 1;
                    int px, py;
                    if (right && !bottom) {          // HL
                        px = Wk[kp] + (x - Wk[lev]) / 2;
                        py = y / 2;
                    } else if (!right && bottom) {   // LH
                        px = x / 2;
                        py = Hk[kp] + (y - Hk[lev]) / 2;
                    } else {                          // HH
                        px = Wk[kp] + (x - Wk[lev]) / 2;
                        py = Hk[kp] + (y - Hk[lev]) / 2;
                    }
                    if (px >= 0 && px < W && py >= 0 && py < H)
                        info.parent[i] = py * W + px;
                }
            }

            orderKey[i] = (lev == 0) ? 0 : (L - lev + 1);
        }
    }

    for (int i = 0; i < N; i++) info.order[i] = i;
    std::sort(info.order.begin(), info.order.end(), [&](int a, int b) {
        if (orderKey[a] != orderKey[b]) return orderKey[a] < orderKey[b];
        return a < b;
    });

    return info;
}

} // namespace codec