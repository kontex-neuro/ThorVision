"""Recording bundle format: video, analytics JSONL, frame index and manifest.

Layout (paths in the manifest are relative to the bundle directory)::

    manifest.json
    video/<segment>.<ext>
    index/<segment>.jsonl            {"frame": FrameRef, "media_index": n, "pts_ns": "..."}
    analytics/<segment>/<run>.jsonl  result and gap records

``media_index`` is the decoded-frame ordinal in presentation order; the
recorder builds or verifies it from the resulting media because muxing
normalises timestamps. A frame indexed during an active run without a record
is ``not_sampled``.
"""
from __future__ import annotations

import json
import os
from dataclasses import dataclass, field

from .contract import FrameRef

BUNDLE_SCHEMA = 'thorvision.bundle/1'
RECORD_SCHEMA = 'thorvision.analytics.record/1'
INDEX_SCHEMA = 'thorvision.frame-index/1'

PROCESSED_OBJECTS = 'processed_objects'
PROCESSED_EMPTY = 'processed_empty'
NOT_SAMPLED = 'not_sampled'
FAILED = 'failed'
GAP = 'gap'
COVERAGE = (PROCESSED_OBJECTS, PROCESSED_EMPTY, NOT_SAMPLED, FAILED, GAP)


class BundleError(ValueError):
    pass


def result_record(frame, run_id, result_seq, producer, result=None, error=None, renderer_version='1'):
    """Record for one sampled frame; identity and producer come from the supervisor, not the callback."""
    if (result is None) == (error is None):
        raise ValueError('exactly one of result or error is required')
    if result is not None:
        status = PROCESSED_OBJECTS if result.detections or result.drawings else PROCESSED_EMPTY
    else:
        status = FAILED
    record = {'schema': RECORD_SCHEMA, 'kind': 'result', 'status': status, 'frame': frame.to_json(),
        'run_id': run_id, 'result_seq': str(result_seq), 'producer': producer,
        'renderer_version': renderer_version}
    if result is not None:
        record['result'] = result.to_json()
    else:
        record['error'] = str(error)[:1024]
    return record


def gap_record(camera, epoch, first_sequence, last_sequence, run_id, reason):
    """Delivery or persistence gap covering an inclusive sequence range."""
    return {'schema': RECORD_SCHEMA, 'kind': 'gap', 'status': GAP, 'camera': camera, 'epoch': epoch,
        'first_sequence': str(first_sequence), 'last_sequence': str(last_sequence), 'run_id': run_id,
        'reason': reason}


class JsonlWriter:
    """Append-only writer; each record is one complete line flushed on write."""

    def __init__(self, path, durable=False):
        os.makedirs(os.path.dirname(path) or '.', exist_ok=True)
        self.durable = durable
        self.file = open(path, 'a', encoding='utf-8', newline='\n')

    def write(self, record):
        self.file.write(json.dumps(record, separators=(',', ':'), allow_nan=False) + '\n')
        self.file.flush()
        if self.durable:
            os.fsync(self.file.fileno())

    def close(self):
        self.file.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()


def read_jsonl(path):
    """Return ``(records, problems)``; a truncated or corrupt line is reported, not fatal."""
    records, problems = [], []
    with open(path, 'rb') as file:
        data = file.read()
    lines = data.split(b'\n')
    for number, line in enumerate(lines, 1):
        if not line.strip():
            continue
        try:
            records.append(json.loads(line))
        except ValueError:
            tail = number == len(lines) and not data.endswith(b'\n')
            problems.append(f'{path}:{number}: {"partial tail" if tail else "corrupt line"}')
    return records, problems


def write_manifest(directory, manifest):
    path = os.path.join(directory, 'manifest.json')
    temporary = path + '.tmp'
    with open(temporary, 'w', encoding='utf-8') as file:
        json.dump(manifest, file, indent=2, sort_keys=True)
        file.write('\n')
    os.replace(temporary, path)


@dataclass
class Segment:
    name: str
    video: str
    index: list = field(default_factory=list)


@dataclass
class Bundle:
    directory: str
    manifest: dict
    segments: list
    results: dict
    gaps: list
    problems: list

    @classmethod
    def load(cls, location):
        """Load a bundle directory (``manifest.json``) or a manifest file such as a recorder ``.bundle.json``."""
        location = str(location)
        path = location if os.path.isfile(location) else os.path.join(location, 'manifest.json')
        directory = os.path.dirname(os.path.abspath(path))
        try:
            with open(path, encoding='utf-8') as file:
                manifest = json.load(file)
        except (OSError, ValueError) as error:
            raise BundleError(f'cannot read bundle manifest: {error}') from None
        if manifest.get('schema') != BUNDLE_SCHEMA:
            raise BundleError(f'unsupported bundle schema {manifest.get("schema")!r}')
        problems, segments, results, gaps = [], [], {}, []
        for entry in manifest.get('segments', []):
            segment = Segment(entry['name'], os.path.normpath(os.path.join(directory, entry['video'])))
            index_path = os.path.normpath(os.path.join(directory, entry['index']))
            if os.path.exists(index_path):
                rows, issues = read_jsonl(index_path)
                problems += issues
                segment.index = sorted((int(row['media_index']), FrameRef.from_json(row['frame'])) for row in rows)
            else:
                problems.append(f'{entry["name"]}: frame index missing')
            segments.append(segment)
            for path in entry.get('analytics', []):
                full = os.path.normpath(os.path.join(directory, path))
                if not os.path.exists(full):
                    problems.append(f'{path}: analytics file missing')
                    continue
                records, issues = read_jsonl(full)
                problems += issues
                for record in records:
                    if record.get('schema') != RECORD_SCHEMA:
                        problems.append(f'{path}: unsupported record schema {record.get("schema")!r}')
                    elif record['kind'] == 'gap':
                        gaps.append(record)
                    else:
                        key = FrameRef.from_json(record['frame']).key()
                        if key in results:
                            problems.append(f'{path}: duplicate result for frame {key}; first kept')
                        else:
                            results[key] = record
        return cls(directory, manifest, segments, results, gaps, problems)

    def in_gap(self, ref):
        return any(gap['camera'] == ref.camera and gap['epoch'] == ref.epoch and
            int(gap['first_sequence']) <= ref.sequence <= int(gap['last_sequence']) for gap in self.gaps)

    def coverage(self, ref):
        record = self.results.get(ref.key())
        if record is not None:
            return record['status']
        return GAP if self.in_gap(ref) else NOT_SAMPLED

    @property
    def complete(self):
        return bool(self.manifest.get('coverage', {}).get('complete', False)) and not self.problems and not self.gaps
