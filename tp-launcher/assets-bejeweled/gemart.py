"""Procedural launcher art: faceted gems over a starry nebula (original, no game assets)."""
import math, random, sys
from PIL import Image, ImageDraw, ImageFilter, ImageChops

def lerp(a, b, t): return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))

def nebula(w, h, seed):
    rnd = random.Random(seed)
    img = Image.new('RGB', (w, h), (6, 10, 28))
    # vertical gradient navy -> deep violet
    g = Image.new('RGB', (1, h))
    for y in range(h):
        g.putpixel((0, y), lerp((8, 14, 40), (30, 10, 52), y / h))
    img = g.resize((w, h))
    # soft color clouds
    cloud = Image.new('RGB', (w, h), (0, 0, 0))
    d = ImageDraw.Draw(cloud)
    for _ in range(26):
        cx, cy = rnd.uniform(0, w), rnd.uniform(0, h)
        r = rnd.uniform(0.12, 0.35) * w
        col = rnd.choice([(40, 90, 200), (110, 40, 170), (20, 140, 190), (170, 50, 140)])
        d.ellipse([cx - r, cy - r * 0.6, cx + r, cy + r * 0.6], fill=tuple(c // 3 for c in col))
    cloud = cloud.filter(ImageFilter.GaussianBlur(w / 18))
    img = ImageChops.add(img, cloud)
    # stars
    d = ImageDraw.Draw(img)
    for _ in range(int(w * h / 2500)):
        x, y = rnd.uniform(0, w), rnd.uniform(0, h)
        b = rnd.randint(120, 255); s = rnd.choice([0, 0, 0, 1])
        d.ellipse([x - s, y - s, x + s + 1, y + s + 1], fill=(b, b, min(255, b + 20)))
    return img

def bokeh(img, n, seed):
    rnd = random.Random(seed); w, h = img.size
    layer = Image.new('RGB', img.size, (0, 0, 0)); d = ImageDraw.Draw(layer)
    for _ in range(n):
        x, y, r = rnd.uniform(0, w), rnd.uniform(0, h), rnd.uniform(0.01, 0.045) * w
        c = rnd.choice([(60, 110, 200), (120, 70, 190), (50, 150, 200)])
        d.ellipse([x - r, y - r, x + r, y + r], fill=tuple(v // 4 for v in c))
    return ImageChops.add(img, layer.filter(ImageFilter.GaussianBlur(w / 400)))

def rays(img, cx, cy, seed):
    rnd = random.Random(seed); w, h = img.size
    layer = Image.new('RGB', img.size, (0, 0, 0)); d = ImageDraw.Draw(layer)
    for i in range(36):
        a = i / 36 * 2 * math.pi + rnd.uniform(-0.04, 0.04); da = rnd.uniform(0.015, 0.04)
        L = max(w, h)
        col = rnd.choice([(70, 120, 255), (160, 90, 255), (60, 200, 255), (255, 200, 90)])
        d.polygon([(cx, cy), (cx + L * math.cos(a - da), cy + L * math.sin(a - da)),
                   (cx + L * math.cos(a + da), cy + L * math.sin(a + da))], fill=tuple(c // 5 for c in col))
    return ImageChops.add(img, layer.filter(ImageFilter.GaussianBlur(w / 250)))

GEMS = {  # name: (sides, rotation, base color)
    'red': (4, math.pi / 4, (225, 30, 50)),
    'orange': (6, 0, (255, 130, 20)),
    'yellow': (4, 0, (255, 215, 30)),
    'green': (8, math.pi / 8, (30, 200, 70)),
    'blue': (5, -math.pi / 2, (40, 130, 255)),
    'purple': (3, -math.pi / 2, (200, 50, 230)),
    'white': (12, 0, (235, 235, 245)),
}

def gem(size, kind, glow=True):
    n, rot, base = GEMS[kind]
    S = size * 2; img = Image.new('RGBA', (S, S), (0, 0, 0, 0)); cx = cy = S / 2; R = size * 0.42
    outer = [(cx + R * math.cos(rot + 2 * math.pi * k / n), cy + R * math.sin(rot + 2 * math.pi * k / n)) for k in range(n)]
    inner = [(cx + 0.55 * (x - cx), cy - R * 0.06 + 0.55 * (y - cy)) for x, y in outer]
    if glow:
        gl = Image.new('RGBA', (S, S), (0, 0, 0, 0)); ImageDraw.Draw(gl).ellipse(
            [cx - R * 1.25, cy - R * 1.25, cx + R * 1.25, cy + R * 1.25], fill=base + (110,))
        img = Image.alpha_composite(img, gl.filter(ImageFilter.GaussianBlur(size * 0.12)))
    d = ImageDraw.Draw(img)
    d.polygon(outer, fill=lerp(base, (0, 0, 0), 0.45) + (255,))
    # facets between outer and inner rings, lit from the top-left
    for k in range(n):
        a, b = outer[k], outer[(k + 1) % n]; ia, ib = inner[k], inner[(k + 1) % n]
        mx, my = (a[0] + b[0]) / 2 - cx, (a[1] + b[1]) / 2 - cy
        light = 0.5 + 0.5 * (-(mx * 0.6 + my * 0.8) / (math.hypot(mx, my) or 1))
        col = lerp(lerp(base, (0, 0, 0), 0.5), lerp(base, (255, 255, 255), 0.35), light)
        d.polygon([a, b, ib, ia], fill=col + (255,))
    d.polygon(inner, fill=lerp(base, (255, 255, 255), 0.18) + (255,))
    # table highlight + sparkle
    hx, hy = cx - R * 0.22, cy - R * 0.28
    d.ellipse([hx - R * 0.16, hy - R * 0.09, hx + R * 0.16, hy + R * 0.09], fill=(255, 255, 255, 170))
    d.polygon(outer, outline=lerp(base, (255, 255, 255), 0.5) + (255,), width=max(2, size // 80))
    return img.resize((size, size), Image.LANCZOS)

def sparkle(img, x, y, r):
    layer = Image.new('RGBA', img.size, (0, 0, 0, 0)); d = ImageDraw.Draw(layer)
    d.polygon([(x - r, y), (x - r * 0.12, y - r * 0.12), (x, y - r), (x + r * 0.12, y - r * 0.12),
               (x + r, y), (x + r * 0.12, y + r * 0.12), (x, y + r), (x - r * 0.12, y + r * 0.12)], fill=(255, 255, 255, 230))
    return Image.alpha_composite(img, layer.filter(ImageFilter.GaussianBlur(r * 0.04)))

def banner(path, w=2736, h=912):
    img = rays(bokeh(nebula(w, h, 7), 60, 3), w * 0.5, h * 0.5, 5).convert('RGBA')
    order = ['purple', 'orange', 'green', 'blue', 'red', 'yellow', 'white']
    for i, k in enumerate(order):
        t = i / (len(order) - 1)
        x = w * (0.12 + 0.76 * t); y = h * (0.52 - 0.10 * math.sin(math.pi * t))
        s = int(h * (0.62 + 0.20 * math.sin(math.pi * t)))
        g = gem(s, k).rotate(random.Random(i).uniform(-12, 12), resample=Image.BICUBIC)
        img.alpha_composite(g, (int(x - s / 2), int(y - s / 2)))
    rnd = random.Random(11)
    for _ in range(14):
        img = sparkle(img, rnd.uniform(0, w), rnd.uniform(0, h), rnd.uniform(0.02, 0.05) * h)
    img.convert('RGB').save(path)

def icon(path, s=1600):
    img = rays(nebula(s, s, 2), s / 2, s / 2, 9).convert('RGBA')
    g = gem(int(s * 1.5), "blue"); img.alpha_composite(g, (int(-s * 0.25), int(-s * 0.22)))
    img = sparkle(img, s * 0.70, s * 0.30, s * 0.09)
    img.convert('RGB').save(path)

out = sys.argv[1]
banner(out + '/banner.png'); icon(out + '/icon.png')
