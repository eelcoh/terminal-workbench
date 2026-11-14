#!/usr/bin/env python3
"""Band's app icon, drawn from one list of shapes.

The icon is the strip: panes side by side that run on past both edges, the
focused one in the brand color with a prompt in it.

    make-icon.py            write native/linux/band.svg and, on macOS,
                            native/macos/Band.icns (through iconutil)
    make-icon.py --check    fail when native/linux/band.svg is out of date
    make-icon.py --png N F  write an N x N preview PNG to F

Only the standard library: the PNGs are rasterized here, each size on its own
pixel grid, so the build needs no image tool on either platform.
"""
from pathlib import Path
import math
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
SVG = ROOT / 'native/linux/band.svg'
ICNS = ROOT / 'native/macos/Band.icns'

# The tile is 112 units square. Colors: the window, a resting pane, the brand.
GROUND, PANE, BRAND = '#17171d', '#3a3a46', '#e9b66a'
TILE, CORNER = 112, 25
# Panes: x, width, color. The outer two run off the tile.
PANES = [(-14, 30, PANE), (24, 46, BRAND), (78, 24, PANE), (110, 30, PANE)]
PANE_TOP, PANE_HEIGHT, PANE_CORNER = 31, 50, 6
# The prompt in the focused pane: line segments in the ground color.
STROKE = 4.5
PROMPT = [((34, 47), (42, 56)), ((42, 56), (34, 65)), ((48, 65), (60, 65))]


def svg():
    lines = [
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {TILE} {TILE}">',
        '  <title>Band</title>',
        f'  <clipPath id="tile"><rect width="{TILE}" height="{TILE}" rx="{CORNER}"/></clipPath>',
        '  <g clip-path="url(#tile)">',
        f'    <rect width="{TILE}" height="{TILE}" fill="{GROUND}"/>',
    ]
    for x, width, color in PANES:
        lines.append(f'    <rect x="{x}" y="{PANE_TOP}" width="{width}" height="{PANE_HEIGHT}" rx="{PANE_CORNER}" fill="{color}"/>')
    path = f'M{PROMPT[0][0][0]} {PROMPT[0][0][1]}L{PROMPT[0][1][0]} {PROMPT[0][1][1]}L{PROMPT[1][1][0]} {PROMPT[1][1][1]}'
    path += f'M{PROMPT[2][0][0]} {PROMPT[2][0][1]}L{PROMPT[2][1][0]} {PROMPT[2][1][1]}'
    lines.append(f'    <path d="{path}" fill="none" stroke="{GROUND}" stroke-width="{STROKE}" stroke-linecap="round" stroke-linejoin="round"/>')
    lines += ['  </g>', '</svg>', '']
    return '\n'.join(lines)


def rgb(color):
    return tuple(int(color[i:i + 2], 16) for i in (1, 3, 5))


def box_distance(px, py, x, y, width, height, corner):
    """Signed distance from a point to a rounded rectangle, negative inside."""
    qx = abs(px - (x + width / 2)) - (width / 2 - corner)
    qy = abs(py - (y + height / 2)) - (height / 2 - corner)
    return math.hypot(max(qx, 0), max(qy, 0)) + min(max(qx, qy), 0) - corner


def segment_distance(px, py, a, b):
    ax, ay, bx, by = a[0], a[1], b[0], b[1]
    dx, dy = bx - ax, by - ay
    t = max(0, min(1, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)))
    return math.hypot(px - ax - t * dx, py - ay - t * dy)


def raster(size, inset):
    """RGBA rows of the icon at `size` pixels; `inset` is the transparent margin
    around the tile as a part of the size (macOS icons leave room, about 0.1)."""
    scale = size * (1 - 2 * inset) / TILE      # pixels per unit
    origin = size * inset
    ground, rows = rgb(GROUND), []
    panes = [(x, width, rgb(color)) for x, width, color in PANES]
    for row in range(size):
        y = (row + 0.5 - origin) / scale
        line = bytearray()
        for column in range(size):
            x = (column + 0.5 - origin) / scale
            # Coverage from a signed distance: half a pixel either side of the edge.
            tile = max(0, min(1, 0.5 - box_distance(x, y, 0, 0, TILE, TILE, CORNER) * scale))
            if tile <= 0:
                line += b'\0\0\0\0'
                continue
            color = ground
            if PANE_TOP - 1 <= y <= PANE_TOP + PANE_HEIGHT + 1:
                for left, width, fill in panes:
                    part = max(0, min(1, 0.5 - box_distance(x, y, left, PANE_TOP, width, PANE_HEIGHT, PANE_CORNER) * scale))
                    if part > 0:
                        color = tuple(c + (f - c) * part for c, f in zip(color, fill))
                near = min(segment_distance(x, y, a, b) for a, b in PROMPT) - STROKE / 2
                part = max(0, min(1, 0.5 - near * scale))
                if part > 0:
                    color = tuple(c + (g - c) * part for c, g in zip(color, ground))
            line += bytes((round(color[0]), round(color[1]), round(color[2]), round(255 * tile)))
        rows.append(bytes(line))
    return rows


def png(size, inset=0.0):
    def chunk(kind, data):
        body = kind + data
        return struct.pack('>I', len(data)) + body + struct.pack('>I', zlib.crc32(body))
    pixels = b''.join(b'\0' + row for row in raster(size, inset))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(pixels, 9)) + chunk(b'IEND', b''))


def icns():
    """macOS: the tile on Apple's icon grid (824 of 1024), every size iconutil wants."""
    with tempfile.TemporaryDirectory(prefix='band-icon-') as directory:
        iconset = Path(directory) / 'Band.iconset'
        iconset.mkdir()
        images = {}
        for points in (16, 32, 128, 256, 512):
            for factor in (1, 2):
                size = points * factor
                if size not in images:
                    images[size] = png(size, inset=100 / 1024)
                (iconset / f'icon_{points}x{points}{"@2x" if factor == 2 else ""}.png').write_bytes(images[size])
        subprocess.run(['/usr/bin/iconutil', '--convert', 'icns', '--output', str(ICNS), str(iconset)], check=True)


def main():
    arguments = sys.argv[1:]
    if arguments == ['--check']:
        if not SVG.exists() or SVG.read_text() != svg():
            sys.exit(f'{SVG.relative_to(ROOT)} is out of date; run scripts/make-icon.py')
        print('PASS: the Linux icon matches scripts/make-icon.py')
        return
    if len(arguments) == 3 and arguments[0] == '--png':
        Path(arguments[2]).write_bytes(png(int(arguments[1]), inset=100 / 1024))
        return
    if arguments:
        sys.exit(__doc__)
    SVG.write_text(svg())
    print(f'wrote {SVG.relative_to(ROOT)}')
    if sys.platform == 'darwin' and shutil.which('iconutil'):
        icns()
        print(f'wrote {ICNS.relative_to(ROOT)}')
    else:
        print(f'{ICNS.relative_to(ROOT)} is made on macOS only (iconutil); left as it is')


if __name__ == '__main__':
    main()
