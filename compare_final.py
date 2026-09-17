import math, subprocess, os

def read_pnm(p):
    with open(p, 'rb') as f:
        magic = f.readline().strip()
        line = f.readline()
        while line.startswith(b'#'):
            line = f.readline()
        w, h = map(int, line.split())
        f.readline()
        return f.read()

def psnr(a, b):
    mse = sum((x - y) ** 2 for x, y in zip(a, b)) / len(a)
    return float('inf') if mse == 0 else 10 * math.log10(255 * 255 / mse)

print(f"{'Image':<6}{'Codec':<10}{'Bytes':>10}{'Ratio':>10}{'PSNR(dB)':>12}")
print('=' * 50)

for name in ['t1', 't2', 't3', 't4', 't5']:
    ppm = f'{name}_C.ppm'
    if not os.path.exists(ppm):
        continue

    orig = read_pnm(ppm)
    osize = os.path.getsize(ppm)

    # NC06
    nc = f'{name}_nc.nc'
    nc_out = f'{name}_nc_out.ppm'
    if os.path.exists(nc):
        subprocess.run(['.\\nc.exe', 'decode', nc, nc_out],
                       stderr=subprocess.DEVNULL)
        if os.path.exists(nc_out):
            sz = os.path.getsize(nc)
            p = psnr(orig, read_pnm(nc_out))
            print(f"{name:<6}{'NC06':<10}{sz:>10}{osize/sz:>9.2f}x{p:>12.2f}")

    # WebP
    wp = f'{name}_webp.webp'
    wp_out = f'{name}_webp_out.ppm'
    if os.path.exists(wp):
        subprocess.run(['magick', wp, wp_out], stderr=subprocess.DEVNULL)
        if os.path.exists(wp_out):
            sz = os.path.getsize(wp)
            p = psnr(orig, read_pnm(wp_out))
            print(f"{name:<6}{'WebP':<10}{sz:>10}{osize/sz:>9.2f}x{p:>12.2f}")

    # JXL
    jxl = f'{name}_jxl.jxl'
    jxl_out = f'{name}_jxl_out.ppm'
    if os.path.exists(jxl):
        subprocess.run(['magick', jxl, jxl_out], stderr=subprocess.DEVNULL)
        if os.path.exists(jxl_out):
            sz = os.path.getsize(jxl)
            p = psnr(orig, read_pnm(jxl_out))
            print(f"{name:<6}{'JXL':<10}{sz:>10}{osize/sz:>9.2f}x{p:>12.2f}")

    # JPEG
    jpg = f'{name}_jpg.jpg'
    jpg_out = f'{name}_jpg_out.ppm'
    if os.path.exists(jpg):
        subprocess.run(['magick', jpg, jpg_out], stderr=subprocess.DEVNULL)
        if os.path.exists(jpg_out):
            sz = os.path.getsize(jpg)
            p = psnr(orig, read_pnm(jpg_out))
            print(f"{name:<6}{'JPEG':<10}{sz:>10}{osize/sz:>9.2f}x{p:>12.2f}")

    print()