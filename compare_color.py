import math, subprocess, os, glob

def read_pnm(p):
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

def run(img_path, label):
    print(f'\n===== {label} =====')
    base = os.path.splitext(img_path)[0]

    pgm = base + '_g.pgm'
    ppm = base + '_c.ppm'
    subprocess.run(['magick', img_path, '-resize', '256x256!',
                    '-colorspace', 'Gray', pgm], stderr=subprocess.DEVNULL)
    subprocess.run(['magick', img_path, '-resize', '256x256!', ppm],
                   stderr=subprocess.DEVNULL)

    for src, tag in [(pgm, 'GRAY'), (ppm, 'COLOR')]:
        if not os.path.exists(src):
            continue
        print(f'\n--- {tag} ---')
        orig = read_pnm(src)
        osize = os.path.getsize(src)

        for q in [30, 50, 80, 95]:
            out_nc = f'nc_{tag}_{q}.nc'
            out_pnm = f'nc_{tag}_{q}.pnm'
            subprocess.run(['.\\nc.exe', 'encode', src, out_nc, str(q)],
                           stderr=subprocess.DEVNULL)
            subprocess.run(['.\\nc.exe', 'decode', out_nc, out_pnm],
                           stderr=subprocess.DEVNULL)

        subprocess.run(['magick', src, f'png_{tag}.png'],
                       stderr=subprocess.DEVNULL)
        subprocess.run(['magick', src, '-quality', '80', f'jpg_{tag}.jpg'],
                       stderr=subprocess.DEVNULL)
        subprocess.run(['magick', src, '-quality', '80', f'webp_{tag}.webp'],
                       stderr=subprocess.DEVNULL)
        subprocess.run(['magick', src, '-quality', '80', f'jxl_{tag}.jxl'],
                       stderr=subprocess.DEVNULL)

        print(f"{'Format':<20}{'Bytes':>10}{'Ratio':>10}{'PSNR(dB)':>12}")
        print('=' * 54)
        print(f"{'Original':<20}{osize:>10}{'1.00x':>10}{'-':>12}")

        for q in [30, 50, 80, 95]:
            nc = f'nc_{tag}_{q}.nc'
            pnm = f'nc_{tag}_{q}.pnm'
            if not os.path.exists(nc):
                continue
            sz = os.path.getsize(nc)
            if os.path.exists(pnm):
                p = psnr(orig, read_pnm(pnm))
                qs = f'{p:.2f}'
            else:
                qs = '-'
            print(f"{'NC05 q=' + str(q):<20}{sz:>10}{osize/sz:>9.2f}x{qs:>12}")

        for f, lbl in [(f'png_{tag}.png', 'PNG'),
                       (f'jpg_{tag}.jpg', 'JPEG q=80'),
                       (f'webp_{tag}.webp', 'WebP q=80'),
                       (f'jxl_{tag}.jxl', 'JXL q=80')]:
            if not os.path.exists(f):
                continue
            sz = os.path.getsize(f)
            pnm = f.replace('.', '_') + '.pnm'
            subprocess.run(['magick', f, pnm], stderr=subprocess.DEVNULL)
            try:
                p = psnr(orig, read_pnm(pnm))
                qs = 'lossless' if p == float('inf') else f'{p:.2f}'
            except:
                qs = '-'
            print(f"{lbl:<20}{sz:>10}{osize/sz:>9.2f}x{qs:>12}")

imgs = []
for ext in ['*.png', '*.jpg', '*.jpeg']:
    imgs += glob.glob(ext)
imgs = [i for i in imgs if '_g.' not in i and '_c.' not in i
        and 'nc_' not in i and 'png_' not in i and 'jpg_' not in i
        and 'webp_' not in i and 'jxl_' not in i]

if not imgs:
    print('No test images. Place a PNG or JPG in D:\\FRS')
else:
    run(imgs[0], f'Image: {imgs[0]}')

print('\nSmaller bytes + higher PSNR = better.')