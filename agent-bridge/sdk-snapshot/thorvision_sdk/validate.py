"""Strict validation of callback results before the supervisor accepts them."""
from __future__ import annotations

import json
from dataclasses import dataclass

from .contract import Detection, FrameResult, Point, Polyline, Rect, Text, TrackPoint, is_finite

MAX_SAFE_ID = 2 ** 63 - 1


@dataclass(frozen=True)
class Limits:
    max_bytes: int = 256 * 1024
    max_objects: int = 256
    max_vertices: int = 4096
    max_track_points: int = 4096
    max_label: int = 64
    max_text: int = 256
    max_diagnostics_bytes: int = 4096
    max_thickness: int = 16
    max_radius: int = 64
    max_text_size: int = 8


LIMITS = Limits()
STATUSES = ('ok', 'degraded')


class ResultError(ValueError):
    def __init__(self, problems):
        self.problems = list(problems)
        super().__init__('; '.join(self.problems))


def vertex_count(drawing):
    kind = drawing['type']
    if kind == 'rect':
        return 4
    if kind == 'polyline':
        return len(drawing['points'])
    return 1


class _Checker:
    def __init__(self, width, height, limits):
        self.width, self.height, self.limits = width, height, limits
        self.problems = []

    def fail(self, where, message):
        self.problems.append(f'{where}: {message}')
        raise _Stop

    def number(self, where, value):
        if not is_finite(value):
            self.fail(where, 'must be a finite number')
        return float(value)

    def coordinate(self, where, x, y):
        # Allow drawing partly outside the image, but reject values that cannot be meaningful.
        x, y = self.number(where + '.x', x), self.number(where + '.y', y)
        if not (-self.width <= x <= 2 * self.width and -self.height <= y <= 2 * self.height):
            self.fail(where, f'({x}, {y}) is outside the source image bounds')
        return x, y

    def box(self, where, value):
        if not isinstance(value, (list, tuple)) or len(value) != 4:
            self.fail(where, 'must be [x0, y0, x1, y1]')
        x0, y0 = self.coordinate(where, value[0], value[1])
        x1, y1 = self.coordinate(where, value[2], value[3])
        if x1 < x0 or y1 < y0:
            self.fail(where, 'must have x0 <= x1 and y0 <= y1')
        return (x0, y0, x1, y1)

    def integer(self, where, value, low, high):
        if not isinstance(value, int) or isinstance(value, bool) or not low <= value <= high:
            self.fail(where, f'must be an integer in [{low}, {high}]')
        return value

    def decimal(self, where, value):
        if not isinstance(value, str) or not value.isdigit() or int(value) > MAX_SAFE_ID:
            self.fail(where, 'must be a non-negative 64-bit decimal string')
        return int(value)

    def color(self, where, value):
        if not isinstance(value, (list, tuple)) or len(value) != 3:
            self.fail(where, 'must be [r, g, b]')
        return tuple(self.integer(where, channel, 0, 255) for channel in value)

    def string(self, where, value, limit):
        if not isinstance(value, str) or len(value) > limit:
            self.fail(where, f'must be a string of at most {limit} characters')
        return value

    def flag(self, where, value):
        if not isinstance(value, bool):
            self.fail(where, 'must be a boolean')
        return value


class _Stop(Exception):
    pass


def _drawing(check, where, data):
    limits = check.limits
    kind = data.get('type') if isinstance(data, dict) else None
    if kind == 'rect':
        return Rect(check.box(where + '.box', data.get('box')), check.color(where + '.color', data.get('color')),
            check.integer(where + '.thickness', data.get('thickness'), 1, limits.max_thickness),
            check.flag(where + '.fill', data.get('fill', False)))
    if kind == 'polyline':
        points = data.get('points')
        if not isinstance(points, list) or len(points) < 2:
            check.fail(where + '.points', 'must contain at least two points')
        parsed = []
        for index, point in enumerate(points):
            if not isinstance(point, (list, tuple)) or len(point) != 2:
                check.fail(f'{where}.points[{index}]', 'must be [x, y]')
            parsed.append(check.coordinate(f'{where}.points[{index}]', *point))
        return Polyline(parsed, check.color(where + '.color', data.get('color')),
            check.integer(where + '.thickness', data.get('thickness'), 1, limits.max_thickness),
            check.flag(where + '.closed', data.get('closed', False)))
    if kind == 'point':
        x, y = check.coordinate(where, data.get('x'), data.get('y'))
        return Point(x, y, check.color(where + '.color', data.get('color')),
            check.integer(where + '.radius', data.get('radius'), 0, limits.max_radius))
    if kind == 'text':
        x, y = check.coordinate(where, data.get('x'), data.get('y'))
        background = data.get('background')
        return Text(x, y, check.string(where + '.text', data.get('text'), limits.max_text),
            check.color(where + '.color', data.get('color')),
            check.integer(where + '.size', data.get('size'), 1, limits.max_text_size),
            None if background is None else check.color(where + '.background', background))
    check.fail(where + '.type', 'must be rect, polyline, point or text')


def _parse(check, data):
    limits = check.limits
    if not isinstance(data, dict):
        check.fail('result', 'must be an object')
    unknown = set(data) - {'detections', 'track_points', 'drawings', 'status', 'diagnostics'}
    if unknown:
        check.fail('result', f'unknown fields {sorted(unknown)}')
    detections, track_points, drawings = (data.get(name, []) for name in ('detections', 'track_points', 'drawings'))
    for name, items in (('detections', detections), ('track_points', track_points), ('drawings', drawings)):
        if not isinstance(items, list):
            check.fail(name, 'must be a list')
    if len(detections) > limits.max_objects:
        check.fail('detections', f'{len(detections)} objects exceed the limit of {limits.max_objects}')
    if len(track_points) > limits.max_track_points:
        check.fail('track_points', f'{len(track_points)} points exceed the limit of {limits.max_track_points}')
    vertices = sum(vertex_count(item) for item in drawings if isinstance(item, dict) and item.get('type') in
        ('rect', 'polyline', 'point', 'text') and isinstance(item.get('points', []), list))
    if vertices > limits.max_vertices:
        check.fail('drawings', f'{vertices} vertices exceed the limit of {limits.max_vertices}')
    result = FrameResult(status=data.get('status', 'ok'))
    if result.status not in STATUSES:
        check.fail('status', f'must be one of {STATUSES}')
    for index, item in enumerate(detections):
        where = f'detections[{index}]'
        if not isinstance(item, dict):
            check.fail(where, 'must be an object')
        confidence = check.number(where + '.confidence', item.get('confidence'))
        if not 0.0 <= confidence <= 1.0:
            check.fail(where + '.confidence', 'must be within [0, 1]')
        track = item.get('track_id')
        result.detections.append(Detection(check.string(where + '.label', item.get('label'), limits.max_label),
            confidence, check.box(where + '.box', item.get('box')),
            None if track is None else check.decimal(where + '.track_id', track)))
    for index, item in enumerate(track_points):
        where = f'track_points[{index}]'
        if not isinstance(item, dict):
            check.fail(where, 'must be an object')
        x, y = check.coordinate(where, item.get('x'), item.get('y'))
        result.track_points.append(TrackPoint(check.decimal(where + '.track_id', item.get('track_id')),
            check.decimal(where + '.time_ns', item.get('time_ns')), x, y))
    for index, item in enumerate(drawings):
        result.drawings.append(_drawing(check, f'drawings[{index}]', item))
    diagnostics = data.get('diagnostics', {})
    if not isinstance(diagnostics, dict):
        check.fail('diagnostics', 'must be an object')
    if len(json.dumps(diagnostics)) > limits.max_diagnostics_bytes:
        check.fail('diagnostics', f'exceeds {limits.max_diagnostics_bytes} bytes')
    result.diagnostics = diagnostics
    return result


def validate_result(result, source_width, source_height, limits=LIMITS):
    """Return a canonical ``FrameResult`` or raise ``ResultError`` listing the first problem found."""
    try:
        data = result.to_json() if isinstance(result, FrameResult) else result
        encoded = json.dumps(data, allow_nan=False, separators=(',', ':'))
    except (TypeError, ValueError, AttributeError) as error:
        raise ResultError([f'result: not serialisable as finite JSON ({error})']) from None
    if len(encoded.encode()) > limits.max_bytes:
        raise ResultError([f'result: {len(encoded.encode())} bytes exceed the limit of {limits.max_bytes}'])
    check = _Checker(source_width, source_height, limits)
    try:
        return _parse(check, json.loads(encoded))
    except _Stop:
        raise ResultError(check.problems) from None
