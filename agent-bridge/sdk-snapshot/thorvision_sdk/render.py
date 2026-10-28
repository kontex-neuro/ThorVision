"""Shared overlay renderer used by the live preview and by replay.

Pure Python over an RGB8 buffer so that the device supervisor, the PC tools
and replay produce identical pixels without third-party imaging libraries.
"""
from __future__ import annotations

import math

from . import font
from .contract import Point, Polyline, Rect, Text, Transform

RENDERER_VERSION = '1'


def _round(value):
    return math.floor(value + 0.5)


class Canvas:
    def __init__(self, buffer, width, height, stride=None):
        self.buffer, self.width, self.height = buffer, width, height
        self.stride = stride or width * 3

    def fill_rect(self, x0, y0, x1, y1, color):
        """Fill the inclusive pixel rectangle, clipped to the canvas."""
        x0, y0 = max(0, x0), max(0, y0)
        x1, y1 = min(self.width - 1, x1), min(self.height - 1, y1)
        if x1 < x0 or y1 < y0:
            return
        run = bytes(color) * (x1 - x0 + 1)
        for y in range(y0, y1 + 1):
            start = y * self.stride + x0 * 3
            self.buffer[start:start + len(run)] = run

    def stamp(self, x, y, thickness, color):
        low = (thickness - 1) // 2
        self.fill_rect(x - low, y - low, x - low + thickness - 1, y - low + thickness - 1, color)

    def line(self, x0, y0, x1, y1, thickness, color):
        dx, dy = abs(x1 - x0), -abs(y1 - y0)
        sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
        error = dx + dy
        # Bound the walk so that far off-canvas endpoints cannot stall rendering.
        for _ in range(dx - dy + 1):
            self.stamp(x0, y0, thickness, color)
            if x0 == x1 and y0 == y1:
                break
            doubled = 2 * error
            if doubled >= dy:
                error += dy
                x0 += sx
            if doubled <= dx:
                error += dx
                y0 += sy

    def circle(self, cx, cy, radius, color):
        for offset in range(-radius, radius + 1):
            half = math.isqrt(radius * radius - offset * offset)
            self.fill_rect(cx - half, cy + offset, cx + half, cy + offset, color)

    def text(self, x, y, text, size, color, background=None):
        width, height = font.text_size(text, size)
        if background is not None:
            self.fill_rect(x - size, y - size, x + width + size - 1, y + height + size - 1, background)
        for index, character in enumerate(text):
            left = x + index * font.ADVANCE * size
            for column, bits in enumerate(font.glyph(character)):
                for row in range(font.HEIGHT):
                    if bits >> row & 1:
                        px, py = left + column * size, y + row * size
                        self.fill_rect(px, py, px + size - 1, py + size - 1, color)


def draw(canvas, drawings, transform=Transform()):
    """Draw validated primitives in order; ``transform`` maps source pixels to the canvas."""
    def at(x, y):
        px, py = transform.from_source(x, y)
        return _round(px), _round(py)

    for item in drawings:
        if isinstance(item, Rect):
            x0, y0 = at(item.box[0], item.box[1])
            x1, y1 = at(item.box[2], item.box[3])
            if item.fill:
                canvas.fill_rect(x0, y0, x1, y1, item.color)
                continue
            t = max(1, min(item.thickness, (x1 - x0 + 1 + 1) // 2, (y1 - y0 + 1 + 1) // 2))
            canvas.fill_rect(x0, y0, x1, y0 + t - 1, item.color)
            canvas.fill_rect(x0, y1 - t + 1, x1, y1, item.color)
            canvas.fill_rect(x0, y0, x0 + t - 1, y1, item.color)
            canvas.fill_rect(x1 - t + 1, y0, x1, y1, item.color)
        elif isinstance(item, Polyline):
            points = [at(*point) for point in item.points]
            if item.closed:
                points.append(points[0])
            for start, end in zip(points, points[1:]):
                canvas.line(*start, *end, item.thickness, item.color)
        elif isinstance(item, Point):
            canvas.circle(*at(item.x, item.y), item.radius, item.color)
        elif isinstance(item, Text):
            canvas.text(*at(item.x, item.y), item.text, item.size, item.color, item.background)
        else:
            raise TypeError(f'unsupported drawing {type(item).__name__}')
