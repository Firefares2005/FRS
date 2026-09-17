import math, subprocess, os

def read_pnm(p):
    with open(p, 'rb') as f:
        f.readline()  # magic
        line = f.readline()
        while line.startswith(b'#'):
            line = f.readline()
        w, h = map(int, line.split())
        f.readline()
        return f.read()

def psnr(a, b):
    if len(a) != len(b): return None
    mse = sum((x - y) ** 2 for x, y in zip(a, b)) / len(a)
    return float('inf') if mse == 0 else 10 * math.log10(255 * 255 / mse)

print(f"{'Image':<8}{'Codec':<8}{'Bytes':>10}{'Ratio':>10}{'PSNR(dB)':>12}")
print('=' * 48)

for name in ['t1', 't2', 't3']:
    orig_ppm = f'{name}_L.ppm'
    if not os.path.exists(orig_ppm):
        continue
    orig = read_pnm(orig_ppm)
    osize = os.path.getsize(orig_ppm)

    # NC08
    nc = f'{name}_L.nc'
    nc_out = f'{name}_L_nc_out.ppm'
    if os.path.exists(nc):
        subprocess.run(['.\\nc.exe', 'decode', nc, nc_out], stderr=subprocess.DEVNULL)
        if os.path.exists(nc_out):
            sz = os.path.getsize(nc)
            p = psnr(orig, read_pnm(nc_out))
            print(f"{name:<8}{'NC08':<8}{sz:>10}{osize/sz:>9.2f}x{p:>12.2f}")

    # WebP
    wp = f'{name}_L_webp.webp'
    wp_out = f'{name}_L_webp_out.ppm'
    if os.path.exists(wp):
        subprocess.run(['magick', wp, wp_out], stderr=subprocess.DEVNULL)
        if os.path.exists(wp_out):
            sz = os.path.getsize(wp)
            p = psnr(orig, read_pnm(wp_out))
            print(f"{name:<8}{'WebP':<8}{sz:>10}{osize/sz:>9.2f}x{p:>12.2f}")

    # JXL
    jxl = f'{name}_L_jxl.jxl'
    jxl_out = f'{name}_L_jxl_out.ppm'
    if os.path.exists(jxl):
        subprocess.run(['magick', jxl, jxl_out], stderr=subprocess.DEVNULL)
        if os.path.exists(jxl_out):
            sz = os.path.getsize(jxl)
            p = psnr(orig, read_pnm(jxl_out))
            print(f"{name:<8}{'JXL':<8}{sz:>10}{osize/sz:>9.2f}x{p:>12.2f}")
    print()