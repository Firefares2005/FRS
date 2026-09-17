import math, subprocess, os, glob

def read_pgm(p):
    with open(p, 'rb') as f:
        magic = f.readline().strip()
        assert magic in (b'P5', b'P6'), magic
        line = f.readline()
        while line.startswith(b'#'):
            line = f.readline()
        w, h = map(int, line.split())
        f.readline()
        return f.read()

def psnr(a, b):
    mse = sum((x - y) ** 2 for x, y in zip(a, b)) / len(a)
    return float('inf') if mse == 0 else 10 * math.log10(255 * 255 / mse)

def process(img_path, label, size=256):
    print(f'\n===== {label} =====')
    base = os.path.splitext(img_path)[0]
    pgm = base + '_test.pgm'

    subprocess.run(['magick', img_path, '-resize', f'{size}x{size}!',
                    '-colorspace', 'Gray', pgm], stderr=subprocess.DEVNULL)

    if not os.path.exists(pgm):
        print('  (failed to convert)')
        return

    orig_size = os.path.getsize(pgm)
    orig = read_pgm(pgm)

    for q in [30, 50, 80, 95]:
        subprocess.run(['.\\nc.exe', 'encode', pgm, f'nc_{q}.nc', str(q)],
                       stderr=subprocess.DEVNULL)
        subprocess.run(['.\\nc.exe', 'decode', f'nc_{q}.nc', f'nc_{q}.pgm'],
                       stderr=subprocess.DEVNULL)

    subprocess.run(['magick', pgm, 'png.png'], stderr=subprocess.DEVNULL)
    subprocess.run(['magick', pgm, '-quality', '80', 'jpg80.jpg'], stderr=subprocess.DEVNULL)
    subprocess.run(['magick', pgm, '-quality', '80', 'webp80.webp'], stderr=subprocess.DEVNULL)
    subprocess.run(['magick', pgm, '-quality', '80', 'jxl80.jxl'], stderr=subprocess.DEVNULL)

    for f in ['jpg80.jpg', 'webp80.webp', 'jxl80.jxl']:
        if os.path.exists(f):
            subprocess.run(['magick', f, f.replace('.', '_') + '.pgm'],
                           stderr=subprocess.DEVNULL)

    print(f"{'Format':<20}{'Bytes':>10}{'Ratio':>10}{'PSNR(dB)':>12}")
    print('=' * 54)
    print(f"{'Original':<20}{orig_size:>10}{'1.00x':>10}{'-':>12}")

    for q in [30, 50, 80, 95]:
        f = f'nc_{q}.nc'
        if not os.path.exists(f):
            continue
        sz = os.path.getsize(f)
        pg = f.replace('.nc', '.pgm')
        if os.path.exists(pg):
            p = psnr(orig, read_pgm(pg))
            qs = f'{p:.2f}'
        else:
            qs = '-'
        print(f"{'NC04 q='+str(q):<20}{sz:>10}{orig_size/sz:>9.2f}x{qs:>12}")

    for f, lbl in [('png.png', 'PNG'), ('jpg80.jpg', 'JPEG q=80'),
                   ('webp80.webp', 'WebP q=80'), ('jxl80.jxl', 'JXL q=80')]:
        if not os.path.exists(f):
            continue
        sz = os.path.getsize(f)
        pg = f.replace('.', '_') + '.pgm'
        if os.path.exists(pg):
            p = psnr(orig, read_pgm(pg))
            qs = 'lossless' if p == float('inf') else f'{p:.2f}'
        else:
            qs = '-'
        print(f"{lbl:<20}{sz:>10}{orig_size/sz:>9.2f}x{qs:>12}")

imgs = []
for ext in ['*.png', '*.jpg', '*.jpeg', '*.bmp']:
    imgs += glob.glob(ext)
imgs = [i for i in imgs if '_test' not in i and 'nc_' not in i
        and i not in ('png.png',) and 'real_' not in i]

if not imgs:
    print('No test images found. Place a PNG/JPG in D:\\FRS')
else:
    for img in imgs[:3]:
        process(img, f'Image: {img}')

print('\nDone. Smaller bytes + higher PSNR = better.')