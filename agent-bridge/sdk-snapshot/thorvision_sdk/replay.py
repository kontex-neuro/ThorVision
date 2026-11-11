"""Redraw saved analytics onto original video without rerunning the callback.

    python -m thorvision_sdk.replay --bundle <directory> --output <video.mp4>

The default ``processed`` mode emits only sampled frames, at the recorded
analytics cadence. ``original`` mode emits every frame and annotates only
those with a matching result; unsampled frames stay unannotated. Frames with
failed processing or inside a recorded gap are marked, and incomplete coverage
is reported on stderr and in ``<output>.report.json``.
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys

from . import bundle as bundles
from .render import RENDERER_VERSION, Canvas, draw
from .validate import validate_result

MARK = {bundles.FAILED: 'ANALYTICS FAILED', bundles.GAP: 'ANALYTICS GAP'}


def probe(path, ffprobe):
    output = subprocess.run([ffprobe, '-v', 'error', '-select_streams', 'v:0', '-show_entries',
        'stream=width,height,avg_frame_rate', '-of', 'json', path], check=True, capture_output=True, text=True).stdout
    stream = json.loads(output)['streams'][0]
    numerator, _, denominator = stream['avg_frame_rate'].partition('/')
    return stream['width'], stream['height'], float(numerator) / float(denominator or 1)


def decode(path, width, height, ffmpeg):
    """Yield RGB frames in presentation order; the ordinal is the bundle's ``media_index``."""
    size = width * height * 3
    process = subprocess.Popen([ffmpeg, '-v', 'error', '-i', path, '-f', 'rawvideo', '-pix_fmt', 'rgb24',
        '-vsync', 'passthrough', '-'], stdout=subprocess.PIPE)
    try:
        while True:
            data = process.stdout.read(size)
            if len(data) < size:
                break
            yield bytearray(data)
    finally:
        process.stdout.close()
        if process.wait() != 0:
            raise RuntimeError(f'ffmpeg failed to decode {path}')


def write_ppm(path, data, width, height):
    with open(path, 'wb') as file:
        file.write(f'P6\n{width} {height}\n255\n'.encode() + bytes(data))


def annotate(data, width, height, record, status):
    canvas = Canvas(data, width, height)
    if record is not None and 'result' in record:
        draw(canvas, validate_result(record['result'], width, height).drawings)
    if status in MARK:
        canvas.text(4, height - 20, MARK[status], 2, (255, 255, 255), (200, 0, 0))


def replay(bundle_dir, output=None, mode='processed', fps=None, frames_dir=None, codec='libx264',
        ffmpeg='ffmpeg', ffprobe='ffprobe'):
    bundle = bundles.Bundle.load(bundle_dir)
    renderer = bundle.manifest.get('renderer_version')
    if renderer != RENDERER_VERSION:
        raise bundles.BundleError(f'bundle uses renderer {renderer!r}; this SDK provides {RENDERER_VERSION!r}')
    counts = dict.fromkeys(bundles.COVERAGE, 0)
    problems = list(bundle.problems)
    encoder = written = None
    if frames_dir:
        os.makedirs(frames_dir, exist_ok=True)
    try:
        for segment in bundle.segments:
            width, height, video_fps = probe(segment.video, ffprobe)
            index = dict(segment.index)
            rate = fps or (bundle.manifest.get('analytics', {}).get('target_fps') if mode == 'processed' else None) or video_fps
            if output and encoder is None:
                encoder = subprocess.Popen([ffmpeg, '-v', 'error', '-y', '-f', 'rawvideo', '-pix_fmt', 'rgb24',
                    '-s', f'{width}x{height}', '-r', str(rate), '-i', '-', '-c:v', codec, '-pix_fmt', 'yuv420p',
                    output], stdin=subprocess.PIPE)
                written = (width, height)
            elif encoder is not None and written != (width, height):
                problems.append(f'{segment.name}: size {width}x{height} differs from the first segment; skipped')
                continue
            decoded = 0
            for media_index, data in enumerate(decode(segment.video, width, height, ffmpeg)):
                decoded += 1
                ref = index.get(media_index)
                if ref is None:
                    problems.append(f'{segment.name}: media frame {media_index} is not in the frame index')
                    status, record = bundles.GAP, None
                else:
                    status, record = bundle.coverage(ref), bundle.results.get(ref.key())
                counts[status] += 1
                if mode == 'processed' and status not in (bundles.PROCESSED_OBJECTS, bundles.PROCESSED_EMPTY,
                        bundles.FAILED):
                    continue
                annotate(data, width, height, record, status)
                if frames_dir:
                    write_ppm(os.path.join(frames_dir, f'{segment.name}-{media_index:06d}.ppm'), data, width, height)
                if encoder is not None:
                    encoder.stdin.write(data)
            if decoded != len(segment.index):
                problems.append(f'{segment.name}: decoded {decoded} frames, index lists {len(segment.index)}')
    finally:
        if encoder is not None:
            encoder.stdin.close()
            if encoder.wait() != 0:
                problems.append('ffmpeg failed to encode the output')
    complete = bundle.complete and not problems and not counts[bundles.FAILED] and not counts[bundles.GAP]
    report = {'bundle': os.path.abspath(bundle_dir), 'mode': mode, 'renderer_version': RENDERER_VERSION,
        'complete': complete, 'coverage': counts, 'problems': problems}
    if output:
        with open(output + '.report.json', 'w', encoding='utf-8') as file:
            json.dump(report, file, indent=2)
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(prog='python -m thorvision_sdk.replay', description=__doc__.split('\n\n')[0])
    parser.add_argument('--bundle', required=True, help='bundle directory or manifest file (e.g. <video>.bundle.json)')
    parser.add_argument('--output', help='encoded video to write, e.g. replay.mp4')
    parser.add_argument('--mode', choices=('processed', 'original'), default='processed')
    parser.add_argument('--fps', type=float, help='override the output frame rate')
    parser.add_argument('--frames-dir', help='also write each annotated frame losslessly as PPM')
    parser.add_argument('--codec', default='libx264')
    parser.add_argument('--ffmpeg', default='ffmpeg')
    parser.add_argument('--ffprobe', default='ffprobe')
    args = parser.parse_args(argv)
    if not args.output and not args.frames_dir:
        parser.error('give --output, --frames-dir or both')
    try:
        report = replay(args.bundle, args.output, args.mode, args.fps, args.frames_dir, args.codec,
            args.ffmpeg, args.ffprobe)
    except (bundles.BundleError, OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f'replay failed: {error}', file=sys.stderr)
        return 2
    print(json.dumps(report['coverage']))
    if not report['complete']:
        print('WARNING: analytics coverage is incomplete', file=sys.stderr)
        for problem in report['problems']:
            print(f'  {problem}', file=sys.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main())
