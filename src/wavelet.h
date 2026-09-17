#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace codec {

static const float CDF97_A = -1.586134342f;
static const float CDF97_B = -0.05298011854f;
static const float CDF97_G =  0.8829110762f;
static const float CDF97_D =  0.4435068522f;
static const float CDF97_K =  1.230174105f;

inline void dwt97_forward(std::vector<float>& x) {
    int n = (int)x.size();
    if (n < 2) return;
    int half = (n + 1) / 2;
    std::vector<float> s(half), d(n - half);
    for (int i = 0; i < half; i++)      s[i] = x[2*i];
    for (int i = 0; i < n - half; i++)  d[i] = x[2*i + 1];

    for (int i = 0; i < (int)d.size(); i++) {
        float sR = (i + 1 < (int)s.size()) ? s[i+1] : s.back();
        d[i] += CDF97_A * (s[i] + sR);
    }
    for (int i = 0; i < (int)s.size(); i++) {
        float dL = (i > 0) ? d[i-1] : d[0];
        s[i] += CDF97_B * (dL + d[i]);
    }
    for (int i = 0; i < (int)d.size(); i++) {
        float sR = (i + 1 < (int)s.size()) ? s[i+1] : s.back();
        d[i] += CDF97_G * (s[i] + sR);
    }
    for (int i = 0; i < (int)s.size(); i++) {
        float dL = (i > 0) ? d[i-1] : d[0];
        s[i] += CDF97_D * (dL + d[i]);
    }
    for (auto& v : s) v *= CDF97_K;
    for (auto& v : d) v /= CDF97_K;

    for (int i = 0; i < half; i++)     x[i]        = s[i];
    for (int i = 0; i < n - half; i++) x[half + i] = d[i];
}

inline void dwt97_inverse(std::vector<float>& x) {
    int n = (int)x.size();
    if (n < 2) return;
    int half = (n + 1) / 2;
    std::vector<float> s(half), d(n - half);
    for (int i = 0; i < half; i++)     s[i] = x[i] / CDF97_K;
    for (int i = 0; i < n - half; i++) d[i] = x[half + i] * CDF97_K;

    for (int i = 0; i < (int)s.size(); i++) {
        float dL = (i > 0) ? d[i-1] : d[0];
        s[i] -= CDF97_D * (dL + d[i]);
    }
    for (int i = 0; i < (int)d.size(); i++) {
        float sR = (i + 1 < (int)s.size()) ? s[i+1] : s.back();
        d[i] -= CDF97_G * (s[i] + sR);
    }
    for (int i = 0; i < (int)s.size(); i++) {
        float dL = (i > 0) ? d[i-1] : d[0];
        s[i] -= CDF97_B * (dL + d[i]);
    }
    for (int i = 0; i < (int)d.size(); i++) {
        float sR = (i + 1 < (int)s.size()) ? s[i+1] : s.back();
        d[i] -= CDF97_A * (s[i] + sR);
    }

    for (int i = 0; i < half; i++)     x[2*i]     = s[i];
    for (int i = 0; i < n - half; i++) x[2*i + 1] = d[i];
}

inline void dwt2d(std::vector<float>& img, int W, int H, int levels) {
    int cw = W, ch = H;
    for (int L = 0; L < levels && cw > 1 && ch > 1; L++) {
        int newCW = (cw + 1) / 2;
        int newCH = (ch + 1) / 2;

        std::vector<float> line(std::max(cw, ch));
        for (int y = 0; y < ch; y++) {
            line.resize(cw);
            for (int x = 0; x < cw; x++) line[x] = img[y * W + x];
            dwt97_forward(line);
            for (int x = 0; x < cw; x++) img[y * W + x] = line[x];
        }
        for (int x = 0; x < cw; x++) {
            line.resize(ch);
            for (int y = 0; y < ch; y++) line[y] = img[y * W + x];
            dwt97_forward(line);
            for (int y = 0; y < ch; y++) img[y * W + x] = line[y];
        }
        cw = newCW;
        ch = newCH;
    }
}

inline void idwt2d(std::vector<float>& img, int W, int H, int levels) {
    std::vector<std::pair<int,int>> sizes;
    int cw = W, ch = H;
    sizes.push_back({cw, ch});
    for (int L = 0; L < levels && cw > 1 && ch > 1; L++) {
        cw = (cw + 1) / 2;
        ch = (ch + 1) / 2;
        sizes.push_back({cw, ch});
    }
    int actualLevels = (int)sizes.size() - 1;

    std::vector<float> line;
    for (int L = actualLevels; L >= 1; L--) {
        int wPrev = sizes[L-1].first;
        int hPrev = sizes[L-1].second;
        for (int x = 0; x < wPrev; x++) {
            line.resize(hPrev);
            for (int y = 0; y < hPrev; y++) line[y] = img[y * W + x];
            dwt97_inverse(line);
            for (int y = 0; y < hPrev; y++) img[y * W + x] = line[y];
        }
        for (int y = 0; y < hPrev; y++) {
            line.resize(wPrev);
            for (int x = 0; x < wPrev; x++) line[x] = img[y * W + x];
            dwt97_inverse(line);
            for (int x = 0; x < wPrev; x++) img[y * W + x] = line[x];
        }
    }
}

} // namespace codec