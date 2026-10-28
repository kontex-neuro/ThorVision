"""Deterministic fixtures: moving numbered boxes with known frame references.

Each box has a unique pure colour, so a processor can recover its exact
bounding box from the (nearest-neighbour) resized processing image. Box 3
appears late and leaves early to exercise track creation and termination.
"""
from __future__ import annotations

import os
import subprocess

from . import bundle
from .contract import Frame, FrameRef, FrameResult, Detection, Polyline, Rect, Text, TrackPoint, fit_within
from .render import RENDERER_VERSION, Canvas
from .tracking import IoUTracker
from .validate import validate_result

CAMERA = 'fixture-camera'
EPOCH = '00000000-0000-4000-8000-000000000001'
FIRST_SEQUENCE = 1000
BACKGROUND = (32, 32, 32)
PALETTE = ((255, 0, 0), (0, 0, 255), (255, 255, 0))
FRAME_NS = 1_000_000_000 // 15


class SyntheticSource:
    def __init__(self, width=320, height=240, camera=CAMERA, epoch=EPOCH, first_sequence=FIRST_SEQUENCE):
        self.width, self.height = width, height
        self.camera, self.epoch, self.first_sequence = camera, epoch, first_sequence
        self.box_width, self.box_height = width // 8, height // 8

    def ref(self, index):
        return FrameRef(self.camera, self.epoch, self.first_sequence + index)

    def truth(self, index):
        """``[(box_number, (x0, y0, x1, y1))]`` in source pixels, inclusive corners."""
        boxes = []
        for number in range(1, len(PALETTE) + 1):
            if number == 3 and not 5 <= index < 20:
                continue
            span_x, span_y = self.width - self.box_width, self.height - self.box_height
            # At most a few pixels per frame, so IoU association survives 1-in-3 sampling.
            x = (number * 37 + index * number) % (2 * span_x)
            y = (number * 53 + index * number // 2) % (2 * span_y)
            x, y = (x if x <= span_x else 2 * span_x - x), (y if y <= span_y else 2 * span_y - y)
            boxes.append((number, (x, y, x + self.box_width - 1, y + self.box_height - 1)))
        return boxes

    def image(self, index):
        data = bytearray(bytes(BACKGROUND) * (self.width * self.height))
        canvas = Canvas(data, self.width, self.height)
        for number, (x0, y0, x1, y1) in self.truth(index):
            canvas.fill_rect(x0, y0, x1, y1, PALETTE[number - 1])
            canvas.text(x0 + 2, y0 + 2, str(number), 1, (0, 0, 0))
        return data

    def frame(self, index, max_width=640, max_height=480):
        width, height, transform = fit_within(self.width, self.height, max_width, max_height)
        return Frame.from_rgb(resize_nearest(self.image(index), self.width, self.height, width, height),
            width, height, self.ref(index), source_width=self.width, source_height=self.height,
            transform=transform, capture_time_ns=index * FRAME_NS, elapsed_ns=None if index == 0 else FRAME_NS)


def resize_nearest(data, width, height, out_width, out_height):
    if (width, height) == (out_width, out_height):
        return bytes(data)
    columns = [min(width - 1, (x * width) // out_width) * 3 for x in range(out_width)]
    rows = []
    for y in range(out_height):
        start = min(height - 1, (y * height) // out_height) * width * 3
        row = data[start:start + width * 3]
        rows.append(b''.join(row[column:column + 3] for column in columns))
    return b''.join(rows)


def find_colour(frame, colour):
    """Bounding box of pixels exactly matching ``colour`` in processing pixels, or ``None``."""
    pattern = bytes(colour)
    left = top = right = bottom = None
    for y in range(frame.height):
        row = bytes(frame.image[y * frame.stride:y * frame.stride + frame.width * 3])
        position = row.find(pattern)
        while position != -1 and position % 3:
            position = row.find(pattern, position + 1)
        if position == -1:
            continue
        last = row.rfind(pattern)
        while last % 3:
            last = row.rfind(pattern, 0, last + 2)
        x0, x1 = position // 3, last // 3
        left = x0 if left is None else min(left, x0)
        right = x1 if right is None else max(right, x1)
        top = y if top is None else top
        bottom = y
    return None if left is None else (left, top, right + 1, bottom + 1)


class ColourBoxProcessor:
    """Reference callback: detect the palette boxes, track them and draw boxes, IDs and trails."""

    def setup(self, context, config):
        self.tracker = IoUTracker(trail_ns=int(config.get('trail_seconds', 2.0) * 1e9))
        self.colour = tuple(config.get('box_colour', (0, 255, 0)))

    def process(self, frame):
        detections, boxes = [], []
        for colour in PALETTE:
            found = find_colour(frame, colour)
            if found is not None:
                boxes.append(frame.transform.box_to_source(found))
        result = FrameResult()
        for track_id, box in self.tracker.update(boxes, frame.capture_time_ns):
            detections.append(Detection('box', 1.0, box, track_id))
            result.drawings.append(Rect(box, self.colour, 2))
            result.drawings.append(Text(box[0], max(0, box[1] - 12), f'#{track_id}', self.colour, 1, (0, 0, 0)))
            trail = self.tracker.trail(track_id)
            result.track_points.append(TrackPoint(track_id, frame.capture_time_ns, *trail[-1][1:]))
            if len(trail) > 1:
                result.drawings.append(Polyline([point[1:] for point in trail], self.colour, 1))
        result.detections = detections
        result.drawings.append(Text(4, 4, f'frame {frame.ref.sequence}', (255, 255, 255), 1, (0, 0, 0)))
        return result

    def close(self):
        pass


def encode_lossless(path, frames, width, height, fps, ffmpeg='ffmpeg'):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    command = [ffmpeg, '-v', 'error', '-y', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-s', f'{width}x{height}',
        '-r', str(fps), '-i', '-', '-c:v', 'ffv1', '-pix_fmt', 'bgr0', path]
    with subprocess.Popen(command, stdin=subprocess.PIPE) as process:
        for data in frames:
            process.stdin.write(data)
        process.stdin.close()
        if process.wait() != 0:
            raise RuntimeError(f'ffmpeg failed to encode {path}')


def write_bundle(directory, count=30, fps=15, sample_every=3, width=320, height=240, fail_at=None,
        gap=None, processor=None, ffmpeg='ffmpeg'):
    """Write a lossless fixture bundle. ``fail_at`` is a frame index; ``gap`` an inclusive index range."""
    source = SyntheticSource(width, height)
    segment = 'seg0001'
    video, index, analytics = f'video/{segment}.mkv', f'index/{segment}.jsonl', f'analytics/{segment}/run-1.jsonl'
    encode_lossless(os.path.join(directory, video), (source.image(i) for i in range(count)), width, height, fps, ffmpeg)
    with bundle.JsonlWriter(os.path.join(directory, index)) as writer:
        for i in range(count):
            writer.write({'schema': bundle.INDEX_SCHEMA, 'frame': source.ref(i).to_json(), 'media_index': i,
                'pts_ns': str(i * 1_000_000_000 // fps)})
    processor = processor or ColourBoxProcessor()
    processor.setup({}, {})
    producer = {'package': 'fixture', 'digest': 'fixture', 'sdk': '0.1.0'}
    with bundle.JsonlWriter(os.path.join(directory, analytics)) as writer:
        if gap is not None:
            writer.write(bundle.gap_record(source.camera, source.epoch, source.ref(gap[0]).sequence,
                source.ref(gap[1]).sequence, 'run-1', 'fixture gap'))
        result_seq = 0
        for i in range(0, count, sample_every):
            if gap is not None and gap[0] <= i <= gap[1]:
                continue
            frame = source.frame(i)
            result_seq += 1
            if i == fail_at:
                writer.write(bundle.result_record(frame.ref, 'run-1', result_seq, producer, error='fixture fault'))
                continue
            result = validate_result(processor.process(frame), width, height)
            writer.write(bundle.result_record(frame.ref, 'run-1', result_seq, producer, result,
                renderer_version=RENDERER_VERSION))
    processor.close()
    bundle.write_manifest(directory, {'schema': bundle.BUNDLE_SCHEMA, 'renderer_version': RENDERER_VERSION,
        'analytics': {'target_fps': fps / sample_every, 'processing_max': [640, 480]},
        'package': producer, 'segments': [{'name': segment, 'video': video, 'index': index,
            'analytics': [analytics]}],
        'coverage': {'complete': gap is None and fail_at is None}})
    return source


def main(argv=None):
    import argparse
    parser = argparse.ArgumentParser(prog='python -m thorvision_sdk.fixtures', description='Write a lossless sample bundle.')
    parser.add_argument('--output', required=True)
    parser.add_argument('--frames', type=int, default=30)
    parser.add_argument('--ffmpeg', default='ffmpeg')
    args = parser.parse_args(argv)
    write_bundle(args.output, count=args.frames, ffmpeg=args.ffmpeg)
    print(args.output)


if __name__ == '__main__':
    main()
