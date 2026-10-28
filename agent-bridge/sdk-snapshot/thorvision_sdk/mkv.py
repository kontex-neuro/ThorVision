"""Build a frame index from an actual Matroska recording.

Reads only element headers and the first bytes of each video block, so a
one-minute 1080p segment indexes quickly and without FFmpeg. Truncated
(interrupted) files yield the frames before the damage plus a problem note.
``media_index`` is the presentation-order ordinal, as decoders emit frames.
"""
from __future__ import annotations

import struct
from dataclasses import dataclass
from typing import Optional

from . import frameref
from .contract import FrameRef

SEGMENT, CLUSTER, TIMESTAMP = 0x18538067, 0x1F43B675, 0xE7
INFO, TIMESTAMP_SCALE = 0x1549A966, 0x2AD7B1
TRACKS, TRACK_ENTRY, TRACK_NUMBER, TRACK_TYPE, CODEC_ID, CODEC_PRIVATE = 0x1654AE6B, 0xAE, 0xD7, 0x83, 0x86, 0x63A2
SIMPLE_BLOCK, BLOCK_GROUP, BLOCK = 0xA3, 0xA0, 0xA1
SEGMENT_LEVEL = {CLUSTER, INFO, TRACKS, 0x1C53BB6B, 0x1254C367, 0x114D9B74, 0x1043A770, 0x1941A469}
PEEK = 8192


@dataclass
class IndexedFrame:
    media_index: int
    pts_ns: int
    keyframe: bool
    ref: Optional[FrameRef]
    capture_time_ns: Optional[int] = None

    def to_json(self):
        row = {'schema': 'thorvision.frame-index/1', 'media_index': self.media_index, 'pts_ns': str(self.pts_ns),
            'keyframe': self.keyframe, 'frame': self.ref.to_json() if self.ref else None}
        if self.capture_time_ns is not None:
            row['capture_time_ns'] = str(self.capture_time_ns)
        return row


class _Reader:
    def __init__(self, file, size):
        self.file, self.size = file, size

    def vint(self, keep_marker):
        first = self.file.read(1)
        if not first:
            raise EOFError
        value = first[0]
        length = 1
        mask = 0x80
        while length <= 8 and not value & mask:
            mask >>= 1
            length += 1
        if length > 8:
            raise ValueError('invalid EBML variable-length integer')
        rest = self.file.read(length - 1)
        if len(rest) < length - 1:
            raise EOFError
        number = value if keep_marker else value & (mask - 1)
        for byte in rest:
            number = number << 8 | byte
        unknown = not keep_marker and number == (1 << (7 * length)) - 1
        return number, unknown

    def header(self):
        start = self.file.tell()
        element, _ = self.vint(True)
        size, unknown = self.vint(False)
        return element, None if unknown else size, start, self.file.tell()

    def read(self, size):
        data = self.file.read(size)
        if len(data) < size:
            raise EOFError
        return data


def codec_name(codec_id, private=b''):
    """matroskamux stores MJPEG as V_MS/VFW/FOURCC with a BITMAPINFOHEADER whose compression is MJPG."""
    if codec_id == 'V_MPEGH/ISO/HEVC':
        return 'h265'
    if codec_id == 'V_MJPEG' or (codec_id == 'V_MS/VFW/FOURCC' and private[16:20].upper() == b'MJPG'):
        return 'jpeg'
    return codec_id


def _uint(data):
    return int.from_bytes(data, 'big')


def index_mkv(path):
    """Return ``(frames, codec, problems)`` for the first video track."""
    problems, blocks = [], []
    scale, video_track, codec, nal_length = 1_000_000, None, None, 4
    with open(path, 'rb') as file:
        file.seek(0, 2)
        reader = _Reader(file, file.tell())
        file.seek(0)
        try:
            element, size, _, body = reader.header()
            if element != 0x1A45DFA3:
                raise ValueError('not a Matroska/EBML file')
            file.seek(body + size)
            element, segment_size, _, segment_body = reader.header()
            if element != SEGMENT:
                raise ValueError('Matroska segment not found')
            segment_end = reader.size if segment_size is None else min(reader.size, segment_body + segment_size)
            cluster_time = 0
            while file.tell() < segment_end:
                element, size, start, body = reader.header()
                end = segment_end if size is None else body + size
                if element == INFO:
                    while file.tell() < end:
                        child, child_size, _, child_body = reader.header()
                        if child == TIMESTAMP_SCALE:
                            scale = _uint(reader.read(child_size))
                        file.seek(child_body + child_size)
                elif element == TRACKS:
                    while file.tell() < end:
                        child, child_size, _, child_body = reader.header()
                        if child == TRACK_ENTRY:
                            fields = {}
                            while file.tell() < child_body + child_size:
                                field, field_size, _, field_body = reader.header()
                                if field in (TRACK_NUMBER, TRACK_TYPE, CODEC_ID, CODEC_PRIVATE):
                                    fields[field] = reader.read(field_size)
                                file.seek(field_body + field_size)
                            if video_track is None and _uint(fields.get(TRACK_TYPE, b'\0')) == 1:
                                video_track = _uint(fields[TRACK_NUMBER])
                                codec_id = fields.get(CODEC_ID, b'').rstrip(b'\0').decode('ascii', 'replace')
                                private = fields.get(CODEC_PRIVATE, b'')
                                codec = codec_name(codec_id, private)
                                if codec == 'h265' and len(private) > 21:
                                    nal_length = (private[21] & 3) + 1
                        file.seek(child_body + child_size)
                elif element == CLUSTER:
                    while file.tell() < end:
                        position = file.tell()
                        child, child_size, _, child_body = reader.header()
                        if size is None and child in SEGMENT_LEVEL:
                            file.seek(position)  # Unknown-size cluster ends at the next top-level element.
                            break
                        if child_size is None:
                            raise ValueError('unsupported unknown-size cluster child')
                        if child == TIMESTAMP:
                            cluster_time = _uint(reader.read(child_size))
                        elif child == SIMPLE_BLOCK:
                            blocks.append(_block(reader, child_body, child_size, cluster_time, True))
                        elif child == BLOCK_GROUP:
                            group_end = child_body + child_size
                            while file.tell() < group_end:
                                field, field_size, _, field_body = reader.header()
                                if field == BLOCK:
                                    blocks.append(_block(reader, field_body, field_size, cluster_time, False))
                                file.seek(field_body + field_size)
                        file.seek(child_body + child_size)
                        if file.tell() > reader.size:
                            raise EOFError
                    continue
                if size is None:
                    break
                file.seek(end)
        except EOFError:
            problems.append('file is truncated; frames after the last complete block are not indexed')
        except ValueError as error:
            problems.append(str(error))
    frames = []
    for track, relative, keyframe, peek in blocks:
        if track != video_track or peek is None:
            continue
        pts = relative * scale
        embedded = _identity(codec, peek, nal_length)
        frames.append(IndexedFrame(0, pts, keyframe, embedded.ref if embedded else None,
            embedded.capture_time_ns if embedded else None))
    frames.sort(key=lambda frame: frame.pts_ns)
    for number, frame in enumerate(frames):
        frame.media_index = number
    return frames, codec, problems


def _block(reader, body, size, cluster_time, simple):
    file = reader.file
    track, _ = reader.vint(False)
    timecode, flags = struct.unpack('>hB', reader.read(3))
    if flags & 0x06:
        return track, cluster_time + timecode, False, None  # Laced video is not produced by the recorder.
    remaining = body + size - file.tell()
    if body + size > reader.size:
        raise EOFError
    peek = reader.read(min(PEEK, remaining))
    return track, cluster_time + timecode, bool(flags & 0x80) if simple else False, peek


def _identity(codec, peek, nal_length):
    if codec == 'jpeg':
        try:
            return frameref.jpeg_picture(peek).embedded
        except ValueError:
            return None
    position = 0
    while position + nal_length <= len(peek):
        size = int.from_bytes(peek[position:position + nal_length], 'big')
        nal = peek[position + nal_length:position + nal_length + size]
        position += nal_length + size
        if len(nal) < 2:
            break
        kind = (nal[0] >> 1) & 0x3F
        if kind <= 31:
            break  # Prefix SEI precedes the picture's slices.
        if kind == frameref.PREFIX_SEI:
            for payload_type, payload in frameref.sei_messages(nal):
                if payload_type == frameref.UNREGISTERED and payload[:16] == frameref.SEI_UUID:
                    embedded = frameref.decode_payload(payload[16:])
                    if embedded:
                        return embedded
    return None
