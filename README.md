# new-codec v2 (NC02)

معيار ضغط صور تجريبي بأداء قريب من JPEG XL على صور معينة.

## الميزات الجديدة في NC02

1. **DC prediction** من الجارين (يسار + أعلى) بمتوسط موزون
2. **Block skipping** — تخطي الكتل المتطابقة مع التنبؤ
3. **Adaptive context** — 4 سياقات للنماذج حسب نشاط المنطقة
4. **Zero-run length coding** — لترميز صفوف الأصفار في AC
5. **ترويسة جديدة NC02** (لا تتوافق مع NC01)

## البناء

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
ctest --output-on-failure