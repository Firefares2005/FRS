# new-codec

معيار ضغط صور تجريبي (Grayscale PGM/P5).

## المكوّنات

- **transform**: DCT-II ثنائي الأبعاد 8×8 (متعامد) + تكميم JPEG.
- **entropy**: Range Coder تكيّفي بأسلوب LZMA (BitModel ثنائي).
- **encoder/decoder**: تكميم + ترميز فرق DC + ترميز AC بـ exp-golomb تكيّفي.
- **main**: أداة CLI.
- **tests**: اختبار roundtrip + حساب PSNR.

## البناء

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
ctest --output-on-failure