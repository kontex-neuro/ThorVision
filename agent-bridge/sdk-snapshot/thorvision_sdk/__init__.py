"""ThorVision analytics callback SDK.

The device repository is the canonical source of this package and of the
schemas under ``thorvision_sdk/schemas``. Client and bridge builds consume a
versioned snapshot whose digest must match ``schema_digest()``.
"""
from .contract import (Detection, Frame, FrameRef, FrameResult, Point, Polyline, Rect, Text,
    TrackPoint, Transform, fit_within, letterbox)
from .validate import LIMITS, Limits, ResultError, validate_result

__version__ = '0.1.0'
PROTOCOL_VERSION = 1

__all__ = ['Detection', 'Frame', 'FrameRef', 'FrameResult', 'LIMITS', 'Limits', 'Point', 'Polyline',
    'PROTOCOL_VERSION', 'Rect', 'ResultError', 'Text', 'TrackPoint', 'Transform', '__version__',
    'fit_within', 'letterbox', 'schema_digest', 'validate_result']


def schema_digest():
    """SHA-256 over the published schemas, used to match client snapshots."""
    import hashlib
    from importlib import resources
    digest = hashlib.sha256()
    folder = resources.files(__package__).joinpath('schemas')
    for entry in sorted(folder.iterdir(), key=lambda item: item.name):
        if entry.name.endswith('.json'):
            digest.update(entry.name.encode() + b'\0' + entry.read_bytes() + b'\0')
    return digest.hexdigest()
