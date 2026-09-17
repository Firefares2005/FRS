# FRS — Fast Range-compressed Subband Codec

> A modern image compression codec built from scratch in C++17.
> Competitive with JPEG XL, WebP, and JPEG 2000.

[![PyPI](https://img.shields.io/pypi/v/frs-codec?color=blue)](https://pypi.org/project/frs-codec/)
[![Python](https://img.shields.io/badge/python-3.8%2B-blue)](https://pypi.org/project/frs-codec/)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-orange)]()
[![License](https://img.shields.io/badge/license-MIT-green)]()

---

## 📦 Installation

### 🐍 Python (easiest)

```bash
pip install frs-codec
```

```python
import frs_python as f

f.compress("photo.jpg", "photo.frs", 85)
f.decompress("photo.frs", "restored.png")
```

### 🖥️ Command-line tool

```bash
frs.exe encode photo.jpg photo.frs 85
frs.exe decode photo.frs restored.png
```

### 📚 C++ library

See `dist/README.md` for details.

```bash
g++ app.cpp -IC:\frs\include -LC:\frs\lib -lfrs -o app.exe
```

---

## 🐍 Python API — 13 Functions

| Function | Description |
|----------|--------------|
| `compress(input, output, quality=85)` | Compress image file to .frs |
| `decompress(input, output)` | Decompress .frs to image |
| `encode(image_path, quality=85)` | Encode file to FRS bytes |
| `decode(frs_bytes, output_path)` | Decode bytes to image file |
| `encode_bytes(image_bytes, quality=85)` | Encode image bytes to FRS bytes |
| `decode_bytes(frs_bytes)` | Decode to (w, h, channels, pixels) |
| `version()` | Return version string |
| `info(frs_file)` | Return dict: width, height, quality, channels |
| `is_frs(path)` | Check if file is FRS format |
| `benchmark(image_path, quality=85)` | Return size ratio + encoding time |
| `compare(original, frs_file)` | Return PSNR + compression ratio |
| `batch_compress(files, output_dir, quality=85)` | Compress multiple files |
| `get_default_quality()` | Return default quality (85) |

### Examples

**Basic compression:**

```python
import frs_python as f
f.compress("photo.jpg", "photo.frs", 85)
f.decompress("photo.frs", "restored.png")
```

**Get file info:**

```python
info = f.info("photo.frs")
print(f"{info['width']}x{info['height']}, quality={info['quality']}")
# Output: 400x400, quality=85
```

**Benchmark performance:**

```python
stats = f.benchmark("photo.jpg", 85)
print(f"Ratio: {stats['ratio']:.2f}x, Time: {stats['time_ms']:.1f}ms")
```

**Compare quality:**

```python
r = f.compare("photo.jpg", "photo.frs")
print(f"PSNR: {r['psnr']:.2f} dB, Ratio: {r['ratio']:.2f}x")
```

**Batch compress:**

```python
files = ["a.jpg", "b.png", "c.bmp"]
results = f.batch_compress(files, "./compressed", 85)
for name, ok in results:
    print(f"{name}: {'OK' if ok else 'FAIL'}")
```

**In-memory (networking / APIs):**

```python
with open("photo.jpg", "rb") as fp:
    image_bytes = fp.read()
frs_bytes = f.encode_bytes(image_bytes, 85)

# Send over network...

w, h, channels, pixels = f.decode_bytes(frs_bytes)
```

---

## 📊 Benchmark Results

### Small images (256×256, quality 85)

| Codec       | Size (bytes) | PSNR (dB) |
|-------------|--------------|-----------|
| **FRS**     | 10,731       | 33.10     |
| JPEG XL q80 | 12,504       | 32.30     |
| WebP q80    | 14,058       | 35.09     |
| JPEG q80    | 16,414       | 33.02     |

### Large images (1024×1024, quality 85)

| Codec   | t1     | t2     | t3      |
|---------|--------|--------|---------|
| **FRS** | 37,675 | 33,401 | 50,690  |
| JPEG XL | 70,573 | 68,005 | 95,373  |
| WebP    | 73,222 | 69,092 | 103,908 |

**FRS produces files 15–50% smaller than JPEG XL on typical images.**

---

## 🧠 How it works

1. **Color conversion**: RGB → YCbCr with adaptive 4:4:4 / 4:2:0 sampling
2. **CDF 9/7 Wavelet Transform** (5–6 levels)
3. **Adaptive subband quantization**
4. **SPIHT entropy coding**
5. **LZMA-style Range Coder**

---

## 📁 Project Structure

```text
FRS/
├── README.md              ← this file
├── frs.h                  ← C++ library header
├── frs.cpp                ← C++ library implementation
├── libfrs.a               ← pre-built C++ library
├── frs.exe                ← command-line tool
├── dist/                  ← C++ library distribution
│   ├── include/frs.h
│   ├── lib/libfrs.a
│   └── README.md
├── python_bind/           ← Python bindings
│   ├── pyfrs.cpp
│   ├── setup.py
│   ├── pyproject.toml
│   └── MANIFEST.in
└── src/                   ← source code
    ├── entropy.h/.cpp
    ├── wavelet.h
    ├── spiht.h
    ├── encoder.h/.cpp
    ├── decoder.cpp
    ├── image_io.h/.cpp
    └── main.cpp
```

---

## 🚀 Building from source

Requirements: C++17 compiler (GCC 7+, Clang 5+, MSVC 2019+)

```bash
g++ -std=c++17 -O3 -Isrc \
    src/entropy.cpp src/image_io.cpp \
    src/encoder.cpp src/decoder.cpp src/main.cpp \
    -o frs.exe
```

---

## 📝 File Format (NC09)

| Field    | Size    | Description               |
|----------|---------|----------------------------|
| magic    | 4 B     | `"NC09"`                   |
| width    | 4 B     | LE uint32 (original)       |
| height   | 4 B     | LE uint32 (original)       |
| quality  | 1 B     | 1–100                      |
| channels | 1 B     | 1 (gray) or 3 (RGB)        |
| body     | varies  | Range-coded data           |

Images with dimensions not divisible by 64 are auto-padded and cropped.

---

## 🗺️ Roadmap

- [x] Wavelet transform + SPIHT
- [x] Adaptive quantization
- [x] PNG/JPG direct support
- [x] Arbitrary image dimensions
- [x] C++ library (`libfrs.a`)
- [x] Python bindings (PyPI: `frs-codec`)
- [x] 13-function Python API
- [ ] WebAssembly (browser)
- [ ] Lossless mode
- [ ] Alpha channel support
- [ ] HDR / 16-bit support

---

## 📄 License

MIT License

---

## 👤 Author

**Firefares2005** — [github.com/Firefares2005](https://github.com/Firefares2005)

---

## 🙏 Acknowledgments

- SPIHT — Said & Pearlman, 1996
- CDF 9/7 wavelet — Cohen, Daubechies, Feauveau, 1992
- LZMA range coder — Igor Pavlov
- stb_image — Sean Barrett