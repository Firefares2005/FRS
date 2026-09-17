# frs-codec — Python Bindings for FRS

A Python image compression library. Compress and decompress images in one line of code.

[![PyPI](https://img.shields.io/pypi/v/frs-codec)](https://pypi.org/project/frs-codec/)

---

## Installation

```bash
pip install frs-codec
```

That is all. No C++ compiler needed.

---

## Quick Start

```python
import frs_python as f

# Compress
f.compress("photo.jpg", "photo.frs", 85)

# Decompress
f.decompress("photo.frs", "restored.png")
```

---

## API Reference — 13 Functions

### File-based (simplest)

#### `compress(input, output, quality=85)`

Compress an image file to `.frs`.

```python
f.compress("photo.jpg", "photo.frs", 85)
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `input`   | str  | Path to source image (JPG/PNG/BMP/TGA/PGM/PPM) |
| `output`  | str  | Path to output .frs file |
| `quality` | int  | 1–100 (default: 85) |
| **Returns** | bool | `True` on success |

#### `decompress(input, output)`

Decompress an `.frs` file back to an image.

```python
f.decompress("photo.frs", "restored.png")
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `input`   | str  | Path to .frs file |
| `output`  | str  | Output image path (format by extension) |
| **Returns** | bool | `True` on success |

### In-memory (networking, APIs, streaming)

#### `encode(image_path, quality=85)`

Encode a file to FRS bytes (no disk output).

```python
data = f.encode("photo.jpg", 85)
print(f"Compressed to {len(data)} bytes")
```

| Parameter    | Type | Description |
|--------------|------|-------------|
| `image_path` | str  | Path to source image |
| `quality`    | int  | 1–100 (default: 85) |
| **Returns**  | bytes | FRS compressed bytes |

#### `decode(frs_bytes, output_path)`

Decode FRS bytes to an image file.

```python
f.decode(data, "restored.png")
```

| Parameter     | Type  | Description |
|---------------|-------|-------------|
| `frs_bytes`   | bytes | FRS compressed data |
| `output_path` | str   | Output image path |
| **Returns**   | bool  | `True` on success |

#### `encode_bytes(image_bytes, quality=85)`

Encode raw image bytes (from memory) to FRS bytes.

```python
with open("photo.jpg", "rb") as fp:
    image_bytes = fp.read()

frs_bytes = f.encode_bytes(image_bytes, 85)
```

| Parameter     | Type  | Description |
|---------------|-------|-------------|
| `image_bytes` | bytes | Raw image bytes (JPG/PNG/...) |
| `quality`     | int   | 1–100 (default: 85) |
| **Returns**   | bytes | FRS compressed bytes |

#### `decode_bytes(frs_bytes)`

Decode FRS bytes to raw pixel data.

```python
w, h, channels, pixels = f.decode_bytes(frs_bytes)
print(f"{w}x{h}, {channels} channels, {len(pixels)} bytes")
```

| Parameter   | Type  | Description |
|-------------|-------|-------------|
| `frs_bytes` | bytes | FRS compressed data |
| **Returns** | tuple | `(width, height, channels, pixels)` where `pixels` is bytes |

> **Note:** `pixels` is a flat array in row-major order. For RGB images (channels=3), pixel `(x,y)` channel `c` is at index `(y*width + x)*3 + c`.

### Information

#### `version()`

Return the library version string.

```python
print(f.version())   # "1.1.0"
```

#### `info(frs_file)`

Return metadata of an FRS file.

```python
info = f.info("photo.frs")
print(info)
# {'width': 400, 'height': 400, 'quality': 85, 'channels': 3}
```

| Parameter  | Type | Description |
|------------|------|-------------|
| `frs_file` | str  | Path to .frs file |
| **Returns** | dict | Keys: `width`, `height`, `quality`, `channels` |

Returns empty dict `{}` if the file is not valid FRS.

#### `is_frs(path)`

Check if a file is in FRS format.

```python
if f.is_frs("photo.frs"):
    print("This is an FRS file")
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `path`    | str  | Path to any file |
| **Returns** | bool | `True` if valid FRS |

### Performance

#### `benchmark(image_path, quality=85)`

Measure compression performance.

```python
stats = f.benchmark("photo.jpg", 85)
print(f"Ratio: {stats['ratio']:.2f}x, Time: {stats['time_ms']:.1f}ms")
```

| Parameter    | Type | Description |
|--------------|------|-------------|
| `image_path` | str  | Path to source image |
| `quality`    | int  | 1–100 (default: 85) |
| **Returns**  | dict | Keys: `original_size`, `compressed_size`, `ratio`, `time_ms`, `width`, `height`, `channels` |

Example output:

```text
Ratio: 15.34x, Time: 45.2ms
```

#### `compare(original_path, frs_path)`

Compare original image with FRS compressed version.

```python
r = f.compare("photo.jpg", "photo.frs")
print(f"PSNR: {r['psnr']:.2f} dB, Ratio: {r['ratio']:.2f}x")
```

| Parameter       | Type | Description |
|-----------------|------|-------------|
| `original_path` | str  | Path to original image |
| `frs_path`      | str  | Path to .frs file |
| **Returns**     | dict | Keys: `original_size`, `frs_size`, `ratio`, `psnr` |

PSNR interpretation:

- `< 30 dB` — noticeable quality loss
- `30–35 dB` — good quality
- `35–40 dB` — very good quality
- `> 40 dB` — excellent quality

### Batch

#### `batch_compress(files, output_dir, quality=85)`

Compress multiple files at once.

```python
files = ["a.jpg", "b.png", "c.bmp"]
results = f.batch_compress(files, "./compressed", 85)
for name, ok in results:
    print(f"{name}: {'OK' if ok else 'FAIL'}")
```

| Parameter    | Type      | Description |
|--------------|-----------|-------------|
| `files`      | list[str] | List of file paths |
| `output_dir` | str       | Destination folder |
| `quality`    | int       | 1–100 (default: 85) |
| **Returns**  | list[tuple] | `[(filename, success), ...]` |

### Defaults

#### `get_default_quality()`

Return the default quality value (85).

```python
q = f.get_default_quality()   # 85
```

---

## Supported Formats

- **Input:** PNG, JPG, JPEG, BMP, TGA, PGM, PPM
- **Output:** PNG, JPG, BMP, TGA, PGM, PPM

Format is auto-detected on input, selected by extension on output.

---

## Complete Examples

### Example 1 — Basic compression

```python
import frs_python as f

f.compress("photo.jpg", "photo.frs", 85)
f.decompress("photo.frs", "restored.png")
```

### Example 2 — Batch folder processing

```python
from pathlib import Path
import frs_python as f

# Get all images in folder
files = [str(p) for p in Path("photos").glob("*.jpg")]
files += [str(p) for p in Path("photos").glob("*.png")]

results = f.batch_compress(files, "./compressed", 85)

for name, ok in results:
    print(f"{name}: {'OK' if ok else 'FAIL'}")
```

### Example 3 — FastAPI web service

```python
from fastapi import FastAPI, UploadFile
import frs_python as f

app = FastAPI()

@app.post("/compress")
async def compress_endpoint(file: UploadFile):
    image_bytes = await file.read()
    frs_bytes = f.encode_bytes(image_bytes, 85)

    return {
        "original_size": len(image_bytes),
        "compressed_size": len(frs_bytes),
        "ratio": len(image_bytes) / len(frs_bytes)
    }
```

### Example 4 — Quality check

```python
import frs_python as f

# Try different qualities
for q in [30, 50, 70, 85, 95]:
    stats = f.benchmark("photo.jpg", q)
    print(f"q={q}: {stats['compressed_size']:>6} bytes "
          f"({stats['ratio']:.2f}x)")
```

### Example 5 — Compare with original

```python
import frs_python as f

f.compress("photo.jpg", "photo.frs", 85)
r = f.compare("photo.jpg", "photo.frs")

print(f"Original: {r['original_size']} bytes")
print(f"FRS:      {r['frs_size']} bytes")
print(f"Ratio:    {r['ratio']:.2f}x")
print(f"PSNR:     {r['psnr']:.2f} dB")
```

---

## Quality Guide

| Quality | Use case | Typical size |
|---------|----------|---------------|
| 20–40   | Thumbnails, quick previews | Very small |
| 50–70   | Web images, social media   | Small |
| 75–85   | General purpose (default)  | Balanced |
| 90–95   | Photography, archives      | Larger |
| 96–100  | Near-lossless               | Very large |

---

## Performance

Typical timings on a modern CPU:

| Operation  | Time (400×400 image) |
|------------|------------------------|
| Compress   | ~50 ms |
| Decompress | ~80 ms |

---

## Troubleshooting

**`ModuleNotFoundError: No module named 'frs_python'`**

Make sure the package is installed:

```bash
pip install frs-codec
```

If you have a local `frs_python.cp311-win_amd64.pyd` in your current directory, Python may import that instead of the installed one. Run Python from a different folder.

**`AttributeError: module 'frs_python' has no attribute 'version'`**

You have an old version installed. Upgrade:

```bash
pip install --upgrade --force-reinstall frs-codec
```

**Wrong channel count on output**

`decode_bytes` returns `channels=1` for grayscale images, `channels=3` for color. Adjust your code accordingly.

---

## Development

If you want to build from source:

```bash
# Install dependencies
pip install pybind11 build twine

# Build
cd python_bind
python -m build

# Install locally
pip install dist/frs_codec-*.whl

# Upload to PyPI (maintainers only)
python -m twine upload dist/*
```

---

## License

MIT License

---

## Author

**Firefares2005** — [github.com/Firefares2005](https://github.com/Firefares2005)