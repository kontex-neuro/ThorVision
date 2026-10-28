"""In-process harness for running a processor over fixture frames.

This is for SDK tests and authoring checks only. On the device the supervisor
runs callbacks in an isolated worker process with OS-enforced limits.
"""
from __future__ import annotations

import importlib.util
import os
import sys
from dataclasses import dataclass
from typing import Optional

from .contract import FrameResult
from .validate import ResultError, validate_result


@dataclass
class Outcome:
    frame: object
    result: Optional[FrameResult] = None
    error: Optional[str] = None


def load_processor(directory, entrypoint):
    module_name, _, attribute = entrypoint.partition(':')
    path = os.path.join(directory, module_name.replace('.', '/') + '.py')
    unique = f'thorvision_extension_{abs(hash(os.path.abspath(directory)))}_{module_name.replace(".", "_")}'
    spec = importlib.util.spec_from_file_location(unique, path)
    module = importlib.util.module_from_spec(spec)
    sys.path.insert(0, directory)
    try:
        spec.loader.exec_module(module)
    finally:
        sys.path.remove(directory)
    return getattr(module, attribute)


def run(processor, frames, config=None, context=None):
    """Run ``setup``/``process``/``close``; stop at the first exception or invalid result."""
    outcomes = []
    processor.setup(context or {}, config or {})
    try:
        for frame in frames:
            try:
                raw = processor.process(frame)
                if not isinstance(raw, FrameResult):
                    raise ResultError([f'process returned {type(raw).__name__}, expected FrameResult'])
                outcomes.append(Outcome(frame, validate_result(raw, frame.source_width, frame.source_height)))
            except Exception as error:  # A callback fault ends the run, as it stops the device worker.
                outcomes.append(Outcome(frame, error=f'{type(error).__name__}: {error}'))
                break
    finally:
        processor.close()
    return outcomes
