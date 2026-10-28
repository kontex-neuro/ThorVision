"""Reference IoU-association tracker with time-bounded trails.

Track IDs are scoped to one tracker instance, which the supervisor creates per
worker run; a restarted worker therefore starts a new track namespace.
"""
from __future__ import annotations

from collections import deque
from dataclasses import dataclass, field


def iou(a, b):
    width = min(a[2], b[2]) - max(a[0], b[0])
    height = min(a[3], b[3]) - max(a[1], b[1])
    if width <= 0 or height <= 0:
        return 0.0
    intersection = width * height
    union = (a[2] - a[0]) * (a[3] - a[1]) + (b[2] - b[0]) * (b[3] - b[1]) - intersection
    return intersection / union if union > 0 else 0.0


@dataclass
class Track:
    track_id: int
    box: tuple
    missed: int = 0
    trail: deque = field(default_factory=deque)

    @property
    def centre(self):
        return ((self.box[0] + self.box[2]) / 2, (self.box[1] + self.box[3]) / 2)


class IoUTracker:
    def __init__(self, threshold=0.3, max_missed=5, trail_ns=2_000_000_000):
        self.threshold, self.max_missed, self.trail_ns = threshold, max_missed, trail_ns
        self.tracks = []
        self.next_id = 1

    def update(self, boxes, time_ns):
        """Associate boxes greedily by IoU; return ``[(track_id, box)]`` in input order."""
        pairs = sorted(((iou(track.box, box), t, b) for t, track in enumerate(self.tracks)
            for b, box in enumerate(boxes)), reverse=True)
        assigned, used = {}, set()
        for score, t, b in pairs:
            if score < self.threshold:
                break
            if b not in assigned and t not in used:
                assigned[b] = t
                used.add(t)
        for t, track in enumerate(self.tracks):
            if t not in used:
                track.missed += 1
        results = []
        for b, box in enumerate(boxes):
            if b in assigned:
                track = self.tracks[assigned[b]]
                track.box, track.missed = tuple(box), 0
            else:
                track = Track(self.next_id, tuple(box))
                self.next_id += 1
                self.tracks.append(track)
            track.trail.append((time_ns, *track.centre))
            results.append((track.track_id, track.box))
        for track in self.tracks:
            while track.trail and track.trail[0][0] < time_ns - self.trail_ns:
                track.trail.popleft()
        self.tracks = [track for track in self.tracks if track.missed <= self.max_missed]
        return results

    def trail(self, track_id):
        for track in self.tracks:
            if track.track_id == track_id:
                return list(track.trail)
        return []
