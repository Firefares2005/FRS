import math, subprocess, os

W = H = 256
d = [max(0, min(255, int(128 + 80*math.sin(x*0.05) + 60*math.cos(y*0.07))))
     for y in range(H) for x in range(W)]
open('img.pgm','wb').write(b'P5\n%d %d\n255\n' % (W, H) + bytes(d))
print('Test image: img.pgm (256x256)\n')

for q in [30, 50, 80, 95]:
    subprocess.run(['.\\nc.exe', 'encode', 'img.pgm', 'nc_q%d.nc' % q, str(q)],
                   stderr=subprocess.DEVNULL)

for cmd in [['magick','img.pgm','png.png'],
            ['magick','img.pgm','-quality','80','jpg80.jpg'],
            ['magick','img.pgm','-quality','80','webp80.webp'],
            ['magick','img.pgm','-quality','80','jxl80.jxl']]:
    try: subprocess.run(cmd, stderr=subprocess.DEVNULL, timeout=15)
    except: pass

def read_pgm(p):
    with open(p,'rb') as f:
        assert f.readline().strip() == b'P5'
        w, h = map(int, f.readline().split())
        f.readline()
        return f.read()

def psnr(a, b):
    mse = sum((x-y)**2 for x, y in zip(a, b)) / len(a)
    return float('inf') if mse == 0 else 10*math.log10(255*255/mse)

for q in [30, 50, 80, 95]:
    subprocess.run(['.\\nc.exe', 'decode', 'nc_q%d.nc' % q, 'nc_q%d.pgm' % q],
                   stderr=subprocess.DEVNULL)

for f in ['jpg80.jpg', 'webp80.webp', 'jxl80.jxl']:
    if os.path.exists(f):
        try: subprocess.run(['magick', f, f.replace('.','_') + '.pgm'],
                            stderr=subprocess.DEVNULL, timeout=15)
        except: pass

orig = read_pgm('img.pgm')
osize = os.path.getsize('img.pgm')

rows = [('img.pgm', 'Original', 'img.pgm')]
for q in [30, 50, 80, 95]:
    rows.append(('nc_q%d.nc' % q, 'NC q=%d' % q, 'nc_q%d.pgm' % q))
rows.append(('png.png', 'PNG (lossless)', None))
for f, l in [('jpg80.jpg','JPEG q=80'), ('webp80.webp','WebP q=80'), ('jxl80.jxl','JXL q=80')]:
    rows.append((f, l, f.replace('.','_') + '.pgm'))

print("%-20s%10s%10s%12s" % ('Format', 'Bytes', 'Ratio', 'PSNR(dB)'))
print('=' * 54)
for f, label, pgm in rows:
    if not os.path.exists(f):
        continue
    sz = os.path.getsize(f)
    ratio = osize / sz
    if pgm is None:
        qs = 'lossless'
    elif pgm == 'img.pgm':
        qs = '-'
    elif os.path.exists(pgm):
        p = psnr(orig, read_pgm(pgm))
        qs = 'lossless' if p == float('inf') else '%.2f' % p
    else:
        qs = '-'
    print('%-20s%10d%9.2fx%12s' % (label, sz, ratio, qs))
print()
print('Smaller bytes + higher PSNR = better')