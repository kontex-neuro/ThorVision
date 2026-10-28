"""Read frame references embedded by the capture encoders (see cpp-src/frame_ref.h).

H.265: prefix SEI user_data_unregistered with ``SEI_UUID``. JPEG: APP11
segment starting with ``JPEG_TAG``. Messages are matched by UUID/tag and
version, never by position, and the legacy hardware metadata is reported
separately so compatibility can be checked.

    python -m thorvision_sdk.frameref <file> [--codec h265|jpeg]

Containers such as MKV are converted to an elementary stream with ffmpeg.
"""
from __future__ import annotations

import argparse
import json
import struct
import subprocess
import sys
import uuid
from dataclasses import dataclass
from typing import Optional

from .contract import FrameRef

VERSION = 1
SEI_UUID = bytes([0x54, 0x56, 0x46, 0x52, 0x2d, 0x9b, 0x4c, 0x1e, 0x8a, 0x52, 0x6f, 0x0d, 0x31, 0xc7, 0x5e, 0x01])
JPEG_TAG = b'ThorVisionFrameRef\0'
JPEG_MARKER = 0xEB
HARDWARE_METADATA = 1
PREFIX_SEI, REGISTERED, UNREGISTERED = 39, 4, 5


@dataclass
class EmbeddedRef:
    ref: FrameRef
    capture_time_ns: int
    flags: int

    @property
    def hardware_metadata(self):
        return bool(self.flags & HARDWARE_METADATA)


@dataclass
class Picture:
    """One access unit (H.265) or image (JPEG) in stream order."""
    index: int
    embedded: Optional[EmbeddedRef]
    legacy: bool
    legacy_first: bool
    keyframe: bool = False


def decode_payload(data):
    if len(data) < 37 or data[0] != VERSION:
        return None
    flags = data[1]
    sequence, capture = struct.unpack_from('<QQ', data, 4)
    epoch = str(uuid.UUID(bytes=bytes(data[20:36])))
    size = data[36]
    if len(data) < 37 + size:
        return None
    camera = bytes(data[37:37 + size]).decode('utf-8', 'replace')
    return EmbeddedRef(FrameRef(camera, epoch, sequence), capture, flags)


def nal_units(stream):
    """Yield NAL unit payloads (header included) from an Annex B byte stream."""
    starts = []
    position = stream.find(b'\0\0\1')
    while position != -1:
        starts.append(position + 3)
        position = stream.find(b'\0\0\1', position + 3)
    for number, start in enumerate(starts):
        end = starts[number + 1] - 3 if number + 1 < len(starts) else len(stream)
        while end > start and stream[end - 1] == 0:  # Trailing zero / 4-byte start code prefix.
            end -= 1
        yield stream[start:end]


def unescape(data):
    return data.replace(b'\0\0\3', b'\0\0')


def sei_messages(nal):
    """``[(payload_type, payload)]`` from an SEI NAL unit."""
    data = unescape(nal[2:])
    messages, position = [], 0
    while position < len(data) and not (data[position] == 0x80 and position == len(data) - 1):
        kind = size = 0
        while position < len(data) and data[position] == 0xFF:
            kind += 255
            position += 1
        if position >= len(data):
            break
        kind += data[position]
        position += 1
        while position < len(data) and data[position] == 0xFF:
            size += 255
            position += 1
        if position >= len(data):
            break
        size += data[position]
        position += 1
        messages.append((kind, data[position:position + size]))
        position += size
    return messages


def h265_pictures(stream):
    """Group NAL units into access units at each picture start and read their SEI."""
    pictures, pending = [], []

    def flush(units):
        embedded, legacy, legacy_first = None, False, False
        keyframe = any(16 <= (unit[0] >> 1) & 0x3F <= 21 for unit in units)  # IRAP pictures.
        for unit in units:
            if (unit[0] >> 1) & 0x3F != PREFIX_SEI:
                continue
            for kind, payload in sei_messages(unit):
                if kind == REGISTERED and not legacy:
                    legacy, legacy_first = True, embedded is None
                elif kind == UNREGISTERED and payload[:16] == SEI_UUID and embedded is None:
                    embedded = decode_payload(payload[16:])
        pictures.append(Picture(len(pictures), embedded, legacy, legacy_first, keyframe))

    current = None
    for unit in nal_units(stream):
        if len(unit) < 3:
            continue
        kind = (unit[0] >> 1) & 0x3F
        if kind <= 31 and unit[2] & 0x80:  # VCL with first_slice_segment_in_pic_flag.
            if current is not None:
                flush(current)
            current, pending = pending + [unit], []
        elif kind <= 31:
            if current is not None:
                current.append(unit)
        else:
            pending.append(unit)
    if current is not None:
        flush(current + pending)
    return pictures


def jpeg_segments(image):
    """``[(marker, payload)]`` for header segments up to start-of-scan."""
    if image[:2] != b'\xff\xd8':
        raise ValueError('not a JPEG image')
    segments, position = [], 2
    while position + 4 <= len(image) and image[position] == 0xFF:
        marker = image[position + 1]
        if marker == 0xDA:
            break
        length = struct.unpack_from('>H', image, position + 2)[0]
        segments.append((marker, image[position + 4:position + 2 + length]))
        position += 2 + length
    return segments


def jpeg_picture(image, index=0):
    segments = jpeg_segments(image)
    embedded = None
    for marker, payload in segments:
        if marker == JPEG_MARKER and payload.startswith(JPEG_TAG) and embedded is None:
            embedded = decode_payload(payload[len(JPEG_TAG):])
    # Legacy readers require the hardware metadata COM directly after SOI.
    legacy = bool(segments) and segments[0][0] == 0xFE and len(segments[0][1]) >= 40
    return Picture(index, embedded, legacy, legacy, True)


def split_mjpeg(stream):
    """Split concatenated JPEG images; the first EOI after start-of-scan ends an image."""
    images, position = [], stream.find(b'\xff\xd8')
    while position != -1:
        scan = stream.find(b'\xff\xda', position)
        end = stream.find(b'\xff\xd9', scan if scan != -1 else position)
        if end == -1:
            break
        images.append(stream[position:end + 2])
        position = stream.find(b'\xff\xd8', end + 2)
    return images


def read_pictures(path, codec='h265', ffmpeg='ffmpeg'):
    if path.endswith(('.h265', '.hevc', '.mjpeg', '.mjpg', '.jpg')):
        with open(path, 'rb') as file:
            stream = file.read()
    else:
        stream = subprocess.run([ffmpeg, '-v', 'error', '-i', path, '-map', '0:v:0', '-c', 'copy', '-f',
            'hevc' if codec == 'h265' else 'mjpeg', '-'], check=True, capture_output=True).stdout
    if codec == 'h265':
        return h265_pictures(stream)
    return [jpeg_picture(image, index) for index, image in enumerate(split_mjpeg(stream))]


def main(argv=None):
    parser = argparse.ArgumentParser(prog='python -m thorvision_sdk.frameref', description='List embedded frame references.')
    parser.add_argument('path')
    parser.add_argument('--codec', choices=('h265', 'jpeg'), default='h265')
    parser.add_argument('--ffmpeg', default='ffmpeg')
    args = parser.parse_args(argv)
    for picture in read_pictures(args.path, args.codec, args.ffmpeg):
        row = {'index': picture.index, 'legacy': picture.legacy, 'legacy_first': picture.legacy_first,
            'frame': None}
        if picture.embedded:
            row.update(frame=picture.embedded.ref.to_json(), capture_time_ns=str(picture.embedded.capture_time_ns),
                hardware_metadata=picture.embedded.hardware_metadata)
        print(json.dumps(row))
    return 0


if __name__ == '__main__':
    sys.exit(main())
