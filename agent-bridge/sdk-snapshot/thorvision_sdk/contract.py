"""Callback contract: frames delivered to a processor and the results it returns.

Drawing and detection coordinates are original source-image pixels with a
top-left origin. ``Frame.transform`` converts processing-image pixels into that
space. 64-bit counters and timestamps are encoded as decimal strings in JSON.
"""
from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import Optional, Union

Color = tuple[int, int, int]
Box = tuple[float, float, float, float]


@dataclass(frozen=True)
class FrameRef:
    """Authoritative frame identity: camera, capture epoch and per-epoch sequence."""
    camera: str
    epoch: str
    sequence: int

    def key(self):
        return (self.camera, self.epoch, self.sequence)

    def to_json(self):
        return {'camera': self.camera, 'epoch': self.epoch, 'sequence': str(self.sequence)}

    @classmethod
    def from_json(cls, data):
        sequence = data['sequence']
        if not isinstance(sequence, str) or not sequence.isdigit():
            raise ValueError('frame sequence must be a decimal string')
        return cls(str(data['camera']), str(data['epoch']), int(sequence))


@dataclass(frozen=True)
class Transform:
    """Processing-image geometry relative to the source: ``p = source * scale + pad``."""
    scale_x: float = 1.0
    scale_y: float = 1.0
    pad_x: float = 0.0
    pad_y: float = 0.0

    def to_source(self, x, y):
        return ((x - self.pad_x) / self.scale_x, (y - self.pad_y) / self.scale_y)

    def from_source(self, x, y):
        return (x * self.scale_x + self.pad_x, y * self.scale_y + self.pad_y)

    def box_to_source(self, box):
        x0, y0 = self.to_source(box[0], box[1])
        x1, y1 = self.to_source(box[2], box[3])
        return (x0, y0, x1, y1)

    def to_json(self):
        return {'scale_x': self.scale_x, 'scale_y': self.scale_y, 'pad_x': self.pad_x, 'pad_y': self.pad_y}


def fit_within(source_width, source_height, max_width=640, max_height=480):
    """Processing size that fits within the bounds, preserving aspect ratio, never upscaling."""
    scale = min(1.0, max_width / source_width, max_height / source_height)
    width = max(1, round(source_width * scale))
    height = max(1, round(source_height * scale))
    return width, height, Transform(width / source_width, height / source_height)


def letterbox(source_width, source_height, out_width, out_height):
    """Transform for an aspect-preserving fit centred in a fixed output canvas."""
    scale = min(out_width / source_width, out_height / source_height)
    width, height = round(source_width * scale), round(source_height * scale)
    return Transform(width / source_width, height / source_height,
        (out_width - width) // 2, (out_height - height) // 2)


@dataclass(frozen=True)
class Frame:
    """A read-only RGB8 processing image and its capture identity."""
    image: memoryview
    width: int
    height: int
    stride: int
    ref: FrameRef
    source_width: int
    source_height: int
    transform: Transform
    capture_time_ns: int
    elapsed_ns: Optional[int]

    def __post_init__(self):
        if not self.image.readonly:
            raise ValueError('frame image must be read-only')
        if self.stride < self.width * 3 or len(self.image) < self.stride * (self.height - 1) + self.width * 3:
            raise ValueError('frame image is smaller than its declared geometry')

    @classmethod
    def from_rgb(cls, data, width, height, ref, *, source_width=None, source_height=None,
            transform=Transform(), capture_time_ns=0, elapsed_ns=None, stride=None):
        return cls(memoryview(bytes(data)).toreadonly(), width, height, stride or width * 3, ref,
            source_width or width, source_height or height, transform, capture_time_ns, elapsed_ns)

    def pixel(self, x, y):
        offset = y * self.stride + x * 3
        return tuple(self.image[offset:offset + 3])


def _color(value):
    return [int(channel) for channel in value]


@dataclass
class Rect:
    box: Box
    color: Color = (0, 255, 0)
    thickness: int = 2
    fill: bool = False

    def to_json(self):
        return {'type': 'rect', 'box': list(self.box), 'color': _color(self.color),
            'thickness': self.thickness, 'fill': self.fill}


@dataclass
class Polyline:
    points: list
    color: Color = (0, 255, 0)
    thickness: int = 2
    closed: bool = False

    def to_json(self):
        return {'type': 'polyline', 'points': [list(point) for point in self.points],
            'color': _color(self.color), 'thickness': self.thickness, 'closed': self.closed}


@dataclass
class Point:
    x: float
    y: float
    color: Color = (0, 255, 0)
    radius: int = 3

    def to_json(self):
        return {'type': 'point', 'x': self.x, 'y': self.y, 'color': _color(self.color), 'radius': self.radius}


@dataclass
class Text:
    """Text anchored at its top-left corner; ``size`` is an integer glyph scale."""
    x: float
    y: float
    text: str
    color: Color = (255, 255, 255)
    size: int = 2
    background: Optional[Color] = None

    def to_json(self):
        return {'type': 'text', 'x': self.x, 'y': self.y, 'text': self.text, 'color': _color(self.color),
            'size': self.size, 'background': None if self.background is None else _color(self.background)}


Drawing = Union[Rect, Polyline, Point, Text]


@dataclass
class Detection:
    label: str
    confidence: float
    box: Box
    track_id: Optional[int] = None

    def to_json(self):
        return {'label': self.label, 'confidence': self.confidence, 'box': list(self.box),
            'track_id': None if self.track_id is None else str(self.track_id)}


@dataclass
class TrackPoint:
    track_id: int
    time_ns: int
    x: float
    y: float

    def to_json(self):
        return {'track_id': str(self.track_id), 'time_ns': str(self.time_ns), 'x': self.x, 'y': self.y}


@dataclass
class FrameResult:
    """What ``Processor.process`` returns. ``status`` is ``ok`` or ``degraded``."""
    detections: list = field(default_factory=list)
    track_points: list = field(default_factory=list)
    drawings: list = field(default_factory=list)
    status: str = 'ok'
    diagnostics: dict = field(default_factory=dict)

    def to_json(self):
        return {'detections': [item.to_json() for item in self.detections],
            'track_points': [item.to_json() for item in self.track_points],
            'drawings': [item.to_json() for item in self.drawings],
            'status': self.status, 'diagnostics': dict(self.diagnostics)}


def is_finite(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)
