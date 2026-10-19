#!/usr/bin/env python3
"""A value as a picture: one text (a number, a short word) as large as it fits, on a transparent ground.

Prototype for showing a value larger than Tessera's largest font: Home Assistant asks for the picture through a
template image (`url`), and a Tessera tile shows that image. The ground is transparent, so the tile's own card colour
shows through and follows light and dark; only the text has a fixed colour.

    GET /value.png?text=5.358&w=490&h=258              the text, centred, as large as it fits
        &color=accent | ink | ffffff                   its colour (default: accent, Tessera's blue)
        &bottom=0.18                                   share of the height kept free at the bottom (a caption)
        &pad=0.06                                      share of the shorter side kept free all round
        &size=180                                      largest font size in pixels, so values side by side match
    GET /value.png?text=5.358&ink=300                  tight: the text alone, its ink 300 pixels high, as wide as it is
                                                       (for a card that sizes the picture itself: Custom card's image)
    GET /health                                        "ok"

The address alone decides the picture, so the same address is the same picture, and a new value is a new address.
Nothing is stored and no data source is read.

    python3 renderer.py --port 8765            serve
    python3 renderer.py --preview out/         write sample pictures on Tessera's light and dark card colours
"""

import argparse
import io
import os
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlsplit

from PIL import Image, ImageDraw, ImageFont

FONTS = ('/usr/share/fonts/truetype/roboto/unhinted/RobotoTTF/Roboto-Bold.ttf',   # Debian, Ubuntu: fonts-roboto
         os.path.join(os.path.dirname(os.path.abspath(__file__)), 'Roboto-Bold.ttf'))
FONT = next((f for f in FONTS if os.path.exists(f)), FONTS[-1])   # Roboto Bold, the face of Tessera's digits
COLORS = {'accent': '0A93D2', 'ink': 'DADADA'}      # Tessera's theme.h, dark values
CARDS = {'dark': '1A1A1A', 'light': 'FFFFFF'}       # theme.h CARD, for the preview only
MAX_SIDE, MAX_TEXT = 2048, 32


def rgb(value):
    value = COLORS.get(value, value).lstrip('#')
    if len(value) != 6:
        raise ValueError('colour')
    return tuple(int(value[i:i + 2], 16) for i in (0, 2, 4))


def render(text, w, h, color='accent', bottom=0.18, pad=0.06, size=0):
    """The PNG bytes of `text` as large as it fits in w x h, minus the paddings."""
    if not 1 <= w <= MAX_SIDE or not 1 <= h <= MAX_SIDE or not 0 < len(text) <= MAX_TEXT:
        raise ValueError('size')
    margin = round(min(w, h) * pad)
    room_w, room_h = w - 2 * margin, round(h * (1 - bottom)) - 2 * margin
    lo, hi = 4, min(max(room_h * 2, 8), size) if size > 0 else max(room_h * 2, 8)
    while lo < hi:   # the largest size whose ink fits the room
        mid = (lo + hi + 1) // 2
        left, top, right, low = ImageFont.truetype(FONT, mid).getbbox(text)
        if right - left <= room_w and low - top <= room_h:
            lo = mid
        else:
            hi = mid - 1
    font = ImageFont.truetype(FONT, lo)
    left, top, right, low = font.getbbox(text)
    x = margin + (room_w - (right - left)) // 2 - left
    y = margin + (room_h - (low - top)) // 2 - top
    image = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    ImageDraw.Draw(image).text((x, y), text, font=font, fill=rgb(color) + (255,))
    out = io.BytesIO()
    image.save(out, 'PNG', optimize=True)
    return out.getvalue()


def render_tight(text, ink, color='accent'):
    """The PNG bytes of `text` with its ink `ink` pixels high and no margin: the card decides the size."""
    if not 8 <= ink <= MAX_SIDE or not 0 < len(text) <= MAX_TEXT:
        raise ValueError('size')
    lo, hi = 4, ink * 3
    while lo < hi:   # the size whose ink is `ink` high
        mid = (lo + hi + 1) // 2
        left, top, right, low = ImageFont.truetype(FONT, mid).getbbox(text)
        if low - top <= ink:
            lo = mid
        else:
            hi = mid - 1
    font = ImageFont.truetype(FONT, lo)
    left, top, right, low = font.getbbox(text)
    if right - left > MAX_SIDE:
        raise ValueError('size')
    image = Image.new('RGBA', (right - left, low - top), (0, 0, 0, 0))
    ImageDraw.Draw(image).text((-left, -top), text, font=font, fill=rgb(color) + (255,))
    out = io.BytesIO()
    image.save(out, 'PNG', optimize=True)
    return out.getvalue()


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        url = urlsplit(self.path)
        if url.path == '/health':
            return self.answer(200, b'ok', 'text/plain')
        if url.path != '/value.png':
            return self.answer(404, b'not found', 'text/plain')
        q = {k: v[-1] for k, v in parse_qs(url.query).items()}
        try:
            if 'ink' in q:
                return self.answer(200, render_tight(q.get('text', ''), int(q['ink']), q.get('color', 'accent')), 'image/png')
            png = render(q.get('text', ''), int(q.get('w', 490)), int(q.get('h', 258)), q.get('color', 'accent'),
                         float(q.get('bottom', 0.18)), float(q.get('pad', 0.06)), int(q.get('size', 0)))
        except (ValueError, OSError) as error:
            return self.answer(400, str(error).encode(), 'text/plain')
        self.answer(200, png, 'image/png')

    def answer(self, code, body, kind):
        self.send_response(code)
        self.send_header('Content-Type', kind)
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'no-store')
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, fmt, *args):   # one short line per request, without the client's address
        print(f'{self.command} {urlsplit(self.path).path} {args[1] if len(args) > 1 else ""}', flush=True)


def preview(folder):
    """Sample pictures as a 2 x 2 tile of the 7-inch Guition shows them, on the light and the dark card colour."""
    os.makedirs(folder, exist_ok=True)
    for text in ('5.358', '5.933', '90 %', '12'):
        layer = Image.open(io.BytesIO(render(text, 490, 258)))
        for look, card in CARDS.items():
            ground = Image.new('RGBA', layer.size, rgb(card) + (255,))
            name = text.replace(' ', '').replace('%', 'pct').replace('.', '_')
            Image.alpha_composite(ground, layer).convert('RGB').save(os.path.join(folder, f'{name}-{look}.png'))
    print('written to', folder)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--bind', default='0.0.0.0')
    parser.add_argument('--port', type=int, default=8765)
    parser.add_argument('--preview', metavar='FOLDER')
    parser.add_argument('--font', help='a TTF to draw with (default: Roboto Bold)')
    args = parser.parse_args()
    if args.font:
        FONT = args.font
    if args.preview:
        preview(args.preview)
    else:
        print(f'serving on {args.bind}:{args.port}', flush=True)
        ThreadingHTTPServer((args.bind, args.port), Handler).serve_forever()
